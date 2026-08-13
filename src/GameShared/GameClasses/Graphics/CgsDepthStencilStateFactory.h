#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::DepthStencilState
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator

// =============================================================================
// CgsDepthStencilStateFactory.h  (GameShared/GameClasses/Graphics)
//
// CgsDepthStencilStateFactory -- builds the fixed set of depth/stencil render
// states the immediate-mode renderer selects between (X360 saDepthStencilStates,
// 5 slots). Construct sizes + carves each state through the supplied resource
// allocator and initialises it via renderengine::DepthStencilState::Initialize,
// CGS_ASSERT-ing every slot came back non-null.
//
// Declaration shape (virtual-ness, return types, vtable order) is DWARF-attested
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Graphics/
// CgsDepthStencilStateFactory.h): a single-vptr polymorphic class, no base.
// Destruct / Prepare / GetState are NOT in the X360 ledger for this TU -- only
// Construct is attested (0x827EBBA0). They are declared here to preserve the DWARF
// vtable order, and they are three different situations, which is worth spelling out:
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
//   * GetState is a NON-VIRTUAL accessor, so it costs the link nothing until a caller
//     exists. It is still declaration-only here, and bodying it means promoting the
//     TU-local saDepthStencilStates array in the .cpp to the DWARF's private static
//     member -- deliberately not done in this pass, because no caller can be written
//     until the BrnRendererModule placeholder swap happens, and that swap is blocked on
//     CgsRasterizerStateFactory still having no header at all.
// =============================================================================

class CgsDepthStencilStateFactory
{
public:
    CgsDepthStencilStateFactory();   // CgsDepthStencilStateFactory.h:57 (DWARF; not X360-attested here)

    // @ 0x827EBBA0 -- build the 5 depth/stencil states into saDepthStencilStates.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);

    virtual void Destruct();   // CgsDepthStencilStateFactory.cpp:116 (DWARF; declared-only here)
    virtual bool Prepare();    // CgsDepthStencilStateFactory.cpp:131 (DWARF; declared-only here)

    // CgsDepthStencilStateFactory.h:78 (DWARF; non-virtual accessor, declared-only here)
    renderengine::DepthStencilState* GetState(u32 luIndex);
};
