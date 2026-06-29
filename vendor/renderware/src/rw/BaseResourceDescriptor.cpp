#include "rw/rwcore_structs.h"

// ===========================================================================
// rw::BaseResourceDescriptor -- out-of-line default-ctor home.
//
// OWNING HOME for the single X360-emitted function:
//     rw::BaseResourceDescriptor::BaseResourceDescriptor  @ 0x821F05C8
//         (referenced across the resource-type tree: CgsDictionaryResourceType,
//          CgsFontResourceType, materialcrc32.h, etc.)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative. No reference source and no DecFIGS DWARF hints exist for this TU.
//
//   0x821F05C8  li   r11, 0
//   0x821F05CC  li   r10, 1
//   0x821F05D0  stw  r11, 0(r3)     ; m_size      = 0
//   0x821F05D4  stw  r10, 4(r3)     ; m_alignment = 1
//   0x821F05D8  blr                 ; returns r3 (this) unchanged
//
// i.e. the trivial default descriptor { m_size = 0, m_alignment = 1 } -- the
// identity element for BaseResourceDescriptors<N>::operator+= (size 0, align 1
// rounds/widens nothing). This is the very initialisation the rest of the
// resource family writes by hand (see rw::physics::SimulationWorkspace::
// GetResourceDescriptor's "fill result[0..4] with { m_size = 0, m_alignment = 1 }"
// and LinearResourceAllocator::Initialize). The struct is declared in
// rw/rwcore_structs.h; the X360 build emitted this ctor out-of-line (not inlined),
// so the body is homed here -- additive: no data members are added, sizeof stays 8
// and the type remains standard-layout (offsetof on m_size/m_alignment unaffected).
// ===========================================================================

namespace rw
{

// X360 0x821F05C8 -- value-initialise to the identity descriptor and return this.
BaseResourceDescriptor::BaseResourceDescriptor()
    : m_size(0)
    , m_alignment(1)
{
}

}  // namespace rw
