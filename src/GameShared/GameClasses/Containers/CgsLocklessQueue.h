#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CgsDev::Assert::BeginAssert/FireAssert/EndAssert
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"         // EA::Thread::ThreadSleep

// CgsLocklessQueue<T>: bounded lock-free MPMC queue.
//
// X360 implementation: outer spinlock (unnamed ___ acquire/sub_82360400 release) +
// inner hardware CAS (mfmsr/mtmsree/ldarx/stdcx. via PPC Memory Extension registers).
// The 64-bit control word at &muCapacity packs capacity (hi32) and write-head
// version+position (lo32). Assert fires at CgsLocklessQueue.h:349 if a concurrent
// writer corrupts the lock invariant.
//
// PC translation: direct word stores (single-threaded compile gate); the lock/CAS
// are elided since PC builds are single-producer or caller-serialised. Semantics
// preserved: push returns 1 on success, 0 when full and non-blocking.
//
// Layout from X360 ARTIST pseudocode (int* a1):
//   +0x00  mpData       *a1        pointer to the element array (T* on X360 = 4 bytes)
//   +0x04  muPad        *(a1+1)   (not accessed in push)
//   +0x08  muCapacity   *(a1+2)   max element count  [high 32-bit of 64-bit CAS word]
//   +0x0C  muCtrl       a1[3]     packed {count:hi16, write_pos:lo16}  [low 32-bit of CAS word]
//
// push() @ X360: *>::  @ 0x82369160   source: CgsLocklessQueue.h:349
template<typename T>
class CgsLocklessQueue
{
public:
    // Push *lpItem to the queue. Returns 1 on success.
    // If the queue is full and lbBlocking is false, returns 0 immediately.
    // If the queue is full and lbBlocking is true, sleeps liSleepMs ms and retries.
    int push(const T* lpItem, bool lbBlocking, u32 luSleepMs);

private:
    T*   mpData;        // pointer to element array (capacity * sizeof(T) bytes)
    u32  muPad;         // not accessed in push()
    u32  muCapacity;    // max elements; X360 CAS: high 32-bit of 64-bit ctrl QWORD
    u32  muCtrl;        // hi16=current count, lo16=write position; low 32-bit of CAS QWORD
};

// ---------------------------------------------------------------------------
// CgsLocklessQueue<T>::push -- see class header for description.
// Reconstructed from X360 ARTIST @ 0x82369160.
// ---------------------------------------------------------------------------
template<typename T>
inline int CgsLocklessQueue<T>::push(const T* lpItem, bool lbBlocking, u32 luSleepMs)
{
    for (;;)
    {
        // Read current queue state.
        // X360: ldarx (64-bit) read of {muCapacity, muCtrl} as one atomic QWORD;
        //       PC: two separate reads (single-threaded gate, no race).
        u32 luCap   = muCapacity;
        u32 luCtrl  = muCtrl;
        u32 luCount = luCtrl >> 16u;        // hi16: current occupancy (X360 HIDWORD(v12))
        u32 luPos   = luCtrl & 0xFFFFu;     // lo16: current write position (X360 v12 & mask)

        if (luCount != luCap)               // X360: HIDWORD(v12) != v10 → queue has space
        {
            // Write item at the current write position.
            // X360: __twllei(v10, 0) asserts capacity > 0; __lwsync() provides barrier.
            mpData[luPos] = *lpItem;

            // Advance write head: new pos wraps at capacity, count increments by one.
            // X360: ldarx/stdcx. CAS loop; on CAS failure the following assert fires.
            u32 luNewPos  = (luPos + 1u < luCap) ? (luPos + 1u) : 0u;
            u32 luNewCtrl = ((luCount + 1u) << 16u) | luNewPos;

            // CAS: verify the ctrl word has not changed under us (X360 ldarx/stdcx.).
            // On PC this is a non-atomic compare; single-threaded callers never race here.
            if (muCtrl == luCtrl)
            {
                muCtrl = luNewCtrl;
            }
            else
            {
                // X360: fires when another thread modified the QWORD between ldarx and stdcx.
                // Path: CgsLocklessQueue.h line 349.
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "Another thread changed the status while locked by this thread\n",
                    "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsLocklessQueue.h",
                    349);
                CgsDev::Assert::EndAssert();
            }
            return 1;
        }

        // Queue is full.
        if (!lbBlocking)
            return 0;

        // Blocking: relinquish and sleep before retrying.
        // X360: sub_82360400(unlock) → EA::Thread::ThreadSleep → ___(relock).
        EA::Thread::ThreadSleep(&luSleepMs);
    }
}
