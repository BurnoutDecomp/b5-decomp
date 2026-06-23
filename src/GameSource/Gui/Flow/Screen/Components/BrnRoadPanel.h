#pragma once

// ===================================================================================
// BrnGui::RoadPanel  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h
//
// The Road Rules HUD panel. GetSelectedFriendName (@0x82417FF8) returns the name string
// of the currently-selected friend row (online road-rule scoreboard).
//
// No prior reconstruction carries this panel's full member layout, and there is no DecFIGS
// DWARF for this TU. The recovered shape comes from the X360 access in
// GetSelectedFriendName:
//   - a bank of friend rows beginning at object +0x1A2 with a 162-byte (0xA2) stride; the
//     row's leading bytes are the friend-name string (the method returns &row[index]).
//   - the selected-row index at object +0xD9C (lwz, signed).
//   - the scoring mode at object +0xDA0 (lwz; asserted == 1, the ONLINE road-panel mode).
// The exact row count is not recoverable from this single accessor; the bank is modeled
// with the maximal count that keeps the index/mode fields at their binary offsets
// (18 rows: +0x1A2 .. +0xD05, leaving +0xD06..+0xD9B reserved). The unrecovered head and
// the gap are reserved byte-spans so each named field lands at its binary offset without
// raw-offset casting; all access is by name. (assert site: BrnRoadPanel.h:231.)
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class RoadPanel
    {
    public:
        // Online road-panel scoring mode (asserted value, X360 cmpwi 1).
        // Mirrors GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE.
        static const s32 KI_ROAD_PANEL_MODE_ONLINE = 1;

        // Per-row stride of a friend entry (X360 mulli 0xA2). The friend-name string is at
        // the start of the row.
        static const s32 KI_FRIEND_ROW_STRIDE = 0xA2;   // 162 bytes

        // Maximal row count that keeps the index/mode fields at their X360 offsets.
        static const s32 KI_MAX_FRIEND_ROWS = 18;

        // @ 0x82417FF8 - return the name string of the currently-selected friend row. Asserts
        // the panel is in the online scoring mode (the selection bank is only valid online).
        const char* GetSelectedFriendName() const;

    private:
        // One friend scoreboard row. 162-byte stride; only the leading name string is
        // recovered by this TU, the rest of the row is reserved storage.
        struct FriendRow
        {
            char macName[KI_FRIEND_ROW_STRIDE];   // row +0x000 (162-byte stride; name at start)
        };

        // Reserved storage for the unrecovered panel head that precedes the row bank
        // (object +0x1A2). Layout-recovery padding only.
        u8        maHeadReserved[0x1A2];

        FriendRow maFriendRows[KI_MAX_FRIEND_ROWS];   // object +0x1A2 .. +0xD05

        // Reserved span between the row bank and the selected-index field (object +0xD9C).
        u8        maGapReserved[0xD9C - (0x1A2 + KI_MAX_FRIEND_ROWS * KI_FRIEND_ROW_STRIDE)];

        s32       miSelectedFriendIndex;   // +0xD9C  (lwz)
        s32       miScoringMode;           // +0xDA0  (lwz; asserted == ONLINE)
    };
}
