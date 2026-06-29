#pragma once

// ===========================================================================
// EATech Apt -- StringPool: the Apt temporary-string pool.
//
// MINIMAL HOME: only the static ClearTemporaryPool() entry point is declared
// here (X360 @0x82AD8E20), needed by AptGC::CleanAll's teardown. The full pool
// (the free-list walk + its node type) is its own TU; the body is supplied there.
// StringPool is forward-declared in EAString.h; this gives it the one method the
// GC teardown calls by name.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

class StringPool
{
public:
    // ClearTemporaryPool @0x82AD8E20 -- release every temporary string node back
    // to its pool (the GC teardown's final step).
    static void ClearTemporaryPool();
};
