// =============================================================================
//  KitchenAPI.hpp – Archicad ile konuşan katman
// =============================================================================
#ifndef KITCHEN_API_HPP
#define KITCHEN_API_HPP

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "KitchenModel.hpp"

namespace KitchenAPI {

struct SelectedWalls {
	std::vector<Kitchen::WallInput> walls;
	short floorInd = 0;
};

GS::UniString	U (const char* utf8);				// UTF-8 sabit metin → UniString
GS::UniString	ToUni (const std::string& utf8);
std::string		ToUtf8 (const GS::UniString& str);

// Eklentiye gömülü "MutfakDolabi.gsm" nesnesinin kütüphane indeksini bulur.
GSErrCode		GetCabinetLibInd (Int32& libInd);

// Seçimdeki düz duvarları okur. Hata/uyarı metni message'a yazılır.
bool			GetSelectedWalls (SelectedWalls& out, GS::UniString& message);

// Yerleşimi Archicad nesneleri olarak oluşturur (tek geri alma adımı).
GSErrCode		CreateKitchen (const std::vector<Kitchen::WallFace>& faces,
							   const std::vector<Kitchen::PlacedModule>& modules,
							   short floorInd, int& created);

// Seçili (yoksa projedeki tüm) mutfak dolaplarından CSV kesim listesi.
GSErrCode		ExportCutList ();

// Mutfak dolabını Nesne aracının varsayılanı yapar (elle yerleştirmek için).
GSErrCode		LoadCabinetAsDefault ();

// Hata/uyarı listesini gösterir. askToContinue: uyarılarda "devam" sorusu.
bool			ShowIssues (const std::vector<Kitchen::Issue>& issues, bool askToContinue);

}	// namespace KitchenAPI

#endif
