#pragma once

// BrnMapIconManager.h
// BrnGui::MapIconManager - owns the on-map icon set the sat-nav / crash-nav maps draw
// (per-icon sat-nav info, the crash-nav and sat-nav icon pools, the road-sign icon
// manager and the event-icon manager) and answers the map UI's queries about which
// icon sits at a selection index.
//
// Sources (source-of-truth ladder):
//   * X360 BURNOUT_X360_ARTIST.XEX (binary; AUTHORITATIVE on layout/widths/branches):
//       MapIconManager (ctor)            @ 0x827DF138
//       ResetOwnerParameter              @ 0x824EBF98
//       SetRoadRuleBatchData             @ 0x824B2F80
//       GetNumRivalIcons                 @ 0x824F7B60
//       GetDriveThroughOrJunkyardAtIndex @ 0x824FAC10  (IDA "GetDriveThroughOrJun")
//       AddTeamToNetworkRivals           @ 0x824F4FF8  (UpdateSatNavIcons network-rivals pass)
//   * references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMapIconManager.h
//       names the nested enums, the member set/order and the method shapes.
//
// PARTIAL-LAYOUT NOTE (same convention as BrnSatNavIcon.h / BrnGameModule): the full
// class is a large aggregate (the X360 `this` spans 0xAA00+ bytes: a 50-entry sat-nav
// info array, a 50-entry crash-nav icon pool, a 16-entry sat-nav icon pool, the embedded
// road-sign and event icon managers, and a long tail of selection/flag state). Only the
// members the reconstructed methods touch are modelled, in their DWARF order and with
// their real types; the X360 byte offset of each is given in a comment as a reference
// (semantic parity, not byte-exact). The not-yet-reconstructed members (the icon pools,
// the event-icon manager, the bulk of the flag tail) land with the rest of the TU.

#include "types.hpp"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"          // GuiEventUpdateSatNav::SatNavIconInfo (+ EPlayerTeam bounds)
#include "GameSource/Gui/View/BrnRoadSignIconManager.h"  // BrnGui::RoadSignIconManager (embedded) + GuiEventRoadRuleBatchDataResponse fwd

namespace BrnGui
{
    class GuiCache;                  // embedded by pointer (mpGuiCache); declared in BrnGuiCache.h
    struct OnlineGameRoomPlayerInfo; // friend (writes meIconSizeMode; see the friend note below)

    class MapIconManager
    {
    public:
        // ---- nested enums (DWARF: BrnMapIconManager.h:52/58/78) ----
        enum IconSizeMode
        {
            E_ICONSIZE_SMALL = 0,
            E_ICONSIZE_LARGE = 1,
        };

        enum IconFilterMode
        {
            E_ICONFILTER_ALL                          = 0,
            E_ICONFILTER_LANDMARKS_ALL                = 1,
            E_ICONFILTER_LANDMARKS_PALM_BAY_HEIGHTS   = 2,
            E_ICONFILTER_LANDMARKS_SILVER_LAKE        = 3,
            E_ICONFILTER_LANDMARKS_HARBOR_TOWN        = 4,
            E_ICONFILTER_LANDMARKS_WHITE_MOUNTAIN     = 5,
            E_ICONFILTER_LANDMARKS_DOWNTOWN_PARADISE  = 6,
            E_ICONFILTER_PLAYER_ONLY                  = 7,
            E_ICONFILTER_NO_RIVALS                    = 8,
            E_ICONFILTER_WORLDOFFENCES                = 9,
        };

        enum OwnerId
        {
            E_OWNERID_INVALID                   = 0,
            E_SATNAV_MAP                        = 1,
            E_CRASHNAV_MAP                      = 2,
            E_PRERACE_FLYBY_MAP                 = 3,
            E_CRASHNAV_MAP_ONLINE               = 4,
            E_CRASHNAV_MAP_ONLINE_SELECT_ROUTE  = 5,
        };

