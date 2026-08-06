// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_ContactBridges.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the contact-BRIDGE slice consumed by
// PhysicsModule::BridgeContactsToSimulation @0x825A99E8 (big-five #2 wave, 2026-08-06).
// Slice TU: home BrnDeformationManager.cpp / the _Contacts group is still unmounted (its
// SolvePenetration / UpdateTriangleCache tail carries ~19 unresolved of its own). Fold back
// when the home mounts.
//
// CONTENTS:
//   * ReadPotentialVehicleWorldContact @0x82604590 -- MOVED VERBATIM from
//     BrnDeformationManager_Contacts.cpp (this TU is mounted; that one is not). One change,
//     flagged inline: the "Un-normalised" tripwire's old always-pass placeholder is upgraded
//     to the faithful renormalise-delta test -- the committed rw::math::vpu Normalize family
//     it was waiting on exists (vector3_operation_inline.h, FAITHFUL).
//   * FindModelIndexByGlobalEntityID @0x825B45B0 -- MOVED VERBATIM (same reason; callee of
//     the above).
//   * ReadPotentialContact @0x826053F8 -- NEW this wave. Reconstructed from the
//     BURNOUT_X360_ARTIST.XEX asm with the PS3 DecFIGS out-of-line body @0x6F048C as the
//     structural oracle (debug names lPotentialContact/lContactId/lpSimInput,
//     liModelIndexA/B from its baked asserts).
//   * BridgeBodyPartCarContactsToSimulation @0x825DD7D0 + BridgeDetachedWheelCar-
//     ContactsToSimulation @0x825DDD48 -- ⚠⚠ TRAP STUBS (closure enforcement; 348/392 X360
//     asm lines each -- named, not landed, this wave). RECONSTRUCT-NEXT.
//
// Everything is dead code today: the only caller chain is PhysicsModule::Update @0x825B0640,
// still a link stub, so /OPT:REF strips this TU's bodies. Mounted for closure enforcement.
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // PotentialContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (+ DeformationSensor)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (GetTransform + the timer bank)
#include "rw/math/vpu/vector3_operation.h"                                                // Normalize / IsValid

namespace BrnPhysics
{
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

        // Per-lane NaN self-compare (the asm's vspltw + vcmpeqfp. over lanes x/y/z) -- the
        // vendor vpu tree's own IsValid(Vector3).
        inline bool IsValidVec3Lanes(const Vector3& lrV) { return rw::math::vpu::IsValid(lrV); }

        // The consoles' "Un-normalised ..." tripwire predicate: renormalise the vector
        // (vrsqrtefp + double Newton-Raphson + the zero-guard vsel -- the committed Normalize
        // reproduces all of it), then per-lane compare |renormalised - original| against the
        // splatted epsilon 0.0099999998 (vsubfp / vandc-abs / vcmpgtfp). True == acceptably
        // unit-length (no lane's delta exceeds the epsilon).
        inline bool IsNormalisedWithinEpsilon(const Vector3& lrV)
        {
            const Vector3 lRenormalised = rw::math::vpu::Normalize(lrV);
            const f32 laDeltas[3] = { lRenormalised.x - lrV.x,
                                      lRenormalised.y - lrV.y,
                                      lRenormalised.z - lrV.z };
            for (int liLane = 0; liLane < 3; ++liLane)
            {
                const f32 lfDelta = laDeltas[liLane];
                if ((lfDelta < 0.0f ? -lfDelta : lfDelta) > 0.0099999998f)
                {
                    return false;
                }
            }
            return true;
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

        Matrix44Affine lWorldTransform;
        lrModel.GetTransform(lWorldTransform);

        // Normalised-normal tripwire (non-gating). ⭐ UPGRADED AT MOVE 2026-08-06: the old body
        // carried an honest always-pass placeholder ("no faithful Vector3 normalise helper is
        // homed") -- the committed rw::math::vpu Normalize family (vector3_operation_inline.h,
        // FAITHFUL banner) retires that FLAG, so the renormalise-delta predicate (epsilon
        // 0.0099999998, per-lane) is now emitted for real. The streamed "Car transform: " message
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
        if (lrSensorA.ValidateAndAddContact(
                lWorldToModelA,
                reinterpret_cast<const CgsSceneManager::PotentialContact&>(lrPotentialContact),
                lContactId, &lrModelB, &lrSensorB))
        {
            // Accepted: zero BOTH cars' contact cool-down (the Y lane of the packed timer bank).
            lrModelA.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;
            lrModelB.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;
        }
    }

    // =================================================================================================
    // DeformationManager::BridgeBodyPartCarContactsToSimulation  @0x825DD7D0  (PS3 DecFIGS 0x6F728C)
    //
    // ⚠⚠ TRAP STUB (closure enforcement, 2026-08-06 big-five #2 wave) -- the REAL body (348 X360
    // asm lines / 11 callees) is NOT reconstructed yet. Dead code today (Update @0x825B0640 is
    // still a link stub; /OPT:REF strips this). RECONSTRUCT-NEXT.
    // =================================================================================================
    void DeformationManager::BridgeBodyPartCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* /*lpContacts*/)
    {
        CGS_ASSERT(false,
                   "TRAP: DeformationManager::BridgeBodyPartCarContactsToSimulation @0x825DD7D0 "
                   "not reconstructed (big-five #2 closure stub)\n");
    }

    // =================================================================================================
    // DeformationManager::BridgeDetachedWheelCarContactsToSimulation  @0x825DDD48  (PS3 0x741E28)
    //
    // ⚠⚠ TRAP STUB (closure enforcement, 2026-08-06 big-five #2 wave) -- the REAL body (392 X360
    // asm lines / 12 callees) is NOT reconstructed yet. Dead code today. RECONSTRUCT-NEXT.
    // =================================================================================================
    void DeformationManager::BridgeDetachedWheelCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* /*lpContacts*/)
    {
        CGS_ASSERT(false,
                   "TRAP: DeformationManager::BridgeDetachedWheelCarContactsToSimulation @0x825DDD48 "
                   "not reconstructed (big-five #2 closure stub)\n");
    }
}
}
