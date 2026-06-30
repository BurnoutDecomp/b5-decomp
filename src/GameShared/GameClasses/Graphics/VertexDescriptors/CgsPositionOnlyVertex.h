#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3 (rw::math::vpu::Vector3)

namespace renderengine
{
    class VertexDescriptor;
}

// CgsGraphics::PositionOnlyVertex - the position-only vertex used by the depth/occlusion
// immediate-mode 3D renderer (CgsGraphics::Im3dZOnly, the debug-line / occludee bounding-box
// path). Layout from the DecFIGS DWARF (CgsPositionOnlyVertex.h:42/44):
//   Vector3 mv3Pos;   // +0x00 (the only attested member)
// The vertex carries no colour / texcoord - it feeds a position-only vertex descriptor (the
// X360 Construct inlines FillVertexDescriptorParameters, building a single 16-byte position
// element; see CgsIm3dZOnly.cpp).
namespace CgsGraphics
{
    struct PositionOnlyVertex
    {
        Vector3 mv3Pos;  // DWARF CgsPositionOnlyVertex.h:44

        // DWARF CgsPositionOnlyVertex.h:60 -- fill the vertex-descriptor parameter block for a
        // position-only stream. The X360 ImRenderer<PositionOnlyVertex>::Construct inlines this
        // (no out-of-line body is attested for THIS TU), so it is declaration-only here.
        void FillVertexDescriptorParameters(renderengine::VertexDescriptor::Parameters& lrParameters);
    };
}
