#pragma once

// ===========================================================================
// EATech Apt -- AptGlobalExtensionObject: the AS "_global extension" object.
//
// AptGlobalExtensionObject : AptObject -- the registry helper that backs the
// ActionScript _global extension namespace. It is a garbage-collected scriptable
// object (so it carries the property hash + prototype it inherits from AptObject)
// that the Apt runtime brings up as a GC root (its ctor calls setGCRoot(1)); the
// host registers/unregisters native extension members on it via Set / UnSet.
//
// SHAPE + BODIES from the X360 ARTIST.XEX pseudocode/asm (the authoritative
// spine):
//     AptGlobalExtensionObject::AptGlobalExtensionObject @ 0x82AF0688
//     AptGlobalExtensionObject::operator new             @ 0x82AE6588
//     AptGlobalExtensionObject::operator delete          @ 0x82AF06E8
//     AptGlobalExtensionObject::Set                      @ 0x82AF5488
//     AptGlobalExtensionObject::UnSet                    @ 0x82AECD20
// (The `vector deleting destructor' @0x82AF0750 is the compiler thunk for
// delete / delete[]; it is dropped -- `delete` codegens it from operator delete
// + ~AptGlobalExtensionObject. Its body confirms the destructor adds nothing
// over AptObject::~AptObject and pins sizeof == 0x20 (operator delete(this,32)).)
//
// LAYOUT: AptObject (32 bytes) + no new members = 32 bytes. The ctor passes
// AptVFT_GlobalExtension (30) + a hash capacity of 8 to the AptValueWithHash base.
//
// AptGlobalExtensionObject is a GC value (AptValueGC base via AptObject), so --
// like AptArray and unlike the non-GC leaves (AptInteger/AptFloat/...) -- it
// allocates from the GC pool (gpGCPoolManager) rather than gpNonGCPoolManager.
// The new/delete bodies live in the .cpp so the header need not pull in the
// pool-manager type (avoids the include cycle with AptDefine.h /
// AptValueGCPoolManager.h, which both include AptValue.h -- same reason as
// AptArray).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC keys

struct AptValue;

struct AptGlobalExtensionObject : public AptObject
{
    // GC-pool allocation (see header note). operator new @0x82AE6588 reaches
    // gpGCPoolManager->AllocateAptValueGC(size); operator delete @0x82AF06E8
    // reaches gpGCPoolManager->DeallocateAptValueGC(p, size).
    static void* operator new(size_t size);
    static void  operator delete(void* p, size_t size);

    AptGlobalExtensionObject();          // @0x82AF0688
    virtual ~AptGlobalExtensionObject(); // (dtor: base teardown only; vector
                                         //  deleting destructor thunk @0x82AF0750)

    // Register / unregister a native extension member by name. Thin forwarders
    // to the inherited property hash (mHash). @0x82AF5488 / @0x82AECD20.
    void Set(const EAStringC& key, AptValue* pValue);   // @0x82AF5488
    void UnSet(const EAStringC& key);                   // @0x82AECD20
};
