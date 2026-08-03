// =================================================================================================
// VehiclePhysicsLinkStubs.cpp -- FLAG (TrafficPhysics de-fork link-mount stubs, 2026-08-03).
//
// TWO stubs. Both are LOUD (CGS_ASSERT(false) traps), both are dead today, and both exist for one
// measured reason.
//
// ⭐ WHY THEY EXIST. `PhysicalTrafficManager::maFullTrafficPhysics[20]` was folded from a byte-pinned
// `u8[5168]` stand-in to the real `BrnPhysics::Vehicle::TrafficPhysics` (the ODR de-fork -- see
// BrnPhysicalTrafficManager.h). TrafficPhysics is polymorphic, VehicleManager embeds the traffic
// manager by value, PhysicsModule embeds VehicleManager, and PhysicsModule's constructor is mounted
// -- so the constructor chain seats twenty vptrs and the linker demands a DEFINITION for every slot
// of TrafficPhysics's vtable. Exactly one slot was missing (`TrafficPhysics::Update`, the only
// virtual the class introduces), and defining it drags its two VehiclePhysics callees:
//     VehiclePhysics::UpdateShunt     @0x825FC748   100 X360 instructions
//     VehiclePhysics::UpdateCrashing  @0x82638810   732 X360 instructions
// Neither has a reconstructed body anywhere in the tree; both are declare-only in VehiclePhysics.h.
//
// ⚠️ THIS IS NOT "the de-fork needed stubs". The de-fork needed the VTABLE, and the vtable is the
// console's own behaviour: PhysicalTrafficManager's constructor @0x827E42E8 writes those same vptrs.
// The alternative was to leave the ODR fork standing, which is the silent-corruption trade the fold
// exists to retire -- a `TrafficPhysics::Construct` body written against the real class linking
// against a call site that strides the array by the console's 5168 while the host class is 4960.
//
// ⭐ THEY ARE DEAD, AND THE DEATH IS CHECKABLE. Nothing in the tree calls TrafficPhysics::Update
// (the console's caller, PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418, is in a TU that
// is not mounted), and nothing calls either stub directly -- grep for `UpdateShunt` / `UpdateCrashing`
// outside this file and TrafficPhysics_Construct.cpp returns the two declarations and nothing else.
//
// ⛔ NEVER ADD BEHAVIOUR HERE. Reconstruct the real body in VehiclePhysics.cpp and DELETE the stub.
// The failure mode if you forget is the good one: two definitions of the same symbol is a hard
// LNK2005, so you cannot body either function without being told about this file.
//
// ⚠️ AND NOTE WHAT A *SILENT* STUB WOULD HAVE COST HERE, since that is the recurring defect in this
// cluster: UpdateShunt is what consumes and clears mShuntEffect and applies the shunt impulse
// (`AddWorldSpaceImpulse`), and UpdateCrashing is the whole crash-damping curve. A quiet no-op for
// either would leave a traffic car that has been shunted looking perfectly healthy while silently
// never receiving the impulse -- plausible, wrong, and unreportable. Hence the trap.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
    // LINK STUB (TrafficPhysics de-fork 2026-08-03): body not reconstructed yet.
    // X360 @0x825FC748, 100 instructions. Reads mShuntEffect (+0x1130 / +0x1140), tests the two
    // lanes the shunt lives in, and on the live branch builds a world-space impulse from the body's
    // transform columns and calls ExternalPhysicsBody::AddWorldSpaceImpulse, then decays the
    // shunt life and clears the effect. Dense VMX128 (vmsum3fp128 / vrlimi128) -- its own wave.
    void VehiclePhysics::UpdateShunt(const BrnPlayerDriverControls*)
    {
        CGS_ASSERT(false, "VehiclePhysics::UpdateShunt: link stub (TrafficPhysics de-fork mount) -- "
                          "reconstruct from X360 @0x825FC748");
    }

    // LINK STUB (TrafficPhysics de-fork 2026-08-03): body not reconstructed yet.
    // X360 @0x82638810, 732 instructions. The crash-damping spine: the per-axis vlogefp/vexptefp
    // angular-velocity curve driven by the rodata coefficient tables unk_82014AC0..82014AF0 that
    // TrafficPhysics::Update's own banner already records as un-homed. Its own wave.
    void VehiclePhysics::UpdateCrashing(f32, const BrnPlayerDriverControls*)
    {
        CGS_ASSERT(false, "VehiclePhysics::UpdateCrashing: link stub (TrafficPhysics de-fork mount) "
                          "-- reconstruct from X360 @0x82638810");
    }
}
}
