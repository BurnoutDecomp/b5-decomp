#include "GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268D9F0
//   (CgsResource::VoiceHierarchyResourceType::GetTypeID)  ->  return 24;
// Virtual override of CgsResource::Type::GetTypeID.

namespace CgsResource
{
    static const uint32_t KU_VOICE_HIERARCHY_RESOURCE_TYPE_ID = 24;

    uint32_t VoiceHierarchyResourceType::GetTypeID() const
    {
        return KU_VOICE_HIERARCHY_RESOURCE_TYPE_ID;
    }
}
