#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::RasterizerState
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator

// =============================================================================
// CgsRasterizerStateFactory.h  (GameShared/GameClasses/Graphics)
//
// CgsRasterizerStateFactory -- builds the fixed set of THREE rasteriser states
// the renderer, the post-fx chain and the dispatch interpreter select between,
// and owns them in the private static table saRasterizerStates.
//
// THIS FILE IS NEW. Until now this class had NO HEADER AT ALL: it was declared
// TU-LOCALLY, non-polymorphically and with the wrong signature inside its own
// .cpp --
//     class CgsRasterizerStateFactory
//     { public: renderengine::RasterizerState* Construct(void* pResourceAllocator); };
// -- which is why nothing outside that one .cpp could name a rasteriser state,
// and why BrnPostFx.cpp had to carry an invented `gpPostFxRasterizerState`
// extern instead of naming saRasterizerStates[2]. Reconciling that declaration
// to that documented shape lands together with this class's two link-closure fillers
// (CgsRasterizerStateFactory::Destruct / ::Prepare).
//
// -----------------------------------------------------------------------------
// SHAPE. The original header gives a single-vptr class, no base, and
// NO non-static data members -- member for member the shape of both committed
// siblings:
//     vptr
//     CgsRasterizerStateFactory()
//     virtual void Construct(rw::IResourceAllocator*)
//     virtual void Destruct()
//     virtual bool Prepare()
//     RasterizerState* GetState(uint32_t)
//   private:
//     static RasterizerState* saRasterizerStates[3]
//
// CONSTRUCT RETURNS void, and the attested epilogue says so: the console body tears
// down its frame and returns with no return-value setup at all. A decompiler prints
// `_DWORD *` and a `result` variable only because the LAST thing the function happens
// to do is store the third Initialize's result into the table -- that value is then
// still live at the epilogue by accident. The committed .cpp's
// `renderengine::RasterizerState* Construct(void*)` inherited that artefact; the
// original's `void Construct(rw::IResourceAllocator*)` is what the source said, and
// nothing in the tree consumes a return value -- the only three references to this
// class outside its own .cpp are BrnRendererModule.h's include, its comment naming the
// three factories, and its `mRasterizerStateFactory` member.
// (BrnRendererModule.h carried an empty PLACEHOLDER struct until the gate-flip wave
// replaced the three placeholders with #includes of the real headers; BrnRendererModule::
// Render now calls all three Constructs in its deferred PC bring-up.)
//
// CONSTRUCT IS A NON-STATIC MEMBER, from the attested calling convention and not from
// the declaration: the prologue keeps the allocator in the SECOND argument slot,
// dispatches the allocator call through it, and never reads the first slot. A function
// whose first argument slot is present but unused, with the real first argument one slot
// along, is a member function with an unused `this`.
// Identical to the reading already recorded in CgsBlendStateFactory.h.
//
// CONSTRUCT IS VIRTUAL, per the original declaration, and consistent with the image:
// nothing in it calls Construct directly -- no direct caller is what vtable dispatch
// looks like.
//
// -----------------------------------------------------------------------------
// GetState IS DECLARED static, AND THAT IS AN INFERENCE -- read this before
// trusting it.
//
// What is NOT in dispute: saRasterizerStates is a PRIVATE STATIC member, the class has
// no non-static data member at all, and there is no GetState symbol anywhere in the
// console image for any of the three factories (it is defined in the header, so it
// inlined into every reader and left no symbol to read a convention off). The original
// declarations render a static and a non-static member function identically -- they
// elide the implicit `this` from every member they print -- so they cannot decide this
// either way, and the committed CgsBlendStateFactory.h records the same attribute as
// "STATICNESS UNDETERMINED".
//
// Why static is chosen here rather than the siblings' non-static:
//   1. REACHABILITY, counted rather than asserted. Every function in the console image
//      that names slot 2:
//          CgsRasterizerStateFactory::Construct      the sole WRITER
//          BrnRendererModule::EndRenderPostFx        reader
//          BrnSunCorona::GenerateOcclusionBuffer     reader
//          BrnSunCorona::RenderOccludedFlare         reader
//          BrnPostFxBloom::Render                    reader
//          BrnBlobbyShadowManager::Render            reader
//          BrnRendererModule::EndRenderAntiAliased   reader
//          BrnRendererModule::BeginQuarterResBuffer  reader
//          BrnPostFx::Render                         reader
//      FIVE of those eight readers are not BrnRendererModule and have no factory
//      instance in scope at all. BrnPostFx::Render reads saRasterizerStates[2] and
//      saDepthStencilStates[1] with no factory anywhere near it -- BrnPostFx cannot
//      even include BrnRendererModule.h. On the console
//      that is invisible because the table is a static and the accessor inlined. On
//      the host a NON-static accessor is simply unreachable from those call sites, so
//      the non-static choice is what blocks them, and it is not attested. (Same shape
//      as the blend twin's twelve-reader census in CgsBlendStateFactory.h.)
//   2. IT COSTS NOTHING TO BE WRONG. `static` is strictly the more permissive
//      spelling: an instance-based call site written later, `lFactory.GetState(i)`,
//      still compiles against a static member. The reverse is not true. So if a
//      future dump shows an instance-based convention, nothing here has to change.
// This is an INFERRED shape decision, disclosed, not an attestation.
//
// NO BOUNDS ASSERT: every attested reader loads its slot with a constant index and
// no check, and the console image carries no assert string for this accessor, so a
// CGS_ASSERT here would be behaviour the binary does not have. (Same reading as
// CgsBlendStateFactory.h.)
//
// -----------------------------------------------------------------------------
// ODR NOTE -- RESOLVED (gate-flip wave, 2026-08-15). BrnRendererModule.h used to
// carry an empty placeholder `struct CgsRasterizerStateFactory {};` (a second
// declaration of this global-namespace name, so no TU could include both headers).
// It now #includes this header instead, embeds the real class by value, and
// BrnRendererModule::Render calls Construct once in its deferred PC bring-up.
// =============================================================================

