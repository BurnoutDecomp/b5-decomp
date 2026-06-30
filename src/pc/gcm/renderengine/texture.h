#pragma once

#include "types.hpp"

// renderengine::Texture / Texture2D - the platform render-engine texture objects. The
// PC backend wraps a D3D9 texture; only the surface the in-scope renderers use is
// declared: the 2D-texture create path (GetResourceDescriptor + Initialize) and the
// lock/unlock upload path. Matches the renderengine resource-descriptor convention
// used by renderstates.h.
struct IDirect3DBaseTexture9;
namespace rw { struct Resource; }   // backing resource memory for the rw-resource create path

namespace renderengine
{
    class Texture
    {
    public:
        struct LockInfo
        {
            void* mpBits;
            u32   muPitch;
        };

        // renderengine::Texture::Locked - the descriptor a Lock fills: where the locked surface's
        // pixels live plus its geometry. Layout from the DecFIGS DWARF (EATech renderengine
        // texture.h:132); the X360 CgsNetworkImageConverter unpack path reads pixelData (+0x04),
        // stride (+0x08) and height (+0x0E) off it. Pointers widen to the x64 target.
        struct Locked
        {
            Texture* mpTexture;     // +0x00
            void*    mpPixelData;   // the locked surface's bits
            u32      muStride;      // row pitch in bytes
            u16      muWidth;
            u16      muHeight;
            u16      muVolumeDepth;
            u8       mu8MipLevel;
            u8       mu8Index;
            u32      muSliceStride;
            u32      muLockFlags;
        };

        static void Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, LockInfo* lpLockInfoOut);
        static void Unlock(Texture* lpTexture, LockInfo* lpLockInfo);

