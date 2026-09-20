// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_ContactBridges.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the contact-BRIDGE slice consumed by
// PhysicsModule::BridgeContactsToSimulation @0x825A99E8.
// Slice TU: home BrnDeformationManager.cpp / the _Contacts group is still unmounted (its
// SolvePenetration / UpdateTriangleCache tail carries ~19 unresolved of its own). Fold back
// when the home mounts.
//
// CONTENTS:
//   * ReadPotentialVehicleWorldContact @0x82604590 -- MOVED VERBATIM from
//     BrnDeformationManager_Contacts.cpp (this TU is mounted; that one is not). One change,
//     flagged inline: the "Un-normalised" tripwire's old always-pass placeholder was upgraded
//     to a real predicate at the move (2026-08-06), and that predicate was corrected to the
//     console's |Magnitude(n) - 1| > 0.01 form on 2026-08-27 -- see IsNormalisedWithinEpsilon.
//   * FindModelIndexByGlobalEntityID @0x825B45B0 -- MOVED VERBATIM (same reason; callee of
//     the above).
//   * ReadPotentialContact @0x826053F8 -- reconstructed from the
//     BURNOUT_X360_ARTIST.XEX asm with the PS3 DecFIGS out-of-line body @0x6F048C as the
//     structural oracle (debug names lPotentialContact/lContactId/lpSimInput,
//     liModelIndexA/B from its baked asserts).
//   * BridgeBodyPartCarContactsToSimulation @0x825DD7D0 + BridgeDetachedWheelCar-
//     ContactsToSimulation -- REAL (landed 2026-09-20; 348/392 console lines each). They
//     drain the two DETACHED part/wheel-vs-car potential-contact queues into the
//     simulation's add-contact queue.
//   * AddRaceCarBodyPartPair @0x82605928 + AddHingedBodyPartPairs @0x82605A98 -- REAL.
//     AddRaceCarWheelPair @0x82605BE8 is a NAMED GATE (one missing cylinder-vs-box appender).
//
// This TU is LIVE: PhysicsModule::Update @0x825B0640 is a real body and the three pair feeders
// run every frame from VehicleManager::StartVehicleContactGeneration.
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the boot gates)

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // PotentialContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (+ DeformationSensor)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (GetTransform + the timer bank)
#include "rw/math/vpu/vector3_operation.h"                                                // Normalize / IsValid
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder::AddPrimitivePair (+ CgsGeometric::Box)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"     // IKBodyPart::GetPartPoolIndex (the hinged-panel walk)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"  // PhysicalWheel::GetVolumeInstanceId (the detached-wheel bridge)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"            // PotentialContactInterface::GetDetached*Queue (the two drained queues)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"                // InputBuffer::GetAddContactQueue
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"               // InAddPotentialContact (the queued record)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                            // EntityId::Set (the proxy-body id pack)

namespace BrnPhysics
{
// [T5-def] DIAG counters, DEFINED in
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp.
// NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
namespace Vehicle
{
    extern u32 gT5Q8Routed;
    extern u32 gT5Q8Accepted;
}

namespace Deformation
{
    namespace
    {
        // Owner byte of a packed VolumeInstanceId (bits [56..63]; see the _Contacts TU banner).
        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }

        // Race-car / total-traffic index bounds (FindModelIndexByGlobalEntityID asserts).
        const u32 KU_OWNER_RACECAR         = 1;      // BrnWorld::E_ENTITYTYPE_RACECAR
        const u32 KU_OWNER_TRAFFIC_VEHICLE = 2;      // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        const u32 KU_MAX_NUM_RACE_CARS     = 8;      // Vehicle::ku8MaxNumRaceCars
        const u32 KU_MAX_TOTAL_TRAFFIC     = 0x258;  // BrnTraffic::KU_MAX_TOTAL_TRAFFIC (600)

        // The two contact-BRIDGE owner sets and pool bounds (the bodies' own assert strings).
        const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;   // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
        const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;   // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART
        const u32 KU_OWNER_DETACHED_RACECAR_WHEEL  = 9;   // BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL
        const u32 KU_OWNER_DETACHED_TRAFFIC_WHEEL  = 10;  // BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL

        // The PROXY ("dummy car") owners the two bridges retarget the car's handling body onto
        // before handing the contact to the simulation. Same retarget PropManager::
        // RoutePropVsRaceCarContactToDummyCar performs for a prop-vs-car contact.
        const u32 KU_OWNER_PROP_COLLISION_RACECAR  = 11;  // BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR
        const u32 KU_OWNER_PROP_COLLISION_TRAFFIC  = 12;  // BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC

        const u32 KU_MAX_DETACHED_PARTS      = 0x32;   // 50   (`cmplwi r31, 0x32`)
        const u32 KU_MAX_DETACHED_WHEELS     = 0x70;   // 112  (`cmplwi r31, 0x70`)
        // The model-pool bound (0x1C == 28) is the namespace-scope
        // BrnPhysics::Deformation::KU_MAX_DEFORMATION_MODELS homed by BrnDeformationState.h --
        // not re-declared here (a file-local copy makes every use ambiguous).

        // Material constants the two bridges stamp into every record they post.
        //   part-vs-car : 0.5 / 0.5 / 0.5        (one float, flt_82001DA0, stored three times)
        //   wheel-vs-car: 0.9 / 0.5 / 0.8        (flt_82005450 / flt_82001DA0 / flt_8208F9C8)
        const f32 KF_PART_CAR_FRICTION            = 0.5f;
        const f32 KF_PART_CAR_RESTITUTION         = 0.5f;
        const f32 KF_WHEEL_CAR_STATIC_FRICTION    = 0.89999998f;
        const f32 KF_WHEEL_CAR_DYNAMIC_FRICTION   = 0.5f;
        const f32 KF_WHEEL_CAR_RESTITUTION        = 0.80000001f;

        // The detached-wheel bridge's maximum tolerated penetration before it rebuilds the
        // contact off the car's own body axis. A SILENT-ZERO splat in the image (a broadcast
        // vector filled by a start-up thunk from the 0.0099999998f literal, not a zero), so it
        // is spelled here as the value the thunk writes.
        const f32 KF_MAX_WHEEL_PENETRATION        = 0.0099999998f;

