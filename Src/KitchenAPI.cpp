// =============================================================================
//  KitchenAPI.cpp – Archicad ile konuşan katman
// =============================================================================
#include "KitchenAPI.hpp"
#include "ResourceIds.hpp"

#include "DG.h"
#include "DGFileDialog.hpp"
#include "File.hpp"
#include "FileTypeManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace KitchenAPI {

using namespace Kitchen;

// -----------------------------------------------------------------------------
//  Metin dönüşümleri
// -----------------------------------------------------------------------------
GS::UniString U (const char* utf8)
{
	return GS::UniString (utf8, CC_UTF8);
}

GS::UniString ToUni (const std::string& utf8)
{
	return GS::UniString (utf8.c_str (), CC_UTF8);
}

std::string ToUtf8 (const GS::UniString& str)
{
	return std::string (str.ToCStr (CC_UTF8).Get ());
}

// -----------------------------------------------------------------------------
//  Kütüphane parçası
// -----------------------------------------------------------------------------
GSErrCode GetCabinetLibInd (Int32& libInd)
{
	libInd = 0;
	char unId[128] = {};
	GSErrCode err = ACAPI_LibraryPart_GetBuiltInLibpartUnId (ID_BUILTIN_CABINET, unId);
	if (err != NoError)
		return err;

	API_LibPart libPart {};
	CHCopyC (unId, libPart.ownUnID);
	err = ACAPI_LibraryPart_Search (&libPart, false);
	if (libPart.location != nullptr)
		delete libPart.location;
	if (err != NoError)
		return err;

	libInd = libPart.index;
	return NoError;
}

