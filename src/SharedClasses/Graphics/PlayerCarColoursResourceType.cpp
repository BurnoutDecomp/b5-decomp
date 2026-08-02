#include "SharedClasses/Graphics/PlayerCarColoursResourceType.h"
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"   // BrnWorld::GlobalColourPalette (single home)
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase
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
// PlayerCarColourPalette entries (single home: BrnGlobalColourPalette.h), each 12 bytes:
//   +0 u32 muPaintColours   (serialised 32-bit colour-array column)
//   +4 u32 muPearlColours   (serialised 32-bit colour-array column)
//   +8 s32 miNumColours
// FixUp/FixDown walk the four entries (stride 12 == 3 dwords) and rebase the two
// colour-array columns (+0, +4) by the load-base delta read from the rw::Resource
// arg (rw::Resource::m_baseResources[0]). The columns are adjusted unconditionally
// (no null guard) to match the X360 exactly.
//
// [x64] Every address here is a 32-BIT SERIALISED SLOT, not a host pointer -- see the
// two-proof banner in BrnGlobalColourPalette.h. The delta is taken through the shared
// CgsResource::GetLoadBase() (the low 32 bits of m_baseResources[0]), which is what the
// VehicleList/WheelList handlers already use; the GameData roots are carved below 4 GB
// (BrnGame.log line 3). This TU previously spelled those truncations as
// `reinterpret_cast<u32>(pointer)`, which does not compile for x64 -- that, and nothing
// else, is why the type was never registered and why the palette never loaded.

namespace CgsResource
{
    static const u32 KU_NUM_PALETTES         = 4;   // BrnWorld::E_NUM_PALETTES
    static const u32 KU_PALETTE_STRIDE_DWORD = 3;   // sizeof(PlayerCarColourPalette)/4

    // The registry id of a serialised PlayerCarColours payload.
    //
    // FLAG: the X360's own GetTypeID body is NOT in the .ida-exports set for this class
    // (the export set has GetSerialisedResourceDescriptor/Serialise/FixUp/FixDown only) --
    // every handler's GetTypeID is a two-instruction `li r3, imm ; blr` and the linker
    // ICF-folds them, so there is no distinct body to recover. The value is taken from
    // GROUND TRUTH instead: the shipped build/game/Vehicles/VehicleList.bundle carries this
    // resource with type id 0x1001E, and 0x1001E is the one id in that bundle's
    // {0x10005, 0x1001E} set that had no registered handler ("[bundle] UNREGISTERED
    // resource type id 65566 in 'Vehicles/VehicleList.bundle'").
    static const u32 KU_PLAYER_CAR_COLOURS_RESOURCE_TYPE_ID = 65566;   // 0x1001E

    uint32_t PlayerCarColoursResourceType::GetTypeID() const
    {
        return KU_PLAYER_CAR_COLOURS_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267C568 (store-for-store). Builds the five-entry
    // serialised descriptor:
    //   entry0 = { size = 16 * (*(res+0x2C)) + (*(res+0x28)) - res, align = 16 }
    //   entry1..4 = { size = 0, align = 1 }
    // The size is the byte span from the resource base to the end of the colour payload: the
    // LAST palette entry's pearl-colour column (+0x28) and colour count (+0x2C) give
    // end = pearl + 16*count, and (end - res) is the total serialised size. The X360 writes
    // all five alignments (1) and four trailing sizes (0) first, then one 64-bit store of
    // {size, align=16} overwrites entry0 -- so the rest are {0,1}.
    //
    // MEASURED against the shipped payload: word[0x28] == 0x9B0, word[0x2C] == 2, so
    // 16*2 + 0x9B0 == 2512 == the bundle entry's recorded size, to the byte. That is the
    // same arithmetic reproduced below, and it is what pins +0x28/+0x2C to palette[3] --
    // i.e. what pins the 12-byte stride from this function alone.
    ResourceDescriptor PlayerCarColoursResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const u32* lpSrc  = reinterpret_cast<const u32*>(lpResource);
        u32        luRes  = static_cast<u32>(reinterpret_cast<uintptr_t>(lpResource));
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

    // FixDown: subtract the delta from both columns of each palette entry. @ 0x8267E090
    void PlayerCarColoursResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = GetLoadBase(lrResource);
        u32* lpEntry = reinterpret_cast<u32*>(lpResource);
        for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
        {
            lpEntry[0] -= luDelta;   // muPaintColours
            lpEntry[1] -= luDelta;   // muPearlColours
            lpEntry += KU_PALETTE_STRIDE_DWORD;
        }
    }

    // FixUp: add the delta to both columns of each palette entry. @ 0x8267E058
    void PlayerCarColoursResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        const u32 luDelta = GetLoadBase(lrResource);
        u32* lpEntry = reinterpret_cast<u32*>(lpResource);
        for (u32 lu = 0; lu < KU_NUM_PALETTES; ++lu)
        {
            lpEntry[0] += luDelta;   // muPaintColours
            lpEntry[1] += luDelta;   // muPearlColours
            lpEntry += KU_PALETTE_STRIDE_DWORD;
        }
    }

    // Serialise: copy the source palette into the destination resource buffer, then
    // rebase the two colour columns of every entry. The X360 (@ 0x8267DF80):
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
        const u32  luSrc  = static_cast<u32>(reinterpret_cast<uintptr_t>(lpResource));
        void*      lpDst  = lrDest.m_baseResources[0];
        const u32  luDst  = static_cast<u32>(reinterpret_cast<uintptr_t>(lpDst));

        // Copy size from the LAST palette entry's pearl column + count (+0x28, +0x2C).
        const u32 luSize = 16u * lpSrc[0x2C / 4] + lpSrc[0x28 / 4] - luSrc;

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
