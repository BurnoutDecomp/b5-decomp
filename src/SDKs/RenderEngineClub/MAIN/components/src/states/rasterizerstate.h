#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include <cstddef>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/resourcedescriptor.h"

// Native ARTIST state and construction parameters; no host renderer storage.
namespace renderengine
{
class RasterizerState
{
public:
    // ARTIST827F1C20 passes 0/2/1; the factory827EBF30 identifies their
    // none/back/front roles. Enumerator spellings here describe those roles.
    enum CullMode
    {
        E_CULL_NONE = 0,
        E_CULL_FRONT = 1,
        E_CULL_BACK = 2
    };

    struct Parameters
    {
        // ARTIST82B64498..44E0; natural padding1D..1F is untouched.
        Parameters()
            : muFillMode(0), muCullMode(2), mfDepthBias(0.0f), mfSlopeScaleDepthBias(0.0f),
              muMultiSampleMask(0xFFFFFFFFu), muPrimitiveResetIndex(0xFFFF),
              mbScissorTestEnable(0), mbMultiSampleAntiAlias(1), mbViewportEnable(1),
              mbHalfPixelOffset(0), mbPrimitiveResetEnable(1) {}

        u32 muFillMode;
        u32 muCullMode;
        f32 mfDepthBias;
        f32 mfSlopeScaleDepthBias;
        u32 muMultiSampleMask;
        u32 muPrimitiveResetIndex;
        u8  mbScissorTestEnable;
        u8  mbMultiSampleAntiAlias;
        u8  mbViewportEnable;
        u8  mbHalfPixelOffset;
        u8  mbPrimitiveResetEnable;
    };

    static rw::ResourceDescriptor GetResourceDescriptor(const Parameters&);
    static RasterizerState* Initialize(const rw::Resource&, const Parameters&);

    // The 13-word marshalled block. Every word's meaning is attested by the X360 rasteriser
    // half of the dispatch state triple (@0x827E8690), which pushes each one through its own
    // D3DDevice_SetRenderState_* fast-set in this order -- so these are names for words that
    // were previously an opaque maState[13].
    u32 muFillMode;                // +0x00 -> D3DDevice_SetRenderState_FillMode
    u32 muCullMode;                // +0x04 -> CullMode
    u32 muDepthBias;               // +0x08 -> DepthBias
    u32 muSlopeScaleDepthBias;     // +0x0C -> SlopeScaleDepthBias
    u32 muMultiSampleMask;         // +0x10 -> MultiSampleMask
    u32 muScissorTestEnable;       // +0x14 -> ScissorTestEnable
    u32 muMultiSampleAntiAlias;    // +0x18 -> MultiSampleAntiAlias
    u32 muUnused1C;                // +0x1C  (word 7: the applier never reads it)
    u32 muViewportEnable;          // +0x20 -> ViewportEnable
    u32 muHalfPixelOffset;         // +0x24 -> HalfPixelOffset
    u32 muPrimitiveResetEnable;    // +0x28 -> PrimitiveResetEnable (the word MaterialState::
                                   //          FixDown @0x828A8A80 marks)
    u32 muPrimitiveResetIndex;     // +0x2C -> PrimitiveResetIndex (bound only when enabled)
    u32 muInitialised;             // +0x30  1 once Initialize has run
};
CGS_STATIC_ASSERT((sizeof(RasterizerState) == 0x34), "X360 rasterizer state");
CGS_STATIC_ASSERT((sizeof(RasterizerState::Parameters) == 0x20), "X360 rasterizer parameters");
CGS_STATIC_ASSERT((offsetof(RasterizerState::Parameters, mbPrimitiveResetEnable) == 0x1C), "X360 last rasterizer parameter");
}
