#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsBankResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665988
//   (CgsResource::AemsBankResourceType::GetTypeID)  ->  return 40994;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_AEMS_BANK_RESOURCE_TYPE_ID = 40994;

    uint32_t AemsBankResourceType::GetTypeID() const
    {
        return KU_AEMS_BANK_RESOURCE_TYPE_ID;
    }
}
