// ===========================================================================
// EATech Apt -- AptFrameStack: interpreter function-scope frame.
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//   ctor 0x82AF02A0 / dtor 0x82AF0488 / operator new 0x82AE6128 /
//   operator delete 0x82AF0370 / ClearScope 0x82AECC50 /
//   GetInScopeChain 0x82AE18D8 / SetWhereExistsInScopeChain 0x82AF5258 /
//   DestroyGCPointers 0x82AF0438.
//
// The frame is an AptValueWithHash (its embedded mHash holds the function's
// locals) plus mpEnclosingScope, the link to the next-outer lexical scope.
// Variable resolution walks that chain. The frame holds a counted reference to
// its enclosing scope (AddRef in the ctor / Release in the GC teardown), reached
// through the AptValue vtable (AddRef = vtbl[0], Release = vtbl[1]).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"        // AddRef/Release (scope ref)
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"            // Allocate / DeallocateAptValueGC + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"              // AptValueGC_MemItem::SetIsAllocated

// ---------------------------------------------------------------------------
// operator new @0x82AE6128 -- allocate from the GC pool, then set the
// AptValueGC_MemItem "allocated" flag (the X360 sets it explicitly here:
// SetIsAllocated(p, gAptValueGCSizeOffset, 1) after the pool Allocate). Guarded
// for null until the Apt runtime startup (AptInit) wires gpGCPoolManager.
// ---------------------------------------------------------------------------
void* AptFrameStack::operator new(size_t size)
{
    if (gpGCPoolManager == 0)
        return 0;

    void* lpMem = gpGCPoolManager->Allocate(size);
    if (lpMem != 0)
        reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @0x82AF0370 -- GC-pool free, clearing the allocated flag on
// success. DeallocateAptValueGC is exactly the X360 body (DOGMA Deallocate, then
// SetIsAllocated(p, gAptValueGCSizeOffset, 0) iff the free succeeded).
// ---------------------------------------------------------------------------
void AptFrameStack::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF02A0 -- AptValueWithHash(AptVFT_FrameStack, 4): a frame value with
// a 4-slot local-variable table. Link to the enclosing scope and take a counted
// reference to it (the X360 calls its vtbl[0] == AddRef when non-null).
// ---------------------------------------------------------------------------
AptFrameStack::AptFrameStack(AptFrameStack* pEnclosingScope)
    : AptValueWithHash(AptVFT_FrameStack, 4)
{
    mpEnclosingScope = pEnclosingScope;
    if (pEnclosingScope != 0)
        pEnclosingScope->AddRef();
}

// ---------------------------------------------------------------------------
// dtor @0x82AF0488 -- the X360 body is `if (mHash.mpTable) mHash.DestroyGCPointers()`,
// i.e. the compiler inlined the embedded-hash teardown into the frame dtor. Here
// the embedded AptNativeHash mHash member's own destructor performs exactly that
// guarded teardown (~AptNativeHash: `if (mpTable) DestroyGCPointers()`), so this
// body is empty -- writing the guarded call again would tear the table down twice.
// The enclosing-scope reference is released only by the GC path (DestroyGCPointers),
// matching the asm (the dtor does not touch mpEnclosingScope).
// ---------------------------------------------------------------------------
AptFrameStack::~AptFrameStack()
{
}

// ---------------------------------------------------------------------------
// DestroyGCPointers @0x82AF0438 -- release the enclosing-scope reference, then
// tear down the local-variable hash (the AptValueWithHash teardown).
// ---------------------------------------------------------------------------
void AptFrameStack::DestroyGCPointers()
{
    if (mpEnclosingScope != 0)
        mpEnclosingScope->Release();
    AptValueWithHash::DestroyGCPointers();   // mHash.DestroyGCPointers()
}

// ---------------------------------------------------------------------------
// ClearScope @0x82AECC50 -- empty this frame's locals but keep the bucket array
// allocated (tail-call into the hash's ClearDataNoDelete).
// ---------------------------------------------------------------------------
void AptFrameStack::ClearScope()
{
    mHash.ClearDataNoDelete();
}

// ---------------------------------------------------------------------------
// GetInScopeChain @0x82AE18D8 -- walk this frame outward; return the value bound
// to key in the nearest enclosing scope that has it, else null.
// ---------------------------------------------------------------------------
AptValue* AptFrameStack::GetInScopeChain(const EAStringC& key)
{
    for (AptFrameStack* pFrame = this; pFrame != 0; pFrame = pFrame->mpEnclosingScope)
    {
        AptValue* pFound = pFrame->mHash.Lookup(key);
        if (pFound != 0)
            return pFound;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// SetWhereExistsInScopeChain @0x82AF5258 -- if key is already bound somewhere up
// the chain, overwrite it in that frame and return true; otherwise return false
// without creating a new binding.
// ---------------------------------------------------------------------------
bool AptFrameStack::SetWhereExistsInScopeChain(const EAStringC& key, AptValue* pValue)
{
    for (AptFrameStack* pFrame = this; pFrame != 0; pFrame = pFrame->mpEnclosingScope)
    {
        if (pFrame->mHash.Lookup(key) != 0)
        {
            pFrame->mHash.Set(key, pValue);
            return true;
        }
    }
    return false;
}

// ===========================================================================
// The interpreter-facing frame-stack glue (HOMED 2026-07-02, retiring the
// AptRenderLinkStubs nulls).
// ===========================================================================
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // spFrameStack

// AptInterp_LookupScopeChain -- getVariable's function-local arm. The X360
// getVariable @0x82B03550 loads spFrameStack (off_8324E3DC), null-checks it,
// and calls AptFrameStack::GetInScopeChain (the innermost-outward locals walk).
class AptActionInterpreter;   // fwd (the interp arg is unused; X360 reads the static)
AptValue* AptInterp_LookupScopeChain(AptActionInterpreter* /*pInterp*/,
                                     const EAStringC* pName)
{
    AptFrameStack* const pFrame = AptScriptFunctionBase::GetActiveFrameStack();
    return pFrame ? pFrame->GetInScopeChain(*pName) : nullptr;
}

// AptScriptFunctionBase_GetActiveFrameStack -- the de-inlined static getter
// (off_8324E3DC) the render-link cluster called through a shim.
AptFrameStack* AptScriptFunctionBase_GetActiveFrameStack()
{
    return AptScriptFunctionBase::GetActiveFrameStack();
}
