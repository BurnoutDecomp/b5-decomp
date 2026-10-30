#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.h"
#include <cstring>

namespace shadow
{
    // ARTIST83010800: eight cached bytes and two float words per sampler.
    struct NativeSamplerShadow
    {
        u8 filter[8];
        f32 lodBias;
        f32 anisotropyBias;
    };
    CGS_STATIC_ASSERT((sizeof(NativeSamplerShadow) == 16), "ARTIST sampler shadow stride");
    NativeSamplerShadow gSamplerShadow83010800[16] = {};

    // ARTIST827E8950. The XDK owns both texture-fetch and pending-tag layouts.
    void ApplyNativeSamplerState(const renderengine::SamplerStateData* state, u32 sampler, bool wasUnset)
    {
        D3DDevice* const device = gpD3DDevice;
        const u32 hardwareSampler = GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(sampler);
        GPUTEXTURE_FETCH_CONSTANT& fetch = device->m_Constants.TextureFetch[hardwareSampler];
        D3DTagCollection_Set(&device->m_Pending, D3DTag_Index(D3DTAG_TEXTUREFETCHCONSTANTS), D3DTag_SamplerMask(hardwareSampler));
        fetch.ClampX = state->mu8AddressU;
        fetch.ClampY = state->mu8AddressV;
        fetch.ClampZ = state->mu8AddressW;
        fetch.ClampPolicy = state->mbState11Zero;
        fetch.MipFilter = state->mu8MinLevel;
        fetch.GradExpAdjustH = static_cast<s8>(state->mu8DepthTextureFunction);
        fetch.GradExpAdjustV = static_cast<s8>(state->mu8Gamma);
        fetch.BorderColor = state->mbState15;
        fetch.ForceBCWToMax = state->mbState16;
        fetch.TriClamp = state->mu8State17;

        NativeSamplerShadow& cache = gSamplerShadow83010800[sampler];
        f32 lodBias, anisotropyBias;
        memcpy(&lodBias, &state->muColor, sizeof(lodBias));
        memcpy(&anisotropyBias, &state->muRemap, sizeof(anisotropyBias));
        if (wasUnset)
        {
            cache.filter[0] = state->mu8MaxLevel; cache.filter[1] = state->mu8MagFilter;
            cache.filter[2] = state->mu8Bias; cache.filter[3] = state->mu8MaxAnisotropy;
            cache.filter[4] = state->mbState22; cache.filter[5] = state->mu8Convolution;
            cache.lodBias = lodBias; cache.anisotropyBias = anisotropyBias;
            cache.filter[6] = state->mu8MipFilter; cache.filter[7] = state->mu8MinFilter;
            D3DDevice_SetSamplerState_MinFilter(device, sampler, state->mu8MaxLevel);
            D3DDevice_SetSamplerState_MinFilterZ(device, sampler, state->mu8MagFilter);
            D3DDevice_SetSamplerState_MagFilter(device, sampler, state->mu8Bias);
            D3DDevice_SetSamplerState_MagFilterZ(device, sampler, state->mu8MaxAnisotropy);
            D3DDevice_SetSamplerState_SeparateZFilterEnable(device, sampler, state->mbState22);
            D3DDevice_SetSamplerState_MaxAnisotropy(device, sampler, state->mu8Convolution);
            D3DDevice_SetSamplerState_AnisotropyBias(device, sampler, state->muRemap);
            D3DDevice_SetSamplerState_MipMapLodBias(device, sampler, state->muColor);
            D3DDevice_SetSamplerState_MaxMipLevel(device, sampler, state->mu8MipFilter);
            D3DDevice_SetSamplerState_MinMipLevel(device, sampler, state->mu8MinFilter);
            return;
        }
        if (cache.filter[0] != state->mu8MaxLevel) { cache.filter[0] = state->mu8MaxLevel; D3DDevice_SetSamplerState_MinFilter(device, sampler, state->mu8MaxLevel); }
        if (cache.filter[1] != state->mu8MagFilter) { cache.filter[1] = state->mu8MagFilter; D3DDevice_SetSamplerState_MinFilterZ(device, sampler, state->mu8MagFilter); }
        if (cache.filter[2] != state->mu8Bias) { cache.filter[2] = state->mu8Bias; D3DDevice_SetSamplerState_MagFilter(device, sampler, state->mu8Bias); }
        if (cache.filter[3] != state->mu8MaxAnisotropy) { cache.filter[3] = state->mu8MaxAnisotropy; D3DDevice_SetSamplerState_MagFilterZ(device, sampler, state->mu8MaxAnisotropy); }
        if (cache.filter[4] != state->mbState22) { cache.filter[4] = state->mbState22; D3DDevice_SetSamplerState_SeparateZFilterEnable(device, sampler, state->mbState22); }
        if (cache.filter[5] != state->mu8Convolution) { cache.filter[5] = state->mu8Convolution; D3DDevice_SetSamplerState_MaxAnisotropy(device, sampler, state->mu8Convolution); }
        if (cache.anisotropyBias != anisotropyBias) { cache.anisotropyBias = anisotropyBias; D3DDevice_SetSamplerState_AnisotropyBias(device, sampler, state->muRemap); }
        if (cache.lodBias != lodBias) { cache.lodBias = lodBias; D3DDevice_SetSamplerState_MipMapLodBias(device, sampler, state->muColor); }
        if (cache.filter[6] != state->mu8MipFilter) { cache.filter[6] = state->mu8MipFilter; D3DDevice_SetSamplerState_MaxMipLevel(device, sampler, state->mu8MipFilter); }
        if (cache.filter[7] != state->mu8MinFilter) { cache.filter[7] = state->mu8MinFilter; D3DDevice_SetSamplerState_MinMipLevel(device, sampler, state->mu8MinFilter); }
    }
}
