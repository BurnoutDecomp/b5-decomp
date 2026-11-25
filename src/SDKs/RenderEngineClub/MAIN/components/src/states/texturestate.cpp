#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"

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
    // Retain ARTIST's compact constructor loops and by-value descriptor copy.
#pragma optimize("s", on)
    rw::ResourceDescriptor TextureState::GetResourceDescriptor(const Parameters&)
    {
        rw::ResourceDescriptor addend(sizeof(Texture*), 1);
        rw::ResourceDescriptor sampler(sizeof(SamplerState), __alignof(SamplerState));
        rw::ResourceDescriptor descriptor(sampler);
        descriptor += addend;
        return descriptor;
    }
#pragma optimize("s", off)


}
