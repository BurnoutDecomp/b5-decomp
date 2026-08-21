#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"

// =============================================================================
// BrnTraffic::GraphicsStub -- the record's own TU.
//
// The original (leaked Feb-2007 SharedClasses/Traffic/BrnTrafficGraphicsStub.cpp)
// is a comment-and-include-only file: GraphicsStub is a plain two-slot record with
// NO methods, which the DecFIGS DWARF confirms (BrnTrafficGraphicsStub.h:52 lists
// the two members and nothing else). There is therefore no body to reconstruct
// here and none is invented.
//
// What this TU does carry is the COMPILE-ONLY instantiation gate, the sibling of
// GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics_layout_check.cpp:
// a concrete class compiles green as long as nothing ever makes one, so the class
// is scratch-instantiated once and both slots are exercised through the accessors
// the real consumers use. It emits no code that runs.
// =============================================================================

namespace BrnTraffic
{
    // Never called. Deliberately external linkage so it is not elided with a
    // C4505 "unreferenced local function" before it has done its job.
    //
    // ⚠️ COMPILE-ONLY GATE -- NOT a bring-up probe, NOT delete-when-stable.
    // It is the only place the record is built as an object; the two reads below
    // are the exact shapes TrafficCarStreamer::GetGraphicsSpec @0x8271D440 and
    // ::GetWheelGraphicsSpec @0x8271D678 use, so a slot retyped to a host pointer
    // (or renamed) fails HERE as well as at every real call site.
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
