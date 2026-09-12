#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::DepthStencilState
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator

// =============================================================================
// CgsDepthStencilStateFactory.h  (GameShared/GameClasses/Graphics)
//
// CgsDepthStencilStateFactory -- builds the fixed set of depth/stencil render
// states the immediate-mode renderer selects between (five slots), and owns them
// in the private static table saDepthStencilStates.
// Construct sizes + carves each state through the supplied resource allocator and
// initialises it via renderengine::DepthStencilState::Initialize, CGS_ASSERT-ing
// every slot came back non-null.
//
// Declaration shape (virtual-ness, return types, vtable order) comes from the
// original header: a single-vptr polymorphic class, no base, no
// non-static data member, and the table as a PRIVATE STATIC member declared in the
// header and defined in the .cpp. Destruct / Prepare / GetState are NOT in the
// ledger for this TU -- only Construct is attested. They are declared
// here to preserve the original vtable order, and they are three different
// situations, which is worth spelling out:
//
//   * Destruct / Prepare have NO console body and never will from the binary: a scan of
//     the whole image finds no symbol for either, on this class or on the blend and
//     rasterizer twins, and no callee's cross-references name one. They are DEFINED -- as
//     flagged, never-called, link-closure fillers -- at the end of
//     CgsDepthStencilStateFactory.cpp. Those fillers are not decoration:
//     this class is polymorphic, so its vtable is emitted in whatever TU
//     constructs it and names every virtual, and the day BrnRendererModule stops using
//     its empty placeholder and embeds the real class by value (it is reached from
//     `static BrnGame::BrnGameModule gGameModule;` in BrnMain.cpp) an undefined virtual
//     is an LNK2019 in the boot link. The per-TU gate is `cl /c`, which cannot see that.
//   * GetState IS NOW BODIED, and the table it reads is now the original's private static
//     member rather than a TU-local array in the .cpp. That is this wave's change, and
//     it exists for one reason: BrnPostFx::Render pushes saDepthStencilStates[1] into
//     shadow::Device::SetState, and with the table TU-local it could not name the slot
//     -- so the post-fx driver carried an INVENTED `gpPostFxDepthStencilState` extern
//     with no definition anywhere. There is no such global on the console (what looked
//     like one is simply saDepthStencilStates[0] + 4); publishing the table the way the
//     original already declares it is what retires the invention, rather than minting a
//     parallel host global for state the tree already owns.
//
//     GetState IS DECLARED static, AND THAT IS AN INFERENCE. The original declarations
//     render a static and a non-static member function identically (they elide the
//     implicit `this` from every member they print), there is no GetState symbol anywhere
//     in the console image for any of the three factories (it is header-defined, so it
//     inlined into every reader), and the committed CgsBlendStateFactory.h records the
//     same attribute as "STATICNESS UNDETERMINED". Static is chosen because (1) THE
//     READERS PROVE IT, and the census is COUNTED rather than asserted -- every function
//     in the console image that names slot 1 is:
//         CgsDepthStencilStateFactory::Construct       the sole WRITER
//         BrnRendererModule::EndRenderPostFx           reader
//         BrnCoronaManager::Construct                  reader
//         BrnSunCorona::GenerateOcclusionBuffer        reader
//         BrnSunCorona::RenderOccludedFlare            reader
//         BrnPostFxBloom::Render                       reader
//         BrnRendererModule::EndRenderAntiAliased      reader
//         BrnPostFx::Render                            reader
//         BrnRendererModule::Render                    reader
//     FIVE of those eight readers are not BrnRendererModule and have no factory instance
//     anywhere in scope; BrnPostFx cannot even include BrnRendererModule.h. A non-static
//     accessor is unreachable from the
//     very call sites this table exists to serve. (Same shape as the blend twin's
//     twelve-reader census in CgsBlendStateFactory.h.) And (2) it cannot be
//     expensively wrong: `static` is the more permissive spelling -- an instance-based
//     call written later, `lFactory.GetState(i)`, still compiles against it.
//
//     FOLLOW-UP: CgsBlendStateFactory::GetState should take the same treatment -- it is
//     what B4Blur::Parameters::m_scatterBlendState == saBlendStates[1] is still waiting
//     on (see BrnPostFx.cpp). Left alone here because it is another wave's file.
// =============================================================================

// Which slot of saDepthStencilStates each of the five built-in states occupies
// (assert-message strings the console binary embeds at each call site -- ground truth
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
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON,     // saDepthStencilStates[0]
    E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF,   // saDepthStencilStates[1]
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF,    // saDepthStencilStates[2]
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON,     // saDepthStencilStates[3]
    E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON,    // saDepthStencilStates[4]
    E_FACTORY_DEPTH_STENCIL_STATE_COUNT                  // 5
};

class CgsDepthStencilStateFactory
{
public:
    CgsDepthStencilStateFactory();   // declared in the original header; no console body attested here

    // Build the 5 depth/stencil states into saDepthStencilStates.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    virtual void Destruct();   // declared in the original header; declared-only here
    virtual bool Prepare();    // declared in the original header; declared-only here

    // An accessor defined inline in the original header. `static` is an INFERENCE --
    // see the banner above.
    //
    // NO BOUNDS ASSERT: every attested reader loads its slot with a constant index and
    // no check, and the console image carries no assert string for this accessor, so a
    // CGS_ASSERT here would be behaviour the binary does not have.
    static renderengine::DepthStencilState* GetState(u32 luIndex) { return saDepthStencilStates[luIndex]; }

private:
    // Declared in the original header and defined in the .cpp. saDepthStencilStates[0..4]
    // are five consecutive words that Construct clears and then fills one at a time.
    // Was the TU-local
    // array in the .cpp; promoted here so its readers can name a slot.
    static renderengine::DepthStencilState* saDepthStencilStates[E_FACTORY_DEPTH_STENCIL_STATE_COUNT];
};
