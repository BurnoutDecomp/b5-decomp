#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::DepthStencilState
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator

// =============================================================================
// CgsDepthStencilStateFactory.h  (GameShared/GameClasses/Graphics)
//
// CgsDepthStencilStateFactory -- builds the fixed set of depth/stencil render
// states the immediate-mode renderer selects between (X360 saDepthStencilStates,
// 5 slots), and owns them in the private static table saDepthStencilStates.
// Construct sizes + carves each state through the supplied resource allocator and
// initialises it via renderengine::DepthStencilState::Initialize, CGS_ASSERT-ing
// every slot came back non-null.
//
// Declaration shape (virtual-ness, return types, vtable order) is DWARF-attested
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Graphics/
// CgsDepthStencilStateFactory.h): a single-vptr polymorphic class, no base, no
// non-static data member, and the table as a PRIVATE STATIC member declared at
// .h:53 and defined at .cpp:25. Destruct / Prepare / GetState are NOT in the X360
// ledger for this TU -- only Construct is attested (0x827EBBA0). They are declared
// here to preserve the DWARF vtable order, and they are three different
// situations, which is worth spelling out:
//
//   * Destruct / Prepare have NO X360 body and never will from the binary: a scan of
//     all 30,095 exports finds no symbol for either, on this class or on the blend and
//     rasterizer twins, and no callee's xrefs_to names one. They are DEFINED -- as
//     documented, never-called, link-closure stubs -- in
//     GameShared/GameClasses/Graphics/CgsStateFactoryLinkStubs.cpp. That file is not
//     decoration: this class is polymorphic, so its vtable is emitted in whatever TU
//     constructs it and names every virtual, and the day BrnRendererModule stops using
//     its empty placeholder and embeds the real class by value (it is reached from
//     `static BrnGame::BrnGameModule gGameModule;`, BrnMain.cpp:45) an undefined virtual
//     is an LNK2019 in the boot link. The per-TU gate is `cl /c`, which cannot see that.
//   * GetState IS NOW BODIED, and the table it reads is now the DWARF's private static
//     member rather than a TU-local array in the .cpp. That is this wave's change, and
//     it exists for one reason: BrnPostFx::Render @0x8240A468 pushes
//     saDepthStencilStates[1] (asm 0x8240A504-0x8240A510) into
//     shadow::Device::SetState, and with the table TU-local it could not name the slot
//     -- so the post-fx driver carried an INVENTED `gpPostFxDepthStencilState` extern
//     with no definition anywhere. There is no such global on the console (0x83010910
//     is simply saDepthStencilStates[0] + 4); publishing the table the way the DWARF
//     already declares it is what retires the invention, rather than minting a parallel
//     host global for state the tree already owns.
//
//     GetState IS DECLARED static, AND THAT IS AN INFERENCE. dwarfdump renders a static
//     and a non-static member function identically (it elides the implicit `this` from
//     every member it prints), there is no GetState symbol anywhere in the X360 export
//     set for any of the three factories (it is header-defined, so it inlined into every
//     reader), and the committed CgsBlendStateFactory.h records the same attribute as
//     "STATICNESS UNDETERMINED". Static is chosen because (1) THE READERS PROVE IT, and the
//     census is COUNTED rather than asserted -- every function in the 30,095-entry X360
//     export whose assembly names dword_83010910 (== slot 1) is:
//         0x827EBBA0  CgsDepthStencilStateFactory::Construct       the sole WRITER
//         0x823F65B0  BrnRendererModule::EndRenderPostFx           reader
//         0x823FCD90  BrnCoronaManager::Construct                  reader
//         0x82400CF8  BrnSunCorona::GenerateOcclusionBuffer        reader
//         0x824010A8  BrnSunCorona::RenderOccludedFlare            reader
//         0x82402B40  BrnPostFxBloom::Render                       reader
//         0x82408B00  BrnRendererModule::EndRenderAntiAliased      reader
//         0x8240A468  BrnPostFx::Render                            reader
//         0x8240BFA8  BrnRendererModule::Render                    reader
//     FIVE of those eight readers are not BrnRendererModule and have no factory instance
//     anywhere in scope; BrnPostFx cannot even include BrnRendererModule.h
//     (BrnPostFxPCComposite.h says why). A non-static accessor is unreachable from the
//     very call sites this table exists to serve. (Same shape as the blend twin's
//     twelve-reader census, CgsBlendStateFactory.h:68-83.) And (2) it cannot be
//     expensively wrong: `static` is the more permissive spelling -- an instance-based
//     call written later, `lFactory.GetState(i)`, still compiles against it.
//
//     FOLLOW-UP: CgsBlendStateFactory::GetState should take the same treatment -- it is
//     what B4Blur::Parameters::m_scatterBlendState == saBlendStates[1] is still waiting
//     on (BrnPostFx.cpp:341-350). Left alone here because it is another wave's file.
// =============================================================================

// Which slot of saDepthStencilStates each of the five built-in states occupies
// (assert-message strings the X360 binary embeds at each call site -- ground truth
// for the names; no attested enum exists elsewhere in the ledger for this set).
//
// MOVED TO THE HEADER THIS WAVE, unchanged, from the anonymous namespace in
// CgsDepthStencilStateFactory.cpp: a caller of GetState has to be able to name the
// slot it wants, and BrnPostFx::Render wants
// E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF. Same file-scope placement as the
// committed CgsBlendStateFactory.h enum, and the same E_ upper-snake re-spelling of
// the console names (references/CXX_NAMING_CONVENTIONS.md).
enum EFactoryDepthStencilState
{
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON,     // saDepthStencilStates[0] == X360 dword_8301090C
    E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF,   // saDepthStencilStates[1] == X360 dword_83010910
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF,    // saDepthStencilStates[2] == X360 dword_83010914
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON,     // saDepthStencilStates[3] == X360 dword_83010918
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON,    // saDepthStencilStates[4] == X360 dword_8301091C
    E_FACTORY_DEPTH_STENCIL_STATE_COUNT                  // 5
};

class CgsDepthStencilStateFactory
{
public:
    CgsDepthStencilStateFactory();   // CgsDepthStencilStateFactory.h:57 (DWARF; not X360-attested here)

    // @ 0x827EBBA0 -- build the 5 depth/stencil states into saDepthStencilStates.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    virtual void Destruct();   // CgsDepthStencilStateFactory.cpp:116 (DWARF; declared-only here)
    virtual bool Prepare();    // CgsDepthStencilStateFactory.cpp:131 (DWARF; declared-only here)

    // CgsDepthStencilStateFactory.h:78 (DWARF): an accessor defined inline in the
    // header. `static` is an INFERENCE -- see the banner above.
    //
    // NO BOUNDS ASSERT: every attested reader loads its slot with a constant index and
    // no check, and the export set carries no assert string for this accessor, so a
    // CGS_ASSERT here would be behaviour the binary does not have.
    static renderengine::DepthStencilState* GetState(u32 luIndex) { return saDepthStencilStates[luIndex]; }

private:
    // Declared CgsDepthStencilStateFactory.h:53, defined CgsDepthStencilStateFactory.cpp:25
    // (DWARF). X360 saDepthStencilStates[0..4] == 0x8301090C .. 0x8301091C, the five
    // consecutive dwords Construct clears and then fills one at a time. Was the TU-local
    // array in the .cpp; promoted here so its readers can name a slot.
    static renderengine::DepthStencilState* saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_COUNT];
};
