#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::SamplerState::GetResourceDescriptor @ 0x82B63588
//   renderengine::SamplerState::Initialize            @ 0x82B62630
//
// GetResourceDescriptor builds the standard five-entry rw resource descriptor: the
// loop fills five {size=0, align=1} pairs, then the first pair is overwritten by a
// 64-bit store with {size=0x20, align=4}. Initialize unpacks a sampler parameters
// block into the 32-byte runtime state. Field names follow the ARTIST
// Initialize -> sub_827E8950 -> D3DDevice_SetSamplerState consumer chain; the
// X360 byte stores and setter arguments arbitrate over the DecFIGS labels.

namespace renderengine
{
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    struct SamplerStateData
    {
        f32 mfMipMapLodBias;
        f32 mfAnisotropyBias;
        u8  mu8AddressU;
        u8  mu8AddressV;
        u8  mu8AddressW;
        u8  mbState11Zero;
        u8  mu8MipFilter;
        u8  mu8DepthTextureFunction;
        u8  mu8Gamma;
        u8  mbState15;
        u8  mbState16;
        u8  mu8State17;
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
    };

    struct SamplerStateParameters
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
        f32 mfMipMapLodBias;
        f32 mfAnisotropyBias;
        u32 muDepthTextureFunction;
        u32 muGamma;
        u32 muState17;
        u8  mbState15;
        u8  mbSeparateZFilterEnable;
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
        lpState->mu8MipFilter = static_cast<u8>(lpParams->muMipFilter);
        lpState->mu8DepthTextureFunction = static_cast<u8>(lpParams->muDepthTextureFunction);
        lpState->mu8Gamma = static_cast<u8>(lpParams->muGamma);
        lpState->mbState15 = static_cast<u8>(lpParams->mbState15 != 0);
        lpState->mbState16 = static_cast<u8>(lpParams->mbState16 != 0);
        lpState->mu8State17 = static_cast<u8>(lpParams->muState17);
        lpState->mu8MinFilter = static_cast<u8>(lpParams->muMinFilter);
        lpState->mu8MinFilterZ = static_cast<u8>(lpParams->muMinFilterZ);
        lpState->mu8MagFilter = static_cast<u8>(lpParams->muMagFilter);
        lpState->mu8MagFilterZ = static_cast<u8>(lpParams->muMagFilterZ);
        lpState->mu8MinMipLevel = static_cast<u8>(lpParams->muMinMipLevel);
        lpState->mu8MaxMipLevel = static_cast<u8>(lpParams->muMaxMipLevel);
        lpState->mu8MaxAnisotropy = static_cast<u8>(lpParams->muMaxAnisotropy);
        lpState->mfMipMapLodBias = lpParams->mfMipMapLodBias;
        lpState->mfAnisotropyBias = lpParams->mfAnisotropyBias;
        lpState->mbSeparateZFilterEnable = static_cast<u8>(lpParams->mbSeparateZFilterEnable != 0);
        lpState->muInitialized = 1;
        lpState->mbState26 = static_cast<u8>(lpParams->mbState26 != 0);
        return lpState;
    }
}
