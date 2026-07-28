#ifndef SDKS_XAM_CCRITICALSECTION_H
#define SDKS_XAM_CCRITICALSECTION_H

#include "types.hpp"

// ===========================================================================
// SDKs/Xam/CCriticalSection.h
//
// CCriticalSection -- a polymorphic critical-section wrapper in the Xbox 360
// XAM (Xbox Account Manager) marshalling cluster of BURNOUT_X360_ARTIST.XEX. Its
// scalar deleting destructor (@ 0x8297D9E0) sits immediately ahead of the sibling
// CArgumentList / CSchemaData / CMarshaller marshalling classes. This is a
// Microsoft-XDK-style C-prefixed class, so the identifier CCriticalSection is
// kept verbatim per the naming convention's external/platform-API carve-out.
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed purely from the X360 asm of the scalar deleting destructor:
//
//   CCriticalSection::`scalar deleting destructor' @ 0x8297D9E0:
//       *this = &off_8210BC20;             // stw r11, 0(r31)  -- reset vtable
//       if ((flag & 1) != 0)               // clrlwi. + beq
//           operator delete(this);         // bl operator_delete (GLOBAL delete)
//       return this;
//
// The routine only resets the vtable pointer and conditionally frees `this`
// through the GLOBAL operator delete -- there is no member teardown and no
// class-specific deallocator in this TU. So ~CCriticalSection has an empty body
// and exists only to anchor the vtable (off_8210BC20); the host compiler
// synthesises the scalar deleting destructor (run dtor, then
// `if (flag & 1) ::operator delete(this)`) from it. No distinct body is written
// for the thunk -- it is emitted by the compiler from the virtual dtor.
// ===========================================================================

class CCriticalSection
{
public:
    // Out-of-line virtual destructor: anchors the vtable (off_8210BC20). The
    // X360 scalar deleting destructor @ 0x8297D9E0 is the compiler-synthesised
    // thunk over this dtor, freeing via the global operator delete.
    virtual ~CCriticalSection();
};

#endif // SDKS_XAM_CCRITICALSECTION_H
