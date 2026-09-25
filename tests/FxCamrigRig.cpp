// FX-CAMRIG (run_fxcamrig_rig.py) -- the Showtime / crash-mode camera rig,
// BrnDirector::Camera::BehaviourAftertouchCrash::Update @0x82228158, driven frame by frame through
// its EXTRACTED production body (fxcamrig_rig.inl) against the real headers.
//
// The rig's out-of-TU callees are fixtures below that RECORD what the rig hands them: the four
// Utils::CreateLookAt calls (eye / target), the two CameraShake::Update calls (which shake, which
// parameter block, which random, timestep, scale), PositionLag::Update (it also shifts the frame by a
// known offset, so the test proves the rig works from the LAGGED origin), SineLerp, SetFlag and the
// asserts. CreateLookAt rebuilds the production construction (z toward the target, x = Y x z,
// y = z x x, w = eye) so the vendor SLerp sees real frames.
//
// Expectations come from the ARTIST asm (see the .cpp banner): a double-precision oracle for the rig's
// arithmetic, and exact values where the console arithmetic is exact (clamps, the debug parameter,
// the random draw, the seeding stores).
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "rw/math/fpu/scalar_operation.h"
#include "rw/math/vpu/vector2_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"   // the rig's file-local rotation builders (FX-GATE 2026-09-25)
#include "SDKs/XboxMath/XMScalarSinCos.h"   // XMMatrixRotationY's sine / cosine (FX-GATE 2026-09-25)

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using BrnDirector::Camera::BehaviourAftertouchCrash;
using BrnDirector::Camera::BehaviourSharedInfo;
using BrnDirector::Camera::BehaviourSharedPrepareReleaseInfo;
typedef BrnDirector::Camera::Camera DirectorCamera;
typedef BrnDirector::Camera::Utils::CameraShake::Parameters ShakeParameters;

// ============================================================================ fixtures
namespace fixture
{
    int giAsserts = 0;

    struct LookAtCall { Vector3 mEye; Vector3 mTarget; };
    LookAtCall gaLookAts[8];
    int giLookAts = 0;

    struct ShakeCall
    {
        const void* mpShake; const void* mpTransform; const void* mpParams; const void* mpRandom;
        ShakeParameters mParams;
        f32 mfTimestep; f32 mfScale;
    };
    ShakeCall gaShakes[8];
    int giShakes = 0;

    const void* gpLagParams = 0;
    f32 gfLagTimestep = -1.0f;
    int giLags = 0;
    Vector3 gLagOffset = { 0.0f, 0.0f, -1.0f, 0.0f };

    f32 gfRandomValue = 0.5f;
    int giRandomDraws = 0;
    int giValidates = 0;
    u32 guFlagsSet = 0;
    f32 gaSineLerp[3] = { -1.0f, -1.0f, -1.0f };
    int giSineLerps = 0;

    void BeginFrame()
    {
        giLookAts = 0; giShakes = 0; giLags = 0; giValidates = 0; guFlagsSet = 0; giSineLerps = 0;
    }
}

namespace CgsDev { namespace Assert {
    int BeginAssert() { return 0; }
    int FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
    {
        ++fixture::giAsserts;
        std::printf("ASSERT: %s (%s:%d)\n", lpcExpression, lpcFile, liLine);
        return 0;
    }
    void* EndAssert() { return 0; }
} }

f32 CgsNumeric::Random::RandomFloat()
{
    ++fixture::giRandomDraws;
    return fixture::gfRandomValue;
}

// The BRN_CAMRIG_DIAG witness at Update's tail prints through this sink; null == no log (the
// witness is diagnostics only and is not exercised here), so the two scalar formatters it links
// against are never reached.
CgsDev::Log::DebugPrint* CgsDev::Log::gpDebugPrint = 0;
CgsDev::StrStreamBase& CgsDev::StrStreamBase::operator<<(s32) { return *this; }
CgsDev::StrStreamBase& CgsDev::StrStreamBase::operator<<(f32) { return *this; }

// The inline CollisionPolicyAttachedToVehicle::Construct seeds its vehicle reference through these
// two (BrnVehicleRef.cpp); the rig never reads the reference.
BrnDirector::VehicleRef* BrnDirector::VehicleRef::Construct() { return this; }
void BrnDirector::VehicleRef::Set(EType, EActiveRaceCarIndex, u32) {}

namespace BrnDirector { namespace Camera {
    // The base head's own body (Behaviour.cpp), repeated: the derived Construct calls it by name.
    void Behaviour::Construct()
    {
        meTimestepType = BrnDirector::Timestep::E_WORLD;
        mbIsPrepared = false; mbHasFailed = false; mbTweakerAttached = false;
        mbCanSwitchToMeNow = false; mbCanSwitchFromMeNow = false; mpcDebugParametersName = 0;
    }
    rw::math::vpu::Matrix44Affine* Camera::ValidateTransformWithDebugInfo()
    {
        ++fixture::giValidates;
        return &mTransform;
    }
    void Camera::SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform) { mTransform = lrTransform; }
    const rw::math::vpu::Matrix44Affine& Camera::GetTransform() const { return mTransform; }
    void Camera::SetFOV(f32 lfFOV) { mfFOV = lfFOV; }
    f32 Camera::GetFOV() const { return mfFOV; }
    void CameraState::SetFlag(u32 luIndex, bool lbValue) { if (lbValue) fixture::guFlagsSet |= (1u << luIndex); }

namespace Utils {
    void PositionLag::Update(const Parameters& lrParameters, f32 lfTimestep,
                             rw::math::vpu::Matrix44Affine& lrTransform)
    {
        ++fixture::giLags;
        fixture::gpLagParams   = &lrParameters;
        fixture::gfLagTimestep = lfTimestep;
        lrTransform.wAxis = rw::math::vpu::Add(lrTransform.wAxis, fixture::gLagOffset);
    }

    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        if (fixture::giLookAts < 8)
        {
            fixture::gaLookAts[fixture::giLookAts].mEye    = lEyePosition;
            fixture::gaLookAts[fixture::giLookAts].mTarget = lTargetPosition;
        }
        ++fixture::giLookAts;

