#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperation.h"  // CgsFileSystem::Operation
#include "eathread/eathread_semaphore.h"
#include <xtl.h>

// CgsFileSystem::OperationPool â€” the lock-guarded, priority-ordered pool of pending
// file-system operations a physical device's worker thread drains. Producers
// (DeviceManager::Open/Read/Write/Close/...) call AddOperation; the device thread calls
// ReadOperation, which blocks on a counting semaphore until an operation is available
// and then returns the highest-priority one (ties broken by the lowest operation number,
// i.e. FIFO within a priority band).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AddOperation  @ 0x828E9828  (EXECUTED in the boot trace)
//   ReadOperation @ 0x828E9960  (EXECUTED in the boot trace)
//
// X360 LAYOUT (asm-authoritative; the DWARF's [32] array sizes are wrong â€” the asm
// iterates and sizes everything to 64):
//   +0x0000  mFutex                (CRITICAL_SECTION-backed lock; asm uses Rtl{Enter,
//                                    Leave}CriticalSection directly on the pool base)
//   +0x0020  mabUsedOperations[64] (bool)   slot-occupied flags
//   +0x0060  mauOperationNumbers[64] (u64)  per-slot FIFO sequence number
//   +0x0260  maOperations[64]      (Operation, 304 bytes each) the operation payloads
//   +0x4E60  muNextOperationNumber (u64)    monotonically-increasing sequence counter
//   +0x4E68  mOperationCount       (EA::Thread::Semaphore) available-operation permits

namespace CgsFileSystem
{
    static const s32 KI_MAX_OPERATIONS = 64;

    // Native Futex platform storage occupies32 bytes; the SDK critical section
    // fits in its first28. The original flag table begins atpool+20hex.
    union OperationFutex
    {
        u64 mData[4];
        CRITICAL_SECTION mCriticalSection;
    };

    CGS_STATIC_ASSERT((sizeof(OperationFutex) == 0x20), "ARTIST operation futex");
    CGS_STATIC_ASSERT((sizeof(Operation) == 0x130), "ARTIST operation stride");
    CGS_STATIC_ASSERT((offsetof(Operation, miPriority) == 0x12C), "ARTIST operation priority");

    struct OperationPool
    {
        void Construct();
        void Destruct();

        // @ 0x828E9828 â€” under the lock, claim the first free slot, copy the operation
        // in, stamp it with the next sequence number, and post one permit. Asserts when
        // muNextOperationNumber wraps to -1; returns false when the pool is full (no free
        // slot), true otherwise.
        bool AddOperation(const Operation* lpOperation);

        // @ 0x828E9960 â€” wait for a permit, then under the lock pick the used slot with
        // the highest priority (lowest sequence number on a tie), copy it out, and free
        // the slot. Returns true on success; the "no operation found" path is an asserted
        // can't-happen that returns false.
        bool ReadOperation(Operation* lpOutOperation);

    private:
        OperationFutex          mFutex;                          // +0x0000
        bool                    mabUsedOperations[KI_MAX_OPERATIONS];   // +0x0020
        u64                     mauOperationNumbers[KI_MAX_OPERATIONS]; // +0x0060
        Operation               maOperations[KI_MAX_OPERATIONS];        // +0x0260
        u64                     muNextOperationNumber;           // +0x4E60
        EA::Thread::Semaphore   mOperationCount;                 // +0x4E68

        void CheckLayout()
        {
            CGS_STATIC_ASSERT((offsetof(OperationPool, mabUsedOperations) == 0x20), "ARTIST operation flags");
            CGS_STATIC_ASSERT((offsetof(OperationPool, mauOperationNumbers) == 0x60), "ARTIST operation sequence table");
            CGS_STATIC_ASSERT((offsetof(OperationPool, maOperations) == 0x260), "ARTIST operations");
            CGS_STATIC_ASSERT((offsetof(OperationPool, muNextOperationNumber) == 0x4E60), "ARTIST operation sequence");
            CGS_STATIC_ASSERT((offsetof(OperationPool, mOperationCount) == 0x4E68), "ARTIST operation semaphore");
        }
    };
}
