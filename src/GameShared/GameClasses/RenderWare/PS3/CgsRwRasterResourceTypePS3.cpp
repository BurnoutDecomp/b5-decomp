// Native ARTIST raster bodies; the PS3 filename is DecFIGS attribution.
#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "rw/rwcore_structs.h"                // rw::Resource (m_baseResources[]), rw::ResourceDescriptor
CGS_STATIC_ASSERT((offsetof(D3DBaseTexture, Format) == 0x1C), "ARTIST packed raster fetch constant");

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
        renderengine::Texture* const lpRaster = static_cast<renderengine::Texture*>(lpResource);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));  // 8(r5)

        // +0x20: high-20 -= delta, low-12 flag bits preserved (unconditional).
        {
            const u32 luWord = lpRaster->Format.dword[1];
            lpRaster->Format.dword[1] = (luWord & 0x00000FFFu)
                               | (((luWord & 0xFFFFF000u) - luDelta) & 0xFFFFF000u);
        }

        // +0x30: same rebase, only when the packed offset (high-20) is non-zero.
        {
            const u32 luWord = lpRaster->Format.dword[5];
            if ((luWord & 0xFFFFF000u) != 0)
            {
                lpRaster->Format.dword[5] = (luWord & 0x00000FFFu)
                                   | (((luWord & 0xFFFFF000u) - luDelta) & 0xFFFFF000u);
            }
        }

        // Reset the serialised header words the fix-down invalidates.
        lpRaster->BaseFlush = 0xFFFF0000u;  // stw lis -1 -> 0xFFFF0000 (-65536)
        lpRaster->Fence = 0u;
        lpRaster->MipFlush = 0xFFFF0000u;  // -65536
    }

    // ---- FixUp @ 0x828A8930 ----------------------------------------------------------------
    //
    // Console FixUp -- the exact inverse of FixDown, minus the header reset. Rebase the packed
    // pixel-base words (+0x20, +0x30) INTO the graphics allocation by ADDING the same delta to
    // their high-20 bits, preserving the low-12 flag bits. +0x30 skipped when its high-20 is 0
    // (asm returns via beqlr). No header reset.
    void RwRasterResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        renderengine::Texture* const lpRaster = static_cast<renderengine::Texture*>(lpResource);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));  // 8(r5)

        // +0x20: high-20 += delta, low-12 preserved (unconditional).
        {
            const u32 luWord = lpRaster->Format.dword[1];
            lpRaster->Format.dword[1] = (luWord & 0x00000FFFu)
                               | (((luWord & 0xFFFFF000u) + luDelta) & 0xFFFFF000u);
        }

        // +0x30: same rebase, only when its high-20 is non-zero.
        {
            const u32 luWord = lpRaster->Format.dword[5];
            if ((luWord & 0xFFFFF000u) != 0)
            {
                lpRaster->Format.dword[5] = (luWord & 0x00000FFFu)
                                   | (((luWord & 0xFFFFF000u) + luDelta) & 0xFFFFF000u);
            }
        }
    }

    // ARTIST 828A9858 initializes seven parameter words and returns the descriptor.
    ResourceDescriptor RwRasterResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        renderengine::Texture::Parameters lParameters =
            { renderengine::Texture::TYPE_NA, 0, 0, 0, 0, 0, static_cast<renderengine::PixelFormat>(0) };
        static_cast<const renderengine::Texture*>(lpResource)->GetParameters(lParameters);
        return renderengine::Texture::GetResourceDescriptor(lParameters);
    }
}
