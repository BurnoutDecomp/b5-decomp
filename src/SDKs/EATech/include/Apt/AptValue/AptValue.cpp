// ===========================================================================
// EATech Apt -- AptValue out-of-line virtual bodies.
//
// SHAPE from the Feb-2007 leak (AptValue.h:509 / :535); BODIES confirmed
// against the X360 ARTIST.XEX pseudocode:
//     AptValue::DeleteThis    @ 0x824ED4B8
//     AptValue::ForceDelete   @ 0x824ED4D8
//
// X360 0x824ED4B8 (DeleteThis): a null guard then a virtual call into the
//   scalar-deleting-destructor slot (vtbl +0x38) with the delete flag set --
//   exactly the codegen of `delete this`. The leak body is `delete this`.
// X360 0x824ED4D8 (ForceDelete): vtbl +0x24 (PreDestroy), then vtbl +0x28
//   (DestroyGCPointers), then vtbl +0x38 (`delete this`). Matches the leak's
//   PreDestroy(); DestroyGCPointers(); delete this;.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"   // the deferred-release vector
#include "SDKs/EATech/include/Apt/AptNativeHash.h"              // mp__Proto__ (the MC parent-chain walk)

#include <intrin.h>   // _InterlockedExchange (the Apt GC flag lock)

// The Apt GC reference-registration callback. The GC value types' RegisterReferences
// call through it, so the mark walk is inert while it is null. The callback BODY is
// AptGC::sReferenceRegistrationCb (AptGC.cpp @0x82AD9C80). FLAG (parked): the console
// installer that stamps this slot (dword_8324E4E8) -- the partial-GC mark/sweep tier,
// XB1 sub_140832E70 -- is un-homed; the install lands with that sweep TU.
AptValue::ReferenceRegistrationCb AptValue::sReferenceRegistrationCb = 0;

// AptGC::CleanAll sets this while it tears the value graph down (X360 byte_8324E38E).
bool AptValue::sbSuspendRefcountDeletions = false;

// The Apt GC lock (X360 unk_8324E75C) -- a single-word spin lock guarding the GC
// mark / deferred-release flag bit mutations; the console's lwarx/stwcx.
// interrupt-masked idiom modelled as an interlocked test-and-set (see the leaf
// FLAG below; host-portable, uncontended on the single-thread bring-up path).
namespace
{
    volatile long gAptGCFlagLock = 0;
    // FLAG PC-platform leaf: single-threaded GC-flag spin lock -- interrupt-masked lwarx/stwcx. modelled as an _InterlockedExchange TAS (threading primitive, not an engine method).
    inline void AptGCFlagLock_Acquire() { while (_InterlockedExchange(&gAptGCFlagLock, 1) != 0) {} }
    inline void AptGCFlagLock_Release() { _InterlockedExchange(&gAptGCFlagLock, 0); }
}

// setGCMark @0x82AD8020 -- set mbHasRegisterReferenceMark under the GC lock.
void AptValue::setGCMark(bool bMark)
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbHasRegisterReferenceMark = bMark ? 1u : 0u;
    AptGCFlagLock_Release();
}

// ClearReleaseAtEnd @0x82AD82A8 -- clear mbIsInDeferredVector under the GC lock.
void AptValue::ClearReleaseAtEnd()
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbIsInDeferredVector = 0u;
    AptGCFlagLock_Release();
}

// incGCRoot @0x82AD8120 -- bump the GC-root count one, under the Apt GC flag
// lock. X360: acquire the lwarx/stwcx spin lock on unk_8324E75C, then
//   if ((*(this+4) & 0x3F00) < 0x3F00) <add 0x100 to the field>
// i.e. if the 6-bit mnGCRootCount is below MAX_GCROOT, increment it, then
// release the lock. Direct field write (NOT via setGCRoot -- the shipped body
// manipulates the word inline, and setGCRoot takes this same non-recursive lock).
void AptValue::incGCRoot()
{
    AptGCFlagLock_Acquire();
    if (mValueBitfield.mnGCRootCount < MAX_GCROOT)
        mValueBitfield.mnGCRootCount = mValueBitfield.mnGCRootCount + 1u;
    AptGCFlagLock_Release();
}

