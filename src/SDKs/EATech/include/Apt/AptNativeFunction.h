#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptNativeFunction.
//
// The ActionScript "native function" value: an AptValue that wraps a C callback
// (an AptExtFunctionPtr) so engine-side code can expose a function to the apt VM.
// It is the value AptExtObject::CreateNewAptFunction(AptExtFunctionPtr) hands back,
// and the per-type objectMemberLookup recognizers (AptKey/AptString/AptError/...)
// construct one to back a scriptable member/method.
//
// SHAPE: the leak's AptExtObject.h declares the wrapper's public face --
//     typedef void * AptExtFunctionPtr;
//     static AptNativeFunction *CreateNewAptFunction(AptExtFunctionPtr pAptExtFnc);
// -- and AptValue.h declares the matching cast `AptNativeFunction* c_nativefunction()`.
// The class itself has no Feb-2007 / DecFIGS header in scope, so the LAYOUT is
// recovered from the X360 ARTIST.XEX:
//     AptNativeFunction::AptNativeFunction        @ 0x82AF0178
//     AptNativeFunction::operator new             @ 0x82AE60D8
//     AptNativeFunction::operator delete          @ 0x82AF01E8
//     AptNativeFunction::`vector deleting destructor' @ 0x82AF0240 (compiler thunk; dropped)
//
// BASE: the ctor forwards directly to AptValueWithHash::AptValueWithHash(eType=9,
// nHashCapacity=8) -- NOT to AptObject::AptObject -- so AptNativeFunction derives
// from AptValueWithHash (a property-bearing AptValueGC), with its own two trailing
// members. (The deleting-destructor thunk's `bl AptObject::~AptObject` is ICF
// folding: AptObject adds only a POD mClassFlags, so its destructor body is
// byte-identical to ~AptValueWithHash and the linker folded the two; the real base
// teardown here is ~AptValueWithHash, run automatically.)
//
// LAYOUT (sizeof = 36 / 0x24, pinned by `operator delete(this, 36)` in the thunk):
//   AptValueWithHash base ............ 28 bytes (vtable + bitfield + AptNativeHash)
//   mClassFlags  uint32_t ............ +0x1C   (class / implemented-object flags)
//   mpFunction   AptExtFunctionPtr ... +0x20   (the wrapped native callback)
//
// vtable object-type index = AptVFT_NativeFunction (9), confirmed by the ctor's
// `li r4, 9` argument to the base.
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptNativeFunction,
// AptExtFunctionPtr, the AptVFT_* index) are an external/middleware API and kept
// verbatim.
// ===========================================================================

#include <cstddef>   // size_t
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValueWithHash.h"            // AptValueWithHash base + AptVFT_NativeFunction
#include "SDKs/EATech/include/Apt/AptDefine.h"                    // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"                // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                  // AptValueGC_MemItem (alloc bookkeeping)

// The native-callback pointer type (leak AptExtObject.h:116). A bare `void*` in
// the SDK: the apt VM stores it opaquely and the dispatch layer casts it to the
// concrete call signature at invocation time.
typedef void* AptExtFunctionPtr;

class AptNativeFunction : public AptValueWithHash
{
public:
    // ---- GC pool new / delete (X360 operator new @0x82AE60D8 / delete @0x82AF01E8)
    // Both route through the garbage-collected value pool (gpGCPoolManager) and flip
    // the AptValueGC_MemItem "allocated" flag, exactly like the rest of the GC value
    // family. Defined out-of-line in AptNativeFunction.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct a native-function value wrapping pFunction.   @ 0x82AF0178
    explicit AptNativeFunction(AptExtFunctionPtr pFunction);

    // The wrapped native callback (read by the apt VM's call dispatch).
    AptExtFunctionPtr GetFunction() const { return mpFunction; }

protected:
    // ~AptNativeFunction has no work of its own: the embedded AptNativeHash (in the
    // AptValueWithHash base) destroys itself, and the two trailing members are POD.
    // The X360 emits only the standard deleting-destructor thunk (restore vtable ->
    // ~AptValueWithHash -> conditional operator delete); that thunk is compiler-
    // generated and intentionally not hand-written.
    virtual ~AptNativeFunction() {}

private:
    // +0x1C -- class / implemented-object flags (the same bitfield family as
    // AptObject::mClassFlags). The ctor starts it cleared. The X360 ctor only
    // partially clears the word (zero the low byte, clear bits 22-23 via
    // `rlwinm 0,10,7`) rather than zeroing it whole -- that is the optimizer's
    // codegen of a logical flags-zero-init on freshly pooled memory; modelled here
    // as the honest `mClassFlags = 0`, matching the committed AptObject precedent.
    uint32_t          mClassFlags;   // +0x1C

    // +0x20 -- the wrapped native callback (ctor arg a2).
    AptExtFunctionPtr mpFunction;    // +0x20

    AptNativeFunction();             // not defined: a callback is always required

    // Reserve room for this many native members in the base property hash. The
    // X360 ctor passes `8` (li r5, 8) to AptValueWithHash.
    static const int32_t KI_HASH_CAPACITY = 8;
};
