#ifndef CGS_POLYGON_SOUP_POLY_H
#define CGS_POLYGON_SOUP_POLY_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // VecFloat (rw::math::vpu::Vector4)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h
//
// CgsGeometric::PolygonSoupPoly -- one polygon record inside a relocated
// PolygonSoup collision resource. OWNING home reconstructed from the X360
// ARTIST build:
//
//   CgsGeometric::PolygonSoupPoly::GetEdgeCosineUncompressed @ 0x82839568  (38)
//   CgsGeometric::PolygonSoupPoly::LoadEdgeCosines           @ 0x8283A120 (534)
//
// SOURCE PATH, read out of the image (the file string the LoadEdgeCosines
// asserts carry, @0x820DF680):
//   d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\geometric\primitives\
//   polygonsoup\CgsPolygonSoupPoly.h
//
// ⭐⭐ 2026-08-11 (the extractor wave): THE OPAQUE 8-BYTE HEADER IS RETIRED.
// This header used to carry `u8 mauHeader[8]` with the note "an opaque on-disk
// record header that this function never reads ... sized (not interpreted) to
// keep the +0x08 displacement exact". ExtractTriangle4ListIntersectingSphere
// @0x82844C80 reads every one of those eight bytes, so they are named now:
//
//   +0x00  u32 muSurfaceTag              `lwz r29, 0(r31)` @0x82844D58, handed to
//                                        PolySoupAddToTriangleBuffer as
//                                        luSurfaceTag and ending up in
//                                        Triangle4::mSurfaceTags.
//   +0x04  u8  mau8VertexIndex[4]        `lbz r11, 4(r31)` .. `lbz r6, 7(r31)`,
//                                        each `rotlwi ,4` (x16) into the unpacked
//                                        vertex buffer. Slot 3 is
//                                        KU8_POLYGON_NO_VERTEX (0xFF) on a
//                                        TRIANGLE record and the triangle paths
//                                        never read it.
//   +0x08  u8  mauCompressedEdgeCosines[4]
//                                        one compressed cosine exponent per edge;
//                                        the low 5 bits are the shift.
//   sizeof == 12 == CgsPolygonSoup.h's KI_POLYGON_STRIDE.
//
// ⭐ SERIALISED. This record lives inside WORLDCOL.BIN, so every slot keeps its
// console width; it is pointer-free, so it is pinnable and the sizeof gate in
// the .cpp is meaningful.
//
// ⭐ THE PERIMETER ORDER, and it is the key to reading LoadEdgeCosines' outputs.
// CgsPolygonSoup.h:25 already records, MEASURED over WORLDCOL.BIN, that "a quad's
// four indices are a STRIP, not a fan: they run 0-1-3-2 around the perimeter".
// LoadEdgeCosines emits one cosine per vertex slot, for the perimeter edge
// LEAVING that vertex:
//   quad     : out0 = V0->V1   out1 = V1->V3   out2 = V2->V0   out3 = V3->V2
//   triangle : out0 = V0->V1   out1 = V1->V2   out2 = V2->V0   out3 = unused
// which is exactly how the extractor re-associates them onto its two split
// triangles (0,1,2) and (3,2,1) -- proved by matching the four output pointers
// against the four PolySoupAddToTriangleBuffer argument slots at 0x82844E60,
// 0x82844EC0, 0x82844F20 and 0x82844F80.
// ============================================================================

namespace CgsGeometric
{
    struct PolygonSoupPoly
    {
        // +0x00  the polygon's surface tag, read whole as a u32.
        u32 muSurfaceTag;

        // +0x04  four vertex indices into the soup's packed vertex array.
        u8 mau8VertexIndex[4];

        // +0x08  One compressed cosine exponent per edge (0..3). The low 5 bits
        // (& 0x1F) of each byte are the shift used to reconstruct the cosine.
        u8 mauCompressedEdgeCosines[4];

        // GetEdgeCosineUncompressed @ 0x82839568 -- decode the cosine of the
        // angle across edge `liEdge`. Asserts liEdge in [0,4). Returns a double.
        double GetEdgeCosineUncompressed(u32 liEdge) const;

        // LoadEdgeCosines @0x8283A120 -- decode all four at once into broadcast
        // VecFloats. Same formula as GetEdgeCosineUncompressed, computed in VMX.
        //
        // DWARF (PS3 twin @0xB7024C, mangled
        //  _ZNK12CgsGeometric15PolygonSoupPoly15LoadEdgeCosinesERN2rw4math3vpu8VecFloatES5_S5_S5_):
        //   void LoadEdgeCosines(VecFloat& lOutEdge0, VecFloat& lOutEdge1,
        //                        VecFloat& lOutEdge2, VecFloat& lOutEdge3) const;
        // ⚠️ IDA's X360 prototype prints nine integer parameters and a trailing
        // `double a9`. The double is the `__savefpr_26` spill of the callee-save
        // FPRs the debug half uses, NOT a parameter (the standing "pseudocode
        // misattributes stack spills" rule); the DWARF arity of five is correct
        // and matches the five registers the body actually reads (r3..r7).
        void LoadEdgeCosines(VecFloat& lOutEdge0, VecFloat& lOutEdge1,
                             VecFloat& lOutEdge2, VecFloat& lOutEdge3) const;
    };
}

#endif // CGS_POLYGON_SOUP_POLY_H
