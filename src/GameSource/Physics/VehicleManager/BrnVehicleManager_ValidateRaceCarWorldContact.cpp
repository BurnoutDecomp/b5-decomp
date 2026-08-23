// ============================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateRaceCarWorldContact.cpp
//
// BrnPhysics::Vehicle::VehicleManager::ValidateRaceCarWorldContact @0x825C6088 (988 insns)
// -- THE VALIDATION WHALE (walls leg 3, 2026-08-14). PS3 DecFIGS 0x70AB20 (1523; the mangle
// is the signature authority, DWARF h:938). Slice TU: the home BrnVehicleManager.cpp (the
// asserts' own file, :7419..:7624) is still unmounted -- RaceCarPhysics_Construct precedent.
//
// Validate ONE race-car-vs-world potential contact (side A == the car, side B == the world
// triangle -- DoRaceCarWorldContactValidation swapped it before calling). The function may
// REWRITE the contact in place (the wall-normal flatten; the wheel/bottom-plane projection)
// and returns whether it survives into the Validated queue.
//
// EVERY CONSTANT BELOW IS MEASURED, none inferred (walls leg 3, x360rd out of the unpacked
// .i64 image; addresses cited per value). The two file-scope values are namespace globals on
// the console; the function-locals are its guarded function-statics (guard dword_82FBA030).
//
// METHOD NOTE: the two VMX-dense blocks (the bottom-plane rewrite and the final AABB
// acceptance) were derived from the RAW INSTRUCTION WORDS with the leg-2-validated field
// decoders (sim_kernel field layouts), NOT from the IDA operand text -- the +32 per-operand-
// field hazard and one vperm operand-order trap live exactly there. Where the raw words
// disagree with a naive reading, the words won (see the CONSOLE QUIRK note in the body).
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // gpDebugPrint / gxMessageFilterFlags (debug-draw gates)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"         // TriangleCacheInterface (GetCache / GetNumCachedTriangleBatches)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"            // PotentialContact
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                    // Triangle4 (+ AOSTriangle)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"             // RaceCarPhysics (GetHeightAboveRoad + the friend-granted member reads)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h" // Vehicle::DebugComponent (SetLastWallTriangle)
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"           // VehicleManagerDebugComponent (mbRenderWallContacts / mbRenderGroundContacts)

#include "rw/math/vpu/vector3_operation.h"                                               // vpu::{Dot, Subtract, Normalize}

namespace BrnPhysics
{
namespace Vehicle
{
    // MEASURED namespace globals (console .data):
    //   gfGroundContactCullHeight: STATICALLY-INITIALISED .data @0x82F2A148 == 0x3ECCCCCD ==
    //     0.4f (image-read; same ordinary-initialised .data bank as the flt_82F2A2xx seed
    //     constants -- nothing to disassemble).
    //   KVF_WALL_NORMAL_DOT_THRESHOLD: DYNAMIC-INIT (zero in the image at 0x82FB7F20). Its
    //     initialiser was FOUND AND READ this wave @0x82C5BBD8..0x82C5BBF8:
    //       lis r11,0x8200 ; lfs f0,0x1DA0(r11)   <- flt_82001DA0 == 0.5f
    //       lis r11,0x82FB ; ... vspltw ; stvx128 -> 0x82FB7F20
    //     == splat(0.5f). The PS3 cross-witness: _Z41__static_initialization_and_destruction
    //     @0x6C2CCC splat-initialises the same-named global. Contacts whose |normal.y| is
    //     below this are flattened to pure horizontal ("wall") normals.
    namespace
    {
        const f32 KF_GROUND_CONTACT_CULL_HEIGHT    = 0.4f;   // gfGroundContactCullHeight @0x82F2A148
        const f32 KF_WALL_NORMAL_DOT_THRESHOLD     = 0.5f;   // KVF_WALL_NORMAL_DOT_THRESHOLD (init @0x82C5BBD8, value == flt_82001DA0)
        const f32 KF_MPS_TO_MPH                    = 2.236936330795288f; // KF_MPS_TO_MPH rodata @0x8208F820 (named X360 symbol; PS3 identical)
        const f32 KF_NORMAL_ZERO_EPSILON           = 1.1920928955078125e-07f; // stru_8208F620 lane 0 == 0x34000000 == FLT_EPSILON
        const f32 KF_UP_DOT_GROUND_NORMAL_MIN      = 0.8f;   // flt_8208F9C8 == 0x3F4CCCCD (the above-ground-plane gate)
        const f32 KF_AABB_MARGIN_XY                = 0.1f;   // flt_82004014 == 0x3DCCCCCD (x/y lanes of the margin vector; the z lane is flt_82001CC0 == 0.0f)
        const f32 KF_LATERAL_RECHECK_MARGIN        = 0.2f;   // flt_82004744 == 0x3E4CCCCD (the velocity-expanded lateral band)
        const f32 KF_FINAL_ACCEPT_MAX_SPEED_MPH    = 25.0f;  // flt_82004FD8 (f29, shared with the clearance gate)

