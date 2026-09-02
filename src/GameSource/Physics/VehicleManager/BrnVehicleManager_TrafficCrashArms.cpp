// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_TrafficCrashArms.cpp
//
// THE TWO TRAFFIC-SIDE CRASH-PREDICTION ARMS DoCrashPrediction @0x82645FE0 dispatches -- the last
// two declare-only callees in that body, each behind a DELETE-WHEN gate since the head landed:
//
//   VehicleManager::HandleTrafficCarTrafficCarPotentialContact  @0x8263EC90 (279)  -- DecFIGS 0x797750
//   VehicleManager::HandleTrafficCarWorldPotentialContact       @0x8263F0F0 (220)  -- DecFIGS 0x7972AC
//
// Slice TU (home BrnVehicleManager.cpp is still unmountable). Both bodies are read off the ARTIST
// asm; the Hex-Rays for both is the usual 17/29-int prototype with "local variable allocation has
// failed". Signatures are the DecFIGS mangles verbatim:
//   (PotentialContact BY VALUE, VehicleOutputRequestInterface*, VehicleOutputInterface*,
//    VehicleManagerOutputInterface*, DeformationInputInterface*, float)
// and the caller confirms the order byte-for-byte: DoCrashPrediction stores r19 (request) /
// r20 (vehicle-out) / r17 (manager-out) / r21 (deformation) at var_81C/814/80C/804 before both
// calls, with f1 = timestep (0x82646310..0x82646338 and 0x82646BB4..0x82646BF8).
//
// ARGUMENT MAP (both prologues): r3 = this; the 80-byte contact arrives in r4..r10 (bytes 0..55,
// spilled to arg_20..arg_50) and on the stack from arg_58 (bytes 56..79); f1 = timestep. The four
// interface pointers sit at arg_74 (request) / arg_7C (vehicle-out) / arg_84 (manager-out) /
// arg_8C (deformation). NEITHER body ever loads arg_74: the request interface is accepted and
// ignored by the console, so the parameter is kept and unused.
//
// ⭐⭐ THE WORLD ARM HAS NO SIDE EFFECTS -- ON THE CONSOLE. Read before "fixing" it:
//   Every store in 0x8263F0F0..0x8263F45C is r1-relative (the two splat scratch vectors, the
//   timestep spill, and one dead bool), and the only calls are the three assert entry points and
//   the two PhysicalTrafficManager accessors (xrefs_from, .ida-exports). It resolves the traffic
//   car, drops the contact if the two bodies are separating, drops it if the car is already
//   CRASHING, tests the car's ground normal against its up axis (> 0.8) and the contact point's
//   height above the ground plane (< 0.4) -- and stores THAT comparison's CR bits into a stack
//   local nothing reads (`mfocrf r11,2 ; stw r11, var_A0` @0x8263F434, then the epilogue).
//   The DecFIGS internal PS3 build (Dec-2007, DEV) has the same shape: xrefs_from are the three
//   asserts only, the tail ends in the 0.8 splat. Whatever consumed the classification was compiled
//   out before Dec-2007 and never came back for retail. ⇒ a traffic car knocked into scenery is NOT
//   crashed by this arm on the console; it keeps the state the race-car / traffic-traffic arms gave
//   it, and its physical answer to the wall is the rigid-body contact solve, not a state change.
//   Reproduced as-is: classify, commit nothing. (The wave brief assumed the opposite; the asm wins.)
//
// THE CONSTANTS:
//   flt_82F31928 = 0.44704f (MPH -> m/s, .data, image-resident), flt_82092BC4 = 60.0f,
//   flt_8208F9C8 = 0.8f, flt_82F2A148 = 0.4f, flt_82001CC0 = 0.0f -- all read straight off the
//   image with tools/re/x360rd.py; none of them is a dyn-init zero.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                 // E_TRAFFIC_TYPE_*
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_*
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"  // BrnTraffic::GetVehicleSpecies
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // flt_82092BC4 * flt_82F31928 (`fmuls f0, f0, f13` @0x8263EEE0): the closing speed, along the
    // contact normal, above which a traffic-vs-traffic contact CRASHES both cars instead of
    // slamming them. 60 mph, converted at the call site, not a pre-baked m/s literal.
    const f32 KF_TRAFFIC_TRAFFIC_CRASH_SPEED_MPH = 60.0f;          // flt_82092BC4
    const f32 KF_TRAFFIC_MPH_TO_MPS              = 0.447039992f;   // flt_82F31928

    // The world arm's two dead thresholds (see the banner).
    const f32 KF_TRAFFIC_WORLD_UPRIGHT_COSINE    = 0.800000012f;   // flt_8208F9C8
    const f32 KF_TRAFFIC_WORLD_CONTACT_HEIGHT    = 0.400000006f;   // flt_82F2A148

    inline u32 EntityWordOf(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        return static_cast<u32>(lrId.muId >> 32);
    }

    inline u16 EntityIndexOfWord(u32 luEntityWord)
    {
        return static_cast<u16>((luEntityWord >> 10) & 0x3FFFu);
    }
}

