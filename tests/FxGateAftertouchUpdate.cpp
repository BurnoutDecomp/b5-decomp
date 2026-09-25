// FX-GATE (crash parity 2026-09-25) -- run_fxgate_aftertouch_update.py: BehaviourAftertouchCrash::Update @0x82228158,
// the WHOLE production body, against the console's words run WHOLE on emu64 (FxGateAftertouchUpdateData.h).
//
// The production text is extracted by run_fxcamrig_rig.generate (Update, its file-local helpers and KF statics,
// CheckForPlayerCarBouncing, CameraImpactEffect::RegisterImpact / ::Update) into fxcamrig_rig.inl; the real
// CgsNumeric::Random::RandomFloat (CgsRandom.cpp) is linked beside it. The emulator interprets the same bodies' console
// words (and XMMatrixRotationY / XMScalarSinCos). The out-of-TU callees are stubbed on BOTH sides with the same
// single-rounding semantics (scratch/CRASHPARITY_0922/fixes/FX-GATE.update/atc_update_emu.py):
//   PositionLag::Update         wAxis += the frame's lag offset (four f32 adds)
//   Utils::CreateLookAt         rows (1,0,0,0) / (0,1,0,0) / target - eye / eye
//   rw::math::vpu::SLerp        every lane from + (to - from) * amount   (the SLerp itself is FX-GATE item B)
//   CameraShake::Update         records its arguments; transform.wAxis.x += scale * 0.001f
//   Utils::SineLerp             from + (to - from) * t
//   Camera::SetFOV              the console's inline tripwire `fcmpu fov, 0.0 ; bgt` (0x8222928C), then the store
// Each frame starts from the CONSOLE's state (the rig and the random are re-seeded from the previous frame's expected
// words), so every frame is an independent check. A word compares equal when the bits match, or when both are NaN.
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
#include "SDKs/XboxMath/XMVectorSinCos.h"
#include "SDKs/XboxMath/XMScalarSinCos.h"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using BrnDirector::Camera::BehaviourAftertouchCrash;
using BrnDirector::Camera::BehaviourSharedInfo;
typedef BrnDirector::Camera::Camera DirectorCamera;
typedef BrnDirector::Camera::Utils::CameraShake::Parameters ShakeParameters;

namespace
{
    uint32_t Bits(float lfValue) { uint32_t luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }
    float Float(uint32_t luBits) { float lfValue; std::memcpy(&lfValue, &luBits, 4); return lfValue; }
}

// ============================================================================ the stubs' records
namespace fixture
{
    BehaviourAftertouchCrash::Parameters gParameters;
    CgsNumeric::Random gSharedRandom;
    alignas(16) unsigned char gaRig[sizeof(BehaviourAftertouchCrash)];
    alignas(16) unsigned char gaInfo[sizeof(BehaviourSharedInfo)];
    alignas(16) unsigned char gaCamera[sizeof(DirectorCamera)];
    BehaviourAftertouchCrash& Rig()  { return *reinterpret_cast<BehaviourAftertouchCrash*>(gaRig); }
    BehaviourSharedInfo&      Info() { return *reinterpret_cast<BehaviourSharedInfo*>(gaInfo); }
    DirectorCamera&           Cam()  { return *reinterpret_cast<DirectorCamera*>(gaCamera); }

    Vector3 gLagOffset = { 0.0f, 0.0f, 0.0f, 0.0f };
    u32 guFlagsSet = 0;
    uint32_t gauRecords[61];
    int giLags = 0, giLookAts = 0, giSLerps = 0, giShakes = 0, giSineLerps = 0, giAsserts = 0;

    void BeginFrame()
    {
        std::memset(gauRecords, 0, sizeof(gauRecords));
        giLags = giLookAts = giSLerps = giShakes = giSineLerps = giAsserts = 0;
        guFlagsSet = 0;
    }
}

namespace CgsDev { namespace Assert {
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { ++fixture::giAsserts; return 0; }
    void* EndAssert() { return 0; }
} }

