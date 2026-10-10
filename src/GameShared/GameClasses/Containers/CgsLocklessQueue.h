#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "eathread/eathread.h"                       // EA::Thread::ThreadSleep
#include "eathread/eathread_atomic.h"                // EA::Thread::AtomicInt
#include "eathread/eathread_sync.h"                  // EAWriteBarrier

// CgsContainers::LocklessQueue<T> -- a bounded multi-producer / multi-consumer ring of T whose
// whole state lives in one 64-bit status word, changed only by compare-and-swap:
//
//     bits 63..48  read index      (the next slot Pop takes)
//     bits 47..32  write index     (the next slot Post fills)
//     bits 31..16  item count
//     bits 15..0   lock word       (1 while a Post / Pop owns the queue)
//
// Post and Pop take the lock word first (Lock), move the item, swap in the new indices with the
// lock still held, then release it (Unlock). A full Post / empty Pop either gives up or drops the
// lock, sleeps, and retries. Lock itself yields (a zero-millisecond sleep) while another thread
// holds the queue. Layout (CgsLocklessQueue.h): the buffer pointer, the u16 buffer
// size, then the atomic status word. Construct is inlined at its call sites (store the buffer,
// the size, and a zero status).
//
// Readers: BrnGameState::RichPresenceManagerX360's two context queues.
namespace CgsContainers
{
template <typename T>
struct LocklessQueue
{
public:
    void Construct(T* lpBuffer, u16 lu16BufferSize)
    {
        mpBuffer     = lpBuffer;
        muBufferSize = lu16BufferSize;
        mEncodedQueueStatus.SetValue(0);
    }

    // CgsLocklessQueue.h. Append *lpItem. A full queue returns false at once unless
    // lbWaitIfFull, in which case the lock is dropped for liSleepTime ms between attempts.
    bool Post(const T* lpItem, bool lbWaitIfFull, s32 liSleepTime);

    // CgsLocklessQueue.h. Remove the oldest item into *lpItem. An empty queue returns
    // false at once unless lbWaitIfEmpty, as for Post.
    bool Pop(T* lpItem, bool lbWaitIfEmpty, s32 liSleepTime);

private:
    // CgsLocklessQueue.h. Unpack the status word.
    void GetStatus(u16* lpu16ReadIndex, u16* lpu16WriteIndex, u16* lpu16NumItems, u16* lpu16Lock)
    {
        const u64 lu64Status = mEncodedQueueStatus.GetValue();
        *lpu16ReadIndex  = static_cast<u16>(lu64Status >> 48);
        *lpu16WriteIndex = static_cast<u16>(lu64Status >> 32);
        *lpu16NumItems   = static_cast<u16>(lu64Status >> 16);
        *lpu16Lock       = static_cast<u16>(lu64Status);
    }

    // CgsLocklessQueue.h. Swap in the new fields if the word still holds the old ones.
    bool SetStatusConditional(u16 lu16NewReadIndex, u16 lu16NewWriteIndex, u16 lu16NewNumItems, u16 lu16NewLock,
                              u16 lu16OldReadIndex, u16 lu16OldWriteIndex, u16 lu16OldNumItems, u16 lu16OldLock)
    {
        return mEncodedQueueStatus.SetValueConditional(
            EncodeStatus(lu16NewReadIndex, lu16NewWriteIndex, lu16NewNumItems, lu16NewLock),
            EncodeStatus(lu16OldReadIndex, lu16OldWriteIndex, lu16OldNumItems, lu16OldLock));
    }

    static u64 EncodeStatus(u16 lu16ReadIndex, u16 lu16WriteIndex, u16 lu16NumItems, u16 lu16Lock)
    {
        return (static_cast<u64>(lu16ReadIndex) << 48) | (static_cast<u64>(lu16WriteIndex) << 32) |
               (static_cast<u64>(lu16NumItems) << 16) | static_cast<u64>(lu16Lock);
    }

