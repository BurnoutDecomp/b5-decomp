#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268D9F0
//   (CgsResource::VoiceHierarchyResourceType::GetTypeID)  ->  return 24;

namespace CgsResource
{
    class VoiceHierarchyResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_VOICE_HIERARCHY_RESOURCE_TYPE_ID = 24;

    int VoiceHierarchyResourceType::GetTypeID()
    {
        return KI_VOICE_HIERARCHY_RESOURCE_TYPE_ID;
    }
}
