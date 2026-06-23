#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::ClusteredMeshCluster -- one cluster of a RenderWare clustered
// collision mesh. A cluster packs a vertex array followed by a stream of
// variable-length "unit" records (triangles / quads, optionally carrying a
// per-unit group id and surface id). CgsSceneManager's fine line-test reads the
// group/surface ids of the unit a ray hit.
//
// OWNING HOME for the single function the X360 binary defines:
//     rw::collision::ClusteredMeshCluster::GetGroupAndSurfaceId  @ 0x828A9C70
//
// No DWARF hints exist for this TU, so the cluster header
// shape and the unit-record decode are reconstructed from the X360 PPC asm
// (pure integer bit-twiddling; no SIMD).
//
// Cluster header (only the fields the function touches):
//     +0x00  ... (cluster flags, not read here)
//     +0x04  muUnitCount : u16   -- number of vertices; the unit data starts at
//                                   this+0x10*(muUnitCount+1) (header row + the
//                                   16-byte-strided vertex array).
//
// Unit record (at this + a2 + 0x10*(muUnitCount+1)):
//     [0]  byte mFlags  -- low nibble selects the type (vertex count code); bit
//                          0x20 = an extra vertex index byte present; bit 0x40 =
//                          a group id follows; bit 0x80 = a surface id follows.
//     [1]  byte mEdge   -- per-edge cosine code (masked out when nibble != 3).
//   then the vertex indices, then the optional group id / surface id bytes.
//
// CompressionInfo (a3): the two mode bytes that say whether the group/surface
// ids are stored 8-bit or 16-bit:
//     +0x06  mGroupIdMode   : u8  (2 == 16-bit)
//     +0x07  mSurfaceIdMode : u8  (2 == 16-bit)
// ===========================================================================

namespace rw
{
namespace collision
{

class ClusteredMeshCluster
{
public:
    // The two id-width modes carried alongside a clustered mesh.
    struct CompressionInfo
    {
        u8 maPad[6];        // +0x00  (other compression flags, not read here)
        u8 mGroupIdMode;    // +0x06  2 == group id stored as 16-bit
        u8 mSurfaceIdMode;  // +0x07  2 == surface id stored as 16-bit
    };

    // @ 0x828A9C70 -- decode the group id / surface id of the unit at byte
    // offset liUnitOffset, writing them to *lpGroupId / *lpSurfaceId. The 64-bit
    // return value mirrors the asm (its HIDWORD is the unit's edge byte when the
    // nibble == 3, its LODWORD a 0/1 "is-quad" flag); callers in this build use
    // the two out-params.
    u64 GetGroupAndSurfaceId(int liUnitOffset,
                             const CompressionInfo* lpInfo,
                             u32* lpGroupId,
                             u32* lpSurfaceId) const;

    u16 muReserved0;    // +0x00  (cluster flags; not read here)
    u16 muReserved2;    // +0x02  (not read here)
    u16 muUnitCount;    // +0x04  vertex count (lhz 4(r3))
    u16 muReserved6;    // +0x06  pads the header to 0x08
    // The variable-length unit/vertex data follows; it is addressed by byte
    // offset from `this` (see GetGroupAndSurfaceId), not by a named member.
};

} // namespace collision
} // namespace rw
