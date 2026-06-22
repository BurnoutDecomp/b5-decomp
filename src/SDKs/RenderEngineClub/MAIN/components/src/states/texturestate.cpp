#include "types.hpp"

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
    // ---- renderengine::SamplerState (canonical X360 home: states/samplerstate.cpp) -------------
    // Re-declared here only to name the dependency Initialize calls. The full body / param-unpack
    // lives in samplerstate.cpp; these two structs mirror its committed layout so the +0x20 raster
    // store lands immediately past the 0x20-byte sampler block.
    struct SamplerStateData
    {
        u32 muColor;                  // +0x00
        u32 muRemap;                  // +0x04
        u8  mu8AddressU;              // +0x08
        u8  mu8AddressV;              // +0x09
        u8  mu8AddressW;              // +0x0A
        u8  mbState11Zero;            // +0x0B
        u8  mu8MinLevel;              // +0x0C
        u8  mu8DepthTextureFunction;  // +0x0D
        u8  mu8Gamma;                 // +0x0E
        u8  mbState15;                // +0x0F
        u8  mbState16;                // +0x10
        u8  mu8State17;               // +0x11
        u8  mu8MaxLevel;              // +0x12
        u8  mu8MagFilter;             // +0x13
        u8  mu8Bias;                  // +0x14
        u8  mu8MaxAnisotropy;         // +0x15
        u8  mbState22;                // +0x16
        u8  mu8Convolution;           // +0x17
        u8  mu8MipFilter;             // +0x18
        u8  mu8MinFilter;             // +0x19
        u8  mbState26;                // +0x1A
        u8  mu8Pad27;                 // +0x1B
        u32 muInitialized;            // +0x1C
    };

    struct SamplerStateParameters
    {
        u32 muAddressU;             // +0x00
        u32 muAddressV;             // +0x04
        u32 muAddressW;             // +0x08
        u32 muBias;                 // +0x0C
        u32 muMaxLevel;             // +0x10
        u32 muMinLevel;             // +0x14
        u32 muMaxAnisotropy;        // +0x18
        u32 muMagFilter;            // +0x1C
        u32 muMinFilter;            // +0x20
        u32 muMipFilter;            // +0x24
        u32 muConvolution;          // +0x28
        u32 muColor;                // +0x2C
        u32 muRemap;                // +0x30
        u32 muDepthTextureFunction; // +0x34
        u32 muGamma;                // +0x38
        u32 muState17;              // +0x3C
        u8  mbState15;              // +0x40
        u8  mbState22;              // +0x41
        u8  mbState16;              // +0x42
        u8  mbState11Zero;          // +0x43
        u8  mbState26;              // +0x44
    };

    class SamplerState
    {
    public:
        static SamplerStateData* Initialize(SamplerStateData** lppState, const SamplerStateParameters* lpParams);
    };

    // ---- renderengine::TextureState ------------------------------------------------------------
    // SamplerState param block at +0x48 carries the bound raster pointer (the field past the 0x44
    // byte-flags); TextureState::Initialize copies it into the state's +0x20 raster slot.
    struct TextureStateParameters
    {
        SamplerStateParameters mSampler;  // +0x00 sampler params (0x48 bytes consumed by SamplerState)
        u32                    muRaster;  // +0x48 bound-raster pointer (X360 32-bit)
    };

    struct TextureStateData
    {
        SamplerStateData mSampler;  // +0x00 the 0x20-byte sampler block
        u32              muRaster;  // +0x20 bound raster pointer (X360 32-bit)
    };

    static_assert(sizeof(SamplerStateData) == 0x20, "SamplerStateData must be the 0x20-byte sampler block");

    class TextureState
    {
    public:
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor);
        static TextureStateData* Initialize(TextureStateData** lppState, const TextureStateParameters* lpParams);
    };

    // X360 0x82B635C8: build {0x20,4}+{0,1}x4 (the SamplerState descriptor) on the stack, copy it
    // into the out descriptor, then accumulate the trailing-raster addend {4,1}+{0,1}x4 with +=.
    rw::BaseResourceDescriptors<5>* TextureState::GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor)
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
    TextureStateData* TextureState::Initialize(TextureStateData** lppState, const TextureStateParameters* lpParams)
    {
        SamplerStateData* lpSampler = reinterpret_cast<SamplerStateData**>(lppState)[0];
        SamplerState::Initialize(reinterpret_cast<SamplerStateData**>(lppState), &lpParams->mSampler);
        TextureStateData* lpState = reinterpret_cast<TextureStateData*>(lpSampler);
        lpState->muRaster = lpParams->muRaster;
        return lpState;
    }
}
