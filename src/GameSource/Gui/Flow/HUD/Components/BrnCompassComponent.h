#ifndef BRN_COMPASS_COMPONENT_H
#define BRN_COMPASS_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                      // Vector2 (16-byte VMX; embedded initial positions)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"     // BrnFlaptComponent (base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // BrnGui::FlaptAnimatorComponent (mPlayerMarkerAnimator)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                           // BrnFlapt::MovieClipRef (embedded clips)

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.h
//
// BrnGui::CompassComponent -- the race HUD's rotating compass strip. Derives from
// BrnFlaptComponent and drives an apt "compass view" movie clip (scrolled by the
// player heading) plus a destination-marker clip (placed by relative bearing),
// with an embedded FlaptAnimatorComponent that plays the on/off-route player-marker
// state and cached initial clip positions used as the strip origin.
//
// Member names/types/enum + byte offsets from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Hud/Components/BrnCompassComponent.h),
// gated on the X360 ledger.
//
// CompassComponent is NON-polymorphic (no own vptr): the base BrnFlaptComponent
// (sizeof 0x0C, base at +0x00) is followed directly by the first own member
// (mPlayerMarkerAnimator @ +0x0C).
//
// LAYOUT (X360-attested by SetBearing @0x8241EC38, SetMarkerPos @0x8241ED10,
// UpdatePlayerMarkerState @0x8241EBA8):
//   +0x00  BrnFlaptComponent base
//   +0x0C  mPlayerMarkerAnimator   FlaptAnimatorComponent (sizeof 0x38)  [Run() @ this+0xC]
//   +0x44  mpGuiCache              GuiCache*                             [DWARF order; untouched by this batch]
//   +0x48  mePlayerOnTrack         EPlayerRouteState                     [lwz/stw 0x48]
//   +0x4C  mCompassViewMovie       MovieClipRef (8 bytes)                [SetPosition on this+0x4C]
//   +0x54  mDestMarkerMovie        MovieClipRef (8 bytes)                [SetPosition/SetVisible on this+0x54]
//   +0x60  mv2InitialViewPos       Vector2 (16 bytes)                    [lvx128/lfs 0x60]
//   +0x70  mv2InitialDestMarkerPos Vector2 (16 bytes)                    [lvx128/lfs 0x70]
//   +0x80  mfSingleViewLength      f32                                   [lfs 0x80]
// All member access is BY NAME.
//
// Only the three functions of this batch are bodied so far; grow this header
// additively as the remaining CompassComponent TUs land.
// ============================================================================

namespace BrnGui
{
    class GuiCache;   // stored by-pointer only

    class CompassComponent : public BrnFlaptComponent
    {
    public:
        // BrnCompassComponent.h:96 -- the player's route-tracking state; indexes both
        // the assert bound and the KAPC_PLAYER_ROUTE_STATES frame-name table.
        enum EPlayerRouteState
        {
            E_PLAYER_ROUTE_ON_COURSE            = 0,
            E_PLAYER_ROUTE_OFF_COURSE           = 1,
            E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS = 2,
            E_PLAYER_ROUTE_COUNT                = 3,
        };

    private:
        // @ 0x8241EBA8 -- on a state change, latch it and play the matching animator
        // frame (KAPC_PLAYER_ROUTE_STATES). No-op if unchanged.
        void UpdatePlayerMarkerState(EPlayerRouteState lePlayerOnTrack);

        // @ 0x8241EC38 -- scroll the compass view clip to the given heading (degrees).
        void SetBearing(f32 lfBearing);

        // @ 0x8241ED10 -- place (or hide) the destination-marker clip by relative bearing.
        void SetMarkerPos(f32 lfMarkerBearing, bool lbShowMarker);

        // BrnCompassComponent.h:105 -- per-route-state animator frame names, indexed by
        // EPlayerRouteState (XEX .data off_82F248AC). DWARF-attested class static.
        // Only [E_PLAYER_ROUTE_ON_COURSE] == "onTrack" is X360-attested; [1]/[2] are
        // filled by the consolidator from the .data table.
        static const char* const KAPC_PLAYER_ROUTE_STATES[E_PLAYER_ROUTE_COUNT];

        // BrnCompassComponent.h:44
        typedef FlaptAnimatorComponent MarkerAnimatorType;

        MarkerAnimatorType     mPlayerMarkerAnimator;    // +0x0C
        GuiCache*              mpGuiCache;               // +0x44
        EPlayerRouteState      mePlayerOnTrack;          // +0x48
        BrnFlapt::MovieClipRef mCompassViewMovie;        // +0x4C
        BrnFlapt::MovieClipRef mDestMarkerMovie;         // +0x54
        Vector2                mv2InitialViewPos;        // +0x60
        Vector2                mv2InitialDestMarkerPos;  // +0x70
        f32                    mfSingleViewLength;       // +0x80
    };
}

#endif // BRN_COMPASS_COMPONENT_H
