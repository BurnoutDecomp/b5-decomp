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
// to the DWARF shape is exactly the change CgsStateFactoryLinkStubs.cpp's
// "NOT COVERED HERE" note asks for, and it lands with the two link stubs that
// note requires (CgsRasterizerStateFactory::Destruct / ::Prepare).
//
// -----------------------------------------------------------------------------
// SHAPE. DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/
// Graphics/CgsRasterizerStateFactory.h) gives a single-vptr class, no base, and
// NO non-static data members -- member for member the shape of both committed
// siblings:
//     vptr
//     CgsRasterizerStateFactory()                                 (.h:56)
//     virtual void Construct(rw::IResourceAllocator*)             (.cpp:45)
//     virtual void Destruct()                                     (.cpp:90)
//     virtual bool Prepare()                                      (.cpp:105)
//     RasterizerState* GetState(uint32_t)                          (.h:77)
//   private:
//     static RasterizerState* saRasterizerStates[3]  (declared .h:52, defined .cpp:25)
//
// CONSTRUCT RETURNS void, and the ASM says so: the X360 body @0x827EBF30 ends
// `addi r1,r1,0x140 / b __restgprlr_28` with no return-value setup at all
// (0x827EC174-0x827EC178). Hex-Rays prints `_DWORD *` and a `result` variable
// only because the LAST thing the function happens to do is store the third
// Initialize's r3 into the table -- the same r3 is then live at the epilogue by
// accident. The committed .cpp's `renderengine::RasterizerState* Construct(void*)`
// inherited that artefact; the DWARF's `void Construct(rw::IResourceAllocator*)`
// is what the source said, and nothing in the tree consumes a return value:
//     $ grep -rn "RasterizerStateFactory" b5-decomp/src --include=*.cpp --include=*.h
//     b5-decomp/src/GameSource/Graphics/BrnRendererModule.h:144:struct CgsRasterizerStateFactory
//     b5-decomp/src/GameSource/Graphics/BrnRendererModule.h:378:  // (see the CgsBlendStateFactory / CgsRasterizerStateFactory / CgsDepthStencilStateFactory
//     b5-decomp/src/GameSource/Graphics/BrnRendererModule.h:554:    CgsRasterizerStateFactory           mRasterizerStateFactory;
//     b5-decomp/src/GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.cpp:6/82
//     b5-decomp/src/GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp:45..53 (the comment)
// (BrnRendererModule.h:144 WAS the empty PLACEHOLDER struct at the time of that grep; the
// gate-flip wave replaced the three placeholders with #includes of the real headers, and
// BrnRendererModule::Render now calls all three Constructs in its deferred PC bring-up.)
//
// CONSTRUCT IS A NON-STATIC MEMBER, from the asm and not from the DWARF: the
// prologue keeps the allocator in r4 (`mr r31, r4` @0x827EBF4C, then
// `lwz r11,0(r31)` / `lwz r11,0x10(r11)` / `bctrl` @0x827EBFC8..0x827EBFE4) and
// never reads r3. A PPC function whose first GPR slot is present but unused,
// with the real first argument in r4, is a member function with an unused `this`.
// Identical to the reading already recorded in CgsBlendStateFactory.h.
//
// CONSTRUCT IS VIRTUAL, per DWARF, and consistent with the export set: nothing
// in the 30,095-entry X360 export calls 0x827EBF30 -- no direct caller is what
// vtable dispatch looks like.
//
// -----------------------------------------------------------------------------
// GetState IS DECLARED static, AND THAT IS AN INFERENCE -- read this before
// trusting it.
//
// What is NOT in dispute: saRasterizerStates is a PRIVATE STATIC member (DWARF),
// the class has no non-static data member at all, and there is no GetState symbol
// anywhere in the X360 export set for any of the three factories (it is defined in
// the header, so it inlined into every reader and left no symbol to read a
// convention off). dwarfdump renders a static and a non-static member function
// identically -- it elides the implicit `this` from every member it prints -- so
// the DWARF cannot decide this either way, and the committed CgsBlendStateFactory.h
// records the same attribute as "STATICNESS UNDETERMINED".
//
// Why static is chosen here rather than the siblings' non-static:
//   1. REACHABILITY, counted rather than asserted. Every function in the
//      30,095-entry X360 export whose assembly names dword_83010A40 (== slot 2):
//          0x827EBF30  CgsRasterizerStateFactory::Construct      the sole WRITER
//          0x823F65B0  BrnRendererModule::EndRenderPostFx        reader
//          0x82400CF8  BrnSunCorona::GenerateOcclusionBuffer     reader
//          0x824010A8  BrnSunCorona::RenderOccludedFlare         reader
//          0x82402B40  BrnPostFxBloom::Render                    reader
//          0x824071B0  BrnBlobbyShadowManager::Render            reader
//          0x82408B00  BrnRendererModule::EndRenderAntiAliased   reader
//          0x82408C38  BrnRendererModule::BeginQuarterResBuffer  reader
//          0x8240A468  BrnPostFx::Render                         reader
//      FIVE of those eight readers are not BrnRendererModule and have no factory
//      instance in scope at all. BrnPostFx::Render @0x8240A468 reads
//      saRasterizerStates[2] (asm 0x8240A514-0x8240A520) and saDepthStencilStates[1]
//      (0x8240A504-0x8240A510) with no factory anywhere near it -- BrnPostFx cannot
//      even include BrnRendererModule.h (see BrnPostFxPCComposite.h). On the console
//      that is invisible because the table is a static and the accessor inlined. On
//      the host a NON-static accessor is simply unreachable from those call sites, so
//      the non-static choice is what blocks them, and it is not attested. (Same shape
//      as the blend twin's twelve-reader census, CgsBlendStateFactory.h:68-83.)
//   2. IT COSTS NOTHING TO BE WRONG. `static` is strictly the more permissive
//      spelling: an instance-based call site written later, `lFactory.GetState(i)`,
//      still compiles against a static member. The reverse is not true. So if a
//      future dump shows an instance-based convention, nothing here has to change.
// This is an INFERRED shape decision, disclosed, not an attestation.
//
// NO BOUNDS ASSERT: every attested reader loads its slot with a constant index and
// no check, and the export set carries no assert string for this accessor, so a
// CGS_ASSERT here would be behaviour the binary does not have. (Same reading as
// CgsBlendStateFactory.h:189-191.)
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
// @0x827EBF30 embeds one CgsDev::Assert::FireAssert string per slot, in exactly
// this order, each one paired with the store to that slot's dword --
//
//   slot 0  0x83010A38  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeBack ]"   (.cpp:63)
//   slot 1  0x83010A3C  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeFront ]"  (.cpp:70)
//   slot 2  0x83010A40  "saRasterizerStates[ eFactoryRasterizerState_Scissor_CullModeNone ]"   (.cpp:77)
//
// (the .cpp line numbers are the third argument FireAssert is handed: li r5,0x3F /
// 0x46 / 0x4D at 0x827EC034 / 0x827EC0CC / 0x827EC160. The file string all three
// share is "..\..\..\GameShared\GameClasses\Graphics/CgsRasterizerStateFactory.cpp",
// which is also where this class's path attribution comes from.) The spelling below
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
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK,   // saRasterizerStates[0] == X360 dword_83010A38
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT,  // saRasterizerStates[1] == X360 dword_83010A3C
    E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE,   // saRasterizerStates[2] == X360 dword_83010A40
    E_FACTORY_RASTERIZER_STATE_COUNT                     // 3
};

