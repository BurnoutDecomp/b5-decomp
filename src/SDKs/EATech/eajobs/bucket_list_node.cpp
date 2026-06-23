#include "SDKs/EATech/eajobs/event.h"
#include "SDKs/EATech/eajobs/job.h" // EA::Jobs::Job::Dependency (BucketListNode payload)

#include <cstring> // std::memset (raw zero-init mirrors the asm dword stores)

// EA::Jobs::Detail::BucketListNode<T,N> generic, reconstructed once and explicitly
// instantiated for the (T,N) the X360 .XEX actually emits in this group:
//   BucketListNode<Event,10> -- Job::mEvents element (mNext@0xA0, mSize@0xA4)
//        scalar deleting destructor @ 0x82BCAFC0
//   BucketListNode<Event,16> -- LocalBackend::JobInstance event list (mNext@0x100,
//        mSize@0x104, sizeof 0x110). Add @ 0x82BCADD0, scalar deleting dtor @ 0x82BCB040
//   BucketListNode<Job::Dependency,10> -- Job::mDependencies. Element stride 0x20
//        (slwi r11,r11,5), mNext@0x140, mSize@0x144, sizeof 0x150 (the li r4,0x150
//        allocation request). Add @ 0x82BCAC20, ListSize @ 0x82BCA030.
//
// The X360 emits a fresh template instance per using-TU; this one .cpp covers all.
// All member bodies are reconstructed store-for-store from the matching X360 asm.

namespace EA
{
namespace Jobs
{
namespace Detail
{
    template <typename T, int N>
    BucketListNode<T, N>::BucketListNode()
        : mNext(0)
        , mSize(0)
        , mPad0(0)
        , mPad1(0)
    {
        // Add @ 0x82BCADD0 zero-initialises a freshly allocated overflow node's
        // bucket with raw dword stores before linking it; the in-place ctor mirrors
        // that (zero the POD bucket directly rather than per-element construction).
        std::memset(mBucket, 0, sizeof(mBucket));
    }

    // ~BucketListNode @ 0x82BCAFC0 / 0x82BCB040 (the scalar-deleting-destructor body,
    // minus the trailing operator-delete that MSVC's deleting-dtor thunk performs).
    //
    // X360 asm (N=16 shown; N=10 is identical at offsets 0xA0/0xA4):
    //   lwz r3,0x100(this)         ; mNext
    //   if (mNext) recurse(mNext, /*delete*/ 1)
    //   stw 0,0x100(this)          ; mNext = 0
    //   stw 0,0x104(this)          ; mSize = 0
    // i.e. delete the whole overflow chain (each link is heap-allocated by Add) and
    // null this node's link/count. The owning node itself is either inline (Job /
    // JobInstance member) or freed by the caller's delete -- not here.
    template <typename T, int N>
    BucketListNode<T, N>::~BucketListNode()
    {
        if (mNext)
        {
            EA::Allocator::ICoreAllocator* lpAllocator =
                EA::Allocator::ICoreAllocator::GetDefaultAllocator();
            // Destroy + free each chained overflow node (the recursion in the asm).
            BucketListNode* lpNode = mNext;
            mNext = 0;
            mSize = 0;
            lpNode->~BucketListNode();
            lpAllocator->Free(lpNode, 0);
        }
        else
        {
            mSize = 0;
        }
    }

    template <typename T, int N>
    void BucketListNode<T, N>::Clear()
    {
        this->~BucketListNode();
    }

    template <typename T, int N>
    u32 BucketListNode<T, N>::ListSize() const
    {
        u32 luTotal = 0;
        for (const BucketListNode* lpNode = this; lpNode; lpNode = lpNode->mNext)
            luTotal += lpNode->mSize;
        return luTotal;
    }

    // Add @ 0x82BCADD0. Walk to the tail node that still has room (mSize < N),
    // allocating + zero-initialising a fresh overflow node off the full tail when
    // needed, then store the element into the next free slot and bump mSize.
    template <typename T, int N>
    void BucketListNode<T, N>::Add(const T& rElement)
    {
        BucketListNode* lpNode = this;
        for (; lpNode->mSize >= (u32)N; lpNode = lpNode->mNext)
        {
            if (!lpNode->mNext)
            {
                EA::Allocator::ICoreAllocator* lpAllocator =
                    EA::Allocator::ICoreAllocator::GetDefaultAllocator();
                // Exact X360 request: sizeof(node) bytes, debug tag, flags 0,
                // 16-byte alignment, alignment offset 0.
                void* lpRaw = lpAllocator->Alloc(
                    sizeof(BucketListNode),
                    "EA::Jobs::Detail::BucketListNode",
                    0,
                    16,
                    0);
                BucketListNode* lpNew = 0;
                if (lpRaw)
                {
                    lpNew = static_cast<BucketListNode*>(lpRaw);
                    // Zero the bucket (the asm's 16 dword-stores), then mNext/mSize.
                    std::memset(lpNew->mBucket, 0, sizeof(lpNew->mBucket));
                    lpNew->mNext = 0;
                    lpNew->mSize = 0;
                }
                lpNode->mNext = lpNew;
            }
        }

        T* lpSlot = &lpNode->mBucket[lpNode->mSize];
        *lpSlot = rElement;
        ++lpNode->mSize;
    }

    template <typename T, int N>
    const T& BucketListNode<T, N>::operator[](int iIndex) const
    {
        const BucketListNode* lpNode = this;
        int liIndex = iIndex;
        while (liIndex >= (int)lpNode->mSize && lpNode->mNext)
        {
            liIndex -= (int)lpNode->mSize;
            lpNode = lpNode->mNext;
        }
        return lpNode->mBucket[liIndex];
    }

    // Explicit instantiations -- the (T,N) the X360 .XEX emits for this group.
    template struct BucketListNode<EA::Jobs::Event, 10>;
    template struct BucketListNode<EA::Jobs::Event, 16>;
    // Job::mDependencies: Add @ 0x82BCAC20 (sizeof 0x150 == 10*0x20 + 0x10),
    // ListSize @ 0x82BCA030.
    template struct BucketListNode<EA::Jobs::Job::Dependency, 10>;
}
}
}
