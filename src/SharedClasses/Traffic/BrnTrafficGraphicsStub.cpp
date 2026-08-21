#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"

// =============================================================================
// BrnTraffic::GraphicsStub -- the record's own TU. GraphicsStub is a plain
// two-slot record with no methods (DWARF BrnTrafficGraphicsStub.h:52), so there is
// no body to reconstruct. What lands here is the compile-only instantiation gate.
// =============================================================================

namespace BrnTraffic
{
    // COMPILE-ONLY GATE -- not a bring-up probe, not delete-when-stable. Never
    // called; external linkage so it is not elided before it has done its job.
    // The only place the record is built as an object: the two reads below are the
    // shapes TrafficCarStreamer::GetGraphicsSpec @0x8271D440 and
    // ::GetWheelGraphicsSpec @0x8271D678 use, so a slot retyped to a host pointer
    // fails here as well as at every real call site.
    void GraphicsStub_LayoutCheck()
    {
        GraphicsStub lStub;
        lStub.mpVehicleGraphics.muSlot = 0u;
        lStub.mpWheelGraphics.muSlot   = 0u;

        const BrnVehicle::GraphicsSpec* lpVehicleGraphics = lStub.mpVehicleGraphics;
        const BrnWheel::GraphicsSpec*   lpWheelGraphics   = lStub.mpWheelGraphics;
        (void)lpVehicleGraphics;
        (void)lpWheelGraphics;

        GraphicsStub::_AssertLayout();
    }
}
