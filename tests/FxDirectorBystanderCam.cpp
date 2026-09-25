// FX-DIRECTOR (crash parity 2026-09-24): Camera::BehaviourBystanderCam is a REAL Camera::Behaviour.
// The PRODUCTION BehaviourBystanderCam.cpp, Behaviour.cpp, BrnPositionFinder.cpp and BrnVehicleRef.cpp
// (and the two range tests extracted from CameraUtils.cpp) compiled against the revision's own headers,
// with recording stand-ins for the out-of-line leaves, and driven the way the behaviour manager drives a
// pooled behaviour: placement-new into raw storage, then EVERY call through a Behaviour*
// (BehaviourHelper::Prepare @0x82255F48 dispatches slot 0 Construct; PrepareBehaviours slot 1;
// BehaviourHelper::Update slot 2). Before the fix the tree carried three definitions of the class and
// none derived from Camera::Behaviour: the first dispatch read a null vptr (the moment tick's AV).
// The parameter bank's four bystander blocks are checked through the real
// BehaviourParameterBank::Construct.
//
// Checked against the ARTIST asm:
//   Parameters::Construct @0x821F9A00  type 5; shake seed then 0.0 / 1.0 / 0.25; looker seed + zoom on;
//                                      0.5 / 40 / 80 / (2,0,0) / 1.0 / false / true
//   Construct @0x822438E8  base; finder 0/0/1; shake 0; looker 0/0.2/1/1/1/0; Random::Construct;
//                          identity (w row 0); policy Construct then SetTestLookingAt(false);
//                          factor 1.0; mbSetup 1; mbGotPosition 0; mTarget = the player
//   Prepare @0x821F9AD0    SetPrepared; assert mbSetup; true
//   Update @0x82243C80     invalid ref -> Fail(11); policy SetTarget(subject); (re)plant under
//                          NoCutTo 14 (target space / position finder with Fail(6) / Fail(7));
//                          flag bit 1; looker (scaled perceived distance, Random by value); shake 1.0;
//                          squared distance; range Fail(7); occluded -> NoCutTo 17; leaving -> 15
//   SetupTweaker @0x821F9B30  Rig X -0.05 [LEFT_STICK_X], Rig Y 0.05 [LOWER_TRIGGERS], Rig Z 0.05 [LEFT_STICK_Y]
//   SetTarget @0x821F3F80 (VehicleRef::SetToRaceCar @0x821F29D8 + its h:222 assert), SetParameters @0x821F3F10
//   BehaviourParameterBank::Construct @0x8223DC90  the four modelled bystander blocks, and the passenger block
//     (BehaviourPassengerCam::Parameters::Construct inlined at 0x8223DF08..0x8223DF54: type 7 + the
//     CameraImpactEffect::Parameters seed) that BehaviourPassengerCam::SetParameters must accept
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"
#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The two range tests, extracted verbatim from the revision's CameraUtils.cpp.
namespace BrnDirector { namespace Camera { namespace Utils {
#include "bystander_range_tests.inc"
} } }

// BehaviourSharedInfo / VehicleInfo hold RaceCarStates by value (their bodies live in the physics TU).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }

using namespace BrnDirector;
using namespace BrnDirector::Camera;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;
namespace CU = BrnDirector::Camera::Utils;

// ---- recording stand-ins for the leaves ------------------------------------------------------------
static int giPolicyConstructs = 0, giSetTargets = 0, giValidates = 0, giSetTransforms = 0;
static int giLookerUpdates = 0, giShakeUpdates = 0, giWorldMapQueries = 0, giTweakerConstructs = 0;
static std::vector<int> gFailFlags, gNoCutToFlags;
static Matrix44Affine gLastPolicyTransform;
static AABBox gLastPolicyAABB;
static u32 guLastPolicyEntity = 0;

// Looker::Update's arguments, as the behaviour handed them over.
static f32 gafLookerDt[4];
static u64 guLookerRandomSeed = 0;
static f32 gfLookerDesiredPerceivedDistance = 0.0f;
static Matrix44Affine gLookerCameraIn, gLookerTarget;
static Vector3 gLookerVelocity;
static AABBox gLookerAABB;
static Vector3 gLookerZAxisOut = { 0.0f, 0.0f, -1.0f, 0.0f };   // what the stand-in "turns" the camera to

// CameraShake::Update's arguments.
static const void* gpShakeTransform = nullptr;
static const void* gpShakeParams = nullptr;
static const void* gpShakeRandom = nullptr;
static f32 gfShakeDt = 0.0f, gfShakeScale = 0.0f;
static const f32 KF_SHAKE_NUDGE_X = 0.5f;                       // the stand-in's visible wobble

// The world map's safe-position query.
static bool gbWorldMapFound = true;
static Vector3 gWorldMapAnswer = { 0.0f, 0.0f, 0.0f, 0.0f };
static Vector3 gWorldMapPosition, gWorldMapDisplacement;

struct TweakRecord { std::string mName; f32* mpfVariable; f32 mfScale; int miAxis; int miMap; };
static std::vector<TweakRecord> gTweaks;