// -----------------------------------------------------------------------------
//  Seçili duvarlar
// -----------------------------------------------------------------------------
bool GetSelectedWalls (SelectedWalls& out, GS::UniString& message)
{
	out = {};
	API_SelectionInfo selInfo {};
	GS::Array<API_Neig> selNeigs;
	GSErrCode err = ACAPI_Selection_Get (&selInfo, &selNeigs, false);
	BMKillHandle ((GSHandle*) &selInfo.marquee.coords);
	if (err != NoError || selNeigs.IsEmpty ()) {
		message = U ("Önce planda mutfağın kurulacağı 1–3 duvarı seçin (düz, L veya U mutfak).");
		return false;
	}

	bool floorSet = false;
	for (const API_Neig& neig : selNeigs) {
		API_Element el {};
		el.header.guid = neig.guid;
		if (ACAPI_Element_Get (&el) != NoError || el.header.type.typeID != API_WallID)
			continue;

		if (std::fabs (el.wall.angle) > 1e-6) {
			message = U ("Eğri duvarlar desteklenmiyor. Lütfen düz duvar seçin.");
			return false;
		}
		if (!floorSet) {
			out.floorInd = el.header.floorInd;
			floorSet = true;
		} else if (el.header.floorInd != out.floorInd) {
			message = U ("Seçili duvarlar aynı katta olmalı.");
			return false;
		}

		WallInput w;
		w.begC = { el.wall.begC.x, el.wall.begC.y };
		w.endC = { el.wall.endC.x, el.wall.endC.y };

		API_ElementMemo memo {};
		if (ACAPI_Element_GetMemo (el.header.guid, &memo, APIMemoMask_Polygon) == NoError &&
			memo.coords != nullptr && memo.pends != nullptr) {
			const Int32 nSub = BMGetHandleSize ((GSHandle) memo.pends) / sizeof (Int32) - 1;
			for (Int32 k = 1; k <= nSub; ++k) {
				std::vector<Vec2> ring;
				for (Int32 i = (*memo.pends)[k - 1] + 1; i <= (*memo.pends)[k]; ++i)
					ring.push_back ({ (*memo.coords)[i].x, (*memo.coords)[i].y });
				if (ring.size () >= 3)
					w.contours.push_back (ring);
			}
		}
		ACAPI_DisposeElemMemoHdls (&memo);

		if (w.contours.empty ()) {
			// Yedek: poligon okunamazsa referans çizgisine ortalanmış dikdörtgen
			const double dx = w.endC.x - w.begC.x, dy = w.endC.y - w.begC.y;
			const double len = std::sqrt (dx * dx + dy * dy);
			if (len > 1e-6) {
				const double h = el.wall.thickness / 2.0;
				const Vec2 n = { -dy / len * h, dx / len * h };
				w.contours.push_back ({ { w.begC.x - n.x, w.begC.y - n.y }, { w.endC.x - n.x, w.endC.y - n.y },
										{ w.endC.x + n.x, w.endC.y + n.y }, { w.begC.x + n.x, w.begC.y + n.y },
										{ w.begC.x - n.x, w.begC.y - n.y } });
			}
		}
		out.walls.push_back (w);
	}

	if (out.walls.empty ()) {
		message = U ("Seçimde duvar yok. Mutfağın kurulacağı 1–3 duvarı seçin.");
		return false;
	}
	if (out.walls.size () > 3) {
		message = U ("En fazla 3 duvar seçin (düz, L veya U mutfak).");
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------------
//  Nesne oluşturma
// -----------------------------------------------------------------------------
static void SetParReal (const char* name, double value)
{
	API_ChangeParamType chg {};
	std::strncpy (chg.name, name, sizeof (chg.name) - 1);
	chg.realValue = value;
	ACAPI_LibraryPart_ChangeAParameter (&chg);
}

static void SetParStr (const char* name, const std::string& utf8)
{
	API_ChangeParamType chg {};
	std::strncpy (chg.name, name, sizeof (chg.name) - 1);
	const GS::UniString us = ToUni (utf8);
	std::vector<GS::uchar_t> buf (us.GetLength () + 1, 0);
	GS::ucscpy (buf.data (), us.ToUStr ());
	chg.uStrValue = buf.data ();
	ACAPI_LibraryPart_ChangeAParameter (&chg);
}

static GSErrCode CreateOne (Int32 libInd, const WallFace& face, const PlacedModule& m, short floorInd)
{
	API_Element element {};
	API_ElementMemo memo {};
	element.header.type = API_ObjectID;
	GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
	if (err != NoError) {
		ACAPI_DisposeElemMemoHdls (&memo);
		return err;
	}

	API_ParamOwnerType owner {};
	owner.libInd = libInd;
	err = ACAPI_LibraryPart_OpenParameters (&owner);
	if (err != NoError) {
		ACAPI_DisposeElemMemoHdls (&memo);
		return err;
	}

	// Sıra önemli: "tip" parametre komut dosyasında B ve ZZYZX'i sıfırlar.
	SetParStr  ("tip", m.tip);
	SetParStr  ("duzen", m.duzen);
	SetParReal ("A", m.Width ());
	SetParReal ("B", m.depth);
	SetParReal ("ZZYZX", m.height);
	SetParStr  ("modulKodu", m.spec.code);
	SetParReal ("duvarNo", m.wall + 1);
	SetParReal ("kapakSayisi", m.kapakSayisi);
	SetParReal ("cekmeceSayisi", m.cekmeceSayisi);
	SetParReal ("rafSayisi", m.rafSayisi);
	SetParReal ("korGen", m.korGen);
	SetParStr  ("korTaraf", m.korTaraf);
	SetParReal ("eviyeVar", m.eviye ? 1.0 : 0.0);
	SetParReal ("ocakVar", m.ocak ? 1.0 : 0.0);
	SetParReal ("tezgahVar", m.tezgah ? 1.0 : 0.0);

	API_GetParamsType getParams {};
	err = ACAPI_LibraryPart_GetActParameters (&getParams);
	ACAPI_LibraryPart_CloseParameters ();
	if (err != NoError) {
		ACAPI_DisposeAddParHdl (&getParams.params);
		ACAPI_DisposeElemMemoHdls (&memo);
		return err;
	}
	ACAPI_DisposeAddParHdl (&memo.params);
	memo.params = getParams.params;

	Vec2 pos;
	double angle = 0.0;
	ModuleOrigin (face, m, pos, angle);

	element.header.floorInd       = floorInd;
	element.object.libInd         = libInd;
	element.object.pos.x          = pos.x;
	element.object.pos.y          = pos.y;
	element.object.angle          = angle;
	element.object.xRatio         = m.Width ();
	element.object.yRatio         = m.depth;
	element.object.level          = m.level;
	element.object.reflected      = false;
	element.object.useObjMaterials = true;
	element.object.useObjPens     = true;
	element.object.useObjLtypes   = true;

	err = ACAPI_Element_Create (&element, &memo);
	ACAPI_DisposeElemMemoHdls (&memo);
	return err;
}

GSErrCode CreateKitchen (const std::vector<WallFace>& faces, const std::vector<PlacedModule>& modules, short floorInd, int& created)
{
	created = 0;
	Int32 libInd = 0;
	GSErrCode err = GetCabinetLibInd (libInd);
	if (err != NoError || libInd <= 0) {
		DGAlert (DG_ERROR, U ("Mutfak Sihirbazı"), U ("Mutfak dolabı nesnesi bulunamadı."),
				 U ("Eklenti kütüphanesi yüklenemedi. Archicad'i yeniden başlatıp tekrar deneyin."), "Tamam");
		return err != NoError ? err : APIERR_GENERAL;
	}

	return ACAPI_CallUndoableCommand (U ("Mutfak Sihirbazı"), [&] () -> GSErrCode {
		for (const PlacedModule& m : modules) {
			if (m.wall < 0 || m.wall >= (int) faces.size ())
				continue;
			const GSErrCode e = CreateOne (libInd, faces[m.wall], m, floorInd);
			if (e != NoError)
				return e;
			++created;
		}
		return NoError;
	});
}

// -----------------------------------------------------------------------------
//  Kesim listesi
// -----------------------------------------------------------------------------
static void ReadParams (API_AddParType** params, CabinetParams& p)
{
	if (params == nullptr)
		return;
	const Int32 n = BMGetHandleSize ((GSHandle) params) / sizeof (API_AddParType);
	for (Int32 i = 0; i < n; ++i) {
		const API_AddParType& par = (*params)[i];
		const std::string name = par.name;
		if (par.typeID == APIParT_CString) {
			const std::string v = ToUtf8 (GS::UniString (par.value.uStr));
			if (name == "tip")				p.tip = v;
			else if (name == "duzen")		p.duzen = v;
			else if (name == "modulKodu")	p.modulKodu = v;
			continue;
		}
		const double v = par.value.real;
		if (name == "ZZYZX")				p.ZZYZX = v;
		else if (name == "duvarNo")			p.duvarNo = (int) std::lround (v);
		else if (name == "kapakSayisi")		p.kapakSayisi = (int) std::lround (v);
		else if (name == "cekmeceSayisi")	p.cekmeceSayisi = (int) std::lround (v);
		else if (name == "rafSayisi")		p.rafSayisi = (int) std::lround (v);
		else if (name == "ustCekmeceYuk")	p.ustCekmeceYuk = v;
		else if (name == "govdeKal")		p.govdeKal = v;
		else if (name == "kapakKal")		p.kapakKal = v;
		else if (name == "kapakBosluk")		p.kapakBosluk = v;
		else if (name == "bazaYuk")			p.bazaYuk = v;
		else if (name == "tezgahVar")		p.tezgahVar = v > 0.5;
		else if (name == "tezgahKal")		p.tezgahKal = v;
		else if (name == "tezgahTasma")		p.tezgahTasma = v;
		else if (name == "korGen")			p.korGen = v;
		else if (name == "buzdolabiYuk")	p.buzdolabiYuk = v;
	}
}

static int TipOrder (const std::string& tip)
{
	if (tip == "Alt Dolap") return 0;
	if (tip == "Boy Dolap") return 1;
	return 2;
}

GSErrCode ExportCutList ()
{
	Int32 libInd = 0;
	if (GetCabinetLibInd (libInd) != NoError || libInd <= 0) {
		DGAlert (DG_ERROR, "Kesim Listesi", U ("Mutfak dolabı nesnesi bulunamadı."), "", "Tamam");
		return APIERR_GENERAL;
	}

	// Seçili dolaplar; seçim yoksa projedeki tüm mutfak dolapları
	GS::Array<API_Guid> guids;
	{
		API_SelectionInfo selInfo {};
		GS::Array<API_Neig> selNeigs;
		if (ACAPI_Selection_Get (&selInfo, &selNeigs, false) == NoError)
			for (const API_Neig& n : selNeigs)
				guids.Push (n.guid);
		BMKillHandle ((GSHandle*) &selInfo.marquee.coords);
	}
	bool fromSelection = true;
	auto collect = [&] (const GS::Array<API_Guid>& list, std::vector<CabinetParams>& out) {
		for (const API_Guid& g : list) {
			API_Element el {};
			el.header.guid = g;
			if (ACAPI_Element_Get (&el) != NoError || el.header.type.typeID != API_ObjectID || el.object.libInd != libInd)
				continue;
			CabinetParams p;
			p.A = el.object.xRatio;
			p.B = el.object.yRatio;
			API_ElementMemo memo {};
			if (ACAPI_Element_GetMemo (g, &memo, APIMemoMask_AddPars) == NoError)
				ReadParams (memo.params, p);
			ACAPI_DisposeElemMemoHdls (&memo);
			out.push_back (p);
		}
	};
	std::vector<CabinetParams> cabinets;
	collect (guids, cabinets);
	if (cabinets.empty ()) {
		fromSelection = false;
		GS::Array<API_Guid> all;
		ACAPI_Element_GetElemList (API_ObjectID, &all);
		collect (all, cabinets);
	}
	if (cabinets.empty ()) {
		DGAlert (DG_INFORMATION, "Kesim Listesi", U ("Projede mutfak dolabı yok."),
				 U ("Önce Mutfak > Mutfak Sihirbazı ile dolap oluşturun."), "Tamam");
		return NoError;
	}

	std::stable_sort (cabinets.begin (), cabinets.end (), [] (const CabinetParams& a, const CabinetParams& b) {
		if (a.duvarNo != b.duvarNo) return a.duvarNo < b.duvarNo;
		return TipOrder (a.tip) < TipOrder (b.tip);
	});

	std::vector<Part> parts;
	int seq = 0, lastWall = -1;
	for (const CabinetParams& c : cabinets) {
		if (c.duvarNo != lastWall) {
			seq = 0;
			lastWall = c.duvarNo;
		}
		++seq;
		char num[32];
		std::snprintf (num, sizeof (num), "%02d", seq);
		const std::string label = (c.duvarNo > 0 ? "D" + std::to_string (c.duvarNo) : std::string ("X")) + "-" + num + " " +
								  (c.modulKodu.empty () ? Cm (c.A) + " " + c.duzen : c.modulKodu);
		const std::vector<Part> cp = CutParts (c, label);
		parts.insert (parts.end (), cp.begin (), cp.end ());
	}
	const std::string csv = CutListCsv (parts);

	DG::FileDialog dialog (DG::FileDialog::Type::Save);
	FTM::FileTypeManager manager ("MutfakSihirbaziCSV");
	FTM::FileType type ("CSV (Excel)", "csv", 0, 0, -1);
	FTM::TypeID typeId = FTM::FileTypeManager::SearchForType (type);
	if (typeId == FTM::UnknownType)
		typeId = manager.AddType (type);
	dialog.AddFilter (typeId);
	dialog.SetTitle ("Kesim listesini kaydet");
	if (!dialog.Invoke ())
		return NoError;

	IO::Location loc = dialog.GetSelectedFile ();
	IO::File file (loc, IO::File::Create);
	GSErrCode err = file.Open (IO::File::WriteEmptyMode, IO::AccessDeniedIsError);
	if (err == NoError) {
		err = file.WriteBin (csv.data (), (USize) csv.size ());
		file.Close ();
	}
	if (err != NoError) {
		DGAlert (DG_ERROR, "Kesim Listesi", U ("Dosya yazılamadı."), "", "Tamam");
		return err;
	}

	int pieceCount = 0;
	for (const Part& p : parts)
		pieceCount += p.count;
	DGAlert (DG_INFORMATION, "Kesim Listesi",
			 GS::UniString::Printf (U ("%d dolap, %d parça kaydedildi."), (int) cabinets.size (), pieceCount),
			 fromSelection ? U ("Seçili dolaplardan oluşturuldu.") : U ("Projedeki tüm mutfak dolaplarından oluşturuldu."), "Tamam");
	return NoError;
}

// -----------------------------------------------------------------------------
//  Nesne aracına yükle
// -----------------------------------------------------------------------------
GSErrCode LoadCabinetAsDefault ()
{
	Int32 libInd = 0;
	GSErrCode err = GetCabinetLibInd (libInd);
	if (err != NoError || libInd <= 0) {
		DGAlert (DG_ERROR, U ("Mutfak Sihirbazı"), U ("Mutfak dolabı nesnesi bulunamadı."), "", "Tamam");
		return APIERR_GENERAL;
	}

	API_Element element {};
	API_Element mask;
	API_ElementMemo memo {};
	element.header.type = API_ObjectID;

	API_ParamOwnerType owner {};
	owner.libInd = libInd;
	if (ACAPI_LibraryPart_OpenParameters (&owner) == NoError) {
		API_GetParamsType getParams {};
		if (ACAPI_LibraryPart_GetActParameters (&getParams) == NoError) {
			ACAPI_DisposeAddParHdl (&memo.params);
			memo.params = getParams.params;
			element.object.xRatio = getParams.a;
			element.object.yRatio = getParams.b;
		}
		ACAPI_LibraryPart_CloseParameters ();
	}

	element.object.libInd = libInd;
	ACAPI_ELEMENT_MASK_CLEAR (mask);
	ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, libInd);
	ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, xRatio);
	ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, yRatio);

	err = ACAPI_Element_ChangeDefaults (&element, &memo, &mask);
	ACAPI_DisposeElemMemoHdls (&memo);
	if (err == NoError)
		DGAlert (DG_INFORMATION, U ("Mutfak Sihirbazı"), U ("Mutfak dolabı Nesne aracına yüklendi."),
				 U ("Nesne aracını seçip tıklayarak tek tek dolap yerleştirebilirsiniz."), "Tamam");
	return err;
}

