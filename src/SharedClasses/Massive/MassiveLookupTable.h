#ifndef SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H
#define SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H

#include <cstddef>   // offsetof (the native resource layout)

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, CgsID

// BrnMassive::MassiveLookupTable -- the "MassiveTable" resource (type 0x1001A) loaded from
// MASSIVETABLE.BIN. One item per in-world advert surface: its bounding box, scene id, the
// impression-event (IE) index it reports under and the renderable massive index that tags the
// world renderables drawing it. BrnMassive::GetSubscriberDataAtIndex matches the renderable
// index and caches the live subscriber pointer in the item.
//
// Native ARTIST resource layout, corroborated by the DecFIGS declaration. FixUp/FixDown
// @8267F0D8/8267F138 relocate mpItems at +4 and visit 64-byte records. The consumer
// GetSubscriberDataAtIndex @822A3F08 loads mpSubscriber at +0x28, miIEIndex at +0x2C,
// and muRenderableIndex at +0x30. Vector3's 16-byte alignment supplies the tail
// alignment; the resource contains the original 32-bit pointers.

namespace BrnMassive
{
    class BrnMassiveSubscriber;

    struct MassiveLookupTableItem
    {
        Vector3               mBoundingBoxMin;     // +0x00
        Vector3               mBoundingBoxMax;     // +0x10
        CgsID                 mSceneID;            // +0x20
        BrnMassiveSubscriber* mpSubscriber;        // +0x28 (null in the data; cached at runtime)
        s32                   miIEIndex;           // +0x2C
        u8                    muRenderableIndex;   // +0x30
    };

    struct MassiveLookupTable
    {
    public:
        // Relocate the serialised item offset against the resource load base (and back).
        void FixUp(void* lpBase);
        void FixDown(void* lpBase);

        MassiveLookupTableItem*       GetItems()          { return mpItems; }
        const MassiveLookupTableItem* GetItems() const    { return mpItems; }
        u32                           GetNumItems() const { return static_cast<u32>(miNumberItems); }

        static void _AssertLayout();

    private:
        s32                     miNumberItems;   // +0x00
        MassiveLookupTableItem* mpItems;         // +0x04 (serialised as an offset from the base)
    };

    inline void MassiveLookupTable::_AssertLayout()
    {
        CGS_STATIC_ASSERT((sizeof(void*) == 4), "MassiveLookupTable stores native 32-bit pointers");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTable, miNumberItems) == 0), "miNumberItems at +0");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, mBoundingBoxMin) == 0), "mBoundingBoxMin at +0");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, mBoundingBoxMax) == 0x10), "mBoundingBoxMax at +0x10");
        CGS_STATIC_ASSERT((sizeof(MassiveLookupTableItem) == 0x40), "MassiveLookupTableItem is a 64-byte record");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, mSceneID) == 0x20), "mSceneID at +0x20");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, mpSubscriber) == 0x28), "mpSubscriber at +0x28");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, miIEIndex) == 0x2C), "miIEIndex at +0x2C");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTableItem, muRenderableIndex) == 0x30), "muRenderableIndex at +0x30");
        CGS_STATIC_ASSERT((offsetof(MassiveLookupTable, mpItems) == 0x04), "mpItems at +0x04");
        CGS_STATIC_ASSERT((sizeof(MassiveLookupTable) == 0x08), "the native table header is 8 bytes");
    }
}

#endif // SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H
