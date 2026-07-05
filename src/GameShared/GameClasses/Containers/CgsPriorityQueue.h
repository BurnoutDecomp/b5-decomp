#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (queue-full tripwire)

#include <cstring>   // std::memcpy (PriorityQueue<T,N>::Push element copy)

namespace CgsContainers
{
// Minimal owning slice for the non-templated base shared by all PriorityQueue<T,N>. Member
// names/types/order are from the DecFIGS DWARF and match the X360 pseudocode word indices
// (a1[0]=muMaxEntries .. a1[4]=mpiEntryPriorities). No vtable (non-polymorphic base). The
// methods defined in CgsPriorityQueue.cpp are owned by this TU; the rest are declared-only.
struct BasePriorityQueue
{
public:
    void Tick(s32 liPriorityDelta);
    void Clear();

    // Read accessors for the parallel arrays, both bounds-checked against the LIVE entry count
    // (muNumEntries), not capacity. Signed index compare mirrors the X360 cmpw (static_cast<s32>
    // on the u32 count). Called by CgsResource::DebugComponent::RenderHUD to render the queue
    // contents, so they must be public. CgsPriorityQueue.h:220/236.
    u32 GetEntryIndex(s32 liIndex) const;    // X360 0x828D6490 -> mpuEntryIndices[liIndex]
    s32 GetEntryPriority(s32 liIndex) const; // X360 0x828D6540 -> mpiEntryPriorities[liIndex]

protected:
    void Construct(u32 luMaxEntries, u32 luNumberToCheck, u32* lpuEntryIndices, s32* lpiEntryPriorities);
    u32  AddEntry(s32 liPriority);
    void RemoveEntry(u32 luEntryIndex);
    bool GetNextIndexByPriority(u32* lpuIndexOut) const;

protected:
    u32 muMaxEntries;   // +0x00
    u32 muNumEntries;   // +0x04
private:
    u32  muNumberToCheck;     // +0x08
    u32* mpuEntryIndices;     // +0x0C  parallel index array
    s32* mpiEntryPriorities;  // +0x10  parallel priority array
    // sizeof(BasePriorityQueue) == 20 (X360 32-bit spine) -- DO NOT GROW: it is embedded by
    // value at fixed offsets inside CgsResource::BundleLoaderModule (mLoadQueue/mUnloadQueue).
    // The templated derived PriorityQueue<T,N> (below) calls the protected Construct/AddEntry.
};

// PriorityQueue<T,N> -- the priority-ordered request queue used by the resource IO module.
// It derives from BasePriorityQueue (whose parallel index/priority arrays + AddEntry/RemoveEntry/
// GetNextIndexByPriority/Tick/Clear it inherits) and adds an element-buffer pointer + an inline
// element store of N T's. Push asks the base for the next free slot index (AddEntry) and writes
// the element there. The X360 emits one out-of-line copy of Push per <T,N> using-TU; the generic
// body is inline here and the per-instantiation thin .cpp files force the emission.
//
// LAYOUT (X360 authoritative): BasePriorityQueue (20B) then mpData @ +0x14 (the first derived
// member, the element-buffer pointer) then the inline arrays. Push @ 0x828DF858
// (PriorityQueue<CgsResource::Events::LoadBundleRequest,128>) confirms: full check
// `muNumEntries == muMaxEntries`, then `memcpy(sizeof(T)*AddEntry(liPriority) + a1[5], lpItem,
// sizeof(T))` where a1[5] (+0x14) == mpData.
template <typename T, s32 N>
class PriorityQueue : public BasePriorityQueue
{
public:
    // Bring the queue up: wire the base's parallel arrays + this queue's element pointer at the
    // inline storage. (BasePriorityQueue::Construct is protected, callable from the derived class.)
    void Construct(u32 luNumberToCheck)
    {
        mpData = maData;
        BasePriorityQueue::Construct(static_cast<u32>(N), luNumberToCheck, mauEntryIndices, maiEntryPriorities);
    }

    // Enqueue one element at the priority-ordered slot the base hands back. Returns false WITHOUT
    // storing when the queue is already full (muNumEntries == muMaxEntries); otherwise reserves a
    // slot via AddEntry and copies the element into it. X360 0x828DF858: the full path logs
    // "Unable to add entry. Queue is full." through the debug print (a non-gating tripwire here).
    bool Push(const T* lpItem, s32 liPriority)
    {
        if (muNumEntries == muMaxEntries)
        {
            return false;
        }

        const u32 luIndex = AddEntry(liPriority);
        std::memcpy(mpData + luIndex, lpItem, sizeof(T));
        return true;
    }

private:
    T*  mpData;               // +0x14  element ring buffer base (points at maData after Construct)
    T   maData[N];            // inline element ring buffer
    u32 mauEntryIndices[N];   // parallel index array backing mpuEntryIndices
    s32 maiEntryPriorities[N];// parallel priority array backing mpiEntryPriorities
};
}
