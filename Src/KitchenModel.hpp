// =============================================================================
//  KitchenModel.hpp
//  Mutfak Sihirbazı – Archicad API'den bağımsız hesaplama çekirdeği.
//  (Modül kodları, otomatik doldurma, usta kuralları, köşe çözümü,
//   çakışma kontrolü, kesim listesi.)
//  Bu dosya yalnızca standart C++ kullanır; Archicad olmadan test edilebilir.
//  Tüm uzunluklar metre cinsindendir.
// =============================================================================
#ifndef KITCHEN_MODEL_HPP
#define KITCHEN_MODEL_HPP

#include <string>
#include <vector>

namespace Kitchen {

// -----------------------------------------------------------------------------
//  Genel ayarlar (diyalogdan gelir)
// -----------------------------------------------------------------------------
struct Settings {
	double baseHeight      = 0.91;	// alt dolap toplam yükseklik (tezgah dahil)
	double baseDepth       = 0.60;	// alt dolap derinliği (kapak dahil)
	double counterThick    = 0.04;	// tezgah kalınlığı
	double counterOverhang = 0.02;	// tezgah ön taşma
	double plinthHeight    = 0.10;	// baza yüksekliği
	double upperBottom     = 1.50;	// üst dolap alt kotu (kat seviyesinden)
	double upperHeight     = 0.72;	// üst dolap yüksekliği
	double upperDepth      = 0.35;	// üst dolap derinliği
	double fillerWidth     = 0.05;	// köşe dolgu genişliği (kulp payı)
	bool   addUppers       = true;	// üst dolapları otomatik ekle
	bool   flipSide        = false;	// dolapları duvarın diğer yüzüne yerleştir

	double TallHeight () const { return upperBottom + upperHeight; }
	double CounterTop () const { return baseHeight; }
	double UnderCounter () const { return baseHeight - counterThick; }
};

// Usta kuralları (KitchenFun'daki kurallarla aynı mantık)
namespace Rules {
	constexpr double MinUnderCounter   = 0.87;	// tezgah altı en az 87 cm
	constexpr double MinCounterToUpper = 0.58;	// tezgah – üst dolap arası en az 58 cm
	constexpr double MinDoorWidth      = 0.19;	// kapak 19 cm'den dar olamaz
	constexpr double MaxDoorWidth      = 0.60;	// tek kapak en fazla 60 cm
	constexpr double MaxModuleWidth    = 1.20;	// tek modül en fazla 120 cm
	constexpr double Tol               = 0.0005;
}

// -----------------------------------------------------------------------------
//  Modül tipleri
// -----------------------------------------------------------------------------
enum class Row { Base, Upper, Tall };

enum class Kind {
	Door,			// K   kapaklı
	Drawers,		// C   çekmeceli
	DrawerDoor,		// CK  çekmece + kapak
	Sink,			// E   eviye dolabı
	Dishwasher,		// BM  bulaşık makinesi
	Oven,			// F   ankastre fırın (+ ocak)
	Fridge,			// BZ  buzdolabı (boy)
	Tall,			// B   boy dolap
	BlindCorner,	// KK  kör köşe
	Filler,			// D   dolgu
	Open,			// A   açık raf
	Gap				// -   boşluk (dolap yok)
};

struct ModuleSpec {
	Kind   kind  = Kind::Door;
	double width = 0.60;
	std::string code;			// kullanıcıya gösterilen kod, ör. "60K"
};

// Bir duvar üzerinde yerleşmiş modül (duvar yüzü boyunca s0..s1)
struct PlacedModule {
	int        wall = 0;
	Row        row  = Row::Base;
	ModuleSpec spec;
	double     s0 = 0.0;
	double     s1 = 0.0;

	// GDL parametreleri
	std::string tip;			// "Alt Dolap" / "Üst Dolap" / "Boy Dolap"
	std::string duzen;			// "Kapaklı", "Çekmeceli", ...
	int    kapakSayisi   = 0;	// 0 = otomatik
	int    cekmeceSayisi = 3;
	int    rafSayisi     = 1;
	bool   eviye = false;
	bool   ocak  = false;
	bool   tezgah = true;
	double korGen = 0.65;
	std::string korTaraf = "Sağ";
	double height = 0.0;		// ZZYZX
	double depth  = 0.0;		// B
	double level  = 0.0;		// kat seviyesinden alt kot

