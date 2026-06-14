#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659B8
//   (CgsResource::SplicerResourceType::GetTypeID)  ->  return 40997;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_SPLICER_RESOURCE_TYPE_ID = 40997;

    uint32_t SplicerResourceType::GetTypeID() const
    {
        return KU_SPLICER_RESOURCE_TYPE_ID;
    }
}
