#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource + rw::BaseResourceDescriptors<5>, complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PolygonSoupListResourceType::GetTypeID                       @ 0x82839FA0
//   CgsResource::PolygonSoupListResourceType::FixUp                           @ 0x82845EB0
//   CgsResource::PolygonSoupListResourceType::GetSerialisedResourceDescriptor @ 0x82845B68
//
// FixUp forwards to CgsGeometric::PolygonSoupList::FixUp (own TU), passing the list
// (the resource) and the relocation delta (the rw::Resource's load base).

namespace CgsGeometric
{
    struct PolygonSoupList
    {
        PolygonSoupList* FixUp(int liDelta);

        // The serialised list records its total byte size at +0x2C (the X360
        // GetSerialisedResourceDescriptor reads *(resource + 44)). Modelled by name; only this
        // field is touched by the descriptor body.
        u8  maHeader[0x2C];   // +0x00..+0x2B opaque header (not touched here)
        u32 muSerialisedSize; // +0x2C   total serialised byte size
    };
}

namespace CgsResource
{
    static const uint32_t KU_POLYGON_SOUP_LIST_RESOURCE_TYPE_ID = 67;

    uint32_t PolygonSoupListResourceType::GetTypeID() const
    {
        return KU_POLYGON_SOUP_LIST_RESOURCE_TYPE_ID;
    }

    // X360 0x82845B68 (store-for-store). Builds the five-entry serialised descriptor:
    //   entry0 = { size = list->muSerialisedSize (*(res+44)), align = 128 }
    //   entry1..4 = { size = 0, align = 1 }
    // The X360 writes all five alignments (1) and four trailing sizes (0) first, then a single
    // 64-bit store of {size=*(res+44), align=128} overwrites entry0 -- so entry0's size is the
    // serialised size and its alignment is 128; the rest are {0,1}. (The Hex-Rays pseudocode's
    // "result[2]=v3 ..." is a misread: the asm stores r10=0, not the size, at entries 1-4.)
    ResourceDescriptor PolygonSoupListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const CgsGeometric::PolygonSoupList* lpList =
            static_cast<const CgsGeometric::PolygonSoupList*>(lpResource);

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = lpList->muSerialisedSize;  // entry0 size = *(res+44)
        lpData[1] = 128u;                      // entry0 align
        lpData[2] = 0u;  lpData[3] = 1u;       // entry1 {0,1}
        lpData[4] = 0u;  lpData[5] = 1u;       // entry2 {0,1}
        lpData[6] = 0u;  lpData[7] = 1u;       // entry3 {0,1}
        lpData[8] = 0u;  lpData[9] = 1u;       // entry4 {0,1}
        return lDescriptor;
    }

    void PolygonSoupListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGeometric::PolygonSoupList*>(lpResource)->FixUp(
            static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
