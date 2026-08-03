#ifndef BRN_GUI_SHARED_H
#define BRN_GUI_SHARED_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/BrnGuiShared.h
//
// Shared GUI enumerations + lookup data used across the BrnGui flow components.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; enum names/values pinned from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiShared.h),
// gated on the X360 ledger.
//
// Only the slice the currently-landed TUs need is declared here (grow additively):
//   - BrnGui::ERoadIcon            -- the 64 sat-nav road-icon ids (the apt timeline
//                                     label for each road, named by its numeric code).
//   - gapcRoadIconNames[]          -- the parallel name table FlaptRoadSignIconComponent
//                                     ::FindRoadFromName (X360 @ 0x8241CD08) walks to map
//                                     an icon name string back to its ERoadIcon. The
//                                     names ARE the enum suffixes ("397134", ...); the
//                                     table is DEFINED by this header's data TU
//                                     (BrnGuiShared.cpp / off_82F27C98), declared here.
//   - BrnGui::EGuiResourceId       -- the GUI resource-id enum (slice), and
//   - gGuiResourceIdentifier[]     -- the parallel resource-NAME table (X360 off_82F278E0).
//   - BrnGui::ECompassPoints       -- the 8-point compass PreRaceFlyByState::FindEventDirection
//                                     (X360 @0x824B4EC8) returns.
// ============================================================================

namespace BrnGui
{
    // ------------------------------------------------------------------------
    // BrnGuiShared.h (DWARF) -- the GUI resource id enum. Only the enumerators the
    // landed TUs name are spelled here; the enum is dense and grows additively.
    //
    // ⚠ VERSION DRIFT: the DecFIGS PS3 DWARF ends this enum at
    // E_GUI_RESOURCEID_NUM = 228. The X360 ARTIST build this tree reconstructs has
    // **237** ids -- its resource-NAME table (off_82F278E0) has 237 entries, and both
    // PhotoBoothComponent::EnsureResourcesAre(Un)Loaded @0x824B34F0/@0x824B3578 compare
    // mPhotoResourceToLoad.muId against the literal 237 (cmplwi 0xED). The NAMES below
    // are the DWARF's; the VALUES are gated on the X360 ledger (each was read back out
    // of gGuiResourceIdentifier[] at that index -- see the trailing comments).
    // ------------------------------------------------------------------------
    enum EGuiResourceId
    {
        // PhotoBoothComponent::SetVisualStyle picks one of these three by EPhotoBoothStyle.
        E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_BASIC       = 91,   // "B5PhotoBoothComponent"
        E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV         = 92,   // "B5PhotoBoothComponentDMV"
        E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV_UPGRADE = 93,   // "B5PhotoBoothCptDMVUpgrade"

        // LicenseComponent::SetPlayerInfo's KA_LICENSE_RESOURCES_AVAILABLE / the two
        // elite tuples (X360 .data @0x82F25360 / 0x82F25390 / 0x82F25398).
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_0             = 97,   // "B5LicenseRank0"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_1             = 98,   // "B5LicenseRank1"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_2             = 99,   // "B5LicenseRank2"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_3             = 100,  // "B5LicenseRank3"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_4             = 101,  // "B5LicenseRank4"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_5             = 102,  // "B5LicenseRank5"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_ELITE         = 103,  // "B5LicenseElite"
        E_GUI_RESOURCEID_APT_COMPONENT_LICENSE_RANK_ELITE_COMPLETE = 104, // "B5LicenseEliteFinal"

        // Terminating count. X360-gated (see the drift note above).
        E_GUI_RESOURCEID_NUM = 237,
    };

    // BrnGuiShared.h:473 (DWARF) -- the resource-NAME table, one entry per
    // EGuiResourceId, in id order (X360 off_82F278E0). It is a SHARED global, not a
    // file-static: GuiCache's own load/unload pump indexes it, and so do
    // PhotoBoothComponent::OnLoad @0x8243CD68, LicenseComponent::OnLoad @0x82440AC0 and
    // LicenseComponent::Update @0x8243C0B8, each of which turns its resource tuple's id
    // straight into the apt movie name it asks StateInterface::PlayAptMovie for.
    // DEFINED by BrnGuiCache.cpp (the TU that owns the resource pump).
    extern const char* const gGuiResourceIdentifier[E_GUI_RESOURCEID_NUM];

