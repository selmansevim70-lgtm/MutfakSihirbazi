// Archicad olmadan çekirdek testi:  g++ -std=c++20 -I../Src ModelTest.cpp ../Src/KitchenModel.cpp
#include "KitchenModel.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace Kitchen;

static void Print (const char* title, const std::vector<WallRun>& runs, const LayoutResult& r)
{
	std::printf ("\n===== %s\n", title);
	for (size_t i = 0; i < runs.size (); ++i)
		std::printf ("Duvar %zu  L=%s  roller=%d/%d  alt: %s\n", i + 1, Cm (runs[i].length).c_str (),
					 (int) runs[i].startRole, (int) runs[i].endRole, runs[i].baseCodes.c_str ());
	for (const auto& m : r.modules)
		std::printf ("  D%d %-6s %-6s %-10s %-18s s=%6.3f..%6.3f kapak=%d kor=%s/%.2f\n", m.wall + 1,
					 m.row == Row::Base ? "alt" : m.row == Row::Upper ? "ust" : "boy", m.spec.code.c_str (), m.tip.c_str (),
					 m.duzen.c_str (), m.s0, m.s1, m.kapakSayisi, m.korTaraf.c_str (), m.korGen);
	for (const auto& i : r.issues)
		std::printf ("  %s %s\n", i.error ? "HATA :" : "uyarı:", i.text.c_str ());
}

int main ()
{
	Settings s;
	// 1) Düz mutfak 3.60
	{
		std::vector<WallRun> runs (1); runs[0].length = 3.60;
		runs[0].baseCodes = AutoFillBase (s, runs[0], 0, 1);
		auto r = Layout (s, runs);
		Print ("Düz 3.60", runs, r);
		assert (!r.HasError ());
	}
	// 2) L mutfak: gönyeli birleşim, oda x>0.2, y>0.2
	{
		WallInput w1, w2;
		w1.begC = { 0, 0 }; w1.endC = { 4, 0 };
		w1.contours.push_back ({ {0,0}, {4,0}, {4,0.2}, {0.2,0.2}, {0,0} });
		w2.begC = { 0, 0 }; w2.endC = { 0, 3 };
		w2.contours.push_back ({ {0,0}, {0.2,0.2}, {0.2,3}, {0,3}, {0,0} });
		std::vector<WallFace> faces; std::vector<Issue> iss;
		bool ok = ComputeFaces ({ w1, w2 }, false, faces, iss);
		assert (ok);
		for (auto& f : faces) std::printf ("yüz p0=(%.2f,%.2f) p1=(%.2f,%.2f) n=(%.0f,%.0f) L=%.2f\n", f.p0.x, f.p0.y, f.p1.x, f.p1.y, f.n.x, f.n.y, f.length);
		std::vector<WallRun> runs (2);
		for (int i = 0; i < 2; ++i) runs[i].length = faces[i].length;
		AssignCorners (faces, runs);
		for (int i = 0; i < 2; ++i) runs[i].baseCodes = AutoFillBase (s, runs[i], i, 2);
		auto r = Layout (s, runs);
		CheckCollisions (faces, r.modules, r.issues);
		Print ("L mutfak 3.80 x 2.80", runs, r);
		assert (!r.HasError ());
		// oda içine bakıyor mu: normal +y ve +x
		assert (faces[0].n.y > 0.9 && faces[1].n.x > 0.9);
		// yerleşim kontrolü: ilk modülün kökeni
		Vec2 pos; double ang; ModuleOrigin (faces[r.modules[0].wall], r.modules[0], pos, ang);
		std::printf ("ilk modül köken=(%.3f,%.3f) açı=%.1f°\n", pos.x, pos.y, ang * 180 / M_PI);

		// köşe yönetimi kapalıyken çakışma yakalanmalı
		std::vector<WallRun> bad = runs; bad[0].startRole = bad[0].endRole = bad[1].startRole = bad[1].endRole = CornerRole::None;
		bad[0].baseCodes = "60K 60K 60K 60K 60K 60K"; bad[1].baseCodes = "60K 60K";
		auto rb = Layout (s, bad); CheckCollisions (faces, rb.modules, rb.issues);
		Print ("Çakışma testi", bad, rb);
		assert (rb.HasError ());
	}
	// 3) U mutfak
	{
		WallInput a, b, c;
		a.begC = {0,0}; a.endC = {0,3}; a.contours.push_back ({ {0,0},{0.2,0.2},{0.2,2.8},{0,3},{0,0} });
		b.begC = {0,3}; b.endC = {3,3}; b.contours.push_back ({ {0,3},{0.2,2.8},{2.8,2.8},{3,3},{0,3} });
		c.begC = {3,3}; c.endC = {3,0}; c.contours.push_back ({ {3,3},{2.8,2.8},{2.8,0.2},{3,0},{3,3} });
		std::vector<WallFace> faces; std::vector<Issue> iss;
		assert (ComputeFaces ({ b, a, c }, false, faces, iss));
		std::vector<WallRun> runs (3);
		for (int i = 0; i < 3; ++i) runs[i].length = faces[i].length;
		AssignCorners (faces, runs);
		for (int i = 0; i < 3; ++i) runs[i].baseCodes = AutoFillBase (s, runs[i], i, 3);
		auto r = Layout (s, runs);
		CheckCollisions (faces, r.modules, r.issues);
		Print ("U mutfak", runs, r);
		assert (!r.HasError ());
	}
	// 4) Kurallar
	{
		Settings bad = s; bad.baseHeight = 0.88; bad.upperBottom = 1.40;
		std::vector<WallRun> runs (1); runs[0].length = 2.0; runs[0].baseCodes = "15K 60c 80e 60bm 60K";
		auto r = Layout (bad, runs);
		Print ("Kural ihlalleri", runs, r);
		assert (r.HasError ());
		runs[0].baseCodes = "60K 7X 62,5K"; auto r2 = Layout (s, runs); Print ("Hatalı kod", runs, r2); assert (r2.HasError ());
	}
	// 5) Kesim listesi
	{
		CabinetParams p; p.A = 0.80; p.duzen = "Kapaklı"; p.rafSayisi = 1;
		auto parts = CutParts (p, "D1-01 80K");
		CabinetParams q; q.A = 1.05; q.duzen = "Kör Köşe"; q.korGen = 0.65;
		auto p2 = CutParts (q, "D1-02 105KK"); parts.insert (parts.end (), p2.begin (), p2.end ());
		CabinetParams f; f.tip = "Boy Dolap"; f.duzen = "Buzdolabı"; f.ZZYZX = 2.22; f.A = 0.60;
		auto p3 = CutParts (f, "D1-03 60BZ"); parts.insert (parts.end (), p3.begin (), p3.end ());
		std::printf ("\n%s\n", CutListCsv (parts).c_str ());
	}
	std::printf ("\nTÜM TESTLER GEÇTİ\n");
}
