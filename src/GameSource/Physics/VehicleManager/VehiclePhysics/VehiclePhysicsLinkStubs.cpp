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

    // =============================================================================================
    // ⭐⭐ THE TRAP THAT STOOD HERE IS GONE -- 2026-08-11 (the VehiclePhysics::Prepare wave).
    // ⭐ DELETED TWICE, INDEPENDENTLY -- which is the point. The trap was installed by the wave
    // that landed RaceCarPhysics::Prepare without its callee and used the resulting LNK2019 to
    // hold the link closure; two different waves then landed the real body on the same day and
    // both removed it. Keeping either copy would be an LNK2005 and would SHADOW a real function.
    // `VehiclePhysics::Prepare` @0x82637C80 is BODIED in VehiclePhysics.cpp, read off the X360 asm
    // and cross-read against the PS3 build of the same source (0x735DEC) store for store; the
    // 3-arg `VehiclePhysics::SetAttributes` @0x8262E140 that used to be `sub_8262E140` landed with
    // it, and so did the declared-but-undefined `SimpleVehiclePhysics::SetAttributes(VehicleAttribs*,
    // const Vector3*, const f32*)` it calls. The census below is preserved because it is the record
    // of how the blocker was found -- the LNK2019 in it is what NAMED the function.
    //
    // ⚠️⚠️ CORRECTED AT THE MERGE. The note that landed with this body said "nothing calls it at
    // runtime yet: the only two callers are ProcessCreateEvents (a LOUD gate) and
    // TrafficPhysics::PreparePhysical (an UNMOUNTED TU)". That was true in the wave that wrote it
    // and is NOT true in this tree: VehicleManager::ProcessCreateEvents @0x82616770 is BODIED and
    // MOUNTED (BrnVehicleManager_ProcessCreateEvents.cpp, in build_game_exe.bat), and it makes the
    // vtable-slot-+0x30 call into RaceCarPhysics::Prepare, which forwards straight into this. ⇒
    // THIS BODY IS LIVE ON THE CREATE PATH. TrafficPhysics::PreparePhysical @0x82639380 remains the
    // one caller in an unmounted TU. Anything that lands here now runs on a real boot.
    //
    // ---- the census as it stood, for the record -------------------------------------------------
    //
    // RaceCarPhysics::Prepare @0x82639CB8 LANDED this wave (RaceCarPhysics.cpp, recovered from the
    // PS3 export 0x73648C at the Δ=+16 member mapping this class's own table already establishes).
    // Its FIRST statement forwards into VehiclePhysics::Prepare @0x82637C80, and the link
    // immediately said what a hundred greps had not: nothing in this tree defines it.
    //     RaceCarPhysics.obj : error LNK2019: unresolved external symbol
    //       "public: bool VehiclePhysics::Prepare(Matrix44Affine, Vector3, Vector3, Vector3,
    //        Vector3, AxisAlignedBox const&, VehicleAttribs*, Vector3 const*, float const*)"
    // That LNK2019 is the point: last wave corrected this declaration from a five-parameter FORK to
    // the DWARF's nine, and the fork's whole danger was that no per-TU gate could see it. The
    // moment a real caller existed, the linker named it. Trap installed so the closure STAYS
    // enforced while the body is recovered.
    //
    // ⭐ IT IS UNREACHABLE TODAY, and that is measured, not hoped: the only two callers are
    // VehicleManager::ProcessCreateEvents @0x82616770 (a LOUD gate AT THE TIME; bodied and
    // mounted since) and TrafficPhysics::PreparePhysical @0x82639380
    // (in an UNMOUNTED TU). `grep -c` for either in the mounted set: zero live call sites.
    //
    // ⛔⛔ THE 306 INSTRUCTIONS ARE THE WHOLE REASON THE CAR IS NOT IN THE WORLD YET. This is the
    // ONLY thing that seats mTransform / mHalfExtent / mSimpleAttribs on a race car, so every
    // observable the campaign is chasing -- the cache sphere leaving the origin, the slot filling
    // with batches, mbIsOnGround -- hangs off it. Read from @0x82637C80:
    //     VehicleAttribs::operator=            @0x82637CF4   (the incoming attribs, by value)
    //     mpAttribs = <the copy>               @0x82637D18   (`stw r29, 0x720(r31)`)
    //     SimpleVehiclePhysics::Prepare        @0x82637D24   ⭐ BODIED (2026-08-11, 554 insns)
    //     sub_8262E140                         @0x82637DC8   (the AttribSys handling re-stream)
    //     VehicleAttribs::Construct            @0x82637F90   ⭐ BODIED
    //     VehicleAttribs::SetupAttribsForAI    @0x82637F9C   ⭐ BODIED
    //     VehiclePhysics::Reset                @0x82637FB0   ⭐ BODIED
    //   then ~40 own-block seeds, every one at an offset this class's header ALREADY NAMES:
    //     +0x1114/+0x1118/+0x111C/+0x1120 = 0.0f · +0x1128 = <the bool arg> · +0x1158/+0x1220 = 0
    //     +0x710/+0x712/+0x1359/+0x10F7/+0x135A/+0x135D/+0x135E/+0x1362 = 0 · +0x135C = <arg>
    //     +0x10D4 = 0 · +0x13DC = <arg> · plus a dozen 16-byte VMX stores.
    // ⇒ SEVEN of its eight callees are already bodied and mounted. What is left is transcription
    // plus naming the VMX lanes -- big, but NOT blocked on anything. That is the next wave.
    //
    // ⭐ AND THAT IS WHAT HAPPENED: the eighth callee (`sub_8262E140`) turned out to be
    // `VehiclePhysics::SetAttributes`, the "~40 own-block seeds" are all named members of this
    // class, and every lane was confirmed twice (X360 `vrlimi128` masks 8/4/2/1 vs PS3
    // `VectorPermuteConstant<4,1,2,3>/<0,5,2,3>/<0,1,6,3>/<0,1,2,7>`). The body is in
    // VehiclePhysics.cpp; the trap is deleted.
    // =============================================================================================

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