// -----------------------------------------------------------------------------
//  Hata / uyarı gösterimi
// -----------------------------------------------------------------------------
bool ShowIssues (const std::vector<Issue>& issues, bool askToContinue)
{
	GS::UniString errors, warnings;
	int nErr = 0, nWarn = 0;
	for (const Issue& i : issues) {
		GS::UniString& target = i.error ? errors : warnings;
		(i.error ? nErr : nWarn)++;
		if ((i.error ? nErr : nWarn) <= 8) {
			target += U ("• ");
			target += ToUni (i.text);
			target += "\n";
		}
	}
	if (nErr > 8)	errors += GS::UniString::Printf (U ("… ve %d hata daha\n"), nErr - 8);
	if (nWarn > 8)	warnings += GS::UniString::Printf (U ("… ve %d uyarı daha\n"), nWarn - 8);

	if (nErr > 0) {
		GS::UniString body = errors;
		if (nWarn > 0) {
			body += U ("\nUyarılar:\n");
			body += warnings;
		}
		DGAlert (DG_ERROR, U ("Mutfak Sihirbazı – Kurallar"),
				 GS::UniString::Printf (U ("%d hata var, mutfak oluşturulamaz."), nErr), body, "Tamam");
		return false;
	}
	if (nWarn == 0)
		return true;
	if (!askToContinue) {
		DGAlert (DG_INFORMATION, U ("Mutfak Sihirbazı – Kurallar"), GS::UniString::Printf (U ("%d uyarı"), nWarn), warnings, "Tamam");
		return true;
	}
	const short answer = DGAlert (DG_WARNING, U ("Mutfak Sihirbazı – Kurallar"),
								  GS::UniString::Printf (U ("%d uyarı var. Yine de oluşturulsun mu?"), nWarn), warnings,
								  U ("Oluştur"), U ("Vazgeç"));
	return answer == DG_OK;
}

}	// namespace KitchenAPI
