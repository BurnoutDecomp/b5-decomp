#pragma once

#include "types.hpp"
#include <xtl.h>
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelformat.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/resourcedescriptor.h"

namespace rw { struct Resource; }

namespace renderengine
{
// ARTIST uses the SDK resource header directly (52 bytes), including its native
// Xenos fetch constant. There is no host-side image or renderer cache attached.
class Texture : public D3DBaseTexture
{
public:
    enum Type { TYPE_NA = -1, TYPE_1D = 0, TYPE_2D = 1, TYPE_ARRAY = 2,
                TYPE_CUBE = 3, TYPE_3D = 4 };
    enum { FLAG_DONTALLOCATE = 1, LOCK_READ = 1, LOCK_WRITE = 2, LOCK_READWRITE = 3 };

    struct Parameters
    {
        Type meType;
        u32 muFlags;
        u32 muWidth;
        u32 muHeight;
        u32 muDepth;
        u32 muNumLevels;
        PixelFormat meFormat;
    };

    // ARTIST 82B62B20 writes these fields; DecFIGS supplies their meanings.
    struct Locked
    {
        Texture* mpTexture;
        void* mpPixelData;
        u32 muStride;
        u16 muWidth;
        u16 muHeight;
        u16 muVolumeDepth;
        u8 muMipLevel;
        u8 muIndex;
        u32 muSliceStride;
        u32 muLockFlags;
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5*, const Parameters*);
    static Texture* Initialize(const rw::Resource*, const Parameters*);
    static void GetParameters(const Texture*, Parameters*);
    static Type GetType(const Texture*);
    static u32 GetWidth(const Texture*);
    static u32 GetHeight(const Texture*);
    static u32 GetDepth(const Texture*);
    static PixelFormat GetFormat(const Texture*);
    static u32 GetPixelDataSize(Type, u32 width, u32 height, u32 depth, u32 levels, PixelFormat);
    static bool Lock(Texture*, u32 flags, u32 mipLevel, u32 index, Locked*);
    static void Unlock(Texture*, const Locked*);
    static void Destruct(Texture*);
    static void Release(Texture*);
    static void Xbox2CheckPhysicalMemoryFlags(Texture*);
};

CGS_STATIC_ASSERT((sizeof(Texture) == 0x34), "ARTIST native texture header");
CGS_STATIC_ASSERT((sizeof(Texture::Parameters) == 0x1C), "ARTIST texture parameters");
CGS_STATIC_ASSERT((sizeof(Texture::Locked) == 0x1C), "ARTIST locked texture");
CGS_STATIC_ASSERT((offsetof(Texture::Locked, mpPixelData) == 4), "ARTIST lock pixels");
CGS_STATIC_ASSERT((offsetof(Texture::Locked, muWidth) == 12), "ARTIST lock dimensions");
CGS_STATIC_ASSERT((offsetof(Texture::Locked, muMipLevel) == 18), "ARTIST lock mip");
CGS_STATIC_ASSERT((offsetof(Texture::Locked, muSliceStride) == 20), "ARTIST lock slice");
CGS_STATIC_ASSERT((offsetof(Texture::Locked, muLockFlags) == 24), "ARTIST lock flags");
}
