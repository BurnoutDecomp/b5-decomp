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
// ⭐ 2026-08-02 (car-placement wave): the four "header (AABB/flags)" words at +0x00 and the
// spare byte at +0x1B are NAMED, not opaque, and both attestations are already committed in
// tools/assets/bundles/world_support_transcode.py's banner:
//   +0x00..+0x0B  s32 miPosX / miPosY / miPosZ   the soup's integer origin
//   +0x0C         f32 mfScale                     the packed-vertex quantum
//                 UnpackPolygonSoupVertices @0x8283B480: world = (pos_s32 + vert_s16) * scale
//   +0x1B         u8  mu8NumQuads                 the poly array is QUADS FIRST
//                 ExtractTriangle4ListIntersectingSphere @0x82844C80 reads +0x1A/+0x1B as
//                 the polygon / quad counts; a record past mu8NumQuads is a triangle and
//                 carries 0xFF in its fourth vertex index.
// ⭐ AND A QUAD'S FOUR INDICES ARE A STRIP, NOT A FAN: they run 0-1-3-2 around the
// perimeter, so the two triangles are (0,1,2) and (1,3,2). Measured over WORLDCOL.BIN --
// the junkyard floor quad (res bf2191aa, soup 86, poly 3) is
//   v0 (2986.305, -3.525, -2005.995)  v1 (2988.315, -3.525, -2005.920)
//   v2 (2986.185, -3.525, -2011.500)  v3 (2989.455, -3.525, -2011.365)
// which is a rectangle only in strip order; the fan split (0,1,2)+(0,2,3) makes a bow tie
// whose halves carry opposite normals.
// ⚠️ The winding is NOT a reliable "up": that junkyard floor quad winds to (0,+1,0) while
// the road quad 70 m away (res b5515579, soup 1, poly 17, y = 1.065) winds to (0,-1,0).
// Anything testing these surfaces must be double-sided.
// Naming them changes no offset and no size (three s32 + one f32 == the old u8[16]); the
// transcoder emits exactly these fields, and the place-on-track drop query reads them.
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

    // One packed polygon record (12 bytes, CgsPolygonSoupPoly.h): the surface tag is read
    // whole as a u32 by ExtractTriangle4ListIntersectingSphere and stored per-lane into
    // Triangle4::mSurfaceTags.
    const u8 KU8_POLYGON_NO_VERTEX = 0xFF;   // slot 3 of a TRIANGLE record

    struct PolygonSoup
    {
        s32 miPosX;           // +0x00  soup origin, in vertex quanta
        s32 miPosY;           // +0x04
        s32 miPosZ;           // +0x08
        f32 mfScale;          // +0x0C  world = (miPos* + vertex_s16) * mfScale
        u8* mpPolygons;       // +0x10  base of the inline polygon array (byte cursor)
        u8* mpVertices;       // +0x14  base of the inline packed-vertex array (byte cursor)
        u8  m_18[2];          // +0x18  the u16 soup byte size
        u8  mu8NumPolygons;   // +0x1A  polygon count
        u8  mu8NumQuads;      // +0x1B  how many of them are QUADS (they come first)
        u8  mu8NumVertices;   // +0x1C  vertex count

        // GetPolygon @ 0x8283A978 — bounds-asserted accessor; returns &polygon[index].
        // The X360 index arg is an unsigned __int8 (clrlwi r,24).
        u8* GetPolygon(u8 lu8PolyIndex) const;

        // GetVertex @ 0x8283A9F0 — bounds-asserted accessor; returns &vertex[index].
        // The X360 index arg is an unsigned __int16 (clrlwi r,16) compared against the
        // u8 vertex count.
        u8* GetVertex(u16 lu16VertexIndex) const;

        u8 GetNumPolygons() const { return mu8NumPolygons; }
        u8 GetNumQuads()    const { return mu8NumQuads; }
        u8 GetNumVertices() const { return mu8NumVertices; }
    };
}
