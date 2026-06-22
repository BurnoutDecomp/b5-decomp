#pragma once

// CgsGeometric::PolygonSoup — a packed soup of polygons over a shared vertex pool.
// Polygons index into the vertex pool; both arrays are stored inline after a small
// header. Accessors below return a pointer (byte cursor) to the requested record.
//
// Layout from the X360 asm (CgsPolygonSoup.h source path attested by the asserts in
// GetPolygon @ 0x8283A978 / GetVertex @ 0x8283A9F0):
//   +0x00  header bytes (overall AABB / flags — not touched by these accessors)
//   +0x10  Polygon* base pointer (inline polygon array; 12-byte stride)
//   +0x14  Vertex*  base pointer (inline packed-vertex array; 6-byte stride)
//   +0x1A  u8       number of polygons
//   +0x1C  u8       number of vertices
//
// Only the two asserted accessors run in these TUs; the Polygon (12-byte) and packed
// Vertex (6-byte) record types are consumed elsewhere and are referenced here purely
// by byte stride, so they are not defined in this home.
#include "types.hpp"

namespace CgsGeometric
{
    // Record strides taken from the index arithmetic in the accessors:
    //   GetPolygon: 12 * index  (slwi r,1 + add + slwi r,2  ==  index*3*4)
    //   GetVertex:   6 * index  (slwi r,1 + add + slwi r,1  ==  index*3*2)
    const s32 KI_POLYGON_STRIDE = 12;
    const s32 KI_VERTEX_STRIDE  = 6;

    struct PolygonSoup
    {
        u8  m_0[16];          // +0x00  header (AABB/flags); not referenced by the accessors
        u8* mpPolygons;       // +0x10  base of the inline polygon array (byte cursor)
        u8* mpVertices;       // +0x14  base of the inline packed-vertex array (byte cursor)
        u8  m_18[2];          // +0x18  padding/other header bytes
        u8  mu8NumPolygons;   // +0x1A  polygon count
        u8  m_1B;             // +0x1B  padding/other header byte
        u8  mu8NumVertices;   // +0x1C  vertex count

        // GetPolygon @ 0x8283A978 — bounds-asserted accessor; returns &polygon[index].
        // The X360 index arg is an unsigned __int8 (clrlwi r,24).
        u8* GetPolygon(u8 lu8PolyIndex) const;

        // GetVertex @ 0x8283A9F0 — bounds-asserted accessor; returns &vertex[index].
        // The X360 index arg is an unsigned __int16 (clrlwi r,16) compared against the
        // u8 vertex count.
        u8* GetVertex(u16 lu16VertexIndex) const;

        u8 GetNumPolygons() const { return mu8NumPolygons; }
        u8 GetNumVertices() const { return mu8NumVertices; }
    };
}
