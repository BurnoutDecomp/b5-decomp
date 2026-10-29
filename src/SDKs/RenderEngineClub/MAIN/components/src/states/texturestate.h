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
            // ARTIST Renderer constructor827E008C..827E0284 inlines these
            // defaults before constructing its Job arrays. Natural padding at
            //45..47 is deliberately untouched.
            Parameters()
                : muAddressU(0), muAddressV(0), muAddressW(0),
                  muMagFilter(0), muMinFilter(0), muMipFilter(2),
                  muMagFilterZ(0), muMinFilterZ(0), muMinMipLevel(13),
                  muMaxMipLevel(0), muMaxAnisotropy(1), mfMipLodBias(0.0f),
                  mfAnisotropyBias(0.0f), miHGradientExpBias(0),
                  miVGradientExpBias(0), muTrilinearThreshold(0),
                  mbWhiteBorder(0), mbSeparateZFilterEnable(0), mbWhiteBorderW(0),
                  mbPointBorderEnable(1), mbUnidentified44(1), mpTexture(0)
            {
            }

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
