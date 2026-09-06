// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_DoCrashPrediction.cpp
//
// The HEAD of the crash-prediction web.
//
//   VehicleManager::DoCrashPrediction  @0x82645FE0 (814)  -- DWARF BrnVehicleManager.h:341
//
// Called every frame by PhysicsModule::Update (BrnPhysicsModuleUpdateFunctions.cpp:416). The chain:
//   DoCarCarContactGeneration -> maCustomEventQueues[8] -> DoCrashPredictionForRaceCarAndTrafficVehicle
//   -> HandleCrashPredictionForRaceCarAndTrafficVehicle -> HandleRaceCarTrafficCarPotentialContact
//   -> PhysicalTrafficManager::SetTrafficVehicle{Crashing,Slammed,Checked} / TestForNearMissFreakOut
//   -> the PhysicalTrafficState readback the world consumes.
//
// Read off the ARTIST asm (.ida-exports 0x82645FE0). Hex-Rays renders it as a 31-int prototype
// because of the PPC float-arg GPR skip: r3=this, r4/r5 = the two stacks, f1 = timestep with r6
// SKIPPED, then r7 input / r8 vehicle-out / r9 request-out / r10 manager-out and the last two
// interfaces on the stack. The eight leading asserts name every parameter (BrnVehicleManager.cpp
// :2880..:2887), which is what pins the mapping.
//
// THE AVERAGER IS A STACK LOCAL OF THIS FUNCTION. `v149[1680] @ sp+0x150` with `v150 @ sp+0x7E0`
// == 0x150+0x690 is exactly PotentialContactAverager (20 * 0x50 pairs, 20 weights @0x640, count
// @0x690), and `v150 = 0` right after AllocateInternalBuffers is its only initialisation. That is
// why no PotentialContactAverager instance exists anywhere else in the tree: the console has none.
//
// QUEUE INDICES, byte-proven against BrnPhysicsModuleIO_PotentialContactInterface.h's
// `16 + n * 0x28010` map:
//     ifc + 2130144 -> [13] traffic-with-traffic        (count at +2130152)
//     ifc + 1310864 -> [ 8] race-car-with-traffic       (count at +1310872)
//     ifc + 1474720 -> [ 9] traffic-with-world          (count at +1474728)
//
// THREE NAMED GATES, each at its seat below. None of them is on the race-car-vs-traffic path.
// (A fourth -- the race-car-vs-WORLD driver at seat (2b) -- was DISCHARGED 2026-09-02 by the crash
// wave; its two callees are bodied in BrnVehicleManager_WorldCrashArm.cpp.)
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnPotentialContactAverager.h"
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // VehicleInputInterface::GetTriangleCacheInterface (the console's inlined `+128016`)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"  // TriangleCacheInterface::GetCache / GetNumCachedTriangleBatches
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"             // CgsGeometric::Triangle4 (the SoA batch the cache stores)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint ([noslowmo] probe only)

#include <cmath>     // std::sqrt  (the vrsqrtefp + 2 Newton normalise, lowered)
#include <cstring>   // std::memcpy (Triangle4 lane masks / surface tags are raw words)
#include <cstdlib>   // getenv      ([noslowmo] probe only)

// =================================================================================================
// THE FORCE-NO-SLOW-MO TRIANGLE-CACHE KERNEL  (inlined by the console into DoCrashPrediction at
// 0x82646450..0x82646B8C -- 463 instructions, the largest single block in this function).
//
// It is Moller-Trumbore, single-sided, UNNORMALISED, four triangles wide over one
// CgsGeometric::Triangle4 batch, keeping the NEAREST hit -- structurally the same kernel the
// traction line tests run (GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp:600..750,
// console 0x82921CC4), down to the two shared .rdata constants:
//     unk_82FB9F10 <- flt_8200D5F0 == 1e-8   the determinant floor  (its twin is unk_8321D330)
//     unk_82FB9EF0 <- flt_82004884 == 1e-5   the barycentric tolerance (twin unk_8321D310)
// Both are splats, so the vperm/vsldoi shuffle the console runs on them (unk_82CDA400 /
// unk_82CDA3C0 at 0x82646694..0x826466B4) collapses to the same scalar -- those two control
// vectors are ordinary .rdata and are readable with x360rd, which is how that was settled rather
// than assumed.
//
// PER-LANE ITERATION rather than four-wide SIMD, with the same justification and the same wording
// as the traction kernel: the console's scalar early-out at 0x8264698C (four `vspltw` +
// `vcmpeqfp.` + `mfocrf`, "did ANY lane's mask survive") is a pure optimisation that changes no
// result, and a lane loop subsumes it exactly. The four-stage `vsel` cascade at
// 0x82646A5C..0x82646AE8 keeps `!(t >= best)` with lane 0 first, so on an exact tie the LOWER lane
// wins; iterating 0..3 with the same strict predicate reproduces that, NaN behaviour included.
// =================================================================================================
namespace
{
    // The determinant floor and the relative barycentric tolerance (see the banner). The console
    // holds them as pre-splatted .bss vectors filled by the CRT bank, not as inline literals.
    const f32 KF_MIN_DETERMINANT       = 1.0e-8f;   // unk_82FB9F10 <- flt_8200D5F0
    const f32 KF_BARYCENTRIC_TOLERANCE = 1.0e-5f;   // unk_82FB9EF0 <- flt_82004884

