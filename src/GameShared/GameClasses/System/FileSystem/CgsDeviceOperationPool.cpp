#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperationPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <cstring>   // memcpy

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CgsFileSystem::OperationPool::AddOperation  @ 0x828E9828
//   CgsFileSystem::OperationPool::ReadOperation @ 0x828E9960

// Use the native SDK critical-section declarations.
namespace CgsFileSystem
{
    // The X360 DeviceManager ctor constructed semaphore data with bInitialize=false; this typed
    // member ctor preserves that uninitialized-handle state. Construct initializes the pool lock
    // and sequence counter; AddPhysicalDevice clears used flags and creates the OS semaphore.
    OperationPool::OperationPool()
        : mOperationCount(0, false)
    {
    }

    void OperationPool::Construct()
    {
        RtlInitializeCriticalSection(&mFutex.mCriticalSection);
        muNextOperationNumber = 0;
    }

    void OperationPool::Destruct()
    {
        RtlDeleteCriticalSection(&mFutex.mCriticalSection);
    }

    // ARTIST 0x828F9320, before the slot is published and its worker starts. Init's bool result
    // is intentionally ignored just as in the native AddPhysicalDevice body.
    void OperationPool::InitializeForDevice()
    {
        RtlEnterCriticalSection(&mFutex.mCriticalSection);
        for (s32 liIndex = 0; liIndex < KI_MAX_OPERATIONS; ++liIndex)
            mabUsedOperations[liIndex] = false;

        EA::Thread::SemaphoreParameters lParameters(0, true, 0);
        (void)mOperationCount.Init(&lParameters);

        RtlLeaveCriticalSection(&mFutex.mCriticalSection);
    }

    // The absolute-deadline argument the X360 hands Semaphore::Wait (unk_820F6FD8). A
    // blocking ReadOperation must wait until an operation is posted, so this is the
    // wait-forever (INFINITE) sentinel the Semaphore::Wait contract documents as -1.
    static const u32 KU_WAIT_FOREVER = 0xFFFFFFFFu;

#define CGS_ARTIST_OPERATION_POOL_FILE \
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceOperationPool.cpp"

    // @ 0x828E9828
    bool OperationPool::AddOperation(const Operation* lpOperation)
    {
        RtlEnterCriticalSection(&mFutex.mCriticalSection);

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

        RtlLeaveCriticalSection(&mFutex.mCriticalSection);

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

        RtlEnterCriticalSection(&mFutex.mCriticalSection);

        s32 liBestIndex = -1;
        s32 liBestPriority = 0;
        u64 luBestNumber = 0;

        for (s32 liIndex = 0; liIndex < KI_MAX_OPERATIONS; ++liIndex)
        {
            if (mabUsedOperations[liIndex])
            {
                if (liBestIndex < 0)
                {
                    liBestPriority = maOperations[liIndex].GetPriority();
                    luBestNumber = mauOperationNumbers[liIndex];
                    liBestIndex = liIndex;
                }
                else
                {
                    const s32 liPriority = maOperations[liIndex].GetPriority();
                    if (liPriority > liBestPriority)
                    {
                        luBestNumber = mauOperationNumbers[liIndex];
                        liBestPriority = liPriority;
                        liBestIndex = liIndex;
                    }
                    else if (liPriority == liBestPriority)
                    {
                        const u64 luNumber = mauOperationNumbers[liIndex];
                        if (luNumber < luBestNumber)
                        {
                            luBestNumber = luNumber;
                            liBestPriority = liPriority;
                            liBestIndex = liIndex;
                        }
                    }
                }
            }
        }

        if (liBestIndex >= 0)
        {
            memcpy(lpOutOperation, &maOperations[liBestIndex], sizeof(Operation));
            mabUsedOperations[liBestIndex] = false;
            RtlLeaveCriticalSection(&mFutex.mCriticalSection);
            return true;
        }

        CGS_ASSERT_STREAM_AT(false,
            "A lockable operation queue should never fail to find an operation - something dodgy is going on\n",
            CGS_ARTIST_OPERATION_POOL_FILE, 210);
        muNextOperationNumber = 0;
        RtlLeaveCriticalSection(&mFutex.mCriticalSection);
        return false;
    }
}

#undef CGS_ARTIST_OPERATION_POOL_FILE