	double Width () const { return s1 - s0; }
};

enum class CornerRole { None, Owner, Other };

struct WallRun {
	double     length = 0.0;			// iç yüz uzunluğu
	CornerRole startRole = CornerRole::None;
	CornerRole endRole   = CornerRole::None;
	std::string baseCodes;				// "60BZ 60K 80E 60BM"
	std::string upperCodes;				// boşsa otomatik
};

struct Issue {
	bool        error = false;			// true: hata (oluşturma yapılmaz), false: uyarı
	std::string text;
};

struct LayoutResult {
	std::vector<PlacedModule> modules;
	std::vector<Issue>        issues;
	bool HasError () const;
};

// -----------------------------------------------------------------------------
//  API
// -----------------------------------------------------------------------------

// "60K 80E 60BM" → modüller. Hatalı kodlar issues'a yazılır.
std::vector<ModuleSpec> ParseCodes (const std::string& text, Row row, std::vector<Issue>& issues, const std::string& context);

// Duvar uzunluğu ve köşe rollerine göre otomatik alt dolap dizisi üretir.
std::string AutoFillBase (const Settings& s, const WallRun& run, int wallIndex, int wallCount);

// Genel ayarların kurallara uygunluğu
void CheckSettings (const Settings& s, std::vector<Issue>& issues);

// Tüm duvarlar için yerleşim + kurallar
LayoutResult Layout (const Settings& s, const std::vector<WallRun>& runs);

// Format yardımcıları
std::string Cm (double meters);					// 0.6 → "60"
std::string FormatCodes (const std::vector<ModuleSpec>& mods);

// -----------------------------------------------------------------------------
//  Kesim listesi
// -----------------------------------------------------------------------------
struct CabinetParams {			// Archicad'deki nesneden okunan değerler
	std::string modulKodu;
	int    duvarNo = 0;
	double A = 0.6, B = 0.6, ZZYZX = 0.9;
	std::string tip   = "Alt Dolap";
	std::string duzen = "Kapaklı";
	int    kapakSayisi = 0, cekmeceSayisi = 3, rafSayisi = 1;
	double ustCekmeceYuk = 0.15;
	double govdeKal = 0.018, kapakKal = 0.018, kapakBosluk = 0.003;
	double bazaYuk = 0.10;
	bool   tezgahVar = true;
	double tezgahKal = 0.04, tezgahTasma = 0.02;
	double korGen = 0.65;
	double buzdolabiYuk = 1.85;
};

struct Part {
	std::string cabinet;			// "D1-03 60K"
	std::string name;				// "Yan", "Kapak", ...
	std::string material;			// "Gövde 18mm", ...
	double      thickness = 0.018;
	double      length = 0.0;		// boy (m)
	double      width  = 0.0;		// en  (m)
	int         count = 1;
	std::string edges;				// bant bilgisi
	double      edgeMeters = 0.0;	// toplam bant (adet dahil)
};

// -----------------------------------------------------------------------------
//  Geometri (plan)
// -----------------------------------------------------------------------------
struct Vec2 {
	double x = 0.0, y = 0.0;
};

// Duvarın iç yüzü: p0 → p1, oda u yönünün SAĞINDA kalır; n = odaya bakan normal.
struct WallFace {
	Vec2   p0, p1;
	Vec2   u, n;
	double length = 0.0;
};

struct WallInput {
	Vec2 begC, endC;
	std::vector<std::vector<Vec2>> contours;	// duvar poligonu (kapalı halkalar)
};

// Duvarların iç yüzlerini bulur (odaya bakan taraf: seçili duvarların ağırlık
// merkezine bakan yüz; tek duvarda sol yüz). flip = tüm yüzleri ters çevir.
bool ComputeFaces (const std::vector<WallInput>& walls, bool flip, std::vector<WallFace>& faces, std::vector<Issue>& issues);

// Yüz uçlarının birbirine değdiği köşeleri bulur, köşe rollerini atar
// (küçük indeksli duvar köşenin sahibidir).
void AssignCorners (const std::vector<WallFace>& faces, std::vector<WallRun>& runs, double tol = 0.02);

// Farklı duvarlardaki dolapların plan + yükseklik çakışmasını denetler.
void CheckCollisions (const std::vector<WallFace>& faces, const std::vector<PlacedModule>& mods, std::vector<Issue>& issues);

// Modülün Archicad'deki yerleşimi: köken (ön-sol köşe), açı
void ModuleOrigin (const WallFace& f, const PlacedModule& m, Vec2& pos, double& angle);

int  AutoDoorCount (double width);
std::vector<Part> CutParts (const CabinetParams& p, const std::string& label);
std::string CutListCsv (const std::vector<Part>& parts);

}	// namespace Kitchen

#endif