    // The running-minimum seed. `lfs f0, flt_8208F5EC` @0x826465B8 then `vspltw128 v59, v0, 0`.
    const f32 KF_NO_HIT_DISTANCE = 3.4028235e38f;   // flt_8208F5EC == FLT_MAX

    // ---- the three constants of the gate proper ------------------------------------------
    // ⭐ The first and third are NOT static-init thunks and NOT inline literals: each is a .bss
    // vector filled by a function-local `static` LAZY FIRST-CALL CACHE inside this very block,
    // sharing the guard word dword_82FBA370 (bit 0 / bit 1). That is the shape a CRT-bank sweep
    // reports as "no CRT writer found" -- the same sentence five genuinely-wrong constants were
    // carried under -- so both are spelled with their writer here.
    const f32 KF_NO_SLOW_MO_NOSE_FRACTION = 0.75f;  // unk_82FBA360 <- lazy cache 0x8264656C
                                                    //                <- flt_82004018
    const f32 KF_NO_SLOW_MO_UPRIGHT_DOT   = 0.7f;   // unk_82FBA350 <- lazy cache 0x82646B08
                                                    //                <- flt_82004C68 (0.699999988)
    // An IMMEDIATE, not rodata: `vspltisw v0, 2` @0x826465AC then `vcfsx v11, v0, 0` @0x826465BC.
    const f32 KF_NO_SLOW_MO_RAY_LENGTH    = 2.0f;

