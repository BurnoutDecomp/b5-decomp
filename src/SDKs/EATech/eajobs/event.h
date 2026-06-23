#ifndef EA_JOBS_EVENT_H
#define EA_JOBS_EVENT_H

#include "types.hpp"
#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator

// EA::Jobs::Event + EA::Jobs::Detail::BucketListNode<Event,N>
//
// EA Tech "job_manager" SDK vendor code, reconstructed from the X360 .XEX and the
// DWARF surface in references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/
// (event.h + detail/bucket_list_node.h). Members/types follow EA's canonical SDK
// convention (mBucket/mNext/mSize, EVENT_* enumerators), NOT the Brn/Cgs project
// convention -- this is vendor code reconstructed in its canonical home.
//
// EA::Jobs::Event @ 0x82BC98B8 (Event::Run).
// BucketListNode<Event,10> @ 0x82BCAFC0 (scalar deleting destructor).
// BucketListNode<Event,16> @ 0x82BCADD0 (Add), 0x82BCB040 (scalar deleting destructor).

namespace EA
{
namespace Jobs
{
    // EA::Jobs::Event -- a one-shot side effect run when a job phase fires. On Run
    // it optionally decrements a shared counter (atomically) and, once that counter
    // hits zero (or if there is no counter), performs its action: invoke a callback,
    // or write a 32-bit value to a location.
    //
    // X360 layout (event.h DWARF + Event::Run @ 0x82BC98B8 asm, store order EXACT):
    //   +0x0  mType                  (Type)
    //   +0x4  union { mWriteValue / mWritePtrValue / mCallback }
    //   +0x8  union { mWriteLocation / mWritePtrLocation / mContext }
    //   +0xC  mDecrementerLocation   (uint32_t*)
    // sizeof == 16 bytes (matches the 16-byte stride in BucketListNode<Event,N>).
    struct Event
    {
        // event.h:28 -- which phase of a job this event fires on.
        enum When
        {
            EVENT_WHEN_JOB_BEGIN = 0,
            EVENT_WHEN_JOB_END   = 1,
            EVENT_WHEN_NUM       = 2
        };

        // event.h:35 -- the action Run performs once the (optional) counter reaches 0.
        enum Type
        {
            EVENT_TYPE_NONE      = 0,
            EVENT_TYPE_WRITE     = 1,
            EVENT_TYPE_CALLBACK  = 2,
            EVENT_TYPE_WRITE_PTR = 3
        };

        Event();
        Event(Type eType, u32 uWriteValue, u32* puWriteLocation, u32* puDecrementerLocation);
        Event(Type eType, void (&fnCallback)(void*), void* pContext, u32* puDecrementerLocation);
        Event(Type eType, void* pWritePtrValue, void** ppWritePtrLocation, u32* puDecrementerLocation);

        // Event::Run @ 0x82BC98B8.
        void Run() const;

        Type mType;                       // +0x0
        union
        {
            struct
            {
                u32  mWriteValue;         // +0x4
                u32* mWriteLocation;      // +0x8
            };
            struct
            {
                void*  mWritePtrValue;    // +0x4
                void** mWritePtrLocation; // +0x8
            };
            struct
            {
                void (*mCallback)(void*); // +0x4
                void* mContext;           // +0x8
            };
        };
        u32* mDecrementerLocation;        // +0xC
    };

namespace Detail
{
    // EA::Jobs::Detail::BucketListNode<T,N> -- a fixed-capacity append-only bucket
    // that chains to an overflow node once full. Used by Job to hold its events,
    // dependencies and dependents. The Event (N=10/16), Job::Dependency (N=10)
    // and Job* dependents (N=6) instantiations are emitted in bucket_list_node.cpp
    // (the X360 .XEX TUs in this group).
    //
    // X360 layout (bucket_list_node.h DWARF + Add @ 0x82BCADD0 asm):
    //   +0              mBucket[N]              (N * sizeof(T) bytes)
    //   +N*sizeof(T)    mNext                   (BucketListNode*)
    //   +N*sizeof(T)+4  mSize                   (uint32_t, count in this node)
    //   +N*sizeof(T)+8  mPad0                   (uint32_t)
    //   +N*sizeof(T)+12 mPad1                   (uint32_t)
    // For N=16, T=Event(16B): mNext@0x100, mSize@0x104, sizeof==0x110 (272) -- the
    // exact size requested from the allocator in Add.
    template <typename T, int N>
    struct BucketListNode
    {
        BucketListNode();
        ~BucketListNode();

        void Clear();
        u32  ListSize() const;

        // Append by value. Walks the chain to the node with free space (or that
        // owns no full successor), allocating + zero-initialising a fresh overflow
        // node through the default ICoreAllocator when the tail is full. Stores the
        // element (member-wise) into the tail node's next free slot and bumps mSize.
        void Add(const T& rElement);

        const T& operator[](int iIndex) const;

        T                mBucket[N];
        BucketListNode*  mNext;
        u32              mSize;
        u32              mPad0;
        u32              mPad1;

    private:
        BucketListNode(const BucketListNode&);
        BucketListNode& operator=(const BucketListNode&);
    };
}
}
}

#endif // EA_JOBS_EVENT_H
