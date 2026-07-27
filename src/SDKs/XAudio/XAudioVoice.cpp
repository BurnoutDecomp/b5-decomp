#include "SDKs/XAudio/XAudioVoice.h"
#include "SDKs/XAudio/XAudioEngine.h"            // gpEngine, CEngine::mpEffectManager
#include "SDKs/XAudio/XAudioXMemMemoryManager.h" // gXMemMemoryManager, XMemFree

// ===========================================================================
// XAUDIO::CVoice -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XAudioVoice.h for the layout and asm notes.
//
// W40 FINISH: with XAUDIO::CEngine homed (the engine singleton gpEngine and its
// effect manager) and the effect/allocator foreign interfaces modelled in the
// header, this wave lands the effect- and allocator-driven bodies on top of the
// earlier accessor set: the constructor + destructor, CreateEffect,
// Get/SetEffectParam, ProcessEffects, and the class `operator delete` (the free
// arm of the scalar deleting destructor).
//
// WAVE D UNBLOCK: the CVoice vtable slot order is now RECOVERED from the rodata
// (off_821B5770; see the header banner + scratchpad/waveD/CVoice.spec.md), the
// module recursive spin-lock is the committed gXAudioModuleLock
// (XAudioEngineVoiceList.h), the per-hw-thread index accessor is declared there
// (KeGetCurrentProcessorNumber), the gpEngine sub-table at +0x0C is the
// per-thread frame-buffer pair array (CEngine::mpThreadFrameBuffers), and the
// init-descriptor / effect-table shapes are homed in the header (SVoiceInit /
// SVoiceEffectTable). The formerly-blocked bodies (Start/Stop/OnStartVoice/
// OnStopVoice/Process/ProcessEffect/Initialize/GetObjectAdditionalSize) are
// implemented in the wave-D partfiles (XAudioVoice_part_*.cpp).
// ===========================================================================

