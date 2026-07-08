#ifndef SDKS_XAM_CCONFORMANCELIST_H
#define SDKS_XAM_CCONFORMANCELIST_H

#include "types.hpp"

#include "SDKs/Xam/CSchemaAccess.h"

// ===========================================================================
// SDKs/Xam/CConformanceList.h
//
// CConformanceList -- the fixed 64-slot table the Xbox 360 XAM (Xbox Account
// Manager) RPC marshalling layer keeps while (un)marshalling a typed message,
// tracking each schema field that carries a run-time "conformance" (an array
// length / union selector). CMarshaller::RecursiveMarshal establishes a slot
// for each conforming field, later acquires it when a dependent field is
// reached, and updates the wire value once it is known. Sibling of the
// CArgumentList / CSchemaData / CSchemaAccess / CMarshaller marshalling classes.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. There is no reference source and
// no DWARF for this TU; the SHAPE below is grounded purely on the X360 asm of
// the four functions (index math is a constant 16-byte stride off `this`, so
// the object is exactly an array of 0x40 entries). These are Microsoft-XDK-style
// C-prefixed marshalling classes, so the identifiers (CConformanceList,
// AcquireConformanceInfo, ...) are kept verbatim per the naming convention's
// external/platform-API carve-out.
//
// ENTRY (16-byte stride on X360; the mpData pointer widens on the 64-bit host):
//     +0x00  muFlags  u32   status word:
//                            bit 0x80000000  slot is established (valid)
//                            bit 0x40000000  slot has been acquired (in use)
//                            bits 0..1       element-size selector (log2 bytes:
//                                            0=byte 1=word 2=dword 3=qword)
//     +0x04  mpData   u8*   destination span the update is written through
//                           (Bind's lpData; 0 for a schema-constant slot)
//     +0x08  muValue  u64   the resolved conformance value
// ===========================================================================

class CConformanceList
{
public:
    // Maximum number of conformance slots (the guard rejects index >= 0x40).
    enum { KU_MAX_ENTRIES = 0x40 };

    // A single conformance slot (see header ENTRY notes).
    struct Entry
    {
        u32 muFlags; // +0x00  status / element-size selector
        u8* mpData;  // +0x04  destination span (widens on host)
        u64 muValue; // +0x08  resolved value
    };

    // @ 0x829802D0 -- establish (initialise) the slot for `luIndex` with a scope
    // (element-size selector, 0..3) and a starting value. When `lpDescriptor` is
    // non-null and its byte[1] carries the low bit, `luValue` is scaled down by the
    // element byte size (1 << luScope). Fails E_INVALIDARG for an out-of-range
    // index/scope and E_FAIL if the slot is already acquired.
    //   luUnused corresponds to the r7 argument the caller passes but this body
    //   never references (kept so lpDescriptor->r8 and lpData->r9 stay register-exact).
    s32 EstablishConformanceInfo(u32 luIndex, u32 luScope, u64 luValue,
                                 u32 luUnused, const u8* lpDescriptor, u8* lpData);

    // @ 0x82980350 -- acquire the established slot `luIndex`, marking it in-use and
    // handing back a pointer to it. Fails E_INVALIDARG when out of range, E_FAIL
    // when the slot is not established or already acquired.
    s32 AcquireConformanceInfo(u32 luIndex, Entry** lppOut);

    // @ 0x82980400 -- resolve the conforming scope + value for the field whose
    // descriptor is read from `lpAccess`. Reads the conforming descriptor, and when
    // it names an explicit schema constant (bit 0x80 set) looks the constant up and
    // establishes the slot, then acquires the slot and returns its value in *lpValue.
    s32 GetConformingScopeAndValue(u8 lScope, CSchemaAccess* lpAccess,
                                   u8* lpDescriptor, u32* lpValue);

    // @ 0x82980510 -- write `luValue` (its low 1/2/4/8 bytes, per the slot's element
    // size) out through the slot's destination span, byte-swapping per lbSwapEndian.
    s32 UpdateConformance(u32 luIndex, u64 luValue, u32 lbSwapEndian);

private:
    Entry maEntries[KU_MAX_ENTRIES]; // +0x00  the conformance slot table
};

#endif // SDKS_XAM_CCONFORMANCELIST_H
