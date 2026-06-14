#include "GameShared/GameClasses/Sound/Playback/CSIS/CgsCsisResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665998
//   (CgsResource::CsisResourceType::GetTypeID)  ->  return 40995;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_CSIS_RESOURCE_TYPE_ID = 40995;

    uint32_t CsisResourceType::GetTypeID() const
    {
        return KU_CSIS_RESOURCE_TYPE_ID;
    }
}
