// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager.cpp
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager (single home for GetStreetData)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SharedClasses/StreetData/BrnStreetData.h"       // BrnStreetData::StreetData / Road

// BrnGameState::StreetManager (incl. its GetStreetData() accessor) now lives in its single
// canonical home GameSource/GameState/StreetData/BrnGameStateStreetManager.h (included above).
// The former file-local `struct StreetManager` shim was removed from this TU: it ODR-collided
// with the new header once a second TU (the road-rules debug component) needed the type.

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
