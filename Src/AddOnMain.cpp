// =============================================================================
//  Mutfak Sihirbazı – Archicad eklentisi
//  Menü: Mutfak > Mutfak Sihirbazı / Kesim Listesi / Nesne aracına yükle
// =============================================================================
#include "APIEnvir.h"
#include "ACAPinc.h"

#include "ResourceIds.hpp"
#include "RS.hpp"
#include "DG.h"
#include "DGModule.hpp"
#include "UniString.hpp"

#include "KitchenAPI.hpp"
#include "KitchenDialog.hpp"

#include <cwchar>

static const GSResID	AddOnInfoID			= ID_ADDON_INFO;
static const Int32		AddOnNameID			= 1;
static const Int32		AddOnDescriptionID	= 2;

static const short		AddOnMenuID			= ID_ADDON_MENU;
static const Int32		CmdWizard			= 1;
static const Int32		CmdCutList			= 2;
static const Int32		CmdLoadObject		= 3;

using KitchenAPI::U;

static void RunWizard ()
{
	KitchenAPI::SelectedWalls walls;
	GS::UniString message;
	if (!KitchenAPI::GetSelectedWalls (walls, message)) {
		DGAlert (DG_INFORMATION, U ("Mutfak Sihirbazı"), message, GS::UniString (), U ("Tamam"));
		return;
	}

	KitchenDialog dialog (walls);
	if (!dialog.Invoke ())
		return;

	int created = 0;
	const GSErrCode err = KitchenAPI::CreateKitchen (dialog.GetFaces (), dialog.GetModules (), walls.floorInd, created);
	if (err != NoError) {
		DGAlert (DG_ERROR, U ("Mutfak Sihirbazı"), U ("Dolaplar oluşturulurken hata oluştu."),
				 GS::UniString::Printf (U ("Hata kodu: %d · oluşturulan: %d"), (int) err, created), U ("Tamam"));
		return;
	}
	DGAlert (DG_INFORMATION, U ("Mutfak Sihirbazı"),
			 GS::UniString::Printf (U ("%d dolap oluşturuldu."), created),
			 U ("Her dolabı Nesne Ayarları'ndan tek tek değiştirebilirsiniz. Kesim listesi: Mutfak > Kesim Listesi."), U ("Tamam"));
}

static GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	if (menuParams->menuItemRef.menuResID != AddOnMenuID)
		return NoError;

	switch (menuParams->menuItemRef.itemIndex) {
		case CmdWizard:		RunWizard ();							break;
		case CmdCutList:	KitchenAPI::ExportCutList ();			break;
		case CmdLoadObject:	KitchenAPI::LoadCabinetAsDefault ();	break;
	}
	return NoError;
}

// -----------------------------------------------------------------------------
//  Gömülü kütüphane (MutfakDolabi.gsm) kontrolü – şablondan
// -----------------------------------------------------------------------------
#if defined(macintosh)
static bool CCALL ResourceTypesCallback (GSResType resType, GSResModule /*resModule*/, void* userData)
{
	constexpr GSResType libPartType = 'FLDR';
	if (resType == libPartType) {
		*static_cast<bool*> (userData) = true;
		return false;
	}
	return true;
}
#else
static bool CCALL ResourceTypesCallback (const WCHAR* resType, GSResModule /*resModule*/, void* userData)
{
	if (std::wcscmp (resType, L"FLDR") == 0) {
		*static_cast<bool*> (userData) = true;
		return false;
	}
	return true;
}
#endif

static bool HasBuiltInLibPart ()
{
	bool hasLibPart = false;
	RSEnumResourceTypes (ResourceTypesCallback, &hasLibPart, ACAPI_GetOwnResModule ());
	return hasLibPart;
}

// -----------------------------------------------------------------------------
//  Eklenti giriş noktaları
// -----------------------------------------------------------------------------
API_AddonType CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, AddOnInfoID, AddOnNameID, ACAPI_GetOwnResModule ());
	RSGetIndString (&envir->addOnInfo.description, AddOnInfoID, AddOnDescriptionID, ACAPI_GetOwnResModule ());
	return APIAddon_Normal;
}

GSErrCode RegisterInterface (void)
{
	GSErrCode err = ACAPI_MenuItem_RegisterMenu (AddOnMenuID, 0, MenuCode_UserDef, MenuFlag_Default);
	if (err == NoError && HasBuiltInLibPart ())
		err = ACAPI_AddOnIntegration_RegisterBuiltInLibrary ();
	return err;
}

GSErrCode Initialize (void)
{
	return ACAPI_MenuItem_InstallMenuHandler (AddOnMenuID, MenuCommandHandler);
}

GSErrCode FreeData (void)
{
	return NoError;
}
