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

    // size = (384 * (a1 + a2) + (a3 << 8) + 127) & ~127
    const u32 luSize =
        (0x180u * static_cast<u32>(luCountA + luCountB)
         + (static_cast<u32>(luCountC) << 8) + 0x7Fu) & ~0x7Fu;

    // result[0] = { m_size = size, m_alignment = 128 }  (the std of the {size,128} qword)
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = 128u;
    return lpResult;
}

} // namespace physics
} // namespace rw
