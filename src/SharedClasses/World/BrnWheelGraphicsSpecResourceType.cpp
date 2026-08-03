#include "SharedClasses/World/BrnWheelGraphicsSpecResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the descriptor body
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"   // BrnWheel::GraphicsSpec (the record)

namespace BrnWheel
{

// Resource registry type id for the wheel graphics-spec resource (0x1000A).
// Recovered verbatim from GetTypeID @ 0x82676490.
static const uint32_t KU_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_ID = 65546;

uint32_t GraphicsSpecResourceType::GetTypeID() const
{
    return KU_WHEEL_GRAPHICS_SPEC_RESOURCE_TYPE_ID;
}

// GetSerialisedResourceDescriptor @ 0x8267D478 (store-for-store). A constant
// descriptor: the wheel graphics-spec serialises into a single 16-byte-aligned
// block of fixed size 0xC -- which is exactly sizeof(GraphicsSpec), the three
// 32-bit slots the header declares (the std at +0 packs {size=0xC, align=0x10} on the big-endian
// image: back_chain+0=0xC -> result+0=m_size, back_chain+4=0x10 -> result+4=m_alignment;
// entries 1..4 are {0,1}). lpResource is unread by the X360 body.
CgsResource::ResourceDescriptor
GraphicsSpecResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    (void)lpResource;

    CgsResource::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = 0x0Cu;  // entry0 m_size @result+0 (back_chain+0 = 0xC)
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;  // entry0 m_alignment @result+4 (back_chain+4 = 0x10)
    for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
    {
        lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
        lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
    }
    return lDescriptor;
}

// FixUp @ 0x82678CF0. The X360 signature is FixUp(this, lpResource); the
// rw::Resource& is part of the virtual contract but this handler performs no
// relocation (version-check only — no field clear, no pointer fix-up), so
// lrResource is unused here.
void GraphicsSpecResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    (void)lrResource;

    const GraphicsSpec* lpGraphicsSpec = static_cast<const GraphicsSpec*>(lpResource);

    CGS_ASSERT(lpGraphicsSpec->muVersion == KU_WHEEL_GRAPHICS_SPEC_VERSION,
               "Incorrect version for wheel Graphics Spec.  Get latest code/tools and rebuild data.  If that doesn't work, grab Keef! \n");
}

// The 12-byte descriptor above is the record's own size: three 32-bit slots, no
// padding. If this ever fails, the two model slots have been widened by mistake
// (see the header banner -- serialised slots stay 32-bit on x64).
static_assert(sizeof(GraphicsSpec) == 0x0C, "BrnWheel::GraphicsSpec must be the console's 12-byte serialised record");

}
