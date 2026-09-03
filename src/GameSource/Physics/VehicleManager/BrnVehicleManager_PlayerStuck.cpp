// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_PlayerStuck.cpp
//
// THE PLAYER-STUCK LEG of the traction-line chain, plus the two PLAYER-ONLY line-test consumers of
// UpdateVehiclePhysicsPostSimulation. Six console bodies; every one of them stood as a log-once
// conductor gate in BrnPhysicsConductorGates.cpp until 2026-09-03 (aiwave lane P2b) and those six
// gates are DELETED with this file (LNK2005 is the tripwire if one reappears):
//   VehicleManager::UpdatePlayerStuckInCollisionTest        @0x825E9DD8   (87 insns)
//   VehicleManager::UpdatePlayerStuckInCollisionSpheres     @0x825C4AB8  (147)
//   VehicleManager::AddPlayerStuckInCollisionLineTests      @0x825E9B28  (171)
//   VehicleManager::ReadPlayerStuckTractionLineTestResults  @0x825C3898  (118)
//   VehicleManager::DoPlayerTractionLineTestsPostSimulation @0x826185A0  (548)
//   VehicleManager::DoPlayerStuckLineTests                  @0x825C3A70 (1041)
//
// WHAT THE LEG DOES (per frame, player car only):
//   Start half (StartVehicleTractionLineTests @0x82629CE0, already real):
//     UpdatePlayerStuckInCollisionTest -> UpdatePlayerStuckInCollisionSpheres keeps a 2 m cache
//     sphere on the car. Leave the sphere: re-centre it and re-arm the 5 s timer. Stay inside it:
//     the timer counts down. Only a car that has sat inside the sphere for 5 s posts the
//     stuck-in-collision command (AddPlayerStuckInCollisionLineTests): four lines between the
//     deformable model's wheel tag points, FL<->RR and FR<->RL, both directions each.
//   End half (EndVehicleTractionLineTests @0x82633CD8, already real):
//     ReadPlayerStuckTractionLineTestResults harvests that one record: if BOTH directions of a
//     diagonal hit world geometry the car is wedged -> mbPlayerCarStuckInCollision = true, which
//     WriteOutVehicleStats @0x8263F460 already turns into a forced reset request (the same
//     place-on-track chain the harness's BRN_CAR_TELEPORT uses). The timer re-arms to 5 s.
//   Post-simulation (UpdateVehiclePhysicsPostSimulation @0x826426E0, already real):
//     DoPlayerTractionLineTestsPostSimulation re-runs the player's four wheel traction lines ON THE
//     PPU against the player's triangle-cache window AFTER the rigid-body step (the SPU harvest fed
//     AddTractionPoint with pre-step geometry), then CalculateNewWheelPlane + StoreLocalWheelPositions.
//     DoPlayerStuckLineTests runs ONE occlusion line per frame round-robin (front plane, rear plane,
//     front sensor) and derives mbIsWedgedInWorld / mbIsFrontRayOccluded, both of which
//     UpdateRaceCarState @0x825EC808 already publishes.
//
// THE LINE-vs-TRIANGLE KERNEL. Two of the six (PostSimulation, DoPlayerStuckLineTests) inline the
// SAME VMX128 Moller-Trumbore-over-Triangle4 kernel that ContactGeneratorJob::
// ExecuteLineWithTriangleListStream @0x82921968 inlines (same vperm/vsldoi epsilon mix, same
// vsel nearest cascade, same mValidMasks AND, same `!(t >= best)` lower-lane-wins select). It is
// reconstructed ONCE here as a file-local function in the same scalar shape as that TU's kernel
// (GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp:594..741) so the three copies agree
// number for number. It is not a shared symbol on the console (each TU has its own inlined copy),
// so a second scalar body is the faithful shape, not a duplicate.
//
// THE EPSILONS ARE NOT ZERO -- A FINDING THAT ALSO CORRECTS ContactGeneratorJob.cpp.
// This TU's two kernel statics live in .bss (unk_82FB9F10 / unk_82FB9EF0, all-zero in the image)
// and are written by DYNAMIC INITIALISERS that sit in an IDA export hole, decoded from the image
// this wave (capstone, big-endian, VA-0x82000000):
//   0x82C5B650  lfs f0, flt_8200D5F0 ; vspltw ; stvx128 -> 0x82FB9F10   == 1.0e-8f  (min determinant)
//   0x82C5B678  lfs f0, flt_82004884 ; vspltw ; stvx128 -> 0x82FB9EF0   == 1.0e-5f  (barycentric tol)
//   0x82C5BA28  lfs f0, flt_82001D9C ; vspltw ; stvx128 -> 0x82FB8FE0   == 2.0f
//               == KVF_STUCK_IN_COLLISION_CACHE_SPHERE_RADIUS (DWARF BrnVehicleManager.cpp:297)
// The job worker's OWN copies (unk_8321D330 / unk_8321D310, which ContactGeneratorJob.cpp carries as
// "KF_MIN_DETERMINANT = 0.0f / KF_BARYCENTRIC_TOLERANCE = 0.0f" on the strength of "no exported
// function writes them") are written the same way from the SAME two rodata floats:
//   0x82C71D50  lfs f0, flt_8200D5F0 -> 0x8321D330   == 1.0e-8f
//   0x82C71D78  lfs f0, flt_82004884 -> 0x8321D310   == 1.0e-5f
// That TU is not this lane's file; the correction is reported to the conductor, not applied here.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"        // GetTriangleCacheInterface
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // EVehicleDrivenWheel, GetTractionLine
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"                     // Wheel::GetPosition
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                // GetPlayerModelIndex / GetPlayerCarModel
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // GetWheelTagPoints

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithTriangleListStreamJobDesc.h"  // StreamCommand / StreamResult
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"   // CheckAlignment / ValidateTriangles
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"        // TriangleCacheInterface
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                   // Triangle4 (the SoA block)
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"       // AllocateCommand / SimpleDataStreamResultIterator
#include "SharedClasses/World/BrnCollisionTag.h"                                        // KU_COLLISION_FLAG_DRIVEABLE
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT

