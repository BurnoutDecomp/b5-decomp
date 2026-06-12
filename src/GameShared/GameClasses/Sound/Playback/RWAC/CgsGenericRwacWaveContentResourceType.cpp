#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665968
//   (CgsResource::GenericRwacWaveContentResourceType::GetTypeID)  ->  return 40992;

namespace CgsResource
{
    class GenericRwacWaveContentResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_ID = 40992;

    int GenericRwacWaveContentResourceType::GetTypeID()
    {
        return KI_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_ID;
    }
}
