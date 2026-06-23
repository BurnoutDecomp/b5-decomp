#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"

namespace CgsResource
{

// CgsResource::DebugPoolTypeList::DebugPoolTypeList @ 0x827DF8E8
//
// Seed the debug pool-type accounting table. The X360 ctor installs the type's vtable, then runs
// two element-construction loops with one running-total seeding pass between them:
//
//   * First loop (512 pool-type entries, counter 0x1FF..0 inclusive): the X360 calls the array
//     constructor-iterator over the three rw::BaseResourceDescriptor of each entry (default-
//     constructing them to size=alignment=0), then stores the qword {0,1} over the first
//     descriptor -- i.e. seeds descriptor[0] = { m_size = 0, m_alignment = 1 }.
//   * Between the loops: three running-total pairs are stored as {0,1} (the b-halves to 1, the
//     a-halves to 0).
//   * Second loop (7 category entries, counter 6..0 inclusive): same per-entry construction then
//     descriptor[0] = { m_size = 0, m_alignment = 1 }.
//
// The C++ aggregate initialisation here reproduces those observable effects (the three descriptors
// per entry start zeroed; descriptor[0] is seeded {0,1}; the running totals are seeded {0,1}). The
// vtable is installed by entering this (virtual) type's constructor.
DebugPoolTypeList::DebugPoolTypeList()
{
    for (s32 liIndex = 0; liIndex < KI_NUM_POOL_TYPES; ++liIndex)
    {
        PoolTypeEntry& lEntry = maPoolTypes[liIndex];
        for (s32 liDesc = 0; liDesc < 3; ++liDesc)
        {
            lEntry.mDescriptors.m_baseResourceDescriptors[liDesc].m_size      = 0;
            lEntry.mDescriptors.m_baseResourceDescriptors[liDesc].m_alignment = 0;
        }
        // Seed the first sub-allocation descriptor (X360 std {0,1} over entry +0).
        lEntry.mDescriptors.m_baseResourceDescriptors[0].m_size      = 0;
        lEntry.mDescriptors.m_baseResourceDescriptors[0].m_alignment = 1;

        lEntry.mauScratch[0] = 0;
        lEntry.mauScratch[1] = 0;
        lEntry.mauScratch[2] = 0;
    }

    for (s32 liIndex = 0; liIndex < KI_NUM_RUNNING_TOTALS; ++liIndex)
    {
        maRunningTotals[liIndex].muA = 0;
        maRunningTotals[liIndex].muB = 1;
    }

    for (s32 liIndex = 0; liIndex < KI_NUM_CATEGORY_ENTRIES; ++liIndex)
    {
        CategoryEntry& lEntry = maCategoryEntries[liIndex];
        for (s32 liDesc = 0; liDesc < 3; ++liDesc)
        {
            lEntry.mDescriptors.m_baseResourceDescriptors[liDesc].m_size      = 0;
            lEntry.mDescriptors.m_baseResourceDescriptors[liDesc].m_alignment = 0;
        }
        // Seed the first sub-allocation descriptor (X360 std {0,1} over entry +0).
        lEntry.mDescriptors.m_baseResourceDescriptors[0].m_size      = 0;
        lEntry.mDescriptors.m_baseResourceDescriptors[0].m_alignment = 1;

        lEntry.muScratch = 0;
    }
}

}
