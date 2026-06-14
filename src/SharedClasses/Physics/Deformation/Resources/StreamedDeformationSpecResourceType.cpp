#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::StreamedDeformationSpecResourceType::Serialise @ 0x82677680
//   BrnResource::StreamedDeformationSpecResourceType::FixUp     @ 0x826776F0
//   BrnResource::StreamedDeformationSpecResourceType::GetTypeID @ 0x82675608

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
    static const u32 KI_STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_ID = 65564;
    static const u32 KU_LOCATOR_POINT_SPEC_SIZE = 80;

    static void* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(luAddress));
    }

    class StreamedDeformationSpecResourceType
    {
    public:
        void* Serialise(const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec, void** lppDestination) const;
        u32 GetTypeID() const;
        BrnPhysics::Deformation::StreamedDeformationSpec* FixUp(
            BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
            const u32* lpuBaseAddress) const;

    private:
        u32 CalculateSizeOfResource(const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec) const;
    };

    u32 StreamedDeformationSpecResourceType::CalculateSizeOfResource(
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec) const
    {
        const u32 luEndAddress =
            lpSpec->mLightTags.mpaLocatorPoints +
            (lpSpec->mLightTags.muNumLocators * KU_LOCATOR_POINT_SPEC_SIZE);
        return luEndAddress - static_cast<u32>(reinterpret_cast<uintptr_t>(lpSpec));
    }

    void* StreamedDeformationSpecResourceType::Serialise(
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
        void** lppDestination) const
    {
        void* lpDestination = *lppDestination;
        const u32 luResourceSize = CalculateSizeOfResource(lpSpec);
        BrnPhysics::Deformation::StreamedDeformationSpec* lpMutableSpec =
            const_cast<BrnPhysics::Deformation::StreamedDeformationSpec*>(lpSpec);

        lpMutableSpec->FixDown(lpMutableSpec);
        std::memcpy(lpDestination, lpSpec, luResourceSize);

        BrnPhysics::Deformation::StreamedDeformationSpec* lpCopiedSpec =
            static_cast<BrnPhysics::Deformation::StreamedDeformationSpec*>(lpDestination);
        lpCopiedSpec->FixUp(lpDestination);
        lpMutableSpec->FixUp(lpMutableSpec);

        return lpDestination;
    }

    u32 StreamedDeformationSpecResourceType::GetTypeID() const
    {
        return KI_STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_ID;
    }

    BrnPhysics::Deformation::StreamedDeformationSpec* StreamedDeformationSpecResourceType::FixUp(
        BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
        const u32* lpuBaseAddress) const
    {
        lpSpec->FixUp(PointerFromU32(*lpuBaseAddress));
        return lpSpec;
    }
}
