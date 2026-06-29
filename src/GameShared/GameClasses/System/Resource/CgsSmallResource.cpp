#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

// SmallResource / SmallResourceDescriptor method bodies.
//
// SOURCE NOTE: the X360 build's small-resource header is CgsSmallResourceX360.h (its
// CreateFromRWResource asserts cite that path), distinct from the PS3 CgsSmallResourcePS3.h
// the DecFIGS DWARF carries (this file mirrors the PS3 header name in-tree, but the bodies
// are decompiled from the X360). The console maps its in-memory pools to rw::Resource slots:
//   small[0] (main)     <-> rw[0]
//   small[1] (graphics) <-> rw[2]
// Recovered from SmallResource::CreateFromRWResource (0x828EB6E8 -- writes small[0]=rw[0],
// small[1]=rw[2] and asserts the other rw slots are empty) and the inlined ConvertToRWResource
// in Pool::FixUpEntry (0x828EB860 -- builds rw[0]=small[0], rw[2]=small[1]).

namespace CgsResource
{
    // small -> rw (build the rw::Resource view FixUp/PostFixUp/DeSerialise operate on).
    void SmallResource::ConvertToRWResource(rw::Resource& lrResource)
    {
        lrResource.m_baseResources[0] = m_baseResources[0];   // main
        lrResource.m_baseResources[2] = m_baseResources[1];   // graphics
    }

    // rw -> small (0x828EB6E8): take the two populated rw slots (main=0, graphics=2) into the
    // compact 3-pool form; the other serialised categories must be empty. The X360 fires THREE
    // "unitialized memory" asserts (slots 3, 4, 1 -- asm 0x828EB708/EB788/EB7E8, lines 200/201/202)
    // over the 5-slot serialised resource (PC rwcore narrowed rw::Resource to <4>; we take the
    // <5> form here, paralleling ResourceDescriptor=<5>). Then it writes ONLY small[0] and small[1].
    void SmallResource::CreateFromRWResource(const rw::BaseResources<5>& lrResource)
    {
        CGS_ASSERT(lrResource.m_baseResources[3] == 0, "Can not convert from rw resource with unitialized memory\n");
        CGS_ASSERT(lrResource.m_baseResources[4] == 0, "Can not convert from rw resource with unitialized memory\n");
        CGS_ASSERT(lrResource.m_baseResources[1] == 0, "Can not convert from rw resource with unitialized memory\n");
        m_baseResources[0] = lrResource.m_baseResources[0];   // main      <- rw[0]
        m_baseResources[1] = lrResource.m_baseResources[2];   // graphics  <- rw[2]
    }

    // rw -> small descriptor (0x826661F8): map the serialised 5-entry resource descriptor's
    // populated categories (main=0, videomapped=2) into the 3-pool runtime descriptor; entries
    // 1/3/4 must be unused (size <= 1).
    void SmallResourceDescriptor::CreateFromRWDescriptor(const ResourceDescriptor& lrDescriptor)
    {
        CGS_ASSERT(lrDescriptor.m_baseResourceDescriptors[1].m_size <= 1u, "Can not convert from rw descriptor with unitialized memory\n");
        CGS_ASSERT(lrDescriptor.m_baseResourceDescriptors[3].m_size <= 1u, "Can not convert from rw descriptor with unitialized memory\n");
        CGS_ASSERT(lrDescriptor.m_baseResourceDescriptors[4].m_size <= 1u, "Can not convert from rw descriptor with unitialized memory\n");
        m_baseResourceDescriptors[0] = lrDescriptor.m_baseResourceDescriptors[0];   // main     <- serialised[0]
        m_baseResourceDescriptors[1] = lrDescriptor.m_baseResourceDescriptors[2];   // graphics <- serialised[2]
    }
}