namespace BrnDirector
{
    bool WorldMap::GetSafePositionNearestPointWithDisplacement(Vector3 lPosition, Vector3 lDisplacement,
                                                                Vector3& lOut) const
    {
        ++giWorldMapQueries;
        gWorldMapPosition = lPosition;
        gWorldMapDisplacement = lDisplacement;
        lOut = gWorldMapAnswer;
        return gbWorldMapFound;
    }
namespace Camera
{
    // The console Construct's visibility-test bytes are 1/0/1 (policy +0x1A0..+0x1A2 = the embedded
    // VisibilityTest's mbTestLookingAt / mbOccluded / mbIsOnScreen); the stand-in does the same, so only
    // the behaviour's own SetTestLookingAt(false) can lower the first.
    void VisibilityCollisionPolicy::Construct()
    {
        ++giPolicyConstructs;
        mVisibilityTest.mbTestLookingAt = true;
        mVisibilityTest.mbOccluded      = false;
        mVisibilityTest.mbIsOnScreen    = true;
    }
    void VisibilityCollisionPolicy::SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                                              CgsSceneManager::EntityId lTargetEntityId)
    {
        ++giSetTargets;
        gLastPolicyTransform = lTargetTransform;
        gLastPolicyAABB = lTargetAABB;
        guLastPolicyEntity = static_cast<u32>(lTargetEntityId);
    }
    // The policy's two per-frame scene-query virtuals (vtable slots 0/1, @0x822402F8 / @0x82224530; bodies in
    // BrnVisibilityCollisionPolicy.cpp, not compiled here). The behaviour under test never calls them --
    // BehaviourManager's collision pass does -- so they are defined only for the policy's vtable to link.
    void VisibilityCollisionPolicy::GenerateSceneQueries(const CollisionPolicySharedInfo&, Camera&) {}
    void VisibilityCollisionPolicy::ProcessSceneQueryResults(const CollisionPolicySharedInfo&, Camera&) {}
    void ValidityAccount::SetFlag(s32 leFlag) { gFailFlags.push_back(leFlag); }
    void ValidityAccount::SetNoCutFromFlag(s32) {}
    void ValidityAccount::SetNoCutToFlag(s32 leFlag) { gNoCutToFlags.push_back(leFlag); }
    void CameraState::ClearFlag(u32 luIndex) { mCurrentFlags.UnSetBit(luIndex); }
    void Camera::SetFOV(f32 lfFOV) { mfFOV = lfFOV; }
    void Camera::SetTransform(const Matrix44Affine& lrTransform) { ++giSetTransforms; mTransform = lrTransform; }
    Matrix44Affine* Camera::ValidateTransformWithDebugInfo() { ++giValidates; return &mTransform; }
namespace Utils
{
    void Looker::Update(VecFloat lvTimeStep, Random lRandom, const Parameters& lrParams, Camera& lrCamera,
                        Matrix44Affine lTarget, Vector3 lTargetVel, AABBox lAABB)
    {
        ++giLookerUpdates;
        for (int i = 0; i < 4; ++i) gafLookerDt[i] = lvTimeStep.maLanes[i];
        guLookerRandomSeed = lRandom.muSeed;
        lRandom.muSeed = 0;                                      // a by-value copy: the behaviour's must not move
        gfLookerDesiredPerceivedDistance = lrParams.mfDesiredPerceivedDistance;
        gLookerCameraIn = lrCamera.mTransform;
        gLookerTarget = lTarget;
        gLookerVelocity = lTargetVel;
        gLookerAABB = lAABB;
        lrCamera.mTransform.zAxis = gLookerZAxisOut;             // "turn" the camera
    }
    void CameraShake::Update(Matrix44Affine& lrTransform, const Parameters& lrParams, Random& lrRandom,
                             f32 lfTimestep, f32 lfSpeedRatio)
    {
        ++giShakeUpdates;
        gpShakeTransform = &lrTransform;
        gpShakeParams = &lrParams;
        gpShakeRandom = &lrRandom;
        gfShakeDt = lfTimestep;
        gfShakeScale = lfSpeedRatio;
        lrTransform.wAxis.x += KF_SHAKE_NUDGE_X;
    }
    void Tweaker::Construct() { ++giTweakerConstructs; }
    void Tweaker::AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32 lfScale, EAxis leAxisToMapTo, EMap leMap)
    {
        gTweaks.push_back(TweakRecord{ lpcName, lpfVariableToTweak, lfScale, static_cast<int>(leAxisToMapTo),
                                       static_cast<int>(leMap) });
    }
}
}
}

// The bank's OTHER per-block Constructs live in TUs this test does not link; the checks read only the
// four bystander blocks, so these five stand-ins do nothing.
void BrnDirector::Camera::BehaviourGameplayBumper::Parameters::Construct() {}
void BrnDirector::Camera::BehaviourGameplayExternal::Parameters::Construct() {}
void BrnDirector::Camera::Utils::PositionLag::Parameters::Construct() {}
void BrnDirector::Camera::BehaviourFixedCam::Parameters::Construct() {}
void BrnDirector::Camera::BehaviourSpirallingDeathcam::Parameters::Construct() {}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 1e-4f; }
static bool NearV(const Vector3& lrA, const Vector3& lrB) { return Near(lrA.x, lrB.x) && Near(lrA.y, lrB.y) && Near(lrA.z, lrB.z); }
static bool NearM(const Matrix44Affine& lrA, const Matrix44Affine& lrB)
{
    return NearV(lrA.xAxis, lrB.xAxis) && NearV(lrA.yAxis, lrB.yAxis) && NearV(lrA.zAxis, lrB.zAxis) &&
           NearV(lrA.wAxis, lrB.wAxis);
}
static bool Has(const std::vector<int>& lrFlags, int liFlag)
{
    for (int liEach : lrFlags) if (liEach == liFlag) return true;
    return false;
}

// The behaviour manager's view of a pooled behaviour: storage, placement-new, a Behaviour*.
alignas(16) static u8 gaSlot[sizeof(BehaviourBystanderCam) + 16];

static BehaviourBystanderCam* Pool()
{
    std::memset(gaSlot, 0xCD, sizeof(gaSlot));
    return new (gaSlot) BehaviourBystanderCam();
}

