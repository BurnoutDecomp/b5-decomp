#include "rw/core/atom/UnfixRefixAtoms.h"

// ===========================================================================
// rw::core::atom::UnfixRefixAtoms -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   UnfixRefixAtoms::`vector deleting destructor'  @ 0x82BA7E20
//
// The X360 deleting-destructor thunk restores the UnfixRefixAtoms vtable pointer
// (off_8217FFE8) at this+0 and, when the low "should-free" flag bit is set,
// calls operator delete on the object before returning `this`:
//
//     *this = &off_8217FFE8;
//     if ( flags & 1 ) operator delete(this);
//
// The thunk emits no member teardown, so the destructor body is empty; MSVC
// synthesises the vtable-store + conditional free shown above from this trivial
// virtual ~UnfixRefixAtoms(). This mirrors the immediately-adjacent sibling
// rw::core::atom::FixupAtoms (vtable off_8217FFE0, thunk @ 0x82BA7DD8) exactly.
// See UnfixRefixAtoms.h.
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

UnfixRefixAtoms::~UnfixRefixAtoms()
{
}

} // namespace atom
} // namespace core
} // namespace rw
