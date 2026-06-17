// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager.cpp
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SharedClasses/StreetData/BrnStreetData.h"       // BrnStreetData::StreetData / Road

// PROVISIONAL minimal slice of BrnGameState::StreetManager: this TU only calls
// the GetStreetData() accessor. The real, complete class lives (uncommitted) at
// GameSource/GameState/StreetData/BrnGameStateStreetManager.h, where it holds
//   ResourcePtr<BrnStreetData::StreetData> mpStreetData;
// and exposes (DWARF, BrnGameStateStreetManager.h:355):
//   const BrnStreetData::StreetData* GetStreetData();   // == mpStreetData.operator->()
// Replace this forward-shim with that real header once StreetManager is committed.
// Declared `struct` to match the DWARF home (BrnGameStateStreetManager.h:199) and the
// forward declaration in the .h, avoiding a future C4099 struct/class tag mismatch.
namespace BrnGameState
{
    struct StreetManager
    {
        // X360: BrnStreetData::StreetData_::oper(&mpStreetData) i.e.
        //       ResourcePtr<StreetData>::operator->()  (out-of-line @ 0x82324E60).
        const BrnStreetData::StreetData* GetStreetData();
    };
}

// extern const CgsID K_INVALID_ID == 0 (X360 'return 0' on no-current-road).
const CgsID BrnGameState::RoadRulesManager::K_INVALID_ID = 0;

// 0x82327438
// DWARF: CgsID GetCurrentRoadID() const;  (the Hex-Rays 'int' return + 4-byte
// read of *(Road+16) is a decompiler truncation of the 8-byte CgsID load).
CgsID BrnGameState::RoadRulesManager::GetCurrentRoadID() const
{
    // *(this+32) == -1  ->  no current road.
    if ( miLastRoadIndex == -1 )
    {
        return K_INVALID_ID;
    }

    // *(this+20): asserted present (baked "mpStreetManager").
    CGS_ASSERT( mpStreetManager, "mpStreetManager" );

    // ResourcePtr<StreetData>::operator-> wrapped by StreetManager::GetStreetData().
    const BrnStreetData::StreetData* lpStreetData = mpStreetManager->GetStreetData();
    const BrnStreetData::Road*       lpRoad       = lpStreetData->GetRoad( miLastRoadIndex );

    // baked "lpRoad".
    CGS_ASSERT( lpRoad, "lpRoad" );

    // X360 reads *(Road+16) == Road::mId; the inlined Road::GetId().
    return lpRoad->GetId();
}
