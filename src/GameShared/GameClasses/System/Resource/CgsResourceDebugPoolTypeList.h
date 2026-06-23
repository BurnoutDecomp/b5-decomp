#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<3> (per-entry serialised descriptor)

// CgsResource::DebugPoolTypeList -- the debug-only per-pool-type resource-accounting table the
// resource DebugComponent embeds (CgsResource::DebugComponent::DebugComponent constructs it at
// +0x480 inside the component). It holds a per-pool-type entry array (512 entries), a small set
// of per-sub-allocation running totals, and a per-debug-category entry array (7 entries, one per
// EDebugResourceCategory plus the count slot). Each entry carries a three-way
// rw::BaseResourceDescriptors<3> (size/alignment running totals for the entry's sub-allocations).
//
// Recovered from CgsResource::DebugPoolTypeList::DebugPoolTypeList @ 0x827DF8E8 (the only function
// of this TU; called only by DebugComponent::DebugComponent). The structure is debug accounting
// state -- it is constructed and updated by the debug component, never serialised and never read
// by raw offset elsewhere -- so it is modelled with named members at semantic parity (NOT byte-
// matched). The ctor's observable effects, reproduced by DebugPoolTypeList() below:
//   * installs the type's vtable (the X360 stores its vtable pointer at +0);
//   * for each of the 512 pool-type entries: default-constructs the three BaseResourceDescriptors
//     (size=alignment=0) then seeds entry descriptor[0] = { m_size = 0, m_alignment = 1 };
//   * seeds the three running-total pairs to { 0, 1 };
//   * for each of the 7 category entries: default-constructs the three descriptors then seeds
//     descriptor[0] = { m_size = 0, m_alignment = 1 }.
namespace CgsResource
{
    class DebugPoolTypeList
    {
    public:
        // X360 0x827DF8E8. Seed the accounting table to its empty-but-initialised state.
        DebugPoolTypeList();

        // Polymorphic: the X360 installs a vtable pointer at +0. The concrete virtuals are debug
        // hooks not exercised by the boot trace; a virtual destructor gives the type its vtable.
        virtual ~DebugPoolTypeList() {}

        // The number of pool-type entries the table tracks (first ctor loop count, 0x1FF+1).
        static const s32 KI_NUM_POOL_TYPES = 512;
        // The number of per-debug-category entries (second ctor loop count, 6+1).
        static const s32 KI_NUM_CATEGORY_ENTRIES = 7;
        // The number of per-sub-allocation running totals seeded between the two arrays.
        static const s32 KI_NUM_RUNNING_TOTALS = 3;

        // One per-pool-type accounting entry: the three-way size/alignment running totals plus a
        // small per-entry scratch the debug component updates as resources are allocated/freed.
        struct PoolTypeEntry
        {
            rw::BaseResourceDescriptors<3> mDescriptors;   // size/alignment running totals
            u32                            mauScratch[3];   // per-entry debug accounting scratch
        };

        // One per-debug-category accounting entry (slightly narrower than PoolTypeEntry).
        struct CategoryEntry
        {
            rw::BaseResourceDescriptors<3> mDescriptors;   // size/alignment running totals
            u32                            muScratch;       // per-category debug accounting scratch
        };

        // A per-sub-allocation running total pair (the {0,1}-seeded couples between the arrays).
        struct RunningTotal
        {
            u32 muA;   // seeded 0
            u32 muB;   // seeded 1
        };

    private:
        PoolTypeEntry maPoolTypes[KI_NUM_POOL_TYPES];           // first ctor loop (stride 36)
        RunningTotal  maRunningTotals[KI_NUM_RUNNING_TOTALS];   // seeded {0,1} between the arrays
        CategoryEntry maCategoryEntries[KI_NUM_CATEGORY_ENTRIES]; // second ctor loop (stride 28)
    };
}
