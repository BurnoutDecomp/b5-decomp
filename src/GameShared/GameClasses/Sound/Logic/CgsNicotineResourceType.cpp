#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659A8
//   (CgsResource::NicotineResourceType::GetTypeID)  ->  return 40996;

namespace CgsResource
{
    class NicotineResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_NICOTINE_RESOURCE_TYPE_ID = 40996;

    int NicotineResourceType::GetTypeID()
    {
        return KI_NICOTINE_RESOURCE_TYPE_ID;
    }
}