    // A Triangle4 lane's enable bit (+0x90, `vand v11, v11, v7` @0x82646988). The producer side
    // writes 0xFFFFFFFF or 0x00000000 per lane (CgsPolygonSoupTests.cpp), so a non-zero word means
    // "this lane holds a real triangle". Same helper, same reason, as ContactGeneratorJob.cpp:193.
    inline bool IsLaneEnabled(const Vector4& lrMasks, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrMasks.x)[liLane], sizeof(u32));
        return luBits != 0u;
    }

    // Read a lane as a RAW WORD. The surface tag rides Triangle4::mSurfaceTags (+0xA0) through the
    // nearest-select as a float register (`vspltw` + `vsel` only, never an FP operation), so
    // reading it as a bit pattern is what keeps a tag whose encoding happens to be a NaN intact.
    inline u32 LaneBits(const Vector4& lrV, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrV.x)[liLane], sizeof(u32));
        return luBits;
    }

    // What the console carries across the batch loop in v59 / v57 / v58 (0x826465CC..0x826465F8
    // seeds them FLT_MAX / 0 / 0) plus the `r25` any-hit byte it sets at 0x82646A28.
    struct NearestCachedTriangleHit
    {
        bool    mbHit;          // r25 -- set on an ACCEPTED lane, before the distance race
        f32     mfNearestT;     // v59
        Vector4 mNormal;        // v58 -- the unit face normal of the winning triangle
        u32     muSurfaceTag;   // v57 -- carried by the console, read by nothing (see below)
        s32     miAccepted;     // NOT the console's: the [noslowmo] probe's armed-counter only
    };

    // 0x826466C0..0x82646AF8. `lpaBatches` is TriangleCacheInterface::GetCache(slot) and
    // `liNumBatches` its GetNumCachedTriangleBatches(slot); the console walks them with a
    // decrementing count and a pointer stepped by sizeof(Triangle4) == 0xE0.
    NearestCachedTriangleHit FindNearestCachedTriangleHit(
        const CgsGeometric::Triangle4* lpaBatches, s32 liNumBatches,
        const Vector3& lrOrigin, const Vector3& lrDelta)
    {
        NearestCachedTriangleHit lResult;
        lResult.mbHit        = false;
        lResult.mfNearestT   = KF_NO_HIT_DISTANCE;
        lResult.mNormal.SetZero();
        lResult.muSurfaceTag = 0u;
        lResult.miAccepted   = 0;

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            const CgsGeometric::Triangle4& lrBlock = lpaBatches[liBatch];

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                const f32 lfP0x = (&lrBlock.mVertex0X.x)[liLane];
                const f32 lfP0y = (&lrBlock.mVertex0Y.x)[liLane];
                const f32 lfP0z = (&lrBlock.mVertex0Z.x)[liLane];

                // e1 = P1 - P0, e2 = P2 - P0   (0x82646824 / 0x8264682C / 0x82646830 and
                // 0x8264681C / 0x82646810 / 0x82646818 -- the SoA vsubfp pairs).
                const f32 lfE1x = (&lrBlock.mVertex1X.x)[liLane] - lfP0x;
                const f32 lfE1y = (&lrBlock.mVertex1Y.x)[liLane] - lfP0y;
                const f32 lfE1z = (&lrBlock.mVertex1Z.x)[liLane] - lfP0z;
                const f32 lfE2x = (&lrBlock.mVertex2X.x)[liLane] - lfP0x;
                const f32 lfE2y = (&lrBlock.mVertex2Y.x)[liLane] - lfP0y;
                const f32 lfE2z = (&lrBlock.mVertex2Z.x)[liLane] - lfP0z;

                // P = cross(delta, e2)   (0x8264683C..0x826468BC)
                const f32 lfPx = (lfE2z * lrDelta.y) - (lfE2y * lrDelta.z);
                const f32 lfPy = (lfE2x * lrDelta.z) - (lfE2z * lrDelta.x);
                const f32 lfPz = (lfE2y * lrDelta.x) - (lfE2x * lrDelta.y);

                // T = origin - P0   (0x826468DC / 0x826468E0 / 0x82646834; the last one is
                // `vsubfp128 v10, v95, v10` -- v95 is v63 once IDA's vA high-bit swap is undone).
                const f32 lfTx = lrOrigin.x - lfP0x;
                const f32 lfTy = lrOrigin.y - lfP0y;
                const f32 lfTz = lrOrigin.z - lfP0z;

                // det = dot(e1, P)  (0x826468D4 / D8 / E4) ; u = dot(T, P)  (0x826468F4 / 0x82646908
                // / 0x82646920). u/v/tnum stay UNNORMALISED; the real barycentric is u/det.
                const f32 lfDet = (lfE1x * lfPx) + (lfE1y * lfPy) + (lfE1z * lfPz);
                const f32 lfU   = (lfTx * lfPx) + (lfTy * lfPy) + (lfTz * lfPz);

                // Q = cross(T, e1)  (0x826468E8..0x82646910)
                const f32 lfQx = (lfTy * lfE1z) - (lfTz * lfE1y);
                const f32 lfQy = (lfTz * lfE1x) - (lfTx * lfE1z);
                const f32 lfQz = (lfTx * lfE1y) - (lfTy * lfE1x);

                // v = dot(Q, delta)  (0x82646918 / 0x82646928 / 0x82646938)
                // tnum = dot(e2, Q)  (0x8264691C / 0x8264692C / 0x8264695C)
                const f32 lfV    = (lfQx * lrDelta.x) + (lfQy * lrDelta.y) + (lfQz * lrDelta.z);
                const f32 lfTNum = (lfE2x * lfQx) + (lfE2y * lfQy) + (lfE2z * lfQz);

                // The acceptance test in the console's own shape (0x82646900..0x82646988):
                //     lo = (-det) * eps   (vxor with the 0x80000000 splat, then vmulfp)
                //     hi = det - lo       (0x82646924  vsubfp v9, v12, v11)
                // ⚠️ Every upper bound is `!(x > hi)`, NOT `x <= hi`. `vcmpgtfp` is false for a NaN
                // and the `vnot` beside it turns that into true, so a NaN is ACCEPTED by the upper
                // bounds and rejected by the lower ones. Writing `<=` would silently change that.
                // ⚠️ `det > 1e-8` and not `|det| > 1e-8`: the test is SINGLE-SIDED, so a triangle
                // whose front face points away from the nose cannot be hit.
                const f32 lfLo = (-lfDet) * KF_BARYCENTRIC_TOLERANCE;
                const f32 lfHi = lfDet - lfLo;

                const bool lbAccept =
                       (lfDet > KF_MIN_DETERMINANT)
                    && (lfU >= lfLo)    && !(lfU > lfHi)
                    && (lfV >= lfLo)    && !((lfU + lfV) > lfHi)
                    && (lfTNum >= lfLo) && !(lfTNum > lfHi)
                    && IsLaneEnabled(lrBlock.mValidMasks, liLane);

                if (!lbAccept)
                {
                    continue;
                }

                // ⚠️ THE ANY-HIT BYTE IS SET HERE, BEFORE THE DISTANCE RACE -- exactly where the
                // console sets it (`li r25, 1` @0x82646A28, inside the "any lane survived" block
                // and ABOVE the vsel cascade). An accepted lane that then loses the race still
                // counts, and the epilogue's `if (r25)` therefore still runs.
                lResult.mbHit = true;
                ++lResult.miAccepted;

                // t = tnum / det   (0x82646A0C vrefp + TWO Newton steps + 0x82646A3C vmulfp).
                // ⚠️ PC LOWERING, FLAGGED: a divide, same precedent/reason as the rsqrt below.
                // `det > 1e-8` is already established on this path, so the divisor is non-zero.
                // ⚠️ `tnum <= hi` above clamped t to ~1: THE PROBE IS A SEGMENT, NOT A RAY -- the
                // nose test does not find a wall beyond the end of its own 2 m reach.
                const f32 lfT = lfTNum / lfDet;

                if (!(lfT >= lResult.mfNearestT))
                {
                    // The winning triangle's unit face normal (0x82646750..0x826467B0 for lane 0,
                    // then three more copies). The console computes all four normals up front,
                    // unconditionally; computing the winner's here is the same number, because a
                    // lane can only be selected after it was ACCEPTED, and an accepted lane has
                    // det > 1e-8 and therefore a non-degenerate cross product.
                    //   a = P0 - P1 ; b = P0 - P2 ; c = cross(a, b)   [two `vpermwi128 ..., 0x63`
                    //   yzxw swizzles folded with `vnmsubfp`] ; N = c * rsqrt(dot(c,c))
                    // cross(P0-P1, P0-P2) == cross(e1, e2), i.e. the front face is the det > 0 one.
                    // ⚠️ PC LOWERING, FLAGGED: `vrsqrtefp` + 2 Newton steps is a ~23-bit reciprocal
                    // square root; `1/sqrt()` here is exact to the last ulp. Same precedent and
                    // same wording as CgsTriangle4.cpp / ContactGeneratorJob.cpp.
                    // ⚠️ The console's dot is `vmsum4fp128` (four components). The transpose sets
                    // the w lane of both vertex differences to 1.0f - 1.0f == 0, so c.w is 0 and
                    // the three-component dot below is the same number.
                    const f32 lfAx = lfP0x - (&lrBlock.mVertex1X.x)[liLane];
                    const f32 lfAy = lfP0y - (&lrBlock.mVertex1Y.x)[liLane];
                    const f32 lfAz = lfP0z - (&lrBlock.mVertex1Z.x)[liLane];
                    const f32 lfBx = lfP0x - (&lrBlock.mVertex2X.x)[liLane];
                    const f32 lfBy = lfP0y - (&lrBlock.mVertex2Y.x)[liLane];
                    const f32 lfBz = lfP0z - (&lrBlock.mVertex2Z.x)[liLane];

                    const f32 lfCx = (lfAy * lfBz) - (lfAz * lfBy);
                    const f32 lfCy = (lfAz * lfBx) - (lfAx * lfBz);
                    const f32 lfCz = (lfAx * lfBy) - (lfAy * lfBx);

                    const f32 lfLenSq  = (lfCx * lfCx) + (lfCy * lfCy) + (lfCz * lfCz);
                    const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);

                    lResult.mfNearestT = lfT;
                    lResult.mNormal.x  = lfCx * lfInvLen;
                    lResult.mNormal.y  = lfCy * lfInvLen;
                    lResult.mNormal.z  = lfCz * lfInvLen;
                    lResult.mNormal.w  = 0.0f;
                    // Carried in v57 across the whole cascade and read by NOTHING on this path --
                    // the epilogue only ever touches v58. Reproduced because the console does it
                    // and because the [noslowmo] probe reports it; do not "clean it up".
                    lResult.muSurfaceTag = LaneBits(lrBlock.mSurfaceTags, liLane);
                }
            }
        }

        return lResult;
    }
}

