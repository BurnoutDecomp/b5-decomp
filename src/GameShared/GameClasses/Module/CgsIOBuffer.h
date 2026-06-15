#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFlagSet.h"

// CgsModule::IOBuffer - the base for the per-frame IO payload buffers modules exchange. It
// guards access with a status flag set: a buffer must be Construct()ed before use, and a
// reader/writer takes a non-exclusive read lock or an exclusive write lock (read and write
// are mutually exclusive). The producing module LockForWrite()s its output buffer while
// filling it; consumers LockForRead() it. Layout + flag semantics recovered from the DecFIGS
// DWARF + the X360 LockForWrite/Read bodies (status bits: 0=constructed, 3=write-locked,
// 4=read-locked). Recovered from Module/CgsIOBuffer.h.
namespace CgsModule
{
    struct IOBuffer
    {
        typedef CgsContainers::FlagSet<s8> FlagSet8;

        // Status flag masks (the X360 build tests bits 0/3/4; bits 1-2 are reserved status).
        enum EStatus
        {
            eStatusConstructed    = 0x01,
            eStatusReserved1      = 0x02,
            eStatusReserved2      = 0x04,
            eStatusLockedForWrite = 0x08,
            eStatusLockedForRead  = 0x10,
        };

        void Construct();
        void Destruct();
        void LockForWrite();
        void UnlockForWrite();
        void LockForRead() const;
        void UnlockForRead() const;

        void* operator new(size_t luSize)            { return ::operator new(luSize); }
        void  operator delete(void* lpMem)           { ::operator delete(lpMem); }
        void* operator new(size_t, void* lpMem)      { return lpMem; }
        void  operator delete(void*, void*)          {}

    protected:
        bool IsBufferLockedForReading() const { return mxStatusFlags.IsBitSet(eStatusLockedForRead); }
        bool IsBufferLockedForWriting() const { return mxStatusFlags.IsBitSet(eStatusLockedForWrite); }
        bool IsBufferLocked() const           { return mxStatusFlags.IsBitSet(eStatusLockedForRead | eStatusLockedForWrite); }

    private:
        // LockForRead/UnlockForRead are const but toggle the read-lock bit -> mutable.
        mutable FlagSet8 mxStatusFlags;
    };
}
