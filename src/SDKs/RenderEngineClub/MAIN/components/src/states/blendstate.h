#pragma once

#include "types.hpp"
#include <xtl.h>
#include "rw/rwcore_structs.h"
#include <cstddef>

namespace renderengine
{
    // Shared sampler descriptor vocabulary; five entries are written by the sizer.
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    // ARTIST82B627C8 and82B636B8: the actual nineteen-word native blend state.
    // The enums describe the original Xenos state consumers at827E7D10.
    class BlendState
    {
    public:
        typedef D3DBLEND Input;
        typedef D3DBLENDOP Operation;

        enum EWord
        {
            E_WORD_BLEND_RT0                = 0,   // packed Xenos RB_BLENDCONTROL, render target 0
            E_WORD_BLEND_RT1                = 1,
            E_WORD_BLEND_RT2                = 2,
            E_WORD_BLEND_RT3                = 3,
            E_WORD_COLOUR_WRITE_ENABLE      = 4,
            E_WORD_COLOUR_WRITE_ENABLE1     = 5,
            E_WORD_COLOUR_WRITE_ENABLE2     = 6,
            E_WORD_COLOUR_WRITE_ENABLE3     = 7,
            E_WORD_ALPHA_TO_MASK_OFFSETS    = 8,
            E_WORD_BLEND_FACTOR             = 9,
            E_WORD_ALPHA_TO_MASK_ENABLE     = 10,
            E_WORD_HIGH_PRECISION_BLEND     = 11,
            E_WORD_HIGH_PRECISION_BLEND1    = 12,
            E_WORD_HIGH_PRECISION_BLEND2    = 13,
            E_WORD_HIGH_PRECISION_BLEND3    = 14,
            E_WORD_ALPHA_FUNC               = 15,
            E_WORD_ALPHA_TEST_ENABLE        = 16,
            E_WORD_ALPHA_REF                = 17,
            E_WORD_INITIALISED              = 18
        };

        struct Parameters
        {
            // ARTIST82B64178..41D0, before the first descriptor/allocator call.
            Parameters()
                : muState15(7), muState4(15), muState5(15), muState6(15),
                  muState7(15), muState8(135), muState17(0), muState9(0xFFFFFFFFu),
                  mbHasCustomBlendFactors(0), mbState10(0), mbState11(0),
                  mbState12(0), mbState13(0), mbState14(0), mbState16(0)
            {
                for (u32 i = 0; i != 4; ++i) maBlendFactor[i] = 0x07060706u;
            }

            u32 maBlendFactor[4];
            u32 muState15;
            u32 muState4;
            u32 muState5;
            u32 muState6;
            u32 muState7;
            u32 muState8;
            u32 muState17;
            u32 muState9;
            u8 mbHasCustomBlendFactors;
            u8 mbState10;
            u8 mbState11;
            u8 mbState12;
            u8 mbState13;
            u8 mbState14;
            u8 mbState16;
        };

        static rw::ResourceDescriptor GetResourceDescriptor(const Parameters&);
        static BlendState* Initialize(const rw::Resource&, const Parameters&);
        void GetParameters(Parameters&) const;

        u32 maState[19];
    };

    // Existing caller spellings name the same native object and parameter types.
    typedef BlendState BlendMaterialState;
    typedef BlendState::Parameters BlendStateParameters;
    CGS_STATIC_ASSERT((sizeof(BlendState) == 0x4C), "ARTIST blend state");
    CGS_STATIC_ASSERT((sizeof(BlendState::Parameters) == 0x38), "ARTIST blend parameters");
    CGS_STATIC_ASSERT((offsetof(BlendState::Parameters, mbHasCustomBlendFactors) == 0x30), "ARTIST custom blend flag");
    CGS_STATIC_ASSERT((offsetof(BlendState::Parameters, mbState16) == 0x36), "ARTIST final blend flag");
}
