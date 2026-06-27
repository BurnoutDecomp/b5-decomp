#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the body

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828EC990
//   (CgsResource::BinaryFileResourceType::GetSerialisedResourceDescriptor)
//
// Five-entry descriptor: entry 0 is { size = lpResource[0] + lpResource[1], alignment 4 };
// the remaining four are { 0, 1 }. The two leading u32s of the serialised binary-file
// resource are a header length and a payload length whose sum is the total block size.
// (The Hex-Rays pseudocode hides the 64-bit first store of { size, align=4 }; the layout
// is recovered from the PPC asm: the final `std` writes size@+0 / 4@+4, overwriting the
// {0,1} placeholders the earlier word stores wrote for entry 0.) Sibling of
// TextFileResourceType, which uses lpResource[0] + 5.

namespace CgsResource
{
    // A plain binary-file resource has no pointers to relocate, so the ctor leaves the Type defaults
    // and FixUp/FixDown are no-ops; concrete handlers (e.g. LuaCodeResourceType) add only GetTypeID.
    // Defining the full override set here lets BinaryFileResourceType's vtable link once a concrete
    // subclass is instantiated/registered.
    BinaryFileResourceType::BinaryFileResourceType()
        : Type()
    {
    }

    // Base id sentinel -- every concrete handler overrides this with its registry id.
    uint32_t BinaryFileResourceType::GetTypeID() const
    {
        return 0xFFFFFFFFu;
    }

    // Raw binary payloads carry no fix-up-able pointers -> both relocation passes are no-ops.
    void BinaryFileResourceType::FixUp(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
    }

    void BinaryFileResourceType::FixDown(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
    }

    ResourceDescriptor BinaryFileResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpHeader = reinterpret_cast<const u32*>(lpResource);
        const u32 luSize = lpHeader[0] + lpHeader[1];   // r8(a3[0]) + r9(a3[1])

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1u;
        }
        return lDescriptor;
    }
}
