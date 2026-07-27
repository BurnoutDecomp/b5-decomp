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
                lpLock->mpOwnerThread = nullptr;
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
            lpLock->mpOwnerThread = nullptr;
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
                    lpLock->mpOwnerThread = nullptr;
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
                lpLock->mpOwnerThread = nullptr;
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
                lpLock->mpOwnerThread = nullptr;
                KeReleaseSpinLockFromRaisedIrql(&lpLock->muLock);
                KfLowerIrql(luRestoreIrql);
            }
        }
    }

    // (exit marker call dropped -- see the file banner)
    return 0; // li r3,0
}

} // namespace XAUDIO
