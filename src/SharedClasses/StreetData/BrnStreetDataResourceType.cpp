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

    // x64: StreetData's serialised offset slots relocate to full 64-bit pointers, so the
    // delta must be the full-width load base (GetLoadBase64). The console passes the u32
    // form only because its pointers are 32-bit; truncating the x64 heap base here would
    // point every table at a wild address. Same treatment as
    // CgsLanguage::LanguageResourceType::FixUp/FixDown.
    void StreetDataResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<StreetData*>(lpResource)->FixDown(CgsResource::GetLoadBase64(lrResource));
    }

    void StreetDataResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<StreetData*>(lpResource)->FixUp(CgsResource::GetLoadBase64(lrResource));
    }
}
