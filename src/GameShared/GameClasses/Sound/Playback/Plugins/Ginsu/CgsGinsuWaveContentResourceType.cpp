#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665958
//   (CgsResource::GinsuWaveContentResourceType::GetTypeID)  ->  return 40993;

namespace CgsResource
{
    class GinsuWaveContentResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_GINSU_WAVE_CONTENT_RESOURCE_TYPE_ID = 40993;

    int GinsuWaveContentResourceType::GetTypeID()
    {
        return KI_GINSU_WAVE_CONTENT_RESOURCE_TYPE_ID;
    }
}
