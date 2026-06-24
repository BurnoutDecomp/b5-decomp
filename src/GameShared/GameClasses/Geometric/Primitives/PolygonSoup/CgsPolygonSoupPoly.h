#ifndef CGS_POLYGON_SOUP_POLY_H
#define CGS_POLYGON_SOUP_POLY_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h
//
// CgsGeometric::PolygonSoupPoly -- one polygon record inside a relocated
// PolygonSoup collision resource. OWNING home reconstructed from the X360
// ARTIST build (CgsGeometric::PolygonSoupPoly::GetEdgeCosineUncompressed
// @ 0x82839568).
//
// Only the +0x08 sub-field is touched by the recovered TU; the leading 8 bytes
// are an opaque on-disk record header that this function never reads, so they
// are sized (not interpreted) to keep the +0x08 displacement exact.
//
// LAYOUT (proven by the GetEdgeCosineUncompressed asm):
//   GetEdgeCosineUncompressed @ 0x82839568:
//       add  r11, r31, r30     ; r11 = this + liEdge
//       lbz  r11, 8(r11)       ; load BYTE at this + liEdge + 0x08
//   => a 4-element byte array of compressed edge-cosine exponents at +0x08,
//      indexed by liEdge (0..3).
//
//   +0x00  u8 [8]   opaque on-disk record header (NOT read by this TU)
//   +0x08  u8 [4]   compressed per-edge cosine exponents (indexed by liEdge)
// ============================================================================

namespace CgsGeometric
{
    struct PolygonSoupPoly
    {
        // +0x00  Opaque leading bytes of the on-disk poly record. The recovered
        // TU never reads them; sized only to anchor the +0x08 displacement.
        // FLAGGED opaque: interior layout not attested by this TU/the DWARF.
        u8 mauHeader[8];

        // +0x08  One compressed cosine exponent per edge (0..3). The low 5 bits
        // (& 0x1F) of each byte are the shift used to reconstruct the cosine.
        u8 mauCompressedEdgeCosines[4];

        // GetEdgeCosineUncompressed @ 0x82839568 -- decode the cosine of the
        // angle across edge `liEdge`. Asserts liEdge in [0,4). Returns a double.
        double GetEdgeCosineUncompressed(u32 liEdge) const;
    };
}

#endif // CGS_POLYGON_SOUP_POLY_H