        // X360 Lock @0x82B62B20 overload: fill the full Locked descriptor (texture +
        // surface bits + geometry), not just the lean {bits,pitch} LockInfo. The
        // CgsNetworkImageConverter unpack path and BrnNetworkPlayerImageRenderer::Prepare
        // pass a Texture::Locked* here; ADDITIVE GROW over the LockInfo overload so the
        // existing lean callers (CgsMoviePlayer / BrnLoadingScreenRenderer) are unchanged.
        static void Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, Locked* lpLockedOut);

        // X360 Destruct @0x82B62D50: release a created texture's GPU/D3D resources in
        // place (the rw-resource sibling of Destroy; BrnNetworkPlayerImageRenderer::Release
        // destructs every buffered texture). ADDITIVE GROW.
        static void Destruct(Texture* lpTexture);

        // The serialised raster parameters: GetParameters fills them from a Texture, then
        // GetResourceDescriptor sizes the resource from them. Field set + order match the X360
        // renderengine::Texture Parameters (a _DWORD[8]); on X360 GetParameters reads them back
        // from the GPU texture descriptor (XGGetTextureDesc), on PC it reads the stored raster
        // header below (the PC D3D9 raster keeps format/size explicitly, having no GPU desc).
        struct Parameters
        {
            s32 miFormat;     // +0   D3DFORMAT (-1 = none)
            u32 muSysMem;     // +4   bit0 set => system-memory texture (no graphics-local pixels)
            u32 muWidth;      // +8
            u32 muHeight;     // +12
            u32 muDepth;      // +16
            u32 muNumLevels;  // +20
            u32 muReserved0;  // +24
            u32 muReserved1;  // +28
        };

        // Faithful to the X360 renderengine::Texture descriptor path:
        //   GetParameters          0x82B60D80
        //   GetResourceDescriptor  0x823FF848  (writes rw::BaseResourceDescriptors<5>: slot0 =
        //                          the Texture object, slot2 = the page-aligned pixel data)
        //   GetPixelDataSize       0x82B61088
        // lpDescriptorOut points at the five {u32 size, u32 align} entries (ten u32s); taken as
        // u32* so this header need not pull in the rw resource-descriptor template.
        static void GetParameters(const Texture* lpTexture, Parameters* lpParamsOut);
        static void GetResourceDescriptor(u32* lpDescriptorOut, const Parameters* lpParams);
        static u32  GetPixelDataSize(s32 liFormat, u32 luWidth, u32 luHeight, u32 luDepth, u32 luNumLevels);

        // Width/height accessors (X360 GetWidth @0x82B60EC8, GetHeight @0x82B60F38): read the stored
        // raster header (muWidth/muHeight). Static (take the texture by pointer) to match the X360
        // call form `GetWidth(texture)`. Bodied in the renderengine texture TU.
        static u32  GetWidth(const Texture* lpTexture);
        static u32  GetHeight(const Texture* lpTexture);

        // Realise the texture: create the managed D3D9 texture into mpD3DTexture from the
        // parameters and (if lpPixelData != null) upload the mip chain. This is the PC body of
        // CgsResource::RwRasterResourceType::FixUp (the console FixUp rebases packed GPU offsets;
        // the PC raster instead creates the D3D texture). Destroy is the matching FixDown body.
        static void Create(Texture* lpTexture, const Parameters* lpParams, const void* lpPixelData);
        static void Destroy(Texture* lpTexture);

        // 0x82403D-region create path: build the texture object into the rw resource memory from the
        // serialised parameters and return it (the rw-resource sibling of TextureState::Initialize;
        // the post-fx Tint lookup-texture create calls this). Body lives in its own renderengine TU.
        static Texture* Initialize(rw::Resource* lpResourceMemory, const Parameters* lpParams);

        // --- Burnout PC raster header (wiki page "Texture/Burnout Paradise/PC"; field order is
        // the wiki's, offsets are the x64 target's). mpD3DTexture is the wiki +0 "texture data"
        // pointer (the D3D texture on the PC backend); SetTexture / Lock bind it directly. ----
        IDirect3DBaseTexture9* mpD3DTexture;        // wiki +0x00  texture data (D3D texture)
        void*    mpTextureDataStruct;               // wiki +0x04  -> struct holding the data ptr
        void*    mpReserved8;                        // wiki +0x08
        bool     mbFlagC;                            // wiki +0x0C
        bool     mbFlagD;                            // wiki +0x0D
        bool     mbFlagE;                            // wiki +0x0E
        bool     mbFlagF;                            // wiki +0x0F
        s32      miFormat;                           // wiki +0x10  D3DFORMAT
        u16      muWidth;                            // wiki +0x14
        u16      muHeight;                           // wiki +0x16
        u8       muDepth;                            // wiki +0x18
        u8       muNumMipLevels;                     // wiki +0x19
        u16      muFlags;                            // wiki +0x1A
    };

    class Texture2D : public Texture
    {
    public:
        struct Parameters
        {
            u32 muWidth;
            u32 muHeight;
            u32 muDepth;
            u32 muNumLevels;
            u32 muFormat;
            u32 muUsage;
            u32 mauReserved[2];
        };

        struct ResourceDescriptor
        {
            u32 mauData[10];
        };

        static ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* lpDescriptorOut,
                                                         const Parameters* lpParams);
        static Texture2D* Initialize(const ResourceDescriptor* lpDescriptor, const Parameters* lpParams);

        // X360 Xbox2GetPhysicalMemorySize @0x82B610D8: lay out a GPU texture header for the
        // serialised descriptor and return its size (XGSetTextureHeader's HRESULT-sized result).
        // The X360 GetResourceDescriptor calls this to size slot2 of the resource descriptor when
        // the descriptor is NOT system-memory (bit0 of word0 clear). The input is the X360
        // descriptor dword block (word1=Width, word2=Height, word3=Levels, word4=Format); taken as
        // const u32* because the X360 marshalling reads it positionally and its field layout is the
        // X360 descriptor's, not the PC Texture2D::Parameters above. ADDITIVE GROW (X360 path only).
        static u32 Xbox2GetPhysicalMemorySize(const u32* lpDescriptorDwords);
    };

    // renderengine::PixelBuffer -- the platform render-target / depth-stencil SURFACE object. On
    // X360 a PixelBuffer wraps a GPU D3DSurface header (a tiled EDRAM surface) and carries the
    // EDRAM placement: Initialize lays the surface header out in place via XGSetSurfaceHeader and
    // sub-allocates EDRAM tiles + a hierarchical-Z region from the two running EDRAM allocators;
    // Xbox2SetBaseEDRAM / Xbox2SetBaseHierarchicalZ patch the EDRAM base words; Xbox2ResolveTo
    // resolves the tiled EDRAM surface out to a linear texture. These are X360 EDRAM/tile address
    // & format operations (NOT VMX pipelines). Reconstructed store-for-store from the X360 ASM:
    //   Initialize                  0x82B621A8
    //   Xbox2ResolveTo              0x82B62300
    //   Xbox2SetBaseEDRAM           0x82B60870
    //   Xbox2SetBaseHierarchicalZ   0x82B60898
    // PixelBuffer derives from Texture (it is a render-engine texture object); the surface header
    // it manages is a separate GPU-resident block reached through the wrapper.
    class PixelBuffer : public Texture
    {
    public:
        // The on-GPU surface header XGSetSurfaceHeader fills, 52 (0x34) bytes (memset(*a1,0,0x34)).
        // Named after the dwords the renderengine surface paths read/write; the GPU writes the rest.
        // muEDRAMBase (+0x1C) low 12 bits = EDRAM tile base; muHierZ (+0x20) high bits (<<17) =
        // hi-Z base; muTileWord (+0x24) packs the tiled width(<<18)/height(<<14 ,15 bits) the
        // resolve path reads back; muKind (+0x30) = 1 for a depth-stencil surface, else colour.
        struct SurfaceHeader
        {
            u32 muCommon;      // +0x00  D3DResource Common (bit31 0x80000000 set => EDRAM-resident)
            u32 muUnused04;    // +0x04
            u32 muUnused08;    // +0x08  (Initialize zeroes this)
            u32 muUnused0C;    // +0x0C
            u32 muUnused10;    // +0x10
            u32 muFence;       // +0x14  (Initialize seeds 0xFFFF0000)
            u32 muUnused18;    // +0x18
            u32 muEDRAMBase;   // +0x1C  low 12 bits = EDRAM tile base address
            u32 muHierZ;       // +0x20  high 15 bits (<<17) = hierarchical-Z base
            u32 muTileWord;    // +0x24  packed tiled width (>>18) / height (>>14, 15 bits)
            u32 muUnused28;    // +0x28
            u32 muUnused2C;    // +0x2C
            u32 muKind;        // +0x30  surface kind (1 = depth-stencil, else colour render target)
        };

        // The outer wrapper Initialize is handed: +0 -> the GPU surface header to fill.
        struct Wrapper
        {
            SurfaceHeader* mpSurface;   // +0x00
        };

        // The parameters block Initialize reads (a2[0..6]):
        //   word0 = kind (1 => depth-stencil; drives the EDRAM-tile reservation)
        //   word1 = flags (bit0 set => no EDRAM placement, just a header)
        //   word2 = width, word3 = height, word4 = format, word5 = multisample type, word6 = mip base
        struct Parameters
        {
            u32 muKind;          // +0x00
            u32 muFlags;         // +0x04
            u32 muWidth;         // +0x08
            u32 muHeight;        // +0x0C
            u32 muFormat;        // +0x10
            u32 muMultiSample;   // +0x14
            u32 muMipBase;       // +0x18
        };

        static SurfaceHeader* Initialize(Wrapper* lpWrapper, const Parameters* lpParams);

        // Resolve the tiled EDRAM surface (lpThis) out to a linear destination texture, clipping the
        // resolve rect to the destination texture's width/height. liFlags is the D3D resolve-flags
        // word; lfClearZ the clear-Z value; the trailing int args are the dest point / level / face
        // and clear-colour the X360 resolve takes. Returns 1 (the X360 body's constant return).
        static int Xbox2ResolveTo(SurfaceHeader* lpThis, Texture* lpDestTexture, u32 luFlags,
                                  s32 liDestX, s32 liDestY, s32 liDestLevel, s32 liDestSlice,
                                  s32 liClearColour, f32 lfClearZ);

        // Patch the EDRAM tile base (low 12 bits of muEDRAMBase) -- only for colour/depth kinds
        // (muKind <= 1); higher kinds are left untouched.
        static SurfaceHeader* Xbox2SetBaseEDRAM(SurfaceHeader* lpThis, s16 li16Base);

        // Patch the hierarchical-Z base (high 15 bits, <<17, of muHierZ).
        static SurfaceHeader* Xbox2SetBaseHierarchicalZ(SurfaceHeader* lpThis, s32 liBase);
    };
}
