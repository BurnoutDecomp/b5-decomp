// ===========================================================================
// EATech Apt -- AptError: the ActionScript Error object value.
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//     AptError::operator new          @ 0x82AE6850
//     AptError::operator delete       @ 0x82AF0BF8
//     AptError::~AptError             @ 0x82AF0DD0
//     AptError::objectMemberLookup    @ 0x82AFD8C8
//     AptError::objectMemberSet       @ 0x82AFB128
//     AptError::sMethod_toString      @ 0x82AFB228
//     AptError::CleanNativeFunctions  @ 0x82AD6328
// The `vector deleting destructor' @0x82AF5550 is a compiler thunk (~AptError ->
// conditional operator delete(this, 40)) and is dropped, not hand-written.
//
// The console's inlined character-by-character name compares (the
// `do v=*p-*q; while(!v && *p)` loops over `pName->GetBuffer()`) are the
// optimizer's expansion of strcmp; restored to strcmp here. The
// InitFromBuffer(temp,buf)+operator=+DecreaseInternalRefCount idiom is a scoped
// EAStringC temporary copied into the destination, restored to RAII strings.
//
// See AptError.h for the layout / base-class derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptError.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"     // AptValue, toString/urlEncode
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"    // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptString/EAString.h"    // EAStringC
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"     // AptNativeFunction + AptExtFunctionPtr
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"         // gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"           // AptValueGC_MemItem

#include <cstring>   // strcmp

// ---------------------------------------------------------------------------
// Statics. The lazy, GC-rooted "toString" native method shared by every Error
// instance (X360 off_8324E3B8). Null until the first "toString" lookup builds
// it; CleanNativeFunctions releases + clears it at Apt shutdown.
// ---------------------------------------------------------------------------
AptNativeFunction* AptError::spToStringFunction = nullptr;   // off_8324E3B8

// ---------------------------------------------------------------------------
// GC pool operator new / delete (X360 @0x82AE6850 / @0x82AF0BF8).
//
// AptError is a garbage-collected value, so its block comes from the GC value
// pool (off_8324D834 == gpGCPoolManager) with the AptValueGC_MemItem "allocated"
// flag flipped (byte_8324D804 == gAptValueGCSizeOffset selects the size-word
// offset) -- the identical external-allocator pattern as AptNativeFunction. The
// cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block, not a
// member poke into a live C++ object.
// ---------------------------------------------------------------------------
void* AptError::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

void AptError::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

// ---------------------------------------------------------------------------
// ctor -- an Error object with an empty property hash; the two string members
// default-construct to the shared empty string. (FLAG: no ctor body is present in
// this TU's dossier; modelled as the obvious AptObject(AptVFT_Error, 8) base init,
// consistent with the sibling object ctors.)
// ---------------------------------------------------------------------------
AptError::AptError()
    : AptObject(AptVFT_Error, 8)
    , mMessage()
    , mName()
{
}

// ---------------------------------------------------------------------------
// objectMemberLookup @0x82AFD8C8
//
// Resolve the Error's scriptable members:
//   "message" / "name" -> a fresh AptString holding that member's contents;
//   "toString"         -> the shared, lazily-built native method (GC-rooted +
//                         AddRef'd on first creation);
//   anything else      -> null.
// ---------------------------------------------------------------------------
AptValue* AptError::objectMemberLookup(AptValue* const /*pThis*/,
                                       const AptNativeString* const pName) const
{
    if (strcmp(pName->c_str(), "message") == 0)
    {
        AptString* pStr = AptString::Create("");
        *pStr->GetInternalString() = EAStringC(mMessage.GetBuffer());
        return pStr;
    }

    if (strcmp(pName->c_str(), "name") == 0)
    {
        AptString* pStr = AptString::Create("");
        *pStr->GetInternalString() = EAStringC(mName.GetBuffer());
        return pStr;
    }

    if (strcmp(pName->c_str(), "toString") != 0)
        return nullptr;

    // Lazily build the shared toString native method, root it in the GC, and take
    // the owning reference. (The console's `if(mem) ctor else 0` null-check is the
    // compiler's standard new-returns-null guard around the ctor; restored to a
    // plain new.)
    if (!spToStringFunction)
    {
        spToStringFunction = new AptNativeFunction(
            reinterpret_cast<AptExtFunctionPtr>(&AptError::sMethod_toString));
        spToStringFunction->setGCRoot(1);
        spToStringFunction->AddRef();
    }
    return spToStringFunction;
}

// ---------------------------------------------------------------------------
// objectMemberSet @0x82AFB128
//
// Store onto the Error's "message" / "name" members (stringifying the assigned
// value); ignore any other member name.
// ---------------------------------------------------------------------------
bool AptError::objectMemberSet(AptValue* const /*pThis*/,
                               const AptNativeString* const pName,
                               AptValue* const pValue)
{
    if (strcmp(pName->c_str(), "message") == 0)
    {
        EAStringC strValue;            // X360: v14 = &s_EmptyInternalData (empty)
        pValue->toString(&strValue);   // committed AptValue::toString(EAStringC*) -- body is the value-layer follow-on
        mMessage.Duplicate(strValue);
        return true;
    }

    if (strcmp(pName->c_str(), "name") == 0)
    {
        EAStringC strValue;
        pValue->toString(&strValue);   // committed AptValue::toString(EAStringC*) -- body is the value-layer follow-on
        mName.Duplicate(strValue);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// sMethod_toString @0x82AFB228
//
// The native callback backing AS Error.prototype.toString: build a new AptString
// holding pContext's url-encoded string form. (Both the urlEncode result and the
// buffer-copy temporary are scoped EAStringCs the console releases via
// DecreaseInternalRefCount.)
// ---------------------------------------------------------------------------
AptValue* AptError::sMethod_toString(AptValue* pContext)
{
    AptString* pResult = AptString::Create("");
    EAStringC  strUrl  = pContext->urlEncodeCustomRender();   // @0x82AF9410 (committed decl; body is the value-layer follow-on)
    *pResult->GetInternalString() = EAStringC(strUrl.GetBuffer());
    return pResult;
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @0x82AD6328
//
// Release the shared lazy toString native method at Apt shutdown
// (AptUpdateShutdown). The console calls vtable slot +4 (Release) on the value.
// ---------------------------------------------------------------------------
void AptError::CleanNativeFunctions()
{
    if (spToStringFunction)
    {
        spToStringFunction->Release();
        spToStringFunction = nullptr;
    }
}
