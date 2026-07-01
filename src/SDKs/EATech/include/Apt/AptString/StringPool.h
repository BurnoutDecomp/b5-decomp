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
// saConstant: the interned `__proto__` property key (X360 dword_8324E580). The
// AS member-op fast path compares an assigned/looked-up member name against it to
// route the special `__proto__` slot (AptNativeHash::Set/Lookup, hash 27581).
// Declared `static const EAStringC` here; AptNativeHash.h carries a matching
// forward decl for the call sites. Defined in AptStringPool.cpp.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC saConstant

class StringPool
{
public:
    // saConstant @ X360 dword_8324E580 -- the interned "__proto__" key.
    static const EAStringC saConstant;

    // Initialize @0x82AE3630 -- allocate the interned AS-name table + the string-pool
    // bucket array (sized to the config's string-pool count). Called once by
    // AptCommonInitialize during the Apt bring-up. Body in AptInit.cpp (beside the
    // other bring-up entry points). `nBucketCount` is the config bucket count.
    static void Initialize(int nBucketCount);

    // ClearTemporaryPool @0x82AD8E20 -- release every temporary string node back
    // to its pool (the GC teardown's final step).
    static void ClearTemporaryPool();
};
