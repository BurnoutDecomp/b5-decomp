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
    void ApplyNativeSamplerState(const renderengine::SamplerState* state, u32 sampler, bool wasUnset)
    {
        D3DDevice* const device = gpD3DDevice;
        const u32 hardwareSampler = GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(sampler);
        GPUTEXTURE_FETCH_CONSTANT& fetch = device->m_Constants.TextureFetch[hardwareSampler];
        D3DTagCollection_Set(&device->m_Pending, D3DTag_Index(D3DTAG_TEXTUREFETCHCONSTANTS), D3DTag_SamplerMask(hardwareSampler));
        fetch.ClampX = state->mu8AddressU;
        fetch.ClampY = state->mu8AddressV;
        fetch.ClampZ = state->mu8AddressW;
        fetch.ClampPolicy = state->mu8ClampPolicy;
        fetch.MipFilter = state->mu8MipFilter;
        fetch.GradExpAdjustH = static_cast<s8>(state->mu8HGradientExpBias);
        fetch.GradExpAdjustV = static_cast<s8>(state->mu8VGradientExpBias);
        fetch.BorderColor = state->mu8BorderColor;
        fetch.ForceBCWToMax = state->mbForceBCWToMax;
        fetch.TriClamp = state->mu8TrilinearClamp;

        NativeSamplerShadow& cache = gSamplerShadow83010800[sampler];
        const f32 lodBias = state->mfMipLodBias;
        const f32 anisotropyBias = state->mfAnisotropyBias;
        u32 lodBiasBits, anisotropyBiasBits;
        memcpy(&lodBiasBits, &state->mfMipLodBias, sizeof(lodBiasBits));
        memcpy(&anisotropyBiasBits, &state->mfAnisotropyBias, sizeof(anisotropyBiasBits));
        if (wasUnset)
        {
            cache.filter[0] = state->mu8MinFilter; cache.filter[1] = state->mu8MinFilterZ;
            cache.filter[2] = state->mu8MagFilter; cache.filter[3] = state->mu8MagFilterZ;
            cache.filter[4] = state->mbSeparateZFilterEnable; cache.filter[5] = state->mu8MaxAnisotropy;
            cache.lodBias = lodBias; cache.anisotropyBias = anisotropyBias;
            cache.filter[6] = state->mu8MaxMipLevel; cache.filter[7] = state->mu8MinMipLevel;
            D3DDevice_SetSamplerState_MinFilter(device, sampler, state->mu8MinFilter);
            D3DDevice_SetSamplerState_MinFilterZ(device, sampler, state->mu8MinFilterZ);
            D3DDevice_SetSamplerState_MagFilter(device, sampler, state->mu8MagFilter);
            D3DDevice_SetSamplerState_MagFilterZ(device, sampler, state->mu8MagFilterZ);
            D3DDevice_SetSamplerState_SeparateZFilterEnable(device, sampler, state->mbSeparateZFilterEnable);
            D3DDevice_SetSamplerState_MaxAnisotropy(device, sampler, state->mu8MaxAnisotropy);
            D3DDevice_SetSamplerState_AnisotropyBias(device, sampler, anisotropyBiasBits);
            D3DDevice_SetSamplerState_MipMapLodBias(device, sampler, lodBiasBits);
            D3DDevice_SetSamplerState_MaxMipLevel(device, sampler, state->mu8MaxMipLevel);
            D3DDevice_SetSamplerState_MinMipLevel(device, sampler, state->mu8MinMipLevel);
            return;
        }
        if (cache.filter[0] != state->mu8MinFilter) { cache.filter[0] = state->mu8MinFilter; D3DDevice_SetSamplerState_MinFilter(device, sampler, state->mu8MinFilter); }
        if (cache.filter[1] != state->mu8MinFilterZ) { cache.filter[1] = state->mu8MinFilterZ; D3DDevice_SetSamplerState_MinFilterZ(device, sampler, state->mu8MinFilterZ); }
        if (cache.filter[2] != state->mu8MagFilter) { cache.filter[2] = state->mu8MagFilter; D3DDevice_SetSamplerState_MagFilter(device, sampler, state->mu8MagFilter); }
        if (cache.filter[3] != state->mu8MagFilterZ) { cache.filter[3] = state->mu8MagFilterZ; D3DDevice_SetSamplerState_MagFilterZ(device, sampler, state->mu8MagFilterZ); }
        if (cache.filter[4] != state->mbSeparateZFilterEnable) { cache.filter[4] = state->mbSeparateZFilterEnable; D3DDevice_SetSamplerState_SeparateZFilterEnable(device, sampler, state->mbSeparateZFilterEnable); }
        if (cache.filter[5] != state->mu8MaxAnisotropy) { cache.filter[5] = state->mu8MaxAnisotropy; D3DDevice_SetSamplerState_MaxAnisotropy(device, sampler, state->mu8MaxAnisotropy); }
        if (cache.anisotropyBias != anisotropyBias) { cache.anisotropyBias = anisotropyBias; D3DDevice_SetSamplerState_AnisotropyBias(device, sampler, anisotropyBiasBits); }
        if (cache.lodBias != lodBias) { cache.lodBias = lodBias; D3DDevice_SetSamplerState_MipMapLodBias(device, sampler, lodBiasBits); }
        if (cache.filter[6] != state->mu8MaxMipLevel) { cache.filter[6] = state->mu8MaxMipLevel; D3DDevice_SetSamplerState_MaxMipLevel(device, sampler, state->mu8MaxMipLevel); }
        if (cache.filter[7] != state->mu8MinMipLevel) { cache.filter[7] = state->mu8MinMipLevel; D3DDevice_SetSamplerState_MinMipLevel(device, sampler, state->mu8MinMipLevel); }
    }
}
