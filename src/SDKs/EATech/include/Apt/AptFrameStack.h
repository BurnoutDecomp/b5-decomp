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
// LAYOUT (x64, ctor ??0AptFrameStack@@QEAA@PEAV0@@Z @0x140825FA0): AptValueWithHash
// base (vptr+bitfield 0x10 + the embedded AptNativeHash mHash at +0x10, 0x28 bytes)
// followed by mpEnclosingScope at +0x38; sizeof 0x40 (GC-pool alloc 64 at
// sub_1408355C0). Base ctor is AptValueWithHash(AptVFT_FrameStack, 4) -- vtbl
// index 10, hash capacity 4. x64 also ships a second ctor
// ??0AptFrameStack@@QEAA@PEAV0@H@Z @0x140826010 (explicit hash size, rounded up
// to the next power of two) -- declared below.
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

    // [x64 +0x38] the next-outer lexical scope frame (the scope-chain link). Counted
    // reference: AddRef'd in the ctor, Release'd in DestroyGCPointers. It is itself
    // an AptFrameStack -- the chain walkers read its mHash -- so it is typed as one
    // (AddRef/Release are the inherited AptValue virtuals the X360 calls by slot).
    AptFrameStack* mpEnclosingScope;

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptFrameStack, mHash)            == 0x10, "x64: ClearScope/Lookup thunks 'add rcx,10h'");
        static_assert(offsetof(AptFrameStack, mpEnclosingScope) == 0x38, "x64 ctor @0x140825FA0: 'mov [rbx+38h],rdi'");
        static_assert(sizeof(AptFrameStack) == 0x40, "x64: GC-pool alloc 64 at sub_1408355C0 (CreateFrameStack)");
    }

    // ctor @0x82AF02A0 -- chain to the enclosing scope (AddRef'd) + an empty local
    // table (hash capacity 4). pEnclosingScope may be null (the outermost frame).
    explicit AptFrameStack(AptFrameStack* pEnclosingScope);

    // x64 second ctor ??0AptFrameStack@@QEAA@PEAV0@H@Z @0x140826010 -- explicit hash
    // capacity, rounded UP to the next power of two before storing (B4Extern
    // corroborates the two-ctor set). Body in AptFrameStack.cpp.
    AptFrameStack(AptFrameStack* pEnclosingScope, int nHashSize);

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
