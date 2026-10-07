// ===========================================================================
// RealmcIface::MemcardInterface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcMemcardInterface.h for the vtable/slot layout. The CreateInstance factory
// sits with the concrete implementation in RealmcMemcardInterfaceImpl.cpp.
// ===========================================================================

#include "SDKs/Realmc/RealmcMemcardInterface.h"

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MemcardInterface::MemcardInterface @ 0x82B51C00
//
//   lis  r11, off_821484E0@ha ; addi r11, r11, off_821484E0@l
//   stw  r11, 0(r3) ; blr
//
// Install the class vtable and return. An empty ctor body emits exactly that
// store; the compiler produces off_821484E0 from the class definition.
// ---------------------------------------------------------------------------
MemcardInterface::MemcardInterface()
{
}

// ---------------------------------------------------------------------------
// MemcardInterface::~MemcardInterface -- backs the X360 `vector deleting
// destructor' @ 0x82B51BB8:
//
//   *a1 = off_821484E0 ; if ( (a2 & 1) != 0 ) operator delete(a1) ; return a1
//
// i.e. restore the vtable, then operator-delete when the delete flag bit0 is set.
// The compiler emits that deleting-destructor thunk from this (empty) virtual dtor.
// ---------------------------------------------------------------------------
MemcardInterface::~MemcardInterface()
{
}

} // namespace RealmcIface
