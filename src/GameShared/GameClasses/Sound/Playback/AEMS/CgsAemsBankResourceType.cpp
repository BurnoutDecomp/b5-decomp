#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665988
//   (CgsResource::AemsBankResourceType::GetTypeID)  ->  return 40994;

namespace CgsResource
{
    class AemsBankResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_AEMS_BANK_RESOURCE_TYPE_ID = 40994;

    int AemsBankResourceType::GetTypeID()
    {
        return KI_AEMS_BANK_RESOURCE_TYPE_ID;
    }
}