// Which slot of saRasterizerStates each of the three built-in states occupies.
//
// THE NAMES ARE GROUND TRUTH, not a reading of what the states do: Construct
// embeds one CgsDev::Assert::FireAssert string per slot, in exactly this order, each
// one paired with the store to that slot --
//
//   slot 0  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]"
//   slot 1  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]"
//   slot 2  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeNone ]"
//
// (all three asserts name this class's own file, which is also where this class's path
// attribution comes from.) The spelling below
// is the project's E_ upper-snake convention (references/CXX_NAMING_CONVENTIONS.md),
// the same re-spelling both committed siblings apply -- and the same three
// enumerators the committed .cpp already carried TU-locally, moved here unchanged so
// that a caller can name a slot.
//
// THE NAMES ARE CORROBORATED BY THE PARAMETER BLOCKS: slot 0 is built with
// muCullMode = 2, slot 1 with `(muCullMode & 4) | 1` == 1, slot 2 with
// `muCullMode &= 4` == 0 -- back / front / none -- and mu8ScissorEnable is 1 for all
// three, which is what "Scissor_" in every name says.
enum EFactoryRasterizerState
{
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK,   // saRasterizerStates[0]
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT,  // saRasterizerStates[1]
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE,   // saRasterizerStates[2]
    E_FACTORY_RASTERIZER_STATE_COUNT                     // 3
};

class CgsRasterizerStateFactory
{
public:
    // Defined in the original header, so inline and empty: the class carries no
    // non-static data member, so the sole generated work is the compiler's own vptr
    // store. No ctor symbol exists in the console image, consistent with inline +
    // trivial. (Same reading as CgsBlendStateFactory.h.)
    CgsRasterizerStateFactory() {}

    // Build the three rasteriser states into saRasterizerStates.
    // Body in CgsRasterizerStateFactory.cpp.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    // Both are declared in the original header. NEITHER HAS A CONSOLE BODY: a
    // scan of the whole image finds no Destruct and no Prepare symbol for any of
    // the three factories, and no cross-reference anywhere names one, so there is nothing to
    // reconstruct and nothing is invented. They are DEFINED -- as flagged,
    // never-called link-closure fillers -- at the end of
    // CgsRasterizerStateFactory.cpp, as for the blend and depth/stencil twins, so that
    // this class's vtable resolves completely the moment anything constructs it.
    virtual void Destruct();
    virtual bool Prepare();

    // An accessor defined inline in the original header. `static` is an INFERENCE --
    // see the banner above.
    static renderengine::RasterizerState* GetState(u32 luIndex) { return saRasterizerStates[luIndex]; }

private:
    // Declared in the original header and defined in the .cpp. saRasterizerStates[0..2]
    // are three consecutive words that Construct clears one after another and then fills
    // one at a time. Was the TU-local `gapRasterizerStates` in the committed .cpp; the
    // original's own name and the shipped assert strings both spell it
    // saRasterizerStates.
    static renderengine::RasterizerState* saRasterizerStates[E_FACTORY_RASTERIZER_STATE_COUNT];
};
