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

class AptString;   // the pooled node (SDKs/EATech/include/Apt/AptValue/AptString.h)

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

    // Teardown @0x82AE3720 -- the Apt-shutdown counterpart of Initialize: Release
    // every pooled node in every bucket chain, free the bucket array back to the
    // pool, and reset the interned AS-name table to empty. Called by AptCommonShutdown.
    static void Teardown();

    // GetFromPool @0x82AF2F68 -- intern an AS name: hash pName into a bucket chain and
    // return the pooled AptString for it, allocating + inserting (and AddRef'ing) a
    // fresh node on a miss. The returned value is GC-rooted (incGCRoot) so it survives
    // the collector. Called by AptActionInterpreter::_parseStream.
    static AptString* GetFromPool(const char* pName);

    // RemoveFromPool @0x82AD8C98 -- drop one GC-root reference to a pooled AptString;
    // permanently-pinned (root == MAX_GCROOT) values are untouched, and when the last
    // root goes away the node is unlinked from its bucket chain and Release()'d.
    static AptString* RemoveFromPool(AptString* pValue);

    // ClearTemporaryPool @0x82AD8E20 -- release every temporary string node back
    // to its pool (the GC teardown's final step).
    static void ClearTemporaryPool();
};
