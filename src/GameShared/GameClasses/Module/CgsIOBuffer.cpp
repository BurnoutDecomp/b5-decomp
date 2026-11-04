#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#define IO_ASSERT(condition, message, line) do { if (!(condition)) { \
    CgsDev::Assert::BeginAssert(); \
    CgsDev::Assert::FireAssert(message, "..\\..\\..\\GameShared\\GameClasses\\Module/CgsIOBuffer.h", line); \
    CgsDev::Assert::EndAssert(); \
} } while (0)

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
        IO_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForRead) && !mxStatusFlags.IsBitSet(eStatusLockedForWrite), "!IsBufferLocked()", 157);
        mxStatusFlags.Clear();
    }

    // @ 0x82204A18 - take the exclusive write lock (must be constructed + not read/write locked).
    void IOBuffer::LockForWrite()
    {
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed), "mxStatusFlags.IsBitSet( eStatusConstructed )", 217);
        if (mxStatusFlags.IsBitSet(eStatusLockedForRead))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream stream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            stream << "Already locked for read, flags: " << CgsDev::E_PRINTMODE_HEXONCE << static_cast<s32>(mxStatusFlags.GetFlags()) << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, "..\\..\\..\\GameShared\\GameClasses\\Module/CgsIOBuffer.h", 218);
            CgsDev::Assert::EndAssert();
        }
        if (mxStatusFlags.IsBitSet(eStatusLockedForWrite))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream stream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            stream << "Already locked for write, flags: " << CgsDev::E_PRINTMODE_HEXONCE << static_cast<s32>(mxStatusFlags.GetFlags()) << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, "..\\..\\..\\GameShared\\GameClasses\\Module/CgsIOBuffer.h", 219);
            CgsDev::Assert::EndAssert();
        }
        mxStatusFlags.SetBit(eStatusLockedForWrite);
    }

    // @ 0x82204BB8 - release the write lock.
    void IOBuffer::UnlockForWrite()
    {
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed), "mxStatusFlags.IsBitSet( eStatusConstructed )", 232);
        IO_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForRead), "!mxStatusFlags.IsBitSet( eStatusLockedForRead )", 233);
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusLockedForWrite), "mxStatusFlags.IsBitSet( eStatusLockedForWrite )", 234);
        mxStatusFlags.UnSetBit(eStatusLockedForWrite);
    }

    // @ CgsIOBuffer.h:77 - take a (shared) read lock (must be constructed + not write locked).
    void IOBuffer::LockForRead() const
    {
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed), "mxStatusFlags.IsBitSet( eStatusConstructed )", 247);
        IO_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForRead), "!mxStatusFlags.IsBitSet( eStatusLockedForRead )", 248);
        IO_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForWrite), "!mxStatusFlags.IsBitSet( eStatusLockedForWrite )", 249);
        mxStatusFlags.SetBit(eStatusLockedForRead);
    }

    // @ CgsIOBuffer.h:81 - release the read lock.
    void IOBuffer::UnlockForRead() const
    {
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusConstructed), "mxStatusFlags.IsBitSet( eStatusConstructed )", 262);
        IO_ASSERT(mxStatusFlags.IsBitSet(eStatusLockedForRead), "mxStatusFlags.IsBitSet( eStatusLockedForRead )", 263);
        IO_ASSERT(!mxStatusFlags.IsBitSet(eStatusLockedForWrite), "!mxStatusFlags.IsBitSet( eStatusLockedForWrite )", 264);
        mxStatusFlags.UnSetBit(eStatusLockedForRead);
    }
}
