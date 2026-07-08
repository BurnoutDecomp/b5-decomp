#pragma once

// ===================================================================================
// BrnGui::SelectRoutes  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h
//
// The route-selection screen component that backs BrnGui::OnlineSelectRoute. It holds the
// per-checkpoint menu-item grid plus the cursor/round bookkeeping the online route screen
// drives. The assert file/line rodata in the X360 build names this header
// ("..\\..\\..\\GameSource\\Gui/Flow/Screen/Components/BrnSelectRoutes.h").
//
// There is no DecFIGS DWARF and no Feb-2007 source for this TU, so the shape is recovered
// purely from the X360 ARTIST access pattern. Proven offsets (from the six ledger funcs):
//   +0x000 (0)       checkpoint menu-item data grid (dual-width: word + halfword access)
//   +0x7D5 (2005)    s8  cursor offset from the first checkpoint item (-1 == none selected)
//   +0x1908 (6408)   GuiCache* the owning cache
//   +0x190C (6412)   s32 current round number (indexes maiNumCheckpoints)
//   +0x1910 (6416)   s32 menu-item index of the first checkpoint item
//   +0x1914 (6420)   s32 maiNumCheckpoints[ liRoundNumber ] (asserted array)
//
// The leading data grid is addressed both as words (25 words per checkpoint) and as
// halfwords (50 halfwords per checkpoint) by the X360 code, with the field selector and the
// round baked into a single flat index. Its interleaved sub-word layout cannot be split into
// separate semantically-named typed arrays without inventing dimensions the binary does not
// attest, so it is modelled as one union with a word view and a halfword view; every access
// uses the flat index the asm computes (layout recovery, no raw-offset casting). Only the
// halfword landmark lane is read by the functions reconstructed in this slice.
//
// SCOPE: this slice reconstructs the three functions that ground cleanly on this layout --
// GetCurrentlySelectedCheckpointIndex, GetCheckpointLandmark and
// GetCurrentlySelectedCheckpointLandmark. RemoveLastCheckpoint, SetStartLightTriggerID and
// SetCurrentlySelectedCheckpointLandmark are left for a later slice: each gates on the
// two-player-mode discriminator at GuiCache+0xA9B8 (== 11), an unrecovered foreign member
// that cannot be named without fabrication. GetMenuItemType is a sibling ledger function
// (its own TU) and is declared-only here so the two landmark getters can call it.
// ===================================================================================

#include <cstddef>   // offsetof
#include "types.hpp"
#include "GameSource/GameState/BrnGameStateTypes.h"   // BrnGameState::LandmarkIndex (2-byte handle)

namespace BrnGui
{
    class GuiCache;   // fwd: mpGuiCache is pointer-only here

    class SelectRoutes
    {
    public:
        // Assert bound fired by GetCheckpointLandmark ("liCheckpointIndex < KI_MAX_CHECKPOINTS").
        static const s32 KI_MAX_CHECKPOINTS = 17;

        // Conservative upper bound for the trailing maiNumCheckpoints array. The exact round
        // count is unattested; this is a trailing member, so its size does not affect any of
        // the proven offsets above.
        static const s32 KI_MAX_ROUNDS = 16;

        // @ 0x82418B90 -- absolute menu-item index of the currently selected checkpoint, or 0
        // when nothing is selected (cursor offset == -1).
        s32 GetCurrentlySelectedCheckpointIndex() const;

        // @ 0x824837B8 -- the landmark stored for (round, checkpoint) in the menu grid.
        BrnGameState::LandmarkIndex GetCheckpointLandmark(s32 liRoundNumber,
                                                          s32 liCheckpointIndex) const;

        // @ 0x824899E8 -- the landmark for the checkpoint the cursor currently sits on,
        // resolving the "next/previous checkpoint" menu-item types.
        BrnGameState::LandmarkIndex GetCurrentlySelectedCheckpointLandmark(s32 liRoundNumber) const;

        // Sibling ledger function (reconstructed in its own TU): classifies a menu item.
        // Declaration-only here so the landmark getters can call it.
        s32 GetMenuItemType(s32 liItemIndex) const;

    private:
        // Checkpoint menu-item data grid, addressed by flat word / halfword indices that fold
        // the checkpoint number, the round and the field selector together (see header note).
        union MenuDataView
        {
            u32 mauWord[501];    // flat word view  (byte 0x000 .. 0x7D3)
            u16 mauHalf[1002];   // flat halfword view (holds the LandmarkIndex lane)
        };

        MenuDataView mMenuData;                  // +0x000
        u8           mPad2004;                   // +0x7D4 (alignment filler before the cursor)
        s8           mi8CursorOffset;            // +0x7D5  (-1 == nothing selected)
        u8           maPad2006[6408 - 2006];     // +0x7D6 .. +0x1907 (unrecovered members)
        // X360 offsets from here on: mpGuiCache @0x1908, miRoundNumber @0x190C,
        // miFirstCheckpointItemIndex @0x1910, maiNumCheckpoints @0x1914 (4-byte pointer build).
        // On the x64 PC target mpGuiCache widens to 8 bytes, so the trailing scalars slide by
        // +4; all of them are accessed by name (never by absolute offset), so the shift is
        // immaterial. Only mMenuData must stay at +0 (the grid is addressed by a flat index
        // relative to the object base), which the assert below pins.
        GuiCache*    mpGuiCache;                 // X360 +0x1908
        s32          miRoundNumber;              // X360 +0x190C
        s32          miFirstCheckpointItemIndex; // X360 +0x1910
        s32          maiNumCheckpoints[KI_MAX_ROUNDS]; // X360 +0x1914

        // Never called: pins the offsets the reconstruction depends on (grid base at +0, and
        // the pre-pointer members that keep their X360 offsets on x64) so an edit cannot drift.
        static void AssertLayout()
        {
            static_assert(sizeof(u32) == 4 && sizeof(u16) == 2, "primitive widths");
            static_assert(offsetof(SelectRoutes, mMenuData) == 0x000, "mMenuData @0");
            static_assert(offsetof(SelectRoutes, mi8CursorOffset) == 0x7D5, "cursor @0x7D5");
            static_assert(offsetof(SelectRoutes, mpGuiCache) == 0x1908, "guiCache @0x1908");
        }
    };
}
