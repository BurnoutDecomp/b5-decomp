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
    };
}
