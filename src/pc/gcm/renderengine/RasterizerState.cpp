#include "renderstates.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::RasterizerState::Initialize            @ 0x82B62958
//   renderengine::RasterizerState::GetResourceDescriptor @ 0x82B63738
//
// The rasteriser third of the render-state triple, and the direct sibling of
// pc/gcm/renderengine/DepthStencilState.cpp (@0x82B62890 / @0x82B636F8) and
// SDKs/RenderEngineClub/.../states/blendstate.cpp (@0x82B627C8 / @0x82B636B8). Same two-function
// shape: a fixed { size, alignment } descriptor builder and a straight-line parameter-block copy.
//
// ==================================================================================================
// WHERE THESE TWO BODIES CAME FROM, AND WHY THIS FILE IS THEIR HOME.
//
// They were ALREADY WRITTEN, correctly, but in the wrong translation unit --
// GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.cpp:61 and :76 -- while this file held
// only the member-function overload, against a `maState[13]` array that renderstates.h no longer has:
//
//     $ cl /c ... b5-decomp\src\pc\gcm\renderengine\RasterizerState.cpp
//     RasterizerState.cpp(7):  error C2065: 'maState': undeclared identifier
//     ... twelve times, through line 18 ...
//
// So the committed RasterizerState.cpp did not compile at all. It is not on
// tools/build/build_game_exe.bat (grep pasted in this wave's report), which is why nobody hit it.
//
// The two static bodies are MOVED here rather than copied: `renderengine::RasterizerState::Initialize`
// and `::GetResourceDescriptor` can have exactly one definition in the program, and the X360 puts
// them in the renderengine state library (0x82B62958 / 0x82B63738 sit inside the same run as
// BlendState::Initialize @0x82B627C8, DepthStencilState::Initialize @0x82B62890,
// SamplerState::GetResourceDescriptor @0x82B63588 ...), not in the game's factory. The factory's copy
// is deleted by the paired edit; the factory keeps CgsRasterizerStateFactory::Construct, which is its
// own X360 function (@0x827EBF30) and the only thing that belongs to it.
//
// GetResourceDescriptor's ADDRESS is recovered, its BODY is inferred. 0x82B63738 is the call target
// recorded in CgsRasterizerStateFactory::Construct's xrefs_from (`bl
// renderengine__RasterizerState__GetResourceDescriptor` @0x827EBFC4 / @0x827EC064 / @0x827EC0F8), but
// like DepthStencilState::Initialize @0x82B62890 the function is absent from the JSON export subset,
// so its immediates cannot be read. The size is nevertheless pinned three independent ways:
//   * Initialize @0x82B62958 stores through +0x30 and no further -> the object ends at 0x34;
//   * the serialised world MaterialState blob lays its three sub-objects out as
//     blend 0x0C..0x58 (0x4C), depth-stencil 0x5C..0xBC (0x60), rasterizer 0xBC..0xF0 (0x34), and
//     tools/assets/bundles/world_type_transcode.py marks 0xF0..0xFC as a serializer PAD
//     (MS_RAW_PADS), not as part of the object;
//   * the pre-existing (moved) body already read 52 == 0x34.
// The alignment 4 is the value all four siblings use.
// ==================================================================================================

namespace renderengine
{
// X360 0x82B63738: five { size 0, alignment 1 } entries, then entry 0 overwritten with the object's
// size and alignment 4 -- the identical shape to BlendState (0x4C) / DepthStencilState (0x60) /
// SamplerState (0x20) / TextureState.
ResourceDescriptor5* RasterizerState::GetResourceDescriptor(
    ResourceDescriptor5* lpDescriptor,
    const Parameters* /*lpParameters*/)
{
    for (ResourceDescriptor5::Entry& lEntry : lpDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    // HOST sizeof, not the console immediate: every member of RasterizerState is a u32, so the two
    // are equal here (0x34 == 52) -- but the idiom is the tree's (CgsRwTextureStateResourceType.cpp:34,
    // DepthStencilState.cpp:16) and it stays correct if a member is ever retyped.
    lpDescriptor->maEntries[0].muSize = static_cast<u32>(sizeof(RasterizerState));  // X360: 0x34
    lpDescriptor->maEntries[0].muAlignment = 4;
    static_assert(sizeof(RasterizerState) == 0x34, "the X360 descriptor sizes the object at 0x34 (52)");
    return lpDescriptor;
}

// Create a rasteriser state from the parameter block (the allocator has already carved the backing
// store the descriptor sized; *ppState is the first handle -- X360 `lwz r3, 0(r3)` @0x82B62960).
RasterizerState* RasterizerState::Initialize(RasterizerState** ppState, const Parameters* lpParameters)
{
    return (*ppState)->Initialize(lpParameters);
}

// The copy itself. X360 0x82B62958-0x82B629BC, thirteen destinations, and it is NOT positional: the
// parameter block is PERMUTED relative to the marshalled block (the same way BlendState's is, and
// unlike DepthStencilState's). Each line below carries the instruction pair that attests it, and the
// destination names are the ones the dispatch applier
// shadow::Device::Xbox2SetRasterizerStateLowLevelShadowed @0x827E8690 pushes through its matching
// D3DDevice_SetRenderState_* fast-set (renderstates.h).
//
// NOTE THE FIVE BYTE FIELDS: Parameters +0x18..+0x1C are u8, read with `lbz` and widened one per
// word into object words +0x14/+0x18/+0x20/+0x24/+0x28. Reading them as u32 would shear the tail --
// this is what makes the block 0x1D bytes of parameters, not 0x2C.
RasterizerState* RasterizerState::Initialize(const Parameters* lpParameters)
{
    muFillMode            = lpParameters->muFillMode;               // lwz 0x00 / stw 0x00
    muCullMode            = lpParameters->muCullMode;               // lwz 0x04 / stw 0x04
    muDepthBias           = lpParameters->muDepthBias;              // lwz 0x08 / stw 0x08
    muSlopeScaleDepthBias = lpParameters->muSlopeScaledDepthBias;   // lwz 0x0C / stw 0x0C
    muMultiSampleMask     = lpParameters->muMultisampleEnable;      // lwz 0x10 / stw 0x10
    muPrimitiveResetIndex = lpParameters->muAntialiasedLineEnable;  // lwz 0x14 / stw 0x2C
    muScissorTestEnable   = lpParameters->mu8ScissorEnable;         // lbz 0x18 / stw 0x14
    muMultiSampleAntiAlias= lpParameters->mu8DepthClipEnable;       // lbz 0x19 / stw 0x18
    muViewportEnable      = lpParameters->mu8FrontCounterClockwise; // lbz 0x1A / stw 0x20
    muHalfPixelOffset     = lpParameters->mu8ConservativeRaster;    // lbz 0x1B / stw 0x24
    muPrimitiveResetEnable= lpParameters->mu8PaddingMode;           // lbz 0x1C / stw 0x28
    muInitialised         = 1u;                                     // li 1     / stw 0x30
    return this;
}
}
