// GameShared/GameClasses/RenderWare/PS3/CgsRwRasterResourceTypePS3.cpp
//
// NOTE: despite the PS3 path, this TU reconstructs the CONSOLE (X360/Xbox2) bodies of the
// three CgsResource::RwRasterResourceType virtuals from BURNOUT_X360_ARTIST.XEX:
//     GetSerialisedResourceDescriptor  @ 0x828A9858
//     FixDown                          @ 0x828A88E0
//     FixUp                            @ 0x828A8930
//
// These SAME three virtuals already have a committed home in the cross TU
//   GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.cpp
// where FixDown/FixUp are the PC (D3D9) divergence (Destroy/Create) and
// GetSerialisedResourceDescriptor is the descriptor port. To avoid a duplicate-definition
// (ODR / duplicate-symbol) collision with that committed PC home, the console bodies below
// are compiled ONLY under CGS_PLATFORM_X360. In the PC build (the compile gate: /DWIN32,
// no CGS_PLATFORM_X360) this TU is empty and the committed cross bodies remain
// authoritative. (Mirrors CgsHardwareInitPS3.cpp, which likewise carries the X360 surface
// on a PS3 path behind CGS_PLATFORM_X360.)

#if defined(CGS_PLATFORM_X360)

#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "rw/rwcore_structs.h"                // rw::Resource (m_baseResources[]), rw::ResourceDescriptor
#include "pc/gcm/renderengine/texture.h"      // renderengine::Texture (runtime resource + descriptor path)

namespace CgsResource
{
    // ---- FixDown @ 0x828A88E0 --------------------------------------------------------------
    //
    // Console FixDown. The serialised raster IS GPU memory; its packed pixel-base words at
    // raster +0x20 and +0x30 carry a graphics-local offset in their high 20 bits and
    // memory/format flags in their low 12 bits. FixDown un-rebases those offsets by subtracting
    // the resource's graphics-local base delta (rw::Resource m_baseResources[2], asm 8(r5),
    // read as a 32-bit byte delta), preserving the low-12 flag bits, then resets three header
    // words. The +0x30 rebase is skipped when its packed offset (high 20 bits) is zero.
    //
    // Store-for-store (0x828A88E0):
    //   r11=+0x20; r9=delta(8(r5)); r7=(r11&~0xFFF)-delta; insrwi -> keep r11 low12; stw +0x20
    //   r8=(+0x30)&~0xFFF; if r8!=0: r11=r8-delta; insrwi low12 of +0x30; stw +0x30
    //   stw -65536,+0x14; stw 0,+0x08; stw -65536,+0x18
    void RwRasterResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* const lpRaster = reinterpret_cast<u32*>(lpResource);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));  // 8(r5)

        // +0x20: high-20 -= delta, low-12 flag bits preserved (unconditional).
        {
            const u32 luWord = lpRaster[0x20 / 4];
            lpRaster[0x20 / 4] = (luWord & 0x00000FFFu)
                               | (((luWord & 0xFFFFF000u) - luDelta) & 0xFFFFF000u);
        }

        // +0x30: same rebase, only when the packed offset (high-20) is non-zero.
        {
            const u32 luWord = lpRaster[0x30 / 4];
            if ((luWord & 0xFFFFF000u) != 0)
            {
                lpRaster[0x30 / 4] = (luWord & 0x00000FFFu)
                                   | (((luWord & 0xFFFFF000u) - luDelta) & 0xFFFFF000u);
            }
        }

        // Reset the serialised header words the fix-down invalidates.
        lpRaster[0x14 / 4] = 0xFFFF0000u;  // stw lis -1 -> 0xFFFF0000 (-65536)
        lpRaster[0x08 / 4] = 0u;
        lpRaster[0x18 / 4] = 0xFFFF0000u;  // -65536
    }

    // ---- FixUp @ 0x828A8930 ----------------------------------------------------------------
    //
    // Console FixUp -- the exact inverse of FixDown, minus the header reset. Rebase the packed
    // pixel-base words (+0x20, +0x30) INTO the graphics allocation by ADDING the same delta to
    // their high-20 bits, preserving the low-12 flag bits. +0x30 skipped when its high-20 is 0
    // (asm returns via beqlr). No header reset.
    void RwRasterResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        u32* const lpRaster = reinterpret_cast<u32*>(lpResource);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));  // 8(r5)

        // +0x20: high-20 += delta, low-12 preserved (unconditional).
        {
            const u32 luWord = lpRaster[0x20 / 4];
            lpRaster[0x20 / 4] = (luWord & 0x00000FFFu)
                               | (((luWord & 0xFFFFF000u) + luDelta) & 0xFFFFF000u);
        }

        // +0x30: same rebase, only when its high-20 is non-zero.
        {
            const u32 luWord = lpRaster[0x30 / 4];
            if ((luWord & 0xFFFFF000u) != 0)
            {
                lpRaster[0x30 / 4] = (luWord & 0x00000FFFu)
                                   | (((luWord & 0xFFFFF000u) + luDelta) & 0xFFFFF000u);
            }
        }
    }

    // ---- GetSerialisedResourceDescriptor @ 0x828A9858 --------------------------------------
    //
    //   renderengine::Texture::Parameters params;  // v5[8], v5[0] = the DIMENSION word = -1, rest 0
    //   renderengine::Texture::GetParameters(resource, &params);
    //   renderengine::Texture::GetResourceDescriptor(out, &params);
    // Descriptor written into the five-lane rw::ResourceDescriptor.
    ResourceDescriptor RwRasterResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        ResourceDescriptor lDescriptor;
        renderengine::Texture::Parameters lParameters;
        // ⚠ v5[0] IS THE DIMENSION WORD, NOT THE FORMAT (corrected 2026-08-16 with the Parameters
        // layout): renderengine::Texture::GetParameters @0x82B60D80 writes -1 into word 0 for a
        // resource type it does not recognise (`li r11, -1` @0x82B60DD8) and puts the format in
        // word 6. The seed is otherwise unchanged -- the console zeroes every other word, and
        // GetParameters overwrites all eight anyway.
        lParameters.meType      = renderengine::Texture::E_TYPE_UNKNOWN;   // v5[0] = -1
        lParameters.muSysMem    = 0;
        lParameters.muWidth     = 0;
        lParameters.muHeight    = 0;
        lParameters.muDepth     = 0;
        lParameters.muNumLevels = 0;
        lParameters.miFormat    = 0;    // v5[6]
        lParameters.muReserved1 = 0;
        renderengine::Texture::GetParameters(static_cast<const renderengine::Texture*>(lpResource), &lParameters);
        renderengine::Texture::GetResourceDescriptor(reinterpret_cast<u32*>(&lDescriptor), &lParameters);
        return lDescriptor;
    }
}

#endif // CGS_PLATFORM_X360
