// =====================================================================================
// rw::physics::SimulationWorkspace::GetResourceDescriptor -- build-time sizer for the
// physics-step scratch workspace.
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82BC4090; the
// PowerPC asm is authoritative. No reference source and no DecFIGS DWARF exist.
//
// Store-for-store from the asm (r3 = result, r4 = a1, r5 = a2, r6 = a3):
//
//   ; three unrolled {0,1}-pair fills into a 40-byte stack scratch (back_chain) -- the
//   ; compiler materialised the descriptor on the stack twice before writing it through
//   ; the return pointer; those scratch stores are never read, so only the result fill is
//   ; observable.
//   ; fill result[0..4] with { m_size = 0, m_alignment = 1 }:
//   for i in 4..0:  stw 0,0(result) ; stw 1,4(result) ; result += 8
//
//   r11 = a1 + a2
//   r10 = a3 << 8
//   r11 = r11 * 0x180                 ; 384 * (a1 + a2)
//   r9  = r11 + r10                   ; + (a3 << 8)
//   r9  = r9 + 0x7F
//   r9  = r9 & ~0x7F                  ; round up to a multiple of 128
//
//   ; std of the {size, 128} qword over result[0]: big-endian +0 = size, +4 = 128:
//   var = (u64(r9) << 32) | 0x80
//   std var, 0(result)               ; result[0] = { m_size = size, m_alignment = 128 }
//   return result
// =====================================================================================

#include "rw/physics/SimulationWorkspace.h"

#include "vendor/renderware/physics/Jacobian.hpp"   // JacobianStride()

namespace rw
{
namespace physics
{

rw::BaseResourceDescriptors<5>* SimulationWorkspace::GetResourceDescriptor(
    rw::BaseResourceDescriptors<5>* lpResult, int luCountA, int luCountB, int luCountC)
{
    rw::BaseResourceDescriptor* lpEntries = lpResult->m_baseResourceDescriptors;

    // result[0..4] = { m_size = 0, m_alignment = 1 }
    for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
    {
        lpEntries[luEntry].m_size      = 0u;
        lpEntries[luEntry].m_alignment = 1u;
    }

    // CONSOLE: size = (384 * (a1 + a2) + (a3 << 8) + 127) & ~127
    //
    // ⚠️⚠️ FIXED 2026-08-04 (task #135) -- THIS WAS A LIVE LATENT BUG, not a cosmetic one.
    // The 384 here is the CONSOLE jacobian stride, and it sizes the very buffer
    // Simulation::SetWorkspace then carves with `JacobianStride()` == sizeof(Jacobian) ==
    // 416 on x64 (the host record grew when the node pointer left its w lane -- see
    // Jacobian.hpp). Sizing at 384 and carving at 416 leaves the block 32 bytes short per
    // joint/drive slot -- with the shipping counts (36 joints + 1 drive) the contact buffer
    // would have started 1,184 bytes past the end of its own allocation. Nothing asserts;
    // the solver would simply corrupt whatever followed. The stride is `JacobianStride()`
    // for exactly the reason SetWorkspace's is.
    // The 256-bytes-per-contact term (`a3 << 8`) is a contact-pair scratch with no pointers
    // in it and is unchanged; the 128-byte alignment is a hardware constraint, not a stride.
    const u32 luSize =
        (JacobianStride() * static_cast<u32>(luCountA + luCountB)
         + (static_cast<u32>(luCountC) << 8) + 0x7Fu) & ~0x7Fu;

    // result[0] = { m_size = size, m_alignment = 128 }  (the std of the {size,128} qword)
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = 128u;
    return lpResult;
}

// -------------------------------------------------------------------------------------
// SimulationWorkspace::Initialize @ 0x82AD5060
//
// ⭐ THIS FUNCTION IS AN IDA NAMING COLLISION, NOT A MISSING SYMBOL. Its call site inside
// CgsPhysics::PhysicsSimulationModule::AllocateMemoryAndInitialiseRW @0x828A2320 disassembles
// as `bl AptDisplayListState__GetFirstItem` -- IDA folded it onto an identical two-instruction
// function elsewhere in the image and kept the other name. Three things identify it: the call
// sits between the workspace's GetResourceDescriptor and the Simulation Initialize that
// consumes its result, it is handed (blockArray, 36, 1, 1024) -- the workspace's own three
// counts -- and its result is what SetWorkspace is given.
//
// The whole body is:
//     0x82AD5060  lwz  r3, 0(r3)
//     0x82AD5064  blr
// i.e. return the first block of the Resource array. The workspace is a bare bump arena with
// no header of its own: SetWorkspace carves the three jacobian buffers straight off the base.
// The three counts are consumed by the sizer, not here -- they are still in the signature
// because the console really does pass them (r4/r5/r6 are live at the call).
// -------------------------------------------------------------------------------------
SimulationWorkspace* SimulationWorkspace::Initialize(
    void** lpMemory, int /*luCountA*/, int /*luCountB*/, int /*luCountC*/)
{
    return static_cast<SimulationWorkspace*>(lpMemory[0]);
}

} // namespace physics
} // namespace rw