namespace XAUDIO
{

// ---------------------------------------------------------------------------
// XAudio C-API effect helpers (separate todo TUs; NOT part of this ledger set).
// Signatures are grounded in the CVoice call-site asm (register-for-register
// argument shapes). Bodies are supplied by their own TUs / the eventual link.
//   CreateEffect asm: XAudioEffectManager_CreateEffect(mgr, desc, allocator, &out).
//   ~CVoice asm     : XAudioEffectManager_Release(frameBuffer)  [IDA-labelled;
//                     r3 = mpFrameBuffer at the call, one pointer argument].
// ---------------------------------------------------------------------------
s32 XAudioEffectManager_CreateEffect(CEffectManager* apManager, const void* apDesc,
                                     void* apAllocator, void** appEffect);
s32 XAudioEffectManager_Release(void* apObject);

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
//   copies the two format words at +0x34 / +0x38 out, returns 0. (The two word
//   moves are expressed as the struct copy they implement.)
s32 CVoice::GetVoiceFormat(SVoiceFormat* apFormat)
{
    *apFormat = mFormat;
    return 0;
}

// @ 0x82C32178:
//   stores the two format words at +0x34 / +0x38. (r3 is left holding `this`;
//   the source-level setter returns void.)
void CVoice::SetVoiceFormat(const SVoiceFormat* apFormat)
{
    mFormat = *apFormat;
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

// The struct the effect's GetSize slot (effect vtable[2]) fills: a leading flag
// byte the record stores at +0x06 and a running-max frame size half at +0x02.
// Grounded in the CreateEffect asm (`lbz var_30` / `lhz var_2E`).
struct SEffectSizeInfo
{
    u8  muFlags;     // +0x00
    u8  mPad01;      // +0x01
    u16 muFrameSize; // +0x02
};

// @ 0x82C324D8:
//   store the allocator (+0x08), refcount 1 (+0x04), AddRef the allocator through
//   its vtable[0], store the voice type (+0x0C), self-link the two intrusive list
//   heads (+0x10 / +0x18) and clear the reserved word (+0x28). The two vtable seats
//   the asm performs (off_821B5514 base image, then off_821B5770) are the base +
//   this-class construction the compiler emits.
CVoice::CVoice(void* apAllocator, u8 auVoiceType)
{
    mpAllocator = apAllocator;
    miRefCount = 1;
    if (apAllocator)
    {
        IXAudioAllocator* pAllocator = static_cast<IXAudioAllocator*>(apAllocator);
        pAllocator->mpVTable->mpAddRef(pAllocator);
    }
    muVoiceType = auVoiceType;

    // Both list links are circular sentinels linked to themselves.
    mListA.mpNext = &mListA;
    mListA.mpPrev = &mListA;
    mListB.mpNext = &mListB;
    mListB.mpPrev = &mListB;

    mpReserved28 = nullptr;
}

// @ 0x82C32278:
//   release every live effect through its vtable[1] (clearing the slot), release
//   the frame buffer, then release the allocator through its vtable[1]. The three
//   vtable reseats (off_821B5770 -> off_821B5514 -> off_82108FF4) are the compiler-
//   emitted this-class -> base -> shared-image teardown.
CVoice::~CVoice()
{
    if (muNumEffects)
    {
        for (u8 i = 0; i < muNumEffects; ++i)
        {
            IXAudioEffect* pEffect = static_cast<IXAudioEffect*>(mpEffects[i].mpEffect);
            if (pEffect)
            {
                pEffect->mpVTable->mpRelease(pEffect);
                mpEffects[i].mpEffect = nullptr;
            }
        }
    }

    if (mpFrameBuffer)
    {
        XAudioEffectManager_Release(mpFrameBuffer);
        mpFrameBuffer = nullptr;
    }

    if (mpAllocator)
    {
        IXAudioAllocator* pAllocator = static_cast<IXAudioAllocator*>(mpAllocator);
        pAllocator->mpVTable->mpRelease(pAllocator);
        mpAllocator = nullptr;
    }
}

// @ 0x82C32340:
//   mgr = gpEngine->mpEffectManager; apEffect->mpEffect = 0;
//   result = XAudioEffectManager_CreateEffect(mgr, apDesc, mpAllocator, &apEffect->mpEffect);
//   on success query the effect size (effect vtable[2]) and seat the record's
//   flags/state, growing the running max frame size (+0x3E).
s32 CVoice::CreateEffect(SVoiceEffect* apEffect, const void* apDesc, u8 auFlag)
{
    CEffectManager* pManager = gpEngine->mpEffectManager;

    apEffect->mpEffect = nullptr;
    s32 result = XAudioEffectManager_CreateEffect(pManager, apDesc, mpAllocator,
                                                  &apEffect->mpEffect);
    if (result >= 0)
    {
        IXAudioEffect* pEffect = static_cast<IXAudioEffect*>(apEffect->mpEffect);
        SEffectSizeInfo sizeInfo;
        result = pEffect->mpVTable->mpGetSize(pEffect, &sizeInfo);
        if (result >= 0)
        {
            apEffect->muFlagA = auFlag;
            apEffect->muState = 1;
            apEffect->muFlagB = sizeInfo.muFlags;
            if (sizeInfo.muFrameSize > muMaxFrameSize)
                muMaxFrameSize = sizeInfo.muFrameSize;
        }
    }
    return result;
}

// @ 0x82C323D8:
//   if index out of range -> E_INVALIDARG; else dispatch effect vtable[3] GetParam.
s32 CVoice::GetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2)
{
    if (auIndex >= muNumEffects)
        return KI_XAUDIO_E_INVALIDARG;

    IXAudioEffect* pEffect = static_cast<IXAudioEffect*>(mpEffects[auIndex].mpEffect);
    return pEffect->mpVTable->mpGetParam(pEffect, aParam0, aParam1, aParam2);
}

// @ 0x82C32420:
//   if index out of range -> E_INVALIDARG; else dispatch effect vtable[4] SetParam.
s32 CVoice::SetEffectParam(u8 auIndex, s32 aParam0, s32 aParam1, s32 aParam2)
{
    if (auIndex >= muNumEffects)
        return KI_XAUDIO_E_INVALIDARG;

    IXAudioEffect* pEffect = static_cast<IXAudioEffect*>(mpEffects[auIndex].mpEffect);
    return pEffect->mpVTable->mpSetParam(pEffect, aParam0, aParam1, aParam2);
}

// @ 0x82C32E60:
//   run ProcessEffect over each effect in order, stopping on the first negative
//   HRESULT (the loop tests the running result before each dispatch).
s32 CVoice::ProcessEffects(CFrameBuffer** apInOut)
{
    s32 result = 0;
    if (muNumEffects)
    {
        u8 uIndex = 0;
        do
        {
            if (result < 0)
                break;
            result = ProcessEffect(&mpEffects[uIndex], apInOut);
            ++uIndex;
        } while (uIndex < muNumEffects);
    }
    return result;
}

// @ 0x82C32578 (free arm): route the class-specific free through the XAudio XMem
// manager (`XMemFree(&gXMemMemoryManager, this, 0x61820000)`). The deleting-
// destructor thunk itself (call ~CVoice, then this) is compiler-generated.
void CVoice::operator delete(void* apMemory)
{
    gXMemMemoryManager.XMemFree(apMemory, KU_XMEM_FREE_ATTRIBUTES);
}

} // namespace XAUDIO
