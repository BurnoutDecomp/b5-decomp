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

    struct BlendMaterialState
    {
        u32 maState[19];
    };

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
