// =============================================================================
//  KitchenDialog.cpp – Mutfak Sihirbazı diyaloğu
// =============================================================================
#include "KitchenDialog.hpp"

#include "DG.h"

using namespace Kitchen;
using KitchenAPI::U;
using KitchenAPI::ToUni;
using KitchenAPI::ToUtf8;

KitchenDialog::KitchenDialog (const KitchenAPI::SelectedWalls& w) :
	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_KITCHEN_DLG, ACAPI_GetOwnResModule ()),
	walls (w),
	okButton		(GetReference (), OkId),
	cancelButton	(GetReference (), CancelId),
	autoFillButton	(GetReference (), AutoFillId),
	checkButton		(GetReference (), CheckId),
	infoText		(GetReference (), InfoId),
	statusText		(GetReference (), StatusId),
	baseHeight		(GetReference (), BaseHeightId),
	baseDepth		(GetReference (), BaseDepthId),
	counterThick	(GetReference (), CounterId),
	plinthHeight	(GetReference (), PlinthId),
	upperBottom		(GetReference (), UpperBottomId),
	upperHeight		(GetReference (), UpperHeightId),
	upperDepth		(GetReference (), UpperDepthId),
	addUppers		(GetReference (), AddUppersId),
	flipSide		(GetReference (), FlipId),
	title1 (GetReference (), Wall1TitleId), title2 (GetReference (), Wall2TitleId), title3 (GetReference (), Wall3TitleId),
	info1  (GetReference (), Wall1InfoId),  info2  (GetReference (), Wall2InfoId),  info3  (GetReference (), Wall3InfoId),
	base1  (GetReference (), Wall1BaseId),  base2  (GetReference (), Wall2BaseId),  base3  (GetReference (), Wall3BaseId),
	upper1 (GetReference (), Wall1UpperId), upper2 (GetReference (), Wall2UpperId), upper3 (GetReference (), Wall3UpperId)
{
	titles[0] = &title1; titles[1] = &title2; titles[2] = &title3;
	infos[0]  = &info1;  infos[1]  = &info2;  infos[2]  = &info3;
	bases[0]  = &base1;  bases[1]  = &base2;  bases[2]  = &base3;
	uppers[0] = &upper1; uppers[1] = &upper2; uppers[2] = &upper3;

	SetTitle (U ("Mutfak Sihirbazı"));

	const Settings def;
	baseHeight.SetValue (def.baseHeight);
	baseDepth.SetValue (def.baseDepth);
	counterThick.SetValue (def.counterThick);
	plinthHeight.SetValue (def.plinthHeight);
	upperBottom.SetValue (def.upperBottom);
	upperHeight.SetValue (def.upperHeight);
	upperDepth.SetValue (def.upperDepth);
	addUppers.SetState (def.addUppers);
	flipSide.SetState (def.flipSide);

	const size_t n = walls.walls.size ();
	const char* kind = n == 1 ? "düz mutfak" : n == 2 ? "L mutfak" : "U mutfak";
	infoText.SetText (U ((std::to_string (n) + " duvar seçildi → " + kind + ". Ölçüler duvarın iç yüzünden alınır.").c_str ()));

	AttachToAllItems (*this);
	Attach (*this);

	RecomputeFaces ();
	AutoFill ();
}

KitchenDialog::~KitchenDialog ()
{
	Detach (*this);
	DetachFromAllItems (*this);
}

Settings KitchenDialog::ReadSettings () const
{
	Settings s;
	s.baseHeight   = baseHeight.GetValue ();
	s.baseDepth    = baseDepth.GetValue ();
	s.counterThick = counterThick.GetValue ();
	s.plinthHeight = plinthHeight.GetValue ();
	s.upperBottom  = upperBottom.GetValue ();
	s.upperHeight  = upperHeight.GetValue ();
	s.upperDepth   = upperDepth.GetValue ();
	s.addUppers    = addUppers.IsChecked ();
	s.flipSide     = flipSide.IsChecked ();
	return s;
}

static std::string RoleText (CornerRole r)
{
	switch (r) {
		case CornerRole::Owner:	return "kör köşe";
		case CornerRole::Other:	return "dolgu";
		default:				return "serbest";
	}
}

void KitchenDialog::RecomputeFaces ()
{
	faceIssues.clear ();
	ComputeFaces (walls.walls, flipSide.IsChecked (), faces, faceIssues);

	runs.assign (faces.size (), WallRun ());
	for (size_t i = 0; i < faces.size (); ++i)
		runs[i].length = faces[i].length;
	AssignCorners (faces, runs);

	for (int i = 0; i < 3; ++i) {
		const bool used = i < (int) runs.size ();
		titles[i]->SetText (used ? U (("Duvar " + std::to_string (i + 1) + " – " + Cm (runs[i].length) + " cm").c_str ())
								 : U (("Duvar " + std::to_string (i + 1) + " (seçilmedi)").c_str ()));
		infos[i]->SetText (used ? U (("baş: " + RoleText (runs[i].startRole) + "   son: " + RoleText (runs[i].endRole)).c_str ())
								: GS::UniString ());
		bases[i]->SetStatus (used);
		uppers[i]->SetStatus (used);
		if (!used) {
			bases[i]->SetText (GS::UniString ());
			uppers[i]->SetText (GS::UniString ());
		}
	}
}

void KitchenDialog::AutoFill ()
{
	const Settings s = ReadSettings ();
	for (size_t i = 0; i < runs.size () && i < 3; ++i) {
		bases[i]->SetText (ToUni (AutoFillBase (s, runs[i], (int) i, (int) runs.size ())));
		uppers[i]->SetText (GS::UniString ());
	}
	statusText.SetText (U ("Otomatik dizildi. Kodları düzenleyebilirsiniz."));
}

bool KitchenDialog::Evaluate (bool forCreate)
{
	const Settings s = ReadSettings ();
	std::vector<WallRun> r = runs;
	for (size_t i = 0; i < r.size () && i < 3; ++i) {
		r[i].baseCodes  = ToUtf8 (bases[i]->GetText ());
		r[i].upperCodes = ToUtf8 (uppers[i]->GetText ());
	}

	result = Layout (s, r);
	if (faceIssues.empty ())
		CheckCollisions (faces, result.modules, result.issues);
	result.issues.insert (result.issues.begin (), faceIssues.begin (), faceIssues.end ());

	int nErr = 0, nWarn = 0;
	for (const Issue& i : result.issues)
		(i.error ? nErr : nWarn)++;
	statusText.SetText (GS::UniString::Printf (U ("%d modül · %d hata · %d uyarı"), (int) result.modules.size (), nErr, nWarn));

	if (forCreate)
		return KitchenAPI::ShowIssues (result.issues, true);

	if (result.issues.empty ())
		DGAlert (DG_INFORMATION, U ("Mutfak Sihirbazı"), U ("Tüm kurallara uygun."),
				 GS::UniString::Printf (U ("%d modül oluşturulmaya hazır."), (int) result.modules.size ()), U ("Tamam"));
	else
		KitchenAPI::ShowIssues (result.issues, false);
	return nErr == 0;
}

void KitchenDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &okButton) {
		if (Evaluate (true))
			PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	} else if (ev.GetSource () == &autoFillButton) {
		AutoFill ();
	} else if (ev.GetSource () == &checkButton) {
		Evaluate (false);
	}
}

void KitchenDialog::CheckItemChanged (const DG::CheckItemChangeEvent& ev)
{
	if (ev.GetSource () == &flipSide) {
		RecomputeFaces ();
		AutoFill ();
	}
}
