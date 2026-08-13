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
// SOURCE OF TRUTH for everything below: CgsBlendStateFactory::Construct
// @ 0x827EB2D8 .. 0x827EBBA0 (2248 bytes, 562 instructions), dumped from
// BURNOUT_X360_ARTIST.XEX.i64 with headless idat. That function is a NAMED X360
// symbol that the per-function JSON export skips entirely (the export directory
// jumps from 0x827EB1F8.json straight to 0x827EBBA0.json), which is why an
// earlier pass could only name it from its callees' xrefs_to.
//
// -----------------------------------------------------------------------------
// SHAPE. DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/
// Graphics/CgsBlendStateFactory.h) gives a single-vptr class, no base, and NO
// non-static data members:
//     vptr
//     CgsBlendStateFactory()                                      (.h:61)
//     virtual void Construct(rw::IResourceAllocator*)             (.cpp:44)
//     virtual void Destruct()                                     (.cpp:235)
//     virtual bool Prepare()                                      (.cpp:250)
//     BlendState* GetState(uint32_t)                               (.h:82)
//   private:
//     static BlendState* saBlendStates[9]        (declared .h:57, defined .cpp:25)
//
// CONSTRUCT IS A NON-STATIC MEMBER, and the ASSEMBLY says so, not the DWARF: the
// dumped prologue keeps the allocator in r4 (0x827EB2F0 `mr r29, r4`, then
// `lwz r11, 0(r29)` / `lwz r11, 0x10(r11)` / `bctrl` at 0x827EB3B4..0x827EB3D0)
// and NEVER READS r3. A PPC function whose first GPR slot is present but unused,
// with the real first argument in r4, is a member function with an unused `this`.
// (dwarfdump elides the implicit `this` from every member it prints, so it cannot
// be used as evidence of staticness either way.)
//
// CONSTRUCT IS VIRTUAL. No function in the 30,095-entry X360 export set calls
// 0x827EB2D8 -- the only references to that address anywhere in the export are the
// six callees listing it in their own xrefs_to (renderengine::BlendState::
// Initialize @0x82B627C8, ::GetResourceDescriptor @0x82B636B8, CgsDev::Assert::
// BeginAssert/FireAssert/EndAssert, __savegprlr_22). The same holds for both
// sibling factories (0x827EBBA0, 0x827EBF30). No direct caller is what vtable
// dispatch looks like, and it is what the DWARF `virtual` says.
//
// GetState: STATICNESS UNDETERMINED, and recorded as such rather than settled.
// The DWARF places it at .h:82 -- defined inline in the header -- and a scan of
// all 30,095 X360 exports finds no GetState symbol for any of the three
// factories, so it is folded into its readers and there is NO CALL SITE anywhere
// to read the convention off. It is declared here as a normal member, matching
// the committed sibling CgsDepthStencilStateFactory.h, and that choice is a
// convention match, NOT an attestation. OPEN QUESTION, for whoever writes the
// readers: SEVEN of the TWELVE X360 readers of this table are outside
// BrnRendererModule and have no factory instance in scope --
// BrnCoronaManager::Construct @0x823FCD90, BrnSunCorona::GenerateOcclusionBuffer
// @0x82400CF8, BrnSunCorona::RenderOccludedFlare @0x824010A8,
// BrnPostFxBloom::PrepareDownSampleBuffer @0x82401AE8,
// BrnBlobbyShadowManager::Render @0x824071B0, BrnPostFx::Construct @0x82409F80,
// CgsGraphics::DrawRenderableMeshZOnly::Interpret @0x827F5AC8. On the console
// that is invisible, because the table is a file-scope static and the accessor is
// inlined into each of them. How those seven reach the table on the PC build is
// NOT decided here.
//
// THE FULL READER CENSUS, counted rather than asserted (every function in the
// 30,095-entry export whose assembly names any of dword_83010F70..0x83010F90):
// TWELVE functions touch the table. SIX of them load displacement 0, i.e. read
// slot 0 -- BeginRenderEnvironmentMapFace (lwz @0x823F6578), EndRenderPostFx
// (@0x823F6658), BrnSunCorona::GenerateOcclusionBuffer (@0x82400DC8),
// BrnPostFxBloom::PrepareDownSampleBuffer (@0x82401B30),
// BrnRendererModule::EndRenderAntiAliased (@0x82408BCC) and
// BrnRendererModule::Render (@0x8240DF40 and @0x8240E0E0, two loads in one
// function). The other six only FORM the base with lis/addi and then subscript a
// different slot: BrnCoronaManager::Construct -> +0x08 = slot 2 (@0x823FCDD8),
// BrnSunCorona::RenderOccludedFlare -> +0x14 = slot 5 (@0x8240116C),
// BrnBlobbyShadowManager::Render -> +0x04 = slot 1 (@0x824073AC),
// BrnRendererModule::BeginQuarterResBuffer -> +0x1C = slot 7 (@0x82408D9C),
// BrnPostFx::Construct -> +0x04 = slot 1 (@0x8240A368), and
// DrawRenderableMeshZOnly::Interpret -> +0x1C = slot 7 (@0x827F5E74) and +0x20 =
// slot 8 (@0x827F5E7C). Slots 3, 4 and 6 have NO reader anywhere in the export.
//
// -----------------------------------------------------------------------------
// TYPE RECONCILIATION. DWARF spells the table `BlendState *[9]`. In this tree
// renderengine::BlendState is a static HELPER class
// (SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h:95) and the
// runtime object every applier consumes is the 19-word
// renderengine::BlendMaterialState (blendstate.h:35; BlendState::Initialize takes
// `BlendMaterialState** ppMaterial`). The table is typed BlendMaterialState* here.
// That is not overriding DWARF: on PS3 renderengine::BlendState IS the object type
// (SDKs/EATech/include/ps3/gcm/renderengine/states.h:94); BlendMaterialState is
// this tree's spelling of the same object.
//
// -----------------------------------------------------------------------------
// ODR NOTE, disclosed rather than hidden. GameSource/Graphics/BrnRendererModule.h
// lines 132-134 still carry the empty placeholder `struct CgsBlendStateFactory {};`
// and line 473 still embeds it by value. That placeholder is a second declaration
// of the same global-namespace name, so no TU may include both headers -- and none
// does today (the only includer of this header is CgsBlendStateFactory.cpp). This
// is the identical situation the two committed sibling factories are already in.
// The swap that deletes the placeholder is DEFERRED, deliberately: see the LINK
// section of CgsBlendStateFactory.cpp.
// =============================================================================