// The world: eight race cars, car 0 the player, cars 0 and 2 in the used set.
static VehicleInfo gaCars[8];
static AllVehicleData gWorld;
static WorldMap* gpWorldMap = nullptr;
alignas(16) static u8 gaWorldMap[sizeof(WorldMap) + 16];
static BehaviourSharedInfo gInfo;
static BrnDirector::Camera::Camera gCamera;
static BehaviourSharedPrepareReleaseInfo gPrepareInfo = {};

static void PlaceCar(int liCar, const Vector3& lrPosition, const Vector3& lrVelocity)
{
    BrnPhysics::Vehicle::RaceCarState& lrState = gaCars[liCar].mRaceCarState;
    lrState.mTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lrState.mTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lrState.mTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lrState.mTransform.wAxis = lrPosition;
    lrState.mLinearVelocity = lrVelocity;
}

static BehaviourBystanderCam* Fresh(const BehaviourBystanderCam::Parameters& lrParams, EActiveRaceCarIndex leCar)
{
    BehaviourBystanderCam* lpCam = Pool();
    Behaviour* lpBehaviour = lpCam;
    lpBehaviour->Construct();
    lpCam->SetParameters(&lrParams);
    lpCam->SetTarget(leCar);
    lpBehaviour->Prepare(gPrepareInfo);
    lpCam->PreUpdate();
    return lpCam;
}

static void Frame(BehaviourBystanderCam* lpCam)
{
    gFailFlags.clear();
    gNoCutToFlags.clear();
    lpCam->PreUpdate();
    static_cast<Behaviour*>(lpCam)->Update(gCamera, gInfo);
}