namespace BrnPhysics
{
namespace Vehicle
{

// -------------------------------------------------------------------------------------------
// DoCrashPrediction  @0x82645FE0 (814)  -- DWARF BrnVehicleManager.h:341
// -------------------------------------------------------------------------------------------
void VehicleManager::DoCrashPrediction(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    f32 lfTimeStep,
    const VehicleInputInterface* lpInputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface)
{
    typedef CgsSceneManager::SceneManagerIO::PotentialContact                       PotentialContact;
    typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

    // The eight leading asserts, BrnVehicleManager.cpp:2880..:2887, in console order.
    CGS_ASSERT(lpInputBufferStack != nullptr,       "lpInputBufferStack != NULL");             // :2880
    CGS_ASSERT(lpOutputBufferStack != nullptr,      "lpOutputBufferStack != NULL");            // :2881
    CGS_ASSERT(lpInputInterface != nullptr,         "lpInputInterface != NULL");               // :2882
    CGS_ASSERT(lpVehicleOutputInterface != nullptr, "lpVehicleOutputInterface != NULL");       // :2883
    CGS_ASSERT(lpRequestOutputInterface != nullptr, "lpRequestOutputInterface != NULL");       // :2884
    CGS_ASSERT(lpManagerOutputInterface != nullptr, "lpVehicleManagerOutputInterface != NULL");// :2885
    CGS_ASSERT(lpDeformationInterface != nullptr,   "lpDeformationInterface != NULL");         // :2886
    CGS_ASSERT(lpContactInterface != nullptr,       "lpPotentialContactQueue != NULL");        // :2887

    // `stwx r30, r15, r11` x2 @0x82646180/84 -> +172416 / +172420, the car-car prediction cache.
    muCachedCarASlot = 0;
    muCachedCarBSlot = 0;

    mPhysicalTrafficManager.AllocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);

    // v149 @ sp+0x150 (1680 bytes) + v150 @ sp+0x7E0 == the averager; `stw r30, var_B0` is its
    // count = 0. Constructed per frame, on the stack, exactly as the console does.
    PotentialContactAverager lContactPairAverager;
    lContactPairAverager.Reset();

    // `stwx r30, r15, 0x273A8` -> +160680 == mDiscardedContacts.miLength.
    mDiscardedContacts.Clear();

