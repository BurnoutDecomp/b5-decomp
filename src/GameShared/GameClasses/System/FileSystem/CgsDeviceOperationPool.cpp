#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperationPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CgsFileSystem::OperationPool::AddOperation  @ 0x828E9828
//   CgsFileSystem::OperationPool::ReadOperation @ 0x828E9960

// OS critical-section hooks (FLAGGED platform APIs). The X360 build calls the NT
// Rtl* critical-section primitives directly on the pool's embedded section (which the
// Futex wraps). Declared extern here, supplied by the platform layer; mirrors the
// MassiveAd precedent of wrapping the OS critical section by name rather than pulling
// in <windows.h>.
extern "C" void RtlEnterCriticalSection(void* lpCriticalSection);
extern "C" void RtlLeaveCriticalSection(void* lpCriticalSection);
extern "C" long RtlInitializeCriticalSection(void* lpCriticalSection);
extern "C" long RtlDeleteCriticalSection(void* lpCriticalSection);

namespace CgsFileSystem
{
    // Construct/Destruct — the X360 split this pool's bring-up between the DeviceManager ctor
    // (RtlInitializeCriticalSection on mFutex + the Semaphore subobject ctor) and Add-
    // PhysicalDevice (zero the used-flags + Semaphore::Init count 0). Here it is consolidated:
    // init the lock, clear the slot table + sequence counter. mOperationCount (the vendor
    // EA::Thread::Semaphore subobject) is created count-0 by its own member ctor.
    void OperationPool::Construct()
    {
        RtlInitializeCriticalSection(&mFutex);
        for (s32 liIndex = 0; liIndex < KI_MAX_OPERATIONS; ++liIndex)
            mabUsedOperations[liIndex] = false;
        muNextOperationNumber = 0;
    }

    void OperationPool::Destruct()
    {
        RtlDeleteCriticalSection(&mFutex);
    }

    // The absolute-deadline argument the X360 hands Semaphore::Wait (unk_820F6FD8). A
    // blocking ReadOperation must wait until an operation is posted, so this is the
    // wait-forever (INFINITE) sentinel the Semaphore::Wait contract documents as -1.
    static const u32 KU_WAIT_FOREVER = 0xFFFFFFFFu;

    // @ 0x828E9828
    bool OperationPool::AddOperation(const Operation* lpOperation)
    {
        RtlEnterCriticalSection(&mFutex);

        // Sequence-counter exhaustion guard (asm: ld @0x4E60 compared to (u64)-1).
        CGS_ASSERT(muNextOperationNumber != static_cast<u64>(-1),
                   "Out of operations - sounds crazy, means we must have done a million billion ticks...\n");

        // Find the first free slot.
        s32 liIndex = 0;
        while (mabUsedOperations[liIndex])
        {
            if (++liIndex >= KI_MAX_OPERATIONS)
            {
                break;
            }
        }

        if (liIndex < KI_MAX_OPERATIONS)
        {
            memcpy(&maOperations[liIndex], lpOperation, sizeof(Operation));
            mabUsedOperations[liIndex]   = true;
            mauOperationNumbers[liIndex] = muNextOperationNumber;
            ++muNextOperationNumber;
        }

        RtlLeaveCriticalSection(&mFutex);

        if (liIndex >= KI_MAX_OPERATIONS)
        {
            return false;   // pool full
        }

        mOperationCount.Post(1);
        return true;
    }

    // @ 0x828E9960
    bool OperationPool::ReadOperation(Operation* lpOutOperation)
    {
        // Block until an operation is available.
        mOperationCount.Wait(&KU_WAIT_FOREVER);

        RtlEnterCriticalSection(&mFutex);

        // Pick the used slot with the highest priority; break ties by the lowest
        // operation number (FIFO within a priority band).
        s32 liBestIndex     = -1;
        s32 liBestPriority  = 0;
        u64 luBestNumber    = 0;

        for (s32 liIndex = 0; liIndex < KI_MAX_OPERATIONS; ++liIndex)
        {
            if (!mabUsedOperations[liIndex])
            {
                continue;
            }

            const s32 liPriority = maOperations[liIndex].GetPriority();
            const u64 luNumber   = mauOperationNumbers[liIndex];

            if (liBestIndex < 0)
            {
                liBestPriority = liPriority;
                luBestNumber   = luNumber;
                liBestIndex    = liIndex;
            }
            else if (liPriority > liBestPriority)
            {
                luBestNumber   = luNumber;
                liBestPriority = liPriority;
                liBestIndex    = liIndex;
            }
            else if (liPriority == liBestPriority)
            {
                if (luNumber < luBestNumber)
                {
                    luBestNumber   = luNumber;
                    liBestPriority = liPriority;
                    liBestIndex    = liIndex;
                }
            }
        }

        if (liBestIndex < 0)
        {
            // Asserted can't-happen: the semaphore guarantees an operation exists.
            CGS_ASSERT(false,
                       "A lockable operation queue should never fail to find an operation - something dodgy is going on\n");
            muNextOperationNumber = 0;
            RtlLeaveCriticalSection(&mFutex);
            return false;
        }

        memcpy(lpOutOperation, &maOperations[liBestIndex], sizeof(Operation));
        mabUsedOperations[liBestIndex] = false;
        RtlLeaveCriticalSection(&mFutex);
        return true;
    }
}
