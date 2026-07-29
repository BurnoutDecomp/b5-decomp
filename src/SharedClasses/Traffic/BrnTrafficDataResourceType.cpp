#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource + rw::BaseResourceDescriptors<5>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // GetLoadBase64
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::TrafficDataResourceType::GetTypeID @ 0x82752560
//   BrnTraffic::TrafficDataResourceType::GetSerialisedResourceDescriptor @ 0x82760660
//   BrnTraffic::TrafficDataResourceType::FixDown   @ 0x82763E68
//   BrnTraffic::TrafficDataResourceType::FixUp     @ 0x82763E70
//
// ⚠️ FixUp @0x82763E70 is ABSENT from .ida-exports/ -- it is an 8-byte tail thunk with no
// .pdata unwind record, so IDA never promoted it to a function and the exporter (which walks
// idautils.Functions()) never saw it. That, plus the fact that TrafficData::FixUp @0x827637D8
// is only ever reached through the vtable (hence zero code xrefs), is what made this type look
// like it had no FixUp at all. It does; the vtable @0x820A1520 holds it in slot 4, and
// CgsResource::Pool::FixUpEntry @0x828EB860 calls slot 4 unconditionally (no flag, no import
// gate -- GetImportCount is the ICF-folded `return 0` for this type).
//
// Both thunks are `mr r3,r4` + branch: TrafficData::FixUp/FixDown receive the resource block
// as BOTH `this` and the relocation base, i.e. every serialised slot is an offset from the
// start of the TrafficData header itself.

namespace BrnTraffic
{
    static const uint32_t KU_TRAFFIC_LANEDATA_RESOURCE_TYPE_ID = 65538;

    uint32_t TrafficDataResourceType::GetTypeID() const
    {
        return KU_TRAFFIC_LANEDATA_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x82760660 (store-for-store). The serialised
    // traffic-data occupies one 16-byte-aligned block whose size is the
    // TrafficData::muSizeInBytes word read at +4 (`lwz r9, 4(r30)`). The X360 first
    // asserts that word is non-zero ("Uninitialised Traffic Data resource"); the
    // std packs {size = muSizeInBytes, align = 0x10}; entries 1..4 are {0,1}.
    CgsResource::ResourceDescriptor
    TrafficDataResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const TrafficData* lpData = static_cast<const TrafficData*>(lpResource);

        CGS_ASSERT(lpData->muSizeInBytes != 0u, "Uninitialised Traffic Data resource");

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lpData->muSizeInBytes;  // entry0 size  (*(a3+4))
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;                  // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // @0x82763E68 -- `mr r3,r4; b TrafficData::FixDown`. The X360 passes lpResource as the
    // relocation base, so the rw::Resource is unused on this leg.
    void TrafficDataResourceType::FixDown(void* lpResource, const rw::Resource&) const
    {
        static_cast<TrafficData*>(lpResource)->FixDown(lpResource);
    }

    // @0x82763E70 -- `mr r3,r4; b TrafficData::FixUp`. Same shape: the block is its own base.
    void TrafficDataResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        static_cast<TrafficData*>(lpResource)->FixUp(lpResource);
    }
}
