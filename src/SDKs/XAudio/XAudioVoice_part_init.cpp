// ===========================================================================
// XAUDIO::CVoice -- the initialisation/sizing pair of the CVoice TU.
// Canonical home: SDKs/XAudio/XAudioVoice.h (this partfile holds two of the
// class's bodies; the remainder live in XAudioVoice.cpp and its siblings).
//
//     XAUDIO::CVoice::Initialize               @ 0x82C32D28
//     XAUDIO::CVoice::GetObjectAdditionalSize  @ 0x82C32190
//
// Reconstructed store-for-store from the X360 ARTIST asm.
// ===========================================================================

#include "SDKs/XAudio/XAudioVoice.h"
#include "SDKs/XAudio/XAudioEngine.h"      // gpEngine, CEngine::mpEffectManager
#include "SDKs/XAudio/XAudioFrameBuffer.h" // CFrameBufferDesc

namespace XAUDIO
{

// XAudio C-API helpers reached by these two bodies. Each is its own (todo) TU;
// the signatures are call-site-attested from the asm. Declared file-scope here
// exactly as the committed XAudioVoice.cpp declares XAudioEffectManager_CreateEffect.
s32 XAudioCreateFrameBuffer(const CFrameBufferDesc* apDesc, void* apAllocator,
                            CFrameBuffer** appFrameBuffer);
s32 XAudioQueryFrameBufferSize(const CFrameBufferDesc* apDesc, u32* apSize);
s32 XAudioFrameBuffer_AddRef(CFrameBuffer* apFrameBuffer);
s32 XAudioEffectManager_QueryEffectSize(CEffectManager* apManager,
                                        const void* apEffectDesc, u32* apSize);

// 0x8007000E -- the XAudio out-of-memory HRESULT the allocation failure returns.
static const s32 KI_XAUDIO_E_OUTOFMEMORY = static_cast<s32>(0x8007000E);

// ---------------------------------------------------------------------------
// @ 0x82C32D28 -- seed the voice from its creation descriptor: copy the process
// callback (+0x2C), the user context (+0x30) and the two-word format (+0x34);
// take the effect count from the optional effect table. Adopt the descriptor's
// external frame buffer (AddRef'ing it) when one is supplied, otherwise create
// one at 48 kHz from a zeroed CFrameBufferDesc carrying the descriptor's channel
// count. Finally allocate the 8-byte-stride effect array through the allocator's
// vtable[5] and populate it via CreateUserEffects.
// ---------------------------------------------------------------------------
s32 CVoice::Initialize(const SVoiceInit* apInit)
{
    s32 liResult = 0;

    mpProcessCallback = apInit->mpfnProcessCallback;
    mpVoiceContext    = apInit->mpContext;
    mFormat           = apInit->mFormat;
    muNumEffects      = apInit->mpEffectTable ? apInit->mpEffectTable->muNumEffects
                                              : static_cast<u8>(0);

    if (apInit->mpFrameBuffer)
    {
        mpFrameBuffer = apInit->mpFrameBuffer;
        // The asm re-loads and re-tests the descriptor's frame-buffer word before
        // the AddRef (`lwz r3,0xC(r30); cmplwi; beq`) -- kept verbatim.
        if (apInit->mpFrameBuffer)
            XAudioFrameBuffer_AddRef(apInit->mpFrameBuffer);
    }
    else
    {
        CFrameBufferDesc lDesc = {};
        lDesc.muUseExternalBuffer  = 0;
        lDesc.mFormat.muByte0      = 0;
        lDesc.mFormat.muChannels   = apInit->mFormat.muChannels;
        lDesc.mFormat.muSampleRate = 48000;
        liResult = XAudioCreateFrameBuffer(&lDesc, mpAllocator, &mpFrameBuffer);
        if (liResult < 0)
            return liResult;
    }

    if (muNumEffects)
    {
        IXAudioAllocator* const lpAllocator =
            static_cast<IXAudioAllocator*>(mpAllocator);
        mpEffects = static_cast<SVoiceEffect*>(
            lpAllocator->mpVTable->mpAllocate(lpAllocator, 8u * muNumEffects));
        if (!mpEffects)
            return KI_XAUDIO_E_OUTOFMEMORY;
        liResult = 0;
    }
    // The count byte is re-read after the allocation arm (`lbz r11,0x3C(r31)`).
    if (!muNumEffects)
        return liResult;
    return CreateUserEffects(apInit->mpEffectTable);
}

// ---------------------------------------------------------------------------
// @ 0x82C32190 -- static: report the extra bytes a voice built from `apInit`
// needs. Without an external frame buffer that is the size XAudioQueryFrameBufferSize
// reports for the same 48 kHz descriptor Initialize builds; on top of it, each
// effect contributes its XAudioEffectManager_QueryEffectSize size plus one
// 8-byte effect record. Any query failure returns WITHOUT writing `*apSize`.
// ---------------------------------------------------------------------------
s32 CVoice::GetObjectAdditionalSize(const SVoiceInit* apInit, u32* apSize)
{
    u32 luTotal  = 0;
    u32 luCount  = 0;
    s32 liResult = 0;
    u32 luSize;

    if (!apInit->mpFrameBuffer)
    {
        CFrameBufferDesc lDesc = {};
        lDesc.muUseExternalBuffer  = 0;
        lDesc.mFormat.muByte0      = 0;
        lDesc.mFormat.muChannels   = apInit->mFormat.muChannels;
        lDesc.mFormat.muSampleRate = 48000;
        liResult = XAudioQueryFrameBufferSize(&lDesc, &luSize);
        if (liResult < 0)
            return liResult;
        luTotal = luSize;
    }

    const SVoiceEffectTable* lpTable = apInit->mpEffectTable;
    if (lpTable && lpTable->muNumEffects)
    {
        do
        {
            liResult = XAudioEffectManager_QueryEffectSize(
                gpEngine->mpEffectManager, lpTable->mpEffectDescs[luCount], &luSize);
            if (liResult < 0)
                return liResult;
            // The table pointer is re-read from the descriptor every iteration
            // (`lwz r11,8(r28)` inside the loop) -- kept.
            lpTable = apInit->mpEffectTable;
            ++luCount;
            luTotal += luSize;
        } while (luCount < lpTable->muNumEffects);
    }

    *apSize = 8 * luCount + luTotal;
    return liResult;
}

} // namespace XAUDIO
