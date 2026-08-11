// ===========================================================================
// EATech Apt -- AptGC.   DECOMPILED from the X360 ARTIST.XEX.
//   sReferenceRegistrationCb @0x82AD9C80 / CleanAll @0x82AE4A40.
// ===========================================================================

#include <cstring>   // std::memmove (the zombie-vector compaction)

#include "SDKs/EATech/include/Apt/AptGC.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"   // the deferred-release vector
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"          // AptInteger::ClearPool
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"            // AptFloat::ClearPool
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"      // the zombie vector (family B)
#include "SDKs/EATech/include/Apt/AptString/StringPool.h"         // StringPool::ClearTemporaryPool
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"                // AptValueGC_PoolManager
#include "SDKs/EATech/include/Apt/AptDefine.h"                    // gpGCPoolManager (off_8324D834)
#include "SDKs/EATech/include/Apt/AptCIH.h"                       // the zombie entries
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"    // mAnimationFilePtr (the reap's file-state swap)
#include "SDKs/EATech/include/Apt/AptFile.h"                      // mnState / mnField12 (the saved-state slot)
#include "SDKs/EATech/include/Apt/AptTarget.h"                    // gpAptTarget / current target
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"           // queued-input counter clear

// ---------------------------------------------------------------------------
// The Apt GC globals (populated at Apt bring-up by AptInit.cpp).
//   gValuesToRelease -- the deferred-release vector pointer (X360 off_8324E51C);
//                       defined in AptGlobals.cpp, declared here.
//   gpGCPoolManager  -- the live-AptValue pool POINTER (X360 off_8324D834);
//                       defined in AptDefine.cpp, declared by AptDefine.h above.
//                       (UNIFIED 2026-08-11: this walk used to run over the
//                       permanently-empty namespace-scope `gAptValueGCPool`
//                       object -- one of three C++ homes for the one console
//                       slot -- so it had never visited a live AptValue. The
//                       console loads the slot as a pointer: @0x82AE4A58
//                       `lis r29, off_8324D834@ha; lwz r3, off_8324D834@l(r29);
//                       bl AptValueGC_PoolManager__GetFirstAptValue`.)
// ---------------------------------------------------------------------------
extern AptValueVector*         gpValuesToRelease;   // off_8324E51C (AptGlobals.cpp)
extern int                     gbAptSavedInputActive;

// ---------------------------------------------------------------------------
// sReferenceRegistrationCb @0x82AD9C80 -- mark-walk callback.
// ---------------------------------------------------------------------------
void* AptGC::sReferenceRegistrationCb(const AptValue* /*pOwner*/, void* pSlot,
                                      const char* /*pDebugName*/, int /*bFlag*/)
{
    AptValue* pValue = *static_cast<AptValue**>(pSlot);
    if (!pValue->getGCMark())
    {
        pValue->setGCMark(true);
        pValue->RegisterReferences();   // recurse: visit the value's own held refs
    }
    return pValue;
}