#include <cmath>     // std::sqrt (the vrsqrtefp/vrefp lowering, see the kernel note)
#include <cstring>   // std::memcpy (lane reads; the raw-byte sphere seat)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    typedef CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc LineDesc;

    // ---- constants --------------------------------------------------------------------------

    // DWARF BrnVehicleManager.cpp:297 `const VecFloat KVF_STUCK_IN_COLLISION_CACHE_SPHERE_RADIUS`.
    // Seat unk_82FB8FE0 (.bss); dyn-init @0x82C5BA28 splats flt_82001D9C == 0x40000000 == 2.0f.
    // Read by UpdatePlayerStuckInCollisionSpheres @0x825C4C58 (`lvx128 v13, r0, r11` then
    // `vrlimi128 v0, v13, 1, 0` -- only the w lane is ever used, as the sphere's radius).
    const f32 KF_STUCK_IN_COLLISION_CACHE_SPHERE_RADIUS = 2.0f;

    // DWARF VehiclePhysics.h:52 `const float32_t KF_STUCK_IN_COLLISION_TEST_INTERVAL`; the value is
    // flt_8200426C == 0x40A00000 == 5.0f at every re-arm site (0x825C4CD4 / 0x825C3A60) and in
    // VehiclePhysics::Reset (VehiclePhysics.cpp:1380 carries the same literal). File-local here
    // because the tree has no out-of-line definition of the DWARF's symbol yet.
    const f32 KF_STUCK_IN_COLLISION_TEST_INTERVAL = 5.0f;

    // DWARF BrnVehicleConstants.h:42 `const int32_t kiNumStuckInCollisionLineTests = 4` -- the
    // `li r3, 4 ; stw r3, 0xA8(cmd)` at 0x825E9D44/0x825E9D80 (miNumLines) and the four-line
    // harvest in ReadPlayerStuckTractionLineTestResults.
    const s32 KI_NUM_STUCK_IN_COLLISION_LINE_TESTS = 4;

    // The two kernel epsilons of THIS TU (see the banner: .bss seats + their dyn-init writers).
    //   unk_82FB9F10 <- flt_8200D5F0 == 0x322BCC77 == 9.99999994e-09  (det must exceed it)
    //   unk_82FB9EF0 <- flt_82004884 == 0x3727C5AC == 9.99999975e-06  (barycentric slack, x det)
    const f32 KF_STUCK_LINE_MIN_DETERMINANT       = 9.99999994e-09f;
    const f32 KF_STUCK_LINE_BARYCENTRIC_TOLERANCE = 9.99999975e-06f;

    // flt_8208F5EC == 0x7F7FFFFF: the "no hit yet" distance the per-line best-t is seeded with
    // (0x82618614 in PostSimulation, 0x825C3A98 in DoPlayerStuckLineTests).
    const f32 KF_NO_HIT_DISTANCE = 3.4028235e38f;

    // DoPlayerStuckLineTests' two "is the car moving" cut-offs, compared against SQUARED
    // magnitudes (`vmsum3fp128 v0, v0, v0` then `vcmpgtfp.`):
    //   flt_82093CE4 == 0x3CECBFB1 == 0.0289f == 0.17^2  vs |angular velocity|^2  (0x825C3A98)
    //   flt_82001C98 == 0x3F800000 == 1.0f    == 1.0^2   vs |linear velocity|^2   (0x825C3B30)
    // The DWARF names for the unsquared pair (KF_STUCK_LINETEST_ANGULARCUTOFF /
    // KF_STUCK_LINETEST_LINEARCUTOFF) belong to another TU's dump; these are the squared literals
    // this body actually loads.
    const f32 KF_STUCK_LINETEST_ANGULARCUTOFF_SQ = 0.0289f;
    const f32 KF_STUCK_LINETEST_LINEARCUTOFF_SQ  = 1.0f;

    // flt_82001DA0 == 0.5f: the wheel-pair midpoint factor (0x825C3BC8 / 0x825C3EA0 / 0x825C4300).
    const f32 KF_STUCK_LINE_MIDPOINT = 0.5f;

    // ---- lane helpers (the same two ContactGeneratorJob.cpp uses, same reasons) ----------------

    // Triangle4::mValidMasks (+0x90), ANDed into the hit mask (`vand v12, v12, v6` @0x82618CB4 with
    // v6 == lvx128 base+0x90; `vand v11, v11, v7` @0x825C48F0 likewise). Non-zero == real triangle.
    inline bool IsLaneEnabled(const Vector4& lrMasks, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrMasks.x)[liLane], sizeof(u32));
        return luBits != 0u;
    }

    // Triangle4::mSurfaceTags (+0xA0) travels as a float register and is only ever vsel'd /
    // vspltw'd, never touched by an FP op -- read as bits so a NaN-patterned tag survives.
    inline u32 LaneBits(const Vector4& lrV, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrV.x)[liLane], sizeof(u32));
        return luBits;
    }

    // ---- the kernel -----------------------------------------------------------------------------

    struct LineTestHit
    {
        bool    mbHit;        // any lane of any block accepted (the console's per-line byte)
        f32     mfBestT;      // nearest accepted t along (end - start), seeded KF_NO_HIT_DISTANCE
        Vector4 mNormal;      // unit face normal of the nearest accepted triangle
        u32     muSurfaceTag; // Triangle4::mSurfaceTags lane of the nearest accepted triangle
    };

    // Segment-vs-Triangle4-list, N lines at once, blocks outer / lines inner -- the console's own
    // loop nesting (PostSimulation: 0x826188B0 block loop, 0x82618A9C line loop; DoPlayerStuckLine-
    // Tests: single line, 0x825C4498 block loop). Per block the four unit face normals are built
    // once (cross(P0-P1, P0-P2), vrsqrtefp + TWO NR steps), then each line runs Moller-Trumbore
    // against all four lanes with the acceptance test in the console's own shape:
    //     det > eps0 ; u >= lo ; !(u > hi) ; v >= lo ; !((u+v) > hi) ; tnum >= lo ; !(tnum > hi)
    //     with lo = (-det) * eps1, hi = det - lo
    // `tnum <= det` clamps t to the SEGMENT -- a probe finds nothing past its end.
    // Upper bounds are `!(x > hi)`, NOT `x <= hi`: vcmpgtfp is false for NaN and vnot makes it
    //    true, so NaN is accepted by the upper bounds and rejected by the lower ones. As shipped.
    // The hit byte is set BEFORE the nearest-select (`stb 1` @0x82618C14 / @0x825C4838 sits above
    //    the vsel cascade): an accepted lane that loses the distance race still counts as a hit.
    // The nearest-select is a lane 0->3 vsel cascade on `!(t >= best)`: on a tie the LOWER lane
    //    wins, which iterating lanes 0..3 with the same predicate reproduces exactly.
    // PC LOWERING, FLAGGED: vrsqrtefp+2NR and vrefp+2NR are ~23-bit; `1/sqrt` and `/` are exact.
    //    Standing precedent CgsTriangle4.cpp / ContactGeneratorJob.cpp. DELETE-WHEN never (this is
    //    the project's chosen lowering, not a deviation to pay down).
    void IntersectLinesWithTriangleBatches(const Vector3* lpaStart, const Vector3* lpaEnd,
                                           s32 liNumLines,
                                           const CgsGeometric::Triangle4* lpaTriangles,
                                           s32 liNumBatches,
                                           LineTestHit* lpaOut)
    {
        for (s32 liLine = 0; liLine < liNumLines; ++liLine)
        {
            lpaOut[liLine].mbHit        = false;
            lpaOut[liLine].mfBestT      = KF_NO_HIT_DISTANCE;
            lpaOut[liLine].muSurfaceTag = 0u;
            lpaOut[liLine].mNormal.x = 0.0f;
            lpaOut[liLine].mNormal.y = 0.0f;
            lpaOut[liLine].mNormal.z = 0.0f;
            lpaOut[liLine].mNormal.w = 0.0f;
        }

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            const CgsGeometric::Triangle4& lrBlock = lpaTriangles[liBatch];

            Vector4 laNormal[4];
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                const f32 lfAx = (&lrBlock.mVertex0X.x)[liLane] - (&lrBlock.mVertex1X.x)[liLane];
                const f32 lfAy = (&lrBlock.mVertex0Y.x)[liLane] - (&lrBlock.mVertex1Y.x)[liLane];
                const f32 lfAz = (&lrBlock.mVertex0Z.x)[liLane] - (&lrBlock.mVertex1Z.x)[liLane];
                const f32 lfBx = (&lrBlock.mVertex0X.x)[liLane] - (&lrBlock.mVertex2X.x)[liLane];
                const f32 lfBy = (&lrBlock.mVertex0Y.x)[liLane] - (&lrBlock.mVertex2Y.x)[liLane];
                const f32 lfBz = (&lrBlock.mVertex0Z.x)[liLane] - (&lrBlock.mVertex2Z.x)[liLane];

                const f32 lfCx = (lfAy * lfBz) - (lfAz * lfBy);
                const f32 lfCy = (lfAz * lfBx) - (lfAx * lfBz);
                const f32 lfCz = (lfAx * lfBy) - (lfAy * lfBx);

                const f32 lfLenSq  = (lfCx * lfCx) + (lfCy * lfCy) + (lfCz * lfCz);
                const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);   // vrsqrtefp + 2 NR

                laNormal[liLane].x = lfCx * lfInvLen;
                laNormal[liLane].y = lfCy * lfInvLen;
                laNormal[liLane].z = lfCz * lfInvLen;
                laNormal[liLane].w = 0.0f;
            }

            for (s32 liLine = 0; liLine < liNumLines; ++liLine)
            {
                const Vector3& lrStart = lpaStart[liLine];
                const Vector3& lrEnd   = lpaEnd[liLine];

                const f32 lfDx = lrEnd.x - lrStart.x;
                const f32 lfDy = lrEnd.y - lrStart.y;
                const f32 lfDz = lrEnd.z - lrStart.z;

                for (s32 liLane = 0; liLane < 4; ++liLane)
                {
                    const f32 lfP0x = (&lrBlock.mVertex0X.x)[liLane];
                    const f32 lfP0y = (&lrBlock.mVertex0Y.x)[liLane];
                    const f32 lfP0z = (&lrBlock.mVertex0Z.x)[liLane];

                    const f32 lfE1x = (&lrBlock.mVertex1X.x)[liLane] - lfP0x;
                    const f32 lfE1y = (&lrBlock.mVertex1Y.x)[liLane] - lfP0y;
                    const f32 lfE1z = (&lrBlock.mVertex1Z.x)[liLane] - lfP0z;
                    const f32 lfE2x = (&lrBlock.mVertex2X.x)[liLane] - lfP0x;
                    const f32 lfE2y = (&lrBlock.mVertex2Y.x)[liLane] - lfP0y;
                    const f32 lfE2z = (&lrBlock.mVertex2Z.x)[liLane] - lfP0z;

                    // P = cross(dir, e2)
                    const f32 lfPx = (lfE2z * lfDy) - (lfE2y * lfDz);
                    const f32 lfPy = (lfE2x * lfDz) - (lfE2z * lfDx);
                    const f32 lfPz = (lfE2y * lfDx) - (lfE2x * lfDy);

                    // T = start - P0
                    const f32 lfTx = lrStart.x - lfP0x;
                    const f32 lfTy = lrStart.y - lfP0y;
                    const f32 lfTz = lrStart.z - lfP0z;

                    const f32 lfDet = (lfE1x * lfPx) + (lfE1y * lfPy) + (lfE1z * lfPz);
                    const f32 lfU   = (lfTx * lfPx) + (lfTy * lfPy) + (lfTz * lfPz);

                    // Q = cross(T, e1)
                    const f32 lfQx = (lfTy * lfE1z) - (lfTz * lfE1y);
                    const f32 lfQy = (lfTz * lfE1x) - (lfTx * lfE1z);
                    const f32 lfQz = (lfTx * lfE1y) - (lfTy * lfE1x);

                    const f32 lfV    = (lfQx * lfDx) + (lfQy * lfDy) + (lfQz * lfDz);
                    const f32 lfTNum = (lfE2x * lfQx) + (lfE2y * lfQy) + (lfE2z * lfQz);

                    const f32 lfLo = (-lfDet) * KF_STUCK_LINE_BARYCENTRIC_TOLERANCE;
                    const f32 lfHi = lfDet - lfLo;

                    const bool lbAccept =
                        (lfDet > KF_STUCK_LINE_MIN_DETERMINANT)
                        && (lfU >= lfLo)    && !(lfU > lfHi)
                        && (lfV >= lfLo)    && !((lfU + lfV) > lfHi)
                        && (lfTNum >= lfLo) && !(lfTNum > lfHi)
                        && IsLaneEnabled(lrBlock.mValidMasks, liLane);

                    if (!lbAccept)
                    {
                        continue;
                    }

                    lpaOut[liLine].mbHit = true;

                    const f32 lfT = lfTNum / lfDet;   // vrefp + 2 NR, then vmulfp; det > 0 here

                    if (!(lfT >= lpaOut[liLine].mfBestT))
                    {
                        lpaOut[liLine].mfBestT      = lfT;
                        lpaOut[liLine].mNormal      = laNormal[liLane];
                        lpaOut[liLine].muSurfaceTag = LaneBits(lrBlock.mSurfaceTags, liLane);
                    }
                }
            }
        }
    }

    // Wheel-pair midpoint in car space, then car space -> world through the four affine rows --
    // the `vaddfp ; vmulfp(0.5) ; vmaddfp x3` cascade at 0x825C3BB0..0x825C3C18 (and its two
    // twins). The per-wheel "Invalid wheel position: ..., please tell Graham D." tripwire is the
    // inlined Wheel::GetPosition (Wheel.h:412) IsFinite check on the three lanes; reproduced as a
    // non-gating CGS_ASSERT here since the tree's GetPosition is a bare accessor.
    Vector3 WorldMidpointOfWheelPair(const RaceCarPhysics& lrCar,
                                     EVehicleDrivenWheel leWheelA, EVehicleDrivenWheel leWheelB)
    {
        const Vector3& lrA = lrCar.GetWheel(leWheelA).GetPosition();
        CGS_ASSERT((lrA.x == lrA.x) && (lrA.y == lrA.y) && (lrA.z == lrA.z),
                   "Invalid wheel position: , please tell Graham D.");            // Wheel.h:412
        const Vector3& lrB = lrCar.GetWheel(leWheelB).GetPosition();
        CGS_ASSERT((lrB.x == lrB.x) && (lrB.y == lrB.y) && (lrB.z == lrB.z),
                   "Invalid wheel position: , please tell Graham D.");            // Wheel.h:412

        // (B + A) * 0.5 -- the console adds the SECOND-loaded wheel into the first
        // (`vaddfp128 v0, v0, v127`); the order is immaterial to the sum.
        const f32 lfLx = (lrB.x + lrA.x) * KF_STUCK_LINE_MIDPOINT;
        const f32 lfLy = (lrB.y + lrA.y) * KF_STUCK_LINE_MIDPOINT;
        const f32 lfLz = (lrB.z + lrA.z) * KF_STUCK_LINE_MIDPOINT;

        const Matrix44Affine& lrM = lrCar.GetTransform();
        // pos + xAxis*lx ; + yAxis*ly ; + zAxis*lz  (vmaddfp128 v123,v125,.. ; v124 ; v126)
        Vector3 lvWorld;
        lvWorld.x = lrM.wAxis.x + (lrM.xAxis.x * lfLx) + (lrM.yAxis.x * lfLy) + (lrM.zAxis.x * lfLz);
        lvWorld.y = lrM.wAxis.y + (lrM.xAxis.y * lfLx) + (lrM.yAxis.y * lfLy) + (lrM.zAxis.y * lfLz);
        lvWorld.z = lrM.wAxis.z + (lrM.xAxis.z * lfLx) + (lrM.yAxis.z * lfLy) + (lrM.zAxis.z * lfLz);
        lvWorld.w = lrM.wAxis.w + (lrM.xAxis.w * lfLx) + (lrM.yAxis.w * lfLy) + (lrM.zAxis.w * lfLz);
        return lvWorld;
    }
}

    // =============================================================================================
    // VehicleManager::UpdatePlayerStuckInCollisionTest  @0x825E9DD8  (87 insns)
    // The Start-half entry (StartVehicleTractionLineTests calls it first, before the allocations).
    // One assert on the cache interface (which is otherwise UNREAD -- r4 is never touched again),
    // then the inlined BitArray<8>::IsBitSet on the player's index gates the sphere update.
    // =============================================================================================
    void VehicleManager::UpdatePlayerStuckInCollisionTest(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
            f32 lfTimeStep)
    {
        CGS_ASSERT(lpTriCacheInterface != nullptr, "lpTriCacheInterface != NULL");        // :3334

        if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return;
        }

        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(luPlayer < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS), "invalid index : "); // CgsBitArray.h:203

        if (mUsedRaceCars.IsBitSet(luPlayer))
        {
            UpdatePlayerStuckInCollisionSpheres(lfTimeStep);
        }
    }

    // =============================================================================================
    // VehicleManager::UpdatePlayerStuckInCollisionSpheres  @0x825C4AB8  (147 insns)
    // THE CACHE SPHERE. mStuckInCollisionTestCacheSphere is {centre.xyz, radius.w}:
    //   0x825C4C68  lvx128 v0, sphere ; vrlimi128 v0, v13, 1, 0 ; stvx128   -> sphere.w = RADIUS
    //   0x825C4C74  vsubfp v0, v9(pos), v0 ; vmsum3fp128 ; vrsqrtefp + 2 NR ; vmul  -> |pos - centre|
    //   0x825C4CB0  vsel (0 where the dot was exactly 0) ; vcmpgtfp. vs vspltw(sphere.w)
    //   hit: 0x825C4CD0  vrlimi128 v9(pos), v0(sphere), 1, 0 ; stvx128  -> centre = pos, w kept
    //        0x825C4CDC  stfs flt_8200426C -> car+0x10F0                  -> timer = 5 s
    //   else 0x825C4CEC  lfs/fsubs/stfs car+0x10F0 -= f31 (dt)
    // `pos` is row 3 of GetGraphicsVehicleTransform (`lvx128 v9, r3, 0x30`).
    // The seat is the header's raw 16-byte model of the DWARF `Sphere` (BrnVehicleManager.h:1087);
    // it is read/written whole through memcpy BY NAME, exactly the console's lvx128/stvx128 pair.
    // =============================================================================================
    void VehicleManager::UpdatePlayerStuckInCollisionSpheres(f32 lfTimeStep)
    {
        CGS_ASSERT(mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");     // :3364
        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(luPlayer < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS), "invalid index : "); // CgsBitArray.h:203
        CGS_ASSERT(mUsedRaceCars.IsBitSet(luPlayer),
                   "mUsedRaceCars.IsBitSet( mePlayerActiveRaceCarIndex )");               // :3365

        // [GUARD] The console indexes maRaceCarVehicles with the raw value after the asserts
        // (a -1 would read 5216 bytes BEFORE the array). Its only caller has already checked both
        // conditions, so this never fires in practice; on the host an out-of-range index is UB
        // rather than a wild read, hence the early-out.
        if (luPlayer >= static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS))
        {
            return;
        }

        RaceCarPhysics& lrCar = maRaceCarVehicles[luPlayer];
        const Matrix44Affine lGraphicsTransform = lrCar.GetGraphicsVehicleTransform();
        const Vector3& lrPos = lGraphicsTransform.wAxis;

        Vector4 lSphere;
        std::memcpy(&lSphere, mStuckInCollisionTestCacheSphere, sizeof(lSphere));
        lSphere.w = KF_STUCK_IN_COLLISION_CACHE_SPHERE_RADIUS;                      // vrlimi128 ..,1,0
        std::memcpy(mStuckInCollisionTestCacheSphere, &lSphere, sizeof(lSphere));   // stvx128 (before the compare)

        const f32 lfDx = lrPos.x - lSphere.x;
        const f32 lfDy = lrPos.y - lSphere.y;
        const f32 lfDz = lrPos.z - lSphere.z;
        const f32 lfDistSq = (lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz);
        // vrsqrtefp(0) is +inf and 0*inf is NaN, which is why the console vsel's a 0 in for an
        // exactly-zero dot (`vcmpeqfp v12, v12(0), v0 ; vsel v0, v0, v8(0), v12`). sqrt(0) is 0.
        const f32 lfDist = (lfDistSq == 0.0f) ? 0.0f : std::sqrt(lfDistSq);

        if (lfDist > lSphere.w)
        {
            lSphere.x = lrPos.x;
            lSphere.y = lrPos.y;
            lSphere.z = lrPos.z;                                                      // w lane kept
            std::memcpy(mStuckInCollisionTestCacheSphere, &lSphere, sizeof(lSphere));
            lrCar.SetTimeUntilStuckInCollisionTest(KF_STUCK_IN_COLLISION_TEST_INTERVAL);
        }
        else
        {
            lrCar.SetTimeUntilStuckInCollisionTest(
                lrCar.GetTimeUntilStuckInCollisionTest() - lfTimeStep);
        }
    }

    // =============================================================================================
    // VehicleManager::AddPlayerStuckInCollisionLineTests  @0x825E9B28  (171 insns)
    // THE GENERATION HALF of the stuck leg: ONE 176-byte command, FOUR lines, posted only when the
    // player car exists, has an active deformation model, and has sat in the cache sphere for 5 s.
    //   0x825E9CB8  lwzx r11, lpDeformationManager, 0x12908 ; cmpwi -1   (miPlayerModelIndex, DWARF :367)
    //   0x825E9CD4  lfs f13, 0x1830(car) ; fcmpu vs flt_82001CC0 ; bgt -> post nothing
    //               (0x1830 - 0x740 == 0x10F0 == mfTimeUntilStuckInCollisionTest: "timer > 0 -> no")
    //   0x825E9CF0  GetPlayerCarModel -> GetWheelTagPoints(&lStack[4])  (v42..v45 at sp+0x70..0xA0)
    //   0x825E9D04  GetCache(player) / GetNumCachedTriangleBatches(player)
    //   0x825E9D30  AllocateCommand off mpTractionLineStreamProducer
    //   0x825E9D80  stw 4 -> miNumLines
    //   0x825E9D88  the eight stvx128 (r11 == cmd; line k start @+0x10k, end @+0x50+0x10k):
    //                 +0x00 <- tag0   +0x50 <- tag3      line 0: FL -> RR
    //                 +0x10 <- tag3   +0x60 <- tag0      line 1: RR -> FL
    //                 +0x20 <- tag1   +0x70 <- tag2      line 2: FR -> RL
    //                 +0x30 <- tag2   +0x80 <- tag1      line 3: RL -> FR
    //   0x825E9DA8  mpTriangles / miNumTriangleBatches, then CheckAlignment + ValidateTriangles
    //   0x825E9DBC  li r3, 1  -- ONE command posted (the count StartVehicleTractionLineTests sums)
    // (VehicleManagerDebugComponent::RecordStuckInCollisionLineTest(Vector3*) is DECLARED in the
    //  DWARF (:211) but the X360 body has no call to it -- xrefs_from is exhaustive above -- so no
    //  debug recording happens on the Add side.)
    // =============================================================================================
    s32 VehicleManager::AddPlayerStuckInCollisionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager)
    {
        CGS_ASSERT(lpTractionContactGen != nullptr, "lpTractionContactGen != NULL");     // :2057
        CGS_ASSERT(lpCacheInterface != nullptr, "lpCacheInterface != NULL");             // :2058
        CGS_ASSERT(lpDeformationManager != nullptr, "lpDeformationManager != NULL");     // :2059

        if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return 0;
        }

        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(luPlayer < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS), "invalid index : "); // CgsBitArray.h:203

        if (!mUsedRaceCars.IsBitSet(luPlayer)
            || lpDeformationManager->GetPlayerModelIndex() == -1
            || (maRaceCarVehicles[luPlayer].GetTimeUntilStuckInCollisionTest() > 0.0f))
        {
            return 0;
        }

        // The console's four stack vectors are uninitialised before GetWheelTagPoints fills them
        // (it skips a wheel with no tag point). Zeroed here so a tagless wheel posts a
        // deterministic degenerate line rather than stack garbage.
        Vector3 laTagPoints[eNumDrivenWheels] = {};
        Deformation::DeformableObject* const lpPlayerModel = lpDeformationManager->GetPlayerCarModel();
        lpPlayerModel->GetWheelTagPoints(laTagPoints);

        const CgsGeometric::Triangle4* const lpTriangles = lpCacheInterface->GetCache(static_cast<s32>(luPlayer));
        const s32 liNumBatches = lpCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luPlayer));

        LineDesc::StreamCommand* lpCommand = nullptr;
        mpTractionLineStreamProducer->AllocateCommand(reinterpret_cast<void**>(&lpCommand));

        lpCommand->miNumLines = KI_NUM_STUCK_IN_COLLISION_LINE_TESTS;

        // {start, end} tag-point pairs per line, exactly the eight stvx128 above.
        const s32 laiStartTag[KI_NUM_STUCK_IN_COLLISION_LINE_TESTS] = { 0, 3, 1, 2 };
        const s32 laiEndTag[KI_NUM_STUCK_IN_COLLISION_LINE_TESTS]   = { 3, 0, 2, 1 };
        for (s32 liLine = 0; liLine < KI_NUM_STUCK_IN_COLLISION_LINE_TESTS; ++liLine)
        {
            const Vector3& lrStart = laTagPoints[laiStartTag[liLine]];
            const Vector3& lrEnd   = laTagPoints[laiEndTag[liLine]];
            // Whole 16-byte registers move on the console; all four lanes travel.
            lpCommand->maLineStart[liLine].x = lrStart.x;
            lpCommand->maLineStart[liLine].y = lrStart.y;
            lpCommand->maLineStart[liLine].z = lrStart.z;
            lpCommand->maLineStart[liLine].w = lrStart.w;
            lpCommand->maLineEnd[liLine].x   = lrEnd.x;
            lpCommand->maLineEnd[liLine].y   = lrEnd.y;
            lpCommand->maLineEnd[liLine].z   = lrEnd.z;
            lpCommand->maLineEnd[liLine].w   = lrEnd.w;
        }

        lpCommand->mpTriangles          = lpTriangles;
        lpCommand->miNumTriangleBatches = liNumBatches;

        // The console passes &cmd->mpTriangles: the {const Triangle4*, s32} pair at +0xA0 IS a
        // TriangleList (same seam AddRaceCarTractionLineTests uses).
        CgsSceneManager::CgsCollision::TriangleList* const lpTriangleList =
            reinterpret_cast<CgsSceneManager::CgsCollision::TriangleList*>(&lpCommand->mpTriangles);
        lpTriangleList->CheckAlignment();
        lpTriangleList->ValidateTriangles();

        return 1;
    }

    // =============================================================================================
    // VehicleManager::ReadPlayerStuckTractionLineTestResults  @0x825C3898  (118 insns)
    // THE HARVEST HALF. Same three preconditions as the Add (player exists, bit set, timer <= 0)
    // -- NOT the deformation-model test, which is why the record read is null-guarded: a frame
    // where the Add posted nothing leaves the cursor past the end and GetCurrent answers NULL.
    //   0x825C39D4  GetCurrent -> r31 (NULL -> done)
    //   0x825C39F0  mDebugComponent.RecordStuckInCollisionLineTestResult(r31)
    //   0x825C39F4  lbz +0xB4/+0xB5 -> (hit0 && hit1) ; lbz +0xB6/+0xB7 -> (hit2 && hit3)
    //   0x825C3A58  stb 1 -> this+0x2A240 (172608 == mbPlayerCarStuckInCollision) if either
    //   0x825C3A64  stfs flt_8200426C -> car+0x10F0  (timer re-armed to 5 s, hit or miss)
    // NO GetNext: this is the LAST of the three harvests EndVehicleTractionLineTests runs, and
    //    the console does not advance the shared cursor after it. Reproduced.
    // =============================================================================================
    void VehicleManager::ReadPlayerStuckTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator* lpResultIterator)
    {
        if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return;
        }

        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(luPlayer < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS), "invalid index : "); // CgsBitArray.h:203

        if (!mUsedRaceCars.IsBitSet(luPlayer)
            || (maRaceCarVehicles[luPlayer].GetTimeUntilStuckInCollisionTest() > 0.0f))
        {
            return;
        }

        const LineDesc::StreamResult* const lpResult =
            static_cast<const LineDesc::StreamResult*>(lpResultIterator->GetCurrent());
        if (lpResult == nullptr)
        {
            return;
        }

        // [FLAG header_request] VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult
        // @0x825B7A48 (34 insns; DWARF BrnVehicleManagerDebugComponent.h:214) is NOT declared in
        // the tree's component header, and that header/TU is not this lane's file. Its body is
        // reproduced INLINE here through the component's existing `friend class VehicleManager`
        // grant (the seats are the DWARF's own :225/:226 members):
        //   0x825B7AA0  lbzx hit = result[0xB4 + i]
        //   0x825B7AB0  stb 1 -> component+0x500+i        (mabStuckInCollisionIntersection[i])
        //   0x825B7AB4  lvx128 result+0x10*i -> stvx128 component+0x4C0+0x10*i
        //                                                 (maStuckInCollisionLineTestPoint[i])
        // DELETE-WHEN the component declares and homes the method; then this block becomes
        // `mDebugComponent.RecordStuckInCollisionLineTestResult(lpResult);`.
        CGS_ASSERT(lpResult != nullptr, "lpResult != NULL");   // BrnVehicleManagerDebugComponent.cpp:1197
        for (s32 liLine = 0; liLine < KI_NUM_STUCK_IN_COLLISION_LINE_TESTS; ++liLine)
        {
            if (lpResult->mabHit[liLine] != 0)
            {
                mDebugComponent.mabStuckInCollisionIntersection[liLine] = true;
                const Vector4& lrHit = lpResult->maHitPosition[liLine];
                Vector3& lrPoint = mDebugComponent.maStuckInCollisionLineTestPoint[liLine];
                lrPoint.x = lrHit.x;
                lrPoint.y = lrHit.y;
                lrPoint.z = lrHit.z;
                lrPoint.w = lrHit.w;
            }
        }
        // ---- end inlined RecordStuckInCollisionLineTestResult ----

        // Lines 0/1 are FL<->RR both ways, 2/3 are FR<->RL both ways (see the Add's store map).
        // A diagonal counts only when BOTH directions hit: geometry between the two wheels.
        const bool lbStuckAlongFrontLeftToRearRight =
            (lpResult->mabHit[0] != 0) && (lpResult->mabHit[1] != 0);
        const bool lbStuckAlongFrontRightToRearLeft =
            (lpResult->mabHit[2] != 0) && (lpResult->mabHit[3] != 0);

        if (lbStuckAlongFrontLeftToRearRight || lbStuckAlongFrontRightToRearLeft)
        {
            mbPlayerCarStuckInCollision = true;
        }

        maRaceCarVehicles[luPlayer].SetTimeUntilStuckInCollisionTest(KF_STUCK_IN_COLLISION_TEST_INTERVAL);
    }

    // =============================================================================================
    // VehicleManager::DoPlayerTractionLineTestsPostSimulation  @0x826185A0  (548 insns)
    // The player's four wheel traction lines, re-tested on the PPU after the simulation step.
    //   0x826185F0  per wheel: GetTractionLine(wheel, &start[w], &end[w]) ; hit=0 ; best=FLT_MAX ;
    //               normal=0 ; tag=0
    //   0x82618678  v13 = car+0x20 (mTransform.yAxis, the body UP row) ; v0 = car+0x50
    //               (mLinearVelocity) ; vmsum3fp128 ; vxor sign  ->  s = -dot3(vel, up)
    //   0x8261868C  if (s > 0)   [loop-invariant; the console re-tests it per wheel]
    //   0x826186C0      start[w] = up * s * dt + start[w]          (vmaddfp v12, v12, v10, v11)
    //               i.e. a car still moving INTO the ground has its probes lifted by the distance
    //               it will sink this step, so the post-step test starts above the surface.
    //   0x826186D8  GetCache(player) / GetNumCachedTriangleBatches(player), both inlined with their
    //               own asserts (CgsSceneManagerModuleIO.h:1286/:1295, CgsCachedTriangleList.h:153)
    //   0x82618760  the per-line state is re-cleared a SECOND time (hit/normal/tag/best) -- the
    //               compiler emitted the initialiser twice; no observable effect, not reproduced.
    //   0x826188B0  the kernel (see IntersectLinesWithTriangleBatches)
    //   0x82618D9C  per hit wheel: pos = (end - start) * t + start  [start == the LIFTED start]
    //               AddTractionPoint(wheel, v1 = pos, v2 = normal, r5 = tag lane 0)
    //   0x82618E0C  CalculateNewWheelPlane() ; StoreLocalWheelPositions()
    // PPC float-arg rule: f1 (the timestep) is the SECOND declared parameter but r4 is still the
    //    input interface -- the prototype in the DWARF (:1454) and `stfs f1, arg_24` agree.
    // =============================================================================================
    void VehicleManager::DoPlayerTractionLineTestsPostSimulation(
            const VehicleInputInterface* lpInputInterface, f32 lfTimeStep)
    {
        // The console reads mePlayerActiveRaceCarIndex unguarded; the one caller
        // (UpdateVehiclePhysicsPostSimulation) has already checked the player bit.
        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        // [GUARD] host-only: an invalid index is UB here where the console would read wild memory.
        if (luPlayer >= static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS))
        {
            return;
        }
        RaceCarPhysics& lrCar = maRaceCarVehicles[luPlayer];

        Vector3 laStart[eNumDrivenWheels];
        Vector3 laEnd[eNumDrivenWheels];
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            lrCar.GetTractionLine(static_cast<EVehicleDrivenWheel>(liWheel),
                                  laStart[liWheel], laEnd[liWheel]);
        }

        // s = -dot3(linear velocity, body up); lift every probe start by up * s * dt when s > 0.
        const Vector3& lrUp  = lrCar.GetUpAxis();
        const Vector3& lrVel = lrCar.GetLinearVelocity();
        const f32 lfSink = -((lrVel.x * lrUp.x) + (lrVel.y * lrUp.y) + (lrVel.z * lrUp.z));
        if (lfSink > 0.0f)
        {
            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                // vmaddfp v12, v12(up*s), v10(start), v11(dt)  ==  (up*s) * dt + start, all lanes
                laStart[liWheel].x = (lrUp.x * lfSink) * lfTimeStep + laStart[liWheel].x;
                laStart[liWheel].y = (lrUp.y * lfSink) * lfTimeStep + laStart[liWheel].y;
                laStart[liWheel].z = (lrUp.z * lfSink) * lfTimeStep + laStart[liWheel].z;
                laStart[liWheel].w = (lrUp.w * lfSink) * lfTimeStep + laStart[liWheel].w;
            }
        }

        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* const lpCacheInterface =
            lpInputInterface->GetTriangleCacheInterface();
        const CgsGeometric::Triangle4* const lpTriangles = lpCacheInterface->GetCache(static_cast<s32>(luPlayer));
        const s32 liNumBatches = lpCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luPlayer));

        LineTestHit laHits[eNumDrivenWheels];
        IntersectLinesWithTriangleBatches(laStart, laEnd, eNumDrivenWheels,
                                          lpTriangles, liNumBatches, laHits);

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const LineTestHit& lrHit = laHits[liWheel];
            if (!lrHit.mbHit)
            {
                continue;
            }

            const Vector3& lrStart = laStart[liWheel];
            const Vector3& lrEnd   = laEnd[liWheel];
            const f32      lfT     = lrHit.mfBestT;

            // vsubfp v13, end, start ; vmaddfp v1, v13, v0(start), v12(t) -- all four lanes.
            const Vector3 lvPosition = { ((lrEnd.x - lrStart.x) * lfT) + lrStart.x,
                                         ((lrEnd.y - lrStart.y) * lfT) + lrStart.y,
                                         ((lrEnd.z - lrStart.z) * lfT) + lrStart.z,
                                         ((lrEnd.w - lrStart.w) * lfT) + lrStart.w };
            const Vector3 lvNormal   = { lrHit.mNormal.x, lrHit.mNormal.y,
                                         lrHit.mNormal.z, lrHit.mNormal.w };

            lrCar.AddTractionPoint(static_cast<EVehicleDrivenWheel>(liWheel),
                                   lvPosition, lvNormal, lrHit.muSurfaceTag);
        }

        lrCar.CalculateNewWheelPlane();
        lrCar.StoreLocalWheelPositions();
    }

    // =============================================================================================
    // VehicleManager::DoPlayerStuckLineTests  @0x825C3A70  (1041 insns)
    // ONE occlusion line per frame, round-robin over three probes, on the player car only:
    //   plane 0 (knVehicleRoundRobinFrontPlaneToTest):  midpoint(FL,FR) -> +zAxis * mfFrontRayLength
    //   plane 4 (knVehicleRoundRobinRearPlaneToTest):   midpoint(RL,RR) -> -zAxis * mfRearRayLength
    //   plane 8 (knVehicleRoundRobinFrontSensorToTest): midpoint(FL,FR) -> +zAxis * mfFrontRaySensorLength
    // Wheel seats: +0x1B0/+0x290 == wheel 0/1 + 0x80 (Wheel::mPosition) for the front pair,
    // +0x370/+0x450 == wheel 2/3 + 0x80 for the rear pair (maWheels @+0x130, stride 0xE0).
    //
    // Control word mn8RoundRobinControlWord (+172464):
    //   0x825C3AEC..0x825C3B68  moving? |angVel|^2 > 0.0289 (car+0x60) or |linVel|^2 > 1.0 (car+0x50)
    //   0x825C3B70..0x825C3B80    -> mbIsWedgedInWorld = 0 ; word = (word & 0xF0) | 8
    //                               (both occluded bits cleared, next test = front sensor)
    //   0x825C3B90  plane = word & 12 ; 0/4/8 arms ; 12 -> assert "Unknown state condition" :3188
    //               (and the line stays whatever was on the stack -- zero here)
    //   0x825C44xx  the kernel, single line, against GetTrianglesForCachedObject(player) x
    //               GetNumCachedTriangleBatches(player)
    //   result: occluded = hit && !(tag & 0x2000)   (`extrwi r11, tag, 3,16 ; clrlwi 31` == bit 13
    //           of the tag WORD == BrnWorld::KU_COLLISION_FLAG_DRIVEABLE in the material halfword)
    //   0x825C49D0  plane 0: word |= 1  / word &= ~1     (knVehicleRoundRobinFrontPlaneOccluded)
    //   0x825C499C  plane 4: word |= 2  / word &= ~2     (knVehicleRoundRobinRearPlaneOccluded)
    //   0x825C496C  plane 8: mbIsFrontRayOccluded = occluded
    //               plane 12: assert "Unknown plane" :3262
    //   0x825C4A04  mbIsWedgedInWorld = (word & 1) && (word & 2)
    //   0x825C4A34  word &= ~12 ; 0 -> |= 4 ; 4 -> |= 8 ; 8 -> (stays 0) ; else assert "Unknown wheel" :3310
    // (No debug-component recording in this body: the DWARF's RecordStuckInCollisionLineTest is
    //  not called from it -- no store to the component's +0x440..+0x4C0 seats anywhere in the asm.)
    // The "moving" reset happens BEFORE this frame's test, so a car that just stopped runs the
    //    front SENSOR first, then plane 0, then plane 4 -- three frames to the first wedge verdict.
    // =============================================================================================
    void VehicleManager::DoPlayerStuckLineTests(const VehicleInputInterface* lpInputInterface)
    {
        const u32 luPlayer = static_cast<u32>(mePlayerActiveRaceCarIndex);
        // [GUARD] host-only, same reason as the post-simulation twin above.
        if (luPlayer >= static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS))
        {
            return;
        }
        RaceCarPhysics& lrCar = maRaceCarVehicles[luPlayer];

        // ---- moving car: forget everything and restart the round robin at the front sensor ----
        const Vector3& lrAngVel = lrCar.GetAngularVelocity();
        const Vector3& lrLinVel = lrCar.GetLinearVelocity();
        const f32 lfAngSq = (lrAngVel.x * lrAngVel.x) + (lrAngVel.y * lrAngVel.y) + (lrAngVel.z * lrAngVel.z);
        const f32 lfLinSq = (lrLinVel.x * lrLinVel.x) + (lrLinVel.y * lrLinVel.y) + (lrLinVel.z * lrLinVel.z);
        if ((lfAngSq > KF_STUCK_LINETEST_ANGULARCUTOFF_SQ) || (lfLinSq > KF_STUCK_LINETEST_LINEARCUTOFF_SQ))
        {
            lrCar.SetIsWedgedInWorld(false);
            mn8RoundRobinControlWord = static_cast<unsigned char>(
                (mn8RoundRobinControlWord & 0xF0u) | knVehicleRoundRobinFrontSensorToTest);
        }

        // ---- which probe this frame ------------------------------------------------------------
        const u32 luPlane = static_cast<u32>(mn8RoundRobinControlWord) & knVehicleRoundRobinNextTest;

        Vector3 lvStart = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3 lvEnd   = { 0.0f, 0.0f, 0.0f, 0.0f };
        const Matrix44Affine& lrM = lrCar.GetTransform();

        if (luPlane == knVehicleRoundRobinFrontPlaneToTest)
        {
            lvStart = WorldMidpointOfWheelPair(lrCar, eFrontLeftWheel, eFrontRightWheel);
            const f32 lfLen = mfFrontRayLength;                                       // +171472
            lvEnd.x = (lfLen * lrM.zAxis.x) + lvStart.x;                             // vmaddcfp128
            lvEnd.y = (lfLen * lrM.zAxis.y) + lvStart.y;
            lvEnd.z = (lfLen * lrM.zAxis.z) + lvStart.z;
            lvEnd.w = (lfLen * lrM.zAxis.w) + lvStart.w;
        }
        else if (luPlane == knVehicleRoundRobinRearPlaneToTest)
        {
            lvStart = WorldMidpointOfWheelPair(lrCar, eRearLeftWheel, eRearRightWheel);
            const f32 lfLen = mfRearRayLength;                                        // +171476
            lvEnd.x = lvStart.x - (lrM.zAxis.x * lfLen);                             // vsubfp128
            lvEnd.y = lvStart.y - (lrM.zAxis.y * lfLen);
            lvEnd.z = lvStart.z - (lrM.zAxis.z * lfLen);
            lvEnd.w = lvStart.w - (lrM.zAxis.w * lfLen);
        }
        else if (luPlane == knVehicleRoundRobinFrontSensorToTest)
        {
            lvStart = WorldMidpointOfWheelPair(lrCar, eFrontLeftWheel, eFrontRightWheel);
            const f32 lfLen = mfFrontRaySensorLength;                                 // +171468
            lvEnd.x = (lfLen * lrM.zAxis.x) + lvStart.x;                             // vmaddcfp128
            lvEnd.y = (lfLen * lrM.zAxis.y) + lvStart.y;
            lvEnd.z = (lfLen * lrM.zAxis.z) + lvStart.z;
            lvEnd.w = (lfLen * lrM.zAxis.w) + lvStart.w;
        }
        else
        {
            // Unreachable by construction (the word only ever holds 0/4/8 in bits 2-3); the
            // console tests an uninitialised stack line here. Zero is the host's stand-in.
            CGS_ASSERT(false, "Unknown state condition");                            // :3188
        }

        // ---- the test ----------------------------------------------------------------------------
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* const lpCacheInterface =
            lpInputInterface->GetTriangleCacheInterface();
        const CgsGeometric::Triangle4* const lpTriangles = lpCacheInterface->GetCache(static_cast<s32>(luPlayer));
        const s32 liNumBatches = lpCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luPlayer));

        LineTestHit lHit;
        IntersectLinesWithTriangleBatches(&lvStart, &lvEnd, 1, lpTriangles, liNumBatches, &lHit);

        // occluded == the line hit something that is NOT driveable (a wall, not the road)
        const bool lbOccluded =
            lHit.mbHit && ((lHit.muSurfaceTag & BrnWorld::KU_COLLISION_FLAG_DRIVEABLE) == 0u);

        // ---- record the verdict --------------------------------------------------------------------
        if (luPlane == knVehicleRoundRobinFrontPlaneToTest)
        {
            if (lbOccluded)
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord | knVehicleRoundRobinFrontPlaneOccluded);
            else
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord & ~static_cast<u32>(knVehicleRoundRobinFrontPlaneOccluded));
        }
        else if (luPlane == knVehicleRoundRobinRearPlaneToTest)
        {
            if (lbOccluded)
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord | knVehicleRoundRobinRearPlaneOccluded);
            else
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord & ~static_cast<u32>(knVehicleRoundRobinRearPlaneOccluded));
        }
        else if (luPlane == knVehicleRoundRobinFrontSensorToTest)
        {
            lrCar.SetIsFrontRayOccluded(lbOccluded);
        }
        else
        {
            CGS_ASSERT(false, "Unknown plane");                                        // :3262
        }

        // wedged == BOTH planes occluded
        lrCar.SetIsWedgedInWorld(
            ((mn8RoundRobinControlWord & knVehicleRoundRobinFrontPlaneOccluded) != 0)
            && ((mn8RoundRobinControlWord & knVehicleRoundRobinRearPlaneOccluded) != 0));

        // ---- advance the round robin: 0 -> 4 -> 8 -> 0 -------------------------------------------
        mn8RoundRobinControlWord = static_cast<unsigned char>(
            mn8RoundRobinControlWord & ~static_cast<u32>(knVehicleRoundRobinNextTest));
        switch (luPlane)
        {
            case knVehicleRoundRobinFrontPlaneToTest:
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord | knVehicleRoundRobinRearPlaneToTest);
                break;
            case knVehicleRoundRobinRearPlaneToTest:
                mn8RoundRobinControlWord = static_cast<unsigned char>(mn8RoundRobinControlWord | knVehicleRoundRobinFrontSensorToTest);
                break;
            case knVehicleRoundRobinFrontSensorToTest:
                // the cleared bits ARE knVehicleRoundRobinFrontPlaneToTest (0)
                break;
            default:
                CGS_ASSERT(false, "Unknown wheel");                                    // :3310
                break;
        }
    }
}
}