// Which slot of saBlendStates each of the nine built-in states occupies.
//
// THE NAMES ARE GROUND TRUTH, not a reading of what the states do: Construct
// @0x827EB2D8 embeds one CgsDev::Assert::FireAssert string per slot, in exactly
// this order, each one paired with the store to that slot's dword --
//
//   slot 0  0x83010F70  "saBlendStates[ eFactoryBlendState_Opaque_Modulate_NoAlphaTest_DestRGBA ]"                (.cpp:71)
//   slot 1  0x83010F74  "saBlendStates[ eFactoryBlendState_Transparent_Modulate_NoAlphaTest_DestRGBA ]"           (.cpp:89)
//   slot 2  0x83010F78  "saBlendStates[ eFactoryBlendState_Transparent_Additive_NoAlphaTest_DestRGBA ]"           (.cpp:107)
//   slot 3  0x83010F7C  "saBlendStates[ eFactoryBlendState_Transparent_Subtractive_NoAlphaTest_DestRGBA ]"        (.cpp:125)
//   slot 4  0x83010F80  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveAlphaOne_NoAlphaTest_DestRGBA ]"   (.cpp:143)
//   slot 5  0x83010F84  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveRGB_NoAlphaTest_DestRGB ]"         (.cpp:165)
//   slot 6  0x83010F88  "saBlendStates[ eFactoryBlendState_Transparent_AdditiveInvDestColor_NoAlphaTest_DestRGBA ]" (.cpp:183)
//   slot 7  0x83010F8C  "saBlendStates[ eFactoryBlendState_NoColourWrite_NoAlphaTest ]"                           (.cpp:201)
//   slot 8  0x83010F90  "saBlendStates[ eFactoryBlendState_NoColourWrite_AlphaTest ]"                             (.cpp:221)
//
// (the .cpp line numbers are the third argument FireAssert is handed:
// li r5,0x47 / 0x59 / 0x6B / 0x7D / 0x8F / 0xA5 / 0xB7 / 0xC9 / 0xDD, at
// 0x827EB420 / 0x827EB508 / 0x827EB5F0 / 0x827EB6D8 / 0x827EB7C0 / 0x827EB8CC /
// 0x827EB9B4 / 0x827EBA98 / 0x827EBB84). The file string every one of them shares
// is "..\..\..\GameShared\GameClasses\Graphics/CgsBlendStateFactory.cpp", which is
// also where this class's own path attribution comes from.
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
// FILE SCOPE, NOT NESTED: the DWARF class outline lists the vptr, the static table
// and the five methods and NO nested type, and that dump does report nested enums
// where they exist (renderengine::BlendState / DepthStencilState / RasterizerState
// in SDKs/EATech/include/ps3/gcm/renderengine/states.h each carry theirs). Same
// placement the committed CgsDepthStencilStateFactory enum uses.
enum EFactoryBlendState
{
    E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA,                  // saBlendStates[0] == X360 dword_83010F70
    E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA,             // saBlendStates[1] == X360 dword_83010F74
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA,             // saBlendStates[2] == X360 dword_83010F78
    E_FACTORY_BLEND_STATE_TRANSPARENT_SUBTRACTIVE_NO_ALPHA_TEST_DEST_RGBA,          // saBlendStates[3] == X360 dword_83010F7C
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_ALPHA_ONE_NO_ALPHA_TEST_DEST_RGBA,   // saBlendStates[4] == X360 dword_83010F80
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB,          // saBlendStates[5] == X360 dword_83010F84
    E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_INV_DEST_COLOR_NO_ALPHA_TEST_DEST_RGBA, // saBlendStates[6] == X360 dword_83010F88
    E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST,                            // saBlendStates[7] == X360 dword_83010F8C
    E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_ALPHA_TEST,                               // saBlendStates[8] == X360 dword_83010F90
    E_FACTORY_BLEND_STATE_COUNT                                                     // 9
};