        // Per-lane finiteness of a normal's xyz (the console's vspltw + vcmpeqfp self-compare
        // triple, used four times in this body).
        inline bool IsValidVec3Lanes(const Vector3& lrV)
        {
            return lrV.x == lrV.x && lrV.y == lrV.y && lrV.z == lrV.z;
        }
    }

    // The whale writes the wall triangle into the per-car debug component through the same
    // span cast Construct stores (BrnVehicleManager_Construct.cpp:265, the FLAGGED opaque
    // 8x1024 span). The partial host DebugComponent must FIT the console's 1024-byte slot.
    static_assert(sizeof(DebugComponent) <= 1024,
                  "Vehicle::DebugComponent (partial host reconstruction) must fit the console's "
                  "1024-byte maRaceCarDebugComponent slot");

    // ==========================================================================================
    // ValidateRaceCarWorldContact @0x825C6088   (DWARF h:938; asserts BrnVehicleManager.cpp
    // :7419..:7624 -- every line number below is the console's own)
    // ==========================================================================================
    bool VehicleManager::ValidateRaceCarWorldContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpInOutContact,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
        f32 lfTimeStep)
    {
        namespace vpu = rw::math::vpu;
        typedef CgsGeometric::Triangle4 Triangle4;

        // ---- the guarded function-statics (guard dword_82FBA030; every value image-read) ------
        static const f32 KVF_GROUND_CLEARANCE_MAX_SPEED_MPH = 25.0f;  // bit0, flt_82004FD8
        static const f32 KVF_GROUND_CLEARANCE_MIN_SPEED_MPH = 10.0f;  // bit1, flt_82004A20
        static const f32 kvfMaxCurbHeight                   = 0.25f;  // bit2, flt_8208F834
        static const f32 kvfWallNormalMaxHeight             = 0.3f;   // bit3, flt_82004740 (f30)
        static const f32 KVF_CAR_BOTTOM_PLANE_MODIFIER      = 0.0f;   // bit4, flt_82001CC0

        CGS_ASSERT(lpInOutContact != nullptr, "lpInOutContact != NULL");           // :7419
        CGS_ASSERT(lpTriCacheInterface != nullptr, "lpTriCacheInterface != NULL"); // :7420

        CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                   "Normal invalid on entry to validate\n");                       // :7421
        CGS_ASSERT(fabsf(lpInOutContact->mNormal.x) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.y) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.z) > KF_NORMAL_ZERO_EPSILON,
                   "Normal zero on entry to validate\n");                          // :7422

        // ---- the wall-normal flatten: near-horizontal normals become PURE horizontal ----------
        if (KF_WALL_NORMAL_DOT_THRESHOLD > fabsf(lpInOutContact->mNormal.y))
        {
            Vector3 lFlat = lpInOutContact->mNormal;
            lFlat.y = 0.0f;                              // vrlimi zero-insert on lane 1
            lpInOutContact->mNormal = vpu::Normalize(lFlat);   // vmsum3fp + vrsqrtefp 2-Newton
            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after clearing Y component\n");             // :7432
        }

        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdA.muId >> 56) == 1u,
                   "lpInOutContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :7436
        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdB.muId >> 56) == 0u,
                   "lpInOutContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");   // :7437

        const u32 luRaceCarIndex =
            static_cast<u32>(lpInOutContact->muVolumeInstanceIdA.muId >> 42) & 0x3FFFu;
        RaceCarPhysics& lrCar = maRaceCarVehicles[luRaceCarIndex];

        const Vector3 lPointOnCar   = lpInOutContact->mPointOnA;   // v117
        const Vector3 lPointOnWorld = lpInOutContact->mPointOnB;   // v119
        const Vector3 lUpAxis       = lrCar.GetUpAxis();           // v122 == car+0x20 (yAxis)

        // speed in MPH off the cached velocity magnitude (mNormLinearVelocityMag.w, car+0x1340
        // lane 3, m/s) -- distinct from the mfSpeedMPH read the final gate makes.
        const f32 lfSpeedMph = lrCar.mNormLinearVelocityMag.w * KF_MPS_TO_MPH;

        // ---- the contact's triangle, re-fetched from the car's cache window --------------------
        const u16 lu16TriangleIndex = lpInOutContact->mu16PrimitiveIndexB;   // +0x4A (the WORLD side)
        const s32 liLane  = static_cast<s32>(lu16TriangleIndex) % 4;         // srawi/addze signed split
        const s32 liBatch = static_cast<s32>(lu16TriangleIndex) / 4;

        const s32 liNumBatches =
            lpTriCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luRaceCarIndex));
        // Streamed value tail ("Invalid contact with triangle N within batch M, when there are
        // only K batches in total") lowered to the static prefix per the standing project rule.
        CGS_ASSERT(liBatch < liNumBatches, "Invalid contact with triangle ");      // :7486

        // GetCache carries the "mpTriangleCacheManager != NULL" tripwire the console fires here.
        const Triangle4* lpCache =
            lpTriCacheInterface->GetCache(static_cast<s32>(luRaceCarIndex));
        Triangle4::AOSTriangle lTriangle;
        lpCache[liBatch].GetAOSTriangle(liLane, lTriangle);   // {V0,V1,V2, unit normal, edge cosines}

        // ---- curb / wall classification --------------------------------------------------------
        // Heights of the triangle's three vertices above the road plane(s) under the car's
        // on-ground wheels (GetHeightAboveRoad -- the real query-point overload, fixed this wave).
        const f32 lfHeight0 = lrCar.GetHeightAboveRoad(lTriangle.mVertex0).x;
        const f32 lfHeight1 = lrCar.GetHeightAboveRoad(lTriangle.mVertex1).x;
        const f32 lfHeight2 = lrCar.GetHeightAboveRoad(lTriangle.mVertex2).x;

        const bool lbIsCurb = kvfMaxCurbHeight > lfHeight0
                           && kvfMaxCurbHeight > lfHeight1
                           && kvfMaxCurbHeight > lfHeight2;
        const bool lbIsWall = !lbIsCurb
                           && (kvfWallNormalMaxHeight > fabsf(lTriangle.mNormal.y));

        // ---- the two debug-draw overlays: GATED, NOT RECONSTRUCTED -----------------------------
        // The console draws the classified triangle + three vertex spheres (DebugRender::
        // DrawTriangle @0x8282C218 / DrawSolidSphere @0x8282BF28, offset flt_820047C8 == 0.05,
        // red 0xFF0000FF for walls / green 0xFF00FF00 for curbs) behind the dev-menu toggles
        // mbRenderWallContacts / mbRenderGroundContacts (mDebugComponent +597/+598). Both
        // toggles are FALSE on this build (nothing sets them); the 3D CInEventDrawTriangle
        // record + dispatcher are not reconstructed. Loud named gate, dead until toggled.
        if ((mDebugComponent.RenderWallContacts() && lbIsWall)
            || (mDebugComponent.RenderGroundContacts() && lbIsCurb))
        {
            static bool sbLoggedDrawGate = false;
            if (!sbLoggedDrawGate)
            {
                sbLoggedDrawGate = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: ValidateRaceCarWorldContact's wall/ground contact "
                           "debug draw (DebugRender::DrawTriangle 3D family) not reconstructed "
                           "[FLAG PC boot gate]. Reported once, not per frame\n";
            }
        }

        // ---- record the wall triangle on the car's debug component (REAL console behaviour,
        //      unconditional on the wall classification -- NOT a draw) --------------------------
        if (lbIsWall)
        {
            // The console indexes the component array directly (`this + 0x27DC0 + (car << 10)`).
            // Same span cast as Construct's (BrnVehicleManager_Construct.cpp:265, FLAGGED there);
            // the fit static_assert at the top of this TU bounds the write.
            reinterpret_cast<DebugComponent*>(&maRaceCarDebugComponent[luRaceCarIndex][0])
                ->SetLastWallTriangle(&lTriangle);
        }

        // ---- the speed-scaled ground-clearance gate --------------------------------------------
        f32 lfCullHeight = KF_GROUND_CONTACT_CULL_HEIGHT;                     // v121 = splat(0.4)
        if (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH > lfSpeedMph)
        {
            if (KVF_GROUND_CLEARANCE_MIN_SPEED_MPH > lfSpeedMph)
            {
                return true;   // below 10 mph EVERY contact is accepted (li r3,1 fast path)
            }
            // 10..25 mph: scale the cull height by (speed-10)/15 (vrefp + 2 Newton reciprocal).
            lfCullHeight = lfCullHeight
                * (lfSpeedMph - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH)
                / (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH);
        }

        // ---- the two ground-cull candidates (both suppressed for walls) ------------------------
        bool lbCullToPlane = false;   // r30

        // (a) the wheel-plane test, gated on mbMinWheelDistValid (car+0x714): is the world-side
        //     point within the (scaled) cull height of the car's wheel plane?
        if (lrCar.mbMinWheelDistValid)
        {
            const Vector3 lDelta = vpu::Subtract(lPointOnWorld,
                                                 lrCar.mWheelPlanePosAndHeight.GetVector3());
            const f32 lfDist = vpu::Dot(lDelta, lUpAxis);
            if (lrCar.mWheelPlanePosAndHeight.w + lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
            }
        }

        // (b) the above-ground-test plane (car+0x570), when its normal is up-ish (dot > 0.8).
        const AboveGroundTestResult& lrAboveGround = *lrCar.GetAboveGroundTestResult();
        if (vpu::Dot(lUpAxis, lrAboveGround.mIntersectionNormal) > KF_UP_DOT_GROUND_NORMAL_MIN)
        {
            // The console re-reads mPointOnB here and asserts it against the register-cached
            // copy ("lPointOnOther == lpInOutContact->mPointOnB", :7598). Nothing between the
            // entry load and this point writes mPointOnB, so the check is an identity on the
            // host -- reproduced as this note, not as a self-comparison.
            const f32 lfDist = vpu::Dot(
                vpu::Subtract(lPointOnWorld, lrAboveGround.mIntersectionPosition),
                lrAboveGround.mIntersectionNormal);
            if (lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
            }
        }

        // ---- the plane projection rewrite ------------------------------------------------------
        if (lbCullToPlane)
        {
            // World-space plane anchor: the FRONT-LEFT wheel's streamed (local) position pushed
            // through the car transform (raw-word verified: vmaddfp chain vD = vA*vC + vB over
            // the xAxis/yAxis/zAxis rows + wAxis, local point = car+0x1C0 ==
            // maWheels[0].mStreamedPositionPlusTwistAmount.xyz).
            const Matrix44Affine& lrTransform = lrCar.GetTransform();
            const Vector3Plus& lrLocal =
                lrCar.GetWheel(eFrontLeftWheel).mStreamedPositionPlusTwistAmount;
            Vector3 lPlanePoint;
            lPlanePoint.x = lrTransform.wAxis.x + lrTransform.xAxis.x * lrLocal.x
                          + lrTransform.yAxis.x * lrLocal.y + lrTransform.zAxis.x * lrLocal.z;
            lPlanePoint.y = lrTransform.wAxis.y + lrTransform.xAxis.y * lrLocal.x
                          + lrTransform.yAxis.y * lrLocal.y + lrTransform.zAxis.y * lrLocal.z;
            lPlanePoint.z = lrTransform.wAxis.z + lrTransform.xAxis.z * lrLocal.x
                          + lrTransform.yAxis.z * lrLocal.y + lrTransform.zAxis.z * lrLocal.z;
            lPlanePoint.w = 0.0f;

            // mNormal := the car's up axis; mPointOnA := its projection onto the wheel plane
            // (shifted by the zero KVF_CAR_BOTTOM_PLANE_MODIFIER, kept for fidelity).
            lpInOutContact->mNormal = lUpAxis;
            const f32 lfHeightAbovePlane =
                vpu::Dot(vpu::Subtract(lPointOnCar, lPlanePoint), lUpAxis)
                - KVF_CAR_BOTTOM_PLANE_MODIFIER;
            lpInOutContact->mPointOnA.x = lPointOnCar.x - lUpAxis.x * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.y = lPointOnCar.y - lUpAxis.y * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.z = lPointOnCar.z - lUpAxis.z * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.w = lPointOnCar.w - lUpAxis.w * lfHeightAbovePlane;

            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after setting to wheel plane normal\n");    // :7615
        }

        // ---- the final acceptance: is the world point inside the car's (deformable) AABB, ------
        //      expanded by the frame's displacement?
        const Matrix44Affine& lrTransform = lrCar.GetTransform();
        const Vector3 lDiff = vpu::Subtract(lPointOnWorld, lrTransform.wAxis);
        const f32 lfLocalX = vpu::Dot(lDiff, lrTransform.xAxis);
        const f32 lfLocalY = vpu::Dot(lDiff, lrTransform.yAxis);
        const f32 lfLocalZ = vpu::Dot(lDiff, lrTransform.zAxis);

        const Vector3 lDisplacement{ lrCar.mLinearVelocity.x * lfTimeStep,
                                     lrCar.mLinearVelocity.y * lfTimeStep,
                                     lrCar.mLinearVelocity.z * lfTimeStep,
                                     lrCar.mLinearVelocity.w * lfTimeStep };
        const f32 lfDispX = vpu::Dot(lDisplacement, lrTransform.xAxis);   // v10 (kept SIGNED for the recheck)
        const f32 lfDispY = vpu::Dot(lDisplacement, lrTransform.yAxis);
        const f32 lfDispZ = vpu::Dot(lDisplacement, lrTransform.zAxis);

        // upper = max + (0.1, 0.1, 0.0) + |disp| ; lower = min - (0.1, 0.1, 0.0) - |disp|
        // (margin lanes raw-word verified: x/y == flt_82004014, z == flt_82001CC0 == 0).
        const f32 lfUpperX = lrCar.mDeformableAABB.mMax.x + KF_AABB_MARGIN_XY + fabsf(lfDispX);
        const f32 lfUpperY = lrCar.mDeformableAABB.mMax.y + KF_AABB_MARGIN_XY + fabsf(lfDispY);
        const f32 lfUpperZ = lrCar.mDeformableAABB.mMax.z + 0.0f            + fabsf(lfDispZ);
        const f32 lfLowerX = lrCar.mDeformableAABB.mMin.x - KF_AABB_MARGIN_XY - fabsf(lfDispX);
        const f32 lfLowerY = lrCar.mDeformableAABB.mMin.y - KF_AABB_MARGIN_XY - fabsf(lfDispY);
        const f32 lfLowerZ = lrCar.mDeformableAABB.mMin.z - 0.0f            - fabsf(lfDispZ);

        // CONSOLE QUIRK REPRODUCED, raw-word verified: the three LOWER-bound compares all
        // test the LOCAL-Y coordinate (0x825C6DAC == 0x825C6DE4 == word 112C4EC6 -> vcmpgtfp.
        // v9, v12(localY), v9(lower splat); 0x825C6E20 -> 100C06C6 likewise), where the upper-
        // bound compares use x/y/z correctly. The PS3 compiles the SAME pairing (its cmp2/4/6
        // all read the one splat register) -- a source-level copy/paste in the shipping game,
        // reproduced faithfully rather than "fixed".
        const bool lbInsideAabb = (lfUpperX > lfLocalX) && (lfLocalY > lfLowerX)
                               && (lfUpperY > lfLocalY) && (lfLocalY > lfLowerY)
                               && (lfUpperZ > lfLocalZ) && (lfLocalY > lfLowerZ);

        bool lbAccept;
        if (lbInsideAabb)
        {
            lbAccept = true;
        }
        else if (lrCar.mbCrashing)                    // car+0x710: crashing cars keep contacts
        {
            lbAccept = true;
        }
        else if (lrCar.mbHasAir)                      // car+0x1350: airborne cars keep contacts
        {
            lbAccept = true;
        }
        else
        {
            // slow-ish cars keep out-of-AABB contacts too (mfSpeedMPH is a lane splat).
            lbAccept = KF_FINAL_ACCEPT_MAX_SPEED_MPH > lrCar.GetSpeedMPH().x;
        }

        // ---- the velocity-expanded LATERAL recheck (always runs; can only REJECT) --------------
        // Band on the local-X axis only, margin 0.2, expanded toward the travel direction.
        // dx == 0.0 exactly collapses the band to zero (the console's vmr-zero path) -- only
        // reachable at speeds >= 10 mph (the sub-10 fast path returned above), where an exactly
        // zero lateral displacement dot is a measure-zero event, exactly as shipped.
        f32 lfBandUpperX;
        f32 lfBandLowerX;
        if (lfDispX > 0.0f)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.x + KF_LATERAL_RECHECK_MARGIN + lfDispX;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.x - KF_LATERAL_RECHECK_MARGIN;
        }
        else if (0.0f > lfDispX)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.x + KF_LATERAL_RECHECK_MARGIN;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.x - KF_LATERAL_RECHECK_MARGIN + lfDispX;
        }
        else
        {
            lfBandUpperX = 0.0f;
            lfBandLowerX = 0.0f;
        }
        if (lfLocalX > lfBandUpperX || lfBandLowerX > lfLocalX)
        {
            lbAccept = false;
        }

        return lbAccept;
    }

    // ==========================================================================================
    // Vehicle::DebugComponent::SetLastWallTriangle @0x825B4D60 (32) -- defined HERE because
    // its declared home TU (B5PhysicsHandlingDebugComponent.cpp) is deliberately NOT mounted:
    // compiling it would emit the class vtable (GetPath is its key-function override) and drag
    // the CgsDev::DebugComponent base's unreconstructed virtual surface onto the link (the
    // DebugUI base-layout block). This is the identical body that TU carries -- assert + the
    // 10-qword (80-byte) AOSTriangle copy into mLastWallTriangle @console+0x350. The day
    // B5PhysicsHandlingDebugComponent.cpp mounts, DELETE this copy (the link will say so:
    // LNK2005).
    // ==========================================================================================
    void DebugComponent::SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle)
    {
        CGS_ASSERT(lpTriangle != nullptr, "lpTriangle != NULL");

        mLastWallTriangle = *lpTriangle;   // the 80-byte AOSTriangle copy (console this+0x350)
    }
}
}
