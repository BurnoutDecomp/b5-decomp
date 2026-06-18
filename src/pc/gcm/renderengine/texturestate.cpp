#include "pc/gcm/renderengine/renderstates.h"
#include "pc/gcm/renderengine/texture.h"   // renderengine::Texture (the bound raster)

#include <cstring>   // memcpy

// renderengine::TextureState creation -- the PC realisation of the X360 sampler-state path
//   GetResourceDescriptor  0x82B635C8
//   Initialize             0x82B62720 (-> SamplerState::Initialize, a Xenos GPU descriptor marshal)
// On X360 a TextureState is a marshalled GPU sampler descriptor allocated in graphics memory; on PC
// a sampler state is a CPU config the device applies at draw time, so Initialize keeps the sampler
// parameters + the bound raster rather than emitting a GPU descriptor. [PC DIVERGENCE]

namespace renderengine
{
    // X360 0x82B635C8 builds rw::BaseResourceDescriptors<5>: base {0x20,4}+{0,1}x4, accumulated
    // (+=) with {{4,1},{0,1}x4} -> slot0 ~= {36,4}. Here slot0 is the PC TextureState object size.
    void TextureState::GetResourceDescriptor(u32* lpDescriptorOut)
    {
        lpDescriptorOut[0] = static_cast<u32>(sizeof(TextureState));  // X360: ~0x24 (36)
        lpDescriptorOut[1] = 4u;
        lpDescriptorOut[2] = 0u;  lpDescriptorOut[3] = 1u;
        lpDescriptorOut[4] = 0u;  lpDescriptorOut[5] = 1u;
        lpDescriptorOut[6] = 0u;  lpDescriptorOut[7] = 1u;
        lpDescriptorOut[8] = 0u;  lpDescriptorOut[9] = 1u;
    }

    // X360 0x82B62720 delegates to SamplerState::Initialize (marshals the params into a Xenos GPU
    // sampler descriptor in the supplied graphics resource). PC: allocate the sampler-state object,
    // bind the raster, and keep the (opaque) sampler config; the D3D sampler is set from it at draw
    // time. [PC DIVERGENCE: lpResourceMemory / the rw allocator dance is X360 graphics memory mgmt.]
    TextureState* TextureState::Initialize(rw::Resource* /*lpResourceMemory*/, const Parameters* lpParams)
    {
        TextureState* lpState = new TextureState();
        lpState->mpRaster = lpParams->mpTexture;
        // Keep the sampler config (address/filter/lod) for draw-time application. The X360 marshals
        // it into the 32-byte GPU descriptor; here we just stash the leading config bytes.
        memcpy(lpState->mauSamplerState, lpParams, sizeof(lpState->mauSamplerState));
        return lpState;
    }
}
