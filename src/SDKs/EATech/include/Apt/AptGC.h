#pragma once

// ===========================================================================
// EATech Apt -- AptGC: the Apt ActionScript-value garbage collector helper.
//
// AptGC drives the mark walk over the live AptValue graph held by the
// AptValueGC_PoolManager, and the full teardown of every value at shutdown. Two
// X360 entry points:
//
//   AptGC::sReferenceRegistrationCb @0x82AD9C80 -- the reference-registration
//       callback installed into AptValue::sReferenceRegistrationCb. Each GC value
//       calls it (through that pointer) once per held AptValue reference from its
//       RegisterReferences() override: it marks the referenced value (if not
//       already marked) and recurses into it, walking the graph.
//
//   AptGC::CleanAll @0x82AE4A40 -- shutdown teardown: flush the deferred-release
//       vector, run PreDestroy()+DestroyGCPointers() over every live value (with
//       refcount-driven deletion suspended), DeleteThis() every live value, flush
//       the deferred vector again, then clear the AptInteger / AptFloat / StringPool
//       free-lists.
//
// No Feb-2007 / DWARF body exists for AptGC; the shape is from the X360 ARTIST.XEX
// pseudocode/asm of its two methods (+ the leak's `friend class AptGC;`). The
// callback signature matches AptValue::ReferenceRegistrationCb so the Apt GC
// startup can install &AptGC::sReferenceRegistrationCb.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue + ReferenceRegistrationCb

class AptGC
{
public:
    // sReferenceRegistrationCb @0x82AD9C80 -- the GC mark-walk callback. Reads the
    // AptValue* in pSlot and, if unmarked, marks it and recurses through its
    // RegisterReferences(). Only pSlot is used (the X360 asm ignores pOwner /
    // pDebugName / bFlag). Returns the slot's value (every caller discards it).
    static void* sReferenceRegistrationCb(const AptValue* pOwner, void* pSlot,
                                          const char* pDebugName, int bFlag);

    // CleanAll @0x82AE4A40 -- the full Apt value-pool teardown (see the header).
    static void CleanAll();
};
