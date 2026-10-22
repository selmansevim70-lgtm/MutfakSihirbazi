// =============================================================================
//  KitchenModel.cpp – Archicad API'den bağımsız mutfak hesaplama çekirdeği
// =============================================================================
#include "KitchenModel.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace Kitchen {

using namespace Rules;

// -----------------------------------------------------------------------------
//  Yardımcılar
// -----------------------------------------------------------------------------
static double FloorMm (double m)
{
	return std::floor (m * 1000.0 + 1e-6) / 1000.0;
}

std::string Cm (double meters)
{
	const double cm = std::round (meters * 1000.0) / 10.0;	// mm hassasiyeti
	char buf[32];
	if (std::fabs (cm - std::round (cm)) < 1e-6)
		std::snprintf (buf, sizeof (buf), "%d", (int) std::lround (cm));
	else
		std::snprintf (buf, sizeof (buf), "%.1f", cm);
	return buf;
}

static std::string Mm (double meters)
{
	return std::to_string ((long) std::lround (meters * 1000.0));
}

static const char* KindCode (Kind k)
{
	switch (k) {
		case Kind::Door:		return "K";
		case Kind::Drawers:		return "C";
		case Kind::DrawerDoor:	return "CK";
		case Kind::Sink:		return "E";
		case Kind::Dishwasher:	return "BM";
		case Kind::Oven:		return "F";
		case Kind::Fridge:		return "BZ";
		case Kind::Tall:		return "B";
		case Kind::BlindCorner:	return "KK";
		case Kind::Filler:		return "D";
		case Kind::Open:		return "A";
		case Kind::Gap:			return "-";
	}
	return "?";
}

static ModuleSpec Make (double width, Kind kind)
{
	ModuleSpec m;
	m.kind = kind;
	m.width = width;
	m.code = Cm (width) + KindCode (kind);
	return m;
}

std::string FormatCodes (const std::vector<ModuleSpec>& mods)
{
	std::string out;
	for (const ModuleSpec& m : mods) {
		if (!out.empty ())
			out += ' ';
		out += Cm (m.width) + KindCode (m.kind);
	}
	return out;
}

bool LayoutResult::HasError () const
{
	return std::any_of (issues.begin (), issues.end (), [] (const Issue& i) { return i.error; });
}

static void AddIssue (std::vector<Issue>& issues, bool error, const std::string& text)
{
	issues.push_back ({ error, text });
}

int AutoDoorCount (double width)
{
	int n = (int) std::ceil (width / MaxDoorWidth - 1e-6);
	if (n < 1)
		n = 1;
	while (n > 1 && width / n < MinDoorWidth)
		--n;
	return n;
}

// -----------------------------------------------------------------------------
//  Kod çözümleme
// -----------------------------------------------------------------------------
std::vector<ModuleSpec> ParseCodes (const std::string& text, Row row, std::vector<Issue>& issues, const std::string& context)
{
	static const std::map<std::string, Kind> baseMap = {
		{ "K", Kind::Door }, { "C", Kind::Drawers }, { "CK", Kind::DrawerDoor }, { "E", Kind::Sink },
		{ "BM", Kind::Dishwasher }, { "F", Kind::Oven }, { "BZ", Kind::Fridge }, { "B", Kind::Tall },
		{ "KK", Kind::BlindCorner }, { "D", Kind::Filler }, { "A", Kind::Open }, { "-", Kind::Gap }
	};
	static const std::map<std::string, Kind> upperMap = {
		{ "K", Kind::Door }, { "KK", Kind::BlindCorner }, { "D", Kind::Filler }, { "A", Kind::Open }, { "-", Kind::Gap }
	};
	const auto& map = (row == Row::Upper) ? upperMap : baseMap;

	std::vector<ModuleSpec> result;
	std::string normalized = text;
	for (char& c : normalized)
		if (c == ';' || c == '\t' || c == '\r' || c == '\n' || c == '|')
			c = ' ';

	std::istringstream in (normalized);
	std::string token;
	while (in >> token) {
		size_t i = 0;
		std::string num;
		while (i < token.size () && (std::isdigit ((unsigned char) token[i]) || token[i] == '.' || token[i] == ',')) {
			num += (token[i] == ',') ? '.' : token[i];
			++i;
		}
		std::string code;
		for (; i < token.size (); ++i)
			code += (char) std::toupper ((unsigned char) token[i]);

		if (num.empty () || code.empty ()) {
			AddIssue (issues, true, context + ": \"" + token + "\" anlaşılamadı (örnek: 60K, 80E, 45C).");
			continue;
		}
		const auto it = map.find (code);
		if (it == map.end ()) {
			AddIssue (issues, true, context + ": \"" + token + "\" içindeki \"" + code + "\" kodu " +
					  (row == Row::Upper ? "üst sırada " : "") + "geçerli değil.");
			continue;
		}
		double cm = 0.0;
		try { cm = std::stod (num); } catch (...) { cm = 0.0; }
		if (cm <= 0.0) {
			AddIssue (issues, true, context + ": \"" + token + "\" genişliği geçersiz.");
			continue;
		}
		result.push_back (Make (cm / 100.0, it->second));
	}
	return result;
}

