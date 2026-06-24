#include "rw/core/atom/FixupAtoms.h"

// ===========================================================================
// rw::core::atom::FixupAtoms -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   FixupAtoms::`vector deleting destructor'  @ 0x82BA7DD8
//
// The X360 deleting-destructor thunk restores the FixupAtoms vtable pointer
// (off_8217FFE0) at this+0 and, when the low "should-free" flag bit is set,
// calls operator delete on the object before returning `this`:
//
//     *this = &off_8217FFE0;
//     if ( flags & 1 ) operator delete(this);
//
// The thunk emits no member teardown, so the destructor body is empty; MSVC
// synthesises the vtable-store + conditional free shown above from this trivial
// virtual ~FixupAtoms(). See FixupAtoms.h.
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

FixupAtoms::~FixupAtoms()
{
}

} // namespace atom
} // namespace core
} // namespace rw
