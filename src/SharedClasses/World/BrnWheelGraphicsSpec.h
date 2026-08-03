#ifndef SHAREDCLASSES_WORLD_BRNWHEELGRAPHICSSPEC_H
#define SHAREDCLASSES_WORLD_BRNWHEELGRAPHICSSPEC_H

#include "types.hpp"

// ============================================================================
// BrnWheel::GraphicsSpec  --  SharedClasses/World/BrnWheelGraphicsSpec.h
//
// The per-wheel VISUAL manifest: the resource `WHE_<id>_GR.bin` publishes as
// "<id>_Graphics" (resource type 0x1000A). It is the wheel-side twin of
// BrnVehicle::GraphicsSpec and is deliberately tiny -- a version word plus the
// two models a wheel draws with.
//
// ---- SOURCES -------------------------------------------------------------
// MEMBER NAMES + ORDER: the DecFIGS DWARF (muVersion, mpWheelModel,
// mpCaliperModel; Construct() seeds both model slots to (Model*)-1 == "absent").
// SIZE: GraphicsSpecResourceType::GetSerialisedResourceDescriptor @0x8267D478
// publishes a single {size = 0x0C, align = 0x10} block -- exactly three 32-bit
// slots, so there is no fourth member.
// VERSION: FixUp @0x82678CF0 asserts `*resource != 1`.
// CONSUMER: BrnWorld::RaceCarEntityModule::RenderRaceCar @0x822CF6A0 reads
// `*(spec + 4)` -- mpWheelModel -- as the model it instances four times.
//
// ---- MEASURED against the shipped data (wheel-load wave, 2026-08-03) -------
// Wheels/WHE_51916650_GR.BNDL, platform 4, 44 resources: the single 0x1000A
// resource (id 0xB289585E) carries TWO imports, at +0x04 and +0x08, and both
// target type-42 (CgsGraphics::Model) resources in the SAME bundle. That is the
// direct byte-level confirmation of the two model slots below -- the import
// table names the offsets the loader rebases.
//
// ---- ⚠️ THE POINTER SLOTS ARE 4 BYTES WIDE, ON x64 TOO --------------------
// Same rule as BrnVehicle::GraphicsSpec (see that header's banner for the
// runtime probe that settled it): slots inside a SERIALISED record stay 32-bit
// on the x64 gate. `Pool::ResolveImportForEntry` writes the resolved address
// into a 4-byte slot and the resource heap lives below 4 GB, so the value is
// representable. A 12-byte descriptor with three members leaves no room for
// 8-byte pointers anyway -- widening either slot would make the record 20 bytes
// and every read past +0x04 garbage.
// ============================================================================

namespace CgsGraphics { struct Model; }

namespace BrnWheel
{

// FixUp's version gate (`*a2 != 1` -> assert).
const u32 KU_WHEEL_GRAPHICS_SPEC_VERSION = 1;

struct GraphicsSpec
{
    // ---- the serialised record (12 bytes; see the banner) -----------------
    u32 muVersion;         // +0   == KU_WHEEL_GRAPHICS_SPEC_VERSION
    u32 mpWheelModel;      // +4   load-relative CgsGraphics::Model*  (import slot 0)
    u32 mpCaliperModel;    // +8   load-relative CgsGraphics::Model*  (import slot 1)

    // ---- typed accessors: widen the load-relative u32 slots on read --------
    // The console's plain member read; the cast exists only because the slot is
    // four bytes wide on a 64-bit host.

    static const void* Widen( u32 luSlot )
    {
        return reinterpret_cast< const void* >( static_cast< uintptr_t >( luSlot ) );
    }

    const CgsGraphics::Model* GetWheelModel() const
    {
        return static_cast< const CgsGraphics::Model* >( Widen( mpWheelModel ) );
    }

    const CgsGraphics::Model* GetCaliperModel() const
    {
        return static_cast< const CgsGraphics::Model* >( Widen( mpCaliperModel ) );
    }
};

} // namespace BrnWheel

#endif // SHAREDCLASSES_WORLD_BRNWHEELGRAPHICSSPEC_H
