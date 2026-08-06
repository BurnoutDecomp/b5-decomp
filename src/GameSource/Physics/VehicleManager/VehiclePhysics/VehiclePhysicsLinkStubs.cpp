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
    // =============================================================================================
    // ⭐ 2026-08-06 (big-five #3, UpdateVehiclePhysics wave) -- the RaceCarPhysics.cpp MOUNT set.
    // RaceCarPhysics.cpp's own banner measured its five LNK2019s; this wave mounts that TU (the
    // manager's per-car dispatch needs RaceCarPhysics::Update), resolves the GetAftertouchValues
    // overload fork and the gbVehicleBounceBoosting home properly, and carries the remaining
    // TWO orchestrator holes as the same loud trap-stub pattern as above:
    //
    //   VehiclePhysics::Update         @0x826412C0 -- THE per-car conductor of the 54 force
    //       leaves ([[vehicle-physics-is-the-wall]]'s named seam). Signature is the DWARF's
    //       (VehiclePhysics.h:1084), spelled per the conformed declaration.
    //   VehiclePhysics::UpdateSteering @0x825D3720 -- the follow-up steering pass. ⚠️ The DWARF
    //       declares a FOUR-argument form (:1499 `(float32_t, float32_t, VecFloat, bool)`); the
    //       committed 2-arg declaration came off the RaceCarPhysics::Update call-site asm. The
    //       stub matches the COMMITTED declaration so the one real call site links; the
    //       reconstruction wave owns reconciling the arity against the DWARF.
    //
    // ⛔ Same rule as above: NEVER ADD BEHAVIOUR HERE. A silent Update no-op would be the
    // invisible-forever handling bug -- every car frozen mid-air with plausible state. Trap.
    // =============================================================================================

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehiclePhysics::Update(const rw::math::vpu::Matrix44Affine*,
                                const BrnPlayerDriverControls*, bool, bool, bool,
                                CgsNumeric::Random&, Vector3, Vector3)
    {
        CGS_ASSERT(false, "VehiclePhysics::Update: link stub (the 54-leaf per-car conductor) -- "
                          "reconstruct from X360 @0x826412C0 before PhysicsModule::Update lands");
    }

    // LINK STUB (UpdateVehiclePhysics wave): body not reconstructed yet.
    void VehiclePhysics::UpdateSteering(s8, f32)
    {
        CGS_ASSERT(false, "VehiclePhysics::UpdateSteering: link stub -- reconstruct from X360 "
                          "@0x825D3720 (DWARF declares a 4-arg form; see the stub banner)");
    }

    // (gbVehicleBounceBoosting needs NO home: the extern was a data fork of
    //  msPlayerParams.mbLaunchActive -- retired at the mount; see RaceCarPhysics.cpp.)

    // LINK STUB (UpdateVehiclePhysics wave; the MEASURED last unresolved of the RaceCarPhysics.cpp
    // mount): the two-argument traction-point entry RaceCarPhysics::AddTractionPoint @0x825FFAE8
    // chains into (`bl` with (this, wheel, tag)). The VehiclePhysics.h:389 declaration models it
    // at VehiclePhysics level over the minimal slice; the real base body resolves the wheel's
    // contact position/normal and forwards to the 4-arg SimpleVehiclePhysics::AddTractionPoint
    // (DWARF BrnSimpleVehiclePhysics.h:205). Reconstruct with the traction/integrator wave.
    void VehiclePhysics::AddTractionPoint(s32, u32)
    {
        CGS_ASSERT(false, "VehiclePhysics::AddTractionPoint(s32,u32): link stub -- the base "
                          "traction-point entry; reconstruct with the integrator wave");
    }
}
}