// The BRN_CAMRIG_DIAG witness prints through this sink; null == no log.
CgsDev::Log::DebugPrint* CgsDev::Log::gpDebugPrint = 0;
CgsDev::StrStreamBase& CgsDev::StrStreamBase::operator<<(s32) { return *this; }
CgsDev::StrStreamBase& CgsDev::StrStreamBase::operator<<(f32) { return *this; }

BrnDirector::VehicleRef* BrnDirector::VehicleRef::Construct() { return this; }
void BrnDirector::VehicleRef::Set(EType, EActiveRaceCarIndex, u32) {}

namespace BrnDirector { namespace Camera {
    void Behaviour::Construct()
    {
        meTimestepType = BrnDirector::Timestep::E_WORLD;
        mbIsPrepared = false; mbHasFailed = false; mbTweakerAttached = false;
        mbCanSwitchToMeNow = false; mbCanSwitchFromMeNow = false; mpcDebugParametersName = 0;
    }
    rw::math::vpu::Matrix44Affine* Camera::ValidateTransformWithDebugInfo() { return &mTransform; }
    void Camera::SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform) { mTransform = lrTransform; }
    const rw::math::vpu::Matrix44Affine& Camera::GetTransform() const { return mTransform; }
    void Camera::SetFOV(f32 lfFOV)
    {
        CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f");   // the console's inline copy: fcmpu f30, f29(0.0) ; bgt
        mfFOV = lfFOV;
    }
    f32 Camera::GetFOV() const { return mfFOV; }
    void CameraState::SetFlag(u32 luIndex, bool lbValue) { if (lbValue) fixture::guFlagsSet |= (1u << luIndex); }

namespace Utils {
    void PositionLag::Update(const Parameters& lrParameters, f32 lfTimestep, rw::math::vpu::Matrix44Affine& lrTransform)
    {
        if (fixture::giLags++ == 0)
        {
            fixture::gauRecords[1] = Bits(lfTimestep);
            fixture::gauRecords[2] = (&lrParameters == &fixture::gParameters.mLagParams) ? 1u : 0u;
        }
        fixture::gauRecords[0] = static_cast<uint32_t>(fixture::giLags);
        lrTransform.wAxis = rw::math::vpu::Add(lrTransform.wAxis, fixture::gLagOffset);
    }

    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        if (fixture::giLookAts < 4)
        {
            uint32_t* lpu = fixture::gauRecords + 4 + 8 * fixture::giLookAts;
            lpu[0] = Bits(lEyePosition.x); lpu[1] = Bits(lEyePosition.y); lpu[2] = Bits(lEyePosition.z); lpu[3] = Bits(lEyePosition.w);
            lpu[4] = Bits(lTargetPosition.x); lpu[5] = Bits(lTargetPosition.y); lpu[6] = Bits(lTargetPosition.z); lpu[7] = Bits(lTargetPosition.w);
        }
        fixture::gauRecords[3] = static_cast<uint32_t>(++fixture::giLookAts);
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lResult.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lResult.zAxis = Vector3{ lTargetPosition.x - lEyePosition.x, lTargetPosition.y - lEyePosition.y,
                                 lTargetPosition.z - lEyePosition.z, lTargetPosition.w - lEyePosition.w };
        lResult.wAxis = lEyePosition;
        return lResult;
    }

    f32 SineLerp(f32 lfFrom, f32 lfTo, f32 lfParameter)
    {
        if (fixture::giSineLerps++ == 0)
        {
            fixture::gauRecords[57] = Bits(lfFrom); fixture::gauRecords[58] = Bits(lfTo); fixture::gauRecords[59] = Bits(lfParameter);
        }
        fixture::gauRecords[56] = static_cast<uint32_t>(fixture::giSineLerps);
        return lfFrom + (lfTo - lfFrom) * lfParameter;
    }

