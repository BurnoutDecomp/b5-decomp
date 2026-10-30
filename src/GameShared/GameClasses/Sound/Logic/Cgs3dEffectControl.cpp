// ============================================================================
// Cgs3dEffectControl.cpp -- CgsSound::Logic::Cgs3dEffectControl runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   Cgs3dEffectControl::Generate3DParams            @ 0x826EA7D8
//   Cgs3dEffectControl::SetMixerInputValueUnbound   @ 0x82682570
//   Cgs3dEffectControl::~Cgs3dEffectControl (scalar deleting dtor) @ 0x826C3E70
//
// The engine 3D-positional effect control: samples the sound-logic MicrophoneSystem
// each frame and drives the dynamic-mixer (Nicotine::DMixIO) panning/distance input
// slots. See Cgs3dEffectControl.h for the layout + offset map.
//
// The private EffectBase mpDynamicMixIo (+0x30) is reached BY NAME through the
// additive accessor EffectBase::GetDMixIOPtr(); the microphone-system listener
// positions inside CgsSound::Logic::Module are read at the X360-attested byte
// offsets (+0x29A0 for mMicrophoneSystem, +0x40/+0x180 for the two listener wAxes)
// and are FLAGGED -- those regions belong to the Module's own (not-fully-modelled)
// surface.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "SDKs/XboxMath/XMVectorACos.h"   // XboxMath::XMVectorACos (X360 0x821F0980)

#include <vectorintrinsics.h>
#include <cmath>

