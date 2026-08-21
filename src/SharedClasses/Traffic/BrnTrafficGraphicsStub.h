#ifndef SHAREDCLASSES_TRAFFIC_BRNTRAFFICGRAPHICSSTUB_H
#define SHAREDCLASSES_TRAFFIC_BRNTRAFFICGRAPHICSSTUB_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"   // CgsGraphics::Ptr32<T> (the 4-byte serialised slot)

// The two types the record's slots name. Included rather than forward-declared:
// both homes are complete and each includes only "types.hpp", so the cascade is
// zero headers deep, and consumers get the complete type.
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"   // BrnVehicle::GraphicsSpec (import slot 0's target)
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"     // BrnWheel::GraphicsSpec   (import slot 1's target)

#include <cstddef>   // offsetof (used only by the never-called _AssertLayout)

// =============================================================================
// BrnTraffic::GraphicsStub -- resource type 65557 (0x10015), the record a traffic
// car's VEH_T<code>_GR bundle carries instead of a full BrnVehicle::GraphicsSpec:
// two BUNDLE IMPORT slots naming the body and wheel graphics that car reuses.
// Shape: DWARF SharedClasses/Traffic/BrnTrafficGraphicsStub.h:52 (:55 / :56).
// Handler: BrnTraffic::GraphicsStubResourceType. Consumers:
//   TrafficCarStreamer::GetGraphicsSpec      @0x8271D440  reads *(stub + 0)
//   TrafficCarStreamer::GetWheelGraphicsSpec @0x8271D678  reads *(stub + 4)
//
// THE SLOTS STAY Ptr32<T>. GetSerialisedResourceDescriptor @0x82760708 bakes the
// literal {size 8, align 4} and GetImportPointer @0x82752780 hands out offsets 0
// and 4, so CgsResource::Pool::ResolveImportForEntry writes the resolved wheel
// pointer at byte 4. Host pointers would seat mpWheelGraphics at byte 8, where the
// loader never wrote, and every traffic wheel would read a stale pointer with
// nothing reporting an error.
// =============================================================================

namespace BrnTraffic
{

class GraphicsStub
{
public:
    // +0x00 -- import slot 0. GetGraphicsSpec @0x8271D440 returns `*(stub + 0)`.
    CgsGraphics::Ptr32<BrnVehicle::GraphicsSpec> mpVehicleGraphics;
    // +0x04 -- import slot 1. GetWheelGraphicsSpec @0x8271D678 returns `*(stub + 4)`.
    CgsGraphics::Ptr32<BrnWheel::GraphicsSpec>   mpWheelGraphics;

    // Never called. Pins the member order, the two import-slot seats, and the
    // 8-byte record size. Inline on purpose, so it fires for every TU that
    // includes this header rather than only the sibling .cpp.
    static void _AssertLayout()
    {
        static_assert(sizeof(CgsGraphics::Ptr32<BrnVehicle::GraphicsSpec>) == 4,
                      "GraphicsStub's slots are the console's FOUR bytes -- widening them is the "
                      "X360-offsets-on-x64 bug this whole banner exists to stop");
        static_assert(offsetof(GraphicsStub, mpVehicleGraphics) == 0,
                      "mpVehicleGraphics is import slot 0 -- GetImportPointer @0x82752780 hands out "
                      "offset 0 for it and GetGraphicsSpec @0x8271D440 reads *(stub + 0)");
        static_assert(offsetof(GraphicsStub, mpWheelGraphics) == 4,
                      "mpWheelGraphics is import slot 1 at offset 4 -- GetImportPointer @0x82752780 "
                      "hands out 4 and GetWheelGraphicsSpec @0x8271D678 reads *(stub + 4). This is "
                      "the assert that fires the moment anyone retypes these as host pointers.");
        static_assert(sizeof(GraphicsStub) == 8,
                      "the whole serialised record is 8 bytes -- the literal 0x800000004 that "
                      "GetSerialisedResourceDescriptor @0x82760708 stores as {size 8, align 4}");
    }
};

}   // namespace BrnTraffic

#endif   // SHAREDCLASSES_TRAFFIC_BRNTRAFFICGRAPHICSSTUB_H
