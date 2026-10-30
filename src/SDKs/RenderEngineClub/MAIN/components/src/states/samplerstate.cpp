#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.h"
#include <cstring>

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
    rw::ResourceDescriptor SamplerState::GetResourceDescriptor(const Parameters&)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 i = 0; i < 5; ++i)
        {
            lDescriptor.m_baseResourceDescriptors[i].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[i].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size = 0x20;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        return lDescriptor;
    }

    SamplerState* SamplerState::Initialize(const rw::Resource& lrResource, const Parameters& lrParams)
    {
        SamplerState* lpState = static_cast<SamplerState*>(lrResource.m_baseResources[0]);
        const Parameters* lpParams = &lrParams;

        lpState->mu8AddressU = static_cast<u8>(lpParams->muAddressU);
        lpState->mu8AddressV = static_cast<u8>(lpParams->muAddressV);
        lpState->mu8AddressW = static_cast<u8>(lpParams->muAddressW);
        lpState->mu8ClampPolicy = static_cast<u8>(lpParams->mbPointBorderEnable == 0);
        lpState->mu8MipFilter = static_cast<u8>(lpParams->muMipFilter);
        lpState->mu8HGradientExpBias = static_cast<u8>(lpParams->miHGradientExpBias);
        lpState->mu8VGradientExpBias = static_cast<u8>(lpParams->miVGradientExpBias);
        lpState->mu8BorderColor = static_cast<u8>(lpParams->mbWhiteBorder != 0);
        lpState->mbForceBCWToMax = static_cast<u8>(lpParams->mbWhiteBorderW != 0);
        lpState->mu8TrilinearClamp = static_cast<u8>(lpParams->muTrilinearThreshold);
        lpState->mu8MagFilter = static_cast<u8>(lpParams->muMagFilter);
        lpState->mu8MinFilter = static_cast<u8>(lpParams->muMinFilter);
        lpState->mu8MagFilterZ = static_cast<u8>(lpParams->muMagFilterZ);
        lpState->mu8MinFilterZ = static_cast<u8>(lpParams->muMinFilterZ);
        lpState->mu8MinMipLevel = static_cast<u8>(lpParams->muMinMipLevel);
        lpState->mu8MaxMipLevel = static_cast<u8>(lpParams->muMaxMipLevel);
        lpState->mu8MaxAnisotropy = static_cast<u8>(lpParams->muMaxAnisotropy);
        lpState->mfMipLodBias = lpParams->mfMipLodBias;
        lpState->mfAnisotropyBias = lpParams->mfAnisotropyBias;
        lpState->mbSeparateZFilterEnable = static_cast<u8>(lpParams->mbSeparateZFilterEnable != 0);
        lpState->muInitialized = 1;
        lpState->mbState26 = static_cast<u8>(lpParams->mbUnidentified44 != 0);
        return lpState;
    }
}