        const Vector3 lUp = { 0.0f, 1.0f, 0.0f, 0.0f };
        const Vector3 lDirection = rw::math::vpu::Subtract(lTargetPosition, lEyePosition);
        Vector3 lZaxis = { 0.0f, 0.0f, 1.0f, 0.0f };
        if (!rw::math::vpu::IsZero(lDirection, 1.1920929e-07f))
            lZaxis = rw::math::vpu::Normalize(lDirection);
        Vector3 lXaxis = rw::math::vpu::Cross(lUp, lZaxis);
        if (rw::math::vpu::IsZero(lXaxis, 1.1920929e-07f))
            lXaxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        else
            lXaxis = rw::math::vpu::Normalize(lXaxis);
        Matrix44Affine lLookAt;
        lLookAt.xAxis = lXaxis;
        lLookAt.yAxis = rw::math::vpu::Cross(lZaxis, lXaxis);
        lLookAt.zAxis = lZaxis;
        lLookAt.wAxis = lEyePosition;
        return lLookAt;
    }

    f32 SineLerp(f32 lfFrom, f32 lfTo, f32 lfParameter)
    {
        fixture::gaSineLerp[0] = lfFrom; fixture::gaSineLerp[1] = lfTo; fixture::gaSineLerp[2] = lfParameter;
        ++fixture::giSineLerps;
        const f32 lfEased = 1.0f - (std::cos(lfParameter * 3.1415927f) + 1.0f) * 0.5f;
        return lfFrom + lfEased * (lfTo - lfFrom);
    }

    void CameraShake::Update(Matrix44Affine& lrTransform, const Parameters& lrParams, Random& lrRandom,
                             f32 lfTimestep, f32 lfSpeedRatio)
    {
        if (fixture::giShakes < 8)
        {
            fixture::ShakeCall& lrCall = fixture::gaShakes[fixture::giShakes];
            lrCall.mpShake = this; lrCall.mpTransform = &lrTransform; lrCall.mpParams = &lrParams;
            lrCall.mpRandom = &lrRandom; lrCall.mParams = lrParams;
            lrCall.mfTimestep = lfTimestep; lrCall.mfScale = lfSpeedRatio;
        }
        ++fixture::giShakes;
    }
} } }

#include "fxcamrig_rig.inl"

