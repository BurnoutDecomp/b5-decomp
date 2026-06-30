#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/Xbox2SurfaceShims.h"

#include <cstring>   // memset
#include <cstddef>   // offsetof

// X360 reconstruction of renderengine::PixelBuffer -- the render-target / depth-stencil SURFACE
// object. These are X360 EDRAM tile / surface-header address & format operations (NOT VMX vector
// pipelines): Initialize sub-allocates EDRAM tiles (and, for a depth surface, a hierarchical-Z
// region) from the two running EDRAM cursors and lays the GPU surface header out via
// XGSetSurfaceHeader; Xbox2ResolveTo resolves the tiled EDRAM surface out to a linear texture;
// Xbox2SetBaseEDRAM / Xbox2SetBaseHierarchicalZ patch the EDRAM base words.
//
// The bodies mirror the X360 image store-for-store: header dwords are written by name in the
// order the asm writes them, the XG / D3D helper argument order is preserved, and the bit masks
// match the PPC rlwinm/insrwi/clrlwi operations exactly.
//
//   Initialize                  0x82B621A8
//   Xbox2ResolveTo              0x82B62300
//   Xbox2SetBaseEDRAM           0x82B60870
//   Xbox2SetBaseHierarchicalZ   0x82B60898

namespace renderengine
{
    // Pin the recovered surface-header byte layout the asm writes by offset (pointer-invariant
    // facts; uncalled). These offsets are where Initialize / the Xbox2Set* / resolve paths store.
    static void _AssertLayout()
    {
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muUnused08)  == 0x08, "v2[2]");
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muFence)     == 0x14, "v2[5]");
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muEDRAMBase) == 0x1C, "+0x1C EDRAM base");
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muHierZ)     == 0x20, "+0x20 hi-Z base");
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muTileWord)  == 0x24, "+0x24 tile word");
        static_assert(offsetof(PixelBuffer::SurfaceHeader, muKind)      == 0x30, "+0x30 kind");
        static_assert(sizeof(PixelBuffer::SurfaceHeader) == 0x34, "surface header memset size");
    }

    // 0x82B621A8 -- lay the GPU surface header out in place and place it in EDRAM.
    PixelBuffer::SurfaceHeader* PixelBuffer::Initialize(Wrapper* lpWrapper, const Parameters* lpParams)
    {
        SurfaceHeader* lpSurface = lpWrapper->mpSurface;   // v2 = *a1
        std::memset(lpSurface, 0, 0x34);                    // memset(*a1, 0, 52)

        // The three-dword parameter block XGSetSurfaceHeader is seeded with: EDRAM tile base,
        // hierarchical-Z base, and mip base.
        u32 luEDRAMBase = 0u;        // v18 = 0
        u32 luHierZBase = 0u;        // v19 = 0
        u32 luHierZSize = 0u;        // v17 (pHierarchicalZSize) BYREF

        if ((lpParams->muFlags & 1u) == 0u)   // (a2[1] & 1) == 0  -> EDRAM-placed surface
        {
            const u32 luTileFootprint = XGSurfaceSize(static_cast<int>(lpParams->muWidth),
                                                      static_cast<int>(lpParams->muHeight),
                                                      static_cast<int>(lpParams->muFormat),
                                                      static_cast<int>(lpParams->muMultiSample));  // v5
            u32 luTileBase = gXbox2EDRAMTileCursor;                      // v6 = cursor
            if ((gXbox2EDRAMTileCursor + luTileFootprint) > 0x800u)     // wrap when it overflows EDRAM
            {
                luTileBase = 0u;                                        // v6 = 0
            }
            luEDRAMBase = luTileBase;                                   // v18 = v6
            gXbox2EDRAMTileCursor = luTileBase + luTileFootprint;       // dword_832716C0 = v6 + v5

            if (lpParams->muKind == 1u)   // *a2 == 1  -> depth-stencil: reserve hierarchical-Z too
            {
                const u32 luMultiSample = lpParams->muMultiSample;   // v7 = a2[5]
                u32 luWidth  = lpParams->muWidth;                    // v8 = a2[2]
                u32 luHeight = lpParams->muHeight;                   // v9 = a2[3]
                if (luMultiSample >= 1u)                             // v7 >= 1 -> double height
                {
                    luHeight *= 2u;
                }
                if (luMultiSample == 2u)                             // v7 == 2 -> double width
                {
                    luWidth *= 2u;
                }
                // tiles = (ceil(W/32) * ceil(H/16)), masked to 23 bits.
                const u32 luHierZTiles = (((luWidth + 31u) >> 5) * ((luHeight + 15u) >> 4)) & 0x7FFFFFu;  // v10
                luHierZSize = luHierZTiles;                          // v17 = v10

                u32 luHierZ;
                if (luHierZTiles > 0xE10u)                           // does not fit the hi-Z pool
                {
                    luHierZ = static_cast<u32>(-1);                 // v11 = -1
                }
                else
                {
                    luHierZ = gXbox2EDRAMHierZCursor;                           // v11 = cursor
                    if ((gXbox2EDRAMHierZCursor + luHierZTiles) > 0xE10u)       // wrap on overflow
                    {
                        luHierZ = 0u;                                          // v11 = 0
                    }
                    gXbox2EDRAMHierZCursor = luHierZ + luHierZTiles;           // dword_832716C4 = v11 + v10
                }
                luHierZBase = luHierZ;                               // v19 = v11
            }
        }

        u32 luParamBlock[3];
        luParamBlock[0] = luEDRAMBase;       // var_38
        luParamBlock[1] = luHierZBase;       // var_34
        luParamBlock[2] = lpParams->muMipBase;   // var_30 = a2[6]

        XGSetSurfaceHeader(static_cast<int>(lpParams->muWidth),
                           static_cast<int>(lpParams->muHeight),
                           static_cast<int>(lpParams->muFormat),
                           static_cast<int>(lpParams->muMultiSample),
                           reinterpret_cast<int*>(luParamBlock),
                           lpSurface,
                           &luHierZSize);

        lpSurface->muUnused08 = 0u;                 // v2[2] = 0
        lpSurface->muFence = 0xFFFF0000u;           // v2[5] = -65536
        if ((lpParams->muFlags & 1u) == 0u)         // (a2[1] & 1) == 0
        {
            lpSurface->muCommon |= 0x80000000u;     // *v2 |= 0x80000000
        }
        lpSurface->muKind = lpParams->muKind;       // v2[12] = *a2
        return lpSurface;
    }

    // 0x82B62300 -- resolve the tiled EDRAM surface out to a linear destination texture.
    int PixelBuffer::Xbox2ResolveTo(SurfaceHeader* lpThis, Texture* lpDestTexture, u32 luFlags,
                                    s32 liDestX, s32 liDestY, s32 liDestLevel, s32 liDestSlice,
                                    s32 liClearColour, f32 lfClearZ)
    {
        void* lpDevice = gpD3DDevice;            // v37 = off_83271608

        // Compare against the canonical depth / colour surface tags the engine keeps; whether this
        // surface IS the active render target gates the tiled-resolve fast path below.
        const void* lpActiveTag;                 // v42
        if (lpThis->muKind == 1u)                // *(a1 + 48) == 1 -> depth-stencil
        {
            D3DDevice_SetDepthStencilSurface(lpDevice, lpThis);
            luFlags |= 4u;                       // a3 |= 4
            lpActiveTag = gpDepthSurfaceTag;     // v42 = &unk_83271140
        }
        else
        {
            D3DDevice_SetRenderTarget(lpDevice, 0u, lpThis);
            lpActiveTag = gpColourSurfaceTag;    // v42 = &unk_83271100
        }

        if (lpActiveTag == lpThis && gXbox2TilingEnabled)   // v42 == a1 && dword_8327161C
        {
            // NOTE: cold (tiling-enabled, untraced) path; D3DDevice_EndTiling is an unhomed Xbox
            // D3D9 shim. The asm @0x82B623A0 packs r3=device,r4=flags,r5=0(rects),r6=destTexture,
            // r7=clearColor,r9=pParameters,r10=0,f1=clearZ; this call's middle args (level/slice/
            // clearColour) are an APPROXIMATE mapping pending the real EndTiling signature (the r8
            // slot is ambiguous in the asm). Declaration-only shim -> never linked on PC.
            D3DDevice_EndTiling(lpDevice, luFlags, nullptr, lpDestTexture,
                                liDestLevel, liDestSlice, liClearColour, 0, lfClearZ);
        }
        else
        {
            u32 luSourceRect[6];                 // v49[6]
            luSourceRect[0] = static_cast<u32>(liDestX);   // v49[0] = a4
            luSourceRect[1] = static_cast<u32>(liDestY);   // v49[1] = a5

            // Right edge = destX + min(this-tiled-width, destTexture-width).
            const u32 luTiledWidth = (lpThis->muTileWord >> 18) + 1u;   // r11 = v7 >> 18; r6 = +1
            u32 luClipW = luTiledWidth;                                  // v43
            if (Texture::GetWidth(lpDestTexture) < luTiledWidth)        // Width < v43
            {
                luClipW = Texture::GetWidth(lpDestTexture);
            }
            luSourceRect[2] = luClipW + static_cast<u32>(liDestX);      // v49[2] = v43 + a4

            // Bottom edge = destY + min(this-tiled-height, destTexture-height).
            const u32 luTiledHeight = ((lpThis->muTileWord >> 14) & 0x7FFFu) + 1u;  // extrwi r11,r7,15,14; +1
            u32 luClipH;                                                // v47
            if (Texture::GetHeight(lpDestTexture) >= luTiledHeight)     // Height >= v45
            {
                luClipH = luTiledHeight;
            }
            else
            {
                luClipH = Texture::GetHeight(lpDestTexture);
            }
            luSourceRect[3] = luClipH + static_cast<u32>(liDestY);      // v49[3] = v47 + a5

            D3DDevice_Resolve(lpDevice, luFlags, luSourceRect, lpDestTexture, nullptr,
                              liDestLevel, liDestSlice, liClearColour, lfClearZ);
        }
        return 1;
    }

    // 0x82B60870 -- patch the EDRAM tile base (low 12 bits of muEDRAMBase); colour/depth kinds only.
    PixelBuffer::SurfaceHeader* PixelBuffer::Xbox2SetBaseEDRAM(SurfaceHeader* lpThis, s16 li16Base)
    {
        const u32 luKind = lpThis->muKind;       // *(result + 48)
        if (luKind == 0u || luKind == 1u)        // <= 1u : early-out (bnelr) for higher kinds
        {
            // rlwimi r4, r11, 0,0,19 : keep the high 20 bits of muEDRAMBase, take the low 12 of base.
            lpThis->muEDRAMBase =
                (lpThis->muEDRAMBase & 0xFFFFF000u) | (static_cast<u32>(li16Base) & 0xFFFu);
        }
        return lpThis;
    }

    // 0x82B60898 -- patch the hierarchical-Z base (high 15 bits, <<17, of muHierZ).
    PixelBuffer::SurfaceHeader* PixelBuffer::Xbox2SetBaseHierarchicalZ(SurfaceHeader* lpThis, s32 liBase)
    {
        // insrwi r11, r4, 15,0 : replace the top 15 bits of muHierZ with liBase, keep the low 17.
        lpThis->muHierZ =
            (static_cast<u32>(liBase) << 17) | (lpThis->muHierZ & 0x1FFFFu);
        return lpThis;
    }
}
