#include "pc/gcm/renderengine/renderstates.h"
#include "pc/gcm/renderengine/texture.h"   // renderengine::Texture (the bound raster)
#include "rw/rwcore_structs.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.h"
#include <cstddef>

#include <cstring>   // memcpy
#include <new>

// renderengine::TextureState creation -- the PC realisation of the X360 sampler-state path
//   GetResourceDescriptor  0x82B635C8
//   Initialize             0x82B62720 (-> SamplerState::Initialize, a Xenos GPU descriptor marshal)
// The runtime block is produced by the same canonical SamplerState::Initialize
// as ordinary sampler states. Native storage/pointer widths retain the existing
// PC allocation adapter; draw-time consumers read the original packed fields.

namespace renderengine
{
    // X360 0x82B635C8 builds rw::BaseResourceDescriptors<5>: base {0x20,4}+{0,1}x4, accumulated
    // (+=) with {{4,1},{0,1}x4} -> slot0 ~= {36,4}. Here slot0 is the PC TextureState object size.
    void TextureState::GetResourceDescriptor(u32* lpDescriptorOut)
    {
        lpDescriptorOut[0] = static_cast<u32>(sizeof(TextureState));  // X360: ~0x24 (36)
        lpDescriptorOut[1] = alignof(TextureState);
        lpDescriptorOut[2] = 0u;  lpDescriptorOut[3] = 1u;
        lpDescriptorOut[4] = 0u;  lpDescriptorOut[5] = 1u;
        lpDescriptorOut[6] = 0u;  lpDescriptorOut[7] = 1u;
        lpDescriptorOut[8] = 0u;  lpDescriptorOut[9] = 1u;
    }

    rw::ResourceDescriptor* TextureState::GetResourceDescriptor(rw::ResourceDescriptor* lpDescriptorOut)
    {
        u32 lauWords[10];
        GetResourceDescriptor(lauWords);
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lpDescriptorOut->m_baseResourceDescriptors[luLane].m_size = lauWords[2 * luLane];
            lpDescriptorOut->m_baseResourceDescriptors[luLane].m_alignment = lauWords[2 * luLane + 1];
        }
        return lpDescriptorOut;
    }

    // ARTIST82B6272C calls the canonical sampler producer, then82B62730..34
    // reads the raster from the parameter suffix and stores it after the block.
    // The flat native declaration retains the original72-byte parameter prefix.
    TextureState* TextureState::Initialize(rw::Resource* lpResourceMemory, const Parameters* lpParams)
    {
        void* lpMemory = lpResourceMemory ? lpResourceMemory->m_baseResources[0] : nullptr;
        TextureState* lpState = lpMemory ? new (lpMemory) TextureState() : new TextureState();
        static_assert(offsetof(Parameters, mpTexture) == sizeof(SamplerStateParameters),
                      "Texture parameters retain the original sampler prefix");
        static_assert(offsetof(Parameters, mfMipLodBias) == 0x2C,
                      "Original float-bit parameter lane");
        SamplerStateParameters lSamplerParameters;
        std::memcpy(&lSamplerParameters, lpParams, sizeof(lSamplerParameters));
        SamplerStateData* lapSamplerHandles[5] = {};
        lapSamplerHandles[0] = reinterpret_cast<SamplerStateData*>(lpState->mauSamplerState);
        SamplerState::Initialize(lapSamplerHandles, &lSamplerParameters);
        lpState->mpRaster = lpParams->mpTexture;
        return lpState;
    }
}