// -----------------------------------------------------------------------------
//  Ayar kuralları
// -----------------------------------------------------------------------------
void CheckSettings (const Settings& s, std::vector<Issue>& issues)
{
	if (s.UnderCounter () + Tol < MinUnderCounter)
		AddIssue (issues, true, "Tezgah altı " + Cm (s.UnderCounter ()) + " cm. Kural: en az " + Cm (MinUnderCounter) +
				  " cm (alt dolap yüksekliğini artırın).");
	if (s.addUppers && s.upperBottom - s.CounterTop () + Tol < MinCounterToUpper)
		AddIssue (issues, true, "Tezgah ile üst dolap arası " + Cm (s.upperBottom - s.CounterTop ()) + " cm. Kural: en az " +
				  Cm (MinCounterToUpper) + " cm (üst dolap alt kotunu en az " + Cm (s.CounterTop () + MinCounterToUpper) + " cm yapın).");
	if (s.baseDepth < 0.30 || s.baseDepth > 0.80)
		AddIssue (issues, true, "Alt dolap derinliği 30–80 cm arasında olmalı.");
	if (s.upperDepth < 0.20 || s.upperDepth > 0.60)
		AddIssue (issues, true, "Üst dolap derinliği 20–60 cm arasında olmalı.");
	if (s.plinthHeight < 0.0 || s.plinthHeight > 0.20)
		AddIssue (issues, true, "Baza yüksekliği 0–20 cm arasında olmalı.");
}

// -----------------------------------------------------------------------------
//  Otomatik doldurma
// -----------------------------------------------------------------------------
std::string AutoFillBase (const Settings& s, const WallRun& run, int wallIndex, int wallCount)
{
	const double bStart = (run.startRole == CornerRole::Other) ? s.baseDepth : 0.0;
	const double bEnd   = run.length - ((run.endRole == CornerRole::Other) ? s.baseDepth : 0.0);
	const double avail  = FloorMm (bEnd - bStart);
	if (avail <= MinDoorWidth)
		return avail > 0.001 ? FormatCodes ({ Make (avail, Kind::Filler) }) : std::string ();

	const double korGen   = s.baseDepth + s.fillerWidth;
	const double kkWidth  = korGen + 0.40;

	std::vector<ModuleSpec> head, tail, core;
	if (run.startRole == CornerRole::Other)	head.push_back (Make (s.fillerWidth, Kind::Filler));
	if (run.startRole == CornerRole::Owner)	head.push_back (Make (kkWidth, Kind::BlindCorner));
	if (run.endRole == CornerRole::Owner)	tail.push_back (Make (kkWidth, Kind::BlindCorner));
	if (run.endRole == CornerRole::Other)	tail.push_back (Make (s.fillerWidth, Kind::Filler));

	if (wallCount <= 1) {
		core = { Make (0.60, Kind::Drawers), Make (0.60, Kind::Oven), Make (0.80, Kind::Sink), Make (0.60, Kind::Dishwasher) };
	} else if (wallIndex == 0) {
		core = { Make (0.80, Kind::Sink), Make (0.60, Kind::Dishwasher) };
	} else if (wallIndex == 1) {
		core = { Make (0.60, Kind::Oven), Make (0.60, Kind::Drawers) };
	} else {
		core = { Make (0.60, Kind::Drawers) };
	}

	// Buzdolabı: ilk duvarın serbest (köşesiz) ucuna
	bool fridgeAtStart = false, fridgeAtEnd = false;
	if (wallIndex == 0) {
		if (run.startRole == CornerRole::None)		fridgeAtStart = true;
		else if (run.endRole == CornerRole::None)	fridgeAtEnd = true;
	}
	const ModuleSpec fridge = Make (0.60, Kind::Fridge);

	auto sumOf = [] (const std::vector<ModuleSpec>& v) {
		double t = 0.0;
		for (const ModuleSpec& m : v) t += m.width;
		return t;
	};
	auto total = [&] () {
		return sumOf (head) + sumOf (tail) + sumOf (core) + ((fridgeAtStart || fridgeAtEnd) ? fridge.width : 0.0);
	};

	// Sığmayanları sırayla çıkar: önce çekirdekten, sonra buzdolabı, en son kör köşe
	while (total () > avail + Tol && !core.empty ())
		core.pop_back ();
	if (total () > avail + Tol)
		fridgeAtStart = fridgeAtEnd = false;
	if (total () > avail + Tol) {
		auto shrinkKK = [] (std::vector<ModuleSpec>& v) {
			for (ModuleSpec& m : v) if (m.kind == Kind::BlindCorner) m = Make (m.width, Kind::Filler);
		};
		shrinkKK (head);
		shrinkKK (tail);
	}

	std::vector<ModuleSpec> fills;
	double rem = FloorMm (avail - total ());
	if (rem > 0.001) {
		int n60 = (int) std::floor ((rem + Tol) / 0.60);
		double r = FloorMm (rem - n60 * 0.60);
		for (int i = 0; i < n60; ++i)
			fills.push_back (Make (0.60, Kind::Door));
		if (r > 0.001) {
			if (r >= 0.30)
				fills.push_back (Make (r, Kind::Door));
			else if (n60 > 0)
				fills.back () = Make (0.60 + r, Kind::Door);
			else if (r >= MinDoorWidth)
				fills.push_back (Make (r, Kind::Door));
			else
				fills.push_back (Make (r, Kind::Filler));
		}
	}

	std::vector<ModuleSpec> all;
	all.insert (all.end (), head.begin (), head.end ());
	if (fridgeAtStart) all.push_back (fridge);
	all.insert (all.end (), core.begin (), core.end ());
	all.insert (all.end (), fills.begin (), fills.end ());
	if (fridgeAtEnd) all.push_back (fridge);
	all.insert (all.end (), tail.begin (), tail.end ());
	return FormatCodes (all);
}

