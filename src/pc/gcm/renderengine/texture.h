#pragma once

#include "types.hpp"

// renderengine::Texture / Texture2D - the platform render-engine texture objects. The
// PC backend wraps a D3D9 texture; only the surface the in-scope renderers use is
// declared: the 2D-texture create path (GetResourceDescriptor + Initialize) and the
// lock/unlock upload path. Matches the renderengine resource-descriptor convention
// used by renderstates.h.
struct IDirect3DBaseTexture9;

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

        static void Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, LockInfo* lpLockInfoOut);
        static void Unlock(Texture* lpTexture, LockInfo* lpLockInfo);

        // The underlying D3D9 texture (PC backend); SetTexture binds it directly.
        IDirect3DBaseTexture9* mpD3DTexture;
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
