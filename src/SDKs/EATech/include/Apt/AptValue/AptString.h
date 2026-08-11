#pragma once

// ===========================================================================
// EATech Apt -- AptString: the string ActionScript value (the AS "primitive"
// string; the boxed "String" object is the separate AptStringObject / vtbl
// index 0x21).
//
// SHAPE verbatim from the Feb-2007 leak (AptValue/AptString.h): a non-GC value
// wrapping an EAStringC (typedef AptNativeString) plus a free-list link
// (mpNext); pooled through gpNonGCPoolManager and recycled by a StringPool. The
// embedded EAStringC sits immediately after the AptValueNoGC base, so
// AptValue::c_string() can hand the conversion layer the string contents.
//
// SCOPE: the data layout + the inline accessors the value-conversion layer needs
// (GetInternalString / the pool new/delete). The pooled lifecycle (Create
// @0x82AEDC18 / Destroy @0x82AE8218), objectMemberLookup (the gperf
// StringMembersIndex recognizer) and the ActionScript String methods
// (sMethod_charAt/indexOf/split/...) are homed in AptString.cpp; the StringPool
// recycler itself (Initialize/GetFromPool/Teardown/...) is homed in
// AptStringPool.cpp. The conversions only READ str, so the layout +
// GetInternalString is what is required here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // EAStringC / AptNativeString
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

class StringPool;   // the recycler that owns the free list (mpNext); homed in AptStringPool.cpp

class AptString : public AptValueNoGC
{
public:
    static void* operator new(size_t size)              { return gpNonGCPoolManager->Allocate(size); }
    static void  operator delete(void* p, size_t size)  { gpNonGCPoolManager->Deallocate(p, size); }
    static void* operator new[](size_t size)            { return AptNonGCAllocSaveSize(size); }
    static void  operator delete[](void* p)             { AptValueNoGC::VerifyAptValueNoGC(); AptNonGCFreeSavedSize(p); }

    // leak: hand back the embedded EAStringC (the value-conversion + String-method
    // layers read/parse it).
    AptNativeString* GetInternalString() { return &str; }

    // leak: pooled construction (recycled via the StringPool free list). Body in
    // AptString.cpp (@0x82AEDC18); the StringPool recycler is AptStringPool.cpp.
    static AptString* Create(const char* szValue);
    static AptString* Create() { return Create(""); }

    // CleanNativeFunctions @0x82AD7A78 -- drop (Release) every cached AS String
    // native-method value (the 13 globals lazily built by objectMemberLookup) at
    // Apt shutdown, nulling each cache slot. Called by AptUpdateShutdown.
    static void CleanNativeFunctions();

    // cat @0x82AECD10 / cpy @0x82AE64E8 -- the engine's string-value mutators: the
    // embedded EAStringC `str += rhs` / `str = rhs` (the X360 tail-calls
    // EAStringC::operator+= / operator= on &str). Return the mutated string.
    EAStringC& cat(const EAStringC& strText) { return str += strText; }
    EAStringC& cpy(const EAStringC& strText) { return str =  strText; }

    // ---- ActionScript String native methods (sMethod_*) -- bodies in AptString.cpp.
    // Static natives (AptExtFunctionPtr): the VM calls f(thisString[, argCount]);
    // AS arguments come off the global native-arg stack. (slice/substring/split/cat
    // are a follow-on -- they need the UTF8 substring-extract helper homed.)
    static AptValue* sMethod_charAt(AptString* pThis);
    static AptValue* sMethod_charCodeAt(AptString* pThis);
    static AptValue* sMethod_concat(AptString* pThis, int nArgCount);
    static AptValue* sMethod_fromCharCode(AptString* pThis, int nArgCount);
    static AptValue* sMethod_indexOf(AptString* pThis, int nArgCount);
    static AptValue* sMethod_lastIndexOf(AptString* pThis, int nArgCount);
    static AptValue* sMethod_toLowerCase(AptString* pThis);
    static AptValue* sMethod_toUpperCase(AptString* pThis);
    static AptValue* sMethod_slice(AptString* pThis, int nArgCount);
    static AptValue* sMethod_substring(AptString* pThis, int nArgCount);
    static AptValue* sMethod_split(AptString* pThis, int nArgCount);
    // sMethod_substr (the legacy AS String.substr(start, length)) -- X360
    // @0x82AFCE5C / PS3 DecFIGS 0xF41074; a separate symbol from the slice/
    // substring pair. Body in AptString.cpp (decompiled from the PS3 export).
    static AptValue* sMethod_substr(AptString* pThis, int nArgCount);

protected:
    virtual void DeleteThis()  { Destroy(); }
    virtual void ForceDelete() { Destroy(); }
    void Destroy();   // @0x82AE8218 -- push back onto the StringPool free list (AptString.cpp)

    // leak: resolve a JS member/method name through the gperf StringMembersIndex
    // recognizer. Body in AptString.cpp (@0x82AFCAB0, with the method-value caches).
    virtual AptValue* objectMemberLookup(AptValue* const pContext,
                                         const AptNativeString* const pName) const;

    void        SetNext(AptString* const pNext) { mpNext = pNext; }
    AptString*  GetNext() const                 { return mpNext; }

    virtual ~AptString() {}

private:
    AptString()
        : AptValueNoGC(AptVFT_StringValue)
        , str()
        , mpNext(0)
    {
    }

    explicit AptString(const char* szValue)
        : AptValueNoGC(AptVFT_StringValue)
        , str(szValue)
        , mpNext(0)
    {
    }

    // IsConstantString @0x82AD5370 -- whether the embedded string is a constant
    // (its StringDataC m_uSize exceeds m_uMaxSize: the X360 reads str.m_pData and
    // compares maxsize < size, exactly EAStringC::IsConstantString).
    bool IsConstantString() { return str.IsConstantString(); }

    // ---- layout (leak AptString.h) -- the embedded string + the free-list link.
    AptNativeString str;       // the EAStringC contents
    AptString*      mpNext;    // StringPool free-list link while recycled

    enum { MAX_SIZE_KEPT_TEMP_STRING = 32 };

    friend class StringPool;
};
