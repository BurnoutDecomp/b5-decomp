#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665978
//   (CgsResource::GenericRwacReverbIRContentResourceType::GetTypeID)  ->  return 41000;

namespace CgsResource
{
    class GenericRwacReverbIRContentResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_ID = 41000;

    int GenericRwacReverbIRContentResourceType::GetTypeID()
    {
        return KI_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_ID;
    }
}