        // ---- capacities (DWARF: BrnMapIconManager.h:88-92) ----
        static const s32 KI_MAX_CRASHNAV_MAP_ICONS = 50;
        static const s32 KI_MAX_SATNAV_MAP_ICONS   = 16;
        static const s32 KI_SATNAV_MAX_ICONS       = 50;

        // @ 0x827DF138 -- constructs the manager: brings each icon pool's elements up as
        // live (polymorphic) icon objects. In the X360 build this is the manual vtable
        // write across the three pools; in C++ it is the members' own construction.
        MapIconManager();

        // @ 0x824B2F80 -- forward the road-rule batch response into the embedded road-sign
        // icon manager (the X360 asserts the pointer is non-null first).
        void SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules);

        // @ 0x824FAC10 -- return the drive-through / junkyard sat-nav icon at the given map
        // selection index (skipping hidden drive-throughs and the road-rage / marked-man
        // mode's paint-shop, and the non-free-burn-lobby junkyards). When rival selection is
        // active the rival icons occupy the front of the index space, so the rival count is
        // subtracted off the incoming index first.
        const GuiEventUpdateSatNav::SatNavIconInfo* GetDriveThroughOrJunkyardAtIndex(s32 liIndex) const;

    private:
        // The online game-room screen's Update stores meIconSizeMode directly (the X360
        // inlines the raw stwx at 0x824B1418; no DWARF accessor row) -- friendship, not
        // a fabricated setter, is the honest exposure (wave-H keystone; same rule as
        // GuiCache's consumer friends).
        friend struct OnlineGameRoomPlayerInfo;

        // @ 0x824F7B60 -- count the rival icons in the sat-nav info set (network rivals,
        // marked men and ordinary rivals).
        s32 GetNumRivalIcons() const;

        // @ 0x824F4FF8 -- the network-rivals pass of UpdateSatNavIcons: for every
        // E_SATNAVICON_NETWORKRIVAL icon, stamp it with the player's current online team
        // (looked up from the cache by the player's active-race-car index).
        void AddTeamToNetworkRivals();

        // @ 0x824EBF98 -- reset the owner id back to E_OWNERID_INVALID (with a debug trace).
        void ResetOwnerParameter();

        // -------------------------------------------------------------------------------
        // Modelled members (DWARF order + types; X360 byte offsets are references, see the
        // partial-layout note above). Members the reconstructed methods do not touch are
        // omitted; they land with the rest of the TU.
        // -------------------------------------------------------------------------------

        // @0x0000 -- the per-icon sat-nav info set (validity loop / rival count / team stamp).
        GuiEventUpdateSatNav::SatNavIconInfo mSatNavIconInfo[KI_SATNAV_MAX_ICONS]; // X360 +0x0000
        GuiEventUpdateSatNav::SatNavIconInfo mPlayerIconInfo;                       // X360 +0x0960
        s32                                  miNumUsedIcons;                        // X360 +0x0990 (count for the loops)

        // [icon pools + event-icon manager + the bulk of the flag tail: not modelled here]

        bool                mbAllowRivalSelection;     // X360 +0x7081 (rivals occupy the front of the selection list)
        RoadSignIconManager mRoadSignIconManager;      // X360 +0x7090 (SetRoadRuleBatchData target)

        // [further selection/flag state: not modelled here]

        GuiCache*           mpGuiCache;                // X360 +0xA9F8 (drive-through list + player team lookups)
        OwnerId             mOwnerId;                  // X360 +0xAA00 (reset to invalid on release)
        // ADDITIVE GROW (OnlineGameRoomPlayerInfo keystone, wave H): the icon size mode
        // (DWARF h:458, the member right after mOwnerId). X360 +0xAA04 -- the game-room
        // screen's Update stores E_ICONSIZE_LARGE (stwx 1, iconmgr+0xAA04 @0x824B1418)
        // once the map cursor first snaps to the local player.
        IconSizeMode        meIconSizeMode;            // X360 +0xAA04
    };
}
