// ============================================================================
// b5-decomp/src/GameSource/Network/Parameters/BrnNetworkRoadRulesData.cpp
// ============================================================================
// BrnNetwork::RoadRulesDownloadData / RoadRulesLocalPlayerDownloadedScores accessors.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (gated against the DWARF shape). Each
// accessor is a bounds/precondition-checked read or append over the fixed score tables;
// every X360 store is reproduced store-for-store at its exact offset (16-byte name slot,
// 4-byte score words, the +0x3C0 / +0x148 count words).
//
//   RoadRulesDownloadData::GetRoadRulesDataForDownloadIndex  @ 0x82541708
//   RoadRulesDownloadData::SetDownloadedRoadRulesData        @ 0x82580810
//   RoadRulesLocalPlayerDownloadedScores::GetRoadRulesDataForDownloadIndex @ 0x825418B0
//   RoadRulesLocalPlayerDownloadedScores::SetDownloadedRoadRulesData       @ 0x82580978
//
// The X360 name copy is a manual strlen loop + a "String too long" precondition assert
// (the inlined CgsStringUtils CopyString guard, CgsStringUtils.h:55) followed by a plain
// strncpy(dest, src, 16); reproduced faithfully here.

#include "GameSource/Network/Parameters/BrnNetworkRoadRulesData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>  // std::strlen / std::strncpy -- the X360 name copy

namespace BrnNetwork
{

// X360 0x82541708.
void RoadRulesDownloadData::GetRoadRulesDataForDownloadIndex(s32 liDownloadIndex, char* lpName,
                                                             s32* lpScoreType, s32* lpQuantisedScore)
{
    // Three index preconditions (the X360 fires each independently before any read).
    CGS_ASSERT(liDownloadIndex >= 0, "liDownloadIndex >= 0");
    CGS_ASSERT(liDownloadIndex < KI_MAX_DOWNLOAD_DATA, "liDownloadIndex < KI_MAX_DOWNLOAD_DATA");
    CGS_ASSERT(liDownloadIndex < mData.miNumScores, "liDownloadIndex < mData.miNumScores");

    // Score words first (the X360 stores *a4/*a5 before the name copy):
    //   *lpScoreType      = maiScoreTypes[index]     (+0x280 array)
    //   *lpQuantisedScore = maiQuantisedScore[index] (+0x320 array)
    *lpScoreType      = mData.maiScoreTypes[liDownloadIndex];
    *lpQuantisedScore = mData.maiQuantisedScore[liDownloadIndex];

    // Name copy: assert the stored name fits the 16-byte slot, then strncpy 16 bytes.
    char* lpStored = mData.maPlayerNames[liDownloadIndex];
    CGS_ASSERT(std::strlen(lpStored) < static_cast<u32>(KI_ROAD_RULES_NAME_LENGTH), "String too long");
    std::strncpy(lpName, lpStored, KI_ROAD_RULES_NAME_LENGTH);
}

// X360 0x82580810.
void RoadRulesDownloadData::SetDownloadedRoadRulesData(char* lpName, s32 liScoreType, s32 liQuantisedScore)
{
    // Room precondition.
    CGS_ASSERT(mData.miNumScores < KI_MAX_DOWNLOAD_DATA, "mData.miNumScores < KI_MAX_DOWNLOAD_DATA");

    // The X360 computes the name-slot pointer (16 * miNumScores + base) BEFORE the
    // strlen/assert; the strncpy of the incoming name happens before the score stores.
    char* lpSlot = mData.maPlayerNames[mData.miNumScores];
    CGS_ASSERT(std::strlen(lpName) < static_cast<u32>(KI_ROAD_RULES_NAME_LENGTH), "String too long");
    std::strncpy(lpSlot, lpName, KI_ROAD_RULES_NAME_LENGTH);

    // Score stores into the two parallel arrays at the current count, then bump it.
    mData.maiScoreTypes[mData.miNumScores]     = liScoreType;
    mData.maiQuantisedScore[mData.miNumScores] = liQuantisedScore;
    ++mData.miNumScores;
}

// X360 0x825418B0.
void RoadRulesLocalPlayerDownloadedScores::GetRoadRulesDataForDownloadIndex(s32 liDownloadIndex,
                                                                            s32* lpScoreType, s32* lpScore)
{
    CGS_ASSERT(liDownloadIndex >= 0, "liDownloadIndex >= 0");
    CGS_ASSERT(liDownloadIndex < KI_MAX_DOWNLOAD_DATA, "liDownloadIndex < KI_MAX_DOWNLOAD_DATA");
    CGS_ASSERT(liDownloadIndex < miNumScores, "liDownloadIndex < miNumScores");

    // *lpScoreType = maiScoreTypes[index] (the +0xA0 array == v5[index+40]);
    // *lpScore     = maiScores[index]     (the +0x00 array == v5[index]).
    *lpScoreType = maiScoreTypes[liDownloadIndex];
    *lpScore     = maiScores[liDownloadIndex];
}

// X360 0x82580978.
void RoadRulesLocalPlayerDownloadedScores::SetDownloadedRoadRulesData(s32 liScoreType, s32 liScore)
{
    CGS_ASSERT(miNumScores < KI_MAX_DOWNLOAD_DATA, "miNumScores < KI_MAX_DOWNLOAD_DATA");

    // v3[v3[82]+40] = a2 -> maiScoreTypes[miNumScores] = liScoreType;
    // v3[v3[82]++]  = a3 -> maiScores[miNumScores]     = liScore; then bump the count.
    maiScoreTypes[miNumScores] = liScoreType;
    maiScores[miNumScores]     = liScore;
    ++miNumScores;
}

}