// -----------------------------------------------------------------------------
//  Modül parametreleri
// -----------------------------------------------------------------------------
static PlacedModule MakePlaced (const Settings& s, int wall, Row row, const ModuleSpec& spec, double s0)
{
	PlacedModule p;
	p.wall = wall;
	p.spec = spec;
	p.s0 = s0;
	p.s1 = s0 + spec.width;
	p.row = row;

	if (row == Row::Upper) {
		p.tip = "Üst Dolap";
		p.height = s.upperHeight;
		p.depth = s.upperDepth;
		p.level = s.upperBottom;
		p.tezgah = false;
		p.korGen = s.upperDepth + s.fillerWidth;
		switch (spec.kind) {
			case Kind::BlindCorner:	p.duzen = "Kör Köşe"; p.rafSayisi = 1; break;
			case Kind::Filler:		p.duzen = "Dolgu";    p.rafSayisi = 0; break;
			case Kind::Open:		p.duzen = "Açık Raf"; p.rafSayisi = 2; break;
			default:				p.duzen = "Kapaklı";  p.rafSayisi = 1; break;
		}
		return p;
	}

	p.tip = "Alt Dolap";
	p.height = s.baseHeight;
	p.depth = s.baseDepth;
	p.level = 0.0;
	p.korGen = s.baseDepth + s.fillerWidth;
	switch (spec.kind) {
		case Kind::Door:		p.duzen = "Kapaklı"; p.rafSayisi = 1; break;
		case Kind::Drawers:		p.duzen = "Çekmeceli"; p.cekmeceSayisi = 3; p.rafSayisi = 0; break;
		case Kind::DrawerDoor:	p.duzen = "Çekmece + Kapak"; p.rafSayisi = 1; break;
		case Kind::Sink:		p.duzen = "Kapaklı"; p.rafSayisi = 0; p.eviye = true; break;
		case Kind::Dishwasher:	p.duzen = "Bulaşık Makinesi"; p.rafSayisi = 0; break;
		case Kind::Oven:		p.duzen = "Ankastre Fırın"; p.rafSayisi = 0; p.ocak = true; break;
		case Kind::BlindCorner:	p.duzen = "Kör Köşe"; p.rafSayisi = 1; break;
		case Kind::Filler:		p.duzen = "Dolgu"; p.rafSayisi = 0; break;
		case Kind::Open:		p.duzen = "Açık Raf"; p.rafSayisi = 1; break;
		case Kind::Fridge:
			p.row = Row::Tall; p.tip = "Boy Dolap"; p.duzen = "Buzdolabı"; p.rafSayisi = 0;
			p.height = s.TallHeight (); p.tezgah = false;
			break;
		case Kind::Tall:
			p.row = Row::Tall; p.tip = "Boy Dolap"; p.duzen = "Kapaklı"; p.rafSayisi = 4;
			p.height = s.TallHeight (); p.tezgah = false;
			break;
		case Kind::Gap: break;
	}
	return p;
}

static bool HasDoors (const PlacedModule& p)
{
	return p.duzen == "Kapaklı" || p.duzen == "Çekmece + Kapak" || p.duzen == "Kör Köşe";
}

