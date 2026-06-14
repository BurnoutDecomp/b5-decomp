#include "GameShared/GameClasses/System/Resource/CgsTextFileResource.h"
#include "rw/rwcore_structs.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828EDF70
//   (CgsResource::TextFileResourceType::GetSerialisedResourceDescriptor)
//
// Five-entry descriptor: entry 0 is {size = *lpResource + 5, alignment 4}; the
// remaining four are {0, 1}. (The Hex-Rays pseudocode hides the 64-bit first store;
// the value is recovered from the PPC asm trace.)

namespace CgsResource
{
    ResourceDescriptor TextFileResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32 luSize = *reinterpret_cast<const u32*>(lpResource) + 5u;

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
