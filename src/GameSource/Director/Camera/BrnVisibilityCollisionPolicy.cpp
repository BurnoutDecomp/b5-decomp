#include "GameSource/Director/Camera/BrnCollisionPolicy.h"          // VisibilityCollisionPolicy / GroundConstraint / CollisionPolicy
#include "GameSource/Director/Camera/Camera.h"                      // Camera (the transform / the validity account / the state flags)
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"    // ValidityAccount::SetFlag
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"      // BrnDirector::SceneQueryInterface::LineTestNearest
#include "GameShared/GameClasses/Numeric/CgsRandom.h"               // CgsNumeric::Random::RandomInt (the two 20% rolls)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // [diag] CgsDev::Log::gpDebugPrint

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdio>    // std::snprintf (the streamed ground-constraint assert message)
#include <cstdlib>   // std::getenv  ([diag] BRN_CRASHCAM_DIAG)

// ============================================================================
// GameSource/Director/Camera/BrnVisibilityCollisionPolicy.cpp
//
// BrnDirector::Camera::VisibilityCollisionPolicy -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (the asserts name BrnCollisionPolicy.h / BrnCollisionPolicy.cpp).
//
// Bodied here:
//   VisibilityCollisionPolicy::Construct                       (inlined on the console; BrnCollisionPolicy.cpp:763)
//   VisibilityCollisionPolicy::GenerateSceneQueries            @0x822402F8   (FX-DIRECTOR2 2026-09-25)
//   VisibilityCollisionPolicy::ProcessSceneQueryResults        @0x82224530   (FX-DIRECTOR2 2026-09-25)
//   VisibilityCollisionPolicy::SetTarget / SetVelocity         (inlined on the console)
//   VisibilityCollisionPolicy::SetDesiredHeight                @0x821F38E0
//   VisibilityCollisionPolicy::TimeUntilCollisionWithGeometry  @0x821F37C8
//   VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle   @0x821F3858
//   GroundConstraint::GenerateSceneQueries                     @0x82240200   (FX-DIRECTOR2 2026-09-25)
//   GroundConstraint::ProcessSceneQueryResults                 @0x8220E3A0   (FX-DIRECTOR2 2026-09-25)
//   CollisionPolicy::Fail                                      @0x82206450   (moved here 2026-09-25)
//
// ⚠️ CollisionPolicy::Fail's PC home is BrnCameraCollisionPolicy.cpp. That file is NOT mounted, and
// its body took a BehaviourSharedInfo* where the console takes the Camera (both of Fail's callers,
// this policy's ProcessSceneQueryResults and CollisionPolicyAttachedToVehicle::ResolveCollisions,
// pass the camera). Mounting that TU now would be a risk of its own, so the corrected body lives in
// this mounted TU and BrnCameraCollisionPolicy.cpp is reduced to a pointer here.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// [diag, NOT X360] The live witness for the scene-query closure: every failure reason a collision
// policy raises, one line each, capped. Gated by BRN_CRASHCAM_DIAG (the crash-camera diag family --
// the HardStop PREPARING lines read the same accounts).
// ----------------------------------------------------------------------------
namespace
{
    const s32 KI_FAIL_DIAG_LINES = 400;

    bool FailDiagLine()
    {
        static const bool sbOn = (std::getenv("BRN_CRASHCAM_DIAG") != 0);
        static s32 siLinesLeft = KI_FAIL_DIAG_LINES;
        if (!sbOn || CgsDev::Log::gpDebugPrint == 0 || siLinesLeft <= 0)
            return false;
        --siLinesLeft;
        return true;
    }

    // The ValidityAccount EFailedFlag names (DWARF BrnCameraValidityAccount.h:8..:24).
    const char* FailedFlagName(s32 leFlag)
    {
        static const char* const sapcNames[14] =
        {
            "COLLISION", "VISIBILITY", "STARTED_OFF_SCREEN", "COULDNT_FIND_GROUND",
            "STARTED_IN_GEOMETRY", "COLLISION_POLICY", "COULDNT_FIND_ROADSIDE",
            "POSITION_TOO_FAR_FROM_SUBJECT", "ICE_MOVIE_COMPLETE", "STARTED_TOO_NEAR_GEOMETRY",
            "STARTED_TOO_NEAR_VEHICLE", "INVALID_VEHICLE_REF", "SUBJECT_LEFT_FRAME", "SUBJECT_OCCLUDED",
        };
        return (leFlag >= 0 && leFlag < 14) ? sapcNames[leFlag] : "?";
    }
}

