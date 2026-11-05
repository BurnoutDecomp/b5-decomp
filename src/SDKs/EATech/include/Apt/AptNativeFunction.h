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
// The ARTIST table at 0x82145A14 inherits the class methods from AptObject.
// The constructor inlines the AptObject base constructor, and the deleting
// destructor calls AptObject::~AptObject.
//
// LAYOUT (sizeof = 36 / 0x24, pinned by `operator delete(this, 36)` in the thunk):
//   AptObject base ................... 32 bytes
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
#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"                    // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"                // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                  // AptValueGC_MemItem (alloc bookkeeping)

// The native-callback pointer type (leak AptExtObject.h:116). A bare `void*` in
// the SDK: the apt VM stores it opaquely and the dispatch layer casts it to the
// concrete call signature at invocation time.
typedef void* AptExtFunctionPtr;

class AptNativeFunction : public AptObject
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
    // The deleting destructor calls the AptObject base destructor.
    virtual ~AptNativeFunction() {}

private:
    // +0x20 -- the wrapped native callback (ctor arg a2).
    AptExtFunctionPtr mpFunction;    // +0x20

    AptNativeFunction();             // not defined: a callback is always required

    // Reserve room for this many native members in the base property hash. The
    // X360 ctor passes `8` (li r5, 8) to AptValueWithHash.
    static const int32_t KI_HASH_CAPACITY = 8;
};
