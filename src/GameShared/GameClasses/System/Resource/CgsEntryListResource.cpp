#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828D8430
//   (CgsResource::EntryListResourceType::GetTypeID)  ->  return 29;
// Virtual override of CgsResource::Type::GetTypeID.

namespace CgsResource
{
    static const uint32_t KU_ENTRY_LIST_RESOURCE_TYPE_ID = 29;

    uint32_t EntryListResourceType::GetTypeID() const
    {
        return KU_ENTRY_LIST_RESOURCE_TYPE_ID;
    }
}
