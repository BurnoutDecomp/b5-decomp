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