    // CgsLocklessQueue.h. Take the lock word; false (only when !lbWait) if it is held.
    bool Lock(bool lbWait);

    // CgsLocklessQueue.h. Release the lock word (asserts it is held).
    void Unlock();

    T*                               mpBuffer;
    u16                              muBufferSize;
    EA::Thread::AtomicInt<u64>       mEncodedQueueStatus;
};

template <typename T>
bool LocklessQueue<T>::Lock(bool lbWait)
{
    for (;;)
    {
        u16 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock;
        GetStatus(&lu16ReadIndex, &lu16WriteIndex, &lu16NumItems, &lu16Lock);

        if (lu16Lock != 0)
        {
            if (!lbWait)
            {
                return false;
            }
            EA::Thread::ThreadSleep(0);
            continue;
        }

        if (SetStatusConditional(lu16ReadIndex, lu16WriteIndex, lu16NumItems, 1,
                                 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock))
        {
            return true;
        }
    }
}

template <typename T>
void LocklessQueue<T>::Unlock()
{
    for (;;)
    {
        u16 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock;
        GetStatus(&lu16ReadIndex, &lu16WriteIndex, &lu16NumItems, &lu16Lock);
        CGS_ASSERT(lu16Lock == 1, "Not locked\n");

        if (SetStatusConditional(lu16ReadIndex, lu16WriteIndex, lu16NumItems, 0,
                                 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock))
        {
            return;
        }
        CGS_ASSERT(false, "Another thread changed the status while locked by this thread\n");
    }
}

template <typename T>
bool LocklessQueue<T>::Post(const T* lpItem, bool lbWaitIfFull, s32 liSleepTime)
{
    Lock(true);
    for (;;)
    {
        u16 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock;
        GetStatus(&lu16ReadIndex, &lu16WriteIndex, &lu16NumItems, &lu16Lock);

        if (lu16NumItems == muBufferSize)
        {
            if (!lbWaitIfFull)
            {
                Unlock();
                return false;
            }
            Unlock();
            EA::Thread::ThreadSleep(static_cast<EA::Thread::ThreadTime>(liSleepTime));
            Lock(true);
            continue;
        }

        const u16 lu16NewWriteIndex = static_cast<u16>((lu16WriteIndex + 1) % muBufferSize);
        mpBuffer[lu16WriteIndex] = *lpItem;
        EAWriteBarrier();

        if (SetStatusConditional(lu16ReadIndex, lu16NewWriteIndex, static_cast<u16>(lu16NumItems + 1), lu16Lock,
                                 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock))
        {
            break;
        }
        CGS_ASSERT(false, "Another thread changed the status while locked by this thread\n");
    }
    Unlock();
    return true;
}

template <typename T>
bool LocklessQueue<T>::Pop(T* lpItem, bool lbWaitIfEmpty, s32 liSleepTime)
{
    Lock(true);
    for (;;)
    {
        u16 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock;
        GetStatus(&lu16ReadIndex, &lu16WriteIndex, &lu16NumItems, &lu16Lock);

        if (lu16NumItems == 0)
        {
            if (!lbWaitIfEmpty)
            {
                Unlock();
                return false;
            }
            Unlock();
            EA::Thread::ThreadSleep(static_cast<EA::Thread::ThreadTime>(liSleepTime));
            Lock(true);
            continue;
        }

        const u16 lu16NewReadIndex = static_cast<u16>((lu16ReadIndex + 1) % muBufferSize);
        EAWriteBarrier();
        *lpItem = mpBuffer[lu16ReadIndex];

        if (SetStatusConditional(lu16NewReadIndex, lu16WriteIndex, static_cast<u16>(lu16NumItems - 1), lu16Lock,
                                 lu16ReadIndex, lu16WriteIndex, lu16NumItems, lu16Lock))
        {
            break;
        }
        CGS_ASSERT(false, "Another thread changed the status while locked by this thread\n");
    }
    Unlock();
    return true;
}
}