// decGCRoot @0x82AD81A8 -- drop the GC-root count one (floored at 0), under the
// Apt GC flag lock. X360: acquire the spin lock, then
//   if (((*(this+4) >> 8) & 0x3F) != 0) <field = field - 1>
// i.e. if mnGCRootCount is non-zero, decrement it, then release the lock.
// Direct field write (see incGCRoot).
void AptValue::decGCRoot()
{
    AptGCFlagLock_Acquire();
    if (mValueBitfield.mnGCRootCount != 0u)
        mValueBitfield.mnGCRootCount = mValueBitfield.mnGCRootCount - 1u;
    AptGCFlagLock_Release();
}

// setRefCount @0x82AD7E90 -- store the reference count under the Apt GC flag
// lock. X360: acquire the spin lock; a count above MAX_REFCOUNT (cmplwi 0xFFF,
// ble skips -- STRICTLY greater) first latches the max-hit flag (`*(this+4) |=
// 0x80`) and clamps to 0xFFF; then insrwi the 12-bit count and release the lock.
void AptValue::setRefCount(uint32_t n)
{
    AptGCFlagLock_Acquire();
    if (n > MAX_REFCOUNT)
    {
        mValueBitfield.mnMaxRefCountHit = 1u;   // X360 ori r10,r10,0x80
        n = MAX_REFCOUNT;                       // X360 li r4,0xFFF
    }
    mValueBitfield.mnReferenceCount = n;        // X360 insrwi r10,r4,12,6
    AptGCFlagLock_Release();
}

// setVtblIndex @0x82AD7F20 -- store the 7-bit value-type tag under the Apt GC
// flag lock (X360 insrwi r9,r4,7,25).
void AptValue::setVtblIndex(AptVirtualFunctionTable_Indices n)
{
    AptGCFlagLock_Acquire();
    mValueBitfield.meValueType = n;
    AptGCFlagLock_Release();
}

// setGCRoot @0x82AD80A8 -- store the 6-bit GC-root count under the Apt GC flag
// lock. NO clamp in the shipped body (insrwi r9,r4,6,18 just masks the low 6
// bits); the MAX_GCROOT guard lives in incGCRoot.
void AptValue::setGCRoot(uint32_t n)
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mnGCRootCount = n & MAX_GCROOT;   // the 6-bit field mask
    AptGCFlagLock_Release();
}

// SetAllowDelayedDeletion @0x82AD7E08 -- store the delayed-deletion flag under
// the Apt GC flag lock (X360 insrwi r8,r9,1,5).
void AptValue::SetAllowDelayedDeletion(bool bAllowed)
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbAllowsDelayedDeletion = bAllowed ? 1u : 0u;
    AptGCFlagLock_Release();
}

// SetReleaseAtEnd @0x82AD8230 -- mark this value as queued in the GC deferred-
// release vector, under the Apt GC flag lock. X360: acquire the spin lock, then
//   *(this+4) |= 0x20000000   (oris r10,r10,0x2000)
// which sets the big-endian bit for mbIsInDeferredVector, then release the lock.
// (The "second locked block" in the pseudocode is the lock release itself.)
void AptValue::SetReleaseAtEnd()
{
    AptGCFlagLock_Acquire();
    mValueBitfield.mbIsInDeferredVector = 1u;
    AptGCFlagLock_Release();
}

// DeleteThis @ 0x824ED4B8
void AptValue::DeleteThis()
{
    // X360: `if (this) <scalar deleting destructor>(this, 1)`. `delete this`
    // on a non-null receiver lowers to exactly that virtual dispatch.
    delete this;
}

// ForceDelete @ 0x824ED4D8
void AptValue::ForceDelete()
{
    PreDestroy();
    DestroyGCPointers();
    delete this;
}

// ===========================================================================
// AptValue lifecycle: the two protected ctors + the refcount virtuals
// (AddRef/Release) + the MovieClip parent-chain query. DECOMPILED from the
// X360 ARTIST.XEX:
//     AptValue::AptValue(eType)             = sub_82AE3000 (the base ctor every
//                                             derived value's member-init runs)
//     AptValue::AptValue(eType, CIH_ONLY)   @ 0x82ADCCA8 (the AptCIH-only ctor)
//     AptValue::AddRef                      @ 0x82ADCF20
//     AptValue::Release                     @ 0x82AE31D0
//     AptValue::isMCInParentChain           @ 0x82AD8458
// ===========================================================================

