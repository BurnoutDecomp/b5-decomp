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

namespace CgsSound
{
namespace Logic
{

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

    // GetLogicModule() (EffectBase +0x28) + 0x29A0 == CgsSound::Logic::Module::mMicrophoneSystem.
    u8* lpModule = reinterpret_cast<u8*>(GetLogicModule());
    MicrophoneSystem& lrMicSystem =
        *reinterpret_cast<MicrophoneSystem*>(lpModule + 0x29A0);

    const s32* lpiNumPlayers = reinterpret_cast<const s32*>(&lrMicSystem);

    if (*lpiNumPlayers == 1)
    {
        if (GetDMixIOPtr())
        {
            const u8* lpMicSys = reinterpret_cast<const u8*>(&lrMicSystem);
            const rw::math::vpu::Vector3& lrEmitterPos = *mpEmitterPosition;

            // slot 3: panning angle to mic 0 (E_MIC_CAMERA).
            s32 liAngle0 = GetPanningAngle(lrMicSystem, MicrophoneSystem::E_MIC_CAMERA);
            SetMixerInputValueUnbound(3, liAngle0);

            // slot 1: distance emitter -> mic 0 listener position (micSys+0x40), *100.
            const rw::math::vpu::Vector3& lrMic0Pos =
                *reinterpret_cast<const rw::math::vpu::Vector3*>(lpMicSys + 0x40);
            f32 lfDist0 = rw::math::vpu::Length(lrMic0Pos - lrEmitterPos) * 100.0f;
            SetMixerInputValueUnbound(1, static_cast<s32>(lfDist0));

            // slot 2: panning angle to mic 1 (E_MIC_PLAYER).
            s32 liAngle1 = GetPanningAngle(lrMicSystem, MicrophoneSystem::E_MIC_PLAYER);
            SetMixerInputValueUnbound(2, liAngle1);

            // slot 0: distance emitter -> mic 1 listener position (micSys+0x180), *100.
            const rw::math::vpu::Vector3& lrMic1Pos =
                *reinterpret_cast<const rw::math::vpu::Vector3*>(lpMicSys + 0x180);
            f32 lfDist1 = rw::math::vpu::Length(lrMic1Pos - lrEmitterPos) * 100.0f;
            SetMixerInputValueUnbound(0, static_cast<s32>(lfDist1));
        }
    }
    else if (*lpiNumPlayers != 0)
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
