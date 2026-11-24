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

    mpReserved28 = 0;
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
                mpEffects[i].mpEffect = 0;
            }
        }
    }

    if (mpFrameBuffer)
    {
        XAudioEffectManager_Release(mpFrameBuffer);
        mpFrameBuffer = 0;
    }

    if (mpAllocator)
    {
        IXAudioAllocator* pAllocator = static_cast<IXAudioAllocator*>(mpAllocator);
        pAllocator->mpVTable->mpRelease(pAllocator);
        mpAllocator = 0;
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

    apEffect->mpEffect = 0;
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

// FOLDED FROM XAudioVoice_part_init.cpp
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

// FOLDED FROM XAudioVoice_part_lists.cpp
#include "SDKs/XAudio/XAudioVoice.h"
#include "SDKs/XAudio/XAudioEngine.h"          // gpEngine, SEngineVoiceList
#include "SDKs/XAudio/XAudioEngineVoiceList.h" // gXAudioModuleLock + Ke* primitives

// ===========================================================================
// XAUDIO::CVoice -- the engine-voice-list arm of the class (wave D, group 1).
// Bodies reconstructed from BURNOUT_X360_ARTIST.XEX; layout/vtable notes live
// in XAudioVoice.h, the recovery spec in scratchpad/waveD/CVoice.spec.md.
//
//     XAUDIO::CVoice::OnStartVoice  @ 0x82C30FE8   (vtable slot 19, +0x4C)
//     XAUDIO::CVoice::OnStopVoice   @ 0x82C30E70   (vtable slot 20, +0x50)
//     XAUDIO::CVoice::Start         @ 0x82C32770   (vtable slot 14, +0x38)
//
// These are a partfile of the XAUDIO::CVoice TU (sibling bodies land in
// XAudioVoice.cpp / the other wave-D partfiles); it compiles standalone.
//
// THE RECURSIVE-LOCK IDIOM. All three bodies are wrapped in the XAudio module
// recursive spin-lock (rodata @0x83222C28 = gXAudioModuleLock), which the X360
// compiler inlined at every acquire/release site. The shape is identical to the
// committed XAUDIO::SEngineVoiceList::SwapActiveLists (XAudioEngineVoiceList.cpp
// lines 43-78) and is mirrored here per site:
//   acquire: raise to DPC IRQL (remembering the old one); if the lock is unheld
//            (miRecursion == 0) OR a different thread owns it, take the
//            raised-IRQL spin-lock and record owner + saved IRQL; ++miRecursion.
//   release: if we still own it, --miRecursion; on the last release clear the
//            saved IRQL + owner, drop the spin-lock and lower the IRQL back.
// De-gotoed to structured C++; every store, branch and call has a counterpart
// and no behaviour is added.
//
// ICF-FOLDED MARKER CALLS. Start's asm brackets its body with
// `li r3,1 ; bl BrnWorld::PVSDebugComponent::IsSimple` at entry and exit and
// discards the result. That symbol is the identity-folded leaf `li r3,0 ; blr`
// (@0x827E2F38), i.e. a stripped profiling/scope marker the linker collapsed
// onto an unrelated `return 0` body -- not a real call to the PVS debug
// component. It is dropped here, matching the established precedent in
// GameSource/Network/Managers/BrnNetworkLaunchManager.cpp:217.
// ===========================================================================

namespace XAUDIO
{

// ---------------------------------------------------------------------------
// [slot 19] @ 0x82C30FE8 -- OnStartVoice: resolve this voice's engine voice list
// and, under the module lock, tail-insert its intrusive link into the list's
// BACK active sub-list (mpActiveB).
//
// The link node is NOT addressed as `&mListB` here: the asm reads the sub-list
// head's mpBase word and ADDS it to `this` (`lwz r10,0x28(r27)` /
// `lwz r11,8(r10)` / `add r11,r11,r28`), i.e. the head carries the byte offset
// of the link within the voice (24 == mListB) so one head can service any node
// layout. That is the same base+offset scheme as the committed
// XAUDIOPACKETCTX_::GetNextEntry, with the roles reversed (base = the voice,
// offset = the head's mpBase word), and it is reproduced faithfully.
// ---------------------------------------------------------------------------
s32 CVoice::OnStartVoice()
{
    SEngineVoiceList* const lpList = gpEngine->GetVoiceList(this);
    if (lpList) // cmplwi cr6,r27,0 ; beq -> straight to `li r3,0`
    {
        SXAudioRecursiveSpinLock* const lpLock = &gXAudioModuleLock;

        // --- acquire -------------------------------------------------------
        const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
        void* const lpThread  = KeGetCurrentThread(); // mr r30,r13 (KPCR token)
        s32 liRecursion = lpLock->miRecursion;
        if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
        {
            KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
            lpLock->mpOwnerThread = lpThread;
            lpLock->muSavedIrql   = luOldIrql;
            liRecursion           = lpLock->miRecursion;
        }
        lpLock->miRecursion = liRecursion + 1;

        // --- critical section: tail-insert before the sub-list head ---------
        XAUDIOPACKETCTX_* const lpHead = lpList->mpActiveB; // lwz 0x28(r27)
        // The asm forms the link address as `this + lpHead->mpBase`
        // (lwz r11,8(r10) / add r11,r11,r28), but mpBase is not data-dependent:
        // CEngine::CEngine @0x82C2EA28 seeds it with the LITERAL immediates
        // `li r6,0x18` -> mActiveB and `li r5,0x10` -> mActiveA, which are the
        // X360 32-bit offsetof(CVoice, mListB) and offsetof(CVoice, mListA).
        // Those literals do not survive the host's widened layout (8-byte vptr
        // and pointers), so reproducing the addition would land mid-object and
        // corrupt mListA/mpFrameBuffer. Address the member by name, as the
        // sibling OnStopVoice already does for this same link.
        XAudioListLink* const lpLink = &mListB;
        lpLink->mpNext = reinterpret_cast<XAudioListLink*>(lpHead); // stw r10,0(r11)
        lpLink->mpPrev = lpHead->mpPrev;                            // lwz 4(r10) ; stw 4(r11)
        lpHead->mpPrev = lpLink;                                    // stw r11,4(r10)
        lpLink->mpPrev->mpNext = lpLink;                            // lwz 4(r11) ; stw r11,0(r10)

        // --- release -------------------------------------------------------
        const s32 liDepth = lpLock->miRecursion;
        if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
        {
            lpLock->miRecursion = liDepth - 1;
            if (liDepth == 1)
            {
                const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                lpLock->muSavedIrql   = 0;
                lpLock->mpOwnerThread = 0;
                KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                KfLowerIrql(luRestoreIrql);
            }
        }
    }
    return 0; // li r3,0
}

// ---------------------------------------------------------------------------
// [slot 20] @ 0x82C30E70 -- OnStopVoice: under the module lock, unlink mListB
// from whatever list holds it (skipped when it is already self-circular, i.e.
// not linked) and re-seat both words back to itself.
// ---------------------------------------------------------------------------
s32 CVoice::OnStopVoice()
{
    SXAudioRecursiveSpinLock* const lpLock = &gXAudioModuleLock;

    // --- acquire -----------------------------------------------------------
    const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
    void* const lpThread  = KeGetCurrentThread(); // mr r30,r13 (KPCR token)
    s32 liRecursion = lpLock->miRecursion;
    if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
    {
        KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
        lpLock->mpOwnerThread = lpThread;
        lpLock->muSavedIrql   = luOldIrql;
        liRecursion           = lpLock->miRecursion;
    }
    lpLock->miRecursion = liRecursion + 1;

    // --- critical section: unlink + self-seat ------------------------------
    // r11 = this + 0x18 (&mListB); r9 = mListB.mpNext.
    if (mListB.mpNext != &mListB) // cmplw cr6,r9,r11 ; beq -> skip
    {
        mListB.mpNext->mpPrev = mListB.mpPrev; // lwz 4(r11) ; stw 4(r9)
        mListB.mpPrev->mpNext = mListB.mpNext; // lwz 4(r11) ; lwz 0(r11) ; stw 0(r10)
        mListB.mpPrev = &mListB;               // stw r11,4(r11)
        mListB.mpNext = &mListB;               // stw r11,0(r11)
    }

    // --- release -----------------------------------------------------------
    const s32 liDepth = lpLock->miRecursion;
    if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
    {
        lpLock->miRecursion = liDepth - 1;
        if (liDepth == 1)
        {
            const KIRQL luRestoreIrql = lpLock->muSavedIrql;
            lpLock->muSavedIrql   = 0;
            lpLock->mpOwnerThread = 0;
            KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
            KfLowerIrql(luRestoreIrql);
        }
    }
    return 0; // li r3,0
}

// ---------------------------------------------------------------------------
// [slot 14] @ 0x82C32770 -- Start: an OUTER lock bracket around the state test,
// with a SHORT INNER bracket around the state-byte write in each arm (the X360
// compiler inlined the recursive lock at all four sites; the recursion counter
// makes the nesting a no-op past the outer acquire).
//   not started (state bit0 clear): state = (state & 0xBE) | 1  -- set started,
//     clear bit6 -- then dispatch the VIRTUAL OnStartVoice (vtable +0x4C).
//   already started:                state &= ~4                 -- cancel a
//     pending async stop.
// Both arms converge on the single outer release. Returns 0.
// ---------------------------------------------------------------------------
s32 CVoice::Start()
{
    // (entry marker call dropped -- see the file banner)
    SXAudioRecursiveSpinLock* const lpLock = &gXAudioModuleLock;

    // --- outer acquire -----------------------------------------------------
    {
        const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
        void* const lpThread  = KeGetCurrentThread();
        s32 liRecursion = lpLock->miRecursion;
        if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
        {
            KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
            lpLock->mpOwnerThread = lpThread;
            lpLock->muSavedIrql   = luOldIrql;
            liRecursion           = lpLock->miRecursion;
        }
        lpLock->miRecursion = liRecursion + 1;
    }

    if ((muState & 1) == 0) // lbz 0x3D(r28) ; clrlwi 31 ; beq -> loc_82C32878
    {
        // --- inner acquire -------------------------------------------------
        {
            const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
            void* const lpThread  = KeGetCurrentThread();
            s32 liRecursion = lpLock->miRecursion;
            if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
            {
                KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
                lpLock->mpOwnerThread = lpThread;
                lpLock->muSavedIrql   = luOldIrql;
                liRecursion           = lpLock->miRecursion;
            }
            lpLock->miRecursion = liRecursion + 1;

            // andi. r11,r11,0xBE ; ori r11,r11,1 ; stb 0x3D(r28)
            muState = static_cast<u8>((muState & 0xBE) | 1);

            // --- inner release ---------------------------------------------
            void* const lpReleaseThread = KeGetCurrentThread(); // mr r10,r13
            const s32 liDepth = lpLock->miRecursion;
            if (liDepth != 0 && lpReleaseThread == lpLock->mpOwnerThread)
            {
                lpLock->miRecursion = liDepth - 1;
                if (liDepth == 1)
                {
                    const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                    lpLock->muSavedIrql   = 0;
                    lpLock->mpOwnerThread = 0;
                    KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                    KfLowerIrql(luRestoreIrql);
                }
            }
        }
        OnStartVoice(); // lwz 0(r28) ; lwz 0x4C(r11) ; bctrl -- virtual slot 19
    }
    else
    {
        // --- inner acquire -------------------------------------------------
        const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
        void* const lpThread  = KeGetCurrentThread();
        s32 liRecursion = lpLock->miRecursion;
        if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
        {
            KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
            lpLock->mpOwnerThread = lpThread;
            lpLock->muSavedIrql   = luOldIrql;
            liRecursion           = lpLock->miRecursion;
        }
        lpLock->miRecursion = liRecursion + 1;

        // andi. r11,r11,0xFB ; stb 0x3D(r28) -- clear the pending-async-stop bit
        muState = static_cast<u8>(muState & 0xFB);

        // --- inner release -------------------------------------------------
        void* const lpReleaseThread = KeGetCurrentThread(); // mr r10,r13
        const s32 liDepth = lpLock->miRecursion;
        if (liDepth != 0 && lpReleaseThread == lpLock->mpOwnerThread)
        {
            lpLock->miRecursion = liDepth - 1;
            if (liDepth == 1)
            {
                const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                lpLock->muSavedIrql   = 0;
                lpLock->mpOwnerThread = 0;
                KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                KfLowerIrql(luRestoreIrql);
            }
        }
    }

    // --- outer release -----------------------------------------------------
    {
        void* const lpThread = KeGetCurrentThread(); // mr r10,r13
        const s32 liDepth = lpLock->miRecursion;
        if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
        {
            lpLock->miRecursion = liDepth - 1;
            if (liDepth == 1)
            {
                const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                lpLock->muSavedIrql   = 0;
                lpLock->mpOwnerThread = 0;
                KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                KfLowerIrql(luRestoreIrql);
            }
        }
    }

    // (exit marker call dropped -- see the file banner)
    return 0; // li r3,0
}

} // namespace XAUDIO

// FOLDED FROM XAudioVoice_part_process.cpp
// ===========================================================================
// XAUDIO::CVoice -- the stop / per-frame processing group of the CVoice TU.
// Canonical home: SDKs/XAudio/XAudioVoice.h (this partfile holds three of the
// class's bodies; the remainder live in XAudioVoice.cpp and its siblings).
//
//     XAUDIO::CVoice::Stop          @ 0x82C32988  (vtable slot 15)
//     XAUDIO::CVoice::Process       @ 0x82C320A8  (vtable slot 17)
//     XAUDIO::CVoice::ProcessEffect @ 0x82C32650  (non-virtual)
//
// Reconstructed store-for-store from the X360 ARTIST asm (no reference source
// and no DWARF exist for this TU; the PowerPC listing is authoritative).
//
// The X360 compiler INLINED the XAudio module recursive spin-lock acquire/
// release around every state-byte write in Stop; each bracket below is the same
// idiom the committed XAUDIO::SEngineVoiceList::SwapActiveLists
// (XAudioEngineVoiceList.cpp) reconstructs, repeated per asm acquire/release
// site: raise to DPC IRQL, take the raised-IRQL spin-lock only when the lock is
// unheld or held by another thread, ++miRecursion; release = owner-checked
// --miRecursion whose last unwind clears the owner/saved IRQL, drops the
// spin-lock and lowers the IRQL back.
//
// Stop and Process both `bl BrnWorld::PVSDebugComponent::IsSimple` with a
// literal (2 / 3) at entry and again at every exit, discarding the result -- a
// folded no-op leaf standing in for a stripped profiling marker. Dropped, as in
// BrnNetworkLaunchManager.cpp and the Network message TUs.
// ===========================================================================

#include "SDKs/XAudio/XAudioVoice.h"
#include "SDKs/XAudio/XAudioEngine.h"          // gpEngine, CEngine::mpThreadFrameBuffers
#include "SDKs/XAudio/XAudioEngineVoiceList.h" // gXAudioModuleLock + Xbox kernel primitives

namespace XAUDIO
{

// ---------------------------------------------------------------------------
// @ 0x82C32988 -- stop the voice. Two whole arms, selected by `auFlag & 1`
// (`clrlwi r11, r31, 31` / `beq`):
//
//   ASYNCHRONOUS (bit0 CLEAR, the 0x82C32B78 arm): take the module lock; when
//   the voice is started (state bit0) take the lock again around
//   `muState |= 4` (`ori r11, r11, 4`), release BOTH brackets, and only THEN
//   store `muFrameCountdown = muMaxFrameSize` (`lhz 0x3E` / `sth 0x40` at
//   0x82C32D0C, outside every release) -- Process finishes the stop later. When
//   the voice is not started the outer bracket is simply released.
//
//   SYNCHRONOUS (bit0 SET, the fall-through arm): take the module lock, block on
//   the voice through the engine singleton
//   (`XAUDIO::CEngine::BlockOnActiveVoice(off_832BB944, this)`), and when the
//   voice is started take the lock again around `muState &= 0xBA`
//   (`andi. r11, r11, 0xBA` -- clears bits 0/2/6), release that inner bracket
//   and dispatch the VIRTUAL OnStopVoice (`lwz r11, 0x50(vtable)`, slot 20)
//   before releasing the outer bracket.
//
// Every arm returns 0 (`li r3, 0`).
// ---------------------------------------------------------------------------
s32 CVoice::Stop(u8 auFlag)
{
    SXAudioRecursiveSpinLock* const lpLock = &gXAudioModuleLock;

    if ((auFlag & 1) == 0)
    {
        // --- asynchronous stop (0x82C32B78) ---------------------------------
        {
            const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
            void* const lpThread  = KeGetCurrentThread();
            s32 liRecursion = lpLock->miRecursion;
            if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
            {
                KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
                lpLock->mpOwnerThread = lpThread;
                lpLock->muSavedIrql   = luOldIrql;
                liRecursion           = lpLock->miRecursion;
            }
            lpLock->miRecursion = liRecursion + 1;
        }

        if ((muState & 1) != 0)
        {
            // Inner bracket around the state-byte write (0x82C32C30).
            {
                const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
                void* const lpThread  = KeGetCurrentThread();
                s32 liRecursion = lpLock->miRecursion;
                if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
                {
                    KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
                    lpLock->mpOwnerThread = lpThread;
                    lpLock->muSavedIrql   = luOldIrql;
                    liRecursion           = lpLock->miRecursion;
                }
                lpLock->miRecursion = liRecursion + 1;

                muState |= 4; // pending async stop

                void* const lpReleaseThread = KeGetCurrentThread();
                const s32 liDepth = lpLock->miRecursion;
                if (liDepth != 0 && lpReleaseThread == lpLock->mpOwnerThread)
                {
                    lpLock->miRecursion = liDepth - 1;
                    if (liDepth == 1)
                    {
                        const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                        lpLock->muSavedIrql   = 0;
                        lpLock->mpOwnerThread = 0;
                        KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                        KfLowerIrql(luRestoreIrql);
                    }
                }
            }

            // Outer release (0x82C32CC8).
            {
                void* const lpThread = KeGetCurrentThread();
                const s32 liDepth = lpLock->miRecursion;
                if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
                {
                    lpLock->miRecursion = liDepth - 1;
                    if (liDepth == 1)
                    {
                        const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                        lpLock->muSavedIrql   = 0;
                        lpLock->mpOwnerThread = 0;
                        KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                        KfLowerIrql(luRestoreIrql);
                    }
                }
            }

            // Seeded AFTER both releases (0x82C32D0C is outside every bracket).
            muFrameCountdown = muMaxFrameSize;
            return 0;
        }

        // Not started: release the outer bracket only (0x82C32BE0).
        {
            void* const lpThread = KeGetCurrentThread();
            const s32 liDepth = lpLock->miRecursion;
            if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
            {
                lpLock->miRecursion = liDepth - 1;
                if (liDepth == 1)
                {
                    const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                    lpLock->muSavedIrql   = 0;
                    lpLock->mpOwnerThread = 0;
                    KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                    KfLowerIrql(luRestoreIrql);
                }
            }
        }
        return 0;
    }

    // --- synchronous stop (0x82C329B0) --------------------------------------
    {
        const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
        void* const lpThread  = KeGetCurrentThread();
        s32 liRecursion = lpLock->miRecursion;
        if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
        {
            KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
            lpLock->mpOwnerThread = lpThread;
            lpLock->muSavedIrql   = luOldIrql;
            liRecursion           = lpLock->miRecursion;
        }
        lpLock->miRecursion = liRecursion + 1;
    }

    gpEngine->BlockOnActiveVoice(this);

    if ((muState & 1) != 0)
    {
        // Inner bracket around the state-byte write (0x82C32A74).
        {
            const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
            void* const lpThread  = KeGetCurrentThread();
            s32 liRecursion = lpLock->miRecursion;
            if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
            {
                KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
                lpLock->mpOwnerThread = lpThread;
                lpLock->muSavedIrql   = luOldIrql;
                liRecursion           = lpLock->miRecursion;
            }
            lpLock->miRecursion = liRecursion + 1;

            muState &= 0xBA; // clear started (bit0), async-stop (bit2), bit6

            void* const lpReleaseThread = KeGetCurrentThread();
            const s32 liDepth = lpLock->miRecursion;
            if (liDepth != 0 && lpReleaseThread == lpLock->mpOwnerThread)
            {
                lpLock->miRecursion = liDepth - 1;
                if (liDepth == 1)
                {
                    const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                    lpLock->muSavedIrql   = 0;
                    lpLock->mpOwnerThread = 0;
                    KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                    KfLowerIrql(luRestoreIrql);
                }
            }
        }

        OnStopVoice(); // virtual, vtable slot 20 (`lwz r11, 0x50(r11)`)
    }

    // Outer release (0x82C32B1C / 0x82C32A18).
    {
        void* const lpThread = KeGetCurrentThread();
        const s32 liDepth = lpLock->miRecursion;
        if (liDepth != 0 && lpThread == lpLock->mpOwnerThread)
        {
            lpLock->miRecursion = liDepth - 1;
            if (liDepth == 1)
            {
                const KIRQL luRestoreIrql = lpLock->muSavedIrql;
                lpLock->muSavedIrql   = 0;
                lpLock->mpOwnerThread = 0;
                KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                KfLowerIrql(luRestoreIrql);
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// @ 0x82C320A8 -- render one voice frame.
//   * Seed the frame-buffer tap array from the VIRTUAL GetFrameBuffer
//     (`lwz r11, 0x48(vtable)`, slot 18) -- the X360 frame reserves three tap
//     words (var_1C..) but the base body only ever writes [0]; derived overrides
//     thread the rest, so the array shape is kept.
//   * When a process callback is installed, copy the context word (+0x30) to the
//     stack and fire the callback with a pointer to that COPY
//     (`stw r10, var_20(r1)` / `addi r3, r1, var_20`), not to the member.
//   * When the async-stop bit (state bit2) is set, an expired countdown finishes
//     the stop through the VIRTUAL Stop(1) (`lwz r11, 0x3C(vtable)`, slot 15)
//     and returns its result; otherwise the countdown is decremented
//     (`addis`/`addi` pair = a 16-bit -1) and the frame continues.
//   * Otherwise run the VIRTUAL ProcessEffects (`lwz r11, 0x54(vtable)`,
//     slot 21) over the taps and return its result.
// ---------------------------------------------------------------------------
s32 CVoice::Process()
{
    CFrameBuffer* lTaps[3];
    lTaps[0] = GetFrameBuffer();

    if (mpProcessCallback)
    {
        void* lpContext = mpVoiceContext;
        mpProcessCallback(&lpContext);
    }

    if ((muState & 4) != 0)
    {
        if (muFrameCountdown == 0)
            return Stop(1);
        --muFrameCountdown;
    }

    return ProcessEffects(lTaps);
}

// ---------------------------------------------------------------------------
// @ 0x82C32650 -- process one effect record and dispatch the effect's own
// Process (`lwz r11, 0x18(effect vtable)`, slot [6]) with the resolved taps.
//
// Gated on the record's enable bit (`lbz 5(rec)` & 1); disabled effects return 0
// without dispatching. The routing byte (`lbz 6(rec)`) then picks the taps:
//   * (x & 6) == 6 -- OUT-OF-PLACE: ping-pong within the current hardware
//     thread's engine frame-buffer pair (`lbz 0x10C(r13)` = the KPCR processor
//     index, indexing gpEngine->mpThreadFrameBuffers at +0x0C, 8-byte stride).
//     The current tap is searched for in the pair; a match routes the output to
//     the OTHER slot (`(index - 1) & 1`), while a miss -- or a null current tap,
//     which skips the search entirely -- routes it to slot 0. The chosen output
//     is written back through `apInOut`, and the ORIGINAL tap stays the input.
//   * (x & 3) != 0 -- input-only:  in = *apInOut, out = null.
//   * (x & 4) != 0 -- output-only: out = *apInOut, in = null.
//   * otherwise -- BOTH taps are read from the SAME never-written stack slot
//     (`lwz r7, var_10(r1)` / `lwz r11, var_10(r1)`; nothing in the function
//     writes var_10). Reproduced faithfully with one uninitialised local: this
//     arm passes indeterminate taps in the shipped binary. (Warns C4700; the
//     compile gate does not use /WX.)
// ---------------------------------------------------------------------------
s32 CVoice::ProcessEffect(SVoiceEffect* apEffect, CFrameBuffer** apInOut)
{
    if ((apEffect->muState & 1) == 0)
        return 0;

    CFrameBuffer* lpIn;
    CFrameBuffer* lpOut;

    if ((apEffect->muFlagB & 6) == 6)
    {
        lpIn = *apInOut;

        const u32      luThread = KeGetCurrentProcessorNumber();
        CEngine* const lpEngine = gpEngine;

        u32 luSlot = 2; // 2 == "not found" (the loop's exit bound)
        if (lpIn)
        {
            for (u32 luIndex = 0; luIndex < 2; ++luIndex)
            {
                if (lpIn == lpEngine->mpThreadFrameBuffers[luThread][luIndex])
                {
                    luSlot = luIndex;
                    break;
                }
            }
        }

        lpOut = (luSlot < 2)
                  ? lpEngine->mpThreadFrameBuffers[luThread][(luSlot - 1) & 1]
                  : lpEngine->mpThreadFrameBuffers[luThread][0];
        *apInOut = lpOut;
    }
    else if ((apEffect->muFlagB & 3) != 0)
    {
        lpIn  = *apInOut;
        lpOut = 0;
    }
    else if ((apEffect->muFlagB & 4) != 0)
    {
        lpOut = *apInOut;
        lpIn  = 0;
    }
    else
    {
        // Both taps come from the same uninitialised frame slot (var_10) -- see
        // the banner above; kept as-is rather than inventing an initial value.
        CFrameBuffer* lpUnwrittenTap;
        lpIn  = lpUnwrittenTap;
        lpOut = lpUnwrittenTap;
    }

    IXAudioEffect* const lpEffect = static_cast<IXAudioEffect*>(apEffect->mpEffect);
    return lpEffect->mpVTable->mpProcess(lpEffect, lpIn, lpOut);
}

} // namespace XAUDIO
