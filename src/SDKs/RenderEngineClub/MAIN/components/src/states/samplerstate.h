#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

// Shared declarations for the existing ARTIST sampler producer and its callers.
// Legacy member names are retained; the original byte stores define the layout.
namespace renderengine
{
    struct ResourceDescriptorEntry;

    class SamplerState
    {
    public:
        f32 mfMipLodBias;
        f32 mfAnisotropyBias;
        u8  mu8AddressU;
        u8  mu8AddressV;
        u8  mu8AddressW;
        u8  mu8ClampPolicy;
        u8  mu8MipFilter;
        u8  mu8HGradientExpBias;
        u8  mu8VGradientExpBias;
        u8  mu8BorderColor;
        u8  mbForceBCWToMax;
        u8  mu8TrilinearClamp;
        u8  mu8MinFilter;
        u8  mu8MinFilterZ;
        u8  mu8MagFilter;
        u8  mu8MagFilterZ;
        u8  mbSeparateZFilterEnable;
        u8  mu8MaxAnisotropy;
        u8  mu8MaxMipLevel;
        u8  mu8MinMipLevel;
        u8  mbState26;
        u8  mu8Pad27;
        u32 muInitialized;


        struct Parameters
        {
            u32 muAddressU, muAddressV, muAddressW;
            u32 muMagFilter, muMinFilter, muMipFilter, muMagFilterZ, muMinFilterZ;
            u32 muMinMipLevel, muMaxMipLevel, muMaxAnisotropy;
            f32 mfMipLodBias, mfAnisotropyBias;
            s32 miHGradientExpBias, miVGradientExpBias;
            u32 muTrilinearThreshold;
            u8 mbWhiteBorder, mbSeparateZFilterEnable, mbWhiteBorderW;
            u8 mbPointBorderEnable, mbUnidentified44;
        };

        static rw::ResourceDescriptor GetResourceDescriptor(const Parameters& lrParameters);
        static SamplerState* Initialize(const rw::Resource& lrResource, const Parameters& lrParams);
    };

    CGS_STATIC_ASSERT((sizeof(SamplerState) == 0x20), "Original sampler runtime block");
    CGS_STATIC_ASSERT((sizeof(SamplerState::Parameters) == 0x48), "Original parameter prefix");
}
