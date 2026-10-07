#include "SharedClasses/Massive/Resources/MassiveLookupTableResourceType.h"
#include "SharedClasses/Massive/MassiveLookupTable.h"
#include "rw/rwcore_structs.h"   // complete rw::Resource for the bodies
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MassiveLookupTableResourceType::FixDown   @ 0x8267F210
//   CgsResource::MassiveLookupTableResourceType::FixUp     @ 0x8267F200
//   CgsResource::MassiveLookupTableResourceType::GetTypeID @ 0x826767A8
//   CgsResource::MassiveLookupTableResourceType::Serialise @ 0x8267F198
//
// FixDown/FixUp forward to the table's own relocation against the resource's segment-0
// load base. Serialise relocates the source to file-relative offsets, copies the whole
// blob to the destination resource's buffer, then re-relocates both.

namespace CgsResource
{
    static const uint32_t KU_MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_ID = 65562;

    uint32_t MassiveLookupTableResourceType::GetTypeID() const
    {
        return KU_MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_ID;
    }

    // The fixed-up table spans from its header to the end of its item
    // array: count*64 + items - base is the whole-resource byte size (the same span
    // Serialise copies). Slot 0 = {size, alignment 16}; the other four entries are the
    // empty {0, 1} default.
    ResourceDescriptor MassiveLookupTableResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const BrnMassive::MassiveLookupTable* lpTable =
            static_cast<const BrnMassive::MassiveLookupTable*>(lpResource);
        const u32 luSize = static_cast<u32>(
            reinterpret_cast<uintptr_t>(lpTable->GetItems() + lpTable->GetNumItems())
            - reinterpret_cast<uintptr_t>(lpTable));

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  lpData[1] = 16u;   // slot0: {whole-resource size, align 16}
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }

    void MassiveLookupTableResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<BrnMassive::MassiveLookupTable*>(lpResource)->FixDown(lrResource.m_baseResources[0]);
    }

    void MassiveLookupTableResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<BrnMassive::MassiveLookupTable*>(lpResource)->FixUp(lrResource.m_baseResources[0]);
    }

    void* MassiveLookupTableResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        BrnMassive::MassiveLookupTable* lpTable =
            static_cast<BrnMassive::MassiveLookupTable*>(const_cast<void*>(lpResource));
        void* lpDst = lrDest.m_baseResources[0];
        const usize luSize = reinterpret_cast<uintptr_t>(lpTable->GetItems() + lpTable->GetNumItems())
                           - reinterpret_cast<uintptr_t>(lpTable);

        lpTable->FixDown(lpTable);
        std::memcpy(lpDst, lpTable, luSize);
        static_cast<BrnMassive::MassiveLookupTable*>(lpDst)->FixUp(lrDest.m_baseResources[0]);
        lpTable->FixUp(lpTable);
        return lpDst;
    }
}
