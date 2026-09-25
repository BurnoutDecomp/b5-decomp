// FX-DIRECTOR2 (crash parity 2026-09-25, CC-13): BehaviourIceAnim::Update's BYSTANDER look space aims the camera.
//
// The runner (run_fxdirector2_iceanim_bystander_looker.py) lifts the revision's PRODUCTION text out of
// BehaviourIceAnim::Update -- the two `if (leLookSpace == ICE::eICE_BYSTANDER_SPACE) { ... }` arms and the
// `mLastCamera = lrCamera;` between them -- into fxd2_iceanim_bystander.inc, and the arm's KF_BYSTANDER_LOOKER_*
// constants into fxd2_iceanim_const.inc. It compiles them inside a fixture whose members carry the behaviour's names:
//   - mLastCamera, a REAL Camera (its four accessors restated from Camera.cpp; the DOF SetParams / GetBlurriness are
//     the real BrnDepthOfField.cpp, linked);
//   - mLooker / mShake, the REAL Utils::Looker / Utils::CameraShake, whose Update bodies are recording stand-ins here;
//   - mKeyAnimController (GetLookPos), mBystanderRef (GetVehicle / Get) and the shared info (GetTimestep / GetRandom),
//     small stand-ins; the Timestep and the Random are the REAL classes, seeded with distinct values.
//
// Against the ARTIST body (BehaviourIceAnim::Update @0x82247108):
//   0x822476A0  GetLookSpace == 0xB (eICE_BYSTANDER_SPACE), else -> 0x8224788C
//   0x822476C8..0x82247708  the orientation rows from mLastCamera (+0x4B0), DepthOfField::SetParams(0.1, 0.2, 0.3, 0.4,
//               the stored blurriness +0x5E4), SetFOV(the stored FOV +0x508)
//   0x82247710  Looker::Parameters::Construct(var_360), then the overrides (block +offset):
//               +0x20 / +0x40 = f31 (flt_82004014 == 0x3DCCCCCD); +0x5F = r21 (1); +0x2C = flt_820054CC (20.0);
//               +0x30 = flt_8200544C (130.0); +0x60 = 2; +0x3C = flt_8200426C (5.0); +0x10 / +0x14 = flt_82001DA0 (0.5);
//               +0x18 = GetLookPos().x * f31, +0x1C = GetLookPos().y * f31 (vmulfp128 0x8224779C / 0x822477E4: one
//               f32 rounding each, ROUNDING_RULE.md rule 4)
//   0x822477F4  Timestep::Get(shared +0x550, `lwz r4, 4(r31)` -- the behaviour's own meTimestepType), splatted
//               (vspltw128 v127, v0, 0 @0x82247844)
//   0x82247808 / 0x82247818 / 0x82247828  VehicleRef::Get(this +0xE10, shared +0x5BC), three times: the first's
//               +0x4A0 AABB (the 0x20-byte memcpy), the second's +0x330 velocity (lvx128 v2), the third's +0x1F0 transform
//   0x82247888  Looker::Update(this +0x660, v1 = the timestep, r4..r9 = the six qwords of *(shared +0x5D4), r10 = the
//               block, the camera, the transform, the velocity, the AABB)
//   0x82247894  Camera::operator=(this +0x4B0, the camera) -- on every path
//   0x822478A8..0x822478E4  (BYSTANDER again) CameraShake::Update(this +0x650, the camera, {0 (flt_82001CC0), 0,
//               1.0 (flt_82001C98), 0.25 (flt_82003F40)}, the shared Random, Timestep::Get(meTimestepType) @0x822478AC,
//               1.0)
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/Camera/Utils/BrnLooker.h"
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