        // Pair-builder feeder constants, read off the three bodies' asm.
        const f32 KF_PART_VS_CAR_CONTACT_PADDING  = 0.5f;   // flt_82001DA0 @0x82605A88
        const f32 KF_HINGED_PART_CONTACT_PADDING  = 1.0f;   // flt_82001C98 @0x82605B20
        const s32 KI_MAX_PARTS_PER_MODEL          = 50;     // `cmpwi r26, 0x32` (maPartStates[50])

        // Per-lane NaN self-compare (the asm's vspltw + vcmpeqfp. over lanes x/y/z) -- the
        // vendor vpu tree's own IsValid(Vector3).
        inline bool IsValidVec3Lanes(const Vector3& lrV) { return rw::math::vpu::IsValid(lrV); }

        // Both contact bridges resolve the car side of the event the same way: take the
        // contacted model's handling-body word (the 8-byte handle at model +0x6710), keep its
        // 14-bit entity index, and re-stamp the owner as the car's PROP-COLLISION PROXY body --
        // racecar -> 11, traffic -> 12 -- with a zero part index. The finished 32-bit word is
        // then widened into the event's 8-byte B-side id as the HIGH dword, leaving the low
        // dword zero. The console inlines this in both bodies; written once here so the two
        // cannot drift.
        //
        // ⚠️ THE RETARGET IS THE POINT, not a detail: the record the simulation receives does
        // NOT name the car's handling body, it names the car's prop-collision proxy ("dummy
        // car") body -- the same proxy a prop-vs-car contact is routed onto.
        inline u64 BuildProxyCarBodyId(u64 lu64HandlingBodyId)
        {
            const u32 luCarEntityWord  = static_cast<u32>(lu64HandlingBodyId >> 32);
            const u32 luCarOwner       = (luCarEntityWord >> 24) & 0xFFu;
            const u32 luCarEntityIndex = (luCarEntityWord >> 10) & 0x3FFFu;

            u32 luProxyOwner = KU_OWNER_PROP_COLLISION_RACECAR;
            if (luCarOwner != KU_OWNER_RACECAR)
            {
                // Fire-and-continue, exactly as the console: the traffic arm is taken either
                // way (the tripwire does not gate the branch it sits in).
                CGS_ASSERT(luCarOwner == KU_OWNER_TRAFFIC_VEHICLE,
                           "lAddContactEvent.mIDB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
                luProxyOwner = KU_OWNER_PROP_COLLISION_TRAFFIC;
            }

            // EntityId::Set carries the console's own entity-index tripwire
            // ("luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"), which is why the pack is
            // spelled through it rather than as loose shifts.
            CgsSceneManager::EntityId lProxyEntityId;
            lProxyEntityId.Set(luProxyOwner, luCarEntityIndex, 0);
            return static_cast<u64>(static_cast<u32>(lProxyEntityId)) << 32;
        }

        // The consoles' "Un-normalised ..." tripwire predicate.
        // ⭐⭐ CORRECTED 2026-08-27 (traffic-bus ram wave): this used to RENORMALISE the vector and
        // compare the per-lane |renormalised - original| against the epsilon. That is not the
        // console's predicate -- it is looser by up to sqrt(3) (the lane delta of a vector of
        // length 1+e is e*n_lane, never more than e) and it is a different computation. The X360
        // body spells the SCALAR magnitude and subtracts ONE, exactly as the assert text says:
        //   0x826047E4  vmsum3fp128 v13, v12(NORMAL), v12    ; dot = |n|^2
        //   0x8260480C  vrsqrtefp   v12, v13                 ; 1/sqrt(dot) + two Newton refines
        //   0x8260483C  vmulfp128   v0,  v13, v0             ; dot * 1/sqrt(dot) == |n|
        //   0x82604840  vsel        v0,  v0,  v7(zero), v3   ; dot == 0 -> magnitude 0
        //   0x82604844  vsubfp      v0,  v0,  v11(1.0)       ; |n| - 1   (v11 = vcfsx(1,0))
        //   0x82604848  vandc       v0,  v0,  v9             ; abs (0x80000000 splat)
        //   0x8260484C  vcmpgtfp    v0,  v0,  v8(0.0099999998)
        // which is the SAME predicate the bridge's own tripwires (BrnPhysicsModuleBridgeFunctions
        // .cpp IsUnitLength) and DeformationSensor::ValidateAndAddContact @0x825E1864 run. One
        // console idiom, one spelling. rw::math::vpu::Magnitude carries the zero-dot guard that
        // reproduces the vsel, so a zero normal still fails.
        inline bool IsNormalisedWithinEpsilon(const Vector3& lrV)
        {
            const f32 lfDelta = rw::math::vpu::Magnitude(lrV) - 1.0f;
            return (lfDelta < 0.0f ? -lfDelta : lfDelta) <= 0.0099999998f;
        }

        // The orthonormal-affine inverse both consoles inline in ReadPotentialContact
        // (@0x826053F8: the vmrglw/vmrghw 3x3 word-interleave transpose of the model's rigid-body
        // transform rows + the `vsubfp 0,T` negated translation folded through the transposed
        // columns via three vspltw/vmaddfp steps): rotation transposed, translation -(T . R).
        inline void BuildWorldToModel(Matrix44Affine& lrOut, const Matrix44Affine& lrTransform)
        {
            // Rotation transposed (the vmrghw/vmrglw word-interleave cascade).
            lrOut.xAxis.x = lrTransform.xAxis.x; lrOut.xAxis.y = lrTransform.yAxis.x;
            lrOut.xAxis.z = lrTransform.zAxis.x; lrOut.xAxis.w = 0.0f;
            lrOut.yAxis.x = lrTransform.xAxis.y; lrOut.yAxis.y = lrTransform.yAxis.y;
            lrOut.yAxis.z = lrTransform.zAxis.y; lrOut.yAxis.w = 0.0f;
            lrOut.zAxis.x = lrTransform.xAxis.z; lrOut.zAxis.y = lrTransform.yAxis.z;
            lrOut.zAxis.z = lrTransform.zAxis.z; lrOut.zAxis.w = 0.0f;

            // Translation: -(T . R) folded through the transposed rows (the three
            // vspltw(-T, lane) / vmaddfp steps).
            const Vector3& lrT = lrTransform.wAxis;
            lrOut.wAxis.x = -(lrT.x * lrOut.xAxis.x + lrT.y * lrOut.yAxis.x + lrT.z * lrOut.zAxis.x);
            lrOut.wAxis.y = -(lrT.x * lrOut.xAxis.y + lrT.y * lrOut.yAxis.y + lrT.z * lrOut.zAxis.y);
            lrOut.wAxis.z = -(lrT.x * lrOut.xAxis.z + lrT.y * lrOut.yAxis.z + lrT.z * lrOut.zAxis.z);
            lrOut.wAxis.w = -(lrT.x * lrOut.xAxis.w + lrT.y * lrOut.yAxis.w + lrT.z * lrOut.zAxis.w);
        }
    }