// -------------------------------------------------------------------------------------------
// HandleTrafficCarTrafficCarPotentialContact  @0x8263EC90 (279)
//
// Called from DoCrashPrediction's queue-[13] walk for every traffic-with-traffic potential
// contact whose two owner bytes are both TRAFFIC_VEHICLE (0x826462FC..0x8264630C). Resolves both
// GLOBAL ids to PHYSICAL traffic slots (a non-physical car on either side ends it), asks the
// swept-box predictor whether the two bodies will actually meet, and then hands each car to one
// of the two response arms:
//     closing speed along the normal > 60 mph, OR the car is the TRAILER  -> CRASHING
//     otherwise, if the car is not already PHYSICAL                       -> SLAMMED by the other,
//                                                                            magnitude = closing * m_other / m_self
// The A half runs first, then the B half, with the roles swapped -- the console emits the two
// halves back to back (0x8263EF40..0x8263F008 and 0x8263F00C..0x8263F0DC), sharing lbHardHit and
// the closing speed (f31).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleTrafficCarTrafficCarPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    f32 lfTimestep)
{
    // arg_74 is never loaded by the console body (see the banner).
    (void)lpRequestOutputInterface;

    // 0x8263ECA0 / 0x8263ECB8 -- the two GLOBAL entity words (r31 / r24).
    const u32 luTrafficAGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luTrafficBGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdB);

    CGS_ASSERT((luTrafficAGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficAEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6690
    CGS_ASSERT((luTrafficBGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficBEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6691

    // 0x8263ED2C..0x8263EE54: GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe, inlined twice (its
    // h:944 bound assert and the EntityId ctor's < 0x4000 tripwire included). A 0x7F map slot on
    // EITHER side -- a traffic car that has no physics -- ends the contact with no assert.
    ::EntityId lTrafficAPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficAGlobalWord, &lTrafficAPhysicsID))
        return;
    ::EntityId lTrafficBPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficBGlobalWord, &lTrafficBPhysicsID))
        return;

    // 0x8263EE5C / 0x8263EE70: `extrwi 14,8` on each physics id -> the two physical slots.
    const u16 lu16TrafficAIndex = EntityIndexOfWord(lTrafficAPhysicsID.muValue);   // r20
    const u16 lu16TrafficBIndex = EntityIndexOfWord(lTrafficBPhysicsID.muValue);   // r17

    const PhysicalTrafficVehicle* const lpTrafficA =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficAIndex));   // r25
    const PhysicalTrafficVehicle* const lpTrafficB =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficBIndex));   // r24

    const SimpleVehiclePhysics* const lpBodyA = lpTrafficA->mpVehicleBody;   // lwz 0x1C(r25)
    const SimpleVehiclePhysics* const lpBodyB = lpTrafficB->mpVehicleBody;   // lwz 0x1C(r24)

    // 0x8263EE98: the swept-box prediction. A pair that will not meet is dropped -- there is no
    // near-miss arm for traffic-on-traffic.
    if (!PredictCarCarIntersection(lpBodyA, lpBodyB, lfTimestep))
        return;

    // 0x8263EEA8..0x8263EEF8: closing speed along the contact normal = (vB - vA) . n, against
    // 60 mph in m/s. `bgt` -> lbHardHit (r30, later r22).
    const Vector3 lvRelativeVelocity = lpBodyB->GetLinearVelocity() - lpBodyA->GetLinearVelocity();   // v12 - v13
    const f32  lfClosingSpeed = rw::math::vpu::Dot(lvRelativeVelocity, lContact.mNormal);           // f31
    const bool lbHardHit      = lfClosingSpeed > (KF_TRAFFIC_TRAFFIC_CRASH_SPEED_MPH * KF_TRAFFIC_MPH_TO_MPS);

    // 0x8263EEFC..0x8263EF3C: `cntlzw(species - 2) >> 5` == (species == E_SPECIES_TRAILER), on the
    // GLOBAL index of each car. A trailer half never takes the slam arm.
    const bool lbAIsTrailer =
        (BrnTraffic::GetVehicleSpecies(EntityIndexOfWord(luTrafficAGlobalWord)) == BrnTraffic::Vehicle::E_SPECIES_TRAILER);   // r23
    const bool lbBIsTrailer =
        (BrnTraffic::GetVehicleSpecies(EntityIndexOfWord(luTrafficBGlobalWord)) == BrnTraffic::Vehicle::E_SPECIES_TRAILER);   // r18

    // ---- the A half (0x8263EF40..0x8263F008): A is the victim, B the crasher --------------------
    if (lbHardHit || lbAIsTrailer)
    {
        mPhysicalTrafficManager.SetTrafficVehicleCrashing(
            lTrafficAPhysicsID, lTrafficBPhysicsID,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }
    else if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficAIndex))->mePhysicalTrafficState
                 != static_cast<u32>(E_TRAFFIC_TYPE_PHYSICAL))                      // lwz 0x20 ; cmpwi 2
    {
        // v2 = (1 / massA) * (massB * closing): vrefp + two Newton steps on A's mass (+0xE0), B's
        // mass scaled by the closing speed, multiplied. v1 = the contact point ON A (arg_20).
        const f32 lfSlamMagnitude =
            lfClosingSpeed * lpBodyB->GetMass().x / lpBodyA->GetMass().x;
        mPhysicalTrafficManager.SetTrafficVehicleSlammed(
            lTrafficAPhysicsID, lTrafficBPhysicsID, lpBodyB,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
            lContact.mPointOnA,
            VecFloat{ lfSlamMagnitude, lfSlamMagnitude, lfSlamMagnitude, lfSlamMagnitude });
    }

    // ---- the B half (0x8263F00C..0x8263F0DC): B is the victim, A the crasher --------------------
    if (lbHardHit || lbBIsTrailer)
    {
        mPhysicalTrafficManager.SetTrafficVehicleCrashing(
            lTrafficBPhysicsID, lTrafficAPhysicsID,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }
    else if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficBIndex))->mePhysicalTrafficState
                 != static_cast<u32>(E_TRAFFIC_TYPE_PHYSICAL))
    {
        // v2 = (1 / massB) * (massA * closing); v1 = the contact point ON B (arg_30).
        const f32 lfSlamMagnitude =
            lfClosingSpeed * lpBodyA->GetMass().x / lpBodyB->GetMass().x;
        mPhysicalTrafficManager.SetTrafficVehicleSlammed(
            lTrafficBPhysicsID, lTrafficAPhysicsID, lpBodyA,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
            lContact.mPointOnB,
            VecFloat{ lfSlamMagnitude, lfSlamMagnitude, lfSlamMagnitude, lfSlamMagnitude });
    }
}

