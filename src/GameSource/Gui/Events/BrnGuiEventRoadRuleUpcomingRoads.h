// BrnGuiEventRoadRuleUpcomingRoads.h
// Home of BrnGui::GuiEventRoadRuleUpcomingRoads -- the GUI event payload describing the
// road-rule "upcoming roads" set sent to the sat-nav / HUD.
//
// This slice reconstructs ONLY the out-of-line enum converter the X360 ARTIST build
// emits for this event:
//
//   ConvertG (truncated; ConvertGameState...) @ 0x824F6170
//       Maps a 3-valued game-state enum (0..2) to the road-rule "upcoming roads"
//       category id used by this event, with two non-fatal range guards:
//         * the input value, after its cast, must be in [0, 2]  (else fire the
//           "Invalid enum after cast - original value was <v>, cast to <v>\n" assert)
//         * the post-switch default fires the "Invalid gamestate enum (<v>)\n" assert
//       Mapping (X360 branch table): 0 -> 0, 1 -> 2, 2 -> 1.
//
//   The X360 emits this as a member function whose `this` is unused (r3 never read);
//   only the enum argument (r4) drives the body. Modelled faithfully as a static helper
//   so the unused-this shape is explicit.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event base

namespace BrnGui
{
    // The road-rule "upcoming roads" GUI event. Only the enum converter is in scope for
    // this slice (X360-attested out-of-line); the event's data members are not modelled
    // here (none are load-bearing for the converter).
    struct GuiEventRoadRuleUpcomingRoads : public CgsModule::Event
    {
        // @ 0x824F6170 -- convert a 3-valued game-state enum to this event's road
        // category id (0->0, 1->2, 2->1), with two non-fatal range guards. Static
        // because the X360 body never reads `this`.
        static s32 ConvertGameStateToCategory( u32 luGameState );
    };
}
