#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"

#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"   // AptValueVector + gpAptOperandStackPool
#include "SDKs/EATech/Apt/DogmaAllocator.h"                    // DOGMA_PoolManager::Deallocate

// AptIntervalTimer member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.

#include <cstdint>   // intptr_t for the array-cookie byte arithmetic

// The shared Apt fixed-size pool (X360 off_8324D808) the timer array is allocated
// from / freed back to (the same single pool gpAptOperandStackPool aliases). The
// array deleting destructor frees its pool block through this handle.
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// ---- ctor @ 0x82AE2548 ---------------------------------------------------
AptIntervalTimer::AptIntervalTimer()
{
    // The X360 ctor zeroes the +0x00 slot gate and the +0x04 callback value (two
    // int32 stores), zeroes the two floats, then constructs the param stack. It
    // does NOT touch mpContext(+0x10) or miId(+0x20) -- the setInterval setup path
    // fills those.
    mpActiveValue   = nullptr;   // *a1     = 0
    mpCBFunction    = nullptr;   // *(a1+4) = 0
    mfInterval      = 0.0f;      // *(a1+8) = 0.0
    mfElapsed       = 0.0f;      // *(a1+12)= 0.0
    mParams         = AptValueVector::ConstructWithCapacity(6);
}

// ---- CleanParams @ 0x82ADF120 --------------------------------------------
// Pop (and Release) every value currently on the param stack. The X360 snapshots
// the live count once and pops exactly that many times (pop() also decrements the
// count), leaving the stack empty.
void AptIntervalTimer::CleanParams()
{
    for (s32 liRemaining = mParams.mnTop; liRemaining > 0; --liRemaining)
    {
        mParams.pop();
    }
}

// ---- ~AptIntervalTimer @ 0x82AE25A0 --------------------------------------
// Empty the param stack, then free its backing slot array (the inlined free half
// of AptValueVector::Shutdown -- the X360 dtor does not bother re-zeroing the
// members of an object that is being destroyed).
AptIntervalTimer::~AptIntervalTimer()
{
    CleanParams();

    if (mParams.mppItems != nullptr)
    {
        gpAptOperandStackPool->Deallocate(mParams.mppItems, 4 * mParams.mnCapacity);
    }
}

// ---- GenerateId @ 0x82ADF170 ---------------------------------------------
// Next process-unique interval id. The X360 lazily zero-initialises a module
// counter (function-local-static init guard) and returns an atomic pre-increment.
s32 AptIntervalTimer::GenerateId()
{
    static s32 siNextId = 0;   // X360: module counter, atomically incremented
    return ++siNextId;
}

// ---- `vector deleting destructor' @ 0x82AEAA68 ---------------------------
// The MSVC array deleting destructor. nFlags bit1 selects the array form; the
// element count is the dword immediately before the array (the new[] cookie), the
// 36-byte-stride elements are destroyed high index -> low, and (bit0) the pool
// block -- which begins one dword before that count, sized cookie+4 -- is freed.
// The non-array branch destroys a single element and (bit0) frees its 36 bytes.
//
// FLAG (x64 widening): the X360 cookie/stride are console 4-byte/36-byte; on PC
// AptIntervalTimer is wider (8-byte AptValue*/AptValueVector), so the genuine PC
// new[] cookie + element stride differ from the console 36. We honour the X360's
// SEMANTICS (count cookie one dword ahead; destroy every element; free the block)
// using sizeof(AptIntervalTimer) for the stride and the count dword the parent's
// allocation wrote, rather than the literal console 36 -- correct per the
// project's semantic-parity-by-named-members rule.
void* AptIntervalTimer::_vector_deleting_destructor_(AptIntervalTimer* pArray, char nFlags)
{
    if ((nFlags & 2) != 0)
    {
        // Array form. The count dword sits at pArray[-1] (the new[] cookie); the
        // pool block starts one dword before it.
        s32* lpCount = reinterpret_cast<s32*>(pArray) - 1;
        const s32 liCount = *lpCount;

        for (s32 liIndex = liCount - 1; liIndex >= 0; --liIndex)
        {
            pArray[liIndex].~AptIntervalTimer();
        }

        if ((nFlags & 1) != 0)
        {
            // X360: Deallocate(off_8324D808, &count - 1, count + 4) -- the block is
            // the count cookie (one dword) preceded by the pool's own size dword.
            s32* lpBlock = lpCount - 1;
            gpAptPseudoDataPool->Deallocate(lpBlock, static_cast<u32>(liCount) + 4u);
        }
        return lpCount;
    }

    pArray->~AptIntervalTimer();
    if ((nFlags & 1) != 0)
    {
        gpAptPseudoDataPool->Deallocate(pArray, sizeof(AptIntervalTimer));
    }
    return pArray;
}

// ===========================================================================
// The interval-table impls (HOMED 2026-07-02, retiring the AptRenderLinkStubs
// nulls). The entry layer (AptActionInterpreter::cbCallMethod_setInterval /
// _clearInterval) reads the guard args; these do the table work.
// ===========================================================================
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // the timer table owner
#include "SDKs/EATech/include/Apt/AptTarget.h"                // gpAptTarget
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"      // the returned id value
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"       // the method-name form

