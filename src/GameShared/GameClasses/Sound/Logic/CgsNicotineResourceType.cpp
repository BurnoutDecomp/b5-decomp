#include "GameShared/GameClasses/Sound/Logic/CgsNicotineResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659A8
//   (CgsResource::NicotineResourceType::GetTypeID)  ->  return 40996;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_NICOTINE_RESOURCE_TYPE_ID = 40996;

    uint32_t NicotineResourceType::GetTypeID() const
    {
        return KU_NICOTINE_RESOURCE_TYPE_ID;
    }
}
