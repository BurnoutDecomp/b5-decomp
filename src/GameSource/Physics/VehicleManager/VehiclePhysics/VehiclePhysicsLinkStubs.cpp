// =================================================================================================
// VehiclePhysicsLinkStubs.cpp -- FLAG (TrafficPhysics de-fork link-mount stubs, 2026-08-03;
// re-measured 2026-08-07 by the orchestrator wave -- see the mid-file banner for the current set).
//
// Every stub is LOUD (a CGS_ASSERT(false) trap), dead until its caller's path goes live, and
// exists for one measured link-closure reason.
//
// ⭐⭐⭐ 2026-08-11 (driving-path wave): THIS FILE NOW DEFINES NOTHING. UpdateInAirBehaviour was the
// LAST surviving trap, and it is bodied in VehiclePhysics.cpp. What remains below is the wave-by-wave
// record of which trap fell to which wave -- kept deliberately, because that ledger is the only place
// several of those addresses/insn-counts are written down. The TU stays mounted so the record travels
// with the build and so a future stub lands here rather than being scattered; it costs one empty
// object file. ⚠️ CONDUCTOR: if you would rather retire the mount entirely, this is the moment --
// nothing links against it any more.
//
// ⭐ WHY THEY EXIST. `PhysicalTrafficManager::maFullTrafficPhysics[20]` was folded from a byte-pinned
// `u8[5168]` stand-in to the real `BrnPhysics::Vehicle::TrafficPhysics` (the ODR de-fork -- see
// BrnPhysicalTrafficManager.h). TrafficPhysics is polymorphic, VehicleManager embeds the traffic
// manager by value, PhysicsModule embeds VehicleManager, and PhysicsModule's constructor is mounted
// -- so the constructor chain seats twenty vptrs and the linker demands a DEFINITION for every slot
// of TrafficPhysics's vtable. Exactly one slot was missing (`TrafficPhysics::Update`, the only
// virtual the class introduces), and defining it dragged two VehiclePhysics callees.
//
// ⭐⭐ 2026-08-09 (CRASH/SHUNT WAVE): both of those stubs are GONE --
//     VehiclePhysics::UpdateShunt     @0x825FC748  BODIED in VehiclePhysics.cpp (100 insns,
//         signature conformed to the DWARF: (BrnPlayerDriverControls*, VecFloat) -- the old
//         1-arg const form was a slice artifact; both call sites re-pointed)
//     VehiclePhysics::UpdateCrashing  @0x82638810  BODIED in VehiclePhysics.cpp (732 insns,
//         the crash-state orchestrator; the vlogefp/vexptefp curve landed as std::pow over
//         image-read damping constants per the DampenAngularVelocity precedent)
// TrafficPhysics::Update itself was reconciled in the same wave (TrafficPhysics_Construct.cpp).
//
// ⚠️ THIS IS NOT "the de-fork needed stubs". The de-fork needed the VTABLE, and the vtable is the
// console's own behaviour: PhysicalTrafficManager's constructor @0x827E42E8 writes those same vptrs.
// The alternative was to leave the ODR fork standing, which is the silent-corruption trade the fold
// exists to retire -- a `TrafficPhysics::Construct` body written against the real class linking
// against a call site that strides the array by the console's 5168 while the host class is 4960.
//
// ⛔ NEVER ADD BEHAVIOUR HERE. Reconstruct the real body in VehiclePhysics.cpp and DELETE the stub.
// The failure mode if you forget is the good one: two definitions of the same symbol is a hard
// LNK2005, so you cannot body either function without being told about this file.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"  // VehicleAttribs (the SetupAttribs(handling) trap below)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
    // ⭐⭐ 2026-08-09 (crash/shunt wave): the UpdateShunt @0x825FC748 and UpdateCrashing
    // @0x82638810 traps this file was created for are GONE -- both BODIED in
    // VehiclePhysics.cpp (see the top banner).

    // =============================================================================================
    // ⭐⭐ 2026-08-07 (ORCHESTRATOR WAVE). The three stubs this file carried for the conductor
    // set -- Update @0x826412C0, UpdateSteering @0x825D3720, AddTractionPoint(s32,u32) -- are
    // GONE: Update and UpdateSteering are BODIED in VehiclePhysics.cpp, and the 2-arg
    // AddTractionPoint never existed on the console (the real 4-arg chain
    // RaceCarPhysics::AddTractionPoint -> SimpleVehiclePhysics::AddTractionPoint is bodied in
    // RaceCarPhysics.cpp / BrnSimpleVehiclePhysics.cpp).
    //
    // What follows is the MEASURED remainder of the driving spine's closure -- the leaves
    // UpdateDriving/Update now call that have no reconstructed body. Same contract as ever:
    // every stub is a LOUD trap, each names its console address and size, and bodying one
    // FAILS LNK2005 until its stub is deleted in the same commit.
    //
    // ⛔ NEVER ADD BEHAVIOUR HERE. Each of these is a force- or state-producer; a silent no-op
    // would be the invisible-forever handling bug.
    // =============================================================================================

    // ⭐ 2026-08-07 (WHEEL-CLUSTER WAVE): the UpdateWheels @0x8261E4F0 and
    // SimpleVehiclePhysics::CalculateNewWheelPlane @0x82602CB8 stubs are GONE -- both are
    // BODIED (VehiclePhysics.cpp / BrnSimpleVehiclePhysics.cpp), along with their four
    // exclusive helper callees UpdateBurnout / UpdateWheelInertia /
    // UpdateBrakesAndGetBrakingFactor / LimitDifferential (never stubbed here: nothing else
    // called them, so they carried no link pressure until this wave).

    // ⭐⭐ 2026-08-11 (DRIVING-PATH WAVE): the VehiclePhysics::UpdateInAirBehaviour @0x825D0BE8
    // trap that stood here is GONE -- the real 809-instruction body is BODIED in VehiclePhysics.cpp.
    // The census banner it carried was accurate about the STRUCTURE and about the closure being
    // clean (DampPitchYawRoll + AddWorldSpaceTorque, both bodied in ExternalPhysicsBody.cpp), and
    // its constant list checked out against a fresh image read, value for value. Two of its
    // *reasons* did not survive contact, and both are worth keeping:
    //   * "reciprocal-refined divisions ... not algebraically pinned" -- every `vrefp` in the
    //     function is followed by the compiler's stock TWO Newton-Raphson steps, i.e. it is a
    //     plain `1.0f / b`. Nothing was un-pinned.
    //   * "the dive path also drives the landing assist" -- it does not. The landing assist lives
    //     entirely in the ALREADY-AIRBORNE leg (@0x825D1240); the shared tail's two legs are a
    //     PITCH axis pair and a ROLL axis pair, not climb/dive-plus-assist.
    // The trap was live pressure, not theory: `UpdateDriving` calls this UNCONDITIONALLY
    // (VehiclePhysics.cpp:5181 area) and the console body early-returns on !mbHasAir, so the trap
    // fired once per driving car per frame the moment a car existed.


    // ⭐ 2026-08-09 (powertrain wave): the Engine::Update @0x825CB288 trap is GONE -- BODIED in
    // Engine.cpp. The 3937-line X360 debug Opt-vs-Unopt assert harness turned out to be ONE
    // algorithm run in two register files (branchy member leg + branchless vsel leg, cross-
    // asserted with tolerance 0.01); the body reproduces the branchless leg the epilogue commits,
    // cross-checked against the branchy leg and against the BPR x86 twin sub_BA63A0. All ~19
    // constants recovered from the X360 image (rdata floats + the 0x82C5Bxxx init-thunk bank);
    // provenance banner on the body.

    // ⭐ 2026-08-09 (attribs-setup wave): the SimpleVehiclePhysics::SwitchAttribs @0x82601978
    // stub is GONE -- BODIED in BrnSimpleVehiclePhysics.cpp. The blocker fell with it: the full
    // 240-byte SimpleVehicleAttribs now lives in BrnSimpleVehiclePhysics.h, and its Construct
    // @0x825E6580 + SetupAttribs @0x825BE0C8 are bodied in VehicleAttribs.cpp.

    // ⭐ 2026-08-09 (attribs-setup wave): the VehiclePhysics::SetAttributes @0x8262DE58 stub is
    // GONE -- BODIED in VehiclePhysics.cpp, together with the whole overload web it drags:
    // SimpleVehiclePhysics::SetAttributes() @0x82620498 (the former sub_82620498) and
    // SetAttributes(const Vector3*, const f32*) @0x826020A0 in BrnSimpleVehiclePhysics.cpp, and
    // SimpleVehicleAttribs::SetupAttribs(handling) @0x825E6778 in VehicleAttribs.cpp. ONE leg of
    // that web is still a trap -- the stub below.

    // ⭐ 2026-08-09 (attribs-data wave): the VehicleAttribs::SetupAttribs(handling) @0x825F4CD8
    // stub is GONE -- BODIED in VehicleAttribs.cpp (770 insns, the streamed-attribute loader;
    // every lane recovered by symbolic emulation of the raw image bytes, and the tire permute
    // table + the two per-car tire scatters landed REAL in Wheel.cpp in the same commit).

    // ⭐ 2026-08-09 (attribs-setup wave): the HackedResetAndFlyAround @0x825D0008 stub is
    // GONE -- BODIED in VehiclePhysics.cpp (139 insns, leaf, full transcription).

    // ⭐ 2026-08-09 (attribs-setup wave): the SetupAttribsForAI @0x825F58E0 stub is GONE --
    // BODIED in VehicleAttribs.cpp (622 insns, full store-for-store transcription).
}
}
