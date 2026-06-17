#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"

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
}
