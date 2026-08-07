#pragma once

// ===========================================================================
// EATech Apt -- AptGlobal: the ActionScript "_global" object (top-level scope).
//
// AptGlobal : AptObject -- the AS _global scope is an Object (it has named
// members + a prototype), so it derives from AptObject and is a garbage-
// collected value (AptValueGC base via AptValueWithHash). It is created once as
// a GC root (the ctor pins setGCRoot(1)) and lives for the life of the VM.
//
// It overrides the AptValue object-model member accessors so that resolving a
// member of _global consults the two global registries (the native/built-in
// globals + a fallback scope) rather than only its own property hash, and so
// that assigning a member of _global refuses to shadow a native global:
//   objectMemberLookup : native-globals table, then the fallback table.
//   objectMemberSet    : store in this->mHash unless the name is a native global.
//
// SHAPE + BODIES from the X360 ARTIST.XEX:
//     AptGlobal::AptGlobal          @ 0x82AF0530  (AptObject ctor inlined)
//     AptGlobal::~AptGlobal         @ 0x82AF05E8
//     AptGlobal::objectMemberLookup @ 0x82AE23B8
//     AptGlobal::objectMemberSet    @ 0x82AF5500
//     AptGlobal::operator new       @ 0x82AE6538
//     AptGlobal::operator delete    @ 0x82AF0590
// (The compiler-synthesized vector deleting destructor @ 0x82AF05F8 is a thunk
// and is not reconstructed.)
//
// LAYOUT: AptObject (32 bytes on the console) -- AptGlobal adds no data members
// (the ctor's only field write is the inherited mClassFlags, cleared to 0). The
// vtable index is AptVFT_Global (17); the object-hash capacity is 11.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptObject.h"
// (AptObject.h -> AptValueWithHash.h transitively provides EAStringC /
//  AptNativeString + the AptValueWithHash base used by the global tables below.)

struct AptGlobal : public AptObject
{
    // GC-pool allocation. AptGlobal is a garbage-collected value (AptValueGC
    // base), so -- like AptArray -- its block comes from the GC pool
    // (gpGCPoolManager) and carries the AptValueGC_MemItem "is allocated" flag,
    // rather than the non-GC leaves' gpNonGCPoolManager route. Bodies are in
    // AptGlobal.cpp so the header need not pull in the pool-manager / MemItem
    // types (avoids an include cycle with AptDefine.h / AptValueGCPoolManager.h,
    // which both include AptValue.h).
    static void* operator new(size_t size);    // @0x82AE6538
    static void  operator delete(void* p, size_t size);   // @0x82AF0590

    AptGlobal();                               // @0x82AF0530
    virtual ~AptGlobal();                      // @0x82AF05E8

    // ---- AptValue object-model virtual overrides --------------------------
    // Resolve a member of _global: the native-globals table first, then the
    // fallback table when the first miss is null or not a defined value.
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x82AE23B8
    // Assign a member of _global: store in this object's own property hash unless
    // the name names a native global (which must not be shadowed). Always true.
    virtual bool      objectMemberSet(AptValue* const pThis,
                                      const AptNativeString* const pName,
                                      AptValue* const pValue);                       // @0x82AF5500
};

// ---------------------------------------------------------------------------
// The two global scope objects _global member resolution consults, DEFINED in
// AptGlobal.cpp and built + wired by AptInit's AptValueInitialize @0x82B02800
// (the _global fallback scope and its extension object, both AddRef'd there).
// Each is an AptValueWithHash whose embedded property hash holds the registered
// members; AptGlobal reaches them through the public Lookup/Set
// (== the X360's AptNativeHash::Lookup(table + 8 /*mHash*/, name)).
//   gpAptNativeGlobals  (X360 off_8324E37C) -- the native/built-in globals.
//       Looked up first; objectMemberSet refuses to shadow a name found here.
//   gpAptGlobalFallback (X360 off_8324E380) -- the fallback scope consulted by
//       objectMemberLookup when the native lookup misses / is undefined.
// ---------------------------------------------------------------------------
extern AptValueWithHash* gpAptNativeGlobals;
extern AptValueWithHash* gpAptGlobalFallback;
