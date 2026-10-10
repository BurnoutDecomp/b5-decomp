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
// The header member names are the declaration's (clusteredmeshcluster.h); the
// unit-record decode is reconstructed from the console asm (pure integer
// bit-twiddling; no SIMD).
//
// Cluster header (the field the function touches):
//     +0x04  muUnitDataStart : u16 -- the unit data starts at
//                                     this+0x10*(muUnitDataStart+1) (header row + the
//                                     16-byte-strided vertex array).
//
// Unit record (at this + a2 + 0x10*(muUnitDataStart+1)):
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

    // The header, by its declaration's names (clusteredmeshcluster.h).
    u16 muUnitCount;       // +0x00  unitCount
    u16 muUnitDataSize;    // +0x02  unitDataSize (bytes of unit records)
    u16 muUnitDataStart;   // +0x04  unitDataStart: the unit records start at
                           //        this + 0x10 * (muUnitDataStart + 1), i.e. that many
                           //        16-byte vertex rows past the header row
    u16 muNormalStart;     // +0x06  normalStart
    u16 muTotalSize;       // +0x08  totalSize
    u8  muVertexCount;     // +0x0A  vertexCount
    u8  muNormalCount;     // +0x0B  normalCount
    u8  muCompressionMode; // +0x0C  compressionMode (1 = 16-bit, 2 = 32-bit, else float rows)
    u8  mauPadding[3];     // +0x0D  padding
    // The vertex array (vertexArray, +0x10) and the unit records follow; they are
    // addressed by byte offset from `this`, not by a named member.
};

} // namespace collision
} // namespace rw