// ---------------------------------------------------------------------------
// The Apt GC globals this TU reads (all defined in AptGlobals.cpp):
//   gpAptDeferredReleaseVector -- the GC "deferred release" vector pointer
//       (X360 off_8324E51C, an AptValueVector*). The base ctor and Release queue
//       a value into it (instead of deleting immediately) when running on the GC
//       thread and the value's type opts into delayed deletion.
//       FLAG (parked): AptCommonInitialize (AptInit.cpp) builds the off_8324E51C
//       vector but publishes it only to its file-local alias (gpAptDeferredVecCommon),
//       and AptGlobals.cpp carries a THIRD view (the embedded AptGCReleaseVector
//       gValuesToRelease) of the same console address; until the three off_8324E51C
//       homes are unified this pointer stays null and the ctor/Release defer arms
//       stay inert (out-of-cluster fix: AptGlobals.cpp + AptInit.cpp).
//   gnAptGCThreadId_Ctor / gnAptGCThreadId_Release -- the captured GC thread ids
//       the ctor (X360 dword_8324E500) and Release (X360 dword_8324E504) compare
//       the current thread against. Distinct globals in the binary; kept distinct.
// ---------------------------------------------------------------------------
extern AptValueVector* gpAptDeferredReleaseVector;   // off_8324E51C
extern uint32_t        gnAptGCThreadId_Ctor;         // dword_8324E500
extern uint32_t        gnAptGCThreadId_Release;      // dword_8324E504

// AptCurrentThreadId -- the GC-thread guard's current-thread query. Defined in
// AptRenderLinkStubs.cpp as a PC-platform leaf (single-threaded host: one thread
// id, 0), so the GC-thread compares below resolve the same way the console's
// single-sim-thread boot path does.
extern uint32_t AptCurrentThreadId();


// The active script "current target" MovieClips the parent-chain walk stops at
// (X360 dword_8324D818 = the highlighted/current target, dword_8324D830 = the
// chain terminator / root). Defined in AptGlobals.cpp. FLAG (parked): the
// interpreter-side stores that publish them once a movie is live are owned by
// the interpreter/target TUs (outside this cluster); null here makes the walk
// run the full chain -- the faithful pre-publish result.
extern AptValue* gpAptCurrentTargetMC;     // dword_8324D818
extern AptValue* gpAptRootTargetMC;        // dword_8324D830

// ---------------------------------------------------------------------------
// AptValue(eType) = sub_82AE3000 -- the base ctor. Set the vtable, then build the
// packed bitfield: type=eType, refcount=0, defined=1, GCmark=0, GCroot(default),
// allow-delayed-deletion=1; finally clear the max-refcount-hit flag (X360
// `a1[1] &= ~0x80`). On the GC thread, value types that DON'T opt out of delayed
// deletion ({Prototype 20, ScriptFunction1 34, ScriptFunction2 35, NativeFunction
// 9, Extension 29}) are queued into the deferred-release vector instead of being
// eligible for immediate teardown.
// ---------------------------------------------------------------------------
AptValue::AptValue(AptVirtualFunctionTable_Indices eType)
{
    setVtblIndex(eType);
    setRefCount(0);
    setIsDefined(true);
    setGCMark(false);
    setGCRoot(0);                    // X360 setGCRoot(r4) -- r4 == 0 here (leftover
                                     // from setGCMark's `li r4,0`); a fresh value
                                     // has no GC roots.
    SetAllowDelayedDeletion(false);  // X360: r4==0 (leftover from setGCMark's li r4,0) -> false

    // X360: the five "no delayed deletion" types skip the deferred-vector path and
    // go straight to ClearReleaseAtEnd.
    const bool lbSkipDefer =
        (eType == AptVFT_Prototype)        ||   // 0x14
        (eType == AptVFT_ScriptFunction1)  ||   // 0x22
        (eType == AptVFT_ScriptFunction2)  ||   // 0x23
        (eType == AptVFT_NativeFunction)   ||   // 0x09
        (eType == AptVFT_Extension);            // 0x1D

    if (!lbSkipDefer && gnAptGCThreadId_Ctor == AptCurrentThreadId())
    {
        // On the GC thread: queue into the deferred-release vector if it has room
        // (X360 family-(B) layout: top(+4)=mnCapacity < capacity(+0)=mnTop).
        SetReleaseAtEnd();
        AptValueVector* lpVec = gpAptDeferredReleaseVector;   // off_8324E51C (see the parked unification FLAG above)
        if (lpVec != 0 && lpVec->mnCapacity < lpVec->mnTop)
        {
            lpVec->mppItems[lpVec->mnCapacity] = this;
            ++lpVec->mnCapacity;
        }
    }
    else
    {
        ClearReleaseAtEnd();
    }

    // X360 `a1[1] &= ~0x80` -- clear the max-refcount-hit flag set by setRefCount's
    // clamp path (rlwinm r11,r11,0,25,23 keeps every bit except bit 24 == 0x80).
    mValueBitfield.mnMaxRefCountHit = 0u;
}

