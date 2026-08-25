#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the body
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SnrResourceType::FixUp     @ 0x826C2258
//   CgsResource::SnrResourceType::GetTypeID @ 0x82689E10
//
// FixUp rebases a single leading pointer field (offset 0) by the relocation delta
// (the rw::Resource's load base). The X360 rebases a 32-bit slot with the 32-bit
// base; on the platform-4 (x64) serialised form the slot is pointer-width and the
// rebase must use the FULL-WIDTH load base (GetLoadBase64) -- the earlier
// `static_cast<int>(GetLoadBase(...))` truncated the 64-bit heap base through
// int (sign-extended), corrupting the rebased pointer for bases above 2GB
// (fixed 2026-08-25 audio-faithfulness wave 1; mirrors the sibling
// StaticSoundMapResourceType::FixUp @0x826775C8 convention).

namespace CgsResource
{
    static const uint32_t KU_SNR_RESOURCE_TYPE_ID = 25;

    uint32_t SnrResourceType::GetTypeID() const
    {
        return KU_SNR_RESOURCE_TYPE_ID;
    }

    void SnrResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        *reinterpret_cast<uintptr_t*>(lpResource) += CgsResource::GetLoadBase64(lrResource);
    }
}
