#include "SharedClasses/Graphics/PlayerCarColoursResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "types.hpp"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PlayerCarColoursResourceType::FixDown                           @ 0x8267E090
//   CgsResource::PlayerCarColoursResourceType::FixUp                             @ 0x8267E058
//   CgsResource::PlayerCarColoursResourceType::Serialise                         @ 0x8267DF80
//   CgsResource::PlayerCarColoursResourceType::GetSerialisedResourceDescriptor   @ 0x8267C568
//
// The serialised payload is a BrnWorld::GlobalColourPalette: four
// PlayerCarColourPalette entries (DWARF PlayerCarColours.h), each 12 bytes:
//   +0 Vector4* mpPaintColours
//   +4 Vector4* mpPearlColours
//   +8 s32      miNumColours
// FixUp/FixDown walk the four entries (stride 12 == 3 dwords) and rebase the two
// colour-array pointers (+0, +4) by the load-base delta read from the rw::Resource
// arg (rw::Resource::m_baseResources[0]). The pointers are adjusted unconditionally
// (no null guard) to match the X360 exactly. The payload is accessed by dword
// offset (the serialised layout, not a host struct).

namespace CgsResource
{
    static const u32 KU_NUM_PALETTES         = 4;   // BrnWorld::eNumPalettes
    static const u32 KU_PALETTE_STRIDE_DWORD = 3;   // sizeof(PlayerCarColourPalette)/4

    // GetSerialisedResourceDescriptor @ 0x8267C568 (store-for-store). Builds the five-entry
    // serialised descriptor:
    //   entry0 = { size = 16 * (*(res+0x2C)) + (*(res+0x28)) - res, align = 16 }
    //   entry1..4 = { size = 0, align = 1 }
    // The size is the byte span from the resource base to the end of the colour payload: the
    // header words at +0x28 (the colour-array base pointer) and +0x2C (the 16-byte-entry count)
    // give end = base + 16*count, and (end - res) is the total serialised size. The X360 writes
    // all five alignments (1) and four trailing sizes (0) first, then one 64-bit store of
    // {size, align=16} overwrites entry0 -- so the rest are {0,1}. Matches the copy-size formula
    // the Serialise body below uses.
    ResourceDescriptor PlayerCarColoursResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpSrc = reinterpret_cast<const u32*>(lpResource);
        u32        luRes = reinterpret_cast<u32>(lpResource);
        u32        luSize = 16u * lpSrc[0x2C / 4] + lpSrc[0x28 / 4] - luRes;

        ResourceDescriptor lDescriptor;
        u32* lpData = reinterpret_cast<u32*>(&lDescriptor);
        lpData[0] = luSize;  // entry0 size
        lpData[1] = 16u;     // entry0 align
        lpData[2] = 0u;  lpData[3] = 1u;   // entry1 {0,1}
        lpData[4] = 0u;  lpData[5] = 1u;   // entry2 {0,1}
        lpData[6] = 0u;  lpData[7] = 1u;   // entry3 {0,1}
        lpData[8] = 0u;  lpData[9] = 1u;   // entry4 {0,1}
        return lDescriptor;
    }

    // FixDown: subtract the delta from both pointers of each palette entry. @ 0x8267E090
    void PlayerCarColoursResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        u32 luDelta = reinterpret_cast<u32>(lrResource.m_baseResources[0]);
        u32* lpEntry = reinterpret_cast<u32*>(lpResource);
        for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
        {
            lpEntry[0] -= luDelta;   // mpPaintColours
            lpEntry[1] -= luDelta;   // mpPearlColours
            lpEntry += KU_PALETTE_STRIDE_DWORD;
        }
    }

    // FixUp: add the delta to both pointers of each palette entry. @ 0x8267E058
    void PlayerCarColoursResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u32 luDelta = reinterpret_cast<u32>(lrResource.m_baseResources[0]);
        u32* lpEntry = reinterpret_cast<u32*>(lpResource);
        for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
        {
            lpEntry[0] += luDelta;   // mpPaintColours
            lpEntry[1] += luDelta;   // mpPearlColours
            lpEntry += KU_PALETTE_STRIDE_DWORD;
        }
    }

    // Serialise: copy the source palette into the destination resource buffer, then
    // rebase the two pointer columns of every entry. The X360 (@ 0x8267DF80):
    //   * computes the copy size from the source header words at +0x28 and +0x2C
    //     (size = 16 * src[0x2C/4] + src[0x28/4] - src) -- the full palette payload,
    //   * FixDowns the SOURCE in place by its own base address (src),
    //   * memcpy's the payload to the destination base (rw::Resource::m_baseResources[0]),
    //   * FixUps the DESTINATION by the destination base address,
    //   * FixUps the SOURCE back to its loaded form (restores the in-place edit).
    // The trailing ResourceDescriptor& arg is unused (Hex-Rays drops it).
    void* PlayerCarColoursResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest,
                                                  const ResourceDescriptor& /*lrDescriptor*/) const
    {
        const u32* lpSrc  = reinterpret_cast<const u32*>(lpResource);
        u32        luSrc  = reinterpret_cast<u32>(lpResource);
        void*      lpDst  = lrDest.m_baseResources[0];
        u32        luDst  = reinterpret_cast<u32>(lpDst);

        // Copy size from the source's trailing header words (+0x28, +0x2C).
        u32 luSize = 16 * lpSrc[0x2C / 4] + lpSrc[0x28 / 4] - luSrc;

        // FixDown the source in place by its own base address.
        {
            u32* lpEntry = const_cast<u32*>(lpSrc);
            for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
            {
                lpEntry[0] -= luSrc;
                lpEntry[1] -= luSrc;
                lpEntry += KU_PALETTE_STRIDE_DWORD;
            }
        }

        std::memcpy(lpDst, lpResource, luSize);

        // FixUp the destination by the destination base address.
        {
            u32* lpEntry = reinterpret_cast<u32*>(lpDst);
            for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
            {
                lpEntry[0] += luDst;
                lpEntry[1] += luDst;
                lpEntry += KU_PALETTE_STRIDE_DWORD;
            }
        }

        // FixUp the source back to its loaded form.
        {
            u32* lpEntry = const_cast<u32*>(lpSrc);
            for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
            {
                lpEntry[0] += luSrc;
                lpEntry[1] += luSrc;
                lpEntry += KU_PALETTE_STRIDE_DWORD;
            }
        }

        return lpDst;
    }
}