// Kapak kuralları + genişlik kontrolleri
static void CheckModule (PlacedModule& p, std::vector<Issue>& issues, const std::string& where)
{
	const double w = p.Width ();
	const std::string name = where + " " + p.spec.code;

	if (w > MaxModuleWidth + Tol)
		AddIssue (issues, true, name + ": modül " + Cm (MaxModuleWidth) + " cm'den geniş olamaz (ikiye bölün).");

	if (p.duzen == "Kör Köşe") {
		const double doorZone = w - p.korGen;
		if (doorZone + Tol < MinDoorWidth)
			AddIssue (issues, true, name + ": kör köşede kapak " + Cm (doorZone) + " cm kalıyor. Kural: kapak en az " +
					  Cm (MinDoorWidth) + " cm (modülü en az " + Cm (p.korGen + MinDoorWidth) + " cm yapın).");
		else
			p.kapakSayisi = AutoDoorCount (doorZone);
		return;
	}

	if (HasDoors (p)) {
		if (w + Tol < MinDoorWidth) {
			AddIssue (issues, false, name + ": " + Cm (w) + " cm'lik kapak olamaz (kural: en az 19 cm). Dolguya çevrildi.");
			p.duzen = "Dolgu";
			p.rafSayisi = 0;
			p.spec.kind = Kind::Filler;
			p.spec.code = Cm (w) + "D";
			return;
		}
		p.kapakSayisi = AutoDoorCount (w);
	}

	if (p.spec.kind == Kind::Dishwasher && std::fabs (w - 0.60) > 0.005 && std::fabs (w - 0.45) > 0.005)
		AddIssue (issues, false, name + ": bulaşık makinesi modülü genelde 60 veya 45 cm olur.");
	if (p.spec.kind == Kind::Oven && std::fabs (w - 0.60) > 0.005)
		AddIssue (issues, false, name + ": ankastre fırın modülü 60 cm olmalı.");
	if (p.spec.kind == Kind::Fridge && w + Tol < 0.60)
		AddIssue (issues, false, name + ": buzdolabı için en az 60 cm gerekir.");
	if (p.spec.kind == Kind::Drawers && w + Tol < 0.30)
		AddIssue (issues, false, name + ": 30 cm'den dar çekmece kullanışsızdır.");
}

static void SetBlindSide (PlacedModule& p, double runStart, double runEnd, const WallRun& run)
{
	if (p.duzen != "Kör Köşe")
		return;
	const bool atStart = std::fabs (p.s0 - runStart) < 0.002;
	const bool atEnd   = std::fabs (p.s1 - runEnd) < 0.002;
	if (atStart && run.startRole == CornerRole::Owner)		p.korTaraf = "Sol";
	else if (atEnd && run.endRole == CornerRole::Owner)		p.korTaraf = "Sağ";
	else p.korTaraf = (p.s0 - runStart < runEnd - p.s1) ? "Sol" : "Sağ";
}

