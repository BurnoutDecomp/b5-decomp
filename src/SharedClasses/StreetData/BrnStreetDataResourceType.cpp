#include "SharedClasses/StreetData/BrnStreetDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnStreetData::StreetDataResourceType::FixDown   @ 0x8267F0B8
//   BrnStreetData::StreetDataResourceType::FixUp     @ 0x8267F0C8
//   BrnStreetData::StreetDataResourceType::GetTypeID @ 0x82676798
//
// FixUp/FixDown forward to BrnStreetData::StreetData (own TU), passing the delta
// (the rw::Resource's load base).

namespace BrnStreetData
{
    static const uint32_t KU_STREET_DATA_RESOURCE_TYPE_ID = 65560;

    uint32_t StreetDataResourceType::GetTypeID() const
    {
        return KU_STREET_DATA_RESOURCE_TYPE_ID;
    }

    void StreetDataResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<StreetData*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    void StreetDataResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<StreetData*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
