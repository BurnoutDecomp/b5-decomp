#pragma once

#include <algorithm>
#include <list>
#include <vector>
#include <limits>
#include <utility>

// FLAG PC-platform leaf: GPU-visible resource storage for the console's retained
// geometry. Suballocations stay immutable while live. Retired bytes cannot be
// reused until an event after their last submitted draw has completed. The
// backend must poll without flushing or waiting. All calls belong to the render
// thread; resource unloads must be serialized with command consumption.
namespace renderengine
{
    enum class GeometryBufferKind { Vertex, Index16, Index32 };
    enum class GeometryFenceStatus { Pending, Complete, Failed };

    template <class Backend>
    class PCGeometryBufferPool
    {
        using Buffer = typename Backend::Buffer;
        using Fence = typename Backend::Fence;
        using Context = typename Backend::Context;
        struct Range { unsigned muOffset, muBytes; };
        struct Page
        {
            Buffer mBuffer{};
            GeometryBufferKind meKind;
            unsigned muBytes = 0, muLive = 0, muRetired = 0;
            bool mbWritten = false, mbWritable = true;
            std::vector<Range> mFree;
        };
    public:
        struct Allocation
        {
            Page* mpPage = nullptr;
            unsigned muOffset = 0, muBytes = 0;
            Buffer GetBuffer() const { return mpPage ? mpPage->mBuffer : Buffer{}; }
            explicit operator bool() const { return mpPage != nullptr; }
        };
        struct Statistics
        {
            unsigned long long muPagesCreated = 0, muPagesDestroyed = 0;
            unsigned long long muResidentBytes = 0, muLiveBytes = 0, muRetiredBytes = 0;
        } mStatistics;
    private:
        struct RetiredBatch { Fence mFence{}; std::vector<Allocation> mAllocations; };
        Backend mBackend;
        Context mContext{};
        bool mbInitialized = false, mbSupported = false;
        unsigned muPageBytes;
        std::list<Page> mPages;
        std::vector<Allocation> mRetiring;
        std::list<RetiredBatch> mBatches;
        Fence mSpareFence{};

        void ReturnRange(const Allocation& lrAllocation)
        {
            Page& lrPage = *lrAllocation.mpPage;
            Range lRange{lrAllocation.muOffset, lrAllocation.muBytes};
            auto lPosition = std::lower_bound(lrPage.mFree.begin(), lrPage.mFree.end(), lRange.muOffset,
                [](const Range& lr, unsigned luOffset) { return lr.muOffset < luOffset; });
            if (lPosition != lrPage.mFree.begin())
            {
                auto lPrevious = lPosition - 1;
                if (lPrevious->muOffset + lPrevious->muBytes == lRange.muOffset)
                {
                    lRange.muOffset = lPrevious->muOffset;
                    lRange.muBytes += lPrevious->muBytes;
                    lPosition = lrPage.mFree.erase(lPrevious);
                }
            }
            if (lPosition != lrPage.mFree.end() && lRange.muOffset + lRange.muBytes == lPosition->muOffset)
            {
                lRange.muBytes += lPosition->muBytes;
                lPosition = lrPage.mFree.erase(lPosition);
            }
            lrPage.mFree.insert(lPosition, lRange);
        }
        void ReleaseEmptyPages()
        {
            for (auto lPage = mPages.begin(); lPage != mPages.end(); )
            {
                if (lPage->muLive || lPage->muRetired) { ++lPage; continue; }
                mBackend.DestroyBuffer(lPage->mBuffer);
                ++mStatistics.muPagesDestroyed;
                mStatistics.muResidentBytes -= lPage->muBytes;
                lPage = mPages.erase(lPage);
            }
        }
        bool Initialize(Context lContext)
        {
            if (mContext != lContext)
            {
                // The owner must retire its cache before changing devices; do
                // not invalidate allocations which it still holds silently.
                if (!mPages.empty()) return false;
                ReleaseAll(); mContext = lContext;
            }
            if (!mbInitialized)
            {
                mbInitialized = true;
                mbSupported = mBackend.Supported(mContext);
                if (mbSupported)
                {
                    mSpareFence = mBackend.CreateFence(mContext);
                    mbSupported = mSpareFence != Fence{};
                }
            }
            return mbSupported;
        }
    public:
        explicit PCGeometryBufferPool(unsigned luPageBytes = 1024u * 1024u)
            : muPageBytes(luPageBytes) {}
        ~PCGeometryBufferPool() { ReleaseAll(); }
        PCGeometryBufferPool(const PCGeometryBufferPool&) = delete;
        PCGeometryBufferPool& operator=(const PCGeometryBufferPool&) = delete;
        Backend& GetBackend() { return mBackend; }