    void CameraShake::Update(Matrix44Affine& lrTransform, const Parameters& lrParams, Random& lrRandom,
                             f32 lfTimestep, f32 lfSpeedRatio)
    {
        if (fixture::giShakes < 2)
        {
            uint32_t* lpu = fixture::gauRecords + 40 + 8 * fixture::giShakes;
            lpu[0] = (this == &fixture::Rig().mShake) ? 1u : (this == &fixture::Rig().mImpactEffect.mCameraShake) ? 2u : 9u;
            lpu[1] = Bits(lrParams.mfXYShakeMagnitudeDegs); lpu[2] = Bits(lrParams.mfZShakeMagnitudeDegs);
            lpu[3] = Bits(lrParams.mfXYWobbleMagnitudeDegs); lpu[4] = Bits(lrParams.mfWobbleCenteringFactor);
            lpu[5] = Bits(lfTimestep); lpu[6] = Bits(lfSpeedRatio);
            lpu[7] = (&lrRandom == &fixture::Rig().mRandom) ? 1u : (&lrRandom == &fixture::gSharedRandom) ? 2u : 9u;
        }
        fixture::gauRecords[39] = static_cast<uint32_t>(++fixture::giShakes);
        lrTransform.wAxis.x = lrTransform.wAxis.x + lfSpeedRatio * 0.001f;
    }
} } }

namespace rw { namespace math { namespace vpu {
    // The SLerp stand-in (#define'd over the vendor name for the extracted body only).
    Matrix44Affine FxGateSLerpStub(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo, float lfAmount, Vector3*)
    {
        if (fixture::giSLerps < 2)
            fixture::gauRecords[37 + fixture::giSLerps] = Bits(lfAmount);
        fixture::gauRecords[36] = static_cast<uint32_t>(++fixture::giSLerps);
        const Vector3* lapFrom[4] = { &lrFrom.xAxis, &lrFrom.yAxis, &lrFrom.zAxis, &lrFrom.wAxis };
        const Vector3* lapTo[4]   = { &lrTo.xAxis, &lrTo.yAxis, &lrTo.zAxis, &lrTo.wAxis };
        Matrix44Affine lResult;
        Vector3* lapOut[4] = { &lResult.xAxis, &lResult.yAxis, &lResult.zAxis, &lResult.wAxis };
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const Vector3& a = *lapFrom[liRow];
            const Vector3& b = *lapTo[liRow];
            *lapOut[liRow] = Vector3{ a.x + (b.x - a.x) * lfAmount, a.y + (b.y - a.y) * lfAmount,
                                      a.z + (b.z - a.z) * lfAmount, a.w + (b.w - a.w) * lfAmount };
        }
        return lResult;
    }
} } }

#define SLerp FxGateSLerpStub
#include "fxcamrig_rig.inl"
#undef SLerp

#include "FxGateAftertouchUpdateData.h"

// ============================================================================ the field walks (atc_update_emu.py's
// RIG_FIELDS / PARAM_FIELDS, in the same order)
namespace
{
    struct Walk
    {
        const uint32_t* mpuIn;   // write the members from these words
        uint32_t*       mpuOut;  // or read the members into these
        unsigned        muIndex;

        void Word(uint32_t& lu) { if (mpuIn) lu = mpuIn[muIndex]; else mpuOut[muIndex] = lu; ++muIndex; }
        void Flag(bool& lb) { if (mpuIn) lb = mpuIn[muIndex] != 0u; else mpuOut[muIndex] = lb ? 1u : 0u; ++muIndex; }
        void Real(f32& lf) { if (mpuIn) lf = Float(mpuIn[muIndex]); else mpuOut[muIndex] = Bits(lf); ++muIndex; }
        template <class V> void Lanes(V& lv) { Real(lv.x); Real(lv.y); Real(lv.z); Real(lv.w); }
    };