namespace BrnDirector
{
namespace Camera
{
// ---- recording stand-ins --------------------------------------------------------------------------------------
struct LookerCall
{
    const void*               mpLooker;
    f32                       mafTimeStep[4];
    u32                       mauRandomBuffer[8];
    u64                       muRandomSeed;
    u32                       muRandomIndex;
    Utils::Looker::Parameters mParams;
    const void*               mpCamera;
    Matrix44Affine            mCameraTransform;   // the camera as the looker received it
    f32                       mfCameraFOV;
    DepthOfField              mCameraDOF;
    Matrix44Affine            mTarget;
    Vector3                   mTargetVel;
    AABBox                    mAABB;
};
struct ShakeCall
{
    const void*                    mpShake;
    const void*                    mpTransform;
    Utils::CameraShake::Parameters mParams;
    const void*                    mpRandom;
    f32                            mfTimestep;
    f32                            mfSpeedRatio;
};
static LookerCall gLooker;
static ShakeCall  gShake;
static int giLookerUpdates = 0, giShakeUpdates = 0, giGetVehicle = 0, giGetWorld = 0, giLookPos = 0;
static const void* gpGetVehicleInfo = nullptr;

static const f32 KF_LOOKER_ZOOMED_FOV = 0.777f;     // what the stand-in looker leaves in the camera
static const f32 KF_SHAKEN_POSITION_X = 999.0f;     // what the stand-in shake leaves in the camera's position

// Camera.cpp's accessor bodies, restated (Camera.cpp also carries the matrix validators the fixture does not link).
f32 Camera::GetFOV() const { return mfFOV; }
void Camera::SetFOV(f32 lfFOV)
{
    CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f");
    mfFOV = lfFOV;
}
DepthOfField&       Camera::GetDepthOfField()       { return mDepthOfField; }
const DepthOfField& Camera::GetDepthOfField() const { return mDepthOfField; }

namespace Utils
{
    void Looker::Update(VecFloat lvTimeStep, Random lRandom, const Parameters& lrParams, Camera& lrCamera,
                        Matrix44Affine lTarget, Vector3 lTargetVel, AABBox lAABB)
    {
        ++giLookerUpdates;
        gLooker.mpLooker = this;
        std::memcpy(gLooker.mafTimeStep, lvTimeStep.maLanes, sizeof(gLooker.mafTimeStep));
        std::memcpy(gLooker.mauRandomBuffer, lRandom.mauIntegerBuffer, sizeof(gLooker.mauRandomBuffer));
        gLooker.muRandomSeed    = lRandom.muSeed;
        gLooker.muRandomIndex   = lRandom.muOldestBufferIndex;
        gLooker.mParams         = lrParams;
        gLooker.mpCamera        = &lrCamera;
        gLooker.mCameraTransform = lrCamera.mTransform;
        gLooker.mfCameraFOV     = lrCamera.GetFOV();
        gLooker.mCameraDOF      = lrCamera.GetDepthOfField();
        gLooker.mTarget         = lTarget;
        gLooker.mTargetVel      = lTargetVel;
        gLooker.mAABB           = lAABB;
        lrCamera.SetFOV(KF_LOOKER_ZOOMED_FOV);   // the looker's zoom, so the copy-back order is observable
    }

    void CameraShake::Update(Matrix44Affine& lrTransform, const Parameters& lrParams, Random& lrRandom, f32 lfTimestep,
                             f32 lfSpeedRatio)
    {
        ++giShakeUpdates;
        gShake = ShakeCall{ this, &lrTransform, lrParams, &lrRandom, lfTimestep, lfSpeedRatio };
        lrTransform.wAxis.x = KF_SHAKEN_POSITION_X;
    }
}

struct FakeSharedInfo
{
    Timestep                   mTimestep;
    mutable CgsNumeric::Random mRandom;
    const Timestep&     GetTimestep() const { return mTimestep; }
    CgsNumeric::Random* GetRandom() const   { return &mRandom; }
};
struct FakeBystanderRef
{
    const VehicleInfo* mpVehicle;
    const VehicleInfo& GetVehicle(const FakeSharedInfo& lrInfo) const
    {
        ++giGetVehicle;
        gpGetVehicleInfo = &lrInfo;
        return *mpVehicle;
    }
    const VehicleInfo* Get(const void*) const { ++giGetWorld; return mpVehicle; }
};
struct FakeKeyAnimController
{
    Vector3 mLookPos;
    Vector3 GetLookPos() const { ++giLookPos; return mLookPos; }
};

// The arm's [DIAG] witness (BRN_CRASHCAM_DIAG, NOT X360) reads the take through the real KeyAnimController; here it
// only counts that the arm reports around the looker.
static int giDiagBefore = 0, giDiagAfter = 0;
static void BrnDiag_BystanderLookBefore(const Camera&, const Matrix44Affine&) { ++giDiagBefore; }
static void BrnDiag_BystanderLookAfter(const void*, const FakeKeyAnimController&, const Camera&, const Matrix44Affine&)
{
    ++giDiagAfter;
}

namespace
{
#include "fxd2_iceanim_const.inc"
}

// The behaviour's members the arms name, and the arms themselves.
struct IceAnimFixture
{
    Camera                mLastCamera;
    Utils::CameraShake    mShake;
    Utils::Looker         mLooker;
    FakeKeyAnimController mKeyAnimController;
    FakeBystanderRef      mBystanderRef;
    Timestep::EType       meTimestepType;

