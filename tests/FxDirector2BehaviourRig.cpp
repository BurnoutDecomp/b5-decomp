// FX-DIRECTOR2 (crash parity 2026-09-25): Camera::BehaviourRig, re-derived from the X360.
//
// The runner (run_fxdirector2_behaviour_rig.py) compiles the revision's PRODUCTION BehaviourRig.cpp with
// Behaviour.cpp, the rig's CameraRig::Construct partfile and presets, and the leaves it drives for real --
// Spring1D, OrientationLag, PositionLag, DepthOfField, VehicleRef, the VehicleInfo / RaceCarState copies. The
// leaves that are tested elsewhere (the collision policy, the looker, the camera's own accessors, CreateLookAt, the
// tweaker, the debug renderer) are recording stand-ins. The behaviour is placement-new'd into pool garbage and
// driven through a Behaviour* the way the behaviour manager drives a pooled one.
//
// Against the ARTIST bodies:
//   Parameters::Construct @0x821F9680  every seed, and what it leaves alone (muVersion, the looker block, the
//                                       orientation lag's three axis springs, the position lag's version)
//   Construct @0x82242488              every member it writes, over pool garbage; mfLastMPH left alone
//   SetParameters @0x821F3B10          the tag tripwire, the pointer, the DEBUG NAME, not prepared
//   Prepare @0x821F9798                the spring from the block, the lag's parameters
//   Update @0x822427C0                 the first frame builds the rig once; camera = rig x car; the FOV and the DOF
//                                       band; the acceleration spring; detached; the look-at snap, then the looker
//                                       (its camera write discarded: 0x82242F44 reloads mLastRigTransform); the
//                                       policy's target; the NoCutTo 16 gate; the tweaker's first-frame FOV clamp
//                                       (AFTER the rig is built) and its DOF clamps and planes; mfLastMPH
//   SetupTweaker @0x821F9870 / GetName @0x821F99F0 / GetCollisionPolicy / AttachToRaceCar / SetDetached
// The camera transform of the default block on a car at the origin is checked against the CONSOLE's own
// CameraRig::Construct output (FxDirector2CameraRigGolden.h, the emulator over the raw words), bit for bit; every
// later camera, the pushed frame and the looked-at car's local frame are checked bit for bit against the console's
// rounding (Utils/BrnConsoleVpu.h, itself pinned to the Update's words by run_fxdirector2_console_vpu.py) -- the
// vendor SDK forms miss them.
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "FxDirector2CameraRigGolden.h"

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText ? lpcText : ""; return 0; }
    void* EndAssert() { return nullptr; }
}
}

using namespace BrnDirector;
using namespace BrnDirector::Camera;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;
namespace ConsoleVpu = BrnDirector::Camera::Utils::ConsoleVpu;

// ---- recording stand-ins ------------------------------------------------------------------------------------
static int giPolicyConstructs = 0, giSetTargets = 0, giLookAts = 0, giLookerUpdates = 0, giValidates = 0;
static int giTweakerConstructs = 0, giNoCutToFlag = -1, giValidFlagSets = 0, giDrawAxes = 0, giDrawQuads = 0;
static Matrix44Affine gLastTargetTransform, gLookerCameraIn, gLookerTarget, gLookAtResult, gLastAxis;
static AABBox gLastTargetAABB, gLookerBox;
static u32 guLastTargetEntity = 0;
static Vector3 gLastEye, gLastLookTarget, gLookerVelocity;
static f32 gfLookerTimestep = -1.0f;
static const void* gpLookerParams = nullptr;
static u32 gauQuadColours[2] = { 0u, 0u };
struct TweakRecord { std::string mName; f32* mpfVariable; f32 mfScale; int miAxis; int miMap; };
static std::vector<TweakRecord> gTweaks;