// ---------------------------------------------------------------------------
// AptValue(eType, CIH_ONLY) @0x82ADCCA8 -- the AptCIH-only ctor. Identical
// bitfield set-up as the base ctor BUT it never enters the deferred-release path:
// it unconditionally ClearReleaseAtEnd()s (an AptCIH is owned by its container's
// display tree, not the GC zombie vector). X360 also clears the max-hit flag.
// ---------------------------------------------------------------------------
AptValue::AptValue(AptVirtualFunctionTable_Indices eType, const CIH_ONLY /*eCIH*/)
{
    setVtblIndex(eType);
    setRefCount(0);
    setIsDefined(true);
    setGCMark(false);
    setGCRoot(0);                    // X360 setGCRoot(r4), r4 == 0 (see base ctor)
    SetAllowDelayedDeletion(false);  // X360: r4==0 (leftover from setGCMark's li r4,0) -> false

    mValueBitfield.mnMaxRefCountHit = 0u;   // X360 `*(this+4) &= ~0x80` (rlwinm 0,25,23)

    ClearReleaseAtEnd();
}

// ---------------------------------------------------------------------------
// AddRef @0x82ADCF20 -- bump the reference count, saturating at MAX_REFCOUNT and
// latching the max-hit flag on overflow. X360: read mnReferenceCount, +1; if it
// would exceed 0xFFF, clamp to 0xFFF and OR in 0x80 (mnMaxRefCountHit), then store
// the (clamped) count back into bits 14..25.
// ---------------------------------------------------------------------------
void AptValue::AddRef()
{
    uint32_t lnCount = getRefCount() + 1;
    if (lnCount > MAX_REFCOUNT)
    {
        lnCount = MAX_REFCOUNT;
        mValueBitfield.mnMaxRefCountHit = 1u;   // X360 `*(this+4) |= 0x80`
    }
    mValueBitfield.mnReferenceCount = lnCount;
}

