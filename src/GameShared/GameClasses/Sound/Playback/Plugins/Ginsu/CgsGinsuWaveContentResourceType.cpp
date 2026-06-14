#include "GameShared/GameClasses/Sound/Playback/Plugins/Ginsu/CgsGinsuWaveContentResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665958
//   (CgsResource::GinsuWaveContentResourceType::GetTypeID)  ->  return 40993;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_GINSU_WAVE_CONTENT_RESOURCE_TYPE_ID = 40993;

    uint32_t GinsuWaveContentResourceType::GetTypeID() const
    {
        return KU_GINSU_WAVE_CONTENT_RESOURCE_TYPE_ID;
    }
}
