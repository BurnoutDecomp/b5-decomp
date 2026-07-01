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
// Destruct / Prepare / GetState are NOT in the X360 ledger for this TU (only
// Construct is attested) -- declared here only to preserve the DWARF vtable
// order; their bodies are a follow-on pass (same convention as
// CgsBufferedDispatchFrame.h).
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
