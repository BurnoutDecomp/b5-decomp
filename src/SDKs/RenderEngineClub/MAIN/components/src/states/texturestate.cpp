#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"

#include <cstring>            // memcpy
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5> + its operator+=

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::TextureState::GetResourceDescriptor @ 0x82B635C8
//   renderengine::TextureState::Initialize            @ 0x82B62720
//
// A TextureState is a SamplerState (the 0x20-byte Xenos sampler block built by
// renderengine::SamplerState::Initialize) followed by a 4-byte bound-raster word
// at +0x20, so the X360 object is 0x24 bytes. GetResourceDescriptor builds the
// serialised five-entry rw resource descriptor for the whole TextureState: it is
// the SamplerState descriptor ({size=0x20, align=4} in slot 0, {0,1} in 1..4)
// accumulated (+=) with a {size=4, align=1} addend in slot 0 -- i.e. the sampler
// block plus the trailing raster pointer. Initialize delegates to SamplerState's
// param-unpack, then copies the raster pointer (params +0x48) into the state's
// +0x20 slot. Layout + stores follow the X360 asm (it overrides DWARF).

namespace renderengine
{
    // X360 0x82B635C8: build {0x20,4}+{0,1}x4 (the SamplerState descriptor) on the stack, copy it
    // into the out descriptor, then accumulate the trailing-raster addend {4,1}+{0,1}x4 with +=.
    rw::ResourceDescriptor* TextureState::GetResourceDescriptor(rw::ResourceDescriptor* lpDescriptor)
    {
        // The accumulated addend: slot0 = {size=4, align=1} (the raster pointer), slots 1..4 = {0,1}.
        rw::BaseResourceDescriptors<5> lAddend;
        for (u32 luIndex = 0; luIndex < 5; ++luIndex)
        {
            lAddend.m_baseResourceDescriptors[luIndex].m_size = 0;
            lAddend.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }
        lAddend.m_baseResourceDescriptors[0].m_size = 4;
        lAddend.m_baseResourceDescriptors[0].m_alignment = 1;

        // The base (SamplerState) descriptor: slot0 = {size=0x20, align=4}, slots 1..4 = {0,1}.
        rw::BaseResourceDescriptors<5> lBase;
        for (u32 luIndex = 0; luIndex < 5; ++luIndex)
        {
            lBase.m_baseResourceDescriptors[luIndex].m_size = 0;
            lBase.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }
        lBase.m_baseResourceDescriptors[0].m_size = 0x20;    // X360 64-bit store: low=0x20 (size), high=4 (align)
        lBase.m_baseResourceDescriptors[0].m_alignment = 4;

        std::memcpy(lpDescriptor, &lBase, sizeof(*lpDescriptor));
        *lpDescriptor += lAddend;
        return lpDescriptor;
    }

    // X360 0x82B62720: result = SamplerState::Initialize(lppState, &params); then the raster pointer
    // at params+0x48 is stored into the state's +0x20 slot (asm: lwz r11,0x48(r4); stw r11,0x20(r3)).
    TextureState* TextureState::Initialize(const rw::Resource* lpResource, const Parameters* lpParams)
    {
        // The shared sampler declaration retains its earlier member spellings;
        // its 0x48-byte parameter representation is the same native prefix.
        SamplerStateData* lpSampler = static_cast<SamplerStateData*>(lpResource->m_baseResources[0]);
        SamplerStateData* const lpResult = SamplerState::Initialize(
            &lpSampler, reinterpret_cast<const SamplerStateParameters*>(lpParams));
        TextureState* lpState = reinterpret_cast<TextureState*>(lpResult);
        lpState->mpRaster = lpParams->mpTexture;
        return lpState;
    }
}