    void WalkRig(BehaviourAftertouchCrash& r, Walk& w)
    {
        w.Flag(r.mbIsPrepared);
        w.Lanes(r.mCurrentTargetPos);
        w.Lanes(r.mWorldSpaceNormalizedVectorFromCar);
        w.Lanes(r.mDesiredWorldSpaceNormalizedVectorFromCar);
        w.Lanes(r.mCrashPoint);
        w.Lanes(r.mCameraPositionLastFrame);
        w.Real(r.mImpactEffect.mfImpactFactor);
        w.Real(r.mfHeight);
        w.Real(r.mfDistance);
        w.Real(r.mfBlendFactor);
        w.Real(r.mfTimeSinceLastDecision);
        w.Real(r.mfTimeSinceLastManualControl);
        w.Lanes(r.mManualCameraDirection);
        w.Lanes(r.mfManualHeightAdjustment);
        w.Flag(r.mbManualCameraControl);
        w.Flag(r.mbWasFallingDownwards);
        w.Flag(r.mbIsTempDebugCrashCamera);
        w.Flag(r.mbIsRandomStartTempDebugCrashCamera);
        w.Real(r.mfDebugCrashCameraParam0to1);
        w.Real(r.mfBounceShakeMultiplier);
        w.Real(r.mfRollAngleRads);
        w.Real(r.mfCloseupAmount0To1);
    }

    void WalkParameters(BehaviourAftertouchCrash::Parameters& p, Walk& w)
    {
        w.Real(p.mShakeParams.mfXYShakeMagnitudeDegs);
        w.Real(p.mShakeParams.mfZShakeMagnitudeDegs);
        w.Real(p.mShakeParams.mfXYWobbleMagnitudeDegs);
        w.Real(p.mShakeParams.mfWobbleCenteringFactor);
        w.Real(p.mfSlowDistance);
        w.Real(p.mfSlowHeight);
        w.Real(p.mfFastDistance);
        w.Real(p.mfFastHeight);
        w.Real(p.mfPitch);
        w.Real(p.mfFOV);
        w.Real(p.mfBlendFactorBlendFactor);
        w.Real(p.mfMinimumBlendFactor);
        w.Real(p.mfMaximumBlendFactor);
        w.Real(p.mfManualBlendFactor);
        w.Real(p.mfHeightDistanceBlendFactor);
        w.Real(p.mfHeightDistanceVelocityRange);
        w.Real(p.mfTimeBetweenDecisions);
    }

    void WalkRandom(CgsNumeric::Random& lr, Walk& w)
    {
        for (u32 k = 0; k < 8; ++k)
            w.Word(lr.mauIntegerBuffer[k]);
        uint32_t luHi = static_cast<uint32_t>(lr.muSeed >> 32), luLo = static_cast<uint32_t>(lr.muSeed);
        w.Word(luHi);
        w.Word(luLo);
        if (w.mpuIn)
            lr.muSeed = (static_cast<u64>(luHi) << 32) | luLo;
        w.Word(lr.muOldestBufferIndex);
    }

    void WalkCameraIn(DirectorCamera& c, Walk& w)
    {
        w.Lanes(c.mTransform.xAxis); w.Lanes(c.mTransform.yAxis); w.Lanes(c.mTransform.zAxis); w.Lanes(c.mTransform.wAxis);
        w.Real(c.mfFOV);
    }