    // ---- (1) traffic-vs-traffic, custom queue [13] -----------------------------------------
    // Owner bytes: A at record +0x30, B at +0x38 (HIBYTE on a big-endian target is byte 0).
    // 0 = world/scene, 1 = race car, 2 = traffic vehicle.
    {
        const Queue& lrQueue = lpContactInterface->GetTrafficWithTrafficQueue();
        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact& lContact = lrQueue.GetEvent(liIndex);
            const u8 lu8OwnerA = lContact.muVolumeInstanceIdA.GetEntityIDOwner();
            const u8 lu8OwnerB = lContact.muVolumeInstanceIdB.GetEntityIDOwner();

            if (lu8OwnerA == 0u)
            {
                CGS_ASSERT(lu8OwnerB != 1u,
                           "Found a race car-world potential contact outside of race car-world queue");  // :2911
                CGS_ASSERT(lu8OwnerB != 2u,
                           "Found a traffic-world potential contact outside of traffic-world queue");    // :2912
            }

            // 0x826462FC..0x8264630C: both owners TRAFFIC_VEHICLE -> the traffic-traffic arm, with
            // the 80-byte record copied to the outgoing-arg area (r4..r10 + the 24-byte memcpy).
            // GATE DISCHARGED 2026-09-02 (traffic crash wave): bodied in
            // BrnVehicleManager_TrafficCrashArms.cpp.
            if (lu8OwnerA == 2u && lu8OwnerB == 2u)
            {
                HandleTrafficCarTrafficCarPotentialContact(
                    lContact, lpRequestOutputInterface, lpVehicleOutputInterface,
                    lpManagerOutputInterface, lpDeformationInterface, lfTimeStep);
            }
        }
    }

