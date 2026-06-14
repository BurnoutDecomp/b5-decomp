#include "SharedClasses/AI/AISectionsResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::AISectionsResourceType::FixDown                       @ 0x8267F4A0
//   BrnAI::AISectionsResourceType::FixUp                         @ 0x8267DB28
//   BrnAI::AISectionsResourceType::GetSerialisedResourceDescriptor @ 0x8267AE88
//   BrnAI::AISectionsResourceType::GetTypeID                     @ 0x82674D50
//
// FixUp/FixDown forward to BrnAI::AISectionsData (own TU), passing the delta (the
// rw::Resource's load base). GetSerialisedResourceDescriptor returns a five-entry
// descriptor: entry 0 = {16, count}, entries 1..4 = {count, 1}, count read at +60.

namespace BrnAI
{
    static const uint32_t KU_AI_SECTIONS_RESOURCE_TYPE_ID = 65537;

    uint32_t AISectionsResourceType::GetTypeID() const
    {
        return KU_AI_SECTIONS_RESOURCE_TYPE_ID;
    }

    void AISectionsResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<AISectionsData*>(lpResource)->FixDown(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    void AISectionsResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<AISectionsData*>(lpResource)->FixUp(static_cast<int>(CgsResource::GetLoadBase(lrResource)));
    }

    CgsResource::ResourceDescriptor AISectionsResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        u32 luCount = *reinterpret_cast<const u32*>(reinterpret_cast<uintptr_t>(lpResource) + 60);

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 16;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = luCount;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = luCount;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}
