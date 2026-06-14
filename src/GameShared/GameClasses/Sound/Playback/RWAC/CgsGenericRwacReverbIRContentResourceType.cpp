#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacReverbIRContentResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82665978
//   (CgsResource::GenericRwacReverbIRContentResourceType::GetTypeID)  ->  return 41000;
// Virtual override of CgsResource::Type::GetTypeID (via AlignedBinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_ID = 41000;

    uint32_t GenericRwacReverbIRContentResourceType::GetTypeID() const
    {
        return KU_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_ID;
    }
}
