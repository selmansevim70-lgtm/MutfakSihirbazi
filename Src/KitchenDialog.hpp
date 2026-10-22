// =============================================================================
//  KitchenDialog.hpp – Mutfak Sihirbazı diyaloğu
// =============================================================================
#ifndef KITCHEN_DIALOG_HPP
#define KITCHEN_DIALOG_HPP

#include "KitchenAPI.hpp"
#include "ResourceIds.hpp"

#include "DGModule.hpp"

class KitchenDialog :	public DG::ModalDialog,
						public DG::PanelObserver,
						public DG::ButtonItemObserver,
						public DG::CheckItemObserver,
						public DG::CompoundItemObserver
{
public:
	enum ItemIds {
		OkId = 1, CancelId = 2, InfoId = 4,
		BaseHeightId = 6, BaseDepthId = 8, CounterId = 10, PlinthId = 12,
		UpperBottomId = 14, UpperHeightId = 16, UpperDepthId = 18,
		AddUppersId = 19, FlipId = 20,
		Wall1TitleId = 21, Wall1BaseId = 22, Wall1InfoId = 23, Wall1UpperId = 24,
		Wall2TitleId = 25, Wall2BaseId = 26, Wall2InfoId = 27, Wall2UpperId = 28,
		Wall3TitleId = 29, Wall3BaseId = 30, Wall3InfoId = 31, Wall3UpperId = 32,
		AutoFillId = 33, CheckId = 34, StatusId = 35
	};

	explicit KitchenDialog (const KitchenAPI::SelectedWalls& walls);
	~KitchenDialog ();

	const std::vector<Kitchen::WallFace>&		GetFaces () const	{ return faces; }
	const std::vector<Kitchen::PlacedModule>&	GetModules () const	{ return result.modules; }

private:
	virtual void ButtonClicked (const DG::ButtonClickEvent& ev) override;
	virtual void CheckItemChanged (const DG::CheckItemChangeEvent& ev) override;

	Kitchen::Settings	ReadSettings () const;
	void				RecomputeFaces ();
	void				AutoFill ();
	bool				Evaluate (bool forCreate);

	const KitchenAPI::SelectedWalls&	walls;
	std::vector<Kitchen::WallFace>		faces;
	std::vector<Kitchen::WallRun>		runs;
	std::vector<Kitchen::Issue>			faceIssues;
	Kitchen::LayoutResult				result;

	DG::Button		okButton, cancelButton, autoFillButton, checkButton;
	DG::LeftText	infoText, statusText;
	DG::LengthEdit	baseHeight, baseDepth, counterThick, plinthHeight;
	DG::LengthEdit	upperBottom, upperHeight, upperDepth;
	DG::CheckBox	addUppers, flipSide;
	DG::LeftText	title1, title2, title3, info1, info2, info3;
	DG::TextEdit	base1, base2, base3, upper1, upper2, upper3;

	DG::LeftText*	titles[3];
	DG::LeftText*	infos[3];
	DG::TextEdit*	bases[3];
	DG::TextEdit*	uppers[3];
};

#endif
