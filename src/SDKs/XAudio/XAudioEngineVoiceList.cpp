#include "SDKs/XAudio/XAudioEngine.h"          // XAUDIO::SEngineVoiceList (committed layout)
#include "SDKs/XAudio/XAudioEngineVoiceList.h" // recursive spin-lock + Xbox primitives

// ===========================================================================
// XAUDIO::CEngine::CEngineVoiceList::SwapActiveLists  @ 0x82C2DC50
//
// Ledger TU class:XAUDIO::CEngine::CEngineVoiceList (1 function). No reference
// source and no DWARF exist; the PowerPC asm is authoritative. The list layout
// is the committed XAUDIO::SEngineVoiceList (XAudioEngine.h); this file only
// bodies the one method and defines the module lock it takes.
//
// Shape from the asm: the whole body is a single acquire/swap/release of the
// XAUDIO module recursive spin-lock (rodata @0x83222C28) wrapped around the
// front/back active-sub-list pointer flip. The recursive-lock idiom the X360
// compiler inlined here is the standard one:
//   * raise to DPC IRQL, remember the old IRQL;
//   * if the lock is unheld (miRecursion==0) OR a different thread holds it,
//     take the raised-IRQL spin-lock and record owner + saved IRQL;
//   * ++miRecursion;                     <-- entered the critical section
//   * ... swap mpActiveA <-> mpActiveB ...
//   * if we own it, --miRecursion; on the last release clear owner + saved
//     IRQL, drop the spin-lock and lower the IRQL back.
// De-gotoed to structured C++; every store / branch / early-out has a
// counterpart and no behaviour is added.
// ===========================================================================

namespace XAUDIO
{

// The module recursive spin-lock (rodata base 0x83222C28). Declared in
// XAudioEngineVoiceList.h; zero-initialised (unheld, no owner) at load, matching
// the .bss/rodata seed the engine relies on before the first swap.
SXAudioRecursiveSpinLock gXAudioModuleLock = {};

s32 SEngineVoiceList::SwapActiveLists()
{
    SXAudioRecursiveSpinLock* const lpLock = &gXAudioModuleLock;

    // Raise to DPC level; the returned old IRQL is the one well-defined value
    // that flows to r3. (KeAcquireSpinLockAtRaisedIrql / KfLowerIrql are void on
    // X360; the Hex-Rays `result = ...` on them is just leftover r3 and is not a
    // meaningful return, so the old IRQL is returned. FLAG: return value.)
    const KIRQL luOldIrql = KeRaiseIrqlToDpcLevel();
    void* const lpThread  = KeGetCurrentThread();
    s32 liResult          = (s32)luOldIrql;

    // Acquire the underlying spin-lock only on first (non-recursive) entry, or
    // when the current holder is a different thread.
    s32 liRecursion = lpLock->miRecursion;
    if (liRecursion == 0 || lpThread != lpLock->mpOwnerThread)
    {
        KeAcquireSpinLockAtRaisedIrql(&lpLock->muLock);
        lpLock->mpOwnerThread = lpThread;
        lpLock->muSavedIrql   = luOldIrql;
        liRecursion           = lpLock->miRecursion;
    }
    lpLock->miRecursion = liRecursion + 1;

    // --- critical section: flip the front/back active sub-list pointers ------
    XAUDIOPACKETCTX_* const lpActiveA = mpActiveA;
    mpActiveA = mpActiveB;
    mpActiveB = lpActiveA;

    // Release: only the owning thread unwinds its recursion; the last release
    // clears the owner, drops the spin-lock and restores the pre-DPC IRQL.
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

    return liResult;
}

} // namespace XAUDIO
