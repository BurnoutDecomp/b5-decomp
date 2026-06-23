#include "SharedClasses/Physics/Props/BrnPropGraphicsListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the descriptor fill
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropGraphicsListResourceType::GetSerialisedResourceDescriptor @ 0x82846698
//
// The serialised payload is a BrnPhysics::Props::PropGraphicsList whose first word is
// muSizeInBytes (the precomputed total byte size of the streamed resource; DWARF
// BrnPropGraphicsList.h:163). GetTypeID/FixDown/FixUp/GetImportCount/GetImportPointer are owned by
// other recon passes and are not bodied here.

namespace BrnPhysics
{
namespace Props
{
    // GetSerialisedResourceDescriptor @ 0x82846698 (store-for-store). The X360:
    //   r9 = *a3 (PropGraphicsList::muSizeInBytes); entry0 = { m_size = muSizeInBytes, m_alignment = 16 }
    // It writes all five alignments (entry0 = 16 via the final 64-bit store; entry1..4 = 1) and the
    // four trailing sizes (0), then one 64-bit store of { muSizeInBytes, 16 } overwrites entry0. The
    // payload size word is read by its serialised dword offset (muSizeInBytes is the 1st dword).
    CgsResource::ResourceDescriptor
    PropGraphicsListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpList = static_cast<const u32*>(lpResource);
        const u32  luSize = lpList[0];   // PropGraphicsList::muSizeInBytes @ +0

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }
}
}
