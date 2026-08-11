#pragma once

// ===========================================================================
// EATech Apt -- AptValueWithHash: an AptValueGC that carries a property table.
//
// The base for every AS value that has named members -- AptObject, AptArray,
// AptGlobal, AptError, AptKey, ... It embeds an AptNativeHash (string -> AptValue)
// and routes the AptValue object-model virtuals (GetNativeHashVirtual /
// Set/Lookup / __proto__ / GC mark + teardown) through it.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (16AptValueWithHash, @0x7E7C9C /
// 0x7F8650 / 0x7E2AA0 / 0x7E2AAC / 0x8042A0 / 0x7F9A68). LAYOUT (x64): AptValueGC
// base (vptr + bitfield, 0x10 bytes) + the embedded AptNativeHash mHash at +0x10
// (x64 Lookup/Set thunks @0x14083C0C0/0x140840C90: `add rcx,10h`); sizeof 0x38
// (derived first members land at +0x38: AptPrototype/AptObject).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueGC
#include "SDKs/EATech/include/Apt/AptNativeHash.h"        // the embedded property table
#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC keys

struct AptValueWithHash : public AptValueGC
{
protected:
    AptNativeHash mHash;   // x64 +0x10 -- the property table

    AptValueWithHash(AptVirtualFunctionTable_Indices eType, int nHashCapacity)
        : AptValueGC(eType), mHash(nHashCapacity)
    {
    }

public:
    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class + protected-member offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptValueWithHash, mHash) == 0x10, "x64: Lookup/Set thunks 'add rcx,10h' @0x14083C0C0/0x140840C90");
        static_assert(sizeof(AptValueWithHash) == 0x38, "x64: derived first members land at +0x38 (AptPrototype @0x140839C70, AptObject ctors)");
    }

protected:

public:
    // ---- AptValue object-model virtual overrides --------------------------
    virtual AptNativeHash* GetNativeHashVirtual()        { return &mHash; }   // @0x7E2AA0
    virtual bool           ContainsNativeHashVirtual() const { return true; } // @0x7E2AAC
    virtual void           RegisterReferences();         // @0x7E7C9C (key function)
    virtual void           DestroyGCPointers();          // @0x7F8650

    // scalar deleting destructor @0x82AF51F0 -- the GC teardown. The asm conditionally
    // (if mHash.mpTable != null, i.e. !mHash.IsEmpty()) calls mHash.DestroyGCPointers(),
    // resets the vtable, then conditionally frees. That is EXACTLY the compiler-generated
    // deleting dtor here: the inherited `virtual ~AptValue` + the member `~AptNativeHash`
    // (which is itself `if (mpTable) DestroyGCPointers()`). Defaulted -- the member dtor
    // supplies the body; declared so the override + its X360 address are documented.
    virtual ~AptValueWithHash() = default;

    // ---- property access (non-virtual; delegate to the hash) --------------
    void      Set(const EAStringC& key, AptValue* pValue) { mHash.Set(key, pValue); } // @0x8042A0
    AptValue* Lookup(const EAStringC& key) const { return mHash.Lookup(key); }        // @0x7F9A68

    AptValue* Get__Proto__() const { return mHash.Get__Proto__(); }
    AptValue* GetPrototype() const { return mHash.GetPrototype(); }
    void      Set__Proto__(AptValue* pValue) { mHash.Set__Proto__(pValue); }
    void      SetPrototype(AptValue* pValue) { mHash.SetPrototype(pValue); }
};
