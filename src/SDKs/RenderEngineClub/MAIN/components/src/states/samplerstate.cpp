#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::SamplerState::GetResourceDescriptor @ 0x82B63588
//   renderengine::SamplerState::Initialize            @ 0x82B62630
//
// GetResourceDescriptor builds the standard five-entry rw resource descriptor: the
// loop fills five {size=0, align=1} pairs, then the first pair is overwritten by a
// 64-bit store with {size=0x20, align=4}. Initialize unpacks a sampler parameters
// block into the 32-byte runtime state. Parameter names follow DecFIGS DWARF; the
// state layout follows the X360 byte stores.

namespace renderengine
{
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

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

    static_assert(sizeof(SamplerStateData) == 0x20, "SamplerStateData must match the X360 resource descriptor size");

    ResourceDescriptorEntry* SamplerState::GetResourceDescriptor(ResourceDescriptorEntry* lpDescriptor)
    {
        for (ResourceDescriptorEntry* lpEntry = lpDescriptor; lpEntry != lpDescriptor + 5; ++lpEntry)
        {
            lpEntry->muSize = 0;
            lpEntry->muAlignment = 1;
        }

        lpDescriptor[0].muSize = 0x20;
        lpDescriptor[0].muAlignment = 4;
        return lpDescriptor;
    }

    SamplerStateData* SamplerState::Initialize(SamplerStateData** lppState, const SamplerStateParameters* lpParams)
    {
        SamplerStateData* lpState = *lppState;

        lpState->mu8AddressU = static_cast<u8>(lpParams->muAddressU);
        lpState->mu8AddressV = static_cast<u8>(lpParams->muAddressV);
        lpState->mu8AddressW = static_cast<u8>(lpParams->muAddressW);
        lpState->mbState11Zero = static_cast<u8>(lpParams->mbState11Zero == 0);
        lpState->mu8MinLevel = static_cast<u8>(lpParams->muMinLevel);
        lpState->mu8DepthTextureFunction = static_cast<u8>(lpParams->muDepthTextureFunction);
        lpState->mu8Gamma = static_cast<u8>(lpParams->muGamma);
        lpState->mbState15 = static_cast<u8>(lpParams->mbState15 != 0);
        lpState->mbState16 = static_cast<u8>(lpParams->mbState16 != 0);
        lpState->mu8State17 = static_cast<u8>(lpParams->muState17);
        lpState->mu8Bias = static_cast<u8>(lpParams->muBias);
        lpState->mu8MaxLevel = static_cast<u8>(lpParams->muMaxLevel);
        lpState->mu8MaxAnisotropy = static_cast<u8>(lpParams->muMaxAnisotropy);
        lpState->mu8MagFilter = static_cast<u8>(lpParams->muMagFilter);
        lpState->mu8MinFilter = static_cast<u8>(lpParams->muMinFilter);
        lpState->mu8MipFilter = static_cast<u8>(lpParams->muMipFilter);
        lpState->mu8Convolution = static_cast<u8>(lpParams->muConvolution);
        lpState->muColor = lpParams->muColor;
        lpState->muRemap = lpParams->muRemap;
        lpState->mbState22 = static_cast<u8>(lpParams->mbState22 != 0);
        lpState->muInitialized = 1;
        lpState->mbState26 = static_cast<u8>(lpParams->mbState26 != 0);
        return lpState;
    }
}