class CgsRasterizerStateFactory
{
public:
    // CgsRasterizerStateFactory.h:56 (DWARF) -- defined in the header, so inline and
    // empty: the class carries no non-static data member, so the sole generated work
    // is the compiler's own vptr store. No ctor symbol exists in the X360 export set,
    // consistent with inline + trivial. (Same reading as CgsBlendStateFactory.h:169.)
    CgsRasterizerStateFactory() {}

    // @ 0x827EBF30 -- build the three rasteriser states into saRasterizerStates.
    // Body in CgsRasterizerStateFactory.cpp.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    // CgsRasterizerStateFactory.cpp:90 / :105 (DWARF). NEITHER HAS AN X360 BODY: a
    // scan of all 30,095 exports finds no Destruct and no Prepare symbol for any of
    // the three factories, and no xrefs_to anywhere names one, so there is nothing to
    // reconstruct and nothing is invented. They are DEFINED -- as documented,
    // never-called link-closure stubs -- in
    // GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp, alongside the
    // blend and depth/stencil twins', so that this class's vtable resolves completely
    // the moment anything constructs it.
    virtual void Destruct();
    virtual bool Prepare();

    // CgsRasterizerStateFactory.h:77 (DWARF): an accessor defined inline in the
    // header. `static` is an INFERENCE -- see the banner above.
    static renderengine::RasterizerState* GetState(u32 luIndex) { return saRasterizerStates[luIndex]; }

private:
    // Declared CgsRasterizerStateFactory.h:52, defined CgsRasterizerStateFactory.cpp:25
    // (DWARF). X360 saRasterizerStates[0..2] == 0x83010A38 / 0x83010A3C / 0x83010A40,
    // the three consecutive dwords Construct clears at 0x827EBFA4 / 0x827EBFAC /
    // 0x827EBFB0 and then fills one at a time. Was the TU-local `gapRasterizerStates`
    // in the committed .cpp; the DWARF's own name and the shipped assert strings both
    // spell it saRasterizerStates.
    static renderengine::RasterizerState* saRasterizerStates[E_FACTORY_RASTERIZER_STATE_COUNT];
};
