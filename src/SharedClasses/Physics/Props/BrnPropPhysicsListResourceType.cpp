#include "SharedClasses/Physics/Props/BrnPropPhysicsListResourceType.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"  // PropPhysicsDataHeader::FixUp/FixDown
#include "rw/rwcore_structs.h"                                      // rw::Resource / BaseResourceDescriptors<5>
#include "types.hpp"

#include <cstddef>   // offsetof (wire-contract pins)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnPhysics::Props::PropPhysicsResourceType::FixDown @ 0x8267F750
//   BrnPhysics::Props::PropPhysicsResourceType::FixUp   @ 0x8267F760
//   ... registered as "PropPhysicsResourceType" (vtable off_820A1898) by
//   BrnResource::GameDataModule::RegisterResourceTypes @ 0x82667EA8.
//
// The serialised payload IS a BrnPhysics::Props::PropPhysicsDataHeader. Both fixups are thin
// forwarders: the X360 tail-calls PropPhysicsDataHeader::FixDown / ::FixUp with lpResource as
// the header `this` and the rw::Resource arg passed through.
//
// GetTypeID / GetSerialisedResourceDescriptor: NOT PRESENT IN THE EXPORT SET.
//   Both are tiny leaf bodies that IDA left unnamed, so neither has a .json to quote. What
//   the type id is, however, is not in doubt -- it is triangulated three ways:
//     1. The shipped resource itself. PROPS/PROPPHYSICS.BUNDLE's one resource entry declares
//        resourceTypeId 0x1000F (65551) with the debug string table naming it
//        `<Resource id="d75c5932" type="PropPhysics" name="PRP_PHYSICS_"/>`. A handler whose
//        GetTypeID did not return 65551 could never be selected for that entry.
//     2. The GetTypeID address block. These bodies are laid out at a uniform 0x10 stride:
//        0x82675608 StreamedDeformationSpec -> 65564, 0x82675628 PropGraphicsList -> 65552,
//        0x82675638 PropInstanceData -> 65553. The one gap is 0x82675618, exactly where
//        PropPhysics sits, and 0x8267561C would hold `ori r3, r3, 0xF` (the neighbours read
//        `lis r3, 1; ori r3, r3, 0x10 # 0x10010`).
//     3. Registration order. RegisterResourceTypes registers PropPhysicsResourceType
//        immediately before PropGraphicsListResourceType (65552) and
//        PropInstanceDataResourceType (65553).
//   GetSerialisedResourceDescriptor follows its two siblings in this directory
//   store-for-store (PropGraphicsList @0x82846698, PropInstanceData @0x8267B0C8): read the
//   payload's own byte-size word into entry0 with alignment 16, and leave entry1..4 as
//   {0, 1}. Here that word is PropPhysicsDataHeader::muSizeInBytes, which the shipped
//   resource carries as the FULL blob size (header + record arena), so -- unlike
//   PropInstanceData, whose muSizeInBytes excludes its header -- nothing is added to it.
//   The bundle entry independently agrees: uncompressedSizeAndAlignment 0x40016A90
//   == size 0x16A90 with alignment 1<<4.

namespace BrnPhysics
{
namespace Props
{
    static const uint32_t KU_PROP_PHYSICS_RESOURCE_TYPE_ID = 65551;  // 0x1000F

    uint32_t PropPhysicsResourceType::GetTypeID() const
    {
        return KU_PROP_PHYSICS_RESOURCE_TYPE_ID;
    }

    // Sized from the resource's own muSizeInBytes; entry0 align 16, entry1..4 {0, 1}.
    CgsResource::ResourceDescriptor
    PropPhysicsResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const PropPhysicsDataHeader* lpHeader =
            static_cast<const PropPhysicsDataHeader*>(lpResource);

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lpHeader->GetSizeInBytes();
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // @ 0x8267F750 -- tail-call PropPhysicsDataHeader::FixDown(lrResource).
    void PropPhysicsResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        _AssertWireContract();
        static_cast<PropPhysicsDataHeader*>(lpResource)->FixDown(lrResource);
    }

    // @ 0x8267F760 -- tail-call PropPhysicsDataHeader::FixUp(lrResource).
    void PropPhysicsResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        _AssertWireContract();
        static_cast<PropPhysicsDataHeader*>(lpResource)->FixUp(lrResource);
    }

    // Porter-contract pins. The blob this handler fixes up is emitted by
    // tools/assets/bundles/world_type_transcode.py::transcode_propphysics, so the host
    // layout of every record it walks is a wire contract; drift here must be a compile
    // error, not a silent misread at boot. Delegates to the owning headers' own pin blocks
    // (which cover the three table offsets, muTimeStamp, the 0x5918 header size, and the
    // 112 / 64 / 96 record strides) and adds the descriptor's own assumptions.
    void PropPhysicsResourceType::_AssertWireContract()
    {
        PropPhysicsDataHeader::_AssertLayout();
        static_assert(sizeof(CgsResource::ResourceDescriptor) ==
                      5 * sizeof(rw::BaseResourceDescriptor),
                      "the serialised descriptor is five (size, alignment) entries");
    }
}
}
