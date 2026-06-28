#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptMovieClip.
//
// AptMovieClip : AptObject -- the scriptable MovieClip object value. It is the
// ActionScript wrapper a clip is exposed to script as; a garbage-collected,
// property-bearing AS object (so it carries the property hash + prototype +
// class-flags word it inherits from AptObject), distinguished only by its value
// type index AptVFT_MovieClip (22). AptActionInterpreter::_createObject builds
// one (it is the type the interpreter instantiates for a MovieClip value).
//
// SHAPE + BODIES from the X360 ARTIST.XEX pseudocode/asm (the authoritative
// spine):
//     AptMovieClip::AptMovieClip                  @ 0x82AF3128
//     AptMovieClip::operator new                  @ 0x82AE91A0
//     AptMovieClip::~AptMovieClip                 @ 0x82AF31D8
//     AptMovieClip::`vector deleting destructor'  @ 0x82AF31E8  (compiler thunk -- dropped)
//
// The `vector deleting destructor' is the compiler thunk for delete / delete[];
// it is dropped -- `delete` codegens it from operator delete + ~AptMovieClip.
// Its body confirms the destructor adds nothing over AptObject::~AptObject and
// pins sizeof == 0x20 (operator delete(this, 32) / `li r4, 0x20`).
//
// LAYOUT: AptObject (32 bytes) + no new members = 32 bytes (0x20). The ctor
// passes AptVFT_MovieClip (22) + a hash capacity of 8 to the AptValueWithHash
// base via AptObject; the only field it touches is the inherited mClassFlags
// word at +0x1C (cleared by the AptObject base ctor -- see the .cpp note on the
// no-op clear-of-bits-22,23 idiom the X360 inlines there).
//
// AptMovieClip is a GC value (AptValueGC base via AptObject), so -- like AptArray
// / AptPrototype / AptGlobalExtensionObject and unlike the non-GC leaves
// (AptInteger/AptFloat/...) -- it allocates from the GC pool (gpGCPoolManager)
// rather than gpNonGCPoolManager. The new/delete bodies live in the .cpp so the
// header need not pull in the pool-manager type (avoids the include cycle with
// AptDefine.h / AptValueGCPoolManager.h, which both include AptValue.h -- same
// reason as AptArray / AptGlobalExtensionObject).
//
// SCOPE: the value-object core -- the GC-pool new/delete, the ctor, and the
// (empty) destructor. The clip's behavioural surface (the objectMember* native
// members, the AS MovieClip methods) is not part of this TU -- the X360 exports
// only these four entries for the class, so the inherited AptObject /
// AptValueWithHash object-model virtuals are correct as-is (an honest minimal
// owning header rather than fabricated overrides).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptObject.h"   // AptObject base (transitively:
                                                 // AptValueWithHash, AptValueGC,
                                                 // AptValue + AptVFT_MovieClip)

struct AptMovieClip : public AptObject
{
    // ---- GC pool new / delete (X360 operator new @0x82AE91A0) -------------
    // AptMovieClip is a garbage-collected value, so its block comes from the GC
    // value pool (gpGCPoolManager) and operator new flips the AptValueGC_MemItem
    // "allocated" flag -- exactly like the rest of the GC value family
    // (AptArray / AptPrototype / AptGlobalExtensionObject). operator new reaches
    // gpGCPoolManager->AllocateAptValueGC(size); operator delete reaches
    // gpGCPoolManager->DeallocateAptValueGC(p, size). Defined out-of-line in
    // AptMovieClip.cpp.
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    // @0x82AF3128 -- construct an empty MovieClip object (AptVFT_MovieClip, hash
    // capacity 8). The X360 ctor only constructs the base + clears the inherited
    // class-flags word; it brings up no GC-root mark and disables nothing (unlike
    // AptGlobalExtensionObject), so the user body is empty.
    AptMovieClip();

    // @0x82AF31D8 -- base teardown only: the X360 destructor resets the vtable
    // then chains to AptObject::~AptObject (which releases the property hash). The
    // MovieClip adds nothing of its own, so the body is empty. The `vector
    // deleting destructor' thunk @0x82AF31E8 is compiler-generated and dropped.
    virtual ~AptMovieClip();   // @0x82AF31D8
};
