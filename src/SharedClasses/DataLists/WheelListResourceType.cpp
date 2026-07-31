#include "SharedClasses/DataLists/WheelListResourceType.h"
#include "SharedClasses/DataLists/WheelList.h"   // BrnResource::WheelListResource (single home)
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::WheelListResourceType::FixUp     @ 0x8267DF28
//   BrnResource::WheelListResourceType::GetTypeID @ 0x826757B8
//
// FixUp rebases the serialised entry-array slot at +0x04 by the rw::Resource's load base.
//
// ⚠️ DEFECT FIXED 2026-07-31 (vehicle-load wave). The previous body was
//     *reinterpret_cast<uintptr_t*>((uintptr_t)lpResource + 4) += (int)GetLoadBase(...)
// -- an EIGHT-byte read-modify-write at the serialised FOUR-byte offset field. On x64 that
// straddles +0x04..+0x0B, i.e. the entry slot *and* the 16-byte header's tail padding, so a
// perfectly ported WHEELLIST.BUNDLE was still misread (the rebased value landed as a 64-bit
// quantity whose high half is header padding). It also disagreed with the header, which
// declared `const WheelListEntry* mpaEntries` "at +0x04" while MSVC x64 actually places an
// 8-byte pointer at +0x08. VehicleListResourceType is the correct model -- u32 slot +
// PointerFromU32 -- and WheelListResource now matches it (see WheelList.h).

namespace BrnResource
{
    static const uint32_t KU_WHEEL_LIST_RESOURCE_TYPE_ID = 65545;

    uint32_t WheelListResourceType::GetTypeID() const
    {
        return KU_WHEEL_LIST_RESOURCE_TYPE_ID;
    }

    void WheelListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<WheelListResource*>(lpResource)->muEntriesOffset +=
            CgsResource::GetLoadBase(lrResource);
    }
}