// ---------------------------------------------------------------------------
// CleanAll @0x82AE4A40 -- tear down every live Apt value at shutdown.
// ---------------------------------------------------------------------------
// The pool-pointer null test below is a PC pre-init guard, NOT console behaviour:
// @0x82AE4A58 / @0x82AE4AC0 the console loads off_8324D834 straight into r3 with no
// test, because on the console the slot is live from AptAllocatorInitialize
// @0x82ADD118 until AptAllocatorShutdown @0x82AE9298 destroys the pool -- and this
// teardown runs strictly between the two (AptUpdateShutdown @0x82B0C170 calls it).
// The PC pool is likewise heap-built by AptAllocatorInitialize and never destroyed
// (AptAllocatorShutdown is un-homed), so the pointer is non-null on every live path
// and the guard changes nothing there; it only covers the window before the Apt
// bring-up ran -- the same guard every GC-value operator new/delete in this tree
// carries, and the one this function already applies to gpValuesToRelease. Each
// walk re-reads the global per step, matching the asm's per-iteration slot reload.
void AptGC::CleanAll()
{
    // 1. Flush anything queued for deferred release.
    if (gpValuesToRelease != nullptr)
        gpValuesToRelease->ReleaseValues();

    // 2. Pre-destroy every live value (drop its GC pointers) with refcount-driven
    //    deletion suspended, so the graph stays walkable while it is dismantled.
    const bool bWasSuspended = AptValue::sbSuspendRefcountDeletions;
    AptValue::sbSuspendRefcountDeletions = true;
    if (gpGCPoolManager != nullptr)
    {
        for (AptValue* pValue = gpGCPoolManager->GetFirstAptValue(); pValue;
             pValue = gpGCPoolManager->GetNextAptValue(pValue))
        {
            pValue->PreDestroy();          // vtbl +0x24
            pValue->DestroyGCPointers();   // vtbl +0x28
        }
    }
    AptValue::sbSuspendRefcountDeletions = bWasSuspended;

    // 3. Flush again (PreDestroy may have queued more), then delete every value
    //    (fetching the next link before deleting the current one).
    if (gpValuesToRelease != nullptr)
        gpValuesToRelease->ReleaseValues();
    if (gpGCPoolManager != nullptr)
    {
        for (AptValue* pValue = gpGCPoolManager->GetFirstAptValue(); pValue; )
        {
            AptValue* pNext = gpGCPoolManager->GetNextAptValue(pValue);
            pValue->DeleteThis();   // vtbl +0x20
            pValue = pNext;
        }
    }

    // 4. Final flush, then clear the value free-lists / temporary string pool.
    if (gpValuesToRelease != nullptr)
        gpValuesToRelease->ReleaseValues();
    AptInteger::ClearPool();
    AptFloat::ClearPool();
    StringPool::ClearTemporaryPool();
}

// ===========================================================================
// The zombie vector (X360 off_8324E528 / XB1 qword_14147A398) -- an AS-visible
// afterlife for externally referenced level movies. When ClearCIH tears down a
// type-9 (animation) node whose zombie count (IncZombieCount -- AS refs against
// a loaded level) is non-zero, the node is NOT freed: its child display list +
// property hash are scrubbed, it is marked CIHState 1 + SetReleaseAtEnd (so
// plain Release() pins it -- see AptCIH::Release), pushed here, and its source
// AptFile's mnState is swapped to 5 (the old state saved in mnField12). The
// reap below walks the vector and finishes the teardown once the zombie count
// drains back to zero (DecZombieCount tails into the reap) or unconditionally
// at shutdown (AptTargetShutdown passes bClear=1).
//
// The vector itself is a family-(B) AptValueVector {capacity@+0 (mnTop member),
// live count@+4 (mnCapacity member), items@+8} allocated by AptUpdateInitialize
// from config word [14] (default 8) -- see AptInit.cpp.
// ===========================================================================
AptValueVector* gpAptZombieVector = nullptr;    // off_8324E528 / qword_14147A398

// byte_8324E38F / XB1 byte_14147A0F2 -- "zombies changed this frame": set by the
// zombie push + every reap that frees one; the per-frame update tail uses it to
// gate the partial-GC sweep (FLAG: that sweep -- XB1 sub_140832E70, the full
// mark/sweep pass -- is a separate un-homed tier; the flag is maintained
// faithfully so the sweep slots straight in when it lands).
bool gbAptZombiesDirty = false;

// dword_8324E8C8 / XB1 qword_14147AB10 -- the host-installed zombie notify hook
// (fired with (bImmediate, 0, instanceName, fileName) when a node zombifies or
// is dropped without a vector slot). Null until a host installs it -- a
// customization hook that defaults OFF on the console too; the null check is
// the shipped form.
void (*gpAptZombieNotifyHook)(int bImmediate, int nReserved,
                              const char* pInstanceName, const char* pFileName) = nullptr;

