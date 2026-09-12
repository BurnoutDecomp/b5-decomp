#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"  // renderengine::BlendMaterialState
#include "rw/rwcore_structs.h"                                            // rw::IResourceAllocator

// =============================================================================
// CgsBlendStateFactory.h  (GameShared/GameClasses/Graphics)
//
// CgsBlendStateFactory -- builds the fixed set of NINE blend states the renderer,
// the post-fx chain and the dispatch interpreter select between, and owns them in
// the private static table saBlendStates.
//
// SOURCE OF TRUTH for everything below: the shipped console build of
// CgsBlendStateFactory::Construct (2248 bytes, 562 instructions). That function is
// a named symbol the per-function export set skips entirely, which is why an
// earlier pass could only name it from its callees' cross-references.
//
// -----------------------------------------------------------------------------
// SHAPE. The original header declares a single-vptr class, no base, and NO
// non-static data members:
//     vptr
//     CgsBlendStateFactory()
//     virtual void Construct(rw::IResourceAllocator*)
//     virtual void Destruct()
//     virtual bool Prepare()
//     BlendState* GetState(uint32_t)
//   private:
//     static BlendState* saBlendStates[9]
//
// CONSTRUCT IS A NON-STATIC MEMBER, and the attested calling convention says so,
// not the declaration: the console prologue keeps the allocator in the SECOND
// argument slot, dispatches the allocator call through it, and never reads the
// first slot at all. A function whose first argument slot is present but unused,
// with the real first argument one slot along, is a member function with an unused
// `this`. (The original declarations elide the implicit `this` from every member
// they print, so they cannot be used as evidence of staticness either way.)
//
// CONSTRUCT IS VIRTUAL. Nothing in the whole console image calls it directly --
// the only references to it are the six callees listing it among their own
// callers (renderengine::BlendState::Initialize, ::GetResourceDescriptor,
// CgsDev::Assert::BeginAssert/FireAssert/EndAssert and the register-save helper).
// The same holds for both sibling factories. No direct caller is what vtable
// dispatch looks like, and it is what the original `virtual` says.
//
// GetState: STATICNESS UNDETERMINED, and recorded as such rather than settled.
// The original declares it inline in the header, and the console image carries no
// GetState symbol for any of the three factories, so it is folded into its readers
// and there is NO CALL SITE anywhere to read the convention off. It is declared
// here as a normal member, matching the committed sibling
// CgsDepthStencilStateFactory.h, and that choice is a convention match, NOT an
// attestation. OPEN QUESTION, for whoever writes the readers: SEVEN of the TWELVE
// console readers of this table are outside BrnRendererModule and have no factory
// instance in scope -- BrnCoronaManager::Construct,
// BrnSunCorona::GenerateOcclusionBuffer, BrnSunCorona::RenderOccludedFlare,
// BrnPostFxBloom::PrepareDownSampleBuffer, BrnBlobbyShadowManager::Render,
// BrnPostFx::Construct and CgsGraphics::DrawRenderableMeshZOnly::Interpret. On the
// console that is invisible, because the table is a file-scope static and the
// accessor is inlined into each of them. How those seven reach the table on the PC
// build is NOT decided here.
//
// THE FULL READER CENSUS, counted rather than asserted (every function in the
// console image that names any of the table's nine words):
// TWELVE functions touch the table. SIX of them load at displacement 0, i.e. read
// slot 0 -- BeginRenderEnvironmentMapFace, EndRenderPostFx,
// BrnSunCorona::GenerateOcclusionBuffer,
// BrnPostFxBloom::PrepareDownSampleBuffer,
// BrnRendererModule::EndRenderAntiAliased and BrnRendererModule::Render (two loads
// in one function). The other six only FORM the base address and then subscript a
// different slot: BrnCoronaManager::Construct -> +0x08 = slot 2,
// BrnSunCorona::RenderOccludedFlare -> +0x14 = slot 5,
// BrnBlobbyShadowManager::Render -> +0x04 = slot 1,
// BrnRendererModule::BeginQuarterResBuffer -> +0x1C = slot 7,
// BrnPostFx::Construct -> +0x04 = slot 1, and
// DrawRenderableMeshZOnly::Interpret -> +0x1C = slot 7 and +0x20 = slot 8.
// Slots 3, 4 and 6 have NO reader anywhere in the image.
//
// -----------------------------------------------------------------------------
// TYPE RECONCILIATION. The original spells the table `BlendState *[9]`. In this
// tree renderengine::BlendState is a static HELPER class
// (SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h) and the
// runtime object every applier consumes is the 19-word
// renderengine::BlendMaterialState (same header; BlendState::Initialize takes
// `BlendMaterialState** ppMaterial`). The table is typed BlendMaterialState* here.
// That is not overriding the original declaration: in the other platform backend
// renderengine::BlendState IS the object type
// (SDKs/EATech/include/ps3/gcm/renderengine/states.h); BlendMaterialState is
// this tree's spelling of the same object.
//
// -----------------------------------------------------------------------------
// ODR NOTE -- RESOLVED (gate-flip wave, 2026-08-15). BrnRendererModule.h used to
// carry an empty placeholder `struct CgsBlendStateFactory {};` (a second
// declaration of this global-namespace name, so no TU could include both headers).
// It now #includes this header instead, embeds the real class by value, and
// BrnRendererModule::Render calls Construct once in its deferred PC bring-up;
// BrnPostFx.cpp and BrnPostFxBloom.cpp read slots through the static GetState.
// =============================================================================

