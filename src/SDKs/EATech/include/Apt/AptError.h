#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptError.
//
// The ActionScript "Error" object value: a garbage-collected scriptable object
// (so it has a property hash + a prototype, via AptObject) that additionally
// carries two dedicated string members -- its "message" and "name" -- and a lazy
// native "toString" method that renders them.
//
// BASE: AptObject. The X360 destructor (~AptError @0x82AF0DD0) releases the two
// trailing EAStringC members and then tail-calls AptObject::~AptObject, so
// AptError IS-A AptObject (a property-bearing AptValueGC). The vtable object-type
// index is AptVFT_Error (32).
//
// SHAPE recovered from the X360 ARTIST.XEX (no Feb-2007 / DecFIGS header is in
// scope for this TU):
//     AptError::operator new              @ 0x82AE6850
//     AptError::operator delete           @ 0x82AF0BF8
//     AptError::~AptError                 @ 0x82AF0DD0
//     AptError::objectMemberLookup        @ 0x82AFD8C8
//     AptError::objectMemberSet           @ 0x82AFB128
//     AptError::sMethod_toString          @ 0x82AFB228
//     AptError::CleanNativeFunctions      @ 0x82AD6328
//     AptError::`vector deleting destructor' @ 0x82AF5550 (compiler thunk; dropped)
//
// LAYOUT (sizeof = 40 / 0x28, pinned by `operator delete(this, 40)` in the
// deleting-destructor thunk):
//     AptObject base ........... 32 bytes (vtable + bitfield + AptNativeHash +
//                                          mClassFlags)
//     mMessage EAStringC ....... +0x20   (the "message" property, a3[8])
//     mName    EAStringC ....... +0x24   (the "name" property,    a3[9])
// The destructor releases the members in reverse declaration order (mName then
// mMessage), matching the asm (`lwz r3,0x24` released before `lwz r3,0x20`).
//
// This is vendor/SDK code reconstructed in its canonical home. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers (AptError, objectMemberLookup,
// sMethod_toString, the AptVFT_* index, ...) are an external/middleware API and
// kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptObject.h"             // AptObject base
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC members (AptNativeString)

class AptValue;            // SDKs/EATech/include/Apt/AptValue/AptValue.h (base, via AptObject)
class AptNativeFunction;   // SDKs/EATech/include/Apt/AptNativeFunction.h (the lazy toString method)

struct AptError : public AptObject
{
    // ---- GC pool new / delete (X360 operator new @0x82AE6850 / delete
    // @0x82AF0BF8). AptError is a garbage-collected value (AptObject -> ... ->
    // AptValueGC), so its block comes from the GC pool (gpGCPoolManager) with the
    // AptValueGC_MemItem "allocated" flag flipped -- the same GC-value pattern as
    // AptNativeFunction / AptArray. Defined out-of-line in AptError.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // Construct an Error object. The two string members default-construct to the
    // shared empty string. FLAG: no ctor body is present in this TU's dossier (the
    // ctor is reached only indirectly -- AptActionInterpreter::_createObject calls
    // operator new); modelled as the obvious AptObject(AptVFT_Error, ...) base init
    // with empty members, consistent with the sibling object ctors.
    AptError();

    // dtor @0x82AF0DD0 -- releases mName then mMessage (the two EAStringC member
    // destructors) and chains to ~AptObject. With RAII members + the AptObject base
    // the compiler emits exactly that sequence, so the body is empty.
    virtual ~AptError() {}

    // ---- AptValue object-model virtual overrides --------------------------
    // Resolve / store the Error's scriptable members. "message" and "name" map to
    // the two string members; "toString" (lookup only) returns the lazy native
    // method. Everything else falls through (lookup -> null, set -> false).
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;   // @0x82AFD8C8
    virtual bool      objectMemberSet(AptValue* const pThis,
                                      const AptNativeString* const pName,
                                      AptValue* const pValue);                        // @0x82AFB128

    // Release the shared lazy "toString" native-function value at Apt shutdown
    // (AptUpdateShutdown calls it). @0x82AD6328
    static void CleanNativeFunctions();

private:
    // The native callback backing the AS Error.prototype.toString method: render
    // the value as its url-encoded string. Its address is handed to
    // AptNativeFunction's ctor. @0x82AFB228
    static AptValue* sMethod_toString(AptValue* pContext);

    // ---- layout (X360) -- the two dedicated string properties.
    EAStringC mMessage;   // +0x20 -- the "message" property
    EAStringC mName;      // +0x24 -- the "name" property

    // The lazily-created, GC-rooted native method shared by every Error instance's
    // "toString" lookup (X360 off_8324E3B8). Null until the first lookup builds it;
    // CleanNativeFunctions releases + clears it at shutdown.
    static AptNativeFunction* spToStringFunction;   // off_8324E3B8
};