extern AptValue** gppAptNativeArgStack;   // off_8324E768 (the operand-stack items)
extern int32_t    gnAptNativeArgCount;    // dword_8324E760 (the live count)
extern AptValue*  gpUndefinedValue;       // off_8324D814

// ---------------------------------------------------------------------------
// AptActionInterpreter_SetIntervalImpl -- the body of cbCallMethod_setInterval
// @0x82B019D8 past the defined-guard. Claim the first free AptIntervalTimer
// slot (mpActiveValue == null); the claim stores the literal-1 SENTINEL into
// the gate (the X360 `stw 1` -- RemoveTimerFunctions/clearInterval only test
// null/non-null) and seeds mpContext = undefined. Two argument forms:
//   * a callable top (script function 34..36 or a native function, defined):
//     fn = top, period = arg[count-2], 2 args consumed;
//   * otherwise object+method: name = arg[count-2] (its embedded EAStringC,
//     the same +8 idiom as gotoAndX), fn = top->findChild(name), mpContext =
//     the object, period = arg[count-3], 3 consumed. The asm binds the
//     findChild result unguarded (a console invariant: the method exists).
// Both arms AddRef the bound fn + context, store toFloat(period) into
// mfInterval AND mfElapsed (the countdown seed), stamp the id, then push each
// trailing arg (top-down) into mParams with an AddRef. Returns the new
// AptInteger id, or the undefined singleton when the table has no free slot.
// ---------------------------------------------------------------------------
AptValue* AptActionInterpreter_SetIntervalImpl(AptValue* pCallback, int nArgCount)
{
    const s32 nId = AptIntervalTimer::GenerateId();

    AptAnimationTarget* const pDir = gpAptTarget->mpAnimationTarget;
    u32 iSlot = 0;
    for (; iSlot < pDir->mnNumIntervalTimers; ++iSlot)
        if (pDir->mpIntervalTimers[iSlot].mpActiveValue == nullptr)
            break;
    if (iSlot == pDir->mnNumIntervalTimers)
        return gpUndefinedValue;   // table full (the X360 tail check)

    AptIntervalTimer& rTimer = pDir->mpIntervalTimers[iSlot];
    rTimer.mpActiveValue = reinterpret_cast<AptValue*>(1);   // the claim sentinel
    rTimer.mpContext     = gpUndefinedValue;

    AptValue* pFn       = pCallback;
    AptValue* pInterval = gppAptNativeArgStack[gnAptNativeArgCount - 2];
    int       nConsumed = 2;

    const AptVirtualFunctionTable_Indices eType = pCallback->getVtblIndex();
    const bool bCallable =
        ((static_cast<u32>(eType) - AptVFT_ScriptFunction1) <= 2u
            && pCallback->getIsDefined())
        || (eType == AptVFT_NativeFunction && pCallback->getIsDefined());
    if (!bCallable)
    {
        // The object + method-name form.
        AptString* const pName =
            static_cast<AptString*>(gppAptNativeArgStack[gnAptNativeArgCount - 2]);
        pInterval = gppAptNativeArgStack[gnAptNativeArgCount - 3];
        pFn = pCallback->findChild(pName->GetInternalString(), nullptr);
        rTimer.mpContext = pCallback;
        nConsumed = 3;
    }

    rTimer.mpCBFunction = pFn;
    rTimer.mpCBFunction->AddRef();
    rTimer.mpContext->AddRef();

    const f32 fPeriod = pInterval->toFloat();
    rTimer.mfInterval = fPeriod;
    rTimer.mfElapsed  = fPeriod;
    rTimer.miId       = nId;

    // The trailing args, top-down, each AddRef'd into the param stack.
    for (int j = 0; j < nArgCount - nConsumed; ++j)
    {
        AptValue* const pArg =
            gppAptNativeArgStack[gnAptNativeArgCount - j - nConsumed - 1];
        rTimer.mParams.mppItems[rTimer.mParams.mnTop++] = pArg;
        pArg->AddRef();
    }

    return AptInteger::Create(nId);
}

// ---------------------------------------------------------------------------
// AptActionInterpreter_ClearIntervalImpl -- the body of cbCallMethod_
// clearInterval @0x82AE3AE0 past the guard: scan the WHOLE table (no early
// break); every armed slot whose id matches is torn down -- Release the
// callback (vtbl[1]), Release the context when non-null, clear the gate,
// CleanParams (Release + pop every queued param), and zero the id.
// ---------------------------------------------------------------------------
void AptActionInterpreter_ClearIntervalImpl(int nId)
{
    AptAnimationTarget* const pDir = gpAptTarget->mpAnimationTarget;
    for (u32 i = 0; i < pDir->mnNumIntervalTimers; ++i)
    {
        AptIntervalTimer& rTimer = pDir->mpIntervalTimers[i];
        if (rTimer.mpActiveValue == nullptr || rTimer.miId != nId)
            continue;
        rTimer.mpCBFunction->Release();
        if (rTimer.mpContext != nullptr)
            rTimer.mpContext->Release();
        rTimer.mpActiveValue = nullptr;
        rTimer.CleanParams();
        rTimer.miId = 0;
    }
}