    bool IsNaNBits(uint32_t lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(uint32_t luA, uint32_t luB) { return luA == luB || (IsNaNBits(luA) && IsNaNBits(luB)); }
}

int main()
{
    static_assert(sizeof(fixture::gauRecords) / sizeof(fixture::gauRecords[0]) == 61, "record layout");
    unsigned luChecks = 0, luFailures = 0, luPrinted = 0;
    if (kuRecordWords != 61u || kuFrameOutWords != kuRigWords + kuRandomWords + 18u + kuRecordWords)
    {
        std::printf("FAIL  the data header's layout does not match this test\n");
        std::printf("FxGateAftertouchUpdate: 1 checks, 1 failures\n");
        return 1;
    }
    for (unsigned luCase = 0; luCase < kuUpdateCases; ++luCase)
    {
        using namespace fixture;
        std::memset(gaRig, 0, sizeof(gaRig));
        std::memset(gaInfo, 0, sizeof(gaInfo));
        std::memset(gaCamera, 0, sizeof(gaCamera));
        std::memset(&gParameters, 0, sizeof(gParameters));
        const uint32_t* lpuCase = kaauCaseIn[luCase];
        Walk w = { lpuCase, 0, 0 };
        WalkRig(Rig(), w);
        WalkParameters(gParameters, w);
        WalkCameraIn(Cam(), w);
        WalkRandom(gSharedRandom, w);
        Rig().SetParameters(&gParameters);
        Info().mpRandom = &gSharedRandom;

        for (unsigned luFrame = 0; luFrame < kuUpdateFrames; ++luFrame)
        {
            const unsigned luRow = luCase * kuUpdateFrames + luFrame;
            const uint32_t* lpuIn = kaauFrameIn[luRow];
            const uint32_t* lpuExpected = kaauFrameOut[luRow];
            BehaviourSharedInfo& s = Info();
            s.mRotationController.mStickVector.x = Float(lpuIn[0]);
            s.mRotationController.mStickVector.y = Float(lpuIn[1]);
            s.mSphericalRotationController.mStickVector.x = Float(lpuIn[2]);
            s.mSphericalRotationController.mStickVector.y = Float(lpuIn[3]);
            Walk lCar = { lpuIn + 4, 0, 0 };
            Matrix44Affine& lrCar = s.mPlayerInfo.mRaceCarState.mTransform;
            lCar.Lanes(lrCar.xAxis); lCar.Lanes(lrCar.yAxis); lCar.Lanes(lrCar.zAxis); lCar.Lanes(lrCar.wAxis);
            lCar.Lanes(s.mPlayerInfo.mRaceCarState.mLinearVelocity);
            s.mTimestep.mafTimestep[BrnDirector::Timestep::E_WORLD]          = Float(lpuIn[24]);
            s.mTimestep.mafTimestep[BrnDirector::Timestep::E_WORLD_NO_SLOMO] = Float(lpuIn[25]);
            s.mTimestep.mafTimestep[BrnDirector::Timestep::E_GAME]           = Float(lpuIn[26]);
            gLagOffset = Vector3{ Float(lpuIn[27]), Float(lpuIn[28]), Float(lpuIn[29]), Float(lpuIn[30]) };

            BeginFrame();
            const bool lbReturned = Rig().BehaviourAftertouchCrash::Update(Cam(), Info());
            gauRecords[60] = static_cast<uint32_t>(giAsserts);

            uint32_t lauOut[256] = {};
            Walk lRead = { 0, lauOut, 0 };
            WalkRig(Rig(), lRead);
            WalkRandom(gSharedRandom, lRead);
            lRead.Lanes(Cam().mTransform.xAxis); lRead.Lanes(Cam().mTransform.yAxis);
            lRead.Lanes(Cam().mTransform.zAxis); lRead.Lanes(Cam().mTransform.wAxis);
            lRead.Real(Cam().mfFOV);
            lauOut[lRead.muIndex++] = (guFlagsSet >> 1) & 1u;
            for (unsigned k = 0; k < 61; ++k)
                lauOut[lRead.muIndex++] = gauRecords[k];

            // four checks a frame: the rig, the random, the camera, the call records
            const unsigned lauGroupEnd[4] = { kuRigWords, kuRigWords + kuRandomWords, kuRigWords + kuRandomWords + 18u,
                                              kuFrameOutWords };
            unsigned luStart = 0;
            for (unsigned luGroup = 0; luGroup < 4; ++luGroup)
            {
                bool lbOk = (luGroup != 0) || lbReturned;
                for (unsigned k = luStart; k < lauGroupEnd[luGroup]; ++k)
                {
                    if (!Same(lauOut[k], lpuExpected[k]))
                    {
                        lbOk = false;
                        if (luPrinted < 40)
                        {
                            ++luPrinted;
                            std::printf("FAIL  case %u (%s) frame %u: %s = %08X, the console has %08X\n", luCase,
                                        kapcCaseNames[luCase], luFrame, kapcOutNames[k], lauOut[k], lpuExpected[k]);
                        }
                    }
                }
                ++luChecks;
                if (!lbOk)
                    ++luFailures;
                luStart = lauGroupEnd[luGroup];
            }

            // The next frame starts from the console's state.
            Walk lSeed = { lpuExpected, 0, 0 };
            WalkRig(Rig(), lSeed);
            WalkRandom(gSharedRandom, lSeed);
        }
    }
    std::printf("FxGateAftertouchUpdate: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
