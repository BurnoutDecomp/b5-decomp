#pragma once

#include "types.hpp"

namespace CgsContainers
{
// Minimal owning slice for the non-templated base shared by all PriorityQueue<T,N>. Member
// names/types/order are from the DecFIGS DWARF and match the X360 pseudocode word indices
// (a1[0]=muMaxEntries .. a1[4]=mpiEntryPriorities). No vtable (non-polymorphic base). The five
// methods defined in CgsPriorityQueue.cpp are owned by this TU; the rest are declared-only.
struct BasePriorityQueue
{
public:
    void Tick(s32 liPriorityDelta);
    void Clear();

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
};
}
