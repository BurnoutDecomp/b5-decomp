#include "vendor/renderware/collision/ClusteredMeshCluster.hpp"

// ===========================================================================
// rw::collision::ClusteredMeshCluster -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
//   ClusteredMeshCluster::GetGroupAndSurfaceId  @ 0x828A9C70
//
// Pure integer bit-twiddling (no SIMD). Faithful transcription of the PPC asm /
// pseudocode: walk to the unit record, decode how many bytes of vertex indices
// precede the optional group/surface id bytes from the flags-byte low nibble
// and bit 0x20, then read the (8-bit or 16-bit, big-endian) ids. See the header
// for the record layout.
//
// The branchless asm uses the idiom  -(nib ^ K) >> 31  (arithmetic shift) which
// is 0xFFFFFFFF when nib != K and 0 when nib == K; the reconstruction spells
// those as the equivalent (nib == K) predicates.
// ===========================================================================

namespace rw
{
namespace collision
{

// 32-bit rotate-left (the asm's __ROL4__/rotlwi). Used to assemble the 16-bit
// big-endian id payloads: ROL(hiByte, 8) + loByte.
static inline u32 Rol4(u32 luValue, u32 luCount)
{
    return (luValue << luCount) | (luValue >> (32 - luCount));
}

// ---------------------------------------------------------------------------
// GetGroupAndSurfaceId @ 0x828A9C70
// ---------------------------------------------------------------------------
u64 ClusteredMeshCluster::GetGroupAndSurfaceId(int liUnitOffset,
                                               const CompressionInfo* lpInfo,
                                               u32* lpGroupId,
                                               u32* lpSurfaceId) const
{
    int liGroupId   = 0;   // v5 (r30)
    int liSurfaceId = 0;   // v6 (r31)

    // v7 = this + a2 + 0x10 * (muUnitCount + 1)
    const u8* lpBytes = reinterpret_cast<const u8*>(this);
    const u8* lpUnit  = lpBytes
                      + (16 * (static_cast<int>(muUnitCount) + 1))
                      + liUnitOffset;

    const u8 luFlags = lpUnit[0];          // v8
    const int liNib  = lpUnit[0] & 0xF;    // low nibble (unit type code)

    // result halves (returned; HI carries the edge byte for nibble == 3).
    const bool lbIsType3 = (liNib == 3);
    const u32  luHi      = lbIsType3 ? lpUnit[1] : 0u;   // HIDWORD(result)
    const u32  luLo      = lbIsType3 ? 1u : 0u;          // LODWORD(result)

    // v10: bytes of index data implied by the nibble.
    const int liV10 = ((liNib == 2) ? 2 : 0)
                    + ((liNib == 1) ? 1 : 0)
                    + static_cast<int>(luHi);

    // v11: pointer past the vertex indices to the optional id bytes.
    const int liExtra = ((luFlags & 0x20) != 0) ? (liV10 + 2) : 0;
    const u8* lpId    = lpUnit + (liExtra + 3 + static_cast<int>(luLo) + liV10);

    // Optional group id (flags bit 0x40): 8-bit, or 16-bit big-endian when the
    // group id mode is 2.
    if ((luFlags & 0x40) != 0)
    {
        const int liByte0 = *lpId++;       // v12
        int liHiPart = 0;                  // v14
        if (lpInfo->mGroupIdMode == 2)
        {
            const int liByte1 = *lpId++;   // v13
            liHiPart = static_cast<int>(Rol4(static_cast<u32>(liByte1), 8));
        }
        liGroupId = liHiPart + liByte0;
    }

    // Optional surface id (flags bit 0x80): same 8-bit / 16-bit-BE scheme.
    if ((luFlags & 0x80) != 0)
    {
        const int liByte0 = *lpId;         // v15
        if (lpInfo->mSurfaceIdMode == 2)
        {
            const int liByte1 = lpId[1];   // v16
            liSurfaceId = static_cast<int>(Rol4(static_cast<u32>(liByte1), 8)) + liByte0;
        }
        else
        {
            liSurfaceId = liByte0;
        }
    }

    *lpGroupId   = static_cast<u32>(liGroupId);
    *lpSurfaceId = static_cast<u32>(liSurfaceId);

    return (static_cast<u64>(luHi) << 32) | static_cast<u64>(luLo);
}

} // namespace collision
} // namespace rw
