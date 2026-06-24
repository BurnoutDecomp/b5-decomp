#pragma once

#include <cstdint>

// ===========================================================================
// rw::core::atom::FixupAtoms -- a polymorphic RenderWare core "atom" helper
// whose only X360-emitted member is its (vector) deleting destructor.
//
// OWNING HOME for:
//     rw::core::atom::FixupAtoms::`vector deleting destructor'  @ 0x82BA7DD8
//
// The X360 body @ 0x82BA7DD8 is the standard MSVC deleting-destructor thunk:
//     *this = &off_8217FFE0;          ; restore the FixupAtoms vtable slot at +0
//     if ( flags & 1 ) operator delete(this);
//     return this;
// It performs no member teardown -- the only store is the vtable pointer at +0,
// so the underlying ~FixupAtoms() body is empty. The object is modelled as a
// polymorphic class carrying just its vtable pointer; MSVC re-synthesises the
// vtable-restore + conditional-free thunk from the virtual destructor below
// (matching the ServerInterface* deleting-destructor reconstruction precedent).
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

class FixupAtoms
{
public:
    // @ 0x82BA7DD8 (deleting-destructor thunk). The thunk restores the vtable at
    // +0 and conditionally frees; the destructor body itself is empty.
    virtual ~FixupAtoms();
};

} // namespace atom
} // namespace core
} // namespace rw