namespace BrnDirector
{
namespace Camera
{
    // The console's inlined policy Construct leaves the visibility test's first byte at 1; this stand-in leaves it at
    // 0, so only the rig's own SetTestLookingAt(true) (`stb 1, 0x1C0` after the policy block) can raise it.
    void VisibilityCollisionPolicy::Construct()
    {
        ++giPolicyConstructs;
        mVisibilityTest.mbTestLookingAt = false;
        mVisibilityTest.mbOccluded      = false;
        mVisibilityTest.mbIsOnScreen    = true;
    }
    void VisibilityCollisionPolicy::SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                                              CgsSceneManager::EntityId lTargetEntityId)
    {
        ++giSetTargets;
        gLastTargetTransform = lTargetTransform;
        gLastTargetAABB      = lTargetAABB;
        guLastTargetEntity   = static_cast<u32>(lTargetEntityId);
    }
    // The policy's two scene-query virtuals (bodies in BrnVisibilityCollisionPolicy.cpp, not compiled here): defined
    // only so the policy's vtable links; the behaviour under test never calls them.
    void VisibilityCollisionPolicy::GenerateSceneQueries(const CollisionPolicySharedInfo&, Camera&) {}
    void VisibilityCollisionPolicy::ProcessSceneQueryResults(const CollisionPolicySharedInfo&, Camera&) {}
    void ValidityAccount::SetFlag(s32) {}
    void ValidityAccount::SetNoCutFromFlag(s32) {}
    void ValidityAccount::SetNoCutToFlag(s32 leFlag) { giNoCutToFlag = leFlag; }
    void CameraState::ClearFlag(u32) {}
    void CameraState::SetFlag(u32 luIndex, bool lbValue)
    {
        if (luIndex == static_cast<u32>(E_FLAG_VALID) && lbValue)
            ++giValidFlagSets;
    }
    void Camera::SetFOV(f32 lfFOV) { CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f"); mfFOV = lfFOV; }
    void Camera::SetTransform(const Matrix44Affine& lrTransform) { mTransform = lrTransform; }
    Matrix44Affine* Camera::ValidateTransformWithDebugInfo() { ++giValidates; return &mTransform; }
    DepthOfField& Camera::GetDepthOfField() { return mDepthOfField; }
namespace Utils
{
    // A deterministic stand-in look-at: a fixed orientation, the eye in the translation row.
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        ++giLookAts;
        gLastEye = lEyePosition;
        gLastLookTarget = lTargetPosition;
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
        lResult.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lResult.zAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lResult.wAxis = lEyePosition;
        gLookAtResult = lResult;
        return lResult;
    }
    // Records its arguments, then moves the camera it was handed -- which the rig must discard.
    void Looker::Update(VecFloat lvTimeStep, Random, const Parameters& lrParams, Camera& lrCamera,
                        Matrix44Affine lTarget, Vector3 lTargetVel, AABBox lAABB)
    {
        ++giLookerUpdates;
        gfLookerTimestep = static_cast<f32>(lvTimeStep);
        gpLookerParams   = &lrParams;
        gLookerCameraIn  = lrCamera.mTransform;
        gLookerTarget    = lTarget;
        gLookerVelocity  = lTargetVel;
        gLookerBox       = lAABB;
        lrCamera.mTransform.wAxis.y += 100.0f;
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

namespace CgsDev
{
    alignas(16) static u8 gaRender[sizeof(DebugRender)];
    DebugInterface::DebugInterface() : mpDebugManager(nullptr), mbIsAutomaticClass(false) {}
    DebugInterface::~DebugInterface() {}
    DebugRender& DebugInterface::GetRender() { return *reinterpret_cast<DebugRender*>(gaRender); }
    void DebugRender::DrawAxis(Matrix44Affine lTransform) { ++giDrawAxes; gLastAxis = lTransform; }
    void DebugRender::DrawSolidQuad(Vector3, Vector3, Vector3, Vector3, RGBA lColour)
    {
        gauQuadColours[giDrawQuads & 1] = static_cast<u32>(lColour);
        ++giDrawQuads;
    }
}

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
static u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }
static f32 Float(u32 luBits) { f32 lfValue; std::memcpy(&lfValue, &luBits, 4); return lfValue; }
static bool Near(f32 a, f32 b, f32 tol) { return std::fabs(a - b) <= tol; }
static bool NearV(const Vector3& a, const Vector3& b, f32 tol)
{
    return Near(a.x, b.x, tol) && Near(a.y, b.y, tol) && Near(a.z, b.z, tol);
}
static bool NearM(const Matrix44Affine& a, const Matrix44Affine& b, f32 tol)
{
    return NearV(a.xAxis, b.xAxis, tol) && NearV(a.yAxis, b.yAxis, tol) && NearV(a.zAxis, b.zAxis, tol)
        && NearV(a.wAxis, b.wAxis, tol);
}
static Vector3 V3(f32 x, f32 y, f32 z) { return Vector3{ x, y, z, 0.0f }; }
static bool AllBytes(const void* lpData, size_t luSize, u8 lu8Value)
{
    const u8* lpBytes = static_cast<const u8*>(lpData);
    for (size_t i = 0; i < luSize; ++i)
        if (lpBytes[i] != lu8Value)
            return false;
    return true;
}

// A car yawed about Y and placed at lrPosition.
static Matrix44Affine CarTransform(f32 lfYawDegrees, const Vector3& lrPosition)
{
    const f32 s = std::sin(lfYawDegrees * 0.017453292f), c = std::cos(lfYawDegrees * 0.017453292f);
    Matrix44Affine m;
    m.xAxis = V3(c, 0.0f, -s);
    m.yAxis = V3(0.0f, 1.0f, 0.0f);
    m.zAxis = V3(s, 0.0f, c);
    m.wAxis = Vector3{ lrPosition.x, lrPosition.y, lrPosition.z, 1.0f };
    return m;
}

alignas(16) static u8 gaSlot[sizeof(BehaviourRig) + 16];
alignas(16) static u8 gaTweaker[sizeof(Utils::Tweaker) + 16];
alignas(16) static VehicleInfo gaCars[8];
static AllVehicleData gWorld;
static BehaviourSharedInfo gInfo;
static BrnDirector::Camera::Camera gCamera;

static void SetCar(int liIndex, const Matrix44Affine& lrTransform, f32 lfSpeedMPH, u32 luEntity,
                   const Vector3& lrMin, const Vector3& lrMax, const Vector3& lrVelocity)
{
    VehicleInfo& lrCar = gaCars[liIndex];
    lrCar.mRaceCarState.mTransform        = lrTransform;
    lrCar.mRaceCarState.mfSpeedMPH        = lfSpeedMPH;
    lrCar.mRaceCarState.mEntityId.muValue = luEntity;
    lrCar.mRaceCarState.mLinearVelocity   = lrVelocity;
    lrCar.mAABB.mMin = lrMin;
    lrCar.mAABB.mMax = lrMax;
}

int main()
{
    // The console's own CameraRig::Construct for the default block on box 0, not mirrored.
    const RigCase* lpGolden = nullptr;
    for (const RigCase& lrCase : kaRigCases)
        if (std::strcmp(lrCase.mpcLabel, "FrontQuarterClose box0") == 0)
            lpGolden = &lrCase;
    if (lpGolden == nullptr)
    {
        std::printf("no golden row\n");
        return 2;
    }
    Matrix44Affine lGoldenRig;
    {
        const u32* r = lpGolden->mauRows;
        lGoldenRig.xAxis = Vector3{ Float(r[0]), Float(r[1]), Float(r[2]), Float(r[3]) };
        lGoldenRig.yAxis = Vector3{ Float(r[4]), Float(r[5]), Float(r[6]), Float(r[7]) };
        lGoldenRig.zAxis = Vector3{ Float(r[8]), Float(r[9]), Float(r[10]), Float(r[11]) };
        lGoldenRig.wAxis = Vector3{ Float(r[12]), Float(r[13]), Float(r[14]), Float(r[15]) };
    }
    const Vector3 lBoxMin = V3(Float(lpGolden->mauMin[0]), Float(lpGolden->mauMin[1]), Float(lpGolden->mauMin[2]));
    const Vector3 lBoxMax = V3(Float(lpGolden->mauMax[0]), Float(lpGolden->mauMax[1]), Float(lpGolden->mauMax[2]));
    unsigned luExpectedAsserts = 0;

    // ================= Parameters::Construct @0x821F9680 =================
    static BehaviourRig::Parameters lParams;
    std::memset(static_cast<void*>(&lParams), 0x5A, sizeof(lParams));
    const u32 luSentinel = 0x5A5A5A5Au;
    lParams.Construct();
    Check(lParams.GetType() == 2u && lParams.GetDebugName() == nullptr,
          "PC1 the tag is eBehaviourRig (`li r9, 2 ; stw`) and the debug name is cleared");
    Check(std::memcmp(&lParams.mRigParams, &Utils::CameraRig::ParamsFrontQuarterClose, sizeof(lParams.mRigParams)) == 0,
          "PC2 mRigParams is ParamsFrontQuarterClose (the 8-doubleword copy from 0x82CDA890)");
    Check(Bits(lParams.mShakeParams.mfXYShakeMagnitudeDegs) == 0x3F19999Au
              && Bits(lParams.mShakeParams.mfZShakeMagnitudeDegs) == 0u
              && Bits(lParams.mShakeParams.mfXYWobbleMagnitudeDegs) == 0x3F933333u
              && Bits(lParams.mShakeParams.mfWobbleCenteringFactor) == 0x3DE147AEu,
          "PC3 the shake block is 0.6 / 0.0 / 1.15 / 0.11 (flt_82004D00 / 82001CC0 / 820047BC / 820047C0)");
    Check(Bits(lParams.mOrientationLagParams.mfSlerpSpring) == 0x3D4CCCCDu && lParams.mOrientationLagParams.mbUseSlerpSpring
              && lParams.mOrientationLagParams.muVersion.muVersion == luSentinel
              && Bits(lParams.mOrientationLagParams.mfPitchSpring) == luSentinel
              && Bits(lParams.mOrientationLagParams.mfYawSpring) == luSentinel
              && Bits(lParams.mOrientationLagParams.mfRollSpring) == luSentinel,
          "PC4 the orientation lag gets its slerp spring (0.05 at +0xD4) and switch (+0xD8) only");
    Check(lParams.mPositionLagParams.mfXResponse == 1.0f && lParams.mPositionLagParams.mfYResponse == 1.0f
              && lParams.mPositionLagParams.mfZResponse == 1.0f && lParams.mPositionLagParams.mfSmoothing == 0.5f
              && lParams.mPositionLagParams.muVersion == luSentinel,
          "PC5 the position lag gets 1 / 1 / 1 and smoothing 0.5 (+0xE0..+0xEC); its version is untouched");
    Check(lParams.mfSpringAccelFactor == 10.0f && lParams.mfSpringMass == 1.0f && lParams.mfSpringStiffness == 40.0f
              && lParams.mfSpringDampening == 5.0f && lParams.mfSpringMinStretch == -1.0f
              && lParams.mfSpringMaxStretch == 1.5f,
          "PC6 the spring: factor 10, mass 1, stiffness 40, dampening 5, stretch -1 .. 1.5 (+0xF0..+0x104)");
    Check(lParams.mfDOFNear == 0.0f && lParams.mfDOFFar == 2000.0f && lParams.mfDOFBlurDepth == 10.0f
              && lParams.mfDOFIntensity == 0.0f,
          "PC7 the DOF band: near 0, far 2000 (flt_82004D08), blur depth 10, intensity 0 (+0x108..+0x114)");
    Check(!lParams.mbUseAccelSpring && lParams.mbUseShake && !lParams.mbReverse && !lParams.mbUseOrientationLag
              && !lParams.mbUsePositionLag,
          "PC8 the switches 0 / 1 / 0 / 0 / 0: spring off, SHAKE ON, not mirrored, no lags (+0x118..+0x11C)");
    Check(lParams.muVersion.muVersion == luSentinel && AllBytes(&lParams.mLookerParams, sizeof(lParams.mLookerParams), 0x5A),
          "PC9 muVersion (+0x08) and the whole looker block (+0x60..+0xC3) are left alone");

    // ================= Construct @0x82242488 over pool garbage, through the base =================
    std::memset(gaSlot, 0xCD, sizeof(gaSlot));
    BehaviourRig* lpRig = new (gaSlot) BehaviourRig;
    // What a recycled slot carries: stale state in every member Construct owns.
    lpRig->meTimestepType = Timestep::E_GAME;
    lpRig->mbIsPrepared = lpRig->mbHasFailed = lpRig->mbTweakerAttached = true;
    lpRig->mbCanSwitchToMeNow = lpRig->mbCanSwitchFromMeNow = true;
    lpRig->mpcDebugParametersName = "stale";
    lpRig->mpParameters = &lParams;
    lpRig->mbDetached = lpRig->mbLookingLast = lpRig->mbLooking = lpRig->mbSnap = true;
    lpRig->mLookingAtRef.mbSet = true;
    lpRig->mAttachedToRef.meType = BrnDirector::VehicleRef::E_TRAFFIC_VEHICLE;
    lpRig->mAttachedToRef.miRaceCarIndex = 5;
    lpRig->mAttachedToRef.muRef = 9u;
    lpRig->mAttachedToRef.mbSet = false;
    lpRig->mfLastMPH = 123.0f;
    Behaviour* lpBase = lpRig;
    lpBase->Construct();
    {
        CgsNumeric::Random lReference;
        lReference.Construct();
        bool lbRandom = lpRig->mRandom.muSeed == lReference.muSeed
                     && lpRig->mRandom.muOldestBufferIndex == lReference.muOldestBufferIndex;
        for (int i = 0; i < 8; ++i)
            lbRandom = lbRandom && lpRig->mRandom.mauIntegerBuffer[i] == lReference.mauIntegerBuffer[i];
        Check(giPolicyConstructs == 1 && lpRig->mCollisionPolicy.mVisibilityTest.mbTestLookingAt,
              "C1 the policy is Constructed, then SetTestLookingAt(true) (`stb 1, 0x1C0`)");
        Check(lpRig->meTimestepType == Timestep::E_WORLD && !lpRig->mbIsPrepared && !lpRig->mbHasFailed
                  && !lpRig->mbTweakerAttached && !lpRig->mbCanSwitchToMeNow && !lpRig->mbCanSwitchFromMeNow
                  && lpRig->mpcDebugParametersName == nullptr,
              "C2 the base's six stores (Behaviour::Construct, +0x04..+0x10)");
        Check(lpRig->mShake.mfCurrentWobbleX == 0.0f && lpRig->mShake.mfCurrentWobbleY == 0.0f
                  && lpRig->mShake.mfCurrentWobbleXVel == 0.0f && lpRig->mShake.mfCurrentWobbleYVel == 0.0f
                  && lpRig->mLooker.mfSlerpFactor == 0.0f && lpRig->mLooker.mfAssessmentTime == 0.2f
                  && lpRig->mLooker.mbFirstFrame && lpRig->mLooker.mbAssessingFOV && lpRig->mLooker.mbConstructed
                  && !lpRig->mLooker.mbForceZoomTargetUpdate && lbRandom,
              "C3 the shake cleared (+0x2B0..+0x2BC), the looker seeded (+0x3E4 0.0, +0x3F4 0.2, latches 1/1/1/0), "
              "the Random primed from the default seed");
        Check(lpRig->mAccelSpring.mfMass == 1.0f && lpRig->mAccelSpring.mfDesiredLength == 0.0f
                  && lpRig->mAccelSpring.mfCurrentLength == 0.0f
                  && lpRig->mPositionLag.mbFirstFrame && lpRig->mPositionLag.mbConstructed
                  && lpRig->mOrientationLag.mpParameters == nullptr && lpRig->mOrientationLag.mbFirstFrame,
              "C4 the spring Constructed (mass 1), the position lag's latches (+0x330/+0x331 = 1), the orientation "
              "lag unparameterised and first-frame (+0x300 = 0, +0x304 = 1)");
        Check(!lpRig->mLookingAtRef.mbSet && lpRig->mAttachedToRef.mbSet
                  && lpRig->mAttachedToRef.meType == BrnDirector::VehicleRef::E_PLAYER_CAR
                  && lpRig->mAttachedToRef.miRaceCarIndex == -1 && lpRig->mAttachedToRef.muRef == 0u,
              "C5 nothing looked at (+0x45C = 0); attached to the PLAYER (+0x44C = 1, +0x440 = 0, +0x444 = -1, +0x448 = 0)");
        Check(lpRig->mpParameters == nullptr && !lpRig->mbDetached && !lpRig->mbLookingLast && !lpRig->mbLooking
                  && !lpRig->mbSnap && lpRig->mfLastMPH == 123.0f,
              "C6 no parameters; the four latches down; mfLastMPH (+0x464) NOT written");
    }

    // ================= SetParameters @0x821F3B10 / Prepare @0x821F9798 =================
    lParams.SetDebugName("RigTestBlock");
    lpRig->mbIsPrepared = true;
    lpRig->SetParameters(&lParams);
    Check(lpRig->mpParameters == &lParams && lpRig->GetDebugParametersName() != nullptr
              && std::strcmp(lpRig->GetDebugParametersName(), "RigTestBlock") == 0 && !lpRig->mbIsPrepared
              && gAsserts == luExpectedAsserts,
          "S1 SetParameters stores the block (+0x460), COPIES ITS DEBUG NAME to the base (+0x10), mbIsPrepared = 0");
    {
        static BehaviourRig::Parameters lWrongType;
        lWrongType = lParams;
        lWrongType.mType = 5u;
        lpRig->SetParameters(&lWrongType);
        ++luExpectedAsserts;
        Check(gAsserts == luExpectedAsserts && gLastAssert == "lpParameters->GetType() == eBehaviourRig",
              "S2 a block of another type trips \"lpParameters->GetType() == eBehaviourRig\" (BehaviourRig.h:255)");
        lpRig->SetParameters(&lParams);
    }
    BehaviourSharedPrepareReleaseInfo lPrepareInfo = {};
    lpRig->mbIsPrepared = true;
    lpRig->mAccelSpring.mfVelocity = 3.0f;
    const bool lbPrepared = lpBase->Prepare(lPrepareInfo);
    Check(lbPrepared && !lpRig->mbIsPrepared && gAsserts == luExpectedAsserts
              && lpRig->mAccelSpring.mfDesiredLength == 0.0f && lpRig->mAccelSpring.mfCurrentLength == 0.0f
              && lpRig->mAccelSpring.mfMass == 1.0f && lpRig->mAccelSpring.mfStiffness == 40.0f
              && lpRig->mAccelSpring.mfDampeningConstant == 5.0f && lpRig->mAccelSpring.mfMinStretch == -1.0f
              && lpRig->mAccelSpring.mfMaxStretch == 1.5f && lpRig->mAccelSpring.mfVelocity == 0.0f
              && lpRig->mOrientationLag.mpParameters == &lParams.mOrientationLagParams,
          "P1 Prepare: not prepared; the spring from the block (0, 0, mass, stiffness, dampening, min, max); the lag "
          "pointed at the block; true; no stray assert");

    // ================= Update @0x822427C0 -- the first frame, car at the origin =================
    Matrix44Affine lIdentity;
    lIdentity.SetIdentity();
    SetCar(0, lIdentity, 30.0f, 0x01000100u, lBoxMin, lBoxMax, V3(0.0f, 0.0f, 13.4f));
    gWorld.mpRaceCars = gaCars;
    gWorld.mePlayerRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    gWorld.mUsedRaceCars.UnSetAll();
    gWorld.mUsedRaceCars.SetBit(0u);
    gWorld.mUsedRaceCars.SetBit(1u);
    gInfo.mpAllVehicleData = &gWorld;
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD] = 1.0f / 30.0f;
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD_NO_SLOMO] = 1.0f / 30.0f;
    gInfo.mTimestep.mafTimestep[Timestep::E_GAME] = 1.0f / 30.0f;
    std::memset(static_cast<void*>(&gCamera), 0, sizeof(gCamera));
    const bool lbUpdated = lpBase->Update(gCamera, gInfo);
    Check(lbUpdated && gAsserts == luExpectedAsserts && giValidFlagSets == 1 && giValidates == 1,
          "U1 returns true, raises the camera's E_FLAG_VALID, validates the transform once, no stray assert");
    Check(NearM(gCamera.mTransform, lGoldenRig, 0.0f),
          "U2 a car at the origin: the camera IS the console's CameraRig::Construct output for this preset and box, "
          "bit for bit");
    Check(Bits(gCamera.mfFOV) == lpGolden->muOutFov,
          "U3 the camera's FOV is the rig's (Camera::SetFOV(mRig.GetFOV()), 0x82243020)");
    Check(gCamera.mDepthOfField.mfFocusStartDistanceMeters == 0.0f
              && gCamera.mDepthOfField.mfFocusEndDistanceMeters == 2000.0f
              && gCamera.mDepthOfField.mfPerfectFocusStartDistanceMeters == (0.0f + 10.0f) + 0.01f
              && gCamera.mDepthOfField.mfPerfectFocusEndDistanceMeters == (2000.0f - 10.0f) - 0.01f
              && gCamera.mDepthOfField.mfBlurriness == 0.0f,
          "U4 the DOF band: SetParams(near, far, blur depth, intensity) @0x821F1C20 -- 0 / 10.01 / 1989.99 / 2000 / 0");
    Check(lpRig->mbIsPrepared && lpRig->mfLastMPH == 30.0f && NearM(lpRig->mLastAttachedToTransform, lIdentity, 0.0f)
              && NearM(lpRig->mLastRigTransform, lpRig->mRig.GetRigTransform(), 0.0f) && giDrawAxes == 0,
          "U5 the first frame builds the rig, keeps the car's frame (+0x340), its speed (+0x464) and the rig (+0x380), "
          "and (no tweaker) marks the rig prepared");
    Check(giSetTargets == 1 && NearM(gLastTargetTransform, lIdentity, 0.0f) && guLastTargetEntity == 0x01000100u
              && NearV(gLastTargetAABB.mMin, lBoxMin, 0.0f) && NearV(gLastTargetAABB.mMax, lBoxMax, 0.0f),
          "U6 nothing looked at: the policy targets the attached car (its transform, box and entity id)");

