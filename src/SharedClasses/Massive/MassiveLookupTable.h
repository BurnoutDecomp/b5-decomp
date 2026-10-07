#ifndef SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H
#define SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H

#include <cstddef>   // offsetof (the wire-contract pins)

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, CgsID

// BrnMassive::MassiveLookupTable -- the "MassiveTable" resource (type 0x1001A) loaded from
// MASSIVETABLE.BIN. One item per in-world advert surface: its bounding box, scene id, the
// impression-event (IE) index it reports under and the renderable massive index that tags the
// world renderables drawing it. BrnMassive::GetSubscriberDataAtIndex matches the renderable
// index and caches the live subscriber pointer in the item.
//
// Member set from the debug-info declaration (SharedClasses/Massive/MassiveLookupTable.h). Loaded in
// place, so the host layout is a wire contract with tools/assets/bundles/nonapt_transcode.py
// (_massive_table): header {s32 count, pad, u64 item offset}, 64-byte items with the subscriber
// pointer widened to 8 bytes. _AssertLayout pins it.

namespace BrnMassive
{
    class BrnMassiveSubscriber;

    struct MassiveLookupTableItem
    {
        Vector3               mBoundingBoxMin;     // +0x00
        Vector3               mBoundingBoxMax;     // +0x10
        CgsID                 mSceneID;            // +0x20
        BrnMassiveSubscriber* mpSubscriber;        // +0x28 (null in the data; cached at runtime)
        s32                   miIEIndex;           // +0x30
        u8                    muRenderableIndex;   // +0x34
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
        MassiveLookupTableItem* mpItems;         // +0x08 (serialised as an offset from the base)
    };

    inline void MassiveLookupTable::_AssertLayout()
    {
        static_assert(sizeof(MassiveLookupTableItem) == 0x40, "MassiveLookupTableItem is a 64-byte record");
        static_assert(offsetof(MassiveLookupTableItem, mSceneID) == 0x20, "mSceneID at +0x20");
        static_assert(offsetof(MassiveLookupTableItem, mpSubscriber) == 0x28, "mpSubscriber at +0x28");
        static_assert(offsetof(MassiveLookupTableItem, miIEIndex) == 0x30, "miIEIndex at +0x30");
        static_assert(offsetof(MassiveLookupTableItem, muRenderableIndex) == 0x34, "muRenderableIndex at +0x34");
        static_assert(offsetof(MassiveLookupTable, mpItems) == 0x08, "mpItems at +0x08");
        static_assert(sizeof(MassiveLookupTable) == 0x10, "the table header is 16 bytes");
    }
}

#endif // SHAREDCLASSES_MASSIVE_MASSIVELOOKUPTABLE_H