    Timestep::EType GetTimestepType() const { return meTimestepType; }

    void Run(Camera& lrCamera, const FakeSharedInfo& lrSharedInfo, const void* lpWorld, ICE::eICESpace leLookSpace)
    {
#include "fxd2_iceanim_bystander.inc"
    }
};
}
}

namespace BrnDirector
{
namespace Camera
{
// ---- helpers -----------------------------------------------------------------------------------------------
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}
static u32 Bits(f32 lf)
{
    u32 lu;
    std::memcpy(&lu, &lf, sizeof(lu));
    return lu;
}
static bool SameRow(const Vector3& lrA, const Vector3& lrB)
{
    return Bits(lrA.x) == Bits(lrB.x) && Bits(lrA.y) == Bits(lrB.y) && Bits(lrA.z) == Bits(lrB.z);
}
static bool SameMatrix(const Matrix44Affine& lrA, const Matrix44Affine& lrB)
{
    return SameRow(lrA.xAxis, lrB.xAxis) && SameRow(lrA.yAxis, lrB.yAxis) && SameRow(lrA.zAxis, lrB.zAxis)
        && SameRow(lrA.wAxis, lrB.wAxis);
}
static Vector3 Row(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
{
    Vector3 lRow;
    lRow.x = lfX;
    lRow.y = lfY;
    lRow.z = lfZ;
    lRow.w = lfW;
    return lRow;
}
static Matrix44Affine Matrix(f32 lfBase)
{
    Matrix44Affine lMatrix;
    lMatrix.xAxis = Row(lfBase + 1.0f, lfBase + 2.0f, lfBase + 3.0f, 0.0f);
    lMatrix.yAxis = Row(lfBase + 4.0f, lfBase + 5.0f, lfBase + 6.0f, 0.0f);
    lMatrix.zAxis = Row(lfBase + 7.0f, lfBase + 8.0f, lfBase + 9.0f, 0.0f);
    lMatrix.wAxis = Row(lfBase + 10.0f, lfBase + 11.0f, lfBase + 12.0f, 1.0f);
    return lMatrix;
}

static IceAnimFixture gBehaviour;
static VehicleInfo    gBystander;
static FakeSharedInfo gInfo;
static const int      KI_WORLD_TAG = 0;

static const f32 KF_WORLD_STEP = 1.0f / 60.0f, KF_NO_SLOMO_STEP = 1.0f / 50.0f, KF_GAME_STEP = 1.0f / 30.0f;
static const f32 KF_STORED_FOV = 0.9f, KF_STORED_BLURRINESS = 0.625f, KF_FRAME_FOV = 1.3f;

static void Seed(Camera& lrCamera)
{
    gBehaviour.mLastCamera.mTransform = Matrix(0.0f);
    gBehaviour.mLastCamera.mfFOV = KF_STORED_FOV;
    gBehaviour.mLastCamera.GetDepthOfField().SetParams(1.0f, 2.0f, 3.0f, 4.0f, KF_STORED_BLURRINESS);
    lrCamera.mTransform = Matrix(100.0f);
    lrCamera.mfFOV = KF_FRAME_FOV;
    lrCamera.GetDepthOfField().SetParams(5.0f, 6.0f, 7.0f, 8.0f, 0.125f);
    giLookerUpdates = giShakeUpdates = giGetVehicle = giGetWorld = giLookPos = 0;
    gpGetVehicleInfo = nullptr;
    std::memset(&gLooker, 0, sizeof(gLooker));
    std::memset(&gShake, 0, sizeof(gShake));
}

static int RunAll()
{
    // The bystander the take looks at: a distinct transform, velocity and box.
    gBystander.mRaceCarState.mTransform      = Matrix(200.0f);
    gBystander.mRaceCarState.mLinearVelocity = Row(21.0f, -22.0f, 23.0f, 0.0f);
    gBystander.mAABB.mMin = Row(-1.0f, -2.0f, -3.0f, 0.0f);
    gBystander.mAABB.mMax = Row(4.0f, 5.0f, 6.0f, 0.0f);

    gBehaviour.mKeyAnimController.mLookPos = Row(3.0f, -7.0f, 11.0f, 0.0f);
    gBehaviour.mBystanderRef.mpVehicle = &gBystander;
    gBehaviour.meTimestepType = Timestep::E_GAME;   // the behaviour's own flavour, not E_WORLD
    gBehaviour.mShake.Construct();
    gBehaviour.mLooker.Construct();

    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD]          = KF_WORLD_STEP;
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD_NO_SLOMO] = KF_NO_SLOMO_STEP;
    gInfo.mTimestep.mafTimestep[Timestep::E_GAME]           = KF_GAME_STEP;
    gInfo.mRandom.Construct();
    gInfo.mRandom.muSeed = 0x0123456789ABCDEFull;
    gInfo.mRandom.muOldestBufferIndex = 5u;
    gInfo.mRandom.mauIntegerBuffer[3] = 0xCAFEF00Du;