// -----------------------------------------------------------------------------
//  Yerleşim
// -----------------------------------------------------------------------------
LayoutResult Layout (const Settings& s, const std::vector<WallRun>& runs)
{
	LayoutResult res;
	CheckSettings (s, res.issues);

	for (int w = 0; w < (int) runs.size (); ++w) {
		const WallRun& run = runs[w];
		const std::string where = "Duvar " + std::to_string (w + 1);

		const double bStart = (run.startRole == CornerRole::Other) ? s.baseDepth : 0.0;
		const double bEnd   = run.length - ((run.endRole == CornerRole::Other) ? s.baseDepth : 0.0);
		const double uStart = (run.startRole == CornerRole::Other) ? s.upperDepth : 0.0;
		const double uEnd   = run.length - ((run.endRole == CornerRole::Other) ? s.upperDepth : 0.0);

		// ---- Alt + boy sıra
		std::vector<ModuleSpec> baseSpecs = ParseCodes (run.baseCodes, Row::Base, res.issues, where);
		double x = bStart;
		std::vector<PlacedModule> base;
		std::vector<std::pair<double, double>> tallZones;
		for (const ModuleSpec& m : baseSpecs) {
			if (m.kind != Kind::Gap) {
				PlacedModule p = MakePlaced (s, w, Row::Base, m, x);
				CheckModule (p, res.issues, where);
				if (p.row == Row::Tall)
					tallZones.push_back ({ p.s0, p.s1 });
				base.push_back (p);
			}
			x += m.width;
		}
		const double used = x - bStart;
		const double avail = bEnd - bStart;
		if (used > avail + Tol) {
			AddIssue (res.issues, true, where + ": alt dolaplar " + Cm (used) + " cm, duvarda " + Cm (avail) +
					  " cm yer var. " + Cm (used - avail) + " cm taşıyor – dolaplar iç içe girer.");
		} else if (avail - used > 0.005) {
			AddIssue (res.issues, false, where + ": sonda " + Cm (avail - used) + " cm boş kaldı (dolgu ekleyebilirsiniz: " +
					  Cm (avail - used) + "D).");
		}
		for (PlacedModule& p : base)
			SetBlindSide (p, bStart, bEnd, run);

		// Bulaşık makinesi eviyenin yanında mı?
		for (size_t i = 0; i < base.size (); ++i) {
			if (base[i].spec.kind != Kind::Dishwasher)
				continue;
			bool nearSink = false;
			for (size_t j = 0; j < base.size (); ++j) {
				if (base[j].spec.kind != Kind::Sink)
					continue;
				if (std::fabs (base[j].s1 - base[i].s0) < 0.002 || std::fabs (base[i].s1 - base[j].s0) < 0.002)
					nearSink = true;
			}
			if (!nearSink)
				AddIssue (res.issues, false, where + ": bulaşık makinesi eviyenin yanında değil (tesisat için yan yana olmalı).");
		}

		// ---- Üst sıra
		std::vector<PlacedModule> upper;
		if (!run.upperCodes.empty ()) {
			std::vector<ModuleSpec> upSpecs = ParseCodes (run.upperCodes, Row::Upper, res.issues, where + " (üst)");
			double ux = uStart;
			for (const ModuleSpec& m : upSpecs) {
				if (m.kind != Kind::Gap)
					upper.push_back (MakePlaced (s, w, Row::Upper, m, ux));
				ux += m.width;
			}
			if (ux - uStart > uEnd - uStart + Tol)
				AddIssue (res.issues, true, where + ": üst dolaplar " + Cm (ux - uStart - (uEnd - uStart)) + " cm taşıyor.");
			for (const PlacedModule& u : upper)
				for (const auto& z : tallZones)
					if (u.s0 < z.second - Tol && u.s1 > z.first + Tol)
						AddIssue (res.issues, true, where + ": üst dolap " + u.spec.code + " boy dolapla çakışıyor (boy dolabın üstüne \"" +
								  Cm (z.second - z.first) + "-\" boşluk koyun).");
		} else if (s.addUppers) {
			for (const PlacedModule& b : base) {
				if (b.row == Row::Tall)
					continue;
				ModuleSpec m = b.spec;
				if (m.kind != Kind::Filler && m.kind != Kind::BlindCorner)
					m.kind = Kind::Door;
				upper.push_back (MakePlaced (s, w, Row::Upper, m, b.s0));
			}
			// Köşe uçlarını üst dolap derinliğine göre uzat
			if (!upper.empty () && run.startRole == CornerRole::Other && std::fabs (upper.front ().s0 - bStart) < 0.002) {
				PlacedModule& f = upper.front ();
				if (f.spec.kind == Kind::Filler && upper.size () > 1 && std::fabs (upper[1].s0 - f.s1) < 0.002) {
					f.s0 = uStart;
					f.s1 = uStart + s.fillerWidth;
					upper[1].s0 = f.s1;
				} else {
					f.s0 = uStart;
				}
			}
			if (!upper.empty () && run.endRole == CornerRole::Other && std::fabs (upper.back ().s1 - bEnd) < 0.002) {
				PlacedModule& l = upper.back ();
				const size_t n = upper.size ();
				if (l.spec.kind == Kind::Filler && n > 1 && std::fabs (upper[n - 2].s1 - l.s0) < 0.002) {
					l.s1 = uEnd;
					l.s0 = uEnd - s.fillerWidth;
					upper[n - 2].s1 = l.s0;
				} else {
					l.s1 = uEnd;
				}
			}
			// Çok genişleri böl
			std::vector<PlacedModule> split;
			for (PlacedModule u : upper) {
				const int parts = (int) std::ceil (u.Width () / MaxModuleWidth - 1e-6);
				if (parts <= 1 || u.spec.kind != Kind::Door) {
					split.push_back (u);
					continue;
				}
				const double pw = u.Width () / parts;
				for (int k = 0; k < parts; ++k) {
					PlacedModule q = u;
					q.s0 = u.s0 + k * pw;
					q.s1 = q.s0 + pw;
					split.push_back (q);
				}
			}
			upper = split;
			for (PlacedModule& u : upper)
				u.spec.code = Cm (u.Width ()) + KindCode (u.spec.kind);
		}
		for (PlacedModule& u : upper) {
			CheckModule (u, res.issues, where + " (üst)");
			SetBlindSide (u, uStart, uEnd, run);
		}

		res.modules.insert (res.modules.end (), base.begin (), base.end ());
		res.modules.insert (res.modules.end (), upper.begin (), upper.end ());
	}
	return res;
}

// -----------------------------------------------------------------------------
//  Geometri
// -----------------------------------------------------------------------------
static Vec2   Add (Vec2 a, Vec2 b)		{ return { a.x + b.x, a.y + b.y }; }
static Vec2   Sub (Vec2 a, Vec2 b)		{ return { a.x - b.x, a.y - b.y }; }
static Vec2   Mul (Vec2 a, double k)	{ return { a.x * k, a.y * k }; }
static double Dot (Vec2 a, Vec2 b)		{ return a.x * b.x + a.y * b.y; }
static double Cross (Vec2 a, Vec2 b)	{ return a.x * b.y - a.y * b.x; }
static double Len (Vec2 a)				{ return std::sqrt (Dot (a, a)); }

