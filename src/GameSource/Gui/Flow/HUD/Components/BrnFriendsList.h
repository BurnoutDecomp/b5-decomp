#pragma once

// ===================================================================================
// BrnGui::FriendsListComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h
//
// The in-HUD friends-list component (the X360 asserts reference this header:
// SetGuiCachePointer @0x82473580 -> BrnFriendsList.h:665). It tracks a "current
// selection" cursor (a highlighted entry + two flag bytes + a count) and, when that
// selection changes, latches a *dirty snapshot* of it into a separate region for the
// renderer/animation layer to pick up, alongside a scroll-arrow indicator computed from
// the selected index vs the entry count.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (stores authoritative on width / which
// field is copied):
//   SetGuiCachePointer @ 0x82473580 -- latch the GuiCache pointer (+0x4100) and cache a
//       far cache field into muCachedCacheField (+0x870). Asserts the pointer non-NULL.
//   SetDirty           @ 0x8241F120 -- snapshot the current selection (+0x874..+0x882)
//       into the dirty region (+0x4108..+0x411A), raise the dirty flag (+0x4118 = 1) and
//       store a scroll-arrow indicator (+0x4114) computed from the selected index.
//
// LAYOUT NOTE: this is a large component (the X360 object spans well past +0x16000); only
// the fields the two reconstructed methods touch are modelled, as NAMED members grouped
// by role. The exact inter-field padding / the unrecovered bulk between the groups is
// NOT reconstructed (no DWARF / leak entry for this TU), so the groups are modelled
// directly rather than byte-padded to the X360 offsets -- this is a host build where
// pointer widening already breaks byte-exactness; the contract here is semantic parity
// with member-by-name access (the established BrnGuiKeyboard / CrashNavIconRenderer
// convention). The X360 byte offsets are cited per field for provenance only.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/BrnGuiCache.h"   // BrnGui::GuiCache (the cache pointer + far field)

namespace BrnGui
{
    class FriendsListComponent
    {
    public:
        // @ 0x82473580 -- latch the (non-NULL) GuiCache pointer and cache its far field.
        // Returns `this`.
        FriendsListComponent* SetGuiCachePointer(GuiCache* lpGuiCache);

        // @ 0x8241F120 -- snapshot the current selection into the dirty region, raise the
        // dirty flag, and store the scroll-arrow indicator. Returns `this`.
        FriendsListComponent* SetDirty();

    private:
        // ---- current selection cursor (X360 +0x870..+0x894) ----
        u32 muCachedCacheField;   // +0x870 -- cached GuiCache far field (set by SetGuiCachePointer)
        u32 muSelectionA;         // +0x874 -> snapshot mSnapshotC
        u32 muSelectionB;         // +0x878 -> snapshot mSnapshotA
        u32 muSelectionC;         // +0x87C -> snapshot mSnapshotB
        u8  mbSelectionFlagA;     // +0x880 -> snapshot mbSnapshotFlagA
        u8  mbSelectionFlagB;     // +0x881 -> snapshot mbSnapshotFlagB
        s8  mi8SelectedIndex;     // +0x882 -- highlighted entry (signed); drives the scroll indicator
        u32 muNumEntries;         // +0x894 -- total entries (scroll-indicator bound)

        // ---- the GuiCache this component reads through (X360 +0x4100) ----
        GuiCache* mpGuiCache;     // +0x4100 -- set by SetGuiCachePointer

        // ---- dirty snapshot region the renderer picks up (X360 +0x4108..+0x411A) ----
        u32 muSnapshotA;          // +0x4108 = muSelectionB
        u32 muSnapshotB;          // +0x410C = muSelectionC
        u32 muSnapshotC;          // +0x4110 = muSelectionA
        u32 muScrollIndicator;    // +0x4114 -- 0..3, from selected index vs count
        u8  mbDirty;              // +0x4118 -- raised to 1 by SetDirty
        u8  mbSnapshotFlagA;      // +0x4119 = mbSelectionFlagA
        u8  mbSnapshotFlagB;      // +0x411A = mbSelectionFlagB
    };
}
