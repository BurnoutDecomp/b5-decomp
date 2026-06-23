// BrnGuiFsmController.h
// Home of BrnGui::GuiFsmController -- the GUI flow finite-state-machine controller. It
// owns one flow object per GUI "flow id" (HUD / Screen / Overlay) and tracks, per flow,
// whether a state transition is currently pending.
//
// This slice reconstructs ONLY the out-of-line accessor the X360 ARTIST build emits:
//
//   IsTransitionPending @ 0x824ECCF8 -> bool. Given a flow id (0..2), returns whether
//                                       that flow has a transition pending, with two
//                                       non-fatal guards:
//         * the flow id must be in [0, 2]   (else "Invalid Flow Id passed", DWARF
//           GameSource/Gui/BrnGuiFsmController.h:190)
//         * the flow at that id must be set  (else "Error, flow not set", :191)
//
// LAYOUT (X360 authoritative):
//   +0x00  mapFlows[3]            -- three flow-object pointers (index = 4 * flow id);
//                                    `lwzx r,4*a2,this`. The flow object type is not in
//                                    scope for this slice, so the slots are held as
//                                    opaque pointers (void*), accessed BY NAME.
//   +0x94  mabTransitionPending[3] -- per-flow "transition pending" byte (index = flow
//                                    id, stride 1); `lbz r3, 0x94(a2 + this)`.
// Only the two members touched by IsTransitionPending are modelled, at their X360-proven
// offsets; the intervening bytes (0x0C .. 0x93) are an explicitly-reserved span so the
// pending-flags array sits at its proven offset. Honest layout boundary, not fabricated.

#pragma once

#include "types.hpp"

namespace BrnGui
{
    // The GUI flow FSM controller. Only the members read by IsTransitionPending are
    // modelled at their X360-proven offsets.
    class GuiFsmController
    {
    public:
        // Number of GUI flows the controller drives (HUD / Screen / Overlay). The X360
        // guards the flow id against [0, KI_NUM_FLOWS).
        static const u32 KU_NUM_FLOWS = 3;

        // @ 0x824ECCF8 -- return whether the flow identified by luFlowId has a state
        // transition pending. Asserts the id is valid and the flow is set (both
        // non-fatal: the X360 returns the stored byte regardless).
        bool IsTransitionPending( u32 luFlowId ) const;

    private:
        void* mapFlows[KU_NUM_FLOWS];          // @0x00 .. 0x0B  (flow object pointers, BY NAME)
        u8    maHeadReserved[0x94 - 0x0C];     // @0x0C .. 0x93  (other controller state)
        u8    mabTransitionPending[KU_NUM_FLOWS]; // @0x94 .. 0x96  (per-flow pending flag)
    };
}
