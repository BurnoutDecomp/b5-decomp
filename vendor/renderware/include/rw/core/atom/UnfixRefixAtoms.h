#pragma once

#include <cstdint>

// ===========================================================================
// rw::core::atom::UnfixRefixAtoms -- a polymorphic RenderWare core "atom" helper
// whose only X360-emitted member is its (vector) deleting destructor.
//
// OWNING HOME for:
//     rw::core::atom::UnfixRefixAtoms::`vector deleting destructor'  @ 0x82BA7E20
//
// The X360 body @ 0x82BA7E20 is the standard MSVC deleting-destructor thunk:
//     *this = &off_8217FFE8;          ; restore the UnfixRefixAtoms vtable slot at +0
//     if ( flags & 1 ) operator delete(this);
//     return this;
// It performs no member teardown -- the only store is the vtable pointer at +0,
// so the underlying ~UnfixRefixAtoms() body is empty. The object is modelled as a
// polymorphic class carrying just its vtable pointer; MSVC re-synthesises the
// vtable-restore + conditional-free thunk from the virtual destructor below.
//
// This is the one-to-one sibling of rw::core::atom::FixupAtoms (vtable
// off_8217FFE0, dtor thunk @ 0x82BA7DD8, just 8 bytes earlier in .rdata); the
// only differences are the vtable slot (off_8217FFE8) and the addresses. The
// vtable off_8217FFE8 is emitted by this dtor TU itself.
// ===========================================================================

namespace rw
{
namespace core
{
namespace atom
{

class UnfixRefixAtoms
{
public:
    // @ 0x82BA7E20 (deleting-destructor thunk). The thunk restores the vtable at
    // +0 (off_8217FFE8) and conditionally frees; the destructor body itself is
    // empty.
    virtual ~UnfixRefixAtoms();
};

} // namespace atom
} // namespace core
} // namespace rw