        bool Store(Context lContext, GeometryBufferKind leKind, const void* lpData,
                   unsigned luBytes, Allocation& lrOut)
        {
            lrOut = Allocation{};
            if (!lpData || !luBytes || luBytes > (std::numeric_limits<unsigned>::max)() - 15u
                || !Initialize(lContext)) return false;
            const unsigned luReserved = (luBytes + 15u) & ~15u;
            Page* lpPage = nullptr;
            unsigned luRange = 0;
            for (auto& lrPage : mPages)
            {
                if (!lrPage.mbWritable || lrPage.meKind != leKind) continue;
                for (unsigned lu = 0; lu < lrPage.mFree.size(); ++lu)
                    if (lrPage.mFree[lu].muBytes >= luReserved)
                    { lpPage = &lrPage; luRange = lu; break; }
                if (lpPage) break;
            }
            if (!lpPage)
            {
                const unsigned luCapacity = luReserved > muPageBytes ? luReserved : muPageBytes;
                Buffer lBuffer = mBackend.CreateBuffer(mContext, leKind, luCapacity);
                if (lBuffer == Buffer{}) return false;
                mPages.emplace_back();
                lpPage = &mPages.back();
                lpPage->mBuffer = lBuffer; lpPage->meKind = leKind; lpPage->muBytes = luCapacity;
                lpPage->mFree.push_back(Range{0, luCapacity});
                ++mStatistics.muPagesCreated;
                mStatistics.muResidentBytes += luCapacity;
            }
            Range& lrRange = lpPage->mFree[luRange];
            lrOut = Allocation{lpPage, lrRange.muOffset, luReserved};
            lrRange.muOffset += luReserved; lrRange.muBytes -= luReserved;
            if (!lrRange.muBytes) lpPage->mFree.erase(lpPage->mFree.begin() + luRange);
            ++lpPage->muLive;
            mStatistics.muLiveBytes += luReserved;
            // DISCARD is legal only on a brand-new page before any slice can have
            // been submitted. Every subsequent append or fence-reclaimed hole is
            // a NOOVERWRITE write; discarding would invalidate other live meshes.
            if (!mBackend.Upload(lpPage->mBuffer, leKind, lrOut.muOffset, lpData, luBytes, !lpPage->mbWritten))
            {
                lpPage->mbWritable = false;
                --lpPage->muLive; mStatistics.muLiveBytes -= luReserved;
                ReturnRange(lrOut); lrOut = Allocation{};
                ReleaseEmptyPages();
                return false;
            }
            lpPage->mbWritten = true;
            return true;
        }
        void Retire(Allocation& lrAllocation)
        {
            if (!lrAllocation) return;
            --lrAllocation.mpPage->muLive;
            ++lrAllocation.mpPage->muRetired;
            mStatistics.muLiveBytes -= lrAllocation.muBytes;
            mStatistics.muRetiredBytes += lrAllocation.muBytes;
            mRetiring.push_back(lrAllocation);
            lrAllocation = Allocation{};
        }
        // Once per render frame, after all earlier uses of retiring slices have
        // been issued. Never block, flush the device, or infer GPU completion from
        // a frame count / Present. Only a completed event releases a range.
        void BeginFrame()
        {
            if (!mbSupported) return;
            for (auto lBatch = mBatches.begin(); lBatch != mBatches.end(); )
            {
                const GeometryFenceStatus leStatus = mBackend.PollFence(lBatch->mFence);
                if (leStatus == GeometryFenceStatus::Failed) { mbSupported = false; return; }
                if (leStatus == GeometryFenceStatus::Pending) { ++lBatch; continue; }
                for (const auto& lrAllocation : lBatch->mAllocations)
                {
                    ReturnRange(lrAllocation);
                    --lrAllocation.mpPage->muRetired;
                    mStatistics.muRetiredBytes -= lrAllocation.muBytes;
                }
                if (mSpareFence == Fence{}) mSpareFence = lBatch->mFence;
                else mBackend.DestroyFence(lBatch->mFence);
                lBatch = mBatches.erase(lBatch);
            }
            ReleaseEmptyPages();
            if (!mRetiring.empty())
            {
                Fence lFence = mSpareFence;
                mSpareFence = Fence{};
                if (lFence == Fence{}) lFence = mBackend.CreateFence(mContext);
                if (lFence == Fence{} || !mBackend.IssueFence(lFence))
                {
                    if (lFence != Fence{}) mBackend.DestroyFence(lFence);
                    // Retired spans remain quarantined until teardown. Future
                    // stores use the caller's individual-buffer fallback.
                    mbSupported = false;
                    return;
                }
                mBatches.emplace_back();
                mBatches.back().mFence = lFence;
                mBatches.back().mAllocations.swap(mRetiring);
            }
        }
        void ReleaseAll()
        {
            for (auto& lrBatch : mBatches) mBackend.DestroyFence(lrBatch.mFence);
            if (mSpareFence != Fence{}) mBackend.DestroyFence(mSpareFence);
            mBatches.clear(); mRetiring.clear(); mSpareFence = Fence{};
            for (auto& lrPage : mPages)
            {
                mBackend.DestroyBuffer(lrPage.mBuffer);
                ++mStatistics.muPagesDestroyed;
            }
            mPages.clear();
            mStatistics.muResidentBytes = mStatistics.muLiveBytes = mStatistics.muRetiredBytes = 0;
            mbInitialized = mbSupported = false;
            mContext = Context{};
        }
    };
}
