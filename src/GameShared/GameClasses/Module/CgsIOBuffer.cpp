#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsModule::IOBuffer - read/write lock state machine over the status flags. Each Lock asserts
// the legal preconditions (constructed; not already locked the conflicting way) then sets its
// lock bit; each Unlock clears it. Bodies recovered from the X360
// LockForWrite/UnlockForWrite/LockForRead/UnlockForRead (CgsIOBuffer.h:217-219 et al).
namespace CgsModule
{
    // @ CgsIOBuffer.h:61 - mark the buffer constructed (clears all other status bits).
    void IOBuffer::Construct()
    {
        mxStatusFlags.Clear();
        mxStatusFlags.SetBit(eStatusConstructed);
    }

    // @ CgsIOBuffer.h:65
    void IOBuffer::Destruct()
    {
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed), "mxStatusFlags.IsBitSet( eStatusConstructed )");
        mxStatusFlags.Clear();
    }

    // @ 0x82204A18 - take the exclusive write lock (must be constructed + not read/write locked).
    void IOBuffer::LockForWrite()
    {
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed),     "mxStatusFlags.IsBitSet( eStatusConstructed )");
        CGS_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForRead),  "Already locked for read");
        CGS_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForWrite), "Already locked for write");
        mxStatusFlags.SetBit(eStatusLockedForWrite);
    }

    // @ 0x82204BB8 - release the write lock.
    void IOBuffer::UnlockForWrite()
    {
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed),    "mxStatusFlags.IsBitSet( eStatusConstructed )");
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusLockedForWrite), "Not locked for write");
        mxStatusFlags.UnSetBit(eStatusLockedForWrite);
    }

    // @ CgsIOBuffer.h:77 - take a (shared) read lock (must be constructed + not write locked).
    void IOBuffer::LockForRead() const
    {
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed),     "mxStatusFlags.IsBitSet( eStatusConstructed )");
        CGS_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForWrite), "Already locked for write");
        mxStatusFlags.SetBit(eStatusLockedForRead);
    }

    // @ CgsIOBuffer.h:81 - release the read lock.
    void IOBuffer::UnlockForRead() const
    {
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed),    "mxStatusFlags.IsBitSet( eStatusConstructed )");
        CGS_ASSERT(mxStatusFlags.IsBitSet(eStatusLockedForRead),  "Not locked for read");
        mxStatusFlags.UnSetBit(eStatusLockedForRead);
    }
}
