#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A8278
//   (CgsResource::ClusteredMeshResourceType::ReBase)
//
// A pure delegate: the ReBase virtual override forwards to the shared base helper
// CgsResource::Type::ReBaseTechniqueFixDownAndCopy (an inline that fix-downs the
// source, copies the resource bytes, then fix-ups source and dest — per the
// Feb-2007 CgsResourceType.h ground truth). The Hex-Rays pseudocode elides the
// forwarded arguments (they pass through in registers); they are restored here
// from the header's declared signature.

namespace rw
{
    struct Resource;
    struct ResourceDescriptor;
}

namespace CgsResource
{
    struct Type
    {
        void ReBaseTechniqueFixDownAndCopy(
            void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
            rw::ResourceDescriptor& lrSize, s32 liMemType) const;
    };

    struct ClusteredMeshResourceType : Type
    {
        void ReBase(
            void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
            rw::ResourceDescriptor& lrSize, s32 liMemType) const;
    };

    void ClusteredMeshResourceType::ReBase(
        void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
        rw::ResourceDescriptor& lrSize, s32 liMemType) const
    {
        ReBaseTechniqueFixDownAndCopy(lpResource, lrSource, lrDest, lrSize, liMemType);
    }
}
