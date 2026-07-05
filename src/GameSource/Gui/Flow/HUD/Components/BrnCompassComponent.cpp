// ===================================================================================
// BrnGui::CompassComponent  -- implementation
//   class:BrnGui::CompassComponent
//
//   UpdatePlayerMarkerState @ 0x8241EBA8
//   SetBearing              @ 0x8241EC38
//   SetMarkerPos            @ 0x8241ED10
// Reconstructed store-for-store from the X360 asm; DWARF-attested shape.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // --------------------------------------------------------------------------
    // KAPC_PLAYER_ROUTE_STATES (BrnCompassComponent.h:105 / XEX .data off_82F248AC).
    // DWARF-attested class static; only [E_PLAYER_ROUTE_ON_COURSE] == "onTrack" is
    // X360-attested. CONSOLIDATOR: fill [E_PLAYER_ROUTE_OFF_COURSE] and
    // [E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS] from the .data string table -- the values
    // below marked TODO are placeholders and MUST NOT ship as-is.
    // --------------------------------------------------------------------------
    const char* const CompassComponent::KAPC_PLAYER_ROUTE_STATES[E_PLAYER_ROUTE_COUNT] =
    {
        "onTrack",   // [0] E_PLAYER_ROUTE_ON_COURSE            -- X360-attested
        "",          // [1] E_PLAYER_ROUTE_OFF_COURSE           -- TODO consolidator (XEX .data @0x82F248AC)
        "",          // [2] E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS -- TODO consolidator (XEX .data @0x82F248AC)
    };

    // @ 0x8241EBA8 -- when the player's on/off-route state changes, remember it and
    // play the matching animator frame (the per-state frame-name table, XEX .data
    // off_82F248AC). No-op when the state is unchanged.
    void CompassComponent::UpdatePlayerMarkerState(EPlayerRouteState lePlayerOnTrack)
    {
        CGS_ASSERT((E_PLAYER_ROUTE_ON_COURSE <= lePlayerOnTrack) && (E_PLAYER_ROUTE_COUNT > lePlayerOnTrack),
                   "(E_PLAYER_ROUTE_ON_COURSE <= lePlayerOnTrack) && (E_PLAYER_ROUTE_COUNT > lePlayerOnTrack)");

        if (lePlayerOnTrack != mePlayerOnTrack)
        {
            mePlayerOnTrack = lePlayerOnTrack;
            mPlayerMarkerAnimator.Run(KAPC_PLAYER_ROUTE_STATES[lePlayerOnTrack]);
        }
    }

    // @ 0x8241EC38 -- rotate the compass view clip so bearing (degrees, 0..360) maps
    // linearly onto the strip: X = (lfBearing/360) * mfSingleViewLength + initialX,
    // keeping the clip's initial Y. The X360 copies the whole initial-position vector
    // to a stack temp, overwrites only its X lane, then hands it to SetPosition.
    void CompassComponent::SetBearing(f32 lfBearing)
    {
        CGS_ASSERT(lfBearing >= -0.1, "lfBearing >= -0.1");            // BrnCompassComponent.h:298
        CGS_ASSERT(lfBearing <= 360.1f, "lfBearing <= 360.1f");        // BrnCompassComponent.h:299

        // flt_82004920 == 0.0027777778 == 1/360 (degrees -> [0,1] strip fraction).
        Vector2 lPos = mv2InitialViewPos;
        lPos.x = (lfBearing * (1.0f / 360.0f)) * mfSingleViewLength + mv2InitialViewPos.x;
        mCompassViewMovie.SetPosition(lPos);
    }

    // @ 0x8241ED10 -- place the destination marker on the compass strip when it should
    // be shown; otherwise just hide the marker clip. When shown, the marker bearing
    // (-180..180) maps linearly the same way SetBearing maps the compass view.
    void CompassComponent::SetMarkerPos(f32 lfMarkerBearing, bool lbShowMarker)
    {
        CGS_ASSERT(lfMarkerBearing >= -180.1, "lfMarkerBearing >= -180.1");     // BrnCompassComponent.h:335
        CGS_ASSERT(lfMarkerBearing <= 180.1f, "lfMarkerBearing <= 180.1f");     // BrnCompassComponent.h:336

        if (lbShowMarker)
        {
            // flt_82004920 == 0.0027777778 == 1/360.
            Vector2 lPos = mv2InitialDestMarkerPos;
            lPos.x = (lfMarkerBearing * (1.0f / 360.0f)) * mfSingleViewLength + mv2InitialDestMarkerPos.x;
            mDestMarkerMovie.SetPosition(lPos);
        }
        else
        {
            mDestMarkerMovie.SetVisible(lbShowMarker);
        }
    }
}