bool ComputeFaces (const std::vector<WallInput>& walls, bool flip, std::vector<WallFace>& faces, std::vector<Issue>& issues)
{
	faces.clear ();
	Vec2 centroid;
	for (const WallInput& w : walls)
		centroid = Add (centroid, Mul (Add (w.begC, w.endC), 0.5));
	if (!walls.empty ())
		centroid = Mul (centroid, 1.0 / walls.size ());

	bool ok = true;
	for (size_t i = 0; i < walls.size (); ++i) {
		const WallInput& w = walls[i];
		const std::string where = "Duvar " + std::to_string (i + 1);
		const Vec2 d = Sub (w.endC, w.begC);
		const double L = Len (d);
		if (L < 0.05) {
			AddIssue (issues, true, where + ": duvar çok kısa.");
			ok = false;
			faces.push_back ({});
			continue;
		}
		const Vec2 dir = Mul (d, 1.0 / L);
		const Vec2 nLeft = { -dir.y, dir.x };

		struct Edge { double off, t0, t1; };
		std::vector<Edge> edges;
		for (const auto& ring : w.contours) {
			for (size_t k = 0; k + 1 < ring.size (); ++k) {
				const Vec2 a = ring[k], b = ring[k + 1];
				const Vec2 e = Sub (b, a);
				const double el = Len (e);
				if (el < 1e-4 || std::fabs (Cross (dir, e)) / el > 0.01)
					continue;
				const double ta = Dot (Sub (a, w.begC), dir), tb = Dot (Sub (b, w.begC), dir);
				edges.push_back ({ Dot (Sub (a, w.begC), nLeft), std::min (ta, tb), std::max (ta, tb) });
			}
		}
		if (edges.empty ()) {
			AddIssue (issues, true, where + ": düz duvar değil (eğri veya poligon duvarlar desteklenmiyor).");
			ok = false;
			faces.push_back ({});
			continue;
		}
		double offMin = edges[0].off, offMax = edges[0].off;
		for (const Edge& e : edges) {
			offMin = std::min (offMin, e.off);
			offMax = std::max (offMax, e.off);
		}
		if (offMax - offMin < 0.01) {
			AddIssue (issues, true, where + ": duvar kalınlığı okunamadı.");
			ok = false;
			faces.push_back ({});
			continue;
		}

		bool useLeft = true;
		if (walls.size () >= 2) {
			const Vec2 mid = Mul (Add (w.begC, w.endC), 0.5);
			const double sgn = Dot (Sub (centroid, mid), nLeft);
			useLeft = (sgn >= -1e-6);
		}
		if (flip)
			useLeft = !useLeft;

		const double off = useLeft ? offMax : offMin;
		double tmin = 1e30, tmax = -1e30;
		for (const Edge& e : edges) {
			if (std::fabs (e.off - off) < 0.001) {
				tmin = std::min (tmin, e.t0);
				tmax = std::max (tmax, e.t1);
			}
		}
		const Vec2 pa = Add (Add (w.begC, Mul (dir, tmin)), Mul (nLeft, off));
		const Vec2 pb = Add (Add (w.begC, Mul (dir, tmax)), Mul (nLeft, off));

		WallFace f;
		if (useLeft) {
			f.p0 = pb; f.p1 = pa; f.u = Mul (dir, -1.0); f.n = nLeft;
		} else {
			f.p0 = pa; f.p1 = pb; f.u = dir; f.n = Mul (nLeft, -1.0);
		}
		f.length = tmax - tmin;
		faces.push_back (f);
	}
	return ok;
}

void AssignCorners (const std::vector<WallFace>& faces, std::vector<WallRun>& runs, double tol)
{
	for (size_t i = 0; i < faces.size (); ++i) {
		for (size_t j = i + 1; j < faces.size (); ++j) {
			for (int ei = 0; ei < 2; ++ei) {
				for (int ej = 0; ej < 2; ++ej) {
					const Vec2 a = ei == 0 ? faces[i].p0 : faces[i].p1;
					const Vec2 b = ej == 0 ? faces[j].p0 : faces[j].p1;
					if (Len (Sub (a, b)) > tol)
						continue;
					CornerRole& ri = ei == 0 ? runs[i].startRole : runs[i].endRole;
					CornerRole& rj = ej == 0 ? runs[j].startRole : runs[j].endRole;
					if (ri == CornerRole::None && rj == CornerRole::None) {
						ri = CornerRole::Owner;
						rj = CornerRole::Other;
					}
				}
			}
		}
	}
}

void ModuleOrigin (const WallFace& f, const PlacedModule& m, Vec2& pos, double& angle)
{
	pos = Add (Add (f.p0, Mul (f.u, m.s0)), Mul (f.n, m.depth));
	angle = std::atan2 (f.u.y, f.u.x);
}

static std::vector<Vec2> Footprint (const WallFace& f, const PlacedModule& m)
{
	const double e = 0.002;
	const Vec2 a = Add (Add (f.p0, Mul (f.u, m.s0 + e)), Mul (f.n, e));
	const Vec2 b = Add (Add (f.p0, Mul (f.u, m.s1 - e)), Mul (f.n, e));
	const Vec2 c = Add (b, Mul (f.n, m.depth - 2 * e));
	const Vec2 d = Add (a, Mul (f.n, m.depth - 2 * e));
	return { a, b, c, d };
}