    // ==========================================================================================
    // ReadPotentialVehicleWorldContact @ 0x82604590   (MOVED from BrnDeformationManager_Contacts
    // .cpp -- see the TU banner. Body verbatim except the flagged tripwire upgrade.)
    //
    // Route one potential VEHICLE-vs-WORLD contact into the owning car's deformation sensor. The
    // asm: validity-gate the contact (assert + ignore if any lane is NaN); look up the car's model
    // slot from muVolumeInstanceIdA's entity; assert the owner tags (A == racecar/traffic, B ==
    // world); fetch the model's deformation sensor for that volume instance and the model's world
    // transform; assert the normal is normalised; then DeformationSensor::ValidateAndAddContact
    // with no other vehicle / other sensor.
    // ==========================================================================================
    void DeformationManager::ReadPotentialVehicleWorldContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
        BrnPhysics::ContactId lContactId,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/)
    {
        // Initial validity gate: the asm self-compares the three lanes of the contact's leading
        // vector (mPointOnA) -- any NaN lane means the contact is invalid and is ignored (asserted,
        // then the function returns without adding it).
        if (!IsValidVec3Lanes(lrPotentialContact.mPointOnA))
        {
            CGS_ASSERT(false, "Invalid contact added to deformation. Ignoring...\n");
            return;
        }

        // The model lookup keys off muVolumeInstanceIdA's embedded entity word (the high dword of the
        // packed 64-bit id) -- the asm passes that word to FindModelIndexByGlobalEntityID.
        const EntityId lEntityA{ static_cast<u32>(lrPotentialContact.muVolumeInstanceIdA.muId >> 32) };
        const s32 liModelIndexA = FindModelIndexByGlobalEntityID(lEntityA);
        if (liModelIndexA == -1)
        {
            CGS_ASSERT(false, "liModelIndexA != -1");
            return;
        }

        // Owner-tag tripwires (non-gating): A must be a racecar / traffic vehicle, B must be world.
        const u32 luOwnerA = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdA);
        CGS_ASSERT(luOwnerA == KU_OWNER_RACECAR || luOwnerA == KU_OWNER_TRAFFIC_VEHICLE,
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        CGS_ASSERT(GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdB) == 0u,
                   "lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");

        DeformableObject& lrModel = mpaModels[liModelIndexA];