    // ---- the second frame: the car turned and moved, and its box changed; a prepared rig is NOT rebuilt ----
    const Matrix44Affine lCar = CarTransform(30.0f, V3(100.0f, 5.0f, -40.0f));
    const Vector3 lBigMin = V3(-5.0f, -5.0f, -5.0f), lBigMax = V3(5.0f, 5.0f, 5.0f);
    SetCar(0, lCar, 45.0f, 0x01000100u, lBigMin, lBigMax, V3(0.0f, 0.0f, 20.0f));
    lpBase->Update(gCamera, gInfo);
    Check(NearM(gCamera.mTransform, ConsoleVpu::Mult(lGoldenRig, lCar), 0.0f)
              && !NearM(rw::math::vpu::Mult(lGoldenRig, lCar), ConsoleVpu::Mult(lGoldenRig, lCar), 0.0f),
          "U7 camera = rig x car, the console's fused cascade (0x82242F90), which the vendor Mult misses here; a "
          "changed box does not rebuild a prepared rig");
    Check(lpRig->mfLastMPH == 45.0f && NearM(lpRig->mLastAttachedToTransform, lCar, 0.0f),
          "U8 mfLastMPH follows the car's speed; the kept frame follows the car");

    // ---- the acceleration spring: desired = -(factor * dMPH / dt), the car's frame pushed along its At ----
    lParams.mbUseAccelSpring = true;
    SetCar(0, lCar, 48.0f, 0x01000100u, lBigMin, lBigMax, V3(0.0f, 0.0f, 20.0f));
    lpBase->Update(gCamera, gInfo);
    {
        const f32 lfAccel = (48.0f - 45.0f) / (1.0f / 30.0f);
        Matrix44Affine lPushed = lCar;
        lPushed.wAxis = ConsoleVpu::MultiplyAdd(lCar.zAxis, lpRig->mAccelSpring.GetLength(), lCar.wAxis);
        Check(lpRig->mAccelSpring.mfDesiredLength == -(10.0f * lfAccel) && lpRig->mAccelSpring.GetLength() == 1.5f,
              "A1 the spring's desired length is -(accel factor * (speed - last) / dt) (0x82242B80..0x82242B94), and "
              "Spring1D::Update ran (the far push clamps at the 1.5 max stretch)");
        Check(NearM(gCamera.mTransform, ConsoleVpu::Mult(lGoldenRig, lPushed), 0.0f)
                  && NearM(lpRig->mLastAttachedToTransform, lPushed, 0.0f),
              "A2 the car's frame is pushed along its At by the spring's length (one vmaddfp128, 0x82242C34), and "
              "that frame is kept");
    }
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD] = std::numeric_limits<f32>::quiet_NaN();
    lpBase->Update(gCamera, gInfo);
    Check(Bits(lpRig->mAccelSpring.mfDesiredLength) == 0x80000000u,
          "A3 a NaN timestep is ZERO to IsZero (bgt / bge): no acceleration, desired length -(10 * 0) == -0.0");
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD] = 1.0f / 30.0f;
    lParams.mbUseAccelSpring = false;

    // ---- detached: the camera keeps the last kept frame while the car drives away ----
    lpBase->Update(gCamera, gInfo);
    const Matrix44Affine lKept = lpRig->mLastAttachedToTransform;
    lpRig->SetDetached(true);
    SetCar(0, CarTransform(90.0f, V3(300.0f, 5.0f, 60.0f)), 60.0f, 0x01000100u, lBigMin, lBigMax,
           V3(0.0f, 0.0f, 20.0f));
    lpBase->Update(gCamera, gInfo);
    Check(lpRig->mbDetached && NearM(gCamera.mTransform, ConsoleVpu::Mult(lGoldenRig, lKept), 0.0f)
              && NearM(lpRig->mLastAttachedToTransform, lKept, 0.0f),
          "D1 SetDetached(true) (+0x468): the rig films from the kept frame and does not update it");
    lpRig->AttachToRaceCar(E_ACTIVE_RACE_CAR_INDEX_1);
    Check(!lpRig->mbDetached && lpRig->mAttachedToRef.meType == BrnDirector::VehicleRef::E_RACE_CAR
              && lpRig->mAttachedToRef.miRaceCarIndex == 1 && lpRig->mAttachedToRef.mbSet,
          "D2 AttachToRaceCar(1): the attached ref is race car 1 (SetToRaceCar) and the rig re-attaches");
    lpRig->AttachToRaceCar(E_ACTIVE_RACE_CAR_INDEX_0);

    // ================= the look-at: the SNAP frame, then the looker =================
    const Matrix44Affine lTarget = CarTransform(-60.0f, V3(80.0f, 4.0f, -55.0f));
    SetCar(1, lTarget, 70.0f, 0x01000200u, V3(-1.0f, 0.0f, -2.0f), V3(1.0f, 1.5f, 2.0f), V3(3.0f, 0.0f, -7.0f));
    SetCar(0, lCar, 45.0f, 0x01000100u, lBigMin, lBigMax, V3(0.0f, 0.0f, 20.0f));
    lpRig->StartLookingAtRaceCar(E_ACTIVE_RACE_CAR_INDEX_1, true);
    Check(lpRig->mLookingAtRef.mbSet && lpRig->mLookingAtRef.meType == BrnDirector::VehicleRef::E_RACE_CAR
              && lpRig->mLookingAtRef.miRaceCarIndex == 1 && lpRig->mLookingAtRef.muRef == 0u && lpRig->mbLooking
              && lpRig->mbSnap,
          "L1 StartLookingAtRaceCar(1, true): the looked-at ref, mbLooking, mbSnap (the look-back's six stores)");
    const Matrix44Affine lRigBefore = lpRig->mLastRigTransform;
    const Matrix44Affine lInverse = ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lCar);
    const Matrix44Affine lLocal = ConsoleVpu::Mult(lTarget, lInverse);
    const Vector3 lVelocityLocal = ConsoleVpu::TransformVector(lInverse, V3(3.0f, 0.0f, -7.0f));
    lpBase->Update(gCamera, gInfo);
    Check(giLookAts == 1 && giLookerUpdates == 0 && NearV(gLastEye, lRigBefore.wAxis, 0.0f)
              && NearV(gLastLookTarget, lLocal.wAxis, 0.0f),
          "L2 the first looking frame SNAPS: CreateLookAt(the held rig's position, the looked-at car in the car's "
          "frame) (0x82242E88); the looker does not run");
    Check(NearM(lpRig->mLastRigTransform, gLookAtResult, 0.0f)
              && NearM(gCamera.mTransform, ConsoleVpu::Mult(gLookAtResult, lCar), 0.0f) && lpRig->mbLookingLast,
          "L3 the snapped look-at becomes the held rig (+0x380) and the camera is it x the car; mbLookingLast latched");
    Check(guLastTargetEntity == 0x01000200u && NearM(gLastTargetTransform, lTarget, 0.0f)
              && NearV(gLastTargetAABB.mMax, V3(1.0f, 1.5f, 2.0f), 0.0f),
          "L4 while looking, the policy targets the LOOKED-AT car");
    lpBase->Update(gCamera, gInfo);
    Check(giLookerUpdates == 1 && giLookAts == 1 && NearM(gLookerCameraIn, gLookAtResult, 0.0f)
              && gfLookerTimestep == 1.0f / 30.0f && gpLookerParams == &lParams.mLookerParams
              && NearM(gLookerTarget, lLocal, 0.0f) && NearV(gLookerVelocity, lVelocityLocal, 0.0f)
              && NearV(gLookerBox.mMin, V3(0.0f, 0.0f, 0.0f), 0.0f) && NearV(gLookerBox.mMax, V3(0.0f, 0.0f, 0.0f), 0.0f),
          "L5 later looking frames run the Looker (0x82242F40): the camera handed the held rig, the timestep splat, "
          "the block's looker parameters (+0x60), the target and its velocity in the car's frame, a zero box");
    Check(NearM(gCamera.mTransform, ConsoleVpu::Mult(gLookAtResult, lCar), 0.0f)
              && NearM(lpRig->mLastRigTransform, gLookAtResult, 0.0f),
          "L6 ... and the camera is the HELD rig x the car: what the looker did to the camera is discarded "
          "(0x82242F44 reloads +0x380)");

    // ================= the NoCutTo gate (0x822436A8) =================
    lpRig->mCollisionPolicy.mVisibilityTest.mbOccluded = true;
    lpRig->mbCanSwitchToMeNow = true;
    giNoCutToFlag = -1;
    lpBase->Update(gCamera, gInfo);
    Check(giNoCutToFlag == 16 && !lpRig->mbCanSwitchToMeNow,
          "N1 the policy's visibility interrupted: NoCutTo 16 (`oris 1` on the account at camera +0x138) and "
          "mbCanSwitchToMeNow = 0");
    giNoCutToFlag = -1;
    lpRig->mbHasFailed = true;
    lpRig->mbCanSwitchToMeNow = true;
    const int liValidBefore = giValidFlagSets;
    lpBase->Update(gCamera, gInfo);
    Check(giNoCutToFlag == -1 && lpRig->mbCanSwitchToMeNow && giValidFlagSets == liValidBefore,
          "N2 a failed rig raises neither the NoCutTo bit nor the camera's E_FLAG_VALID");
    lpRig->mbHasFailed = false;
    lpRig->mCollisionPolicy.mVisibilityTest.mbOccluded = false;

    // ================= SetupTweaker @0x821F9870 / GetName @0x821F99F0 / GetCollisionPolicy =================
    lpBase->SetupTweaker(*reinterpret_cast<Utils::Tweaker*>(gaTweaker));
    Check(giTweakerConstructs == 1 && gTweaks.size() == 4
              && gTweaks[0].mName == "DOF Blur Near" && gTweaks[0].mpfVariable == &lParams.mfDOFNear && gTweaks[0].miAxis == 1
              && gTweaks[1].mName == "DOF Blur Far" && gTweaks[1].mpfVariable == &lParams.mfDOFFar && gTweaks[1].miAxis == 3
              && gTweaks[2].mName == "DOF Blur Depth" && gTweaks[2].mpfVariable == &lParams.mfDOFBlurDepth && gTweaks[2].miAxis == 6
              && gTweaks[3].mName == "DOF Intensity" && gTweaks[3].mpfVariable == &lParams.mfDOFIntensity && gTweaks[3].miAxis == 8,
          "T1 SetupTweaker: Construct, then the four DOF mappings into map-0 slots 1 / 3 / 6 / 8 (+0x14 / +0x3C / +0x78 "
          "/ +0xA0)");
    Check(gTweaks.size() == 4 && gTweaks[0].mfScale == 0.05f && gTweaks[1].mfScale == 0.05f && gTweaks[2].mfScale == 0.05f
              && gTweaks[3].mfScale == 0.05f && gTweaks[0].miMap == 0 && gTweaks[1].miMap == 0 && gTweaks[2].miMap == 0
              && gTweaks[3].miMap == 0,
          "T2 every mapping is scale 0.05 (flt_820047C8), map 0 (E_MAP_NORMAL)");
    Check(std::strcmp(lpBase->GetName(), "BehaviourRig") == 0 && lpBase->GetCollisionPolicy() == &lpRig->mCollisionPolicy,
          "T3 GetName \"BehaviourRig\"; GetCollisionPolicy is the embedded policy (`addi r3, r3, 0x20`)");

    // ================= the tweaker's first frame (0x82242920): the rig is built, THEN the FOV clamped =================
    lpRig->SetTweakerAttached(true);
    lParams.mRigParams.mfFOV = 200.0f;
    lpRig->SetParameters(&lParams);
    const int liAxesBefore = giDrawAxes;
    lpBase->Update(gCamera, gInfo);
    {
        Matrix44Affine lTargetOffset;
        lTargetOffset.SetIdentity();
        lTargetOffset.wAxis = rw::math::vpu::Mult(lParams.mRigParams.mOffsetFromTarget, lBigMax - lBigMin);
        lTargetOffset.wAxis.w = 1.0f;
        Check(gCamera.mfFOV == 200.0f && lParams.mRigParams.mfFOV == 120.0f && !lpRig->mbIsPrepared
                  && giDrawAxes == liAxesBefore + 1,
              "W1 tweaker attached, unprepared: the rig is built with the block's FOV (200 reaches the camera), then "
              "the block's FOV is clamped to [5, 120] (flt_8200426C / flt_82004A28) and the rig stays unprepared");
        Check(NearM(gLastAxis, ConsoleVpu::Mult(lTargetOffset, lCar), 0.0f),
              "W2 ... and draws the target offset (offset x box size, no centre term) on the car (DrawAxis)");
    }
    lpBase->Update(gCamera, gInfo);
    Check(gCamera.mfFOV == 120.0f && !lpRig->mbIsPrepared && giDrawAxes == liAxesBefore + 2,
          "W3 the next frame rebuilds the rig with the clamped FOV (a tweaked rig is rebuilt every frame)");

    // ================= the tweaker's DOF clamps and planes (0x82243050) =================
    lParams.mfDOFNear = -3.0f;
    lParams.mfDOFFar = 0.1f;
    lParams.mfDOFBlurDepth = 7.0f;
    lParams.mfDOFIntensity = 1.5f;
    const int liQuadsBefore = giDrawQuads;
    lpBase->Update(gCamera, gInfo);
    Check(lParams.mfDOFNear == 0.0f && lParams.mfDOFFar == 0.0f + 0.2f
              && lParams.mfDOFBlurDepth == (0.2f - 0.0f) * 0.5f - 0.05f && lParams.mfDOFIntensity == 1.0f,
          "W4 tweaker: near >= 0, far >= near + 0.2, blur depth <= half the band - 0.05, intensity <= 1 (in place)");
    Check(giDrawQuads == liQuadsBefore + 2 && gauQuadColours[liQuadsBefore & 1] == 0x07FF0000u
              && gauQuadColours[(liQuadsBefore + 1) & 1] == 0x070000FFu,
          "W5 two DOF planes: the far one (alpha 7 << 24 | 0xFF0000, `oris`) then the near one (| 0xFF, `ori`)");
    lpRig->SetTweakerAttached(false);

    Check(gAsserts == luExpectedAsserts, "no stray assert fired");

    std::printf("FxDirector2BehaviourRig: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