namespace CgsSound
{
namespace Logic
{

void (*Cgs3dEffectControl::spDebugRenderingFunction)(const Cgs3dEffectControl*) = 0;

Cgs3dEffectControl::Cgs3dEffectControl()
    : EffectControl()
    , mfDistanceToMic()
    , mfVelocityToMic()
    , mpEmitterPosition(0)
    , mpEmitterDirection(0)
    , mEmitterPosition()
    , mEmitterDirection()
    , mDebugRenderingMessageData()
{
}

ClassTypeInfo<EffectControl>* Cgs3dEffectControl::GetTypeInfo() const
{
    static ClassTypeInfo<EffectControl> sTypeInfo(
        0, "Cgs3dEffectControl", EffectControl::GetStaticTypeInfo(), 0);
    return &sTypeInfo;
}

const char* Cgs3dEffectControl::GetTypeName() const
{
    return "Cgs3dEffectControl";
}

void Cgs3dEffectControl::AttachEmitterPosition(
    const rw::math::vpu::Vector3* apPosition)
{
    mpEmitterPosition = apPosition;
}

void Cgs3dEffectControl::AttachEmitterDirection(
    const rw::math::vpu::Vector3* apDirection)
{
    mpEmitterDirection = apDirection;
}

void Cgs3dEffectControl::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)
{
    CGS_ASSERT(apMessageHeader != 0, "lpMessageHeader");
    if (apMessageHeader)
    {
        const f32* lpfPayload = reinterpret_cast<const f32*>(apMessageHeader + 1);
        mDebugRenderingMessageData.mfYOffset = lpfPayload[0];
        mDebugRenderingMessageData.mfRadius = lpfPayload[1];
        mDebugRenderingMessageData.mbEnable = lpfPayload[2] != 0.0f;
    }
}

bool Cgs3dEffectControl::Detach()
{
    EffectBase::Detach();
    if (GetDMixIOPtr())
    {
        SetMixerInputValueUnbound(3, 0);
        SetMixerInputValueUnbound(1, -1);
        SetMixerInputValueUnbound(2, 0);
        SetMixerInputValueUnbound(0, -1);
        SetMixerInputValueUnbound(
            15, GetDMixIOPtr()->GetDMixInput(15) & ~1);
    }
    return true;
}

void Cgs3dEffectControl::UpdateParams(f32 afDeltaTime)
{
    if (!mpEmitterPosition)
    {
        if (GetDMixIOPtr())
        {
            SetMixerInputValueUnbound(3, 0);
            SetMixerInputValueUnbound(1, -1);
            SetMixerInputValueUnbound(2, 0);
            SetMixerInputValueUnbound(0, -1);
            SetMixerInputValueUnbound(
                15, GetDMixIOPtr()->GetDMixInput(15) & ~1);
        }
        return;
    }

    mEmitterPosition.Update(*mpEmitterPosition);
    if (mpEmitterDirection)
        mEmitterDirection.Update(*mpEmitterDirection);
    if (GetDMixIOPtr())
        SetMixerInputValueUnbound(15, GetDMixIOPtr()->GetDMixInput(15) | 1);
    Generate3DParams(0);
    UpdateDoppler(afDeltaTime, 0);

    // ARTIST 0x826FA9D4..0x826FA9FC: draw only after the emitter/Doppler update, and only
    // when the original DebugRendererMessage enable byte (+0xA8) and callback slot are set.
    if (mDebugRenderingMessageData.mbEnable && Cgs3dEffectControl::spDebugRenderingFunction)
        Cgs3dEffectControl::spDebugRenderingFunction(this);
}

s32 Cgs3dEffectControl::GetPanningAngle(
    MicrophoneSystem& arMicSystem, MicrophoneSystem::EMicPositions aePosition)
{
    // ARTIST 0x826DBEA0: the permute selects x,z,x,x; the two VMX
    // normalisations have one and two Newton refinements respectively.
    __declspec(align(16)) static const u8 kauPlanar[16] =
        {0,1,2,3,24,25,26,27,0,1,2,3,0,1,2,3};
    const __vector4 lvPermute = __lvx(kauPlanar, 0);
    const __vector4 lvEmitter = __lvx(mpEmitterPosition, 0);
    const rw::math::vpu::Vector3 lPosition =
        arMicSystem.GetMicrophone(aePosition, MicrophoneSystem::E_PLAYER_1)->GetCurPosition();
    const __vector4 lvPosition = __lvx(&lPosition, 0);
    const __vector4 lvDelta = __vsubfp(__vperm(lvEmitter, lvEmitter, lvPermute),
                                       __vperm(lvPosition, lvPosition, lvPermute));
    const __vector4 lvDeltaSquared = __vmulfp(lvDelta, lvDelta);
    const __vector4 lvDistanceSquared = __vaddfp(__vspltw(lvDeltaSquared, 0),
                                                  __vspltw(lvDeltaSquared, 1));
    const __vector4 lvInRange = __vcmpgefp(lvDistanceSquared, XMVectorReplicate(0.01f));
    const __vector4 lvNotInRange = __vnor(lvInRange, lvInRange);
    unsigned int luComparison;
    __vcmpeqfpR(lvNotInRange, __vspltisw(0), &luComparison);
    if (!XMComparisonAllTrue(luComparison))
        return 0;

    const __vector4 lvOne = XMVectorReplicate(1.0f);
    const __vector4 lvHalf = XMVectorReplicate(0.5f);
    __vector4 lvEstimate = __vrsqrtefp(lvDistanceSquared);
    __vector4 lvSquared = __vmulfp(lvEstimate, lvEstimate);
    __vector4 lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
    __vector4 lvError = __vnmsubfp(lvDistanceSquared, lvSquared, lvOne);
    lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
    const __vector4 lvNormalDelta = __vmulfp(lvDelta, lvEstimate);

    const rw::math::vpu::Vector3 lDirection =
        arMicSystem.GetMicrophone(aePosition, MicrophoneSystem::E_PLAYER_1)->GetDirection();
    const __vector4 lvRawDirection = __lvx(&lDirection, 0);
    const __vector4 lvDirection = __vperm(lvRawDirection, lvRawDirection, lvPermute);
    const __vector4 lvDirectionSquared = __vmulfp(lvDirection, lvDirection);
    const __vector4 lvDirectionLengthSquared = __vaddfp(__vspltw(lvDirectionSquared, 0),
                                                         __vspltw(lvDirectionSquared, 1));
    lvEstimate = __vrsqrtefp(lvDirectionLengthSquared);
    lvSquared = __vmulfp(lvEstimate, lvEstimate);
    lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
    lvError = __vnmsubfp(lvDirectionLengthSquared, lvSquared, lvOne);
    lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
    lvSquared = __vmulfp(lvEstimate, lvEstimate);
    lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
    lvError = __vnmsubfp(lvDirectionLengthSquared, lvSquared, lvOne);
    lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
    const __vector4 lvNormalDirection = __vmulfp(lvDirection, lvEstimate);

    // The binary samples direction a second time for the dot-product path.
    const rw::math::vpu::Vector3 lDotDirection =
        arMicSystem.GetMicrophone(aePosition, MicrophoneSystem::E_PLAYER_1)->GetDirection();
    const __vector4 lvDotRaw = __lvx(&lDotDirection, 0);
    const __vector4 lvDotDirection = __vperm(lvDotRaw, lvDotRaw, lvPermute);
    const __vector4 lvDotSquared = __vmulfp(lvDotDirection, lvDotDirection);
    const __vector4 lvDotLengthSquared = __vaddfp(__vspltw(lvDotSquared, 0),
                                                   __vspltw(lvDotSquared, 1));
    lvEstimate = __vrsqrtefp(lvDotLengthSquared);
    lvSquared = __vmulfp(lvEstimate, lvEstimate);
    lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
    lvError = __vnmsubfp(lvDotLengthSquared, lvSquared, lvOne);
    lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
    lvSquared = __vmulfp(lvEstimate, lvEstimate);
    lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
    lvError = __vnmsubfp(lvDotLengthSquared, lvSquared, lvOne);
    lvEstimate = __vmaddfp(lvHalfEstimate, lvError, lvEstimate);
    const __vector4 lvDotProducts = __vmulfp(__vmulfp(lvDotDirection, lvEstimate), lvNormalDelta);
    const __vector4 lvDot = __vaddfp(__vspltw(lvDotProducts, 0), __vspltw(lvDotProducts, 1));
    const __vector4 lvAngle = __vmulfp(::XMVectorACos(lvDot), XMVectorReplicate(10430.21875f));
    const __vector4 lvCross = __vsubfp(
        __vmulfp(__vspltw(lvNormalDelta, 0), __vspltw(lvNormalDirection, 1)),
        __vmulfp(__vspltw(lvNormalDelta, 1), __vspltw(lvNormalDirection, 0)));
    const __vector4 lvResult = __vsel(lvAngle, __vsubfp(XMVectorReplicate(65535.0f), lvAngle),
                                       __vcmpgtfp(lvCross, __vspltisw(0)));
    return static_cast<s32>(lvResult.x);
}

void Cgs3dEffectControl::UpdateDoppler(f32 afDeltaTime, s32 aiPlayer)
{
    if (!mpEmitterPosition)
        return;

    MicrophoneSystem& lrMicSystem =
        GetLogicModule()->GetEnvironment().GetMicrophoneSystem();
    // ARTIST @ 0x826A4E90: both samples use the requested player microphone.
    // The histories are updated in reverse order: history 1 feeds mixer slot 13.
    for (s32 liMic = 1; liMic >= 0; --liMic)
    {
        const rw::math::vpu::Vector3& lrMicPos =
            lrMicSystem.GetMicrophone(
                MicrophoneSystem::E_MIC_PLAYER,
                static_cast<MicrophoneSystem::EPlayer>(aiPlayer))
                ->GetMicrophoneMatrix().wAxis;
        const f32 lfDistance = rw::math::vpu::Length(*mpEmitterPosition - lrMicPos);
        const f32 lfPrevious = mfDistanceToMic[liMic].GetCurrent();
        mfDistanceToMic[liMic].Update(lfDistance);
        const f32 lfVelocity = (lfDistance - lfPrevious) /
            (afDeltaTime == 0.0f ? 1.0f : afDeltaTime);
        mfVelocityToMic[liMic].Update(lfVelocity);
    }
    if (!GetDMixIOPtr())
        return;
    for (s32 liMic = 1; liMic >= 0; --liMic)
    {
        const f32 lfVelocity = mfVelocityToMic[liMic].GetCurrent();
        const f32 lfPreviousVelocity = mfVelocityToMic[liMic].GetPrevious();
        if ((lfVelocity < 0.0f && lfPreviousVelocity > 0.0f) ||
            (lfVelocity > 0.0f && lfPreviousVelocity < 0.0f))
        {
            const s32 liFlag = liMic == 1 ? static_cast<s32>(0x80000000u)
                                          : 0x40000000;
            SetMixerInputValueUnbound(
                15, GetDMixIOPtr()->GetDMixInput(15) | liFlag);
        }
        SetMixerInputValueUnbound(14 - liMic,
                                  static_cast<s32>(lfVelocity * 100.0f));
    }
}

// ---------------------------------------------------------------------------
// Cgs3dEffectControl::SetMixerInputValueUnbound  @ 0x82682570
//
// Store a raw value directly into one dynamic-mixer INPUT slot, bypassing the
// bound-value path. mpDynamicMixIo (inherited EffectBase +0x30) is the Nicotine
// DMixIO handle; its input block (GetDMixInputPtr() == m_pDMixInputBlock @+0x08) is
// indexed by slot (int*, so 4*aiSlot bytes from the base). Both the handle and its
// input block are asserted non-null before the store (assert strings are the source
// local-variable spellings: "lpNicotineData").
// ---------------------------------------------------------------------------
void Cgs3dEffectControl::SetMixerInputValueUnbound(s32 aiSlot, s32 aiValue)
{
    Nicotine::DMixIO* lpNicotineData = GetDMixIOPtr();
    CGS_ASSERT(lpNicotineData, "lpNicotineData");
    CGS_ASSERT(lpNicotineData->GetDMixInputPtr(), "lpNicotineData->GetDMixInputPtr()");

    lpNicotineData->GetDMixInputPtr()[aiSlot] = aiValue;
}

// ---------------------------------------------------------------------------
// Cgs3dEffectControl::Generate3DParams  @ 0x826EA7D8
//
// Recompute the mixer 3D input slots from the sound-logic microphone system.
// The int32_t param is present in the DWARF signature (Generate3DParams(int32_t))
// but the X360 body never reads r4 -- it operates entirely off `this`.
//
// Single-player path (miNumPlayers == 1): for each of the two mic slots, take the
// listener position out of the MicrophoneSystem, form (micPos - emitterPos), and
// push a panning angle (GetPanningAngle) and the emitter->mic DISTANCE (scaled by
// 100 and truncated to int) into the dynamic-mixer input slots:
//   slot 3 = pan angle  (mic 0 / E_MIC_CAMERA)
//   slot 1 = distance   (mic 0 : micSys+0x40  == current-matrix wAxis of cam mic)
//   slot 2 = pan angle  (mic 1 / E_MIC_PLAYER)
//   slot 0 = distance   (mic 1 : micSys+0x180 == current-matrix wAxis of player mic)
//
// The X360 SIMD block is the SDK normalize/magnitude idiom used as a length:
//   v0 = |delta|^2 (vmsum3fp); refined 1/sqrt (vrsqrtefp + Newton); |delta| =
//   |delta|^2 * (1/|delta|); vsel-against-0 for the degenerate case; then * 100.
// De-optimised here to Length()*100 (the corpus convention for the rsqrt idiom).
//
// FLAG (medium): the MicrophoneSystem listener-position fields at byte offsets
// +0x40 and +0x180 are read raw off the microphone-system sub-object (they are the
// per-mic current-matrix wAxis/translation per committed CgsMicrophone.h). GetPanningAngle
// is DEFERRED (sibling TU); only its call shape (this, MicrophoneSystem&, EMicPositions)
// is used here. FLAG: the two-player branch is an unconditional assert tripwire (not
// implemented in the shipped build).
// ---------------------------------------------------------------------------
void Cgs3dEffectControl::Generate3DParams(s32 /*aiUnused*/)
{
    CGS_ASSERT(mpEmitterPosition, "mpEmitterPosition");

    MicrophoneSystem& lrMicSystem =
        GetLogicModule()->GetEnvironment().GetMicrophoneSystem();

    if (lrMicSystem.GetNumberOfPlayers() == 1)
    {
        if (GetDMixIOPtr())
        {
            const rw::math::vpu::Vector3& lrEmitterPos = *mpEmitterPosition;

            // slot 3: panning angle to mic 0 (E_MIC_CAMERA).
            s32 liAngle0 = GetPanningAngle(lrMicSystem, MicrophoneSystem::E_MIC_CAMERA);
            SetMixerInputValueUnbound(3, liAngle0);

            // slot 1: distance emitter -> mic 0 listener position (micSys+0x40), *100.
            const rw::math::vpu::Vector3& lrMic0Pos =
                lrMicSystem.GetMicrophone(MicrophoneSystem::E_MIC_CAMERA,
                                          MicrophoneSystem::E_PLAYER_1)
                    ->GetMicrophoneMatrix().wAxis;
            f32 lfDist0 = rw::math::vpu::Length(lrMic0Pos - lrEmitterPos) * 100.0f;
            SetMixerInputValueUnbound(1, static_cast<s32>(lfDist0));

            // slot 2: panning angle to mic 1 (E_MIC_PLAYER).
            s32 liAngle1 = GetPanningAngle(lrMicSystem, MicrophoneSystem::E_MIC_PLAYER);
            SetMixerInputValueUnbound(2, liAngle1);

            // slot 0: distance emitter -> mic 1 listener position (micSys+0x180), *100.
            const rw::math::vpu::Vector3& lrMic1Pos =
                lrMicSystem.GetMicrophone(MicrophoneSystem::E_MIC_PLAYER,
                                          MicrophoneSystem::E_PLAYER_1)
                    ->GetMicrophoneMatrix().wAxis;
            f32 lfDist1 = rw::math::vpu::Length(lrMic1Pos - lrEmitterPos) * 100.0f;
            SetMixerInputValueUnbound(0, static_cast<s32>(lfDist1));
        }
    }
    else if (lrMicSystem.GetNumberOfPlayers() != 0)
    {
        CGS_ASSERT(false, "Two Player game not supported yet. Needs some work.");
    }
}

// ---------------------------------------------------------------------------
// Cgs3dEffectControl::~Cgs3dEffectControl  @ 0x826C3E70  (scalar deleting destructor)
//
// MSVC's scalar-deleting-destructor for Cgs3dEffectControl. Decoded from the asm:
//   stw  off_820AA820, 0(this)   ; re-install the MemBase base-subobject vtable
//   stb  0, 0x2D(this)           ; mbHasLoadedData = false
//   stw  3, 0x24(this)           ; meDetachState  = E_DETACH_STATE_FINISHED
//   stw  0, 0x20(this)           ; meAttachState  = E_ATTACH_STATE_NONE
//   if (flags & 1)               ; deleting flavour -> allocator Free via off_82FFB954
//
// The three explicit member stores are the source-level teardown, reproduced by
// name through EffectBase::ResetOnDestroy(). Cgs3dEffectControl's own members are
// trivially destructible PODs (DataPoint<f32>/DataPoint<Vector3>/pointers/POD
// sub-struct), so the asm emits NO member-dtor calls -- consistent with a body that
// is just the EffectBase reset. The store sequence is byte-identical to the
// committed EffectControl/EffectObject deleting-destructor thunks
// (0x826963D8 / 0x826965D0). The MemBase vptr re-install (off_820AA820) and the
// conditional allocator-routed free (off_82FFB954) are the compiler-synthesised
// parts of the thunk; the host toolchain's `delete` stands in for the custom
// allocator dispatch.
// ---------------------------------------------------------------------------
Cgs3dEffectControl::~Cgs3dEffectControl()
{
    ResetOnDestroy();
}

} // namespace Logic
} // namespace CgsSound
