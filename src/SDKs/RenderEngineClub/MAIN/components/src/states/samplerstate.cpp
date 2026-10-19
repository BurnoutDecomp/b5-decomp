#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"

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
