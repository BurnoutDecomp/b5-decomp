#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacWaveContentResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665968
//   (CgsResource::GenericRwacWaveContentResourceType::GetTypeID)  ->  return 40992;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_ID = 40992;

    uint32_t GenericRwacWaveContentResourceType::GetTypeID() const
    {
        return KU_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_ID;
    }
}
