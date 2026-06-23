#pragma once

// ===================================================================================
// BrnGui::CarSelectOnlinePlayerList  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h
//
// The online car-select lobby player list: a fixed bank of up to 8 player rows. IsShowing
// (@0x82482950) reports whether a given row index is currently shown; it range-checks the
// index (0..7) and reads the row's "showing" flag.
//
// No prior source carries this component's full member layout (no Feb-2007 leak entry and
// no DecFIGS DWARF for this TU). The recovered shape comes from the X360 access in
// IsShowing: index*0x2F0 (752-byte stride per row) + 0x378 (the per-row showing byte),
// i.e. an 8-entry row bank that begins at object +0x378. The unrecovered head ahead of
// the bank and the unrecovered remainder of each row are kept as reserved byte-spans so
// the showing flag lands at its binary offset without raw-offset casting; all access is
// by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    class CarSelectOnlinePlayerList
    {
    public:
        // Number of player rows in the lobby bank (X360 range check: index in [0,8)).
        static const s32 KI_MAX_PLAYERS = 8;

        // @ 0x82482950 - is the row at liPlayerIndex currently shown? Range-checks the
        // index (asserts on out-of-range) and returns the row's showing flag.
        bool IsShowing(s32 liPlayerIndex) const;

    private:
        // One lobby player row. 752-byte stride (X360 mulli r28, 0x2F0). Only the leading
        // showing flag is recovered by this TU; the rest of the row is reserved storage.
        struct PlayerRow
        {
            bool mbShowing;             // row +0x000 -- read by IsShowing (lbz)
            u8   maRowReserved[0x2F0 - 1];  // row +0x001 .. +0x2EF -> 752-byte stride
        };

        // Reserved storage for the unrecovered component head that precedes the row bank
        // (object +0x378). Layout-recovery padding only.
        u8        maHeadReserved[0x378];

        PlayerRow maPlayerRows[KI_MAX_PLAYERS];   // object +0x378 .. (8 * 0x2F0)
    };
}
