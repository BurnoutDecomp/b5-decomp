// GameSource/Effects/Particles/Native/BrnSkidVertex.h
//
// BrnGraphics::SkidVertex -- the skid-trail decal vertex (a Vector3 position + a Vector4
// UV/time/alpha attribute) and its writing iterator, feeding the
// CgsGraphics::ImRenderer<BrnGraphics::SkidVertex> instantiation TU (BrnSkidVertex.cpp).
//
// DWARF AUTHORITY (DecFIGS dumps -- GameSource/Effects/Particles/Native/BrnSkidVertex.h):
//   BrnSkidVertex.h:45  struct BrnGraphics::SkidVertex {
//                           Vector3 mv3Pos;          // :47
//                           Vector4 mv4UvTimeAlpha;  // :48
//                           void FillVertexDescriptorParameters(
//                               renderengine::VertexDescriptor::Parameters&); // :50
//                       }
//   BrnSkidVertex.h:64  struct SkidVertex::VertexIterator : public VertexIteratorBaseClass {
//                           void Write(const BrnGraphics::SkidVertex&); // :68
//                           uint32_t GetStride();                       // :75
//                       }
//
// Vector3 / Vector4 are the 16-byte rw::math::vpu types (BrnCommonTypes.h), so the in-memory
// SkidVertex is 32 bytes (position 16 + attribute 16). The GPU stream stride the Render path submits
// is 28 bytes (li r6, 0x1C in ImRenderer<SkidVertex>::Render @ 0x8228E068): pos.xyz (12) + uv/time/
// alpha.xyzw (16), skipping the position's w-pad lane. Only the SkidVertex member bodies of
// ImRenderer<SkidVertex> (AddProgram / BeginRendering / Construct / Render / SetProgram) live in the
// .cpp; the descriptor element words / offsets / stride all come from the X360 asm immediates.
#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector4 (rw::math::vpu)
// renderengine::VertexDescriptor::Parameters is named (by reference) in the declaration
// of FillVertexDescriptorParameters below, so the full descriptor type must be visible.
#include "pc/gcm/renderengine/VertexDescriptor.h"

namespace BrnGraphics
{
    // BrnSkidVertex.h:45 (DWARF). The skid-trail decal vertex.
    struct SkidVertex
    {
        Vector3 mv3Pos;          // BrnSkidVertex.h:47  (+0x00, 16-byte vpu; xyz meaningful)
        Vector4 mv4UvTimeAlpha;  // BrnSkidVertex.h:48  (+0x10, uv.xy + time + alpha packed in a Vector4)

        // BrnSkidVertex.h:50 -- fill the vertex-descriptor parameter block for the skid stream. The
        // X360 ImRenderer<SkidVertex>::Construct inlines this (no out-of-line body attested for this
        // TU), so it is declaration-only here.
        void FillVertexDescriptorParameters(renderengine::VertexDescriptor::Parameters& lrParameters);
    };
}