// ============================================================================ harness
namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    void Check(bool lbOk, const char* lpcWhat)
    {
        ++giChecks;
        if (!lbOk)
        {
            ++giFailures;
            std::printf("FAIL: %s\n", lpcWhat);
        }
    }

    uint32_t Bits(float lfValue) { uint32_t luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }
    bool Same(float a, float b) { return Bits(a) == Bits(b); }

    bool Near(double a, double b, double lfTolerance = 2.0e-5)
    {
        return std::fabs(a - b) <= lfTolerance * (1.0 + std::fabs(b));
    }

    // -------------------------------------------------------------- the double oracle
    struct D3 { double x, y, z; };
    D3 V(const Vector3& v) { D3 r = { v.x, v.y, v.z }; return r; }
    D3 operator+(D3 a, D3 b) { D3 r = { a.x + b.x, a.y + b.y, a.z + b.z }; return r; }
    D3 operator-(D3 a, D3 b) { D3 r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
    D3 operator*(D3 a, double s) { D3 r = { a.x * s, a.y * s, a.z * s }; return r; }
    double Dot(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    D3 Norm(D3 a) { return a * (1.0 / std::sqrt(Dot(a, a))); }
    D3 Cross(D3 a, D3 b) { D3 r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; return r; }

    bool NearV(const Vector3& v, D3 e, double lfTolerance = 2.0e-5)
    {
        return Near(v.x, e.x, lfTolerance) && Near(v.y, e.y, lfTolerance) && Near(v.z, e.z, lfTolerance);
    }

    // .cpp:266..:270: y := -0.6 * sqrt(clamp(y^2, 0.1 * (x^2 + z^2), 0.6 * (x^2 + z^2)))
    D3 LookDown(D3 d)
    {
        const double lfH = d.x * d.x + d.z * d.z;
        double lfY = d.y * d.y;
        if (lfY < 0.1 * lfH) lfY = 0.1 * lfH;
        if (lfY > 0.6 * lfH) lfY = 0.6 * lfH;
        D3 r = { d.x, -0.6 * std::sqrt(lfY), d.z };
        return r;
    }

    // The zAxis of the vendor SLerp of two look-at frames == the slerp of the two unit directions
    // (with the SDK's 2-degree lerp threshold, flt_82001764).
    D3 SlerpDirection(D3 a, D3 b, double t)
    {
        a = Norm(a); b = Norm(b);
        double c = Dot(a, b);
        if (c < -1.0) c = -1.0;
        if (c > 1.0) c = 1.0;
        const double th = std::acos(c);
        if (th < 0.03490658476948738)
            return Norm(a * (1.0 - t) + b * t);
        return Norm(a * (std::sin((1.0 - t) * th) / std::sin(th)) + b * (std::sin(t * th) / std::sin(th)));
    }

    // -------------------------------------------------------------- instances
    alignas(16) unsigned char gaRig[sizeof(BehaviourAftertouchCrash)];
    alignas(16) unsigned char gaInfo[sizeof(BehaviourSharedInfo)];
    alignas(16) unsigned char gaCamera[sizeof(DirectorCamera)];
    BehaviourAftertouchCrash::Parameters gParameters;
    CgsNumeric::Random gSharedRandom;

    BehaviourAftertouchCrash& Rig()   { return *reinterpret_cast<BehaviourAftertouchCrash*>(gaRig); }
    BehaviourSharedInfo&      Info()  { return *reinterpret_cast<BehaviourSharedInfo*>(gaInfo); }
    DirectorCamera&           Cam()   { return *reinterpret_cast<DirectorCamera*>(gaCamera); }

    const f32 KF_DT_WORLD    = 0.015625f;    // 1/64
    const f32 KF_DT_NO_SLOMO = 0.03125f;     // 1/32
    const f32 KF_DT_GAME     = 0.0078125f;   // 1/128

    void NewRig()
    {
        std::memset(gaRig, 0, sizeof(gaRig));
        gParameters.Construct();
        Rig().BehaviourAftertouchCrash::Construct();
        Rig().SetParameters(&gParameters);
        BehaviourSharedPrepareReleaseInfo lPrepare = { 0, 0 };
        Rig().BehaviourAftertouchCrash::Prepare(lPrepare);
    }

    void NewWorld()
    {
        std::memset(gaInfo, 0, sizeof(gaInfo));
        std::memset(gaCamera, 0, sizeof(gaCamera));
        Info().mTimestep.Set(BrnDirector::VecFloat(KF_DT_GAME), BrnDirector::VecFloat(KF_DT_WORLD),
                             BrnDirector::VecFloat(KF_DT_NO_SLOMO), KF_DT_GAME, KF_DT_WORLD, KF_DT_NO_SLOMO);
        Info().mpRandom = &gSharedRandom;
    }

    void SetCar(D3 lPosition, D3 lVelocity)
    {
        Matrix44Affine& lrTransform = Info().mPlayerInfo.mRaceCarState.mTransform;
        lrTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lrTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lrTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lrTransform.wAxis = Vector3{ static_cast<f32>(lPosition.x), static_cast<f32>(lPosition.y),
                                     static_cast<f32>(lPosition.z), 0.0f };
        Info().mPlayerInfo.mRaceCarState.mLinearVelocity =
            Vector3{ static_cast<f32>(lVelocity.x), static_cast<f32>(lVelocity.y), static_cast<f32>(lVelocity.z), 0.0f };
    }

    void SetSticks(f32 lfHeldX, f32 lfHeldY, f32 lfOrbitX, f32 lfOrbitY)
    {
        Info().mRotationController.mStickVector          = Vector2{ lfHeldX, lfHeldY, 0.0f, 0.0f };
        Info().mSphericalRotationController.mStickVector = Vector2{ lfOrbitX, lfOrbitY, 0.0f, 0.0f };
    }

    bool Frame()
    {
        fixture::BeginFrame();
        return Rig().BehaviourAftertouchCrash::Update(Cam(), Info());
    }

    bool LanesAll(const ::VecFloat& lrValue, f32 lfExpected)
    {
        return Same(lrValue.x, lfExpected) && Same(lrValue.y, lfExpected)
            && Same(lrValue.z, lfExpected) && Same(lrValue.w, lfExpected);
    }
}

int main()
{
    // ------------------------------------------------------------ 1. Construct seeds the rig
    // (@0x822461F8: +0x320/+0x321 = 1, Random::Construct at +0x330, the shake and impact zeros,
    //  mfManualHeightAdjustment = splat(0) at +0x3B0) -- over a pool slot full of junk.
    {
        std::memset(gaRig, 0x5A, sizeof(gaRig));
        Rig().BehaviourAftertouchCrash::Construct();
        BehaviourAftertouchCrash& r = Rig();

        Check(r.mPositionLag.mbConstructed == true && r.mPositionLag.mbFirstFrame == true,
              "Construct: PositionLag::Construct ran (+0x320 / +0x321 = 1 -- its Update asserts mbConstructed)");

        uint64_t luSeed = 0xC87CD8C91AD0891Bull;
        uint32_t luSlot1 = 0;
        for (int i = 0; i < 7; ++i)
        {
            if (i == 0) luSlot1 = 0x3F800000u | (static_cast<uint32_t>(luSeed >> 32) >> 9);
            luSeed = luSeed * 0x5851F42D4C957F2Dull + 1u;
        }
        Check(r.mRandom.muSeed == luSeed && r.mRandom.muOldestBufferIndex == 0u
              && r.mRandom.mauIntegerBuffer[0] == 0x3F800000u && r.mRandom.mauIntegerBuffer[1] == luSlot1,
              "Construct: mRandom is Random::Construct-primed (default seed + 7 LCG steps, slot 0 == 1.0f, index 0)");
        Check(Same(r.mShake.mfCurrentWobbleX, 0.0f) && Same(r.mShake.mfCurrentWobbleY, 0.0f)
              && Same(r.mShake.mfCurrentWobbleXVel, 0.0f) && Same(r.mShake.mfCurrentWobbleYVel, 0.0f),
              "Construct: mShake cleared (+0x360..+0x36C)");
        Check(Same(r.mImpactEffect.mfImpactFactor, 0.0f)
              && Same(r.mImpactEffect.mCameraShake.mfCurrentWobbleX, 0.0f)
              && Same(r.mImpactEffect.mCameraShake.mfCurrentWobbleYVel, 0.0f),
              "Construct: mImpactEffect cleared (+0x370..+0x380)");
        Check(LanesAll(r.mfManualHeightAdjustment, 0.0f),
              "Construct: mfManualHeightAdjustment = splat(0.0f) (stvx128 +0x3B0)");
        Check(!r.mbManualCameraControl && !r.mbWasFallingDownwards && !r.mbDisableCollision
              && !r.mbIsTempDebugCrashCamera && !r.mbIsRandomStartTempDebugCrashCamera
              && Same(r.mfBounceShakeMultiplier, 1.0f) && Same(r.mfRollAngleRads, 0.0f)
              && Same(r.mfCloseupAmount0To1, 0.0f)
              && !r.mCollisionPolicy.mbAutoElevate && r.mCollisionPolicy.mbSmoothRadiusChanges
              && r.mCollisionPolicy.mbTestAgainstWorldOnly && r.mCollisionPolicy.mbUseFrustrumResolver,
              "Construct: the flag / scalar tail and the collision policy's four authored overrides");
    }

    // ------------------------------------------------------------ 2. CheckForPlayerCarBouncing
    // @0x8220F340: vcmpgefp v >= 0 (NaN fails -> arms), armed + v > 2.3 (flt_82CDA734) -> one bounce.
    {
        NewRig();
        NewWorld();
        struct Step { f32 mfVy; bool mbExpect; bool mbArmedAfter; const char* mpcWhat; };
        volatile f32 lfZero = 0.0f;
        const f32 lfNaN = std::nanf("");
        const Step laSteps[] =
        {
            { -1.0f,       false, true,  "bounce: moving down arms (returns false)" },
            {  0.0f,       false, false, "bounce: armed, v == +0 disarms without a bounce (0 > 2.3 fails)" },
            {  3.0f,       false, false, "bounce: not armed -> no bounce even at 3 m/s" },
            { -lfZero,     false, false, "bounce: v == -0 counts as >= 0 (not armed, stays unarmed)" },
            { -2.0f,       false, true,  "bounce: re-arm" },
            {  2.3f,       false, false, "bounce: 2.3 exactly is NOT above KF_MIN_SPEED_FOR_CAMERA_BOUNCE (disarms)" },
            { -2.0f,       false, true,  "bounce: re-arm again" },
            {  2.3000002f, true,  false, "bounce: the next float above 2.3 fires once and disarms" },
            {  5.0f,       false, false, "bounce: a second upward frame does not fire again" },
            { lfNaN,       false, true,  "bounce: a NaN vertical speed fails vcmpgefp and ARMS" },
            {  5.0f,       true,  false, "bounce: ...so the next upward frame fires" },
        };
        for (const Step& lrStep : laSteps)
        {
            Info().mPlayerInfo.mRaceCarState.mLinearVelocity = Vector3{ 0.0f, lrStep.mfVy, 0.0f, 0.0f };
            const bool lbResult = Rig().CheckForPlayerCarBouncing(Info());
            Check(lbResult == lrStep.mbExpect && Rig().mbWasFallingDownwards == lrStep.mbArmedAfter, lrStep.mpcWhat);
        }
    }

    // ------------------------------------------------------------ 3. follow (Showtime), frame 1
    const D3 kCar1 = { 100.0, 5.0, 200.0 };
    const D3 kVelocity = { 0.0, 0.0, 10.0 };
    const D3 kLagged1 = { 100.0, 5.0, 199.0 };       // the PositionLag fixture moves the frame by (0,0,-1)
    D3 lWorld = { 0.0, 0.0, -1.0 };
    D3 lDesired, lCameraPosition;
    double lfHeight = 2.0, lfDistance = 9.0;           // Prepare: mfFastHeight / mfFastDistance
    double lfBlend = 0.0;
    {
        NewRig();
        NewWorld();
        SetCar(kCar1, kVelocity);
        SetSticks(0.0f, 0.0f, 0.0f, 0.0f);
        fixture::giAsserts = 0;
        const bool lbReturned = Frame();
        BehaviourAftertouchCrash& r = Rig();

        lDesired = Norm(LookDown(kVelocity)) * -1.0;
        const double t = 10.0 / 25.0;                  // Min(speed / mfHeightDistanceVelocityRange, 1)
        lfHeight   += ((2.0 - 1.75) * t + 1.75 - lfHeight) * 0.1;
        lfDistance += ((9.0 - 4.0) * t + 4.0 - lfDistance) * 0.1;
        lfBlend = 0.01 + (0.95 - 0.01) * 0.01;          // not manual, |look| >= 2 -> the maximum, then eased
        const D3 lWorld0 = lWorld;
        lWorld = SlerpDirection(lWorld0, lDesired, lfBlend);
        const D3 lEye = kLagged1 + lWorld * lfDistance;
        lCameraPosition = lEye;
        lCameraPosition.y += lfHeight;

        Check(lbReturned && fixture::giAsserts == 0, "frame 1: Update returns true, no assert");
        Check(r.mbIsPrepared, "frame 1: the first frame prepares the rig");
        Check(NearV(r.mCrashPoint, D3{ 100.0, 4.0, 199.0 }) && NearV(r.mCurrentTargetPos, D3{ 100.0, 4.0, 199.0 }),
              "frame 1: crash point / target = the LAGGED origin one metre down");
        Check(Same(r.mfTimeSinceLastDecision, 1.0f), "frame 1: mfTimeSinceLastDecision = mfTimeBetweenDecisions");
        Check(fixture::giLags == 1 && fixture::gpLagParams == &gParameters.mLagParams
              && Same(fixture::gfLagTimestep, KF_DT_WORLD),
              "frame 1: PositionLag::Update(mLagParams, the E_WORLD step, the car frame)");
        Check(NearV(r.mDesiredWorldSpaceNormalizedVectorFromCar, lDesired),
              "frame 1: desired = -Normalize(velocity with the look-down y)");
        Check(Near(r.mfHeight, lfHeight) && Near(r.mfDistance, lfDistance),
              "frame 1: height / distance blend toward Lerp(slow, fast, speed / 25)");
        Check(Near(r.mfBlendFactor, lfBlend), "frame 1: blend factor = max (0.01) eased toward 0.95 by 0.01");
        Check(NearV(r.mWorldSpaceNormalizedVectorFromCar, lWorld),
              "frame 1: the camera direction SLerps from behind the car toward the desired one");
        Check(fixture::giLookAts == 4, "frame 1: four look-at frames (two blend, the camera, the close-up)");
        Check(NearV(fixture::gaLookAts[0].mEye, kLagged1) && NearV(fixture::gaLookAts[0].mTarget, kLagged1 + lWorld0 * lfDistance)
              && NearV(fixture::gaLookAts[1].mEye, kLagged1) && NearV(fixture::gaLookAts[1].mTarget, kLagged1 + lDesired * lfDistance),
              "frame 1: the blend frames look from the lagged origin along current / desired * mfDistance");
        Check(NearV(fixture::gaLookAts[2].mEye, lEye) && NearV(fixture::gaLookAts[2].mTarget, kLagged1),
              "frame 1: the camera looks at the lagged origin from mfDistance out along its direction");
        Check(NearV(Cam().mTransform.wAxis, lCameraPosition) && NearV(r.mCameraPositionLastFrame, lCameraPosition),
              "frame 1: the published camera sits mfHeight above that eye (no smoothing from the zero position)");
        Check(NearV(Cam().mTransform.zAxis, Norm(kLagged1 - lEye)), "frame 1: the published camera faces the car");
        Check(fixture::giShakes == 2
              && fixture::gaShakes[0].mpShake == &r.mShake && fixture::gaShakes[0].mpParams == &gParameters.mShakeParams
              && fixture::gaShakes[0].mpRandom == &r.mRandom && Same(fixture::gaShakes[0].mfTimestep, KF_DT_WORLD)
              && Same(fixture::gaShakes[0].mfScale, 1.0f),
              "frame 1: the rig's own shake: mShake with mShakeParams, the rig's Random, the E_WORLD step, scale 1.0");
        Check(fixture::giShakes == 2
              && fixture::gaShakes[1].mpShake == &r.mImpactEffect.mCameraShake
              && fixture::gaShakes[1].mpTransform == &Cam().mTransform && fixture::gaShakes[1].mpRandom == &gSharedRandom
              && Same(fixture::gaShakes[1].mParams.mfXYShakeMagnitudeDegs, 0.0f)
              && Same(fixture::gaShakes[1].mParams.mfZShakeMagnitudeDegs, 0.0f)
              && Same(fixture::gaShakes[1].mParams.mfXYWobbleMagnitudeDegs, 3.0f)
              && Same(fixture::gaShakes[1].mParams.mfWobbleCenteringFactor, 1.0f)
              && Same(fixture::gaShakes[1].mfTimestep, KF_DT_WORLD) && Same(fixture::gaShakes[1].mfScale, 0.0f),
              "frame 1: the impact shake on the CAMERA with {0,0,3,1}, the shared random, the step, impact * 60");
        Check(Same(Cam().mfFOV, 80.0f) && (fixture::guFlagsSet & 2u) != 0u && fixture::giValidates == 2,
              "frame 1: FOV = mfFOV (80), the camera marked valid, validated twice");
        Check(fixture::giSineLerps == 1 && Same(fixture::gaSineLerp[0], 0.0f) && Same(fixture::gaSineLerp[1], 1.0f)
              && Same(fixture::gaSineLerp[2], 0.0f),
              "frame 1: the close-up amount is eased through SineLerp(0, 1, mfCloseupAmount0To1)");
        const D3 lCloseupTarget = { 100.0, 4.5, 199.0 };
        const D3 lFlat = { lWorld.x, 0.0, lWorld.z };
        Check(NearV(fixture::gaLookAts[3].mTarget, lCloseupTarget) && NearV(fixture::gaLookAts[3].mEye, lCloseupTarget + lFlat * 4.0),
              "frame 1: the close-up aims half a metre below the lagged origin from 4 m out, flattened");
        Check(!r.mbManualCameraControl && Same(r.mfTimeSinceLastManualControl, KF_DT_NO_SLOMO)
              && LanesAll(r.mfManualHeightAdjustment, 0.0f),
              "frame 1: stick idle -> not manual, the idle clock runs on the no-slomo step");
    }

    // ------------------------------------------------------------ 4. follow, frame 2 (smoothing)
    const D3 kLagged2 = { 100.0, 5.0, 199.15625 };
    {
        SetCar(D3{ 100.0, 5.0, 200.15625 }, kVelocity);
        const bool lbReturned = Frame();
        BehaviourAftertouchCrash& r = Rig();
        lfHeight   += (1.85 - lfHeight) * 0.1;
        lfDistance += (6.0 - lfDistance) * 0.1;
        lWorld = SlerpDirection(lWorld, lDesired, lfBlend);
        const D3 lRaw = kLagged2 + lWorld * lfDistance + D3{ 0.0, lfHeight, 0.0 };
        const D3 lMove = lRaw - lCameraPosition;
        const bool lbSmoothed = Dot(lMove, lMove) < 25.0;
        const D3 lPrevious = lCameraPosition;
        lCameraPosition = lbSmoothed ? lPrevious + lMove * 0.5 : lRaw;

        Check(lbReturned && lbSmoothed && fixture::giAsserts == 0, "frame 2: a small move (< 5 m) is smoothed");
        Check(Near(r.mfHeight, lfHeight) && Near(r.mfDistance, lfDistance) && Near(r.mfBlendFactor, lfBlend),
              "frame 2: height / distance keep blending; the auto blend factor is re-seeded each frame");
        Check(NearV(r.mWorldSpaceNormalizedVectorFromCar, lWorld), "frame 2: the direction keeps swinging");
        Check(NearV(Cam().mTransform.wAxis, lCameraPosition) && NearV(r.mCameraPositionLastFrame, lCameraPosition),
              "frame 2: the camera goes KF_SMOOTHING_FACTOR (0.5) of the way from last frame's position");
    }

    // ------------------------------------------------------------ 5. orbit (manual control)
    {
        SetSticks(0.5f, 0.0f, 1.0f, 0.5f);              // held: 0.25 > 0.01 ; orbit x = 1, height y = 0.5
        const bool lbReturned = Frame();
        BehaviourAftertouchCrash& r = Rig();
        const D3 lStart = lWorld * -1.0;                // entering manual control: -current direction
        const double lfAngle = 1.0 * -0.05;             // stick x * KF_CAMERA_X_ROTATION_SPEED
        const double s = std::sin(lfAngle), c = std::cos(lfAngle);
        const D3 lOrbit = { lStart.x * c + lStart.z * s, lStart.y, -lStart.x * s + lStart.z * c };
        const D3 lDesired3 = Norm(LookDown(lOrbit)) * -1.0;
        lfHeight   += (1.85 - lfHeight) * 0.1;
        lfDistance += (6.0 - lfDistance) * 0.1;
        lfBlend += (0.95 - lfBlend) * 0.01;             // manual: no re-seed
        lWorld = SlerpDirection(lWorld, lDesired3, lfBlend);
        const D3 lEye = kLagged2 + lWorld * lfDistance + D3{ 0.0, 0.04, 0.0 };

        Check(lbReturned && r.mbManualCameraControl && Same(r.mfTimeSinceLastManualControl, 0.0f),
              "orbit: a held stick takes manual control and restarts the idle clock");
        Check(NearV(r.mManualCameraDirection, lOrbit),
              "orbit: the manual direction starts at -current and yaws by stick x * -0.05 rad (XMMatrixRotationY)");
        Check(LanesAll(r.mfManualHeightAdjustment, 0.5f * 0.08f), "orbit: height tweak = stick y * 0.08 (all four lanes)");
        Check(NearV(r.mDesiredWorldSpaceNormalizedVectorFromCar, lDesired3),
              "orbit: the desired direction follows the manual one (look-down applied)");
        Check(Near(r.mfBlendFactor, lfBlend), "orbit: manual control keeps easing the blend factor (no re-seed)");
        Check(NearV(fixture::gaLookAts[2].mEye, lEye),
              "orbit: the eye is lifted by the height tweak along K_V3D_YAXIS");
    }
    {
        SetSticks(0.5f, 0.0f, 0.0f, 1.0f);
        Rig().mfManualHeightAdjustment = rw::math::vpu::Splat(1.99f);
        const Vector3 lBefore = Rig().mManualCameraDirection;
        Frame();
        Check(LanesAll(Rig().mfManualHeightAdjustment, 2.0f),
              "orbit: the height tweak clamps at KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK (1.99 + 0.08 -> 2.0)");
        Check(Same(Rig().mManualCameraDirection.x, lBefore.x) && Same(Rig().mManualCameraDirection.z, lBefore.z),
              "orbit: a zero stick x leaves the manual direction where it is");
    }
    {
        SetSticks(0.0f, 0.0f, 0.0f, 0.0f);              // released: the idle clock runs
        Rig().mfTimeSinceLastManualControl = 2.97f;
        Frame();
        Check(!Rig().mbManualCameraControl && Same(Rig().mfTimeSinceLastManualControl, 2.97f + KF_DT_NO_SLOMO),
              "orbit: after KF_TIME_UNTIL_CAMERA_RESET (3 s) idle, a moving car (|v|^2 = 100 >= 3) resets the camera");
        Check(LanesAll(Rig().mfManualHeightAdjustment, 2.0f * 0.99f),
              "orbit: once released the height tweak decays by KF_CAMERA_RESET_SPEED (0.99)");
    }
    {
        SetSticks(0.5f, 0.0f, 0.0f, 0.0f);              // take control again
        Frame();
        SetSticks(0.0f, 0.0f, 0.0f, 0.0f);
        SetCar(D3{ 100.0, 5.0, 200.15625 }, D3{ 0.0, 0.0, 1.0 });
        Rig().mfTimeSinceLastManualControl = 2.97f;
        Frame();
        Check(Rig().mbManualCameraControl,
              "orbit: a slow car (|v|^2 = 1 < KF_CAMERA_RESET_MIN_SPEED 3) keeps manual control past 3 s");
        SetCar(D3{ 100.0, 5.0, 200.15625 }, kVelocity);
        Rig().mfTimeSinceLastManualControl = 1.0f;
        Frame();
        Check(Rig().mbManualCameraControl, "orbit: within 3 s idle the camera stays manual even when moving");
    }

    // ------------------------------------------------------------ 6. bounce -> impact shake
    {
        SetSticks(0.0f, 0.0f, 0.0f, 0.0f);
        Rig().mfTimeSinceLastManualControl = 10.0f;
        SetCar(D3{ 100.0, 5.0, 200.15625 }, D3{ 0.0, -3.0, 10.0 });
        Frame();
        const bool lbArmed = Rig().mbWasFallingDownwards;
        Check(lbArmed && Same(Rig().mImpactEffect.mfImpactFactor, 0.0f) && Same(fixture::gaShakes[1].mfScale, 0.0f),
              "bounce: falling arms the detector, no impact yet");
        SetCar(D3{ 100.0, 5.0, 200.15625 }, D3{ 0.0, 3.0, 10.0 });
        Frame();
        Check(!Rig().mbWasFallingDownwards && Near(fixture::gaShakes[1].mfScale, 0.3 * 60.0, 1.0e-6)
              && Near(Rig().mImpactEffect.mfImpactFactor, 0.3 - 0.3 * 0.06, 1.0e-6),
              "bounce: 3 m/s up after falling registers 1.0 * 0.3, shaken at 0.3 * 60 this frame, decayed by 0.06");
        Frame();
        Check(Near(fixture::gaShakes[1].mfScale, 0.282 * 60.0, 1.0e-5)
              && Near(Rig().mImpactEffect.mfImpactFactor, 0.282 - 0.282 * 0.06, 1.0e-5),
              "bounce: no second bounce; the impact keeps decaying");
    }

    // ------------------------------------------------------------ 7. roll and close-up
    {
        Rig().mfRollAngleRads = 0.3f;
        Frame();
        const Matrix44Affine lFrame = BrnDirector::Camera::Utils::CreateLookAt(fixture::gaLookAts[2].mEye,
                                                                              fixture::gaLookAts[2].mTarget);
        const double c = std::cos(0.3), s = std::sin(0.3);
        const D3 lX = V(lFrame.xAxis) * c + V(lFrame.yAxis) * s;
        const D3 lY = V(lFrame.xAxis) * -s + V(lFrame.yAxis) * c;
        Check(NearV(Cam().mTransform.xAxis, lX) && NearV(Cam().mTransform.yAxis, lY)
              && NearV(Cam().mTransform.zAxis, V(lFrame.zAxis)),
              "roll: the published camera is rolled by mfRollAngleRads (MakeRotationZ pre-multiplied)");
        Rig().mfRollAngleRads = 0.0f;
        Rig().mfCloseupAmount0To1 = 1.0f;
        Frame();
        Check(Same(fixture::gaSineLerp[2], 1.0f) && NearV(Cam().mTransform.wAxis, V(fixture::gaLookAts[3].mEye))
              && NearV(Cam().mTransform.zAxis, Norm(V(fixture::gaLookAts[3].mTarget) - V(fixture::gaLookAts[3].mEye))),
              "close-up: a full close-up amount publishes the close-up look-at");
        Rig().mfCloseupAmount0To1 = 0.0f;
    }

    // ------------------------------------------------------------ 8. the debug crash camera (takedowns)
    {
        NewRig();
        NewWorld();
        SetCar(kCar1, kVelocity);
        SetSticks(0.0f, 0.0f, 0.0f, 0.1f);              // inside sfDeadzone (0.2)
        Rig().mbIsTempDebugCrashCamera = true;
        Rig().mbDisableCollision = true;
        Frame();
        BehaviourAftertouchCrash& r = Rig();
        Check(Near(fixture::gaShakes[1].mfScale, 0.3 * 60.0, 1.0e-6)
              && Near(r.mImpactEffect.mfImpactFactor, 0.3 - 0.3 * 0.06, 1.0e-6),
              "debug cam: the first frame kicks the impact shake with KF_BOUNCE_SHAKE_MAGNITUDE (0.3)");
        double h = 2.0 + ((2.0 - 1.75) * 0.5 + 1.75 - 2.0) * 0.1;
        double d = 9.0 + ((9.0 - 4.0) * 0.5 + 4.0 - 9.0) * 0.1;
        Check(Same(r.mfDebugCrashCameraParam0to1, 0.5f) && Near(r.mfHeight, h) && Near(r.mfDistance, d),
              "debug cam: framing from the 0..1 parameter (0.5 from Prepare), the dead-zoned stick ignored");
        SetSticks(0.0f, 0.0f, 0.0f, 1.0f);
        Frame();
        h += ((2.0 - 1.75) * 0.4921875 + 1.75 - h) * 0.1;
        d += ((9.0 - 4.0) * 0.4921875 + 4.0 - d) * 0.1;
        Check(Same(r.mfDebugCrashCameraParam0to1, 0.4921875f) && Near(r.mfHeight, h) && Near(r.mfDistance, d),
              "debug cam: stick y walks the parameter by the E_GAME step * sfAdjustmentRate (0.5 - 1/128)");
        r.mfDebugCrashCameraParam0to1 = 0.001f;
        Frame();
        Check(Same(r.mfDebugCrashCameraParam0to1, 0.0f), "debug cam: the parameter clamps at 0");
        SetSticks(0.0f, 0.0f, 0.0f, -1.0f);
        r.mfDebugCrashCameraParam0to1 = 0.999f;
        Frame();
        Check(Same(r.mfDebugCrashCameraParam0to1, 1.0f), "debug cam: the parameter clamps at 1");
        SetSticks(0.0f, 0.0f, 0.0f, -0.1999f);
        Frame();
        Check(Same(r.mfDebugCrashCameraParam0to1, 1.0f), "debug cam: |stick y| < sfDeadzone leaves it alone");
    }

    // ------------------------------------------------------------ 9. the random start
    {
        NewRig();
        NewWorld();
        SetCar(kCar1, kVelocity);
        SetSticks(0.0f, 0.0f, 0.0f, 0.0f);
        Rig().mbIsTempDebugCrashCamera = true;
        Rig().mbIsRandomStartTempDebugCrashCamera = true;
        fixture::giRandomDraws = 0;
        fixture::gfRandomValue = 0.5f;
        Frame();
        Check(fixture::giRandomDraws == 1 && Rig().mbManualCameraControl
              && NearV(Rig().mManualCameraDirection, D3{ std::sin(0.5), 0.0, std::cos(0.5) }, 1.0e-6),
              "random start: one [0,1) draw, the direction is RotationY(draw).zAxis, manual forced");
        Frame();
        Check(fixture::giRandomDraws == 1 && Rig().mbManualCameraControl,
              "random start: no further draw once prepared; manual stays forced");
    }

    // ------------------------------------------------------------ 10. the console's VMX forms
#if FXCAMRIG_HAVE_HELPERS
    {
        const f32 lfNaN = std::nanf("");
        Check(IsZeroVmx(Vector3{ lfNaN, 0.0f, 0.0f, 0.0f }, 1.1920929e-07f),
              "IsZeroVmx: a NaN lane counts as zero (vcmpgtfp. + CR6 all-false)");
        Check(IsZeroVmx(Vector3{ 1.0e-8f, -1.0e-8f, 0.0f, 99.0f }, 1.1920929e-07f)
              && !IsZeroVmx(Vector3{ 0.0f, 0.0f, -2.0e-7f, 0.0f }, 1.1920929e-07f),
              "IsZeroVmx: lanes x, y, z against the tolerance, w ignored");
        const f32 lfClampedNaN = VecFloatClamp(lfNaN, -2.0f, 2.0f);
        Check(lfClampedNaN != lfClampedNaN && Same(VecFloatClamp(2.07f, -2.0f, 2.0f), 2.0f)
              && Same(VecFloatClamp(-0.0f, 0.0f, 1.0f), 0.0f),
              "VecFloatClamp: vmaxfp/vminfp -- NaN stays NaN, +0 beats -0");
    }
#else
    Check(false, "IsZeroVmx exists (the console's vector IsZero)");
    Check(false, "(IsZeroVmx lanes: absent)");
    Check(false, "VecFloatClamp exists (the console's vmaxfp/vminfp clamp)");
#endif

    Check(fixture::giAsserts == 0, "no assert fired in any scenario");

    std::printf("fxcamrig rig: %d/%d checks passed%s\n", giChecks - giFailures, giChecks,
                FXCAMRIG_HAVE_RIG ? "" : " (Update ABSENT in this revision -- the base Behaviour::Update ran)");
    return giFailures == 0 ? 0 : 1;
}
