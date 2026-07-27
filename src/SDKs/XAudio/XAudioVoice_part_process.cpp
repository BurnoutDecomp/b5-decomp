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
                        lpLock->mpOwnerThread = nullptr;
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
                        lpLock->mpOwnerThread = nullptr;
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
                    lpLock->mpOwnerThread = nullptr;
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
                    lpLock->mpOwnerThread = nullptr;
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
                lpLock->mpOwnerThread = nullptr;
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
        lpOut = nullptr;
    }
    else if ((apEffect->muFlagB & 4) != 0)
    {
        lpOut = *apInOut;
        lpIn  = nullptr;
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
