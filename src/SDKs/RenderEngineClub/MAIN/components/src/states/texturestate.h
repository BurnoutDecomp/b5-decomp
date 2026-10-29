#pragma once

#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.h"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    class Texture;

    class TextureState
    {
    public:
        enum { KU_INVALID_TEXTURE = 0xFFFFFFFFu };

        // ARTIST 82B62630 reads the sampler prefix; 82B62730 reads mpTexture.
        // Field meanings follow the consumers in 827E8950 and the native XDK.
        struct Parameters
        {
            u32 muAddressU;
            u32 muAddressV;
            u32 muAddressW;
            u32 muMagFilter;
            u32 muMinFilter;
            u32 muMipFilter;
            u32 muMagFilterZ;
            u32 muMinFilterZ;
            u32 muMinMipLevel;
            u32 muMaxMipLevel;
            u32 muMaxAnisotropy;
            f32 mfMipLodBias;
            f32 mfAnisotropyBias;
            s32 miHGradientExpBias;
            s32 miVGradientExpBias;
            u32 muTrilinearThreshold;
            u8 mbWhiteBorder;
            u8 mbSeparateZFilterEnable;
            u8 mbWhiteBorderW;
            u8 mbPointBorderEnable;
            u8 mbUnidentified44;
            Texture* mpTexture;
        };

        static rw::ResourceDescriptor* GetResourceDescriptor(rw::ResourceDescriptor* lpDescriptor);
        static TextureState* Initialize(const rw::Resource* lpResource, const Parameters* lpParams);

        SamplerStateData mSampler;
        Texture* mpRaster;
    };

    CGS_STATIC_ASSERT((sizeof(TextureState) == 0x24), "ARTIST texture state size");
    CGS_STATIC_ASSERT((offsetof(TextureState, mpRaster) == 0x20), "ARTIST raster pointer");
    CGS_STATIC_ASSERT((sizeof(TextureState::Parameters) == 0x4C), "ARTIST texture state parameters");
    CGS_STATIC_ASSERT((offsetof(TextureState::Parameters, mfMipLodBias) == 0x2C), "ARTIST sampler float bias");
    CGS_STATIC_ASSERT((offsetof(TextureState::Parameters, mbWhiteBorder) == 0x40), "ARTIST sampler flags");
    CGS_STATIC_ASSERT((offsetof(TextureState::Parameters, mbUnidentified44) == 0x44), "ARTIST final sampler flag");
    CGS_STATIC_ASSERT((offsetof(TextureState::Parameters, mpTexture) == 0x48), "ARTIST parameter raster");
}
