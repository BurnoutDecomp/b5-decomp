#pragma once

#include "types.hpp"

// Shared declarations for the existing ARTIST sampler producer and its callers.
// Legacy member names are retained; the original byte stores define the layout.
namespace renderengine
{
    struct ResourceDescriptorEntry;

    struct SamplerStateData
    {
        u32 muColor;
        u32 muRemap;
        u8  mu8AddressU;
        u8  mu8AddressV;
        u8  mu8AddressW;
        u8  mbState11Zero;
        u8  mu8MinLevel;
        u8  mu8DepthTextureFunction;
        u8  mu8Gamma;
        u8  mbState15;
        u8  mbState16;
        u8  mu8State17;
        u8  mu8MaxLevel;
        u8  mu8MagFilter;
        u8  mu8Bias;
        u8  mu8MaxAnisotropy;
        u8  mbState22;
        u8  mu8Convolution;
        u8  mu8MipFilter;
        u8  mu8MinFilter;
        u8  mbState26;
        u8  mu8Pad27;
        u32 muInitialized;
    };

    struct SamplerStateParameters
    {
        u32 muAddressU;
        u32 muAddressV;
        u32 muAddressW;
        u32 muBias;
        u32 muMaxLevel;
        u32 muMinLevel;
        u32 muMaxAnisotropy;
        u32 muMagFilter;
        u32 muMinFilter;
        u32 muMipFilter;
        u32 muConvolution;
        u32 muColor;
        u32 muRemap;
        u32 muDepthTextureFunction;
        u32 muGamma;
        u32 muState17;
        u8  mbState15;
        u8  mbState22;
        u8  mbState16;
        u8  mbState11Zero;
        u8  mbState26;
    };

    class SamplerState
    {
    public:
        static ResourceDescriptorEntry* GetResourceDescriptor(ResourceDescriptorEntry* lpDescriptor);
        static SamplerStateData* Initialize(SamplerStateData** lppState, const SamplerStateParameters* lpParams);
    };

    static_assert(sizeof(SamplerStateData) == 0x20, "Original sampler runtime block");
    static_assert(sizeof(SamplerStateParameters) == 0x48, "Original parameter prefix");
}
