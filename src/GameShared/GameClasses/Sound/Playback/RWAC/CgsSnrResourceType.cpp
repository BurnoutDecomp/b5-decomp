#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SnrResourceType::FixUp     @ 0x826C2258
//   CgsResource::SnrResourceType::GetTypeID @ 0x82689E10
//
// FixUp rebases a single leading pointer field (offset 0) by the relocation delta
// (the rw::Resource's load base).

namespace CgsResource
{
    static const uint32_t KU_SNR_RESOURCE_TYPE_ID = 25;

    uint32_t SnrResourceType::GetTypeID() const
    {
        return KU_SNR_RESOURCE_TYPE_ID;
    }

    void SnrResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        *reinterpret_cast<uintptr_t*>(lpResource) += static_cast<int>(CgsResource::GetLoadBase(lrResource));
    }
}
