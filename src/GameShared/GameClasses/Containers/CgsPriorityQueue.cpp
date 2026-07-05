#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsContainers
{
// X360 0x82815160. Append a priority entry; returns the pre-seeded entry-index for the slot.
u32 BasePriorityQueue::AddEntry(s32 liPriority)
{
    CGS_ASSERT(muNumEntries != muMaxEntries, "Out of space in queue\n");

    u32 luIndex = mpuEntryIndices[muNumEntries];
    mpiEntryPriorities[muNumEntries] = liPriority;
    ++muNumEntries;
    return luIndex;
}

// X360 0x82815228. Remove the slot whose stored entry-index equals luEntryIndex, shifting the
// tail down in both parallel arrays and returning the freed index to the new tail slot.
void BasePriorityQueue::RemoveEntry(u32 luEntryIndex)
{
    u32 luIndex = 0;
    u32 luIter  = 0;
    if (muNumEntries != 0)
    {
        const u32* lpuIndices = mpuEntryIndices;
        while (*lpuIndices != luEntryIndex)
        {
            ++luIter;
            ++lpuIndices;
            if (luIter >= muNumEntries)
            {
                goto NotFound;
            }
        }
        luIndex = luIter;
    }

NotFound:
    CGS_ASSERT(luIter < muNumEntries, "Entry not found in queue\n");

    for (luIter = luIndex + 1; luIter < muNumEntries; ++luIter)
    {
        mpiEntryPriorities[luIter - 1] = mpiEntryPriorities[luIter];
        mpuEntryIndices[luIter - 1]    = mpuEntryIndices[luIter];
    }

    --muNumEntries;
    mpiEntryPriorities[muNumEntries] = 0;
    mpuEntryIndices[muNumEntries]    = luEntryIndex;
}

// X360 0x828153E0. Return the entry-index with the highest priority among the first
// min(muNumberToCheck, muNumEntries) slots. Ties keep the earlier slot. Empty queue -> false.
bool BasePriorityQueue::GetNextIndexByPriority(u32* lpuIndexOut) const
{
    const u32 luNumEntries = muNumEntries;
    if (luNumEntries == 0)
    {
        return false;
    }

    u32 luMaxToCheck = muNumberToCheck;
    if (luNumEntries <= luMaxToCheck)
    {
        luMaxToCheck = muNumEntries;
    }

    u32 luMaxIndex    = mpuEntryIndices[0];
    s32 liMaxPriority = mpiEntryPriorities[0];
    for (u32 luIter = 1; luIter < luMaxToCheck; ++luIter)
    {
        if (mpiEntryPriorities[luIter] > liMaxPriority)
        {
            luMaxIndex    = mpuEntryIndices[luIter];
            liMaxPriority = mpiEntryPriorities[luIter];
        }
    }

    *lpuIndexOut = luMaxIndex;
    return true;
}

// X360 0x82815458. Age every live priority by a fixed delta, walking at most
// min(muNumberToCheck, muNumEntries) entries. Only the priority array is mutated.
void BasePriorityQueue::Tick(s32 liPriorityDelta)
{
    u32 luMaxToCheck = muNumberToCheck;
    if (muNumEntries <= luMaxToCheck)
    {
        luMaxToCheck = muNumEntries;
    }
    for (u32 luIter = 0; luIter < luMaxToCheck; ++luIter)
    {
        mpiEntryPriorities[luIter] += liPriorityDelta;
    }
}

// X360 0x82815E58. Bring the queue to empty without releasing storage: only the live-entry
// count is reset (every read of the buffers is gated on muNumEntries).
void BasePriorityQueue::Clear()
{
    muNumEntries = 0;
}

// X360 0x828D6490. Read the stored entry-index at slot liIndex (parallel index array). Bounds
// check is against the live-entry count (muNumEntries), not capacity. CgsPriorityQueue.h:220.
u32 BasePriorityQueue::GetEntryIndex(s32 liIndex) const
{
    CGS_ASSERT(liIndex < static_cast<s32>(muNumEntries), "Index out of range\n");
    return mpuEntryIndices[liIndex];
}

// X360 0x828D6540. Read the stored priority at slot liIndex (parallel priority array). Same
// live-count bounds check as GetEntryIndex. CgsPriorityQueue.h:236.
s32 BasePriorityQueue::GetEntryPriority(s32 liIndex) const
{
    CGS_ASSERT(liIndex < static_cast<s32>(muNumEntries), "Index out of range\n");
    return mpiEntryPriorities[liIndex];
}
}