// Which slot of saBlendStates each of the nine built-in states occupies.
//
// THE NAMES ARE GROUND TRUTH, not a reading of what the states do: Construct
// embeds one CgsDev::Assert::FireAssert string per slot, in exactly this order,
// each one paired with the store to that slot --
//
//   slot 0  "saBlendStates[ eFactoryBlendState_Opaque_Modulate_NoAlphaTest_DestRGBA ]"
//   slot 1  "saBlendStates[ eFactoryBlendState_Transparent_Modulate_NoAlphaTest_DestRGBA ]"
//   slot 2  "saBlendStates[ eFactoryBlendState_Transparent_Additive_NoAlphaTest_DestRGBA ]"
//   slot 3  "saBlendStates[ eFactoryBlendState_Transparent_Subtractive_NoAlphaTest_DestRGBA ]"
//   slot 4  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ]"
//   slot 5  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveRGB_NoAlphaTest_DestRGB ]"
//   slot 6  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ]"
//   slot 7  "saBlendStates[ eFactoryBlendState_NoColourWrite_NoAlphaTest ]"
//   slot 8  "saBlendStates[ eFactoryBlendState_NoColourWrite_AlphaTest ]"
//
// Every one of those asserts names this class's own file, which is also where this
// class's path attribution comes from.
//
// The spelling below is the project's E_ upper-snake convention
// (references/CXX_NAMING_CONVENTIONS.md), which wins over a recovered spelling --
// the same re-spelling the committed CgsDepthStencilStateFactory already applies
// to its five console names.
//
// TWO OF THE NAMES ARE CORROBORATED BY THE PARAMETER BLOCKS THEMSELVES, which is
// worth recording because it is a check on the whole decode: every slot whose name
// ends "NoAlphaTest" is built with mbState16 (AlphaTestEnable) = 0, and the one
// named "AlphaTest" -- slot 8 -- is the only slot that sets mbState16 = 1, and the
// only one that carries a non-default AlphaFunc (4) and AlphaRef (0x80). Likewise
// the two "NoColourWrite" slots (7 and 8) are exactly the two that set muState4
// (ColorWriteEnable) = 0 where the other seven set 15.
//
// FILE SCOPE, NOT NESTED: the original class outline lists the vptr, the static
// table and the five methods and NO nested type, and that outline does report
// nested enums where they exist (renderengine::BlendState / DepthStencilState /
// RasterizerState in SDKs/EATech/include/ps3/gcm/renderengine/states.h each carry
// theirs). Same
// placement the committed CgsDepthStencilStateFactory enum uses.
enum EFactoryBlendState
{
    E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA,                  // saBlendStates[0]
    E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA,             // saBlendStates[1]
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA,             // saBlendStates[2]
    E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA,          // saBlendStates[3]
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA,   // saBlendStates[4]
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB,          // saBlendStates[5]
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA, // saBlendStates[6]
    E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST,                            // saBlendStates[7]
    E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST,                               // saBlendStates[8]
    E_FACTORY_BLEND_STATE_COUNT                                                     // 9
};

class CgsBlendStateFactory
{
public:
    // Declared inline in the original header, and EMPTY -- which is the only
    // possibility rather than a convenience: the class carries no non-static data
    // member, so the sole generated work is the compiler's own vptr store. No ctor
    // symbol exists in the console image, consistent with inline + trivial.
    CgsBlendStateFactory() {}

    // Build the nine blend states into saBlendStates.
    // Body in CgsBlendStateFactory.cpp.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    // Both are declared in the original header. NEITHER HAS A CONSOLE BODY: a scan
    // of the whole image finds no Destruct and no Prepare symbol for any of the
    // three factories, and no cross-reference anywhere names one, so there is nothing to
    // reconstruct and nothing is invented here. They are DEFINED -- as flagged,
    // never-called link-closure fillers -- at the end of CgsBlendStateFactory.cpp, so
    // that this class's vtable resolves completely the moment anything constructs it.
    virtual void Destruct();
    virtual bool Prepare();

    // A member accessor, defined inline in the original header. STATICNESS INFERRED
    // (2026-08-15, matching the depth-stencil / rasterizer factories): the original
    // declaration elides `this`, no standalone symbol exists (header-inline),
    // and of the table's readers only BrnRendererModule owns an instance -- BrnPostFx
    // (B4Blur::Parameters::m_scatterBlendState == saBlendStates[1])
    // cannot even include BrnRendererModule.h. `static` is the strictly more permissive
    // spelling: an instance call still compiles against it.
    //
    // NO BOUNDS ASSERT: every attested reader loads its slot with a constant index
    // and no check, and the console image carries no assert string for this accessor,
    // so a CGS_ASSERT here would be behaviour the binary does not have.
    static renderengine::BlendMaterialState* GetState(u32 luIndex) { return saBlendStates[luIndex]; }

private:
    // Declared in the original header and defined in the .cpp. The nine consecutive
    // words are what Construct clears in one nine-iteration loop -- a count of nine
    // and a 4-byte stride -- and then fills one at a time.
    static renderengine::BlendMaterialState* saBlendStates[E_FACTORY_BLEND_STATE_COUNT];
};