class CgsBlendStateFactory
{
public:
    // CgsBlendStateFactory.h:61 (DWARF). Inline and EMPTY, which is the only
    // possibility rather than a convenience: the class carries no non-static data
    // member, so the sole generated work is the compiler's own vptr store. No ctor
    // symbol exists in the X360 export set, consistent with inline + trivial.
    CgsBlendStateFactory() {}

    // @ 0x827EB2D8 -- build the nine blend states into saBlendStates.
    // Body in CgsBlendStateFactory.cpp.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    // CgsBlendStateFactory.cpp:235 / :250 (DWARF). NEITHER HAS AN X360 BODY: a scan
    // of all 30,095 exports finds no Destruct and no Prepare symbol for any of the
    // three factories, and no xrefs_to anywhere names one, so there is nothing to
    // reconstruct and nothing is invented here. They are DEFINED -- as documented,
    // never-called link-closure stubs -- in
    // GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp, so that this
    // class's vtable resolves completely the moment anything constructs it.
    virtual void Destruct();
    virtual bool Prepare();

    // CgsBlendStateFactory.h:82 (DWARF): a member accessor, defined inline in the
    // header. STATICNESS UNDETERMINED -- see the banner above; this is the
    // committed siblings' shape, not an attested convention.
    //
    // NO BOUNDS ASSERT: every attested reader loads its slot with a constant index
    // and no check, and the export set carries no assert string for this accessor,
    // so a CGS_ASSERT here would be behaviour the binary does not have.
    renderengine::BlendMaterialState* GetState(u32 luIndex) { return saBlendStates[luIndex]; }

private:
    // Declared CgsBlendStateFactory.h:57, defined CgsBlendStateFactory.cpp:25 (DWARF).
    // X360 saBlendStates[0..8] == 0x83010F70 .. 0x83010F90, the nine consecutive
    // dwords Construct clears in one nine-iteration loop (li r9,9 @0x827EB308,
    // stw/addi 4 @0x827EB320..0x827EB328) and then fills one at a time.
    static renderengine::BlendMaterialState* saBlendStates[E_FACTORY_BLEND_STATE_COUNT];
};
