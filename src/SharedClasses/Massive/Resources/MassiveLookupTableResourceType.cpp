#include "SharedClasses/Massive/Resources/MassiveLookupTableResourceType.h"
#include "rw/rwcore_structs.h"   // complete rw::Resource for the bodies
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::MassiveLookupTableResourceType::FixDown   @ 0x8267F210
//   CgsResource::MassiveLookupTableResourceType::FixUp     @ 0x8267F200
//   CgsResource::MassiveLookupTableResourceType::GetTypeID @ 0x826767A8
//   CgsResource::MassiveLookupTableResourceType::Serialise @ 0x8267F198
//
// FixDown/FixUp forward to the table's own relocation; the relocation delta is the
// leading word of the rw::Resource (its load base). Serialise relocates the source
// to file-relative pointers, copies the whole blob to the destination resource's
// buffer, then re-relocates both. The blob spans base + field0*64 + field4.

namespace BrnMassive
{
    // Own TU; trap stubs until it lands.
    class MassiveLookupTable
    {
    public:
        static void* FixDown(void* pTable, int liDelta);
        static void* FixUp(void* pTable, int liDelta);
    };

    void* MassiveLookupTable::FixDown(void*, int) { __debugbreak(); return nullptr; }
    void* MassiveLookupTable::FixUp(void*, int)   { __debugbreak(); return nullptr; }
}

namespace CgsResource
{
    static const uint32_t KU_MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_ID = 65562;

    uint32_t MassiveLookupTableResourceType::GetTypeID() const
    {
        return KU_MASSIVE_LOOKUP_TABLE_RESOURCE_TYPE_ID;
    }

    // Faithful port of X360 0x8267D818. The serialised lookup-table blob carries its own
    // span: leading word [0] is the entry count, word [1] is the absolute end pointer;
    // count*64 + end - base is the whole-resource byte size (same span Serialise copies).
    // Slot 0 = {size, alignment 16}; the other four entries are the empty {0, 1} default.
    ResourceDescriptor MassiveLookupTableResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const uintptr_t lSrc  = reinterpret_cast<uintptr_t>(lpResource);
        const u32       luSize = static_cast<u32>(
            ((*reinterpret_cast<const u32*>(lSrc) << 6)
             + *reinterpret_cast<const u32*>(lSrc + 4)) - lSrc);

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  lpData[1] = 16u;   // slot0: {whole-resource size, align 16}
        lpData[2] = 0u;  lpData[3] = 1u;
        lpData[4] = 0u;  lpData[5] = 1u;
        lpData[6] = 0u;  lpData[7] = 1u;
        lpData[8] = 0u;  lpData[9] = 1u;
        return lDescriptor;
    }

    // The relocation delta is the first word of the rw::Resource (the load base).
    void MassiveLookupTableResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        BrnMassive::MassiveLookupTable::FixDown(lpResource, static_cast<s32>(CgsResource::GetLoadBase(lrResource)));
    }

    void MassiveLookupTableResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        BrnMassive::MassiveLookupTable::FixUp(lpResource, static_cast<s32>(CgsResource::GetLoadBase(lrResource)));
    }

    void* MassiveLookupTableResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        void*     lpRes = const_cast<void*>(lpResource);
        uintptr_t lSrc  = reinterpret_cast<uintptr_t>(lpResource);
        void*     lpDst = lrDest.m_baseResources[0];
        usize     luSize = ((*reinterpret_cast<const u32*>(lSrc) << 6)
                            + *reinterpret_cast<const u32*>(lSrc + 4)) - lSrc;

        BrnMassive::MassiveLookupTable::FixDown(lpRes, 0);
        std::memcpy(lpDst, lpResource, luSize);
        BrnMassive::MassiveLookupTable::FixUp(lpDst, reinterpret_cast<int>(lpDst));
        BrnMassive::MassiveLookupTable::FixUp(lpRes, static_cast<int>(lSrc));
        return lpDst;
    }
}
