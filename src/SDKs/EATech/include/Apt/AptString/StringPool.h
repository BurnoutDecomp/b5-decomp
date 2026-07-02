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
    // The interned AS-name TABLE (X360 dword_8324E580, 88 entries; PS3
    // _ZN10StringPool10saConstantE). Entry [0] is the "__proto__" key the AS
    // member-op fast path compares against (hash 27581); [56]=onEnterFrame,
    // [59]=onLoad, [69]=onUnload etc. drive the clip-event dispatch. The 88
    // names were extracted from the TARGET binaries: the record data block
    // sStringPoolData @0x82F733FC (stride 264, end 0x82F78EBC => 88) populated
    // by the CRT initializer sub_82C71F10 (string literals recovered from its
    // store set); StringPool::Initialize points each entry at its record.
    enum { KU_CONSTANT_COUNT = 88 };
    static EAStringC saConstant[KU_CONSTANT_COUNT];

    // GetString @PS3 0x7DF664 -- `return &saConstant[code]` (the StringCode
    // accessor the engine indexes handler names through).
    static const EAStringC* GetString(int nCode) { return &saConstant[nCode]; }

    // Initialize @0x82AE3630 -- intern the 88 AS names into saConstant + allocate
    // the string-pool bucket array (sized to the config's string-pool count).
    // Called once by AptCommonInitialize during the Apt bring-up.
    static void Initialize(int nBucketCount);

    // ClearTemporaryPool @0x82AD8E20 -- release every temporary string node back
    // to its pool (the GC teardown's final step).
    static void ClearTemporaryPool();
};