    // ---- a take looking in the BYSTANDER space ----
    Camera lCamera;
    Seed(lCamera);
    const Matrix44Affine lFrameTransform = lCamera.mTransform;
    gBehaviour.Run(lCamera, gInfo, &KI_WORLD_TAG, ICE::eICE_BYSTANDER_SPACE);

    Check(giLookerUpdates == 1 && gLooker.mpLooker == &gBehaviour.mLooker,
          "K1 the BYSTANDER arm runs Looker::Update once, on the behaviour's own looker (0x82247888, this +0x660)");

    const Utils::Looker::Parameters& lrP = gLooker.mParams;
    Check(Bits(lrP.mfTrackingTolerance) == 0x3DCCCCCDu && Bits(lrP.mfToleranceForDistanceFromTarget) == 0x3DCCCCCDu,
          "K2 tracking tolerance (+0x20) and distance-from-target tolerance (+0x40) = 0.1 (f31, flt_82004014 "
          "0x3DCCCCCD)");
    Check(lrP.mbUseZoom && lrP.meZoomType == Utils::Looker::Parameters::E_ZOOM_SCREEN_REGION && lrP.meZoomType == 2,
          "K3 zoom on (+0x5F = r21 = 1) in the SCREEN-REGION mode (+0x60 = 2)");
    Check(Bits(lrP.mfMinFOVVelocity) == 0x41A00000u && Bits(lrP.mfMaxFOVVelocity) == 0x43020000u,
          "K4 the FOV velocity band 20 .. 130 (+0x2C flt_820054CC 0x41A00000, +0x30 flt_8200544C 0x43020000)");
    Check(Bits(lrP.mfToleranceForDistanceFromIdeal) == 0x40A00000u,
          "K5 distance-from-ideal tolerance 5.0 (+0x3C, flt_8200426C 0x40A00000)");
    Check(Bits(lrP.mfTargetSubjectXSize) == 0x3F000000u && Bits(lrP.mfTargetSubjectYSize) == 0x3F000000u,
          "K6 the subject X / Y size 0.5 (+0x10 / +0x14, flt_82001DA0 0x3F000000)");
    const f32 lfTolerance = 0.1f;
    const f32 lfExpectX = static_cast<f32>(3.0 * static_cast<double>(lfTolerance));    // one rounding (rule 4)
    const f32 lfExpectY = static_cast<f32>(-7.0 * static_cast<double>(lfTolerance));
    Check(Bits(lrP.mfTargetSubjectXScreenOffset) == Bits(lfExpectX)
              && Bits(lrP.mfTargetSubjectYScreenOffset) == Bits(lfExpectY) && giLookPos >= 2,
          "K7 the subject screen offsets = GetLookPos().x * 0.1 (+0x18) and .y * 0.1 (+0x1C), one f32 rounding each "
          "(vmulfp128 0x8224779C / 0x822477E4)");
    Utils::Looker::Parameters lDefaults;
    lDefaults.Construct();
    Check(lrP.mfInitialXLookOffsetRange == lDefaults.mfInitialXLookOffsetRange
              && lrP.mfInitialYLookOffsetRange == lDefaults.mfInitialYLookOffsetRange
              && lrP.mfTargetSubjectSize == lDefaults.mfTargetSubjectSize
              && lrP.mfTrackingSpeed == lDefaults.mfTrackingSpeed
              && lrP.mfTrackingAcceleration == lDefaults.mfTrackingAcceleration
              && lrP.mfDesiredPerceivedDistance == lDefaults.mfDesiredPerceivedDistance
              && lrP.mfDistanceToVelocityFactor == lDefaults.mfDistanceToVelocityFactor
              && lrP.mfOvershootFactor == lDefaults.mfOvershootFactor && lrP.mfMinFOV == lDefaults.mfMinFOV
              && lrP.mfMaxFOV == lDefaults.mfMaxFOV
              && lrP.mfIdealFOVVelocityLerpAmount == lDefaults.mfIdealFOVVelocityLerpAmount
              && lrP.mfStaticDOF == lDefaults.mfStaticDOF && lrP.mfStaticFocalLength == lDefaults.mfStaticFocalLength
              && lrP.mbUseStaticDOF == lDefaults.mbUseStaticDOF
              && lrP.mbInitialiseToLookingAtTarget == lDefaults.mbInitialiseToLookingAtTarget
              && lrP.mbInitialiseToZoomedToTarget == lDefaults.mbInitialiseToZoomedToTarget,
          "K8 every other field keeps Parameters::Construct's default (Construct @0x821F8D80 runs first, 0x82247710)");
    Check(Bits(gLooker.mafTimeStep[0]) == Bits(KF_GAME_STEP) && Bits(gLooker.mafTimeStep[1]) == Bits(KF_GAME_STEP)
              && Bits(gLooker.mafTimeStep[2]) == Bits(KF_GAME_STEP) && Bits(gLooker.mafTimeStep[3]) == Bits(KF_GAME_STEP),
          "K9 the timestep is the behaviour's own flavour (`lwz r4, 4(r31)` @0x822477BC, here E_GAME), splatted to all "
          "four lanes (vspltw128 @0x82247844)");
    Check(gLooker.muRandomSeed == 0x0123456789ABCDEFull && gLooker.muRandomIndex == 5u
              && gLooker.mauRandomBuffer[3] == 0xCAFEF00Du
              && std::memcmp(gLooker.mauRandomBuffer, gInfo.mRandom.mauIntegerBuffer, sizeof(gLooker.mauRandomBuffer)) == 0,
          "K10 the shared Random, by value (the six qwords of *(shared +0x5D4) in r4..r9)");
    const DepthOfField& lrDof = gLooker.mCameraDOF;
    const Matrix44Affine lStored = Matrix(0.0f);
    Check(gLooker.mpCamera == &lCamera
              && SameRow(gLooker.mCameraTransform.xAxis, lStored.xAxis) && SameRow(gLooker.mCameraTransform.yAxis, lStored.yAxis)
              && SameRow(gLooker.mCameraTransform.zAxis, lStored.zAxis)
              && SameRow(gLooker.mCameraTransform.wAxis, lFrameTransform.wAxis)
              && Bits(gLooker.mfCameraFOV) == Bits(KF_STORED_FOV) && lrDof.mfFocusStartDistanceMeters == 0.1f
              && lrDof.mfPerfectFocusStartDistanceMeters == 0.2f && lrDof.mfPerfectFocusEndDistanceMeters == 0.3f
              && lrDof.mfFocusEndDistanceMeters == 0.4f && lrDof.mfBlurriness == KF_STORED_BLURRINESS,
          "K11 the looker aims the frame camera itself (var_3D4 = r27) and sees it AFTER the re-focus: the stored "
          "orientation rows (not the position), the DOF band 0.1 / 0.2 / 0.3 / 0.4 with the stored blurriness, the "
          "stored FOV (0x822476C8..0x82247708)");
    Check(SameMatrix(gLooker.mTarget, gBystander.mRaceCarState.mTransform),
          "K12 the target is the bystander's transform (the third VehicleRef::Get's +0x1F0, 0x82247854)");
    Check(SameRow(gLooker.mTargetVel, gBystander.mRaceCarState.mLinearVelocity),
          "K13 the target velocity is the bystander's linear velocity (the second Get's +0x330, lvx128 v2 @0x8224786C)");
    Check(SameRow(gLooker.mAABB.mMin, gBystander.mAABB.mMin) && SameRow(gLooker.mAABB.mMax, gBystander.mAABB.mMax),
          "K14 the box is the bystander's AABB (the first Get's +0x4A0, the 0x20-byte memcpy @0x82247848)");
    Check(giGetVehicle == 3 && gpGetVehicleInfo == &gInfo && giGetWorld == 0,
          "K15 the bystander is resolved through the shared info three times (VehicleRef::Get 0x82247808 / 0x82247818 "
          "/ 0x82247828)");
    Check(giShakeUpdates == 1 && gShake.mpShake == &gBehaviour.mShake && gShake.mpTransform == &lCamera.mTransform
              && gShake.mParams.mfXYShakeMagnitudeDegs == 0.0f && gShake.mParams.mfZShakeMagnitudeDegs == 0.0f
              && Bits(gShake.mParams.mfXYWobbleMagnitudeDegs) == 0x3F800000u
              && Bits(gShake.mParams.mfWobbleCenteringFactor) == 0x3E800000u && gShake.mpRandom == &gInfo.mRandom
              && gShake.mfSpeedRatio == 1.0f,
          "K16 the shake: this +0x650 over the camera's transform, {0, 0, 1.0 flt_82001C98, 0.25 flt_82003F40}, the "
          "shared Random, speed ratio 1.0 (0x822478A8..0x822478E4)");
    Check(Bits(gShake.mfTimestep) == Bits(KF_GAME_STEP),
          "K17 the shake's timestep is the behaviour's own flavour too (`lwz r4, 4(r31)` @0x822478AC), not E_WORLD");
    Check(Bits(gBehaviour.mLastCamera.GetFOV()) == Bits(KF_LOOKER_ZOOMED_FOV)
              && gBehaviour.mLastCamera.mTransform.wAxis.x != KF_SHAKEN_POSITION_X
              && lCamera.mTransform.wAxis.x == KF_SHAKEN_POSITION_X,
          "K18 the stored camera is copied AFTER the looker and BEFORE the shake (Camera::operator= @0x82247894)");

    // ---- a take looking in another space: neither arm runs ----
    Seed(lCamera);
    const Matrix44Affine lUntouched = lCamera.mTransform;
    gBehaviour.Run(lCamera, gInfo, &KI_WORLD_TAG, ICE::eICE_SCENE_SPACE);
    Check(giLookerUpdates == 0 && giShakeUpdates == 0 && giGetVehicle == 0 && giGetWorld == 0
              && SameMatrix(lCamera.mTransform, lUntouched) && Bits(lCamera.GetFOV()) == Bits(KF_FRAME_FOV)
              && SameMatrix(gBehaviour.mLastCamera.mTransform, lUntouched),
          "K19 another look space: no re-focus, no looker, no shake (`cmpwi r3, 0xB ; bne` @0x822476A4 / 0x822478A4); "
          "the stored camera still takes the frame camera");
    Check(gAsserts == 0, "K20 no assert fired");

    std::printf("FxDirector2IceAnimBystanderLooker: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
}
}

int main()
{
    return BrnDirector::Camera::RunAll();
}
