#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnr.h"
#include "rw/rwcore_structs.h"

// ARTIST 826C21B0; the two descriptors are composed through RW operator+=.
CgsResource::ResourceDescriptor CgsResource::SnrResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const CgsSound::Playback::Snr* lpSnr = static_cast<const CgsSound::Playback::Snr*>(lpResource);
    CgsResource::ResourceDescriptor lResult;
    lResult.m_baseResourceDescriptors[0].m_size = 8;
    CgsResource::ResourceDescriptor lData;
    lData.m_baseResourceDescriptors[0].m_size = lpSnr->miSnrSize;
    lResult += lData;
    return lResult;
}
