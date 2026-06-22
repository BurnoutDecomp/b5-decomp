#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGeometric::PolygonSoup:
//   GetPolygon @ 0x8283A978 — return &mpPolygons[12 * lu8PolyIndex], asserting the index
//                             is in range (lu8PolyIndex < GetNumPolygons()).
//   GetVertex  @ 0x8283A9F0 — return &mpVertices[6 * lu16VertexIndex], asserting the index
//                             is in range (lu16VertexIndex < GetNumVertices()).
//
// Behaviour-faithful to the X360 pseudocode. The asm reads the index into r31 (after the
// clrlwi width truncation), bounds-checks it against the u8 count, then computes
// base + stride*index and returns that byte cursor. The leading int arg = `this`.

namespace CgsGeometric
{
    // GetPolygon @ 0x8283A978
    // X360: `v3 = a2; if (a2 >= *(this+0x1A)) Assert(...); return 12*v3 + *(this+0x10);`
    // Index widened from u8 (clrlwi r,24); stride 12 (slwi/add/slwi == index*3*4).
    u8* PolygonSoup::GetPolygon(u8 lu8PolyIndex) const
    {
        CGS_ASSERT(lu8PolyIndex < mu8NumPolygons, "lu8PolyIndex < GetNumPolygons()");
        return mpPolygons + KI_POLYGON_STRIDE * static_cast<s32>(lu8PolyIndex);
    }

    // GetVertex @ 0x8283A9F0
    // X360: `v3 = a2; if (a2 >= *(this+0x1C)) Assert(...); return 6*v3 + *(this+0x14);`
    // Index widened from u16 (clrlwi r,16), compared against the u8 vertex count;
    // stride 6 (slwi/add/slwi == index*3*2).
    u8* PolygonSoup::GetVertex(u16 lu16VertexIndex) const
    {
        CGS_ASSERT(lu16VertexIndex < mu8NumVertices, "lu8VertexIndex < GetNumVertices()");
        return mpVertices + KI_VERTEX_STRIDE * static_cast<s32>(lu16VertexIndex);
    }
}
