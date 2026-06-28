#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"

#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"   // AptValueVector + gpAptOperandStackPool
#include "SDKs/EATech/Apt/DogmaAllocator.h"                    // DOGMA_PoolManager::Deallocate

// AptIntervalTimer member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The compiler-emitted vector deleting destructor @ 0x82AEAA68 is an array
// new[]/delete[] thunk (it walks the 36-byte-stride array calling ~AptIntervalTimer
// and frees via the array cookie) and is intentionally NOT reconstructed, per the
// project convention of dropping deleting-destructor thunks.

// ---- ctor @ 0x82AE2548 ---------------------------------------------------
AptIntervalTimer::AptIntervalTimer()
{
    miId            = 0;
    miFunctionValue = 0;
    mfInterval      = 0.0f;
    mfElapsed       = 0.0f;
    mParams         = AptValueVector::ConstructWithCapacity(6);
    // (+0x10 / +0x20 deliberately left uninitialised, matching the X360 ctor.)
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
