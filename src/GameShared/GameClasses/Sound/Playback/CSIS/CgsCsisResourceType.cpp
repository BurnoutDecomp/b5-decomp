#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665998
//   (CgsResource::CsisResourceType::GetTypeID)  ->  return 40995;

namespace CgsResource
{
    class CsisResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_CSIS_RESOURCE_TYPE_ID = 40995;

    int CsisResourceType::GetTypeID()
    {
        return KI_CSIS_RESOURCE_TYPE_ID;
    }
}