static bool ConvexOverlap (const std::vector<Vec2>& p, const std::vector<Vec2>& q)
{
	for (const auto* poly : { &p, &q }) {
		for (size_t i = 0; i < poly->size (); ++i) {
			const Vec2 e = Sub ((*poly)[(i + 1) % poly->size ()], (*poly)[i]);
			const Vec2 axis = { -e.y, e.x };
			double pmin = 1e30, pmax = -1e30, qmin = 1e30, qmax = -1e30;
			for (const Vec2& v : p) { pmin = std::min (pmin, Dot (v, axis)); pmax = std::max (pmax, Dot (v, axis)); }
			for (const Vec2& v : q) { qmin = std::min (qmin, Dot (v, axis)); qmax = std::max (qmax, Dot (v, axis)); }
			if (pmax <= qmin || qmax <= pmin)
				return false;
		}
	}
	return true;
}

void CheckCollisions (const std::vector<WallFace>& faces, const std::vector<PlacedModule>& mods, std::vector<Issue>& issues)
{
	int reported = 0;
	for (size_t i = 0; i < mods.size () && reported < 6; ++i) {
		for (size_t j = i + 1; j < mods.size () && reported < 6; ++j) {
			const PlacedModule& a = mods[i];
			const PlacedModule& b = mods[j];
			if (a.wall == b.wall)
				continue;
			if (a.level + a.height <= b.level + 0.001 || b.level + b.height <= a.level + 0.001)
				continue;
			if (!ConvexOverlap (Footprint (faces[a.wall], a), Footprint (faces[b.wall], b)))
				continue;
			AddIssue (issues, true, "Duvar " + std::to_string (a.wall + 1) + " " + a.spec.code + " ile Duvar " +
					  std::to_string (b.wall + 1) + " " + b.spec.code + " iç içe giriyor. Köşede birinin önüne " +
					  "dolgu/boşluk ekleyin veya duvarların birleşimini kontrol edin.");
			++reported;
		}
	}
}

// -----------------------------------------------------------------------------
//  Kesim listesi (GDL nesnesinin 3B geometrisiyle birebir aynı kurallar)
// -----------------------------------------------------------------------------
static std::string MatName (const char* base, double t)
{
	return std::string (base) + " " + Mm (t) + "mm";
}