// ---------------------------------------------------------------------------
// Release @0x82AE31D0 -- drop one reference; on the last release tear the value
// down. Faithful to the X360 control flow:
//   * decrement mnReferenceCount; only the last release (old count == 1) proceeds;
//   * while AptGC::CleanAll runs (sbSuspendRefcountDeletions): call the scalar-
//     deleting-destructor virtual (vtbl +0x30) and, if it consumed the value
//     (non-null `this` return), stop -- otherwise fall through to the normal path;
//   * a max-hit value (mnMaxRefCountHit, bit 24) is permanent: re-pin its count to
//     MAX_REFCOUNT and stop (never deleted);
//   * if the value does NOT allow delayed deletion (mbAllowsDelayedDeletion, bit
//     26 == 0) -> tear down now (vtbl +0x2C, DeleteThis);
//   * an already-deferred value (mbIsInDeferredVector, be-bit 2 / >>29) is inert -> stop;
//   * else, when running ON the GC thread (captured id == current) -> tear down now
//     (vtbl +0x2C); when running OFF the GC thread -> queue into the deferred-
//     release vector if it has room (SetReleaseAtEnd + push), and only if it is
//     full roll the queue back (ClearReleaseAtEnd) and tear down now (vtbl +0x2C).
// X360 vtbl: slot +0x30 = the scalar-deleting destructor (suspend path); slot
// +0x2C = the normal teardown == ForceDelete, NOT a bare `delete this`: the shipped
// slot body (X360 0x82AD4DF8; XB1 slot +88 = sub_140837DE0, byte-identical shape)
// is the composite `vtbl[PreDestroy](this); vtbl[DestroyGCPointers](this);
// vtbl[deleting-dtor](this, 1)`. Every refcount death therefore runs the full GC
// teardown (parent release + char-inst/render-item destruction), which a bare
// DeleteThis skipped -- that was an audit bug, fixed 2026-07-02.
// ---------------------------------------------------------------------------
void AptValue::Release()
{
    // X360: mnReferenceCount-- then test whether the OLD count was 1 (i.e. the new
    // count is 0). The field stores (count-1) masked to 12 bits.
    const uint32_t lnOldCount = getRefCount();
    mValueBitfield.mnReferenceCount = (lnOldCount - 1u) & MAX_REFCOUNT;
    if (lnOldCount != 1u)
        return;

    // During a GC CleanAll the collector drives destruction via the scalar-deleting
    // destructor (vtbl +0x30). X360: `if (DeleteThis(this) != 0) return;` -- a
    // scalar-deleting destructor always returns its (non-null) receiver, so the
    // console's null test is vestigial codegen and the branch is always taken; the
    // void DeleteThis + unconditional return is behaviour-identical. (CleanAll is
    // live -- AptGC.cpp @0x82AE4A40 -- and drives this path at shutdown.)
    if (sbSuspendRefcountDeletions)
    {
        DeleteThis();   // X360 `result = (*(*this+0x30))(this)`; non-null -> return
        return;
    }

    // A value that hit MAX_REFCOUNT is permanent -- re-pin its count, never delete.
    if (GetMaxRefCountHit())           // X360 ((v3 >> 24/bit) & 1) != 0 (the 0x80 bit)
    {
        setRefCount(MAX_REFCOUNT);     // X360 setRefCount(this, 0xFFF)
        return;
    }

    // Non-delayed-deletion values tear down immediately (vtbl +0x2C == ForceDelete).
    if (!getAllowsDelayedDeletion())   // X360 ((v3 >> 26) & 1) == 0 -> goto teardown
    {
        ForceDelete();                 // X360 `(*(*this+0x2C))(this)` -- the composite
        return;
    }

    // An already-deferred value is inert here -- the X360 extrwi tests be-bit 2 ==
    // (v3 >> 29) & 1 == mbIsInDeferredVector (the IsReleaseAtEnd flag), NOT mbDestroyedGC.
    if (mValueBitfield.mbIsInDeferredVector)
        return;

    // ON the GC thread: tear down now. OFF the GC thread: try the deferred vector.
    if (gnAptGCThreadId_Release == AptCurrentThreadId())
    {
        ForceDelete();                 // X360 beq cr6, loc_82AE32C0 -> slot +0x2C
        return;
    }

    AptValueVector* lpVec = gpAptDeferredReleaseVector;   // off_8324E51C (see the parked unification FLAG above)
    // X360: if top(+4) < capacity(+0) there is room. (Family-(B) layout: capacity is
    // the +0 member mnTop, the live top is the +4 member mnCapacity.)
    if (lpVec == 0 || lpVec->mnCapacity >= lpVec->mnTop)
    {
        ForceDelete();                 // no room -> teardown now (slot +0x2C)
        return;
    }

    SetReleaseAtEnd();
    // X360 reloads off_8324E51C after SetReleaseAtEnd and re-checks for room.
    AptValueVector* lpVec2 = gpAptDeferredReleaseVector;
    if (lpVec2->mnCapacity >= lpVec2->mnTop)
    {
        ClearReleaseAtEnd();           // full now -> roll back the queue intent
        return;
    }
    lpVec2->mppItems[lpVec2->mnCapacity] = this;   // mppItems[top] = this
    ++lpVec2->mnCapacity;                            // ++top
}

// ---------------------------------------------------------------------------
// isMCInParentChain @0x82AD8458 -- does the active "current target" MovieClip
// appear in this value's MovieClip parent chain? X360: stop immediately if this
// IS the current target (dword_8324D818); otherwise climb the chain via the
// GetParent virtual (slot +8) -> the parent's parent field (+8), returning 1 on a
// match and stopping at the root terminator (dword_8324D830).
// ---------------------------------------------------------------------------
int AptValue::isMCInParentChain() const
{
    // CORRECTED 2026-07-02 against the shipped asm: the walk is
    // `hash = vtbl[2]() (GetNativeHashVirtual); node = hash->mp__Proto__ (+8)`
    // -- NOT a "GetParent" virtual + raw +8 read (that shim was a
    // reconstruction invention; retired from AptRenderLinkStubs).
    AptValue* const lpTarget = gpAptCurrentTargetMC;   // X360 r31 = dword_8324D818
    AptValue* const lpRoot   = gpAptRootTargetMC;      // X360 r30 = dword_8324D830

    AptValue* lpNode = const_cast<AptValue*>(this);
    if (lpNode == lpTarget)
        return 1;

    for (;;)
    {
        AptNativeHash* const lpHash = lpNode->GetNativeHashVirtual();   // vtbl[2]
        if (lpHash == nullptr)
            break;
        lpNode = lpHash->mp__Proto__;                                   // hash +8
        if (lpNode == nullptr)
            break;
        if (lpNode == lpTarget)
            return 1;
        if (lpNode == lpRoot)
            break;
    }
    return 0;
}
