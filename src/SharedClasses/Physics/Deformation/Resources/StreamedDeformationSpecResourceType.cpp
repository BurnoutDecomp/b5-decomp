#include "SharedClasses/Physics/Deformation/Resources/StreamedDeformationSpecResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::StreamedDeformationSpecResourceType::Serialise @ 0x82677680
//   BrnResource::StreamedDeformationSpecResourceType::FixUp     @ 0x826776F0
//   BrnResource::StreamedDeformationSpecResourceType::GetTypeID @ 0x82675608
//
// FixUp forwards to the spec's own relocation (base = the rw::Resource's load base).
// Serialise un-relocates the source, copies the computed resource span to the
// destination buffer, then re-relocates both. The spec's own FixDown/FixUp take the
// object's base address.

namespace BrnPhysics
{
namespace Deformation
{
    struct LocatorPointSpecList
    {
        u32 muNumLocators;
        u32 mpaLocatorPoints;
    };

    struct StreamedDeformationSpec
    {
        void FixDown(void* lpBaseAddress);
        void FixUp(void* lpBaseAddress);

        u8 maPad0[36];
        LocatorPointSpecList mGenericTags;
        LocatorPointSpecList mCameraTags;
        LocatorPointSpecList mLightTags;
    };
}
}

namespace BrnResource
{
    using BrnPhysics::Deformation::StreamedDeformationSpec;

    static const uint32_t KU_STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_ID = 65564;
    static const u32 KU_LOCATOR_POINT_SPEC_SIZE = 80;

    static u32 CalculateSizeOfResource(const StreamedDeformationSpec* lpSpec)
    {
        const u32 luEndAddress =
            lpSpec->mLightTags.mpaLocatorPoints + (lpSpec->mLightTags.muNumLocators * KU_LOCATOR_POINT_SPEC_SIZE);
        return luEndAddress - static_cast<u32>(reinterpret_cast<uintptr_t>(lpSpec));
    }

    uint32_t StreamedDeformationSpecResourceType::GetTypeID() const
    {
        return KU_STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_ID;
    }

    void StreamedDeformationSpecResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        void* lpBaseAddress = lrResource.m_baseResources[0];
        static_cast<StreamedDeformationSpec*>(lpResource)->FixUp(lpBaseAddress);
    }

    void* StreamedDeformationSpecResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        StreamedDeformationSpec* lpSpec = const_cast<StreamedDeformationSpec*>(
            static_cast<const StreamedDeformationSpec*>(lpResource));
        void* lpDest = lrDest.m_baseResources[0];
        const u32 luResourceSize = CalculateSizeOfResource(lpSpec);

        lpSpec->FixDown(lpSpec);
        std::memcpy(lpDest, lpSpec, luResourceSize);

        static_cast<StreamedDeformationSpec*>(lpDest)->FixUp(lpDest);
        lpSpec->FixUp(lpSpec);

        return lpDest;
    }
}
