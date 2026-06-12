#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659B8
//   (CgsResource::SplicerResourceType::GetTypeID)  ->  return 40997;

namespace CgsResource
{
    class SplicerResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_SPLICER_RESOURCE_TYPE_ID = 40997;

    int SplicerResourceType::GetTypeID()
    {
        return KI_SPLICER_RESOURCE_TYPE_ID;
    }
}