std::vector<Part> CutParts (const CabinetParams& p, const std::string& label)
{
	std::vector<Part> parts;
	auto add = [&] (const std::string& name, const std::string& mat, double t, double l, double w, int n,
					const std::string& edges, double edgeLenEach) {
		if (n <= 0 || l <= 0.001 || w <= 0.001)
			return;
		Part pt;
		pt.cabinet = label;
		pt.name = name;
		pt.material = mat;
		pt.thickness = t;
		pt.length = std::max (l, w);
		pt.width = std::min (l, w);
		pt.count = n;
		pt.edges = edges;
		pt.edgeMeters = edgeLenEach * n;
		parts.push_back (pt);
	};

	const bool tipAlt = p.tip == "Alt Dolap";
	const bool tipUst = p.tip == "Üst Dolap";
	const bool fridge = p.duzen == "Buzdolabı";
	const bool dish   = p.duzen == "Bulaşık Makinesi";
	const bool filler = p.duzen == "Dolgu";

	const double gk = p.govdeKal, kk = p.kapakKal, bs = p.kapakBosluk;
	const bool   baza = !tipUst && !fridge;
	const double zAlt = baza ? p.bazaYuk : 0.0;
	const bool   tezgah = tipAlt && p.tezgahVar;
	const double zUst = tezgah ? p.ZZYZX - p.tezgahKal : p.ZZYZX;
	const double gH = zUst - zAlt;
	const double gD = p.B - kk;
	const double iW = p.A - 2 * gk;

	const std::string mG = MatName ("Gövde", gk);
	const std::string mK = MatName ("Kapak", kk);
	const std::string mH = "Arkalık 8mm HDF";
	const std::string mT = MatName ("Tezgah", p.tezgahKal);

	auto doors = [&] (double width, double height, int n, const std::string& name) {
		if (n <= 0) n = AutoDoorCount (width);
		const double dw = width / n - bs, dh = height - bs;
		add (name, mK, kk, dh, dw, n, "4 kenar", 2 * (dw + dh));
	};

	if (filler) {
		add ("Dolgu paneli", mK, kk, gH, p.A, 1, "2 uzun", 2 * gH);
	} else if (fridge) {
		const double zBox = p.buzdolabiYuk + 0.03;
		const double boxH = p.ZZYZX - zBox;
		add ("Yan (boy)", mG, gk, p.ZZYZX, gD, 2, "1 uzun (ön)", p.ZZYZX);
		if (boxH > 0.10) {
			add ("Alt/Üst tabla (üst kutu)", mG, gk, iW, gD, 2, "1 uzun (ön)", iW);
			add ("Arkalık (üst kutu)", mH, 0.008, iW, boxH - 2 * gk, 1, "-", 0.0);
			doors (p.A, boxH, p.kapakSayisi, "Kapak (buzdolabı üstü)");
		}
	} else if (dish) {
		add ("Bulaşık makinesi paneli", mK, kk, gH - bs, p.A - bs, 1, "4 kenar", 2 * (gH + p.A - 2 * bs));
	} else {
		// Gövde
		add ("Yan", mG, gk, gH, gD, 2, "1 uzun (ön)", gH);
		add ("Alt/Üst tabla", mG, gk, iW, gD, 2, "1 uzun (ön)", iW);
		add ("Arkalık", mH, 0.008, iW, gH - 2 * gk, 1, "-", 0.0);

		const bool shelves = p.duzen == "Kapaklı" || p.duzen == "Çekmece + Kapak" || p.duzen == "Kör Köşe" || p.duzen == "Açık Raf";
		if (shelves && p.rafSayisi > 0)
			add ("Raf", mG, gk, iW, gD - 0.028, p.rafSayisi, "1 uzun (ön)", iW);

		if (p.duzen == "Kapaklı") {
			doors (p.A, gH, p.kapakSayisi, "Kapak");
		} else if (p.duzen == "Çekmeceli") {
			const int m = std::max (1, p.cekmeceSayisi);
			const double fh = gH / m - bs, fw = p.A - bs;
			add ("Çekmece önü", mK, kk, fh, fw, m, "4 kenar", 2 * (fh + fw));
		} else if (p.duzen == "Çekmece + Kapak") {
			const double fh = p.ustCekmeceYuk - bs, fw = p.A - bs;
			add ("Çekmece önü", mK, kk, fh, fw, 1, "4 kenar", 2 * (fh + fw));
			doors (p.A, gH - p.ustCekmeceYuk, p.kapakSayisi, "Kapak");
		} else if (p.duzen == "Kör Köşe") {
			const double pw = p.korGen - bs, ph = gH - bs;
			add ("Kör köşe paneli", mK, kk, ph, pw, 1, "4 kenar", 2 * (pw + ph));
			doors (p.A - p.korGen, gH, p.kapakSayisi, "Kapak");
		} else if (p.duzen == "Ankastre Fırın") {
			const double dh = gH - 0.595 - bs, dw = p.A - bs;
			add ("Çekmece önü (fırın altı)", mK, kk, dh, dw, 1, "4 kenar", 2 * (dh + dw));
		}
	}

	if (baza)
		add ("Baza", mG, gk, p.A, p.bazaYuk, 1, "1 uzun", p.A);
	if (tezgah)
		add ("Tezgah", mT, p.tezgahKal, p.A, p.B + p.tezgahTasma, 1, "1 uzun (ön)", p.A);

	return parts;
}

static std::string Dec (double v, int digits)
{
	char buf[32];
	std::snprintf (buf, sizeof (buf), "%.*f", digits, v);
	std::string s = buf;
	std::replace (s.begin (), s.end (), '.', ',');
	return s;
}

std::string CutListCsv (const std::vector<Part>& parts)
{
	std::ostringstream out;
	out << "\xEF\xBB\xBF";	// UTF-8 BOM (Excel Türkçe karakterleri doğru açsın)
	out << "Dolap;Parça;Malzeme;Kalınlık (mm);Boy (mm);En (mm);Adet;Bant;Bant (m)\r\n";

	struct Sum { double area = 0.0; int count = 0; double edge = 0.0; };
	std::map<std::string, Sum> sums;
	double edgeTotal = 0.0;

	for (const Part& p : parts) {
		out << p.cabinet << ';' << p.name << ';' << p.material << ';' << Mm (p.thickness) << ';'
			<< Mm (p.length) << ';' << Mm (p.width) << ';' << p.count << ';' << p.edges << ';' << Dec (p.edgeMeters, 2) << "\r\n";
		Sum& s = sums[p.material];
		s.area += p.length * p.width * p.count;
		s.count += p.count;
		s.edge += p.edgeMeters;
		edgeTotal += p.edgeMeters;
	}

	out << "\r\nÖZET\r\nMalzeme;Parça adedi;Alan (m²);Bant (m)\r\n";
	for (const auto& kv : sums)
		out << kv.first << ';' << kv.second.count << ';' << Dec (kv.second.area, 2) << ';' << Dec (kv.second.edge, 2) << "\r\n";
	out << "Toplam bant;;;" << Dec (edgeTotal, 2) << "\r\n";
	out << "\r\nNot: Levha fire payı dahil değildir. Çekmece kutuları ve cihazlar listede yoktur.\r\n";
	return out.str ();
}

}	// namespace Kitchen