    // ---- (2) race-car-vs-PHYSICAL-traffic, custom queue [8] ---------------------------------
    // THE QUEUE RECORDS STILL HOLD GLOBAL ENTITY IDS ON BOTH SIDES.
    // FixUpVehicleContacts splices the physical id into a LOCAL VolumeInstanceId and passes it to
    // the DeformationManager (BrnPhysicsModuleUpdateFunctions.cpp:110-119); it never writes the id
    // back into the record (the console's @0x825A6010 has no `std` into the queue storage, only
    // two stack slots). That is why the consumer does its own lookup --
    // HandleRaceCarTrafficCarPotentialContact @0x8263FA50 calls
    // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe on the record's B id
    // (BrnVehicleManager_RaceCarTrafficContact.cpp:415-428).
    // The console re-reads miLength every iteration (`lwz r11, 8(r29)` @0x826463FC) and copies the
    // 80-byte record to the stack before the call; the by-value local reproduces both.
    {
        const Queue& lrQueue = lpContactInterface->GetRaceCarWithTrafficQueue();

        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact lContact = lrQueue.GetEvent(liIndex);   // 80-byte stack copy
            DoCrashPredictionForRaceCarAndTrafficVehicle(
                &lContactPairAverager, &lContact, lfTimeStep,
                lpVehicleOutputInterface, lpRequestOutputInterface,
                lpManagerOutputInterface, lpDeformationInterface);
        }
    }

    // Final flush of whatever the loop accumulated (@0x82646428).
    HandleCrashPredictionForRaceCarAndTrafficVehicle(
        &lContactPairAverager, lfTimeStep,
        lpVehicleOutputInterface, lpRequestOutputInterface,
        lpManagerOutputInterface, lpDeformationInterface);

    // ---- (2b) race-car-vs-WORLD (@0x82646428+): the validated queue [6], grouped per car and
    //      ordered by predicted impact time, then classified + committed contact by contact.
    // GATE DISCHARGED 2026-09-02 (crash wave). The DELETE-WHEN that stood here ("both callees are
    // declare-only") is met: HandleRaceCarWorldPotentialContact @0x8263E3B8 and
    // PredictCarWorldContactTime @0x825B5300 are bodied in BrnVehicleManager_WorldCrashArm.cpp and
    // BrnVehicleManagerCrashPrediction.cpp is mounted. Argument order is the console's
    // (timestep, contactIfc, inputIfc, vehicleOut, requestOut, managerOut, deformIn); the
    // GameStateModuleIO fork spelling on arg 4 was retired with it (BrnVehicleManager.h banner).
    // THIS IS THE LINE THAT MAKES HITTING A WALL ACTUALLY CRASH THE CAR.
    HandleCrashPredictionForRaceCarAndWorld(
        lfTimeStep, lpContactInterface, lpInputInterface,
        lpVehicleOutputInterface, lpRequestOutputInterface,
        lpManagerOutputInterface, lpDeformationInterface);

    // ---- (3) the force-no-slow-mo clear (0x82646450..0x82646B8C) ---------------------------
    // ⭐⭐ LANDED 2026-09-06. FOUR waves parked this 463-instruction VMX128 block, the last one
    // after verifying every one of its constants byte-exact and declining on VERIFICATION
    // grounds: "a 4-wide SoA ray/triangle test silently answers 'no hit' exactly like the absent
    // gate, so it needs its own attributable run." This is that run -- nothing else lands in this
    // subsystem beside it, and the [noslowmo] probe reports the REACH counters as well as the
    // verdict so that a quiet log cannot be misread as a pass.
    //
    // WHAT IT DOES, in one sentence: once a traffic contact has set mbForceNoSlowMo -- which
    // SUPPRESSES the 0.001x super-slow-motion (BrnPhysicsModuleUpdateFunctions.cpp:520) -- cast a
    // 2 m segment straight out of the player's nose against the player's OWN cached collision
    // triangles, and if the nearest hit faces back at the car within ~45.6 degrees, CLEAR the
    // flag. The car is about to bury itself in a wall, so the console lets the slow-motion play.
    //
    // THE RAY, straight off 0x826465FC..0x8264662C, reading `vmaddfp vD,vA,vB,vC` as vA*vC + vB
    // (raw field order -- neither natural reading of the mnemonic is right):
    //     origin = mTransform.Pos() + mTransform.At() * (mHalfExtent.z * 0.75)
    //     end    = origin + mTransform.At() * 2.0
    //     delta  = end - origin                      (`vsubfp v12, v12, v13` @0x8264662C)
    // The console really does build `end` and subtract `origin` back off it; that is the source's
    // own start/end pair, and it is kept here because the kernel below is a SEGMENT test.
    //
    // THE THREE OFFSETS ARE NAMED, not poked. `this + index*0x1460 + 0x770 / 0x780 / 0xDE0` is
    // maRaceCarVehicles[index] (array base +0x740, stride 0x1460) at member +0x30 / +0x40 / +0x6A0
    // == mTransform.zAxis (At), mTransform.wAxis (Pos) and SimpleVehiclePhysics::mHalfExtent,
    // whose lane .z the console splats (`vspltw v0, v0, 2` @0x8264660C -- the same lane both boost
    // appliers take off +0x6A0). The +0x740 base is pinned inside this very block: the guard reads
    // +0xE53, and mbCrashedThisFrame is +0x713.
    //
    // ⭐ EVERY CONSTANT IS IMAGE-ATTESTED. The two thresholds live in .bss slots that a LAZY
    // FIRST-CALL CACHE fills -- a function-local `static` with a guard word, NOT a CRT thunk --
    // which is why a static-init sweep reports them as "no writer found":
    //     unk_82FBA360 <- lazy cache 0x8264656C <- flt_82004018 == 0.75  (guard dword_82FBA370 b0)
    //     unk_82FBA350 <- lazy cache 0x82646B08 <- flt_82004C68 == 0.7   (guard dword_82FBA370 b1)
    // The 2.0 and the 0.5/1.0 Newton constants are immediates, not rodata: `vcfsx(vspltisw 2, 0)`
    // @0x826465AC/BC, `vcfsx(vspltisw 1, 1)` @0x82646658, `vcfsx(vspltisw 1, 0)` @0x826466BC.
    // The kernel's 1e-8 / 1e-5 / FLT_MAX are cited at their seats above.
    //
    // ⭐⭐ MEASURED, TWO RUNS, 240 s each, teleport 2958,12.5,-1764 heading 90 with a 0.6 s
    // steering pulse every 4 s. THE WEAVE IS THE POINT: the latch upstream needs a corner
    // CLIP (IsFrontCornerClip on both cars), so a dead-straight drive into traffic can never
    // reach this gate at all, and a run that does not weave measures nothing.
    //   run 1, exe 5eff9a427a9e:  calls 13200  crashed 1  forced 1  entered 1  batchesSeen 12
    //                             lanesAccepted 1  anyHit 1  cleared 1
    //     [noslowmo] ENTER batches 12  accepted 1  hit 1  t 0.451133  dot 0.957111  -> CLEARED
    //   run 2, exe 1239d22e4310:  calls 13200  crashed 3  forced 1  entered 1  batchesSeen 12
    //     [noslowmo] ENTER batches 12  accepted 1  hit 1  t 0.491190  dot 0.925138  -> CLEARED
    //     [slomo]    crashArm  crashed 1  inhibited 0 -> armed 1  framesLeft 2
    //                scaledThisFrame 1  step 0.000017            <- 0.016667 * 0.001
    // In BOTH runs the gate fired on the frame the crash record opened -- "[crash-exit] OPENED
    // crash record for active race car 0" is the very next log line, at 62.5 m/s -- and on the
    // SAME surface, tag 2216724827 both times. t 0.45/0.49 puts the wall ~0.9 m past the nose
    // point and dot 0.93/0.96 is 17-22 degrees off head-on, well inside the 0.7 the console
    // asks for. The two runs do not agree to the last digit because the sim is frame-coupled on
    // this box; they agree on the geometry, which is the claim.
    //
    // ⭐ THE COUNTERFACTUAL IS IN-FRAME, not an A/B of two builds -- the same discipline as the
    // [wheellock] counter, and for the same reason (two runs of one build have driven 1,643 m
    // and 305 m on this box, so a cross-run delta prices the box). "entered" can only increment
    // on a frame where mbForceNoSlowMo was ALREADY TRUE, since that is half the gate; and
    // nothing writes the flag between here and the consumer, because ResetForceNoSlowMo runs
    // AFTER it (BrnPhysicsModuleUpdateFunctions.cpp, the super-slow-motion latch). So on that
    // frame the pre-gate value was 1 and the consumer read 0: without this block the same crash
    // prints "inhibited 1 -> armed 0" and the 0.001x window never opens.
    // ⇒ THE BEHAVIOUR CHANGE THE OWNER SEES: a corner-clip traffic crash with a wall in front
    // now gets the super-slow-motion it did not get before.
    // ⚠️ Stated plainly, because a verification nobody has seen FAIL is not a verification: the
    // "inhibited 1 -> armed 0" line has NOT been observed in a log, and it cannot be while this
    // block is landed -- the only frame that could print it is the frame the block cleared. The
    // consumer witness prints its INPUT as well as its verdict precisely so that a build without
    // this gate would show it, and so that a reader can tell the two apart.
    //
    // ⚠️ WHAT THIS BODY CANNOT DECIDE, stated so nobody over-reads the run: the kernel reproduces
    // the console's arithmetic, but whether the PLAYER'S CACHE SLOT IS POPULATED at the moment of
    // a traffic crash is a property of the triangle-cache producer, not of this block. The probe
    // therefore prints the batch count on every entry: `batches 0` means the gate was reached and
    // had nothing to test, which is a DIFFERENT fact from "the test said no hit".
    {
        const s32 liPlayerCar = static_cast<s32>(mePlayerActiveRaceCarIndex);

        // The two guards, 0x82646458..0x82646480. Either one false skips the whole block
        // (`beq cr6, loc_82646B90`), landing past it on queue [9] below.
        const bool lbCrashedThisFrame = maRaceCarVehicles[liPlayerCar].HasCrashedThisFrame();
        const bool lbEntered          = lbCrashedThisFrame && mbForceNoSlowMo;

        s32 liNumBatches = 0;
        NearestCachedTriangleHit lHit;
        lHit.mbHit        = false;
        lHit.mfNearestT   = KF_NO_HIT_DISTANCE;
        lHit.mNormal.SetZero();
        lHit.muSurfaceTag = 0u;
        lHit.miAccepted   = 0;
        f32  lfUprightDot = 0.0f;
        bool lbCleared    = false;

        if (lbEntered)
        {
            // The console reaches the interface as a raw `lpInputInterface + 0x1F410` and inlines
            // both accessors, baking their asserts at CgsSceneManagerModuleIO.h:0x506 (GetCache,
            // @0x826464A8) and :0x50F (GetNumCachedTriangleBatches, @0x82646524) plus the manager's
            // own "mpaTriangleCache != NULL" at :0x99 (@0x826464E8). Calling them by name restores
            // all three; the per-car CacheSlot stride 48 and the +0x24 / +0x28 fields the asm walks
            // are exactly what those two accessors do.
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCache =
                lpInputInterface->GetTriangleCacheInterface();

            const CgsGeometric::Triangle4* lpaBatches = lpTriangleCache->GetCache(liPlayerCar);
            liNumBatches = lpTriangleCache->GetNumCachedTriangleBatches(liPlayerCar);

            const RaceCarPhysics& lrPlayerCar = maRaceCarVehicles[liPlayerCar];
            const Matrix44Affine  lTransform  = lrPlayerCar.GetTransform();
            const Vector3         lvAt        = lTransform.At();     // +0x30
            const Vector3         lvPos       = lTransform.Pos();    // +0x40
            const Vector3         lvHalfExtent = lrPlayerCar.GetHalfExtent();  // +0x6A0

            const f32 lfNoseOffset = lvHalfExtent.z * KF_NO_SLOW_MO_NOSE_FRACTION;

            Vector3 lvOrigin;
            lvOrigin.x = lvPos.x + lvAt.x * lfNoseOffset;
            lvOrigin.y = lvPos.y + lvAt.y * lfNoseOffset;
            lvOrigin.z = lvPos.z + lvAt.z * lfNoseOffset;
            lvOrigin.w = 0.0f;

            Vector3 lvDelta;
            lvDelta.x = lvAt.x * KF_NO_SLOW_MO_RAY_LENGTH;
            lvDelta.y = lvAt.y * KF_NO_SLOW_MO_RAY_LENGTH;
            lvDelta.z = lvAt.z * KF_NO_SLOW_MO_RAY_LENGTH;
            lvDelta.w = 0.0f;

            // `ble cr6, loc_82646AFC` @0x82646624 -- a zero-batch slot skips straight to the
            // epilogue, where the any-hit byte is still 0 and nothing is cleared.
            if (liNumBatches > 0)
            {
                lHit = FindNearestCachedTriangleHit(lpaBatches, liNumBatches, lvOrigin, lvDelta);
            }

            // The epilogue, 0x82646B3C..0x82646B84. `vxor128 v0, v90, v0` negates the carried
            // normal (v90 is v58 after IDA's vA high-bit swap), `vmsum3fp128` dots it against the
            // car's forward axis, and `vcmpgtfp.` + the CR6 "all lanes" bit is a strict >.
            if (lHit.mbHit)
            {
                lfUprightDot = (-lHit.mNormal.x) * lvAt.x
                             + (-lHit.mNormal.y) * lvAt.y
                             + (-lHit.mNormal.z) * lvAt.z;

                if (lfUprightDot > KF_NO_SLOW_MO_UPRIGHT_DOT)
                {
                    mbForceNoSlowMo = false;   // `stb r30, 0(r22)`, r30 == 0  @0x82646B84
                    lbCleared = true;
                }
            }
        }

        // ---- [noslowmo] the attributable measurement --------------------------------------
        // OPT-IN via the EXISTING BRN_SLOMO_DIAG (already in flow_run.ps1's DEFAULT-run clear
        // list, and already the name for "where the slow-motion decision comes from" in
        // BrnMainDirector.cpp / BrnGameModule.cpp). Reusing it is deliberate: registering a NEW
        // engine getenv means editing flow_run.ps1 in the same commit, and that file is another
        // wave's dirty file this week -- the exact situation that cost f92879f1 its instrument.
        //
        // ⭐ IT REPORTS REACH, NOT JUST VERDICT. A block guarded by "crashed this frame AND the
        // traffic arm asked for no slow-mo" can be silent for four different reasons, and a
        // ray/triangle test that silently answers "no hit" is indistinguishable from the gate
        // being absent -- which is precisely why three waves declined to land it without a run.
        // The periodic line separates all of them: calls / crashed / forced / entered are the
        // ladder down to the gate, batches is whether there was anything to test, accepted is
        // whether the kernel ever accepted a lane, and cleared is the verdict.
        {
            static s32 siSlowMoDiag = -1;
            if (siSlowMoDiag < 0)
            {
                const char* lpcEnv = getenv("BRN_SLOMO_DIAG");
                siSlowMoDiag = (lpcEnv != 0 && lpcEnv[0] != '0') ? 1 : 0;
            }
            if (siSlowMoDiag == 1 && CgsDev::Log::gpDebugPrint != 0)
            {
                static u32 suCalls = 0, suCrashed = 0, suForced = 0, suEntered = 0;
                static u32 suBatchesSeen = 0, suAccepted = 0, suAnyHit = 0, suCleared = 0;
                ++suCalls;
                if (lbCrashedThisFrame) ++suCrashed;
                if (mbForceNoSlowMo || lbCleared) ++suForced;
                if (lbEntered)
                {
                    ++suEntered;
                    suBatchesSeen += static_cast<u32>(liNumBatches);
                    suAccepted    += static_cast<u32>(lHit.miAccepted);
                    if (lHit.mbHit) ++suAnyHit;
                    if (lbCleared)  ++suCleared;

                    // Every entry gets its own line -- the gate fires a handful of times per
                    // crash, so this cannot storm the harness the way a per-frame probe would.
                    *CgsDev::Log::gpDebugPrint
                        << "[noslowmo] ENTER batches " << liNumBatches
                        << "  accepted " << lHit.miAccepted
                        << "  hit "      << static_cast<s32>(lHit.mbHit ? 1 : 0)
                        << "  t "        << lHit.mfNearestT
                        << "  dot "      << lfUprightDot
                        << "  tag "      << lHit.muSurfaceTag
                        << "  -> "       << (lbCleared ? "CLEARED (slow-mo allowed)"
                                                       : "kept (slow-mo suppressed)")
                        << "\n";
                }
                if ((suCalls % 600u) == 0u)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[noslowmo] calls "  << static_cast<s32>(suCalls)
                        << "  crashed "         << static_cast<s32>(suCrashed)
                        << "  forced "          << static_cast<s32>(suForced)
                        << "  entered "         << static_cast<s32>(suEntered)
                        << "  batchesSeen "     << static_cast<s32>(suBatchesSeen)
                        << "  lanesAccepted "   << static_cast<s32>(suAccepted)
                        << "  anyHit "          << static_cast<s32>(suAnyHit)
                        << "  cleared "         << static_cast<s32>(suCleared)
                        << "\n";
                }
            }
        }
    }

    // ---- (4) traffic-vs-world, custom queue [9] (0x82646B8C..0x82646C08) ----------------------
    // GATE DISCHARGED 2026-09-02 (traffic crash wave): HandleTrafficCarWorldPotentialContact
    // @0x8263F0F0 is bodied in BrnVehicleManager_TrafficCrashArms.cpp. The console re-reads
    // miLength every iteration (`lwz r11, 8(r29)` @0x82646BFC) and copies the record to the
    // outgoing-arg area before each call; the by-value local reproduces both. NOTE: on the console
    // this arm commits NOTHING (no store leaves its stack frame) -- traffic knocked into scenery is
    // not crashed here; see the TU banner before reading this loop as a "wall crash" for traffic.
    {
        const Queue& lrQueue = lpContactInterface->GetTrafficWithWorldQueue();
        for (s32 liIndex = 0; liIndex < lrQueue.GetLength(); ++liIndex)
        {
            const PotentialContact lContact = lrQueue.GetEvent(liIndex);
            HandleTrafficCarWorldPotentialContact(
                lContact, lpRequestOutputInterface, lpVehicleOutputInterface,
                lpManagerOutputInterface, lpDeformationInterface, lfTimeStep);
        }
    }

    // ---- tail --------------------------------------------------------------------------------
    // The console inlines BridgeArticulatedJointRequestsToSim here; its two asserts are the ones
    // Hex-Rays cites at BrnPhysicalTrafficManager.h:880/881.
    mPhysicalTrafficManager.BridgeArticulatedJointRequestsToSim(lpRequestOutputInterface);
    mPhysicalTrafficManager.DeallocateInternalBuffers(lpInputBufferStack, lpOutputBufferStack);
}

}   // namespace Vehicle
}   // namespace BrnPhysics