// ----------------------------------------------------------------------------
// CollisionPolicy::Fail @0x82206450 (DWARF BrnCollisionPolicy.h:523)
//   addi r3, camera, 0x138 ; bl ValidityAccount::SetFlag(reason)   -- the camera's account
//   ld/and ~2/std on camera +0x140                                  -- drop the follow request
//   stb 1, 4(this)                                                  -- mbHasFailed
// ----------------------------------------------------------------------------
void CollisionPolicy::Fail(Camera& lrCamera, s32 leFailedFlag)
{
    lrCamera.GetValidityAccount().SetFlag(leFailedFlag);
    lrCamera.GetState().ClearFlag(1u);
    mbFailed = true;

    if (FailDiagLine())
    {
        char lacPolicy[32];
        std::snprintf(lacPolicy, sizeof(lacPolicy), "%p", static_cast<const void*>(this));
        *CgsDev::Log::gpDebugPrint
            << "[scenequery] policy " << lacPolicy
            << " fail " << leFailedFlag << " (" << FailedFlagName(leFailedFlag) << ")\n";
    }
}

// ----------------------------------------------------------------------------
// VisibilityCollisionPolicy::Construct (DWARF BrnCollisionPolicy.cpp:763). No out-of-line X360
// copy; the owners inline it -- BehaviourGyroCam::Construct 0x82244B88..0x82244BEC and
// BehaviourIceAnim::Construct 0x822561F0..0x8225625C agree store for store:
//   +0x04 = 0 (the base's failure latch)       predictor +0x80: velocity 0, box empty, !willCollide
//   +0x70 = 0 (vehicle predictor)              +0xF0..+0x1A2: the visibility test (0 / 0 / 0 / 0.5,
//   +0x1B0 = 0 (mVolumeTest empty)                              testLookingAt 1, occluded 0, onScreen 1)
//   +0x210 = -1.0, +0x1C0 = 0 (ground)         +0x234 = 1.5 (flt_82004D04), +0x238 = 0.5 (flt_82001DA0)
//   +0x08 = 1, +0x220 = 0 (velocity), +0x09 = 1, +0x0A = 0, +0x23C = 0
// The two per-frame decision bytes (+0x23D / +0x23E), the target and its entity id are NOT
// written -- GenerateSceneQueries sets the decisions, SetTarget the target.
// ----------------------------------------------------------------------------
void VisibilityCollisionPolicy::Construct()
{
    ClearFailed();
    mGeometryCollisionPredictor.Construct();
    mVehicleCollisionPredictor.Construct();
    mVisibilityTest.Construct();
    mVolumeTest.Construct();
    mGroundConstraint.Construct();
    mfOcclusionTimeout    = 1.5f;     // flt_82004D04 == 0x3FC00000
    mfOffscreenTimeout    = 0.5f;     // flt_82001DA0 == 0x3F000000
    mbCanFail             = true;
    mVelocity.SetZero();
    mbFirstFrame          = true;
    mbTargetSet           = false;
    mbUseGroundConstraint = false;
}

void VisibilityCollisionPolicy::SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                                          CgsSceneManager::EntityId lTargetEntityId)
{
    mbTargetSet      = true;
    mTargetTransform = lTargetTransform;
    mTargetAABB      = lTargetAABB;
    mTargetEntityId  = lTargetEntityId;
}

void VisibilityCollisionPolicy::SetVelocity(Vector3 lVelocity)
{
    mVelocity = lVelocity;
}

// @ 0x821F38E0 -- h:489. The latch is raised BEFORE the assert (the asm's stb 1,0x23C precedes the
// compare); NaN heights fire the assert like the X360's fcmpu (bgt-only skip). The height is the
// embedded ground constraint's (+0x210 == mGroundConstraint +0x50).
void VisibilityCollisionPolicy::SetDesiredHeight(f32 lfDesiredHeight)
{
    mbUseGroundConstraint = true;
    CGS_ASSERT(lfDesiredHeight > 0.0f, "lfDesiredHeight > 0.0f");   // :489 (non-gating)
    mGroundConstraint.SetDesiredHeight(lfDesiredHeight);
}

