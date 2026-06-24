#include "rw/rwcore_structs.h"

// ===========================================================================
// rw::BaseResourceDescriptors<3> -- out-of-line template-instance home.
//
// OWNING HOME for the single X360-emitted member of the 3-entry instantiation:
//     rw::BaseResourceDescriptors<3>::operator+=  @ 0x828DEF40
//         (called by CgsResource::DebugPoolTypeList::Update)
//
// The generic per-Count body lives inline in rw/rwcore_structs.h. The X360 build
// emitted a non-inlined copy of the Count==3 instance, so this file forces that
// out-of-line emission with an explicit template instantiation. No new body is
// written here -- the instantiation realises the committed generic operator+=.
//
// The X360 asm @ 0x828DEF40 unrolls three descriptor pairs (offsets 0/4, 8/0xC,
// 0x10/0x14); each pair is a rw::BaseResourceDescriptor {m_size, m_alignment}.
// For each pair the asm:
//   * rounds this.m_size UP to other.m_alignment when other.m_alignment > 1
//       (addi -1 ; add ; andc  ==  (align-1 + size) & ~(align-1)),
//   * widens this.m_alignment to max(this.m_alignment, other.m_alignment)
//       (load other.align into r9; if this.align > other.align keep this.align),
//   * adds other.m_size to the (rounded) this.m_size.
// That is exactly the committed generic operator+= loop with Count==3, so the
// explicit instantiation reproduces the asm store-for-store (three iterations).
// ===========================================================================

namespace rw
{
    // Force out-of-line emission of the Count==3 instance's operator+= (and the
    // rest of the instantiation) -- the home the X360 @ 0x828DEF40 occupies.
    template struct BaseResourceDescriptors<3>;
}
