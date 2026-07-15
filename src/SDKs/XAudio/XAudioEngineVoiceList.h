#pragma once

// ===========================================================================
// XAUDIO::CEngine::CEngineVoiceList -- the nested engine voice-list of
// XAUDIO::CEngine in BURNOUT_X360_ARTIST.XEX. Its layout is the committed
// XAUDIO::SEngineVoiceList (see XAudioEngine.h, embedded at CEngine+0x50 and one
// per element of the CEngine+0x7C active-list array). This header is the ledger
// home for the TU class:XAUDIO::CEngine::CEngineVoiceList (1 function):
//
//     XAUDIO::CEngine::CEngineVoiceList::SwapActiveLists  @ 0x82C2DC50
//
// It does NOT re-home the list layout (that stays committed in XAudioEngine.h);
// it only supplies the Xbox-360 kernel recursive-spin-lock boundary the body
// needs. `XAUDIO` is an external middleware boundary, so its identifiers are
// preserved verbatim per the naming convention.
//
// There is no reference source and no DWARF for this TU; the PowerPC asm of
// SwapActiveLists @0x82C2DC50 is authoritative.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// --- Xbox-360 kernel spin-lock / IRQL primitives (platform boundary) --------
// Real X360 NT-kernel entry points, declared with the shapes the SwapActiveLists
// call sites use so the reconstructed body compiles. Supplied by the platform
// layer; NOT reconstructed here. (KfLowerIrql / KeAcquireSpinLockAtRaisedIrql /
// KeReleaseSpinLockFromRaisedIrql return void; the Hex-Rays `result = ...` on
// them is just the live r3 register, not a meaningful value -- see the .cpp.)
typedef u32 KSPIN_LOCK;
typedef u8  KIRQL;

KIRQL KeRaiseIrqlToDpcLevel(void);
void  KeAcquireSpinLockAtRaisedIrql(KSPIN_LOCK* apSpinLock);
void  KeReleaseSpinLockFromRaisedIrql(KSPIN_LOCK* apSpinLock);
void  KfLowerIrql(KIRQL auNewIrql);

// Current-thread identity token used as the recursive-lock owner. The X360 asm
// reads the raw KPCR (`mr r29, r13`) -- the per-hardware-thread control block --
// as the ownership token. At the raised (DPC) IRQL this lock runs at, that is
// exactly the current thread, so it is modelled by the documented Xbox current-
// thread accessor. FLAG: token source is the KPCR (r13) in the binary; declared
// here as the platform current-thread pointer. Supplied by the platform layer.
void* KeGetCurrentThread(void);

// ---------------------------------------------------------------------------
// The XAudio module-shared RECURSIVE spin-lock (rodata @ 0x83222C28). One global
// lock guards the engine's voice-list swaps and the CEngine spinlock-protected
// processing paths (the same unk_83222C28/dword_83222C2C/dword_83222C30/
// byte_83222C34 quartet named in the XAudioEngine.h/.cpp BLOCKED notes). Field
// offsets are attested by the SwapActiveLists asm, which addresses the whole
// quartet off r31 = &unk_83222C28: the lock at +0x00, the recursion count at
// +0x04, the owner token at +0x08 and the saved IRQL at +0x0C.
//   +0x00 muLock        -- KSPIN_LOCK (unk_83222C28)
//   +0x04 miRecursion   -- recursion depth (dword_83222C2C)
//   +0x08 mpOwnerThread -- owning thread token (dword_83222C30)
//   +0x0C muSavedIrql   -- IRQL to restore on final release (byte_83222C34)
// ---------------------------------------------------------------------------
struct SXAudioRecursiveSpinLock
{
    KSPIN_LOCK muLock;        // +0x00  unk_83222C28
    s32        miRecursion;   // +0x04  dword_83222C2C
    void*      mpOwnerThread; // +0x08  dword_83222C30
    KIRQL      muSavedIrql;   // +0x0C  byte_83222C34
    u8         mPad0D[3];     // +0x0D  align
};

// The single module lock instance (rodata base 0x83222C28). Defined in
// XAudioEngineVoiceList.cpp.
extern SXAudioRecursiveSpinLock gXAudioModuleLock;

} // namespace XAUDIO
