#define _CRT_SECURE_NO_WARNINGS   // strncpy (POSIX/PPC source idiom)

#include "GameSource/GameState/SharedIO/BrnGameActionData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // strncpy

namespace
{
// GameStats array bounds (the X360 loop counts; == the declared array extents).
const s32 KI_TAKEDOWN_TYPE_COUNT     = 13;
const s32 KI_SCORE_TYPE_COUNT        = 2;
const s32 KI_COUNTY_VALID_COUNT      = 5;
const s32 KI_STUNT_ELEMENT_TYPE_COUNT = 3;
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x82355038. Copy the player name (truncated/terminated at 32) and store the car id,
// violation points, and rank. liTotalViolationPoints (param 6) is genuinely unused by the body.
// The Hex-Rays (a3,a4) pair is the single 64-bit CgsID arg; restored to one store.
void PlayerInfo::Construct(const char* lpcName,
                           CgsID       lCarId,
                           s32         liViolationPoints,
                           s32         liViolationPointsToNextRank,
                           s32         liCarUnlockedCount,
                           s32         liTotalViolationPoints,
                           s32         liRank)
{
    (void)liTotalViolationPoints;

    strncpy(macName, lpcName, KI_NAME_LENGTH);

    mCarId                      = lCarId;
    miViolationPoints           = liViolationPoints;
    miViolationPointsToNextRank = liViolationPointsToNextRank;
    miCarUnlockedCount          = liCarUnlockedCount;
    miRank                      = liRank;

    macName[KI_NAME_LENGTH - 1] = 0;   // X360: stb r11(=0), 0x1F(r31)
}

// X360 0x82354F38. Reset the stats block to zero (miTotalRoads excepted -- see NOTE below).
// The X360 build merges the per-array zero loops and splits boundary words across doubleword/
// standalone stores; the logical source is the per-array reset the DWARF de-inlined body
// attests. Array index math against the raw stores fixes maFloatValues at 4 elements (not 3):
// the asm's 33-word merged loop (result+6..result+38) covers all 32 maIntValues words plus the
// first maFloatValues word, and the three explicit stores at result[39..41] are the remaining
// three maFloatValues words -- that is the only split that leaves maTakedownTypeCounts[13]
// (result[42..54], one clean 13-word loop) and maRoadsRuledCounts[2] (result[55],[56]) exactly
// matching their X360-attested counts (BrnTakedownType.h E_TAKEDOWN_COUNT=13, BrnChallengeData.h
// E_SCORE_TYPE_COUNT=2) with no leftover/missing word. The two asserts are the range checks the
// inlined enum operator++ emits on the stunt/county counters; called directly (not via
// CGS_ASSERT) with the byte-exact expression/file/line the asm embeds, since CGS_ASSERT's
// __FILE__/__LINE__ would clobber them with this TU's own location.
void GameStats::Construct()
{
    for (s32 liIntValueTypeIndex = 0; liIntValueTypeIndex < E_INT_VALUE_TYPE_COUNT; ++liIntValueTypeIndex)
    {
        maIntValues[liIntValueTypeIndex] = 0;
    }

    for (s32 liIdValueTypeIndex = 0; liIdValueTypeIndex < E_ID_VALUE_TYPE_COUNT; ++liIdValueTypeIndex)
    {
        maIdValues[liIdValueTypeIndex] = 0;
    }

    for (s32 liFloatValueTypeIndex = 0; liFloatValueTypeIndex < E_FLOAT_VALUE_TYPE_COUNT; ++liFloatValueTypeIndex)
    {
        maFloatValues[liFloatValueTypeIndex] = 0.0f;
    }

    for (s32 liTakedownTypeIndex = 0; liTakedownTypeIndex < KI_TAKEDOWN_TYPE_COUNT; ++liTakedownTypeIndex)
    {
        maTakedownTypeCounts[liTakedownTypeIndex] = 0;
    }

    for (s32 liScoreTypeIndex = 0; liScoreTypeIndex < KI_SCORE_TYPE_COUNT; ++liScoreTypeIndex)
    {
        maRoadsRuledCounts[liScoreTypeIndex] = 0;
    }

    s32 liStuntElementType = 0;
    do
    {
        s32 liCounty = 0;
        do
        {
            maaiMaxStuntElementsPerCounty[liStuntElementType][liCounty]     = 0;
            maaiCurrentStuntElementsPerCounty[liStuntElementType][liCounty] = 0;

            ++liCounty;
            if (!(liCounty <= KI_COUNTY_VALID_COUNT))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leEnumIndex <= E_COUNTY_VALID_COUNT",
                                            "..\\..\\..\\SharedClasses\\World/BrnWorldRegion.h", 44);
                CgsDev::Assert::EndAssert();
            }
        }
        while (liCounty < KI_COUNTY_VALID_COUNT);

        ++liStuntElementType;
        if (!(liStuntElementType <= KI_STUNT_ELEMENT_TYPE_COUNT))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("leEnumIndex <= E_STUNT_ELEMENT_TYPE_COUNT",
                                        "..\\..\\..\\GameSource\\GameState/BrnGameStateTypes.h", 98);
            CgsDev::Assert::EndAssert();
        }
    }
    while (liStuntElementType < KI_STUNT_ELEMENT_TYPE_COUNT);

    // NOTE: miTotalRoads is intentionally left untouched here -- the X360 asm at 0x82354F38
    // stores exactly 87 dwords (indices 0-86: 3 CgsID, 32 int, 4 float, 13 takedown, 2 roads-
    // ruled, then the interleaved 3x5 max/current stunt-element grids) and has no store beyond
    // the stunt grids, so miTotalRoads is never zeroed by Construct() in the binary.
}
}
}