// @ 0x821F37C8 -- h:425 wrapper assert, then the embedded geometry predictor's accessor (its own
// :206 "mbWillCollide" tripwire, exactly the X360's second inlined assert).
f32 VisibilityCollisionPolicy::TimeUntilCollisionWithGeometry() const
{
    CGS_ASSERT(WillCollideWithGeometry(), "WillCollideWithGeometry()");   // :425 (non-gating)
    return mGeometryCollisionPredictor.GetTimeUntilCollision();
}

// @ 0x821F3858 -- h:431 wrapper assert, then the embedded vehicle predictor's accessor (its own
// BrnVehicleCollisionPredictor.h:69 "HasPredictedCollision()" tripwire).
f32 VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle() const
{
    CGS_ASSERT(WillCollideWithVehicle(), "WillCollideWithVehicle()");     // :431 (non-gating)
    return mVehicleCollisionPredictor.TimeUntilCollision();
}

// ----------------------------------------------------------------------------
// VisibilityCollisionPolicy::GenerateSceneQueries @0x822402F8 (BrnCollisionPolicy.cpp:805)
//
//   assert mbTargetSet                                        (:807, 0x327)
//   two 20% rolls off the director's camera Random, each forced on while mbFirstFrame:
//     mbDoingVisibilityTestThisTime      = draw % 101 < 20 || mbFirstFrame   (0x82240348..0x822403C4)
//     mbDoingCollisionPredictionThisTime = draw % 101 < 20 || mbFirstFrame   (0x822403C8..0x82240438)
//     (each draw is the Random's LCG step: the OLD seed's high word -- RandomInt(0, 100); the
//      `mulhwu 0x446F8657` / `mulli 0x65` pair is the unsigned % 101)
//   mbUseGroundConstraint       -> GroundConstraint::GenerateSceneQueries(camera, E_WORLD dt, request)
//   mbDoingCollisionPrediction  -> the geometry predictor, velocity first (the inlined pair
//                                  SetVelocity + GenerateSceneQueries, 0x82240468..0x822404AC)
//   mbCanFail                   -> assert the ground constraint's height is above the sphere test's
//                                  radius (:845), then the camera-in-geometry sphere test (GATED, below)
//   the visibility test         -> VisibilityTest::GenerateSceneQueries(camera, E_WORLD_NO_SLOMO dt,
//                                  random, request, target, bounds, mbDoingVisibilityTestThisTime,
//                                  target entity)                                     (0x822405A0)
// ----------------------------------------------------------------------------
void VisibilityCollisionPolicy::GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo,
                                                     Camera& lrCamera)
{
    CGS_ASSERT(mbTargetSet, "mbTargetSet");                                          // :807

    mbDoingVisibilityTestThisTime      = (lrSharedInfo.mpRandom->RandomInt(0, 100) < 20) || mbFirstFrame;
    mbDoingCollisionPredictionThisTime = (lrSharedInfo.mpRandom->RandomInt(0, 100) < 20) || mbFirstFrame;

    if (mbUseGroundConstraint)
    {
        mGroundConstraint.GenerateSceneQueries(lrCamera, lrSharedInfo.mTimestep.Get(Timestep::E_WORLD),
                                               lrSharedInfo.mpRequestInterface);
    }

    if (mbDoingCollisionPredictionThisTime)
    {
        mGeometryCollisionPredictor.SetVelocity(mVelocity);
        mGeometryCollisionPredictor.GenerateSceneQueries(lrCamera,
                                                         lrSharedInfo.mTimestep.Get(Timestep::E_WORLD_NO_SLOMO),
                                                         *lrSharedInfo.mpRandom,
                                                         lrSharedInfo.mpRequestInterface);
    }

    if (mbCanFail)
    {
        // `lfs f0, 0x210 ; fcmpu f0, 0.1 ; bgt skip` -- the assert fires unless the height is above
        // the sphere test's radius (0.1, flt_82004014); NaN fires it too.
        if (mbUseGroundConstraint && !(mGroundConstraint.GetDesiredHeight() > 0.1f))
        {
            char lacMessage[192];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "VisibilityCollisionPolicy: Ground contraint will conflict with sphere test "
                          "because the desired height is too low: %f",
                          static_cast<double>(mGroundConstraint.GetDesiredHeight()));
            CGS_ASSERT(false, lacMessage);                                           // :845
        }

        // ⚠️ FLAGGED GATE (conductor-approved G3, 2026-09-25) -- NOT X360. The console then builds a
        // 0.1 m rw::collision::SphereVolume at the camera (0x82240528..0x82240550) and asks
        //     request->VolumeTestDeepest(mVolumeTest, 30, 0xFF, &lSphere, lrCamera.mTransform,
        //                                KU_INVALID_ENTITY_ID (0xFFFFFFFF @0x82CDA790),
        //                                E_EXCLUDE_ENTITY_ONLY)                   (0x82240574)
        // whose answer, in ProcessSceneQueryResults, fails a camera that starts inside geometry
        // (E_FAILED_COLLISION, reason 0). The SceneManager's side of that query,
        // SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460, is a CGS_ASSERT(false) trap on
        // this build (CgsSceneManagerModule.cpp) -- it needs BaseCollisionGenerator::
        // TestSphereAgainstPolySoupList and FineIntersectionTestModule::ComputeVolumeTestDeepest,
        // neither of which is reconstructed -- so issuing it would assert every frame. mVolumeTest
        // therefore never receives a package and reason 0 cannot fire.
        // DELETE-WHEN: ProcessVolumeTestDeepest @0x828D4460 lands (logged by the conductor).
    }

    mVisibilityTest.GenerateSceneQueries(lrCamera, lrSharedInfo.mTimestep.Get(Timestep::E_WORLD_NO_SLOMO),
                                         *lrSharedInfo.mpRandom, lrSharedInfo.mpRequestInterface,
                                         mTargetTransform, mTargetAABB, mbDoingVisibilityTestThisTime,
                                         mTargetEntityId);
}