// ---------------------------------------------------------------------------
// AptUpdateZombieVector -- the zombie reap (XB1 sub_140830A40; the X360 body is
// reached from AptTargetShutdown/DecZombieCount -- same shape). Walk the vector
// BACKWARDS; every entry in CIHState 1 whose zombie count has drained to zero
// (or every entry, when bClear) is compacted out of the vector, and -- when it
// still owns a character instance -- finished: CIHState -> 0, the source
// AptFile's state swapped to 4 (old saved in mnField12), the GC roots dropped,
// DestroyGCPointers run, and the zombies-dirty flag raised. The entry is NOT
// Release()d here -- the value stays SetReleaseAtEnd-pinned for the partial-GC
// sweep to reclaim (matching the shipped reap, which never calls vtbl[+8]).
// Returns null (the X360 canonical void* the callers ignore).
// ---------------------------------------------------------------------------
void* AptUpdateZombieVector(char bClear)
{
    AptValueVector* pVec = gpAptZombieVector;
    if (pVec == nullptr)
        return nullptr;

    // Family-(B) fields: capacity == mnTop (+0), live count == mnCapacity (+4).
    for (int i = pVec->mnCapacity - 1; i >= 0; --i)
    {
        if (pVec->mnCapacity <= i)   // the vector shrank under us (re-entrancy)
            break;

        AptCIH* pZombie = reinterpret_cast<AptCIH*>(pVec->mppItems[i]);
        if (pZombie->GetCIHState() != 1u)
            continue;
        if (!bClear && pZombie->GetZombieCount() > 0)
            continue;

        // Compact the vector (shift the tail down one; clear the vacated top).
        // XB1: --count; if (count && i != count) memmove(items+i, items+i+1,
        // 8*(count-i)); items[count] = 0.
        const int nNewCount = --pVec->mnCapacity;
        if (nNewCount != 0 && i != nNewCount)
            std::memmove(&pVec->mppItems[i], &pVec->mppItems[i + 1],
                         sizeof(AptValue*) * static_cast<size_t>(nNewCount - i));
        pVec->mppItems[nNewCount] = nullptr;

        AptCharacterInst* pInst = pZombie->GetCharacterInst();
        if (pInst != nullptr)
        {
            pZombie->SetCIHState(0);

            // The source .apt file's load state: save the current into the
            // spare slot, then mark it 4 (reaped). XB1: ri=*(inst+72);
            // [+20]=[+16]; [+16]=4 -- inst+72 == mAnimationFilePtr (only
            // type-9 animation insts ever enter the vector).
            AptFile* pFile =
                static_cast<AptCharacterAnimationInst*>(pInst)->mAnimationFilePtr.pData;
            if (pFile != nullptr)
            {
                pFile->mnField12 = pFile->mnState;
                pFile->mnState   = 4;
            }

            pZombie->setGCRoot(0);          // XB1 bitfield &= 0xFF03FFFF (locked)
            pZombie->DestroyGCPointers();   // vtbl[+80]
            gbAptZombiesDirty = true;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// AptFlushDeferredReleases -- the per-opcode / stack-empty deferred-release
// drain the console inlines as ReleaseValues(off_8324E51C) at each call site (the
// AS interpreter opcode handlers + the display-list teardown). Homed here as the
// single de-inlined helper over the real gValuesToRelease vector, retiring the
// AptRenderLinkStubs {} no-op that dropped every drain. Empty-vector safe.
// ---------------------------------------------------------------------------
void AptFlushDeferredReleases()
{
    if (gpValuesToRelease != nullptr)
        gpValuesToRelease->ReleaseValues();
}

// ---------------------------------------------------------------------------
// AptPartialGarbageCollection @0x82ADD2A0 -- the tiny load-complete hook. The
// shipped body only raises byte_8324E38F, the same zombies-dirty flag the zombie
// vector maintenance code raises when a deferred sweep must run.
// ---------------------------------------------------------------------------
void AptPartialGarbageCollection()
{
    gbAptZombiesDirty = true;
}

// ---------------------------------------------------------------------------
// AptFlushInputQueue @0x82ADD270 -- unless saved-input playback is active,
// clear the queued input count on the current target's animation director.
// ---------------------------------------------------------------------------
void AptFlushInputQueue()
{
    if (gbAptSavedInputActive != 0)
        return;

    AptAnimationTarget* pAnimationTarget =
        (gpAptTarget != nullptr) ? gpAptTarget->mpAnimationTarget : nullptr;
    if (pAnimationTarget != nullptr)
        pAnimationTarget->SetQueuedInputsSize(0);
}