        // The car's deformation sensor for the contacted volume instance, and the car's world
        // transform (the asm reads the rigid-body transform off the model @ +6476 and packs it into
        // the Matrix44Affine handed to ValidateAndAddContact).
        DeformationSensor& lrSensor =
            lrModel.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdA);

        // ⭐⭐ FIXED 2026-08-14 (walls leg 4, at-rest probe): the console hands ValidateAndAddContact
        // the INVERSE (world -> model) transform -- the PS3 body's own argument is NAMED
        // lInverseVehicleTransform, and the car-car route below already builds exactly this
        // inverse. The old forward-transform pass made the stored local contact points garbage
        // (latent while nothing consumed them; the solver's at-rest pop measured it).
        Matrix44Affine lTransform;
        lrModel.GetTransform(lTransform);
        Matrix44Affine lWorldTransform;
        BuildWorldToModel(lWorldTransform, lTransform);

        // Normalised-normal tripwire (non-gating). ⭐ UPGRADED AT MOVE 2026-08-06 (the old body
        // carried an honest always-pass placeholder), ⭐⭐ CORRECTED 2026-08-27 to the console's
        // own scalar |Magnitude(n) - 1| > 0.01 predicate @0x826047E4..0x8260484C -- see
        // IsNormalisedWithinEpsilon. This site is the SAFE one either way: queues [6]/[9]
        // renormalise their copy before calling here (BrnPhysicsModuleBridgeFunctions.cpp :821 /
        // :846), so the normal is unit by construction. The streamed "Car transform: " message
        // tail is still lowered to the static prefix per the standing project rule.
        CGS_ASSERT(IsNormalisedWithinEpsilon(lrPotentialContact.mNormal),
                   "Un-normalised vehicle-world contact: ");   // BrnDeformationManager.cpp:1198

        // Validate + store the contact in the sensor (no other vehicle / other sensor for a
        // vehicle-vs-world contact).
        // FLAG (cross-TU type-namespace mismatch to reconcile at consolidation):
        // DeformationSensor::ValidateAndAddContact declares its contact arg as the stale forward-decl
        // CgsSceneManager::PotentialContact, whereas the canonical (DWARF) type is
        // CgsSceneManager::SceneManagerIO::PotentialContact (homed by CgsPotentialContact.h).
        // They are the SAME on-disk record (only the namespace differs), so the canonical record is
        // passed through a layout-safe reference cast here; the sensor header should retype its arg to
        // the SceneManagerIO:: home at consolidation, after which this cast can be dropped.
        lrSensor.ValidateAndAddContact(
            lWorldTransform,
            reinterpret_cast<const CgsSceneManager::PotentialContact&>(lrPotentialContact),
            lContactId, nullptr, nullptr);
    }

    // ==========================================================================================
    // FindModelIndexByGlobalEntityID @ 0x825B45B0   (MOVED VERBATIM from
    // BrnDeformationManager_Contacts.cpp -- see the TU banner.)
    // ==========================================================================================
    s32 DeformationManager::FindModelIndexByGlobalEntityID(EntityId lGlobalEntityId)
    {
        // Owner = high byte of the entity word; index = the 14-bit field at bit 10 (the
        // VolumeInstanceId entity-word geometry the asm decodes: `(id >> 10) & 0x3FFF`).
        const u32 luOwner = (lGlobalEntityId.muValue >> 24) & 0xFFu;

        CGS_ASSERT(luOwner == KU_OWNER_RACECAR || luOwner == KU_OWNER_TRAFFIC_VEHICLE,
                   "lID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

        if (luOwner == KU_OWNER_RACECAR)
        {
            const u32 lu16RaceCarIndex = (lGlobalEntityId.muValue >> 10) & 0x3FFFu;
            CGS_ASSERT(lu16RaceCarIndex < KU_MAX_NUM_RACE_CARS, "lu16RaceCarIndex < Vehicle::ku8MaxNumRaceCars");
            return ma8RaceCarToModelIndex[lu16RaceCarIndex];
        }

        const u32 lu16TrafficGlobalIndex = (lGlobalEntityId.muValue >> 10) & 0x3FFFu;
        CGS_ASSERT(lu16TrafficGlobalIndex < KU_MAX_TOTAL_TRAFFIC,
                   "lu16TrafficGlobalIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
        return ma8GlobalTrafficToModelIndex[lu16TrafficGlobalIndex];
    }

    // ==========================================================================================
    // ReadPotentialContact @ 0x826053F8   (NEW this wave; PS3 DecFIGS 0x6F048C)
    //
    // Route one potential VEHICLE-vs-VEHICLE contact into car A's deformation sensor (with car B
    // supplied as the "other" vehicle/sensor pair), then -- if the sensor accepted it -- clear
    // BOTH cars' contact cool-down lane. The asm:
    //   * owner tripwire :1872 (both owners racecar / traffic; fire-and-continue);
    //   * model lookups through FindModelIndexByEntityID (the PHYSICAL-id tables -- the PS3
    //     inlines its racecar/traffic table reads with bounds asserts h:701/h:710);
    //   * :1885/:1886 -1 tripwires (fire-and-continue -- the console indexes with -1 anyway;
    //     the host gates the array reads instead, diagnostics unchanged);
    //   * the inlined world->model-A affine inverse (see BuildWorldToModel above) handed to
    //     DeformationSensor::ValidateAndAddContact as its transform argument;
    //   * sensors resolved through GetDeformationSensorFromVolumeInstance (out-of-line on the
    //     X360 @0x825B2xxx; the PS3 inlines its spec-count/wheel-map remap, h:1041/:1050/:1051);
    //   * on acceptance: `vrlimi128 v0, v127(zero), 4, 0` at BOTH cars' vehicle-physics
    //     +4192 (X360) / the vperm<0,5,2,3> zero-merge at +4176 (PS3) == zero the Y lane of
    //     mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction -- the
    //     "TimeSinceLastRaceCarContact cool-down" reset the declaration's own banner names.
    // ==========================================================================================
    // ⛔ THE UNUSED lpSimInput IS FAITHFUL -- DO NOT "FIX" IT (verified 2026-08-29).
    // A 2026-08-29 lead read the commented-out parameter as a dropped side effect and named it
    // the cause of the empty traffic contact queue. It is not. On the X360 this is a THREE-
    // argument function: the prologue saves r3/r4/r5 only (`mr r30,r4` @0x8260540C, `mr r28,r3`
    // @0x82605414, `mr r25,r5` @0x82605418), and across all 135 instructions r6 is never read --
    // the single later use of the saved r5 is `mr r6,r25` @0x826055B0, the ContactId going into
    // ValidateAndAddContact. The declaration keeps the parameter because the DWARF and every
    // sibling Read*/Bridge* in this family carry it; the body has nothing to do with it.
    // ⇒ The race-car-vs-traffic loss is NOT here. See the retraction in
    // BrnPhysicsModuleBridgeFunctions.cpp's ProcessContactSpy banner for what it actually is.
    void DeformationManager::ReadPotentialContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
        BrnPhysics::ContactId lContactId,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/)
    {
        const u32 luOwnerA = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdA);
        const u32 luOwnerB = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdB);

        CGS_ASSERT((luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE)
                       && (luOwnerA == KU_OWNER_RACECAR || luOwnerA == KU_OWNER_TRAFFIC_VEHICLE),
                   "( lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE) && "
                   "( lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");  // :1872

        // PHYSICAL-id model lookups (both entity words are already physics-local here -- the
        // bridge rewrote them upstream).
        const s32 liModelIndexA = FindModelIndexByEntityID(
            EntityId{ static_cast<u32>(lrPotentialContact.muVolumeInstanceIdA.muId >> 32) });
        const s32 liModelIndexB = FindModelIndexByEntityID(
            EntityId{ static_cast<u32>(lrPotentialContact.muVolumeInstanceIdB.muId >> 32) });

        CGS_ASSERT(liModelIndexA != -1, "liModelIndexA != -1");   // :1885
        CGS_ASSERT(liModelIndexB != -1, "liModelIndexB != -1");   // :1886
        if (liModelIndexA == -1 || liModelIndexB == -1)
        {
            // Host bounds guard only -- the console's asserts above are fire-and-continue and it
            // indexes the model pool with the -1 anyway; nothing live changes (diagnostics TU).
            return;
        }

        DeformableObject& lrModelA = mpaModels[liModelIndexA];
        DeformableObject& lrModelB = mpaModels[liModelIndexB];

        // world -> model-A space (the inlined orthonormal-affine inverse of A's rigid-body
        // transform; see BuildWorldToModel's asm citation).
        Matrix44Affine lWorldToModelA;
        BuildWorldToModel(lWorldToModelA, lrModelA.GetVehiclePhysics()->GetTransform());

        // Sensor resolution: B first, then A (the console's call order).
        DeformationSensor& lrSensorB =
            lrModelB.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdB);
        DeformationSensor& lrSensorA =
            lrModelA.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdA);

        // Same cross-TU namespace cast FLAG as ReadPotentialVehicleWorldContact above.
        // [T5-def] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Count the race-car-vs-traffic
        // (queue [8]) contacts that actually reach a sensor, and how many the sensor keeps -- the
        // two numbers that separate "no contact was ever routed" from "the sensor rejected it".
        const bool lbT5Q8 = ((static_cast<u32>(lContactId) >> 24) == 0x08u);
        if (lbT5Q8)
        {
            ++BrnPhysics::Vehicle::gT5Q8Routed;
        }

        if (lrSensorA.ValidateAndAddContact(
                lWorldToModelA,
                reinterpret_cast<const CgsSceneManager::PotentialContact&>(lrPotentialContact),
                lContactId, &lrModelB, &lrSensorB))
        {
            if (lbT5Q8)
            {
                ++BrnPhysics::Vehicle::gT5Q8Accepted;
            }
            // Accepted: zero BOTH cars' contact cool-down (the Y lane of the packed timer bank).
            lrModelA.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;
            lrModelB.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;
        }
    }

    // =================================================================================================
    // DeformationManager::BridgeBodyPartCarContactsToSimulation
    //
    // Drain custom potential-contact queue [3] -- DETACHED body part vs car -- into the simulation's
    // add-contact queue. Live every frame from PhysicsModule::BridgeContactsToSimulation, which is
    // itself reached every frame from PhysicsModule::Update.
    //
    // Per queued contact, in the console's order:
    //   * an 80-byte stack copy of the record (the console's ten-doubleword copy loop);
    //   * the three material constants (0.5 / 0.5 / 0.5, all from one float);
    //   * owner tripwires -- A is a racecar/traffic DEFORMABLE PART (:2366), B is a racecar/
    //     traffic vehicle (:2369) -- and the part-slot bound muPolyTagA < 50 (:2373);
    //   * the pool gate: an unused part slot SKIPS the contact entirely (the whole tail sits
    //     inside `if (IsPartIndexUsed)`);
    //   * the event's A-side id <- the part's own packed 64-bit handle, read whole from +0x1D0;
    //   * mIDB <- the contacted car model's PROXY body id (see BuildProxyCarBodyId), reached
    //     through muPolyTagB with the model-slot bound (:2381), the bit-array index tripwire
    //     and the mModelsAdded live-slot tripwire (:2382);
    //   * the geometry: normal NEGATED into the event (the console's sign-bit splat over all
    //     four lanes), and a CROSSED (penetrating) pair collapsed -- if dot3(mPointOnB - mPointOnA,
    //     mNormal) is negative the event's mPointOnB is replaced by mPointOnA (threshold 0.0f);
    //   * muTag <- the event index tagged with this queue's owner byte, 0x03000000, behind the
    //     ContactId range tripwire (BrnContactId.h:122);
    //   * the queue-headroom tripwire (:2412) then the bounds-gated AddEventSafe.
    // =================================================================================================
    void DeformationManager::BridgeBodyPartCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        // ⛔ THE UNUSED lpInputBuffer IS FAITHFUL -- DO NOT "FIX" IT. The prologue spills only
        // `this`, the sim input and the contact interface; the module input buffer's register is
        // never read again across the whole body. The declaration keeps the parameter because the
        // recovered signature and every sibling Bridge* in this family carry it.
        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        const Queue& lrQueue = lpContacts->GetDetachedBodyPartCarQueue();

        // The loop bound is a SNAPSHOT: the console reads the length once into a stack slot
        // before the loop and compares against that copy at the bottom.
        const s32 liQueueLength = lrQueue.GetLength();

        for (s32 liEventIndex = 0; liEventIndex < liQueueLength; ++liEventIndex)
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact lContact =
                lrQueue.GetEvent(liEventIndex);   // 80-byte stack copy

            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lAddContactEvent;
            lAddContactEvent.mStaticFriction  = KF_PART_CAR_FRICTION;
            lAddContactEvent.mDynamicFriction = KF_PART_CAR_FRICTION;
            lAddContactEvent.mRestitution     = KF_PART_CAR_RESTITUTION;

            const u32 luOwnerA = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdA);
            CGS_ASSERT(luOwnerA == KU_OWNER_RACECAR_DEFORMABLE_PART
                           || luOwnerA == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");   // :2366

            const u32 luOwnerB = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdB);
            CGS_ASSERT(luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");           // :2369

            const u32 luPartSlot = lContact.muPolyTagA;
            CGS_ASSERT(luPartSlot < KU_MAX_DETACHED_PARTS,
                       "lContact.muPolyTagA < KU_MAX_DETACHED_PARTS");                                                         // :2373

            // A dead pool slot drops the contact -- the console's `beq` jumps straight to the
            // loop increment.
            if (!mDetachedPartManager.IsPartIndexUsed(static_cast<s32>(luPartSlot)))
            {
                continue;
            }

            // The part's own packed handle, read whole (the console's single `ld 0x1D0(part)`).
            const PhysicalBodyPart* lpPart =
                mDetachedPartManager.GetPartFromIndex(static_cast<u16>(luPartSlot));
            lAddContactEvent.mIDA = lpPart->GetRigidBodyId().GetBaseRigidBodyID();

            // The contacted car's deformable-model slot.
            const u32 luModelIndex = lContact.muPolyTagB;
            CGS_ASSERT(luModelIndex < KU_MAX_DEFORMATION_MODELS,
                       "lContact.muPolyTagB < BrnPhysics::Deformation::KU_MAX_DEFORMATION_MODELS");                            // :2381
            CGS_ASSERT(luModelIndex < KU_MAX_DEFORMATION_MODELS, "invalid index : ");        // CgsBitArray.h:203
            if (luModelIndex >= KU_MAX_DEFORMATION_MODELS)
            {
                // Host bounds guard only. The console's tripwires above are fire-and-continue and
                // it indexes mModelsAdded / mpaModels with the out-of-range tag anyway, then posts
                // a contact built from that out-of-range entry; the host refuses to read past the
                // pool and drops the contact.
                continue;
            }
            CGS_ASSERT(mModelsAdded.IsBitSet(luModelIndex),
                       "mModelsAdded.IsBitSet(lContact.muPolyTagB)");                                                          // :2382

            lAddContactEvent.mIDB =
                BuildProxyCarBodyId(mpaModels[luModelIndex].GetHandlingBodyVolumeInstanceId().muId);

            // Geometry. dot3(B - A, N) is the SIGNED separation along the contact normal; a
            // negative one means the pair is penetrating, and the console then collapses the
            // event's second point onto the first rather than shipping the crossed pair.
            const f32 lfSeparation =
                rw::math::vpu::Dot(lContact.mPointOnB - lContact.mPointOnA, lContact.mNormal);

            lAddContactEvent.mNormal   = rw::math::vpu::Negate(lContact.mNormal);
            lAddContactEvent.mPointOnA = lContact.mPointOnA;
            lAddContactEvent.mPointOnB =
                (lfSeparation < 0.0f) ? lContact.mPointOnA : lContact.mPointOnB;

            CGS_ASSERT(liEventIndex >= 0 && liEventIndex < 65535,
                       "liEventIndex >= 0 && liEventIndex < 65535");                          // BrnContactId.h:122
            lAddContactEvent.muTag = static_cast<u32>(
                BrnPhysics::ContactId((static_cast<u32>(liEventIndex) & 0xFFFFu) | 0x03000000u));

            CGS_ASSERT(lpSimInput->GetAddContactQueue()->GetLength() + 1
                           < lpSimInput->GetAddContactQueue()->GetMaxLength(),
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetLength() + 1 < "
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetMaxLength()");                                        // :2412
            lpSimInput->GetAddContactQueue()->AddEventSafe(lAddContactEvent);
        }
    }

    // =================================================================================================
    // DeformationManager::BridgeDetachedWheelCarContactsToSimulation
    //
    // The detached-WHEEL sibling of the body above: drain custom potential-contact queue [4] into
    // the simulation's add-contact queue. Live every frame from the same caller. Same shape as the
    // part bridge with four differences, all console-attested:
    //   * the owner set on side A is DETACHED_RACECAR_WHEEL / DETACHED_TRAFFIC_WHEEL (:2458) and
    //     the slot bound is muPolyTagA < 112 (:2465);
    //   * the slot lookup is DetachedWheelManager::IsSlotUsed / GetWheel; the console calls
    //     IsSlotUsed TWICE -- once as the gate, once as GetWheel's own inlined tripwire
    //     (BrnDetachedWheelManager.h:114) -- and the A-side id is the wheel's packed handle,
    //     read whole from +0x70;
    //   * the materials are 0.9 / 0.5 / 0.8, not the part bridge's 0.5 / 0.5 / 0.5;
    //   * instead of collapsing a separating pair, a DEEPLY penetrating one is REBUILT: when the
    //     separation is worse than -0.01, the contact normal is replaced by the car's own body
    //     RIGHT axis (mTransform.xAxis of the model's vehicle physics) signed to agree with the
    //     original normal, the depth is clamped to 0.01, and mPointOnB is re-derived as
    //     mPointOnA - normal*depth. This is what stops a wheel that has ended up inside a car
    //     from being pushed out along a garbage mesh normal.
    //   ⚠️ Note the model-slot leg carries NO KU_MAX_DEFORMATION_MODELS assert of its own here
    //     (the part bridge's :2381) -- only the bit-array index tripwire. Reproduced as issued.
    // =================================================================================================
    void DeformationManager::BridgeDetachedWheelCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        // Same faithful-unused-parameter note as the body above: the module input buffer's
        // register is never read across the whole body.
        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        const Queue& lrQueue = lpContacts->GetDetachedWheelCarQueue();
        const s32 liQueueLength = lrQueue.GetLength();   // snapshot, as above

        for (s32 liEventIndex = 0; liEventIndex < liQueueLength; ++liEventIndex)
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact lContact =
                lrQueue.GetEvent(liEventIndex);   // 80-byte stack copy

            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lAddContactEvent;
            lAddContactEvent.mStaticFriction  = KF_WHEEL_CAR_STATIC_FRICTION;
            lAddContactEvent.mDynamicFriction = KF_WHEEL_CAR_DYNAMIC_FRICTION;
            lAddContactEvent.mRestitution     = KF_WHEEL_CAR_RESTITUTION;

            const u32 luOwnerA = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdA);
            CGS_ASSERT(luOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL
                           || luOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");    // :2458

            const u32 luOwnerB = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdB);
            CGS_ASSERT(luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");           // :2461

            CGS_ASSERT(lContact.muPolyTagA < KU_MAX_DETACHED_WHEELS,
                       "lContact.muPolyTagA < KU_MAX_DETACHED_WHEELS");                                                        // :2465

            const u16 lu16WheelSlot = static_cast<u16>(lContact.muPolyTagA);
            if (!mDetachedWheelManager.IsSlotUsed(lu16WheelSlot))
            {
                continue;
            }

            // GetWheel re-runs IsSlotUsed as its own tripwire -- that is the console's second
            // call, not a duplicated gate.
            const PhysicalWheel* lpWheel = mDetachedWheelManager.GetWheel(lu16WheelSlot);
            lAddContactEvent.mIDA = lpWheel->GetVolumeInstanceId().muId;

            const u32 luModelIndex = lContact.muPolyTagB;
            CGS_ASSERT(luModelIndex < KU_MAX_DEFORMATION_MODELS, "invalid index : ");        // CgsBitArray.h:203
            if (luModelIndex >= KU_MAX_DEFORMATION_MODELS)
            {
                continue;   // host bounds guard only -- see the part bridge's note
            }
            CGS_ASSERT(mModelsAdded.IsBitSet(luModelIndex),
                       "mModelsAdded.IsBitSet(lContact.muPolyTagB)");                                                          // :2473

            DeformableObject& lrCarModel = mpaModels[luModelIndex];
            lAddContactEvent.mIDB = BuildProxyCarBodyId(lrCarModel.GetHandlingBodyVolumeInstanceId().muId);

            const f32 lfSeparation =
                rw::math::vpu::Dot(lContact.mPointOnB - lContact.mPointOnA, lContact.mNormal);

            Vector3 lNormal   = lContact.mNormal;
            Vector3 lPointOnB = lContact.mPointOnB;
            if (lfSeparation < -KF_MAX_WHEEL_PENETRATION)
            {
                // Depth, clamped to the tolerance (the console's vminfp against the same splat;
                // inside this branch the clamp always wins, but the min is spelled as issued).
                const f32 lfPenetration = -lfSeparation;
                const f32 lfDepth = (KF_MAX_WHEEL_PENETRATION < lfPenetration)
                                        ? KF_MAX_WHEEL_PENETRATION : lfPenetration;

                // The car's body RIGHT axis -- the first row of the vehicle-physics transform.
                const Vector3 lCarRightAxis = lrCarModel.GetVehiclePhysics()->GetTransform().xAxis;

                // sign(dot3(N, right)) via the console's two-compare vsel ladder: an unordered
                // compare falls through to -1.0, which is why this is not written as a plain
                // ternary on (lfAlong < 0.0f).
                const f32 lfAlong = rw::math::vpu::Dot(lNormal, lCarRightAxis);
                f32 lfSign = -1.0f;
                if (lfAlong >= 0.0f)
                {
                    lfSign = (lfAlong > 0.0f) ? 1.0f : 0.0f;
                }

                lNormal   = lCarRightAxis * lfSign;
                lPointOnB = lContact.mPointOnA - lNormal * lfDepth;
            }

            lAddContactEvent.mPointOnA = lContact.mPointOnA;
            lAddContactEvent.mPointOnB = lPointOnB;
            lAddContactEvent.mNormal   = rw::math::vpu::Negate(lNormal);

            CGS_ASSERT(liEventIndex >= 0 && liEventIndex < 65535,
                       "liEventIndex >= 0 && liEventIndex < 65535");                          // BrnContactId.h:122
            lAddContactEvent.muTag = static_cast<u32>(
                BrnPhysics::ContactId((static_cast<u32>(liEventIndex) & 0xFFFFu) | 0x04000000u));

            CGS_ASSERT(lpSimInput->GetAddContactQueue()->GetLength() + 1
                           < lpSimInput->GetAddContactQueue()->GetMaxLength(),
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetLength() + 1 < "
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetMaxLength()");                                        // :2513
            lpSimInput->GetAddContactQueue()->AddEventSafe(lAddContactEvent);
        }
    }

    // =================================================================================================
    // The three pair-builder feeders StartVehicleContactGeneration @0x8262AEE8 calls.
    // AddHingedBodyPartPairs is called UNCONDITIONALLY for EVERY overlapping car-car pair
    // (BrnVehicleManagerContactGeneration.cpp:321), race-car-vs-traffic included. The third
    // (AddRaceCarWheelPair) is a NAMED GATE with one missing leaf, see there.
    //
    // Common shape: resolve the car's deformable model, take its aligned deformed bounding box, take
    // the part's oriented bounding box, and append ONE box-vs-box record to the caller's builder with
    // (part pool slot, car model index) as the two primitive tags. StartVehicleContactGeneration
    // collides the finished builders at the end of its overlap walk.
    // =================================================================================================

    // -------------------------------------------------------------------------------------------------
    // AddRaceCarBodyPartPair @0x82605928 (92)  -- one DETACHED body part vs one car.
    //   0x82605938  the part's pool slot == the LOW 16 bits of the packed volume-instance id
    //               (`clrlwi r31, r5, 16` over the whole 64-bit id the caller loads with `ld r5,8(r21)`;
    //               VolumeInstanceId packs {entityWord<<32 | subA<<16 | subB}, so this is muSubB)
    //   0x82605954  IsPartIndexUsed / GetPart on mDetachedPartManager (console this+48112 == mPartPool)
    //   0x8260597C  mbJoinedToVehicle (+0x1E4) -> STILL HINGED, so AddHingedBodyPartPairs owns it
    //   0x82605990  FindModelIndexByEntityID, -1 == the car has no deformable model, nothing to pair
    //   0x826059C8  the tags: `ld 0x1D0(part)` (mRigidBodyId) low byte == GetPoolIndex, and the model
    //               index as a u16; padding flt_82001DA0 == 0.5f
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddRaceCarBodyPartPair(EntityId lEntityId,
                                                    CgsSceneManager::VolumeInstanceId lVolumeInstanceId,
                                                    PrimitivePairListBuilder* lpBuilder)
    {
        const u16 lu16PoolSlot = static_cast<u16>(lVolumeInstanceId.muId & 0xFFFFu);

        if (!mDetachedPartManager.IsPartIndexUsed(lu16PoolSlot))
        {
            return;
        }
        PhysicalBodyPart* lpPart = mDetachedPartManager.GetPartFromIndex(lu16PoolSlot);
        if (lpPart == nullptr || lpPart->IsJoinedToVehicle())
        {
            return;
        }

        const s32 liModelIndex = FindModelIndexByEntityID(lEntityId);
        if (liModelIndex == -1)
        {
            return;
        }

        CgsGeometric::Box lPartBox;
        lpPart->GetBoundingBox(&lPartBox);
        CgsGeometric::Box lCarBox;
        mpaModels[liModelIndex].GetAlignedDeformedBoundingBox(&lCarBox);

        const u8 lu8PartIndex = lpPart->GetPoolIndex();
        CGS_ASSERT(lu8PartIndex < PhysicalBodyPartPool::KU_MAX_DETACHED_PARTS,
                   "luPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");                            // :2186
        // The streamed value tail is lowered to the static prefix per the standing project rule.
        CGS_ASSERT(mDetachedPartManager.IsPartIndexUsed(lu8PartIndex), "Bad Part index: ");    // :2187

        lpBuilder->AddPrimitivePair(&lPartBox, &lCarBox, KF_PART_VS_CAR_CONTACT_PADDING,
                                    lu8PartIndex, static_cast<u16>(liModelIndex));
    }

    // -------------------------------------------------------------------------------------------------
    // AddHingedBodyPartPairs @0x82605A98 (84)  -- every STILL-HINGED panel of car A vs car B's body
    // box, and every still-hinged panel of car B vs car A's. Two fixed 50-iteration walks
    // (`cmpwi r26, 0x32`), gated on maPartStates[i] == E_PART_STATE_HINGED (`lbzx` == 3), reading the
    // panel's pool slot out of maIKParts[i].miPartPoolIndex (`lhz` at stride 0x10, then `extsh`).
    // Padding is flt_82001C98 == 1.0f, not the 0.5f its two siblings use.
    //
    // CONSOLE DEFECT, REPRODUCED: BOTH loops pass modelIndex A as the SECOND primitive tag
    // (`clrlwi r8, r28, 16` at 0x82605B50 AND 0x82605BB0; r28 is loaded once at 0x82605AB8 and never
    // re-derived). The second loop's records therefore name car A while carrying car B's box. Kept as
    // issued -- the tag is a debug/attribution field on the pair record, not a lookup key.
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddHingedBodyPartPairs(EntityId lEntityIdA, EntityId lEntityIdB,
                                                    PrimitivePairListBuilder* lpBuilder)
    {
        const s32 liModelIndexA = FindModelIndexByEntityID(lEntityIdA);
        const s32 liModelIndexB = FindModelIndexByEntityID(lEntityIdB);
        if (liModelIndexA == -1 || liModelIndexB == -1)
        {
            return;
        }

        DeformableObject& lrModelA = mpaModels[liModelIndexA];
        DeformableObject& lrModelB = mpaModels[liModelIndexB];

        CgsGeometric::Box lCarBoxA;
        CgsGeometric::Box lCarBoxB;
        lrModelA.GetAlignedDeformedBoundingBox(&lCarBoxA);
        lrModelB.GetAlignedDeformedBoundingBox(&lCarBoxB);

        // car A's hinged panels vs car B's body box (0x82605B28..0x82605B78)
        for (s32 liPart = 0; liPart < KI_MAX_PARTS_PER_MODEL; ++liPart)
        {
            if (lrModelA.GetPartState(liPart) != DeformableObject::E_PART_STATE_HINGED)
            {
                continue;
            }
            const s16 li16PoolSlot = lrModelA.GetIKPartDebug(liPart).GetPartPoolIndex();
            CgsGeometric::Box lPartBox;
            mDetachedPartManager.GetPartFromIndex(static_cast<u16>(li16PoolSlot))
                                ->GetBoundingBox(&lPartBox);
            lpBuilder->AddPrimitivePair(&lPartBox, &lCarBoxB, KF_HINGED_PART_CONTACT_PADDING,
                                        static_cast<u16>(li16PoolSlot),
                                        static_cast<u16>(liModelIndexA));
        }

        // car B's hinged panels vs car A's body box (0x82605B88..0x82605BD8)
        for (s32 liPart = 0; liPart < KI_MAX_PARTS_PER_MODEL; ++liPart)
        {
            if (lrModelB.GetPartState(liPart) != DeformableObject::E_PART_STATE_HINGED)
            {
                continue;
            }
            const s16 li16PoolSlot = lrModelB.GetIKPartDebug(liPart).GetPartPoolIndex();
            CgsGeometric::Box lPartBox;
            mDetachedPartManager.GetPartFromIndex(static_cast<u16>(li16PoolSlot))
                                ->GetBoundingBox(&lPartBox);
            lpBuilder->AddPrimitivePair(&lPartBox, &lCarBoxA, KF_HINGED_PART_CONTACT_PADDING,
                                        static_cast<u16>(li16PoolSlot),
                                        static_cast<u16>(liModelIndexA));   // A, per the defect note
        }
    }

    // -------------------------------------------------------------------------------------------------
    // GATE AddRaceCarWheelPair @0x82605BE8 (136 insns) -- reachable only once a wheel has been torn
    // off. BLOCKER: its appender is sub_828149F8, a CYLINDER-vs-BOX AddPrimitivePair overload that has
    // no declaration and no body in the tree (its home is CgsPrimitivePairListBuilder.h/.cpp); the
    // Box/Box overload would stamp the wrong volume type into the record.
    // DELETE-WHEN AddPrimitivePair(Cylinder*, Box*, f32, u16, u16) @0x828149F8 lands.
    // The rest of the body is recovered: DetachedWheelManager::IsSlotUsed/Get on this+72928,
    // FindModelIndexByEntityID, a 5-row cylinder built from the wheel transform (row0 = -wheelRow2,
    // row1/row2 = wheelRow1/row0, row3 = wheelRow3, row4.xy = wheel+0x7C/+0x78), the car box via
    // CgsGeometric::Box::Set off model+0x194C, the "Bad Pool Index: " tripwire (:2324), padding 0.5f.
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddRaceCarWheelPair(EntityId /*lEntityId*/,
                                                 CgsSceneManager::VolumeInstanceId /*lVolumeInstanceId*/,
                                                 PrimitivePairListBuilder* /*lpBuilder*/)
    {
        static bool sbLoggedWheelPairGate = false;
        if (!sbLoggedWheelPairGate)
        {
            sbLoggedWheelPairGate = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: DeformationManager::AddRaceCarWheelPair @0x82605BE8 reached "
                       "(a detached wheel overlapped a car) but not landed -- needs PrimitivePairList"
                       "Builder::AddPrimitivePair(Cylinder*, Box*) @0x828149F8, undeclared "
                       "[FLAG PC boot gate]. Reported once, not per frame\n";
        }
    }
}
}