    // BrnGuiShared.h:416 (DWARF) -- the 8-sector compass the pre-race fly-by resolves an
    // event's bearing into. The order is counter-clockwise from north (N, NW, W, SW, S,
    // SE, E, NE) because FindEventDirection @0x824B4EC8 floors a 0..360 bearing measured
    // from the reference vector by 45 and returns the sector index directly; that same
    // index also selects from the DIRECTION_* localisation-id table @0x82F27820, whose 8
    // entries are in exactly this order (image read: scratchpad/waveJ/prfb_rodata.txt).
    // The cpp:1648 assert bounds the result against 8 (`cmpwi cr6, r31, 8` / `blt`).
    //
    // FIXED UNDERLYING TYPE `: s32` IS LOAD-BEARING: the leaf class header
    // GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h forward-declares this enum
    // opaquely as `namespace BrnGui { enum ECompassPoints : s32; }`, and an opaque-enum
    // declaration and its definition must agree on the underlying type. (The DWARF does
    // not record an underlying type for it; s32 is what the existing in-tree opaque
    // declaration already committed to, and it matches the console's 4-byte stores of the
    // returned value.)
    enum ECompassPoints : s32
    {
        E_COMPASS_POINTS_N     = 0,
        E_COMPASS_POINTS_NW    = 1,
        E_COMPASS_POINTS_W     = 2,
        E_COMPASS_POINTS_SW    = 3,
        E_COMPASS_POINTS_S     = 4,
        E_COMPASS_POINTS_SE    = 5,
        E_COMPASS_POINTS_E     = 6,
        E_COMPASS_POINTS_NE    = 7,
        E_COMPASS_POINTS_COUNT = 8,
        E_COMPASS_POINTS_START = 0,
    };

    // BrnGuiShared.h:327 (DWARF) -- a sat-nav road icon. Each value's numeric suffix
    // is the apt timeline-label code for that road's sign artwork; the parallel name
    // table below holds those code strings in the same order.
    enum ERoadIcon
    {
        E_ROADICON_397134 = 0,
        E_ROADICON_394306 = 1,
        E_ROADICON_535195 = 2,
        E_ROADICON_505312 = 3,
        E_ROADICON_393062 = 4,
        E_ROADICON_396133 = 5,
        E_ROADICON_394474 = 6,
        E_ROADICON_386215 = 7,
        E_ROADICON_394965 = 8,
        E_ROADICON_385737 = 9,
        E_ROADICON_392688 = 10,
        E_ROADICON_393166 = 11,
        E_ROADICON_397409 = 12,
        E_ROADICON_396228 = 13,
        E_ROADICON_385860 = 14,
        E_ROADICON_392532 = 15,
        E_ROADICON_392323 = 16,
        E_ROADICON_396706 = 17,
        E_ROADICON_393198 = 18,
        E_ROADICON_395917 = 19,
        E_ROADICON_392997 = 20,
        E_ROADICON_393860 = 21,
        E_ROADICON_396988 = 22,
        E_ROADICON_396188 = 23,
        E_ROADICON_397165 = 24,
        E_ROADICON_395490 = 25,
        E_ROADICON_387153 = 26,
        E_ROADICON_393756 = 27,
        E_ROADICON_392684 = 28,
        E_ROADICON_394853 = 29,
        E_ROADICON_506394 = 30,
        E_ROADICON_394273 = 31,
        E_ROADICON_396114 = 32,
        E_ROADICON_395952 = 33,
        E_ROADICON_396456 = 34,
        E_ROADICON_393760 = 35,
        E_ROADICON_397201 = 36,
        E_ROADICON_396742 = 37,
        E_ROADICON_390723 = 38,
        E_ROADICON_397455 = 39,
        E_ROADICON_561416 = 40,
        E_ROADICON_396460 = 41,
        E_ROADICON_383595 = 42,
        E_ROADICON_396718 = 43,
        E_ROADICON_561415 = 44,
        E_ROADICON_387198 = 45,
        E_ROADICON_397601 = 46,
        E_ROADICON_395656 = 47,
        E_ROADICON_386734 = 48,
        E_ROADICON_397702 = 49,
        E_ROADICON_395197 = 50,
        E_ROADICON_506519 = 51,
        E_ROADICON_395600 = 52,
        E_ROADICON_392589 = 53,
        E_ROADICON_393911 = 54,
        E_ROADICON_394487 = 55,
        E_ROADICON_397348 = 56,
        E_ROADICON_393900 = 57,
        E_ROADICON_393120 = 58,
        E_ROADICON_387078 = 59,
        E_ROADICON_393762 = 60,
        E_ROADICON_535196 = 61,
        E_ROADICON_561121 = 62,
        E_ROADICON_561413 = 63,
        E_ROADICON_COUNT  = 64,
    };

    // off_82F27C98 .. off_82F27D98 (X360) -- the road-icon name table, one entry per
    // ERoadIcon, in enum order. FindRoadFromName linear-searches it for an exact match
    // and returns the matching index as an ERoadIcon. Owned + defined by BrnGuiShared's
    // own data TU (the 64-string set is serialised data, not in the function export);
    // declared here so FindRoadFromName resolves against the single shared home.
    extern const char* const gapcRoadIconNames[E_ROADICON_COUNT];
}

#endif // BRN_GUI_SHARED_H