// ----------------------------------------------------------------------------
// VisibilityCollisionPolicy::ProcessSceneQueryResults @0x82224530 (BrnCollisionPolicy.cpp:865)
//
// The reason codes are ValidityAccount::EFailedFlag (DWARF BrnCameraValidityAccount.h):
//   3  COULDNT_FIND_GROUND        the ground constraint's line found nothing        (0x822245A0)
//   9  STARTED_TOO_NEAR_GEOMETRY  first frame, the camera hits the world within 1 s  (0x82224644)
//  10  STARTED_TOO_NEAR_VEHICLE   first frame, a vehicle hits the camera within 1 s  (0x822246B0)
//   0  COLLISION                  the camera sphere is inside geometry               (0x822246FC)
//   1  VISIBILITY                 first frame, occluded or off screen                (0x82224770)
//   2  STARTED_OFF_SCREEN         ... and specifically off screen                    (0x822247A0)
//  12  SUBJECT_LEFT_FRAME         later frames, off screen longer than 0.5 s         (0x8222482C)
//  13  SUBJECT_OCCLUDED           later frames, occluded longer than 1.5 s           (0x8222484C)
//   1  VISIBILITY                 ... and either timeout, always with 12/13          (0x8222485C)
// Every failure but 3 is behind mbCanFail (`lbz 8(this)` before each arm).
// ----------------------------------------------------------------------------
void VisibilityCollisionPolicy::ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo,
                                                         Camera& lrCamera)
{
    CGS_ASSERT(mbTargetSet, "mbTargetSet");                                          // :867

    if (mbUseGroundConstraint
        && !mGroundConstraint.ProcessSceneQueryResults(lrSharedInfo.mTimestep.Get(Timestep::E_WORLD), lrCamera))
    {
        Fail(lrCamera, 3);   // E_FAILED_COULDNT_FIND_GROUND
    }

    if (mbDoingCollisionPredictionThisTime)
        mGeometryCollisionPredictor.ProcessSceneQueryResults(lrSharedInfo.mTimestep.Get(Timestep::E_WORLD_NO_SLOMO));

    // ⚠️ FLAGGED GATE -- NOT X360. The console now runs
    //     Utils::VehicleCollisionPredictor::Update(&mVehicleCollisionPredictor,
    //         lrSharedInfo.mpAllVehicleData, splat(E_WORLD_NO_SLOMO dt), camera position, mVelocity)
    //                                                                          (0x822245F8, @0x822230D8)
    // which clears the prediction and then walks AllVehicleData's TRAFFIC array, testing the
    // camera's motion line against each traffic car's ellipsoid. The array arrives since
    // 2026-09-25 (BridgeWorldToDirector step 4 -> AllVehicleData::Update's lpTrafficVehicleArray,
    // FX-DIRECTOR2 item 3), so the gate's first reason expired; what remains is the callee itself:
    // VehicleCollisionPredictor::Update has no PC body yet, so the call is still not made. The
    // prediction keeps its Construct value (none) -- the console's own answer with no traffic car
    // in range -- so reason 10 cannot fire.
    // DELETE-WHEN: VehicleCollisionPredictor::Update @0x822230D8 is bodied (logged by the conductor).

    if (mbDoingCollisionPredictionThisTime && mGeometryCollisionPredictor.WillCollide()
        && mGeometryCollisionPredictor.GetTimeUntilCollision() < 1.0f && mbFirstFrame && mbCanFail)
    {
        Fail(lrCamera, 9);   // E_FAILED_STARTED_TOO_NEAR_GEOMETRY

        if (FailDiagLine())  // [diag, NOT X360] the prediction that failed it
        {
            const rw::math::vpu::Vector3& lrCameraPosition = lrCamera.mTransform.wAxis;
            char lacLine[224];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[scenequery] policy %p reason 9: cam (%.3f, %.3f, %.3f) vel (%.3f, %.3f, %.3f) tHit %.4f\n",
                          static_cast<const void*>(this),
                          static_cast<double>(lrCameraPosition.x), static_cast<double>(lrCameraPosition.y),
                          static_cast<double>(lrCameraPosition.z),
                          static_cast<double>(mVelocity.x), static_cast<double>(mVelocity.y),
                          static_cast<double>(mVelocity.z),
                          static_cast<double>(mGeometryCollisionPredictor.GetTimeUntilCollision()));
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }

    if (mVehicleCollisionPredictor.HasPredictedCollision()
        && mVehicleCollisionPredictor.TimeUntilCollision() < 1.0f && mbFirstFrame && mbCanFail)
    {
        Fail(lrCamera, 10);  // E_FAILED_STARTED_TOO_NEAR_VEHICLE
    }

    mVisibilityTest.ProcessSceneQueryResults(lrSharedInfo.mTimestep.Get(Timestep::E_WORLD_NO_SLOMO));

    if (mbCanFail && mVolumeTest.HasPackage() && mVolumeTest.GetPackage().mbIntersection)
        Fail(lrCamera, 0);   // E_FAILED_COLLISION

    if (mbCanFail && mbFirstFrame && mbDoingVisibilityTestThisTime && mVisibilityTest.IsVisibilityInterrupted())
    {
        Fail(lrCamera, 1);   // E_FAILED_VISIBILITY
        if (mVisibilityTest.WillTestLookingAt() && !mVisibilityTest.IsOnScreen())
            Fail(lrCamera, 2);   // E_FAILED_STARTED_OFF_SCREEN
    }

    if (mbCanFail && !mbFirstFrame && mbDoingVisibilityTestThisTime && GetMinTimeToVisibilityFailure() == 0.0f)
    {
        if (mVisibilityTest.WillTestLookingAt() && mVisibilityTest.GetOffscreenTime() > mfOffscreenTimeout)
            Fail(lrCamera, 12);  // E_FAILED_SUBJECT_LEFT_FRAME
        if (mVisibilityTest.GetOccludedTime() > mfOcclusionTimeout)
            Fail(lrCamera, 13);  // E_FAILED_SUBJECT_OCCLUDED
        Fail(lrCamera, 1);       // E_FAILED_VISIBILITY
    }

    mbFirstFrame = false;                                                           // stb 0, 9
    if (mbCanFail)
        mVolumeTest.Clear();                                                        // stw 0, 0x1B0
}

