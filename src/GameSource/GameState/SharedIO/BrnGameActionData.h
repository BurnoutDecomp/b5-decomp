#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (u64)

// Owning header for the BrnGameState::GameStateModuleIO value-record types reconstructed by the
// GameMode/ModeManager leaf batch: PlayerInfo (X360 0x82355038) and GameStats (0x82354F38).
// Minimal slices -- only the members the reconstructed Construct() bodies touch are declared;
// the Get*/Set* accessors that need ETakedownType/StuntElementType/ECounty/ScoreType are left
// for those types' own TUs (so this header pulls no un-homed enum). Single owner; grow in place.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// DWARF BrnGameActionData.h:219 -- one player's rank/car/violation state. Console-faithful
// layout: macName[32] @0x00, mCarId (u64) @0x20, then four int32s @0x28..0x34.
struct PlayerInfo
{
    void Construct(const char* lpcName,
                   CgsID       lCarId,
                   s32         liViolationPoints,
                   s32         liViolationPointsToNextRank,
                   s32         liCarUnlockedCount,
                   s32         liTotalViolationPoints,
                   s32         liRank);

private:
    static const s32 KI_NAME_LENGTH = 32;   // strncpy count (X360: li r5, 0x20)

    char  macName[KI_NAME_LENGTH];      // 0x00
    CgsID mCarId;                       // 0x20
    s32   miViolationPoints;            // 0x28
    s32   miViolationPointsToNextRank;  // 0x2C
    s32   miCarUnlockedCount;           // 0x30
    s32   miRank;                       // 0x34
};

// DWARF BrnGameActionData.h:53 -- flat stats block, no base. Construct() zeroes every member.
struct GameStats
{
    enum IntValueType   { E_INT_VALUE_TYPE_COUNT   = 32 };
    enum FloatValueType { E_FLOAT_VALUE_TYPE_COUNT = 3  };
    enum IdValueType    { E_ID_VALUE_TYPE_COUNT    = 3  };

    void Construct();   // X360 0x82354F38

private:
    // DWARF BrnGameActionData.h:196-206 (CgsID == u64).
    CgsID maIdValues[3];
    s32   maIntValues[32];
    f32   maFloatValues[3];
    s32   maTakedownTypeCounts[13];
    s32   maRoadsRuledCounts[2];
    s32   maaiMaxStuntElementsPerCounty[3][5];
    s32   maaiCurrentStuntElementsPerCounty[3][5];
    s32   miTotalRoads;
};
}
}
