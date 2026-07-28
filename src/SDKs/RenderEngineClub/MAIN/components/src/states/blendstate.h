#ifndef RENDERENGINE_BLENDSTATE_H
#define RENDERENGINE_BLENDSTATE_H

#include "types.hpp"

// renderengine::BlendState -- the render-engine vendor wrapper around a fixed-function blend state.
// Declaration surface for the home (states/blendstate.cpp) and its consumers (the post-fx helper,
// which builds the shared opaque blend state). The bodies + the X360 layout commentary live in
// blendstate.cpp.
//
// LAYOUT (X360 asm authoritative; no DWARF/leak source exists for this vendor TU). The material is a
// 19-word block; the four leading words are the per-channel blend factors and the rest are the
// op/mask/flag words GetParameters/Initialize pack and unpack.
namespace renderengine
{
    struct ResourceDescriptorEntry
    {
        u32 muSize;
        u32 muAlignment;
    };

    // The 19-word marshalled block. Every word's meaning is attested by the X360 blend half of
    // the dispatch state triple, shadow::Device::Xbox2SetStateLowLevelShadowed @0x827E7D10,
    // which pushes each one through its own D3D fast-set in this order:
    //
    //   [0..3]  D3DDevice_SetBlendState(renderTarget 0..3, word)
    //   [4..7]  ColorWriteEnable / 1 / 2 / 3
    //   [8]     AlphaToMaskOffsets     [9]  BlendFactor      [10] AlphaToMaskEnable
    //   [11..14] HighPrecisionBlendEnable / 1 / 2 / 3
    //   [15]    AlphaFunc              [16] AlphaTestEnable  [17] AlphaRef      [18] initialised
    //
    // The maState[i] array form is kept (that is the object's own shape, and BlendState::
    // Initialize @0x82B627C8 fills it positionally); the enumerators below name the lanes so
    // consumers index them by meaning rather than by number.
    struct BlendMaterialState
    {
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
            E_WORD_INITIALISED              = 18,
        };

        u32 maState[19];
    };

    // The CONSTRUCTION parameter block. BlendState::Initialize @0x82B627C8 copies it into the
    // marshalled block above field for field, which is what fixes each member's meaning:
    //   maBlendFactor[4] -> maState[0..3]  (or the 0x00010001 ONE/ZERO/add default when
    //                                       mbHasCustomBlendFactors is 0)
    //   muState15 -> [15] AlphaFunc      muState4  -> [4]  ColorWriteEnable
    //   muState5  -> [5]  ColorWriteEnable1        muState6  -> [6]  ColorWriteEnable2
    //   muState7  -> [7]  ColorWriteEnable3        muState8  -> [8]  AlphaToMaskOffsets
    //   muState17 -> [17] AlphaRef                 muState9  -> [9]  BlendFactor
    //   mbState10 -> [10] AlphaToMaskEnable        mbState11..14 -> [11..14] HighPrecisionBlend*
    //   mbState16 -> [16] AlphaTestEnable
    // The member names are left as written by the TUs that already build these blocks; the
    // mapping above is the recovered semantics.
    struct BlendStateParameters
    {
        u32 maBlendFactor[4];
        u32 muState15;
        u32 muState4;
        u32 muState5;
        u32 muState6;
        u32 muState7;
        u32 muState8;
        u32 muState17;
        u32 muState9;
        u8  mbHasCustomBlendFactors;
        u8  mbState10;
        u8  mbState11;
        u8  mbState12;
        u8  mbState13;
        u8  mbState14;
        u8  mbState16;
    };

    class BlendState
    {
    public:
        // Unpack a packed BlendMaterialState into a BlendStateParameters struct.
        // X360 pseudocode shows 2-arg call (no visible 'this') → static.
        static void* GetParameters(const BlendMaterialState* pMaterial, BlendStateParameters* pParams);
        static void* GetResourceDescriptor(void* pOut);
        // X360 the descriptor build is sized from the resource only and ignores the params, but the
        // immediate-mode state-library builder (CgsGraphics::ImRendererBase::Construct*BlendState)
        // passes the params block alongside the out buffer. Additive overload so that call compiles;
        // the params are unused (the descriptor is a fixed { 0x4C, 4 } block).
        static void* GetResourceDescriptor(void* pOut, const BlendStateParameters* pParams);
        static void* Initialize(BlendMaterialState** ppMaterial, const BlendStateParameters* pParams);
    };
}

#endif // RENDERENGINE_BLENDSTATE_H
