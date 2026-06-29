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
// (GetInternalString / the pool new/delete). The pooled lifecycle (Create /
// Destroy / the StringPool), objectMemberLookup (the gperf StringMembersIndex
// recognizer in the Packages tree), and the ActionScript String methods
// (sMethod_charAt/indexOf/split/...) need StringPool + AptNativeFunction and are
// the follow-on -- declared where cheap, FLAG'd otherwise. The conversions only
// READ str, so the layout + GetInternalString is what is required here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // EAStringC / AptNativeString
#include "SDKs/EATech/include/Apt/AptDefine.h"           // gpNonGCPoolManager + AptNonGC*SaveSize
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

class StringPool;   // the recycler that owns the free list (mpNext); follow-on

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

    // leak: pooled construction (recycled via the StringPool free list). FLAG:
    // bodies + the StringPool are the follow-on; declared so callers can name them.
    static AptString* Create(const char* szValue);
    static AptString* Create() { return Create(""); }

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

protected:
    virtual void DeleteThis()  { Destroy(); }
    virtual void ForceDelete() { Destroy(); }
    void Destroy();   // FLAG: returns the value to the StringPool free list (follow-on)

    // leak: resolve a JS member/method name through the gperf StringMembersIndex
    // recognizer. FLAG: body is the follow-on (needs the native-method table).
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

    bool IsConstantString();   // FLAG: follow-on

    // ---- layout (leak AptString.h) -- the embedded string + the free-list link.
    AptNativeString str;       // the EAStringC contents
    AptString*      mpNext;    // StringPool free-list link while recycled

    enum { MAX_SIZE_KEPT_TEMP_STRING = 32 };

    friend class StringPool;
};
