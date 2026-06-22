#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.h"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the body
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AttribSysVaultResourceType::GetSerialisedResourceDescriptor @ 0x828EBA48
//
// The serialised vault blob's header words give its two contributing spans: word [1]
// plus word [3], plus a fixed 16-byte header, is the whole-resource byte size. The X360
// stores that size into ALL FIVE descriptor entries (the vault sub-allocations share one
// span); slot 0 takes alignment 16, the remaining four take alignment 1. Word access
// mirrors the X360 reads of the opaque serialised header.

namespace CgsResource
{
    ResourceDescriptor AttribSysVaultResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpWords = static_cast<const u32*>(lpResource);
        const u32  luSize  = lpWords[3] + lpWords[1] + 16u;   // X360: *(a3+12) + *(a3+4) + 16

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  lpData[1] = 16u;   // slot0: {size, align 16}
        lpData[2] = luSize;  lpData[3] = 1u;
        lpData[4] = luSize;  lpData[5] = 1u;
        lpData[6] = luSize;  lpData[7] = 1u;
        lpData[8] = luSize;  lpData[9] = 1u;
        return lDescriptor;
    }
}
