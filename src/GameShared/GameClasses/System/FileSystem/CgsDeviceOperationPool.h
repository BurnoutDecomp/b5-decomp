#pragma once

#include "types.hpp"
// Forked-vendor EA::Thread::Semaphore (mOperationCount). Per the eathread-strategy decision
// the FileSystem device layer links the (forked) vendor EAThread, not the reconstructed X360
// set; the vendor Semaphore carries an added X360-aligned Wait(const u32*) overload that
// ReadOperation calls. (Was SDKs/EATech/eathread/eathread_semaphore.h — now reference-only.)
#include "eathread/eathread_semaphore.h"   // EA::Thread::Semaphore (mOperationCount)

// CgsFileSystem::OperationPool — the lock-guarded, priority-ordered pool of pending
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
// X360 LAYOUT (asm-authoritative; the DWARF's [32] array sizes are wrong — the asm
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

    // One queued file-system operation. The pool moves it around whole (a 304-byte
    // memcpy) and the scheduler only inspects its priority field (+0x12C / +300). The
    // full Operation layout is owned by the device layer (CgsDevice.h); modelled here as
    // an opaque 304-byte payload with a named priority accessor at the asm-attested
    // offset so the pool can pick the highest-priority operation without depending on
    // that layout.
    struct Operation
    {
        s32 GetPriority() const
        {
            return *reinterpret_cast<const s32*>(
                reinterpret_cast<const u8*>(this) + 0x12C);
        }

        u8 maPayload[304];
    };

    // Opaque storage for the embedded OS critical section (the Futex's lock). The X360
    // RTL_CRITICAL_SECTION is 28 bytes; a host-sized reservation keeps the pool
    // self-contained without pulling in <windows.h>. Touched only by name via the Rtl
    // hooks (declared in the .cpp). It sits first so RtlEnterCriticalSection(&mFutex)
    // matches the X360's RtlEnterCriticalSection(poolBase).
    struct OperationFutex
    {
        u8 maCriticalSection[64];
    };

    struct OperationPool
    {
        void Construct();
        void Destruct();

        // @ 0x828E9828 — under the lock, claim the first free slot, copy the operation
        // in, stamp it with the next sequence number, and post one permit. Asserts when
        // muNextOperationNumber wraps to -1; returns false when the pool is full (no free
        // slot), true otherwise.
        bool AddOperation(const Operation* lpOperation);

        // @ 0x828E9960 — wait for a permit, then under the lock pick the used slot with
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
    };
}