// ----------------------------------------------------------------------------
// GroundConstraint (DWARF BrnCollisionPolicy.h:291). Its DWARF home is BrnCollisionPolicy.cpp with
// the rest of the policy family; on this build the family is split by class, and the ground
// constraint rides with its first embedder.
// ----------------------------------------------------------------------------
const f32 GroundConstraint::KF_MIN_TEST_BELOW_LENGTH = 5.0f;   // flt_8200426C == 0x40A00000
const f32 GroundConstraint::KF_MIN_TEST_ABOVE_LENGTH = 1.0f;   // flt_82001C98 == 0x3F800000

// @0x82240200 (BrnCollisionPolicy.cpp:719 assert). One world-only nearest line test straight down:
//   start = camera position + up * KF_MIN_TEST_ABOVE_LENGTH     (vmaddfp v1 = up * splat(1.0) + camera.w)
//   end   = start - up * max(KF_MIN_TEST_BELOW_LENGTH, height)  (the `fsel` picks 5.0 unless the
//                                                               height is larger; vsubfp)
// with up = (0, 1, 0, 0) (unk_82181510), flags 2 (world), volume flags 0xFF, no excluded entity
// (0xFFFFFFFF @0x82CDA790) and E_EXCLUDE_ENTITY_ONLY. The timestep is part of the DWARF signature
// and unused.
void GroundConstraint::GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep,
                                            const SceneQueryInterface* lpRequestInterface)
{
    (void)lfTimestep;
    // `lfs f13, 0x50 ; fcmpu cr6, f13, flt_82001CC0 (0.0f) ; bge 0x8224024C` (0x8224021C..0x82240228):
    // bge is "not less than", which an unordered compare satisfies, so a NaN height SKIPS the assert
    // on the console -- only a height below 0 fires it. (FX-GATE's NaN sweep, 2026-09-25: this read
    // `h >= 0.0f`, which fires on NaN.)
    CGS_ASSERT(!(mfDesiredHeight < 0.0f), "mfDesiredHeight >= 0.0f");               // :719

    const rw::math::vpu::Vector3& lrCameraPosition = lrCamera.mTransform.wAxis;

    rw::math::vpu::Vector3 lStart;
    lStart.x = 0.0f * KF_MIN_TEST_ABOVE_LENGTH + lrCameraPosition.x;
    lStart.y = 1.0f * KF_MIN_TEST_ABOVE_LENGTH + lrCameraPosition.y;
    lStart.z = 0.0f * KF_MIN_TEST_ABOVE_LENGTH + lrCameraPosition.z;
    lStart.w = 0.0f * KF_MIN_TEST_ABOVE_LENGTH + lrCameraPosition.w;

    const f32 lfBelow = ((KF_MIN_TEST_BELOW_LENGTH - mfDesiredHeight) >= 0.0f) ? KF_MIN_TEST_BELOW_LENGTH
                                                                              : mfDesiredHeight;
    rw::math::vpu::Vector3 lEnd;
    lEnd.x = lStart.x - 0.0f * lfBelow;
    lEnd.y = lStart.y - 1.0f * lfBelow;
    lEnd.z = lStart.z - 0.0f * lfBelow;
    lEnd.w = lStart.w - 0.0f * lfBelow;

    lpRequestInterface->LineTestNearest(mLineTest, 2u, 0xFFu, lStart, lEnd,
                                        CgsSceneManager::EntityId(0xFFFFFFFFu),
                                        CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
}

// @0x8220E3A0 (BrnCollisionPolicy.cpp:738 / :739 asserts). When the line found the ground the
// camera's HEIGHT becomes the hit's height plus mfDesiredHeight (vspltw y ; vaddfp ; `vrlimi128
// v13, v0, 4, 3` inserts the y lane only); the box is emptied either way; the answer is the hit
// flag. The timestep is part of the DWARF signature and unused.
bool GroundConstraint::ProcessSceneQueryResults(f32 lfTimestep, Camera& lrCamera)
{
    (void)lfTimestep;
    // `fcmpu cr6, f13(+0x50), flt_82001CC0 (0.0f) ; bge 0x8220E3EC` (0x8220E3C4..0x8220E3CC): the
    // same NaN-skipping polarity as GenerateSceneQueries above.
    CGS_ASSERT(!(mfDesiredHeight < 0.0f), "mfDesiredHeight >= 0.0f");               // :738
    CGS_ASSERT(mLineTest.HasPackage(), "mLineTest.HasPackage()");                   // :739

    const bool lbFoundGround = mLineTest.GetPackage().mbIntersection;
    if (lbFoundGround)
        lrCamera.mTransform.wAxis.y = mLineTest.GetPackage().mPosition.y + mfDesiredHeight;

    mLineTest.Clear();
    return lbFoundGround;
}

}
}
