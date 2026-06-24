#include "SharedClasses/Sound/Engines/BrnSoundLoopModelResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixDown   @ 0x826801E0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::FixUp     @ 0x826801D0
//   BrnSound::Vehicles::Engines::LoopModelResourceType::GetTypeID @ 0x82675570
//
// FixUp/FixDown forward to LoopModelData (own TU), passing the delta (the
// rw::Resource's load base).

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
    static const uint32_t KU_LOOP_MODEL_RESOURCE_TYPE_ID = 0x10000;

    uint32_t LoopModelResourceType::GetTypeID() const
    {
        return KU_LOOP_MODEL_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267AED0 (store-for-store). The serialised
    // loop-model occupies one block whose size is the LoopModelData::muSizeInBytes
    // word the X360 reads at +0x10 (`lwz r9, 0x10(r5)`), with alignment 1 (the std
    // packs {size = muSizeInBytes, align = 1}); entries 1..4 are {0,1}.
    CgsResource::ResourceDescriptor
    LoopModelResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const LoopModelData* lpData = static_cast<const LoopModelData*>(lpResource);

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lpData->muSizeInBytes;  // entry0 size  (*(a3+0x10))
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 1u;                     // entry0 align (high dword = 1)
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    void LoopModelResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<LoopModelData*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    void LoopModelResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<LoopModelData*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }
}
}
}
