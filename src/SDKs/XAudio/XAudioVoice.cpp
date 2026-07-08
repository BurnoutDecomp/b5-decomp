#include "SDKs/XAudio/XAudioVoice.h"

// ===========================================================================
// XAUDIO::CVoice -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioVoice.h for the layout and asm notes.
//
// This wave lands the self-contained accessor set. The remaining CVoice methods
// (ctor/dtor, Initialize, CreateEffect, GetObjectAdditionalSize, Get/SetEffectParam,
// Process, ProcessEffect(s), OnStart/OnStopVoice, Start, Stop, scalar deleting
// destructor) depend on un-homed collaborators (the XAudio allocator interface,
// XAUDIO::CEngine, the XAudioEffectManager_* / XAudioFrameBuffer_* helpers, the
// per-effect foreign vtable, and the Xbox kernel spinlock/IRQL primitives) and are
// left for later waves.
// ===========================================================================

namespace XAUDIO
{

// HRESULT E_INVALIDARG, baked into the asm as lis -0x7FF9 / ori 0x57.
static const s32 KI_XAUDIO_E_INVALIDARG = static_cast<s32>(0x80070057);

// @ 0x82C2FC78:
//   lbz r11, 0xC(r3)  ; r11 = muVoiceType
//   li  r3, 0
//   stb r11, 0(r4)    ; *apType = muVoiceType
s32 CVoice::GetVoiceType(u8* apType)
{
    *apType = muVoiceType;
    return 0;
}

// @ 0x82C2FC68:
//   lwz r11, 0x30(r3) ; r11 = mpVoiceContext
//   li  r3, 0
//   stw r11, 0(r4)    ; *apContext = mpVoiceContext
s32 CVoice::GetVoiceContext(void** apContext)
{
    *apContext = mpVoiceContext;
    return 0;
}

// @ 0x82C2FC88:
//   copies the two format words at +0x34 / +0x38 out, returns 0.
s32 CVoice::GetVoiceFormat(SVoiceFormat* apFormat)
{
    apFormat->muField0 = mFormat.muField0;
    apFormat->muField1 = mFormat.muField1;
    return 0;
}

// @ 0x82C32178:
//   stores the two format words at +0x34 / +0x38. (r3 is left holding `this`;
//   the source-level setter returns void.)
void CVoice::SetVoiceFormat(const SVoiceFormat* apFormat)
{
    mFormat.muField0 = apFormat->muField0;
    mFormat.muField1 = apFormat->muField1;
}

// @ 0x82C30828:
//   lbz r11, 0x3D(r3) ; r11 = muState
//   li  r3, 0
//   stb r11, 0(r4)    ; *apState = muState
s32 CVoice::GetVoiceState(u8* apState)
{
    *apState = muState;
    return 0;
}

// @ 0x82C32468:
//   if aIndex >= muNumEffects -> E_INVALIDARG
//   *apState = mpEffects[aIndex].muState ; return 0
s32 CVoice::GetEffectState(u8 aIndex, u8* apState)
{
    if (aIndex >= muNumEffects)
        return KI_XAUDIO_E_INVALIDARG;

    *apState = mpEffects[aIndex].muState;
    return 0;
}

// @ 0x82C324A0:
//   if aIndex >= muNumEffects -> E_INVALIDARG
//   mpEffects[aIndex].muState = aState ; return 0
s32 CVoice::SetEffectState(u8 aIndex, u8 aState)
{
    if (aIndex >= muNumEffects)
        return KI_XAUDIO_E_INVALIDARG;

    mpEffects[aIndex].muState = aState;
    return 0;
}

} // namespace XAUDIO
