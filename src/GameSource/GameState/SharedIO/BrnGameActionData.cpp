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

// X360 0x82354F38. Reset the whole stats block to zero. The X360 build merges the per-array
// zero loops and splits boundary words across doubleword/standalone stores; the logical source
// is the per-array reset the DWARF de-inlined body attests. Every member is zeroed. The two
// asserts are the range checks the inlined enum operator++ emits on the stunt/county counters;
// reproduced with byte-exact expression/file/line (so CGS_ASSERT's __FILE__/__LINE__ would not
// clobber them).
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
            if (liCounty > KI_COUNTY_VALID_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "leEnumIndex <= E_COUNTY_VALID_COUNT",
                    "..\\..\\..\\SharedClasses\\World/BrnWorldRegion.h",
                    44);
                CgsDev::Assert::EndAssert();
            }
        }
        while (liCounty < KI_COUNTY_VALID_COUNT);

        ++liStuntElementType;
        if (liStuntElementType > KI_STUNT_ELEMENT_TYPE_COUNT)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "leEnumIndex <= E_STUNT_ELEMENT_TYPE_COUNT",
                "..\\..\\..\\GameSource\\GameState/BrnGameStateTypes.h",
                98);
            CgsDev::Assert::EndAssert();
        }
    }
    while (liStuntElementType < KI_STUNT_ELEMENT_TYPE_COUNT);

    miTotalRoads = 0;
}
}
}
