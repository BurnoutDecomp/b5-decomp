#pragma once

// =====================================================================================
// rw::physics::SimulationWorkspace -- the per-frame scratch arena the EATech RenderWare
// physics step runs in. The only surface this TU reconstructs is the build-time memory
// sizer the physics module calls before it carves the workspace.
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the
// asm is authoritative.
//
// ⚠️ CORRECTION 2026-08-04 (task #135): the line that used to stand here said "There is NO
// matching reference TU and no DecFIGS DWARF for this type." THE SECOND HALF WAS FALSE --
// references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/physics/simulationworkspace.h
// exists and declares the whole surface (ctor, dtor, GetResourceDescriptor, Initialize,
// Release). It is the same false claim rw/physics/simulation.h carried until this week.
// `Initialize` below is now the DWARF's own (:84), not an invented name.
//
// LAYOUT AUTHORITY (GetResourceDescriptor @0x82BC4090): the function never touches a
// SimulationWorkspace instance -- it only fills a 5-entry serialised resource descriptor
// (rw::BaseResourceDescriptors<5>) describing the single 128-byte-aligned block the
// workspace needs. So no instance layout is modelled here.
// =====================================================================================

#include "rw/rwcore_structs.h" // rw::BaseResourceDescriptors<5>

namespace rw
{
namespace physics
{

struct SimulationWorkspace
{
    // GetResourceDescriptor @0x82BC4090 -- size the physics-step workspace.
    //
    // Returns a 5-entry resource descriptor whose first entry sizes the single block:
    //     entry[0] = { m_size = (384 * (a1 + a2) + (a3 << 8) + 127) & ~127, m_alignment = 128 }
    //     entry[1..4] = { m_size = 0, m_alignment = 1 }
    //
    // The three counts (called by CgsPhysics::PhysicsSimulationModule::
    // AllocateMemoryAndInitialiseRW) feed the bump: 384 bytes per (luCountA + luCountB)
    // entity plus 256 bytes (1 << 8) per luCountC, rounded up to the 128-byte block
    // alignment.
    static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(
        rw::BaseResourceDescriptors<5>* lpResult, int luCountA, int luCountB, int luCountC);

    // Initialize @0x82AD5060 -- `lwz r3,0(r3) ; blr`. Returns the workspace arena base
    // (Resource block 0). See the body's banner for why this address disassembles under
    // the name `AptDisplayListState::GetFirstItem`.
    static SimulationWorkspace* Initialize(
        void** lpMemory, int luCountA, int luCountB, int luCountC);
};

} // namespace physics
} // namespace rw