// -------------------------------------------------------------------------------------------
// HandleTrafficCarWorldPotentialContact  @0x8263F0F0 (220)
//
// Called from DoCrashPrediction's queue-[9] walk for every traffic-with-world potential contact.
// Orients the record so the traffic car is "A" (the world's normal is sign-flipped when the
// scene manager reported the pair the other way round), resolves the physical slot, and drops the
// contact if the two are separating or the car is already crashing. Then it classifies the hit
// -- upright on its ground normal, contact low against the ground plane -- into a local that the
// retail build never reads. NO STATE CHANGES, NO EVENTS, NO CALLS PAST THE ACCESSORS (banner).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleTrafficCarWorldPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    f32 lfTimestep)
{
    // None of the four interfaces is loaded by the console body (see the banner).
    (void)lpRequestOutputInterface;
    (void)lpVehicleOutputInterface;
    (void)lpManagerOutputInterface;
    (void)lpDeformationInterface;

    // 0x8263F118..0x8263F1A8: which side is the traffic car? Owner byte of id A == TRAFFIC_VEHICLE
    // keeps the record as-is (v127 = pointOnA, v126 = pointOnB, v125 = normal); otherwise the
    // roles swap and the normal is sign-flipped (`vspltisw -1 ; vslw ; vxor128`).
    const u32 luWordA = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luWordB = EntityWordOf(lContact.muVolumeInstanceIdB);

    u32     luTrafficGlobalWord;   // r31
    u32     luWorldWord;           // r30
    Vector3 lvPointOnTraffic;      // v127
    Vector3 lvPointOnWorld;        // v126
    Vector3 lvNormal;              // v125  (points OUT of the world, into the traffic car)
    if ((luWordA >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
    {
        luTrafficGlobalWord = luWordA;
        luWorldWord         = luWordB;
        lvPointOnTraffic    = lContact.mPointOnA;
        lvPointOnWorld      = lContact.mPointOnB;
        lvNormal            = lContact.mNormal;
    }
    else
    {
        luTrafficGlobalWord = luWordB;
        luWorldWord         = luWordA;
        lvPointOnTraffic    = lContact.mPointOnB;
        lvPointOnWorld      = lContact.mPointOnA;
        lvNormal            = rw::math::vpu::Negate(lContact.mNormal);
    }

    CGS_ASSERT((luTrafficGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");   // :6831
    CGS_ASSERT((luWorldWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lWorldEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");               // :6832

    // 0x8263F204..0x8263F29C: the inlined _Safe lookup; a 0x7F slot ends it without an assert.
    ::EntityId lTrafficPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficGlobalWord, &lTrafficPhysicsID))
        return;

    const u16 lu16TrafficIndex = EntityIndexOfWord(lTrafficPhysicsID.muValue);   // r28

    const PhysicalTrafficVehicle* lpTraffic =
        static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    CGS_ASSERT(lpTraffic != 0, "lpTraffic");                                       // :6848

    // ---- 0x8263F2E0..0x8263F38C: the separating test, the same shape as the race-car arm's.
    //      vP = w x (pTraffic - pos) + v ; (pTraffic + vP*dt - pWorld) . n > 0 -> moving APART.
    {
        const SimpleVehiclePhysics* const lpBody = lpTraffic->mpVehicleBody;           // lwz 0x1C
        const Matrix44Affine lTransform      = lpBody->GetTransform();                 // +0x10..+0x40
        const Vector3 lvRadius        = lvPointOnTraffic - lTransform.wAxis;           // v13
        const Vector3 lvPointVelocity =
            rw::math::vpu::Cross(lpBody->GetAngularVelocity(), lvRadius) + lpBody->GetLinearVelocity();   // +0x60 / +0x50
        const Vector3 lvPredictedPoint = lvPointVelocity * lfTimestep + lvPointOnTraffic;
        if (rw::math::vpu::Dot(lvPredictedPoint - lvPointOnWorld, lvNormal) > 0.0f)  // flt_82001CC0
            return;
    }

    // 0x8263F390..0x8263F3A4: already CRASHING -> nothing to do (lwz 0x20 ; cmpwi 1).
    if (mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex))->mePhysicalTrafficState
            == static_cast<u32>(E_TRAFFIC_TYPE_CRASHING))
        return;

    // ---- 0x8263F3A8..0x8263F438: THE DEAD CLASSIFICATION. Body +0x580 is
    //      mAboveGroundTestResult.mIntersectionNormal and +0x570 its mIntersectionPosition.
    //      upright  := up . groundNormal > 0.8
    //      lowHit   := 0.4 > (pTraffic - groundPoint) . groundNormal        (only if upright)
    //      The console stores lowHit's CR word to var_A0 and returns. Nothing consumes it; there is
    //      no store outside the stack frame past this point. Reproduced as computed-and-discarded.
    lpTraffic = static_cast<const PhysicalTrafficManager&>(mPhysicalTrafficManager).GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    {
        const SimpleVehiclePhysics* const lpBody = lpTraffic->mpVehicleBody;
        const AboveGroundTestResult* const lpGround = lpBody->GetAboveGroundTestResult();
        const Vector3 lvUp = lpBody->GetTransform().yAxis;                              // lvx128 +0x20

        bool lbLowContactOnUprightCar = false;                                          // var_A0, dead
        if (rw::math::vpu::Dot(lvUp, lpGround->mIntersectionNormal) > KF_TRAFFIC_WORLD_UPRIGHT_COSINE)   // vcmpgtfp. vs 0.8
        {
            lbLowContactOnUprightCar =
                KF_TRAFFIC_WORLD_CONTACT_HEIGHT >
                rw::math::vpu::Dot(lvPointOnTraffic - lpGround->mIntersectionPosition, lpGround->mIntersectionNormal);   // vcmpgtfp. 0.4 vs
        }
        (void)lbLowContactOnUprightCar;
    }
}

}   // namespace Vehicle
}   // namespace BrnPhysics
