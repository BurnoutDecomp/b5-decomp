#ifndef SHAREDCLASSES_TRAFFIC_BRNTRAFFICGRAPHICSSTUB_H
#define SHAREDCLASSES_TRAFFIC_BRNTRAFFICGRAPHICSSTUB_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"   // CgsGraphics::Ptr32<T> (the 4-byte serialised slot)

// The two types the record's slots name. The leaked original FORWARD-DECLARED
// both here rather than including them; this reconstruction includes the real
// homes instead, because AGENTS.md permits a forward declaration only for a
// cycle, a genuinely LARGE transitive cascade, or a type with no reference to
// reconstruct -- and none of the three applies (MEASURED: both homes exist and
// are complete, and each one's ONLY #include is "types.hpp", so the cascade is
// zero headers deep). Including them also hands every consumer the complete
// type, which is what TrafficCarStreamer::GetGraphicsSpec @0x8271D440 and
// ::GetWheelGraphicsSpec @0x8271D678 return.
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"   // BrnVehicle::GraphicsSpec (import slot 0's target)
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"     // BrnWheel::GraphicsSpec   (import slot 1's target)

#include <cstddef>   // offsetof (used only by the never-called _AssertLayout)

// =============================================================================
// BrnTraffic::GraphicsStub -- the resource a TRAFFIC car's VEH_T<code>_GR bundle
// carries INSTEAD of a full BrnVehicle::GraphicsSpec. Resource type 65557
// (0x10015); its handler is BrnTraffic::GraphicsStubResourceType, its home
// header BrnTrafficGraphicsStubResourceType.h.
//
// It is a pair of pointers naming the body graphics and the wheel graphics the
// traffic car reuses -- nothing else. Both are BUNDLE IMPORTS: the loader
// resolves them and writes the resolved addresses into the two slots.
//
// SHAPE (three sources agree):
//   * DecFIGS DWARF, SharedClasses/Traffic/BrnTrafficGraphicsStub.h:52
//         BrnVehicle::GraphicsSpec* mpVehicleGraphics;   // .h:55
//         BrnWheel::GraphicsSpec*   mpWheelGraphics;     // .h:56
//   * leaked Feb-2007 SharedClasses/Traffic/BrnTrafficGraphicsStub.h -- the same
//     two members, in the same order, with both GraphicsSpec types
//     FORWARD-DECLARED rather than included. See the include note at the top of
//     this file for why the reconstruction includes their real homes instead.
//   * X360 ARTIST asm, the two consumers:
//         TrafficCarStreamer::GetGraphicsSpec      @0x8271D440  ->  reads *(stub + 0)
//         TrafficCarStreamer::GetWheelGraphicsSpec @0x8271D678  ->  reads *(stub + 4)
//
// ⭐ WHY THE MEMBERS ARE Ptr32<T> AND NOT HOST POINTERS -- READ BEFORE "FIXING".
// This is the tree's #1 recurring defect (X360 widths on the x64 host), and here
// it would be a live wrong-address read, not a cosmetic one:
//   * GraphicsStubResourceType::GetSerialisedResourceDescriptor @0x82760708
//     stores the 64-bit literal 0x800000004 at +0, i.e. {size = 8, alignment = 4}.
//     The WHOLE resource is EIGHT bytes -- two 4-byte slots and nothing else.
//   * GetImportPointer @0x82752780 hands out offsets 0 and 4 for the two slots,
//     and the shipped bundles' own import tables declare imports at exactly
//     {0x0, 0x4} in 42/42 of the retail VEH_T*_GR.BIN (measured; see the
//     GraphicsStub section of tools/assets/bundles/vehicle_transcode.py).
//   * CgsResource::Pool::ResolveImportForEntry (CgsResourcePool.cpp, X360
//     0x828F6068) writes the resolved pointer AT THE BUNDLE'S IMPORT OFFSET, and
//     stores only the low 32 bits for a sub-4 GB target (the project's low-memory
//     reservation). So the wheel-graphics pointer physically lands at byte 4.
// Declaring these as host `GraphicsSpec*` would seat mpWheelGraphics at byte 8,
// where the loader never wrote, and every traffic wheel would read a stale/zero
// pointer while nothing anywhere reported an error. Ptr32<T> pins the console's
// 4 bytes and stays source-compatible at every `stub.mpWheelGraphics->...` /
// `GraphicsSpec* p = stub.mpWheelGraphics;` call site -- the same convention
// CgsGraphics::Model / CgsGraphics::Instance / BrnVehicle::GraphicsSpec already
// use for their serialised slots.
//
// The transcoder is a deliberate NO-OP for this type (both slots are dead bytes
// on disc because ResolveImportForEntry overwrites them before anything reads
// them), so the PC data has the same 8-byte, 4-byte-slot shape as the console's.
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

    // Never called. Pins the facts that are TRUE ON BOTH PLATFORMS because every
    // member is a fixed-width serialised slot: the member order, the two console
    // seats the asm and the shipped import tables both name, and the 8-byte
    // record size GetSerialisedResourceDescriptor bakes as a literal.
    // Inline on purpose -- it then fires for every TU that includes this header,
    // not only for the sibling .cpp.
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
