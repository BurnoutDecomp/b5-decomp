// =================================================================================================
// VehiclePhysicsLinkStubs.cpp -- FLAG (TrafficPhysics de-fork link-mount stubs, 2026-08-03;
// re-measured 2026-08-07 by the orchestrator wave -- see the mid-file banner for the current set).
//
// Every stub is LOUD (a CGS_ASSERT(false) trap), dead until its caller's path goes live, and
// exists for one measured link-closure reason.
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
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"  // VehicleAttribs (the SetupAttribs(handling) trap below)

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

    // LINK STUB (TrafficPhysics de-fork 2026-08-03; SIGNATURE conformed 2026-08-07 to the real
    // @0x82638810 register map -- f1=dt, r5=camera, r6=controls, r7/r8/r9 the impact/aftertouch/
    // showtime bools; see VehiclePhysics.h): body not reconstructed yet.
    // X360 @0x82638810, 732 instructions. The crash-damping spine: the per-axis vlogefp/vexptefp
    // angular-velocity curve driven by the rodata coefficient tables unk_82014AC0..82014AF0 that
    // TrafficPhysics::Update's own banner already records as un-homed. Its own wave.
    void VehiclePhysics::UpdateCrashing(f32, const rw::math::vpu::Matrix44Affine*,
                                        const BrnPlayerDriverControls*, bool, bool, bool)
    {
        CGS_ASSERT(false, "VehiclePhysics::UpdateCrashing: link stub (TrafficPhysics de-fork mount) "
                          "-- reconstruct from X360 @0x82638810");
    }

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

    // LINK STUB (orchestrator wave; FULL CENSUS added by the in-air + powertrain wave 2026-08-07):
    // X360 @0x825D0BE8, 809 instructions -- the in-air attitude controller. Closure is CLEAN (its
    // only callees, ExternalPhysicsBody::DampPitchYawRoll and AddWorldSpaceTorque, are bodied in
    // ExternalPhysicsBody.cpp) and every constant is recovered -- but it is 809 instructions of the
    // densest VMX128 in the cluster (reciprocal-refined divisions, vsel sign ladders, and a
    // ~240-instruction cross-product restoring-torque tail across three conditional paths), all
    // DORMANT until PhysicsModule::Update so unverifiable at runtime. Bodying it is its own wave with
    // dedicated verification, not a bolt-on; a rushed body risks a silent sign/lane error -- the
    // exact hollow-shell / silent-drop defect this cluster keeps producing. Decoded structure:
    //
    //   GATE: if (!mbHasAir @+0x1350) { mPitchYawRollFromTakeOff @+0x13C0 = 0; mbRollingInAir
    //         @+0x13D8 = 0; return; }
    //   rollRate = dot3(mAngularVelocity, zAxis);  yawRate = dot3(mAngularVelocity, yAxis).
    //   BRANCH on mbHadAirLastFrame @+0x1351:
    //     A) JUST TOOK OFF (==0): rollAmt = |xAxis.y|; rollDampFactor = piecewise(rollAmt) rising
    //        0->0.3 across [0, kfFullDampThreshold 0.125] then 0.3->1.0 across [0.125,
    //        kfNoDampThreshold 0.25], clamped 1.0 above (kfMinRollFactor 0.3). yawDamp =
    //        kYawDamp_Target 0.05 + (kYawDamp_Min 0.01 - 0.05) * rollDampFactor; mvDampRollVel
    //        @+0x1080 .x = yawDamp * kDamp_BlendRate 60.0 * dt. DampPitchYawRoll(pitch =
    //        attribs+0xC0 .x, yaw = 0, roll = 1 - rollDampFactor, dt) -- yaw 0 kills yaw on the
    //        takeoff frame. Then capture mPitchYawRollFromTakeOff @+0x13C0 (flt_82001D9C 2.0,
    //        flt_8208F5FC PI, attribs+0xC0).
    //     B) ALREADY AIRBORNE (!=0): if (|xAxis.y| > kfMinRollToAllowCorrection 0.3) mbRollingInAir
    //        = 1; when rolling, correct mAngularVelocity (kfLandingAssistDamping 0.14, flt_820037C8
    //        -1.0, flt_82092BC4 60.0).
    //   SHARED TAIL: fwdSpeed = dot3(linvel, zAxis); latSpeed = dot3(linvel, xAxis). Branch on
    //   (zAxis*fwdSpeed).y > 0 (climb vs dive); each path builds a cross-product restoring torque
    //   (cross(velInPlane, xAxis) scaled by mfMass @+0xE0, minus pitchRate*mass*10) and calls
    //   AddWorldSpaceTorque up to 3x (unk_82FB9310 0.0, unk_82FB9230 0.1, unk_82FB9330 10.0,
    //   unk_82FB8B60 1.0, unk_82FB9190 0.25); the dive path also drives the landing assist and
    //   mWheelFFSpring @+0x13D0. Cross-product idiom = `vpermwi128 v,v,0x63 ; vnmsubfp`.
    void VehiclePhysics::UpdateInAirBehaviour(const BrnPlayerDriverControls*, VecFloat)
    {
        CGS_ASSERT(false, "VehiclePhysics::UpdateInAirBehaviour: link stub -- reconstruct from "
                          "X360 @0x825D0BE8 (809 insns; clean closure + constants recovered, see "
                          "the full census banner above -- its own wave)");
    }

    // LINK STUB (in-air + powertrain wave, 2026-08-07): X360 @0x825CB288 -- Engine::Update, THE
    // POWERTRAIN TORQUE CORE (throttle -> clutch -> flywheel -> gearbox -> drive force). It is
    // trapped, not bodied, because it ships as a debug Opt-vs-Unopt ASSERT HARNESS on BOTH readable
    // consoles: X360 @0x825CB288 is 3937 asm lines whose only callees are CgsDev::Assert /
    // StrStream / AttribSysModule::GetVaultArray / BasePriorityQueue::Clear, and the PS3 DecFIGS
    // copy @0x712834 is 10324 lines with 1173 assert references. Reconstructing the real torque math
    // out of that harness is a whole wave. ApplyEngineForces (bodied this wave) calls it and
    // ApplyEngineForcesOntoWheels (also bodied) reads the mvEngineDrive lane it is meant to produce
    // -- so the engine-force APPLICATION layer is real while the torque MODEL stays deferred here.
    // The 9-arg signature is the PS3 mangled name laid against the X360 ApplyEngineForces register
    // map (see Engine.h). A silent no-op here would leave every driven wheel with a stale/zero drive
    // force that OntoWheels would then apply as plausible zeros -- exactly the invisible-forever bug
    // the trap exists to prevent.
    void Engine::Update(VecFloat, VecFloat, VecFloat, bool, VecFloat, VecFloat, bool, VecFloat,
                        VecFloat)
    {
        CGS_ASSERT(false, "Engine::Update: link stub (the powertrain torque core) -- reconstruct "
                          "from X360 @0x825CB288; ships as a debug opt-vs-unopt assert harness in "
                          "both console builds, so it is its own wave");
    }

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

    // LINK STUB (attribs-setup wave, 2026-08-09): X360 @0x825F4CD8, 770 instructions --
    // VehicleAttribs::SetupAttribs(physicsvehiclehandling), THE STREAMED-ATTRIBUTE LOADER.
    // FULL CENSUS: closure is CLEAN -- every callee exists in the tree (the eight generated
    // sub-record wrapper ctors are inline in their committed headers; InterpedParam3::
    // Construct/Prepare, EngineAttribs::InitializeFromAttribs, TireAttribs::PrepareFront/
    // RearTire are bodied; RefSpec::GetCollection + Instance dtor are the mounted AttribSys
    // SDK). What remains is 770 instructions of per-lane record->attrib scatter (the
    // base/steering/engine/drift/collision/boost/bodyroll/suspension blocks) whose source byte
    // offsets each need the same dataflow tracking the 622-insn SetupAttribsForAI got -- its
    // own wave, not a bolt-on. Called by VehiclePhysics::SetAttributes' re-stream leg (dormant
    // until the driving spine goes live) and by VehiclePhysics::Prepare's chain when that
    // lands. A silent no-op here would leave a reset car driving on STALE attribs -- plausible
    // and invisible -- hence the trap.
    void VehicleAttribs::SetupAttribs(const Attrib::Gen::physicsvehiclehandling&)
    {
        CGS_ASSERT(false, "VehicleAttribs::SetupAttribs(handling): link stub -- reconstruct "
                          "from X360 @0x825F4CD8 (770 insns, the streamed-attribute loader; "
                          "closure clean, its own wave)");
    }

    // ⭐ 2026-08-09 (attribs-setup wave): the HackedResetAndFlyAround @0x825D0008 stub is
    // GONE -- BODIED in VehiclePhysics.cpp (139 insns, leaf, full transcription).

    // ⭐ 2026-08-09 (attribs-setup wave): the SetupAttribsForAI @0x825F58E0 stub is GONE --
    // BODIED in VehicleAttribs.cpp (622 insns, full store-for-store transcription).
}
}
