#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsRasterizerStateFactory::Construct  @ 0x827EBF30
//
// Builds the fixed set of three rasteriser states the immediate-mode renderer, the
// post-fx chain and the dispatch interpreter select between. Only Construct is
// X360-attested for this TU (Destruct / Prepare / GetState are DWARF-only -- see the
// header, which is NEW in this wave: this class used to be declared TU-locally right
// here, non-polymorphically and with a Hex-Rays return type).

// Declared CgsRasterizerStateFactory.h:52, defined CgsRasterizerStateFactory.cpp:25
// (DWARF) -- the three X360 file-scope dwords dword_83010A38 / dword_83010A3C /
// dword_83010A40, now the class's private static table rather than a TU-local array,
// so that its readers (BrnPostFx::Render wants slot 2) can name a slot.
renderengine::RasterizerState* CgsRasterizerStateFactory::saRasterizerStates[E_FACTORY_RASTERIZER_STATE_COUNT] = {};

namespace
{
    // Carve the backing store for one rasteriser state through the supplied resource
    // allocator, then initialise it. X360: size via RasterizerState::GetResourceDescriptor
    // (slot-0 {52, 4}), carve through the allocator's vtable slot +0x10 (DoAllocate) --
    // `lwz r11,0(r31)` / `lwz r11,0x10(r11)` / `bctrl` @0x827EBFC8..0x827EBFE4 -- copy the
    // five returned handle words into the state-handle array (the five-iteration
    // `lwz/stw` loop @0x827EBFF8..0x827EC008), then RasterizerState::Initialize.
    //
    // The allocator is reached BY NAME through CgsGraphics::ResourceAllocatorCreate, not
    // through the old TU-local single-virtual `ResourceAllocator` shim this file used to
    // declare: that shim put `Create` at vtable SLOT 0, which on the rw::IResourceAllocator
    // actually behind the cast is the VIRTUAL DESTRUCTOR -- it allocated nothing and left
    // the allocator permanently downgraded. The whole diagnosis is in
    // CgsResourceAllocatorCreate.h; this is the last of the fifteen shims in that family
    // to be retired, and it is retired here only because Construct's parameter is now the
    // DWARF's typed rw::IResourceAllocator* instead of a void*.
    renderengine::RasterizerState* CreateRasterizerState(
        rw::IResourceAllocator* lpAllocator,
        const renderengine::RasterizerState::Parameters* lpParameters)
    {
        renderengine::ResourceDescriptor5 lDescriptor;
        renderengine::RasterizerState* lapStateHandles[5] = {};
        renderengine::RasterizerState* lapAllocatedHandles[5] = {};

        renderengine::RasterizerState::GetResourceDescriptor(&lDescriptor, lpParameters);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, lapAllocatedHandles, &lDescriptor);
        for (int liHandle = 0; liHandle < 5; ++liHandle)
            lapStateHandles[liHandle] = lapAllocatedHandles[liHandle];

        return renderengine::RasterizerState::Initialize(lapStateHandles, lpParameters);
    }
}

// MOVED OUT 2026-08-15 (step-3 vignette/dof/raster wave): the definitions of
// renderengine::RasterizerState::GetResourceDescriptor and ::Initialize that used to sit here now
// live in pc/gcm/renderengine/RasterizerState.cpp, beside the member overload they call and next to
// their siblings DepthStencilState.cpp / states/blendstate.cpp / states/samplerstate.cpp. They are
// renderengine's, not the factory's: on the X360 they are @0x82B63738 and @0x82B62958, inside the
// same run of the render-engine state library as BlendState::Initialize @0x82B627C8 and
// SamplerState::GetResourceDescriptor @0x82B63588 -- nowhere near CgsRasterizerStateFactory::Construct
// @0x827EBF30, which is what this file legitimately owns.
//
// This is a MOVE, not a copy: a second definition of either symbol is LNK2005 the moment both TUs are
// on tools/build/build_game_exe.bat (neither is today). The bodies are unchanged apart from the
// descriptor size, which is now spelled `static_cast<u32>(sizeof(RasterizerState))` with the console's
// 0x34 = 52 in a comment beside it, matching the idiom the rest of the tree uses.

// @ 0x827EBF30 -- build the three rasteriser states into saRasterizerStates, asserting
// each slot came back non-null. All three share one parameter block; only muCullMode
// differs (2 = back, `(&4)|1` = 1 = front, `&=4` = 0 = none).
//
// ⚠ RETURN TYPE CORRECTED THIS WAVE: void, not renderengine::RasterizerState*. The asm
// epilogue @0x827EC174-0x827EC178 is `addi r1,r1,0x140 / b __restgprlr_28` with no
// return-value setup; Hex-Rays' `result` is just the third Initialize's r3 still being
// live. DWARF agrees (CgsRasterizerStateFactory.cpp:45, `virtual void Construct
// (rw::IResourceAllocator *)`), and nothing in the tree read the old return value.
void CgsRasterizerStateFactory::Construct(rw::IResourceAllocator* lpAllocator)
{
    renderengine::RasterizerState::Parameters lParameters = {};

    lParameters.muMultisampleEnable = 0xffffffff;
    lParameters.muAntialiasedLineEnable = 0xffff;
    lParameters.mu8PaddingMode = 1;
    lParameters.mu8DepthClipEnable = 1;
    lParameters.mu8FrontCounterClockwise = 1;
    lParameters.mu8ScissorEnable = 1;

    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] = nullptr;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] = nullptr;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] = nullptr;

    lParameters.muFillMode = 0;
    lParameters.muCullMode = 2;
    lParameters.mu8ConservativeRaster = 0;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK] =
        CreateRasterizerState(lpAllocator, &lParameters);
    CGS_ASSERT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]");

    lParameters.mu8ScissorEnable = 1;
    lParameters.muCullMode = (lParameters.muCullMode & 4) | 1;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT] =
        CreateRasterizerState(lpAllocator, &lParameters);
    CGS_ASSERT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]");

    lParameters.mu8ScissorEnable = 1;
    lParameters.muCullMode &= 4u;
    saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE] =
        CreateRasterizerState(lpAllocator, &lParameters);
    CGS_ASSERT(saRasterizerStates[E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE],
               "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeNone ]");
}
