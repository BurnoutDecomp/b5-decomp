#pragma once

// ===========================================================================
// EATech Apt -- AptFrameStack: the ActionScript interpreter function-scope frame.
//
// An AptFrameStack is one activation record of the AS call stack: a garbage-
// collected, property-bearing value (AptValueWithHash) whose embedded hash holds
// the function's local variables, plus a link to the enclosing lexical scope
// (mpEnclosingScope). Variable resolution walks this chain
// (GetInScopeChain / SetWhereExistsInScopeChain) from the innermost frame
// outward; AptScriptFunctionBase::CreateFrameStack builds one per call and the
// interpreter's getVariable/setVariable consult it.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (the authoritative spine):
//     ctor                        @ 0x82AF02A0
//     dtor                        @ 0x82AF0488
//     `vector deleting destructor'@ 0x82AF04E0  (compiler thunk -- dropped)
//     operator new                @ 0x82AE6128
//     operator delete             @ 0x82AF0370
//     ClearScope                  @ 0x82AECC50
//     GetInScopeChain             @ 0x82AE18D8
//     SetWhereExistsInScopeChain  @ 0x82AF5258
//     DestroyGCPointers           @ 0x82AF0438
//
// LAYOUT: AptValueWithHash base (AptValueGC vtable+bitfield 8 bytes + the embedded
// AptNativeHash mHash, 20 bytes -> ends at +0x1C) followed by mpEnclosingScope at
// +0x1C. Base ctor is AptValueWithHash(AptVFT_FrameStack, 4) -- vtbl index 10,
// hash capacity 4 (asm: li r4,0xA / li r5,4).
//
// AptFrameStack is garbage-collected (AptValueGC base), so -- like AptArray and
// unlike the non-GC leaves (AptInteger/AptFloat) -- it allocates from the GC pool
// (gpGCPoolManager) and marks the AptValueGC_MemItem allocated flag. The
// operator new/delete bodies live in AptFrameStack.cpp so this header need not
// pull in the pool-manager type (avoids the AptDefine.h / AptValueGCPoolManager.h
// include cycle, both of which include AptValue.h).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"     // AptValueWithHash base + mHash (pulls in AptValue)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC keys

struct AptFrameStack : public AptValueWithHash
{
    // GC-pool allocation (see header note). new @0x82AE6128 / delete @0x82AF0370.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // [+0x1C] the next-outer lexical scope frame (the scope-chain link). Counted
    // reference: AddRef'd in the ctor, Release'd in DestroyGCPointers. It is itself
    // an AptFrameStack -- the chain walkers read its mHash -- so it is typed as one
    // (AddRef/Release are the inherited AptValue virtuals the X360 calls by slot).
    AptFrameStack* mpEnclosingScope;

    // ctor @0x82AF02A0 -- chain to the enclosing scope (AddRef'd) + an empty local
    // table (hash capacity 4). pEnclosingScope may be null (the outermost frame).
    explicit AptFrameStack(AptFrameStack* pEnclosingScope);

    // dtor @0x82AF0488 -- the embedded mHash member teardown frees the locals (see
    // the .cpp note); the enclosing-scope reference is released by the GC path
    // (DestroyGCPointers), not here.
    virtual ~AptFrameStack();

    // ---- GC virtual override ---------------------------------------------------
    // DestroyGCPointers @0x82AF0438 -- release the enclosing scope, then tear down
    // the local-variable hash.
    virtual void DestroyGCPointers();

    // ---- scope-chain operations ------------------------------------------------
    // ClearScope @0x82AECC50 -- empty this frame's locals, keeping the bucket array
    // allocated for reuse (ClearDataNoDelete).
    void ClearScope();

    // GetInScopeChain @0x82AE18D8 -- resolve key by walking this frame and each
    // enclosing scope; returns the first frame's value holding it, else null.
    AptValue* GetInScopeChain(const EAStringC& key);

    // SetWhereExistsInScopeChain @0x82AF5258 -- if key already exists somewhere up
    // the scope chain, store pValue into that frame and return true; else false (no
    // new binding is created).
    bool SetWhereExistsInScopeChain(const EAStringC& key, AptValue* pValue);
};