int main()
{
    Check(std::is_base_of<Behaviour, BehaviourBystanderCam>::value && std::is_polymorphic<BehaviourBystanderCam>::value,
          "BehaviourBystanderCam derives from Camera::Behaviour (DWARF BehaviourBystanderCam.h:107) and has a vtable");

    // ---- the two range tests (CameraUtils.cpp:1376 / :1399) ----------------------------------------
    Check(!CU::TargetOutsideRange(Vector3{ 0, 0, 0, 0 }, Vector3{ 3, 4, 0, 0 }, 5.0f) &&
          CU::TargetOutsideRange(Vector3{ 0, 0, 0, 0 }, Vector3{ 3, 4, 0, 0 }, 4.99f),
          "TargetOutsideRange: |p - t|^2 > range^2 (25 is not > 25)");
    Check(!CU::TargetWillExceedRangeInXSecs(Vector3{ 0, 0, 0, 0 }, Vector3{ 3, 0, 0, 0 }, Vector3{ 2, 0, 0, 0 }, 4.0f, 0.5f) &&
          CU::TargetWillExceedRangeInXSecs(Vector3{ 0, 0, 0, 0 }, Vector3{ 3, 0, 0, 0 }, Vector3{ 2, 0, 0, 0 }, 4.0f, 0.6f),
          "TargetWillExceedRangeInXSecs: |t + v*secs - p|^2 > range^2");

    // ---- Parameters::Construct @0x821F9A00 ---------------------------------------------------------
    static BehaviourBystanderCam::Parameters lDefaults;
    std::memset(static_cast<void*>(&lDefaults), 0xCD, sizeof(lDefaults));
    lDefaults.Construct();
    CU::Looker::Parameters lLookerSeed;
    std::memset(static_cast<void*>(&lLookerSeed), 0xCD, sizeof(lLookerSeed));
    lLookerSeed.Construct();
    Check(lDefaults.GetType() == static_cast<u32>(eBehaviourBystanderCam) && lDefaults.GetDebugName() == nullptr,
          "Parameters::Construct: type 5 (stw 5, 0(r8)), no debug name");
    Check(lDefaults.mShakeParams.mfXYShakeMagnitudeDegs == 0.0f && lDefaults.mShakeParams.mfZShakeMagnitudeDegs == 0.0f &&
          lDefaults.mShakeParams.mfXYWobbleMagnitudeDegs == 1.0f && lDefaults.mShakeParams.mfWobbleCenteringFactor == 0.25f,
          "Parameters::Construct: the shake seed re-tuned to 0.0 / (0.0) / 1.0 / 0.25 (flt_82001CC0 / flt_82001C98 / flt_82003F40)");
    Check(lDefaults.mLookerParams.mbUseZoom && lDefaults.mLookerParams.mfDesiredPerceivedDistance == lLookerSeed.mfDesiredPerceivedDistance &&
          lDefaults.mLookerParams.mfTrackingSpeed == lLookerSeed.mfTrackingSpeed &&
          lDefaults.mLookerParams.mfMaxFOV == lLookerSeed.mfMaxFOV && lDefaults.mLookerParams.mbInitialiseToLookingAtTarget,
          "Parameters::Construct: the looker block is Looker::Parameters::Construct's (bl 0x821F8D80) with mbUseZoom on (+0x77)");
    Check(lDefaults.mfVelocityInfluenceOnPosition == 0.5f && lDefaults.mfMaxInitialDistanceKM == 40.0f &&
          lDefaults.mfDistanceForFailKM == 80.0f && lDefaults.mfTargetSpaceX == 2.0f && lDefaults.mfTargetSpaceY == 0.0f &&
          lDefaults.mfTargetSpaceZ == 0.0f && lDefaults.mfHeight == 1.0f &&
          !lDefaults.mbUseTargetSpaceInsteadOfPositionFinder && lDefaults.mbUseRangeTesting,
          "Parameters::Construct: 0.5 / 40 / 80 / (2, 0, 0) / 1.0 / false / true (+0x7C..+0x99)");

    // ---- BehaviourParameterBank::Construct @0x8223DC90: the four modelled bystander blocks ------------
    static BrnDirector::Camera::BehaviourParameterBank lBank;
    std::memset(static_cast<void*>(&lBank), 0xCD, sizeof(lBank));
    lBank.Construct();
    const BehaviourBystanderCam::Parameters& lrFar = lBank.GetBystanderCamMomentParams();
    const BehaviourBystanderCam::Parameters& lrClose = lBank.GetBystanderCamCloseMomentParams();
    const BehaviourBystanderCam::Parameters& lrJumpLeft = lBank.GetPlayerJumpingBystanderShotParams(0);
    const BehaviourBystanderCam::Parameters& lrJumpBehind = lBank.GetPlayerJumpingBystanderShotParams(2);
    Check(lrFar.GetType() == 5u && lrFar.mLookerParams.mfTrackingTolerance == 0.5f &&
          lrFar.mLookerParams.mfMinFOVVelocity == 120.0f && lrFar.mLookerParams.mfMaxFOVVelocity == 130.0f &&
          lrFar.mLookerParams.mfDesiredPerceivedDistance == 8.0f &&
          lrFar.mLookerParams.mfToleranceForDistanceFromIdeal == 20.0f &&
          lrFar.mLookerParams.mfToleranceForDistanceFromTarget == 0.1f && lrFar.mfVelocityInfluenceOnPosition == 0.75f &&
          lrFar.mfMaxInitialDistanceKM == 0.04f && lrFar.mfDistanceForFailKM == 0.06f &&
          lrFar.mLookerParams.mbUseZoom && lrFar.mfHeight == 1.0f && !lrFar.mbUseTargetSpaceInsteadOfPositionFinder &&
          lrFar.mbUseRangeTesting && lrFar.mShakeParams.mfWobbleCenteringFactor == 0.25f,
          "bank Far (slot 5, +0x1024): Construct + 0.5 / 120 / 130 / 8 / 20 / 0.1 / 0.75 / 0.04 / 0.06 (0x8223E220..0x8223E25C)");
    Check(lrClose.mLookerParams.mfDesiredPerceivedDistance == 4.0f && lrClose.mLookerParams.mfMinFOVVelocity == 120.0f &&
          lrClose.mfMaxInitialDistanceKM == 0.04f && lrClose.mfDistanceForFailKM == 0.06f &&
          lrClose.mfVelocityInfluenceOnPosition == 0.75f && lrClose.GetType() == 5u,
          "bank Close (slot 3, +0xEEC) = memcpy(Far) (0x8223E260) with perceived distance 4.0 (0x8223E268)");
    Check(lrJumpLeft.GetType() == 5u && lrJumpLeft.mLookerParams.mfTrackingTolerance == 0.2f &&
          lrJumpLeft.mLookerParams.mfTrackingSpeed == 0.1f && lrJumpLeft.mLookerParams.mfDesiredPerceivedDistance == 5.0f &&
          !lrJumpLeft.mLookerParams.mbUseZoom && lrJumpLeft.mfDistanceForFailKM == 0.0125f &&
          lrJumpLeft.mfTargetSpaceX == 2.0f && lrJumpLeft.mfTargetSpaceY == -2.0f && lrJumpLeft.mfTargetSpaceZ == 4.0f &&
          lrJumpLeft.mbUseTargetSpaceInsteadOfPositionFinder && !lrJumpLeft.mbUseRangeTesting &&
          lrJumpLeft.mfMaxInitialDistanceKM == 40.0f,
          "bank JumpLeft (slot 0, +0xD18): 0.2 / 0.1 / 5 / no zoom / 0.0125 / (2, -2, 4) / target space / no range test");
    Check(lrJumpBehind.GetType() == 5u && lrJumpBehind.mfTargetSpaceX == 1.1f && lrJumpBehind.mfTargetSpaceY == -0.78f &&
          lrJumpBehind.mfTargetSpaceZ == -3.31f && lrJumpBehind.mbUseTargetSpaceInsteadOfPositionFinder &&
          !lrJumpBehind.mbUseRangeTesting && lrJumpBehind.mLookerParams.mfDesiredPerceivedDistance == 5.0f,
          "bank JumpFromBehind (slot 2, +0xE50): (1.1, -0.78, -3.31), target space, no range test");

    // ---- the passenger block (record +8660, bank +0x21E4) ---------------------------------------------
    CU::CameraImpactEffect::Parameters lImpactSeed;
    std::memset(static_cast<void*>(&lImpactSeed), 0xCD, sizeof(lImpactSeed));
    lImpactSeed.Construct();
    Check(lImpactSeed.mShakeParams.mfXYShakeMagnitudeDegs == 0.06f && lImpactSeed.mShakeParams.mfZShakeMagnitudeDegs == 0.0f &&
          lImpactSeed.mShakeParams.mfXYWobbleMagnitudeDegs == 1.15f && lImpactSeed.mShakeParams.mfWobbleCenteringFactor == 0.11f &&
          lImpactSeed.mfShakeDecayFactor == 0.05f && lImpactSeed.mfShakeMagnitude == 15.0f &&
          lImpactSeed.mfShakeFrequencyScale == 5.0f,
          "CameraImpactEffect::Parameters::Construct (h:143): shake seed, then 0.05 / 15.0 / 5.0 (820047C8 / 820047C4 / 8200426C)");
    const BehaviourPassengerCam::Parameters& lrPassenger = lBank.GetPassengerCamMomentParams();
    Check(lrPassenger.GetType() == static_cast<u32>(eBehaviourPassengerCam) && lrPassenger.GetDebugName() == nullptr &&
          std::memcmp(&lrPassenger.mImpactParams, &lImpactSeed, sizeof(lImpactSeed)) == 0,
          "bank passenger block (+0x21E4): type 7, no name, the impact seed (0x8223DF08..0x8223DF54) -- was zeroed, type 0");
    alignas(16) static u8 laPassengerSlot[sizeof(BehaviourPassengerCam) + 16];
    std::memset(laPassengerSlot, 0xCD, sizeof(laPassengerSlot));
    BehaviourPassengerCam* lpPassenger = new (laPassengerSlot) BehaviourPassengerCam();
    static_cast<Behaviour*>(lpPassenger)->Construct();
    const unsigned luAssertsBeforePassenger = gAsserts;
    lpPassenger->SetParameters(&lrPassenger);
    Check(gAsserts == luAssertsBeforePassenger && lpPassenger->mpParameters == &lrPassenger,
          "BehaviourPassengerCam::SetParameters takes the bank's block without its type assert "
          "(MomentPassengerSeesAction's shot)");

    // ---- the world ----------------------------------------------------------------------------------
    gWorld.Construct();
    gWorld.mpRaceCars = gaCars;
    gWorld.mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
    gWorld.mUsedRaceCars.SetBit(0);
    gWorld.mUsedRaceCars.SetBit(2);
    gpWorldMap = reinterpret_cast<WorldMap*>(gaWorldMap);
    gInfo.mpAllVehicleData = &gWorld;
    gInfo.mpWorldMap = gpWorldMap;
    gInfo.mTimestep.mafTimestep[BrnDirector::Timestep::E_WORLD] = 0.02f;
    gInfo.mTimestep.mafTimestep[BrnDirector::Timestep::E_WORLD_NO_SLOMO] = 0.5f;
    gInfo.mTimestep.mafTimestep[BrnDirector::Timestep::E_GAME] = 0.25f;
    PlaceCar(0, Vector3{ 0.0f, 0.0f, 0.0f, 1.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    PlaceCar(2, Vector3{ 100.0f, 5.0f, 200.0f, 1.0f }, Vector3{ 10.0f, 0.0f, 0.0f, 0.0f });
    gaCars[2].mRaceCarState.mEntityId.muValue = 0x2222;
    gaCars[2].mAABB.mMin = Vector3{ -1.0f, -0.5f, -2.0f, 0.0f };
    gaCars[2].mAABB.mMax = Vector3{ 1.0f, 0.5f, 2.0f, 0.0f };

    // ---- 1. Construct (slot 0) through a Behaviour* -----------------------------------------------
    BehaviourBystanderCam* lpCam = Pool();
    Behaviour* lpBehaviour = lpCam;
    lpBehaviour->Construct();
    CgsNumeric::Random lReferenceRandom;
    std::memset(static_cast<void*>(&lReferenceRandom), 0, sizeof(lReferenceRandom));
    lReferenceRandom.Construct();
    Check(giPolicyConstructs == 1 && !lpCam->mCollisionPolicy.mVisibilityTest.mbTestLookingAt,
          "Construct: VisibilityCollisionPolicy::Construct, then SetTestLookingAt(false) (stb 0, 0x270 after the policy's stb 1)");
    Check(!lpCam->mbIsPrepared && !lpCam->mbHasFailed && !lpCam->mbTweakerAttached && !lpCam->mbCanSwitchToMeNow &&
          !lpCam->mbCanSwitchFromMeNow && lpCam->mpcDebugParametersName == nullptr &&
          lpCam->meTimestepType == BrnDirector::Timestep::E_WORLD,
          "Construct: the base's Construct (+0x04 = 0, +0x08..+0x0C = 0, +0x10 = 0)");
    Check(!lpCam->mPositionFinder.mbIsInitialised && !lpCam->mPositionFinder.mbFoundPosition &&
          lpCam->mPositionFinder.mbConstructed,
          "Construct: PositionFinder::Construct (stb 0 +0x90, stb 0 +0x91, stb 1 +0x92)");
    Check(lpCam->mShake.mfCurrentWobbleX == 0.0f && lpCam->mShake.mfCurrentWobbleY == 0.0f &&
          lpCam->mShake.mfCurrentWobbleXVel == 0.0f && lpCam->mShake.mfCurrentWobbleYVel == 0.0f,
          "Construct: CameraShake::Construct (four stfs 0.0 +0xA0..+0xAC)");
    Check(lpCam->mLooker.mfSlerpFactor == 0.0f && lpCam->mLooker.mfAssessmentTime == 0.2f &&
          lpCam->mLooker.mbFirstFrame && lpCam->mLooker.mbAssessingFOV && lpCam->mLooker.mbConstructed &&
          !lpCam->mLooker.mbForceZoomTargetUpdate,
          "Construct: Looker::Construct (0.0 +0xB0, 0.2 +0xC0, 1/1/1/0 +0xCC..+0xCF)");
    Check(std::memcmp(&lpCam->mRandom, &lReferenceRandom, 44) == 0 && lpCam->mRandom.muSeed != 0,
          "Construct: CgsNumeric::Random::Construct (the default seed, the 1.0 slot and seven LCG draws, the index bump)");
    Check(lpCam->mTransform.xAxis.x == 1.0f && lpCam->mTransform.yAxis.y == 1.0f && lpCam->mTransform.zAxis.z == 1.0f &&
          lpCam->mTransform.wAxis.x == 0.0f && lpCam->mTransform.wAxis.w == 0.0f && lpCam->mTransform.xAxis.y == 0.0f,
          "Construct: mTransform = identity rows, position row (0,0,0,0) (Matrix44Affine::SetIdentity)");
    Check(lpCam->mfPerceivedDistanceModificationFactor == 1.0f && lpCam->mbSetup && !lpCam->mbGotPosition,
          "Construct: factor 1.0 (+0x358), mbSetup 1 (+0x35C), mbGotPosition 0 (+0x35D)");
    Check(lpCam->mTarget.meType == BrnDirector::VehicleRef::E_PLAYER_CAR && lpCam->mTarget.miRaceCarIndex == -1 &&
          lpCam->mTarget.muRef == 0 && lpCam->mTarget.mbSet,
          "Construct: mTarget = VehicleRef::SetToPlayer (+0x340 = 0, +0x344 = -1, +0x348 = 0, +0x34C = 1)");
    Check(std::string(lpBehaviour->GetName()) == "BehaviourBystanderCam", "GetName (slot 7) @0x821F9C78");
    Check(lpBehaviour->GetCollisionPolicy() == &lpCam->mCollisionPolicy,
          "GetCollisionPolicy (slot 5) returns the embedded policy (addi r3, r3, 0xD0)");

    // ---- 2. SetParameters / SetTarget / Prepare ----------------------------------------------------
    static BehaviourBystanderCam::Parameters lFar;
    lFar = lrFar;
    lFar.SetDebugName("Bystander Far");
    lpCam->SetParameters(&lFar);
    Check(lpCam->mpParameters == &lFar && std::string(lpCam->GetDebugParametersName()) == "Bystander Far" && gAsserts == 0,
          "SetParameters: stores the block (+0x350) and its debug name (+0x10)");
    lpCam->mbSetup = false;
    lpCam->SetTarget(static_cast<EActiveRaceCarIndex>(2));
    Check(lpCam->mTarget.meType == BrnDirector::VehicleRef::E_RACE_CAR && lpCam->mTarget.miRaceCarIndex == 2 &&
          lpCam->mTarget.muRef == 0 && lpCam->mTarget.mbSet && lpCam->mbSetup && gAsserts == 0,
          "SetTarget(2): VehicleRef::SetToRaceCar (+0x344 = 2, +0x34C = 1, +0x340 = 1, +0x348 = 0), mbSetup = 1");
    Check(lpBehaviour->Prepare(gPrepareInfo) && lpCam->mbIsPrepared && gAsserts == 0, "Prepare (slot 1): SetPrepared, true");

    // ---- 3. Update, position-finder shot (the bank's Far block) -------------------------------------
    gWorldMapAnswer = Vector3{ 130.0f, 5.0f, 200.0f, 1.0f };
    gbWorldMapFound = true;
    lpCam->PreUpdate();
    gFailFlags.clear();
    gNoCutToFlags.clear();
    const bool lbUpdated = lpBehaviour->Update(gCamera, gInfo);
    Check(lbUpdated, "Update (slot 2) returns true");
    Check(giSetTargets == 1 && NearM(gLastPolicyTransform, gaCars[2].mRaceCarState.mTransform) &&
          guLastPolicyEntity == 0x2222 && NearV(gLastPolicyAABB.mMax, gaCars[2].mAABB.mMax),
          "Update: the policy's target is the SUBJECT's (mTarget's) transform, AABB and entity id");
    Check(giWorldMapQueries == 1 && NearV(gWorldMapPosition, Vector3{ 100.0f, 5.0f, 200.0f, 0.0f }) &&
          NearV(gWorldMapDisplacement, Vector3{ 7.5f, 0.0f, 0.0f, 0.0f }),
          "Update: FindPosition(subject position, velocity * mfVelocityInfluenceOnPosition 0.75), then the world-map query");
    Check(Has(gNoCutToFlags, 14) && !lpCam->mbCanSwitchToMeNow && lpCam->mbGotPosition && gFailFlags.empty(),
          "Update: planting raises NoCutTo 14 'Finding position' (mbCanSwitchToMeNow = 0) and latches mbGotPosition");
    Check(NearV(gLookerCameraIn.wAxis, Vector3{ 130.0f, 6.0f, 200.0f, 0.0f }),
          "Update: the camera stands at the found point lifted by GetVector3_YAxis() * mfHeight (1.0)");
    Check((gCamera.mState_uFlags & 2) != 0, "Update: not failed -> camera state flag E_FLAG_VALID (ori 2 on +0x140)");
    Check(giLookerUpdates == 1 && gafLookerDt[0] == 0.02f && gafLookerDt[3] == 0.02f &&
          gfLookerDesiredPerceivedDistance == 8.0f && NearM(gLookerTarget, gaCars[2].mRaceCarState.mTransform) &&
          NearV(gLookerVelocity, Vector3{ 10.0f, 0.0f, 0.0f, 0.0f }) && NearV(gLookerAABB.mMin, gaCars[2].mAABB.mMin),
          "Update: Looker::Update(VecFloat(dt E_WORLD), mRandom, params' looker block, camera, subject transform/velocity/AABB)");
    Check(guLookerRandomSeed == lpCam->mRandom.muSeed && lpCam->mRandom.muSeed == lReferenceRandom.muSeed,
          "Update: the looker gets mRandom BY VALUE (the six ld's) -- the behaviour's generator does not move");
    Check(giSetTransforms == 1 && giValidates == 1, "Update: Camera::SetTransform(mTransform), ValidateTransformWithDebugInfo");
    Check(NearV(lpCam->mTransform.zAxis, gLookerZAxisOut) && Near(gCamera.mTransform.wAxis.x, 130.0f + KF_SHAKE_NUDGE_X) &&
          Near(lpCam->mTransform.wAxis.x, 130.0f),
          "Update: mTransform takes the looker's result back BEFORE the shake; the shake moves only the camera");
    Check(giShakeUpdates == 1 && gpShakeTransform == &gCamera.mTransform && gpShakeParams == &lFar.mShakeParams &&
          gpShakeRandom == &lpCam->mRandom && gfShakeDt == 0.02f && gfShakeScale == 1.0f,
          "Update: CameraShake::Update(camera transform, params' shake block, mRandom, dt, 1.0 flt_82001C98)");
    Check(Near(lpCam->GetSquaredDistanceToTarget(), 901.0f),
          "Update: mfSquaredDistanceToTarget = |subject - camera position|^2 (30^2 + 1^2) -- GetSquaredDistanceToTarget");
    Check(!Has(gNoCutToFlags, 15) && !Has(gNoCutToFlags, 17) && !lpCam->mbHasFailed,
          "Update: in range (60 m), not occluded, not leaving within 0.5 s -> no further flags");

    // ---- 4. the shot holds; then the subject leaves ----------------------------------------------
    Frame(lpCam);
    Check(giWorldMapQueries == 1 && !Has(gNoCutToFlags, 14) && lpCam->mbCanSwitchToMeNow,
          "Update: once planted (mbGotPosition), no re-plant and no NoCutTo 14");
    PlaceCar(2, Vector3{ 170.0f, 5.0f, 200.0f, 1.0f }, Vector3{ 100.0f, 0.0f, 0.0f, 0.0f });
    Frame(lpCam);
    Check(Has(gNoCutToFlags, 15) && !lpCam->mbCanSwitchToMeNow && !lpCam->mbHasFailed,
          "Update: will be out of range in 0.5 s (flt_82001DA0) -> NoCutTo 15 'Subject about to leave frame'");
    lpCam->mCollisionPolicy.mVisibilityTest.mbOccluded = true;
    Frame(lpCam);
    Check(Has(gNoCutToFlags, 17) && !Has(gNoCutToFlags, 15) && !lpCam->mbCanSwitchToMeNow,
          "Update: visibility interrupted -> NoCutTo 17 'Subject occluded' (and not 15: else-if)");
    lpCam->mCollisionPolicy.mVisibilityTest.mbOccluded = false;
    PlaceCar(2, Vector3{ 200.0f, 5.0f, 200.0f, 1.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    Frame(lpCam);
    Check(Has(gFailFlags, 7) && lpCam->mbHasFailed && lpCam->mbCanSwitchFromMeNow && !lpCam->mbCanSwitchToMeNow &&
          (gCamera.mState_uFlags & 2) == 0,
          "Update: subject beyond mfDistanceForFailKM * 1000 (60 m) -> Fail(7) (flag 7, failed, camera flag 1 cleared)");
    const f32 lfFailedDistance = lpCam->GetSquaredDistanceToTarget();
    Frame(lpCam);
    Check(gFailFlags.empty() && gNoCutToFlags.empty() && Near(lfFailedDistance, 4901.0f) &&
          (gCamera.mState_uFlags & 2) == 0,
          "Update: failed -> the distance is still measured, then return (no range test, no flags, no flag bit 1)");

    // ---- 5. no roadside: Fail(6), and a failed re-plant raises NoCutTo 14 WITHOUT an assert -----------
    PlaceCar(2, Vector3{ 100.0f, 5.0f, 200.0f, 1.0f }, Vector3{ 10.0f, 0.0f, 0.0f, 0.0f });
    gbWorldMapFound = false;
    lpCam = Fresh(lFar, static_cast<EActiveRaceCarIndex>(2));
    Frame(lpCam);
    Check(Has(gFailFlags, 6) && lpCam->mbHasFailed && !lpCam->mbGotPosition,
          "Update: the position finder found nothing -> Fail(6) \"Couldn't find roadside\"");
    const unsigned luAssertsBefore = gAsserts;
    Frame(lpCam);
    Check(Has(gNoCutToFlags, 14) && gAsserts == luAssertsBefore && gFailFlags.size() == 1 && Has(gFailFlags, 6),
          "Update: a failed behaviour still re-plants -- SetCantSwitchToMeNow(14) with NO assert (the console's inlined copy has none)");
    gbWorldMapFound = true;

    // ---- 6. found, but too far from the subject: Fail(7) -------------------------------------------
    gWorldMapAnswer = Vector3{ 150.0f, 5.0f, 200.0f, 1.0f };
    lpCam = Fresh(lFar, static_cast<EActiveRaceCarIndex>(2));
    Frame(lpCam);
    Check(Has(gFailFlags, 7) && lpCam->mbHasFailed && !lpCam->mbGotPosition,
          "Update: the roadside point is past mfMaxInitialDistanceKM * 1000 (40 m) -> Fail(7), not planted");

    // ---- 7. an invalid reference: the inlined Fail(11) --------------------------------------------
    const int liSetTargetsBefore = giSetTargets;
    lpCam = Fresh(lFar, static_cast<EActiveRaceCarIndex>(5));
    gCamera.mState_uFlags |= 2;
    Frame(lpCam);
    Check(Has(gFailFlags, 11) && lpCam->mbHasFailed && lpCam->mbCanSwitchFromMeNow && !lpCam->mbCanSwitchToMeNow &&
          (gCamera.mState_uFlags & 2) == 0 && giSetTargets == liSetTargetsBefore,
          "Update: car 5 is not in the used set -> Fail(11) 'Invalid vehicle ref' and nothing else");

    // ---- 8. target space (the bank's JumpLeft block): TransformPoint, no finder, no range test -------
    static BehaviourBystanderCam::Parameters lJumpLeft;
    lJumpLeft = lrJumpLeft;
    BrnPhysics::Vehicle::RaceCarState& lrCar2 = gaCars[2].mRaceCarState;
    lrCar2.mTransform.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
    lrCar2.mTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lrCar2.mTransform.zAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lrCar2.mTransform.wAxis = Vector3{ 100.0f, 5.0f, 200.0f, 1.0f };
    const int liQueriesBefore = giWorldMapQueries;
    lpCam = Fresh(lJumpLeft, static_cast<EActiveRaceCarIndex>(2));
    Frame(lpCam);
    Check(NearV(gLookerCameraIn.wAxis, Vector3{ 104.0f, 3.0f, 198.0f, 0.0f }) && lpCam->mbGotPosition &&
          giWorldMapQueries == liQueriesBefore && Has(gNoCutToFlags, 14) && gfLookerDesiredPerceivedDistance == 5.0f,
          "Update: target space -> TransformPoint(subject, (2, -2, 4)) = (104, 3, 198), no position finder");
    PlaceCar(2, Vector3{ 5000.0f, 5.0f, 200.0f, 1.0f }, Vector3{ 1000.0f, 0.0f, 0.0f, 0.0f });
    Frame(lpCam);
    Check(gFailFlags.empty() && !Has(gNoCutToFlags, 15) && !lpCam->mbHasFailed,
          "Update: mbUseRangeTesting false -> neither the fail range nor the leave-frame test runs");

    // ---- 9. the tweaker re-plants every frame ---------------------------------------------------------
    lpCam->SetTweakerAttached(true);
    Frame(lpCam);
    Check(Has(gNoCutToFlags, 14) && NearV(lpCam->mTransform.wAxis, Vector3{ 5002.0f, 3.0f, 204.0f, 0.0f }),
          "Update: tweaker attached (+0x0A) -> re-plant each frame (NoCutTo 14, the target-space point re-evaluated)");

    // ---- 10. SetPerceivedDistanceModificationFactor (h:268) -----------------------------------------
    lpCam->mLooker.mbForceZoomTargetUpdate = false;
    lpCam->SetPerceivedDistanceModificationFactor(0.5f);
    Check(lpCam->mfPerceivedDistanceModificationFactor == 0.5f && lpCam->mLooker.mbForceZoomTargetUpdate,
          "SetPerceivedDistanceModificationFactor: a new value is stored and raises mLooker.mbForceZoomTargetUpdate (+0xCF)");
    lpCam->mLooker.mbForceZoomTargetUpdate = false;
    lpCam->SetPerceivedDistanceModificationFactor(0.5f);
    Check(!lpCam->mLooker.mbForceZoomTargetUpdate, "SetPerceivedDistanceModificationFactor: the same value does nothing (fcmpu)");
    Frame(lpCam);
    Check(gfLookerDesiredPerceivedDistance == 2.5f,
          "Update: the looker's perceived distance is the block's (5.0) times the factor (0.5)");

    // ---- 11. SetupTweaker (slot 6) -------------------------------------------------------------------
    gTweaks.clear();
    alignas(16) static u8 laTweaker[sizeof(CU::Tweaker) + 16];
    lpBehaviour = lpCam;
    lpBehaviour->SetupTweaker(*reinterpret_cast<CU::Tweaker*>(laTweaker));
    Check(giTweakerConstructs == 1 && gTweaks.size() == 3, "SetupTweaker: Tweaker::Construct, then three mappings");
    Check(gTweaks.size() == 3 && gTweaks[0].mName == "Rig X" && gTweaks[0].mpfVariable == &lJumpLeft.mfTargetSpaceX &&
          gTweaks[0].mfScale == -0.05f && gTweaks[0].miAxis == CU::Tweaker::E_AXIS_LEFT_STICK_X &&
          gTweaks[0].miMap == CU::Tweaker::E_MAP_NORMAL,
          "SetupTweaker: \"Rig X\" -> mfTargetSpaceX, -0.05 (flt_82004E3C), [NORMAL][LEFT_STICK_X] (+0x00)");
    Check(gTweaks.size() == 3 && gTweaks[1].mName == "Rig Y" && gTweaks[1].mpfVariable == &lJumpLeft.mfTargetSpaceY &&
          gTweaks[1].mfScale == 0.05f && gTweaks[1].miAxis == CU::Tweaker::E_AXIS_LOWER_TRIGGERS,
          "SetupTweaker: \"Rig Y\" -> mfTargetSpaceY, 0.05 (flt_820047C8), [NORMAL][LOWER_TRIGGERS] (+0x78)");
    Check(gTweaks.size() == 3 && gTweaks[2].mName == "Rig Z" && gTweaks[2].mpfVariable == &lJumpLeft.mfTargetSpaceZ &&
          gTweaks[2].mfScale == 0.05f && gTweaks[2].miAxis == CU::Tweaker::E_AXIS_LEFT_STICK_Y,
          "SetupTweaker: \"Rig Z\" -> mfTargetSpaceZ, 0.05, [NORMAL][LEFT_STICK_Y] (+0x14)");

    Check(gAsserts == 0, "no assert fired so far");

    // ---- 12. the asserts the console has --------------------------------------------------------------
    lpCam->SetTarget(static_cast<EActiveRaceCarIndex>(-1));
    Check(gAsserts == 0, "SetTarget(-1): the h:222 index assert is a SIGNED compare (cmpwi) -- -1 passes");
    lpCam->SetTarget(static_cast<EActiveRaceCarIndex>(8));
    Check(gAsserts == 1 && gLastAssert == "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars",
          "SetTarget(8): VehicleRef::SetToRaceCar's BrnVehicleRef.h:222 assert (0x821F29F0 cmpwi r4, 8)");
    lpCam->mbSetup = false;
    lpBehaviour->Prepare(gPrepareInfo);
    Check(gAsserts == 2 && gLastAssert == "mbSetup", "Prepare: assert mbSetup (cpp:256)");
    static BehaviourBystanderCam::Parameters lWrongType;
    lWrongType = lFar;
    lWrongType.mType = 15;
    lpCam->SetParameters(&lWrongType);
    Check(gAsserts == 3 && gLastAssert == "lpParameters->GetType() == eBehaviourBystanderCam",
          "SetParameters: the h:215 type assert");

    std::printf("FxDirectorBystanderCam: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
