#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Network/Parameters/BrnNetworkRoadRulesData.h
// ============================================================================
// BrnNetwork::RoadRulesDownloadData and BrnNetwork::RoadRulesLocalPlayerDownloadedScores
// -- the two fixed-capacity road-rules score tables the server-download pipeline fills.
//
// SHAPE from DecFIGS DWARF (GameSource/Network/Parameters/BrnNetworkRoadRulesData.h)
// gated against the X360 binary. Both tables cap their score count at
// KI_MAX_DOWNLOAD_DATA == 40 (X360 asserts "liDownloadIndex < KI_MAX_DOWNLOAD_DATA" /
// "mData.miNumScores < KI_MAX_DOWNLOAD_DATA").
//
// X360 layout note: the DWARF spells RoadRulesDownloadData's player-name field as
// char[40][20] (a 20-byte PS3 name slot), but the X360 build addresses names at a
// 16-byte stride (`slwi r11,r31,4` == 16*index in GetRoadRulesDataForDownloadIndex
// @ 0x82541708 and SetDownloadedRoadRulesData @ 0x82580810). The X360 asm is
// authoritative, so the on-X360 name slot is 16 bytes (the same 16-vs-20 PlayerName
// width divergence seen elsewhere in this codebase). The two parallel int32 arrays then
// land at exactly 16*40 == 0x280 (maiScoreTypes) and 0x320 (maiQuantisedScore), and the
// count word miNumScores at +0x3C0 -- matching the X360 displacements
// (`4*(index+160)` == +0x280, `4*(index+200)` == +0x320, `lwz 0x3C0(rN)`).
// ============================================================================

#include "types.hpp"

namespace BrnNetwork
{
    // BrnNetworkRoadRulesData.h:32 -- max score rows either download table holds.
    const s32 KI_MAX_DOWNLOAD_DATA = 40;

    // BrnNetworkRoadRulesData.h:179 -- the fixed "is this real download data" stamp length.
    const s32 KI_DOWNLOAD_DATA_PATTERN_LENGTH = 123;

    // On-X360 fixed name slot width (16 bytes; the DWARF's 20 is the PS3 width).
    const s32 KI_ROAD_RULES_NAME_LENGTH = 16;

    // ------------------------------------------------------------------------
    // RoadRulesDownloadData -- the leaderboard rows downloaded for one road: up to 40
    // {player-name, score-type, quantised-score} rows. sizeof(mData) == 0x3C4; the
    // owning struct then carries a 123-byte validity pattern. Names are stored as a
    // fixed 16*40 block; the two score arrays are 40 int32 words each.
    // ------------------------------------------------------------------------
    class RoadRulesDownloadData
    {
    public:
        // DownloadData mirrors the DWARF nested struct (BrnNetworkRoadRulesData.h:166).
        struct DownloadData
        {
            char maPlayerNames[KI_MAX_DOWNLOAD_DATA][KI_ROAD_RULES_NAME_LENGTH]; // +0x000 (16*40 == 0x280)
            s32  maiScoreTypes[KI_MAX_DOWNLOAD_DATA];                            // +0x280
            s32  maiQuantisedScore[KI_MAX_DOWNLOAD_DATA];                        // +0x320
            s32  miNumScores;                                                    // +0x3C0
        };

        // X360 0x82541708 -- read row liDownloadIndex out into the caller's score-type /
        // quantised-score words and copy its name (<=16 chars) into lpName. Asserts the
        // index is in [0, KI_MAX_DOWNLOAD_DATA) and < miNumScores, and that the stored
        // name fits the 16-byte slot.
        void GetRoadRulesDataForDownloadIndex(s32 liDownloadIndex, char* lpName,
                                              s32* lpScoreType, s32* lpQuantisedScore);

        // X360 0x82580810 -- append a downloaded row (name + score-type + quantised
        // score) at miNumScores and bump the count. Asserts there is room
        // (miNumScores < KI_MAX_DOWNLOAD_DATA) and that the name fits the 16-byte slot.
        void SetDownloadedRoadRulesData(char* lpName, s32 liScoreType, s32 liQuantisedScore);

    private:
        DownloadData mData;                                  // +0x000
        char         macPattern[KI_DOWNLOAD_DATA_PATTERN_LENGTH]; // +0x3C4
    };
    static_assert(sizeof(RoadRulesDownloadData) >= 0x3C4 + KI_DOWNLOAD_DATA_PATTERN_LENGTH,
                  "RoadRulesDownloadData must cover mData (0x3C4) + macPattern (123)");

    // ------------------------------------------------------------------------
    // RoadRulesLocalPlayerDownloadedScores -- the local player's own downloaded scores
    // for one road-rules id: two parallel int32[40] arrays (score, score-type), the
    // 64-bit road-rules id, and the live row count. miNumScores lands at +0x148
    // (DWORD index 82) -- matching the X360 `lwz 0x148(rN)`; the 8 bytes at +0x140 are
    // the mu64RoadRulesID, which is why miNumScores is two dwords past the second array.
    // ------------------------------------------------------------------------
    class RoadRulesLocalPlayerDownloadedScores
    {
    public:
        // X360 0x825418B0 -- read row liDownloadIndex out into the caller's score-type
        // and score words. Asserts the index is in [0, KI_MAX_DOWNLOAD_DATA) and
        // < miNumScores.
        void GetRoadRulesDataForDownloadIndex(s32 liDownloadIndex, s32* lpScoreType,
                                              s32* lpScore);

        // X360 0x82580978 -- append a downloaded row (score-type + score) at miNumScores
        // and bump the count. Asserts there is room (miNumScores < KI_MAX_DOWNLOAD_DATA).
        void SetDownloadedRoadRulesData(s32 liScoreType, s32 liScore);

    private:
        s32 maiScores[KI_MAX_DOWNLOAD_DATA];      // +0x000
        s32 maiScoreTypes[KI_MAX_DOWNLOAD_DATA];  // +0x0A0
        u64 mu64RoadRulesID;                       // +0x140
        s32 miNumScores;                           // +0x148
    };
    static_assert(sizeof(RoadRulesLocalPlayerDownloadedScores) == 0x150,
                  "RoadRulesLocalPlayerDownloadedScores layout (miNumScores @ +0x148)");
}
