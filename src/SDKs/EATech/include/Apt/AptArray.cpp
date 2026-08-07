// ===========================================================================
// EATech Apt -- AptArray storage core.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctors 0x82D978/0x82D728 / dtor 0x82EA7C / _reserve 0x7F01D0 / length 0x7DF380
//   / GetAt 0x7E094C / get 0x7E14F4 / SetAt 0x7E11B0 / set 0x7F0328 /
//   RegisterReferences 0x7E7CB0 / DestroyGCPointers 0x7F8660.
//
// The element vector holds ref-counted AptValue*s (SetAt AddRef's the new value /
// Release's the old via the AptValue vtable). _reserve grows to a power of two
// (min 8). Reconstructed with sizeof(AptValue*) strides (x64-width) rather than
// the console's literal 4-byte pointers.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptArray.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"  // sMethod_join boxes the result into an AptString
#include "SDKs/EATech/include/Apt/AptDefine.h"        // gpNonGCPoolManager / gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"    // AptValueGC_PoolManager::AllocateAptValueGC / DeallocateAptValueGC
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <cstring>   // memcpy

// GC-pool operator new/delete @0x82AE6088 -- AptArray is a garbage-collected value
// (AptValueGC base), so its block comes from the GC pool (gpGCPoolManager), the GC
// analogue of the non-GC leaves' gpNonGCPoolManager route. AllocateAptValueGC =
// DOGMA Allocate + AptValueGC_MemItem::SetIsAllocated (the X360 inlines that pair
// into every GC type's operator new). Reached as AptArray::operator new(44) from
// AptValueFactory::CreateArray. (gpGCPoolManager is wired by AptInit's
// AptAllocatorInitialize @0x82ADD118; the null guard covers the pre-init window.)
void* AptArray::operator new(size_t size)
{
    return (gpGCPoolManager != nullptr) ? gpGCPoolManager->AllocateAptValueGC(size) : nullptr;
}

void AptArray::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != nullptr)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ctor @0x82D978 -- empty array (object hash capacity 8).
AptArray::AptArray() : AptObject(AptVFT_Array, 8)
{
    mpArray    = nullptr;
    mnCapacity = 0;
    mnLength   = 0;
}

// ctor @0x82D728 -- array of nCount given elements.
AptArray::AptArray(int nCount, AptValue** ppItems) : AptObject(AptVFT_Array, nCount)
{
    mpArray    = nullptr;
    mnCapacity = 0;
    mnLength   = nCount;
    _reserve(nCount);
    for (int i = 0; i < nCount; ++i)
        SetAt(i, ppItems[i]);
}

// dtor @0x82EA7C -- release any remaining elements + free the vector. (The object
// property hash is released by the AptValueWithHash member destructor.)
AptArray::~AptArray()
{
    if (mpArray)
    {
        for (int i = 0; i < mnLength; ++i)
        {
            if (mpArray[i])
            {
                mpArray[i]->Release();
                mpArray[i] = nullptr;
            }
        }
        gpNonGCPoolManager->Deallocate(mpArray, sizeof(AptValue*) * mnCapacity);
        mpArray = nullptr;
    }
    mnCapacity = 0;
    mnLength = 0;
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' @0x82AF5C88 -- compiler-generated thunk
// (~AptArray, then operator delete when the low bit of the flags arg is set:
// resets the AptArray vtable, chains the AptObject base teardown, then frees
// the 44-byte block via the GC-pool operator delete(this, 44) above).
// Intentionally NOT hand-written: the C++ compiler emits the deleting
// destructor for AptArray from the dtor above + the (pool-backed) operator
// delete, exactly as for the sibling Apt value/instance types.
// ---------------------------------------------------------------------------

// _reserve @0x7F01D0 -- ensure capacity for nCount, growing to a power of two
// (minimum 8) and copying the existing elements.
void AptArray::_reserve(int32_t nCount)
{
    if (mnCapacity >= nCount)
        return;

    int newCap = 1;
    int v = nCount - 1;
    if (v)
    {
        int bits = 0;
        do { v >>= 1; ++bits; } while (v);
        newCap = 1 << bits;
    }
    if (newCap < 8)
        newCap = 8;

    AptValue** newArray =
        static_cast<AptValue**>(gpNonGCPoolManager->Allocate(sizeof(AptValue*) * newCap));
    if (mpArray)
    {
        memcpy(newArray, mpArray, sizeof(AptValue*) * mnCapacity);
        gpNonGCPoolManager->Deallocate(mpArray, sizeof(AptValue*) * mnCapacity);
    }
    for (int i = mnCapacity; i < newCap; ++i)
        newArray[i] = gpUndefinedValue;   // X360 fills grown slots with the undefined singleton (off_8324D814), not null

    mpArray    = newArray;
    mnCapacity = newCap;
}

// ---- element access -------------------------------------------------------
AptValue* AptArray::GetAt(int32_t nIndex) const
{
    return (nIndex < mnLength) ? mpArray[nIndex] : gpUndefinedValue;
}

AptValue* AptArray::get(int32_t nIndex) const
{
    if (nIndex < 0 || nIndex >= mnLength)
        return gpUndefinedValue;
    AptValue* p = mpArray[nIndex];
    return p ? p : gpUndefinedValue;
}

// SetAt @0x7E11B0 -- store at an existing slot (AddRef new, Release old).
void AptArray::SetAt(int32_t nIndex, AptValue* pValue)
{
    AptValue* pOld = mpArray[nIndex];
    pValue->AddRef();
    if (pOld)
        pOld->Release();
    mpArray[nIndex] = pValue;
}

// set @0x7F0328 -- store at nIndex, growing + extending the length as needed.
void AptArray::set(int32_t nIndex, AptValue* pValue)
{
    if (nIndex < 0)
        return;
    _reserve(nIndex + 1);
    if (nIndex + 1 > mnLength)
        mnLength = nIndex + 1;
    SetAt(nIndex, pValue);
}

// ---- GC -------------------------------------------------------------------
void AptArray::RegisterReferences()
{
    AptObject::RegisterReferences();   // mark the property hash
    if (!AptValue::sReferenceRegistrationCb)
        return;
    for (int i = 0; i < mnLength; ++i)
        if (mpArray[i])
            AptValue::sReferenceRegistrationCb(this, &mpArray[i], "Element", 0);
}

void AptArray::DestroyGCPointers()
{
    AptObject::DestroyGCPointers();    // tear down the property hash
    for (int i = 0; i < mnLength; ++i)
    {
        if (mpArray[i])
        {
            mpArray[i]->Release();
            mpArray[i] = nullptr;
        }
    }
    if (mpArray)
        gpNonGCPoolManager->Deallocate(mpArray, sizeof(AptValue*) * mnCapacity);
    mnCapacity = 0;
    mpArray    = nullptr;
    mnLength   = 0;
}

// ===========================================================================
// ActionScript Array native methods (sMethod_*) + the default sort comparator.
// Reconstructed from the X360 ARTIST pseudocode/asm (decompile->verify workflow);
// AptExtFunctionPtr natives the VM calls as f(thisArray[, argCount]), AS args off
// the global native-arg stack. Reuse the homed storage accessors (length/get/
// GetAt/set/SetAt/_reserve); the raw mpArray slot moves (pop/reverse/unshift/
// splice) are deliberate -- they transfer element ownership without touching
// refcounts, exactly as the console does. (sort/sortOn/join + scriptFunctionSort
// Func are a follow-on -- they need the sort-state globals, AptArray::toString,
// and the interpreter call path homed.)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"   // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"     // AptNativeFunction (lazy native methods)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"         // AptNativeHash::Lookup (sortOn member fetch)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"  // the global VM (scriptFunctionSortFunc)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h" // PushStaticData + GetCIH (the sort-comparator VM call)
// The global VM instance (off_8324E760), defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

#include <cstdlib>   // qsort / atoi / strtol

// The global native-method arg stack, defined in AptGlobals.cpp and published by
// the interpreter's native-call dispatch (AptActionInterpreterInterpHelpers.cpp
// saves/installs X360 off_8324E768 = gAptActionInterpreter.mpStack, dword_8324E760
// = its mnStackTop around each native call). The i-th AS argument (i=0 = last pushed) is
// gppAptNativeArgStack[gnAptNativeArgCount - 1 - i].
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// @0x82AED118 -- Array.prototype.push: append every argument to the end of the
// array and return the new length (boxed as an AptInteger).
AptValue* AptArray::sMethod_push(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    for (int i = 0; i < nArgCount; ++i)
        pThis->set(pThis->length(), gppAptNativeArgStack[gnAptNativeArgCount - 1 - i]);

    return AptInteger::Create(pThis->length());
}

// @0x82AD6C50 -- Array.prototype.pop: remove and return the last element. The
// popped value's reference is handed to the caller (no Release), and its slot is
// cleared so the shortened array does not double-own it.
AptValue* AptArray::sMethod_pop(AptArray* pThis)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    if (pThis->length() <= 0)
        return gpUndefinedValue;

    const int nLast = pThis->length() - 1;

    AptValue* pElement = pThis->mpArray[nLast];
    if (!pElement)
        pElement = gpUndefinedValue;

    pThis->mnLength = nLast;
    pThis->mpArray[nLast] = nullptr;   // transfer ref to caller -- do NOT Release

    return pElement;
}

// @0x82AD6D98 -- Array.prototype.reverse: reverse the elements in place and return
// the (now reversed) array. Pure pointer swap -- ownership stays inside the array,
// so no AddRef/Release.
AptValue* AptArray::sMethod_reverse(AptArray* pThis)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    const int nHalf = pThis->length() / 2;
    for (int i = 0; i < nHalf; ++i)
    {
        const int j = pThis->length() - 1 - i;
        AptValue* pTmp        = pThis->mpArray[i];
        pThis->mpArray[i]     = pThis->mpArray[j];
        pThis->mpArray[j]     = pTmp;
    }

    return pThis;
}

// sMethod_unshift @0x82AED1B8 -- AS Array.unshift(...): prepend the call's
// arguments to the front of the array (shifting the existing elements up) and
// return the new length. The front slot gets the top-of-stack argument.
AptValue* AptArray::sMethod_unshift(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    pThis->_reserve(nArgCount + pThis->mnLength);
    if (nArgCount != 0)
    {
        // Slide the existing elements up to make room at the front.
        memmove(pThis->mpArray + nArgCount, pThis->mpArray,
                sizeof(AptValue*) * pThis->mnLength);
        pThis->mnLength += nArgCount;

        for (int i = 0; i < nArgCount; ++i)
        {
            // The memmove duplicated this pointer into a higher slot; clear the
            // front slot so set()'s SetAt does not Release the live element.
            pThis->mpArray[i] = nullptr;
            pThis->set(i, gppAptNativeArgStack[gnAptNativeArgCount - 1 - i]);
        }
    }

    return AptInteger::Create(pThis->mnLength);
}

// @0x82AF21E0 -- Array.prototype.slice: return a new array with the elements in the
// half-open range [start, end). Negative indices count from the end; end defaults
// to the array length.
AptValue* AptArray::sMethod_slice(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    int nStart = 0;
    int nEnd   = pThis->length();

    if (nArgCount > 0)
    {
        nStart = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();
        if (nStart < 0)
            nStart = pThis->length() + nStart;
    }

    if (nArgCount > 1)
    {
        nEnd = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();
        if (nEnd < 0)
            nEnd = pThis->length() + nEnd;
        else if (nEnd > pThis->length())
            nEnd = pThis->length();
    }

    if (nStart > nEnd || nStart < 0 || nEnd < 0)
        return gpUndefinedValue;

    AptArray* pResult = new AptArray();
    for (int i = nStart; i < nEnd; ++i)
        pResult->set(pResult->length(), pThis->GetAt(i));

    return pResult;
}

// @0x82AF1F40 -- Array.prototype.splice: remove `deleteCount` elements starting at
// `start`, optionally insert the remaining arguments in their place, and return a
// new array of the removed elements.
AptValue* AptArray::sMethod_splice(AptArray* pThis, int nArgCount)
{
    // Requires an array and a defined start argument.
    if (!pThis->isArray()
        || nArgCount <= 0
        || !gppAptNativeArgStack[gnAptNativeArgCount - 1]->getIsDefined())
        return gpUndefinedValue;

    // start (clamped into [0, length]).
    int nStart = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();
    if (nStart < 0)
    {
        nStart = pThis->length() + nStart;
        if (nStart < 0)
            nStart = 0;
    }
    if (nStart >= pThis->length())
        nStart = pThis->length();

    // deleteCount (default = to-the-end; clamped to what is available).
    int nDeleteCount = pThis->length() - nStart;
    if (nArgCount > 1)
    {
        if (!gppAptNativeArgStack[gnAptNativeArgCount - 2]->getIsDefined())
            return gpUndefinedValue;
        nDeleteCount = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();
        if (nDeleteCount > pThis->length() - nStart)
            nDeleteCount = pThis->length() - nStart;
    }
    if (nDeleteCount < 0)
        return gpUndefinedValue;

    AptArray* pRemoved = new AptArray();

    if (nDeleteCount > 0)
    {
        // Collect the removed elements. Each is AddRef'd to keep it alive through the
        // memmove below, which drops it from the source without a Release.
        for (int k = 0; k < nDeleteCount; ++k)
        {
            AptValue* pElement = pThis->GetAt(nStart + k);
            pRemoved->set(pRemoved->length(), pElement);
            if (pElement)
                pElement->AddRef();
        }

        // Close the hole: shift the surviving tail left over the deleted range.
        memmove(pThis->mpArray + nStart,
                pThis->mpArray + nStart + nDeleteCount,
                sizeof(AptValue*) * (pThis->length() - nDeleteCount - nStart));

        // The now-duplicated tail slots are cleared (they alias values still owned
        // after the move, so no Release here).
        for (int k = 0; k < nDeleteCount; ++k)
            pThis->mpArray[pThis->length() - nDeleteCount + k] = nullptr;

        pThis->mnLength -= nDeleteCount;
    }

    // Insertion: any arguments beyond start/deleteCount are spliced in at `start`.
    if (nArgCount > 2)
    {
        const int nInsertCount = nArgCount - 2;
        pThis->_reserve(pThis->length() + nInsertCount);

        // Open a gap by shifting the suffix right.
        const int nTail = pThis->length() - nStart;
        if (nTail > 0)
            memmove(pThis->mpArray + nStart + nInsertCount,
                    pThis->mpArray + nStart,
                    sizeof(AptValue*) * nTail);

        pThis->mnLength += nInsertCount;

        for (int k = 0; k < nInsertCount; ++k)
        {
            // Clear the aliased slot first so set()'s SetAt does not Release a value
            // that was just memmoved (and is still owned at its new home).
            pThis->mpArray[nStart + k] = nullptr;
            pThis->set(nStart + k,
                       gppAptNativeArgStack[gnAptNativeArgCount - k - 3]);
        }
    }

    return pRemoved;
}

// @0x82AF1DB8 -- Array.prototype.concat: copy this array's elements into a fresh
// array, then append each argument (spreading argument arrays element-by-element,
// non-array arguments whole).
AptValue* AptArray::sMethod_concat(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    AptArray* pResult = new AptArray();   // X360: operator new(44) + inlined default ctor

    // 1) the receiver's own elements.
    for (int i = 0; i < pThis->length(); ++i)
        pResult->set(pResult->length(), pThis->GetAt(i));

    // 2) the arguments, top of the native arg stack first.
    for (int nArg = 0; nArg < nArgCount; ++nArg)
    {
        AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1 - nArg];
        if (pArg->isArray())
        {
            AptArray* pArgArray = static_cast<AptArray*>(pArg);
            for (int j = 0; j < pArgArray->length(); ++j)
                pResult->set(pResult->length(), pArgArray->GetAt(j));
        }
        else
        {
            pResult->set(pResult->length(), pArg);
        }
    }

    return pResult;
}

// defaultSortCompareFunc @0x82AE6FC8 -- the default (no compare function) qsort
// comparator: coerce both elements to strings and compare them lexically.
int AptArray::defaultSortCompareFunc(AptValue* const* ppA, AptValue* const* ppB)
{
    EAStringC scratchA;
    EAStringC scratchB;
    const EAStringC* pStrA = AptValue::Get_ToString(*ppA, &scratchA);
    const EAStringC* pStrB = AptValue::Get_ToString(*ppB, &scratchB);

    return strcmp(pStrA->GetBuffer(), pStrB->GetBuffer());
}

// ---------------------------------------------------------------------------
// toString @0x82AED040 -- render the array as separator-joined element strings into
// pOut (the join() backing helper). Each element is coerced to a string via
// AptValue::Get_ToString; the separator goes BETWEEN elements (not after the last).
// (A null slot contributes no text but still gets a following separator.)
// ---------------------------------------------------------------------------
void AptArray::toString(EAStringC* pOut, const char* pSeparator) const
{
    *pOut = EAStringC("");   // X360 seeds pOut from the empty-string literal
    for (int i = 0; i < mnLength; ++i)
    {
        AptValue* pElem = mpArray[i];
        if (pElem)
        {
            EAStringC strScratch;
            *pOut += *AptValue::Get_ToString(pElem, &strScratch);
        }
        if (i < mnLength - 1)
            *pOut += pSeparator;
    }
}

// ---------------------------------------------------------------------------
// sMethod_join @0x82AFB850 -- AS Array.join([separator]): render every element to a
// string joined by the separator (default ","), boxed as a fresh AptString.
// ---------------------------------------------------------------------------
AptValue* AptArray::sMethod_join(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    EAStringC strResult;       // accumulates the joined text
    EAStringC strSeparator;    // holds the rendered separator argument (if any)

    const char* szSeparator;
    if (nArgCount > 0)
    {
        gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&strSeparator);
        szSeparator = strSeparator.GetBuffer();
    }
    else
    {
        szSeparator = ",";
    }

    pThis->toString(&strResult, szSeparator);

    AptString* pString = AptString::Create("");
    *pString->GetInternalString() = strResult;
    return pString;
}

// ===========================================================================
// Array.sort / sortOn + their comparators + the AS object-model overrides.
// DECOMPILED from the X360 ARTIST.XEX:
//   defaultSortOnCompareFunc @0x82AE7058 / sMethod_sort @0x82AED3C8 /
//   sMethod_sortOn @0x82AFB938 / scriptFunctionSortFunc @0x82AED2A8 /
//   objectMemberLookup @0x82AFDA50 / objectMemberSet @0x82AE2A00 /
//   CleanNativeFunctions @0x82AD6A10.
// ===========================================================================

// ---------------------------------------------------------------------------
// File-static Apt-array native-method cache + sort state (X360 .rdata block at
// 0x8324E3E0..0x8324E410, this TU's statics). Each off_8324E3xx is a lazily-built,
// GC-rooted AptNativeFunction* shared by every array (objectMemberLookup builds it
// on first lookup; CleanNativeFunctions releases them at shutdown). The two
// sort-state slots (0x8324E3F8/0x8324E3FC) hold the script compare function + its
// context while a qsort with a user comparator is in flight.
// ---------------------------------------------------------------------------
static AptNativeFunction* gpArrayMethod_concat   = nullptr;   // off_8324E3E0
static AptNativeFunction* gpArrayMethod_join     = nullptr;   // off_8324E3E4
static AptNativeFunction* gpArrayMethod_pop      = nullptr;   // off_8324E3E8
static AptNativeFunction* gpArrayMethod_push     = nullptr;   // off_8324E3EC
static AptNativeFunction* gpArrayMethod_shift    = nullptr;   // off_8324E3F0
static AptNativeFunction* gpArrayMethod_unshift  = nullptr;   // off_8324E3F4
static AptNativeFunction* gpArrayMethod_sort     = nullptr;   // off_8324E400
static AptNativeFunction* gpArrayMethod_sortOn   = nullptr;   // off_8324E404
static AptNativeFunction* gpArrayMethod_reverse  = nullptr;   // off_8324E408
static AptNativeFunction* gpArrayMethod_splice   = nullptr;   // off_8324E40C
static AptNativeFunction* gpArrayMethod_slice    = nullptr;   // off_8324E410

// Sort-state for a user (script) compare function: the AS function value
// (dword_8324E3F8) and its bound context (dword_8324E3FC == the function's
// mpCIH at console +0x20, the bound character-instance handle). sMethod_sort
// latches them, scriptFunctionSortFunc reads them.
static AptValue* gpSortScriptFunction = nullptr;   // dword_8324E3F8
static AptValue* gpSortScriptContext  = nullptr;   // dword_8324E3FC

// Homed HERE with the sort natives (its only console xrefs are this TU's
// sortOn path + comparator): the "sortOn" field-name string,
// a global EAStringC the sortOn path renders the field name into and the
// comparator reads back. The X360 stores it as the bare m_pData pointer
// off_82F73384 (seeded to the empty-string sentinel unk_82F72FF8); modelled here
// as a real EAStringC default-constructed to the empty string.
static EAStringC gAptSortOnField;   // off_82F73384

// AS Array.shift native -- referenced by objectMemberLookup's method-cache build.
// The X360 ARTIST body was ICF-folded out of this class's dossier, but the PS3
// DecFIGS export carries it verbatim: AptArray::sMethod_shift @0xF1D39C. The free
// name AptArray_sMethod_shift (extern "C") is kept -- the method-cache macro casts
// it to AptExtFunctionPtr anyway -- and the unused AS-call argument (a2) the member
// form takes is dropped (the sibling pop native is the same single-arg shape).
// AptArray::sMethod_shift is now a static member (declared in AptArray.h); the method-cache below references it directly.

// ---------------------------------------------------------------------------
// AptArray_sMethod_shift @0xF1D39C (PS3) -- Array.prototype.shift: remove and return
// the FIRST element, sliding the rest down one slot. The shifted-out value's
// reference is handed to the caller (no Release), and the now-vacated tail slot is
// cleared so the shortened array does not double-own it (the pop sibling's contract).
//
//   PS3: if (!isArray) return undefined;  if (length<=0) return undefined;
//        v5 = get(0);  length -= 1;
//        if (oldLen != 1) memmove(mpArray, mpArray+1, 4*(oldLen-1));
//        mpArray[length] = 0;  return v5;
// The console memmove strides 4-byte pointers; widened here to sizeof(AptValue*).
// ---------------------------------------------------------------------------
AptValue* AptArray::sMethod_shift(AptArray* pThis)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    const int nOldLen = pThis->mnLength;   // *(this+10)
    if (nOldLen <= 0)
        return gpUndefinedValue;

    // The first element (handed to the caller; ref ownership transfers out).
    AptValue* pElement = pThis->get(0);    // AptArray::get(this, 0)

    pThis->mnLength = nOldLen - 1;         // *(this+10) = v4 - 1
    if (nOldLen != 1)
        std::memmove(pThis->mpArray, pThis->mpArray + 1,
                     sizeof(AptValue*) * static_cast<size_t>(nOldLen - 1));   // 4*(v4-1) -> x64 stride
    pThis->mpArray[pThis->mnLength] = nullptr;   // *(4*length + mpArray) = 0 -- transfer ref out

    return pElement;
}

// ---------------------------------------------------------------------------
// defaultSortOnCompareFunc @0x82AE7058 -- sortOn comparator. Both elements must be
// the same shape: AS Objects (look the field up by name in each object's property
// hash) or AS Arrays (treat the field name as a numeric index into each sub-array).
// The two resolved field values are then compared with the lexical
// defaultSortCompareFunc. Anything else compares equal (0).
// ---------------------------------------------------------------------------
int AptArray::defaultSortOnCompareFunc(AptValue* const* ppA, AptValue* const* ppB)
{
    AptValue* pA = *ppA;
    AptValue* pB = *ppB;

    AptValue* pFieldA;
    AptValue* pFieldB;

    if (pA->isObject() && pB->isObject())
    {
        // Object path: the field is a named property of each element.
        pFieldA = pA->GetNativeHashVirtual()->Lookup(gAptSortOnField);
        if (!pFieldA)
            return 0;
        pFieldB = pB->GetNativeHashVirtual()->Lookup(gAptSortOnField);
        if (!pFieldB)
            return 0;
    }
    else if (pA->isArray() && pB->isArray())
    {
        // Array path: the field name is a numeric index into each sub-array.
        AptArray* pArrA = static_cast<AptArray*>(pA);
        AptArray* pArrB = static_cast<AptArray*>(pB);

        int nIndexA = atoi(gAptSortOnField.GetBuffer());
        if (nIndexA < 0 || nIndexA >= pArrA->mnLength || (pFieldA = pArrA->mpArray[nIndexA]) == nullptr)
            pFieldA = gpUndefinedValue;

        int nIndexB = atoi(gAptSortOnField.GetBuffer());
        if (nIndexB < 0 || nIndexB >= pArrB->mnLength || (pFieldB = pArrB->mpArray[nIndexB]) == nullptr)
            pFieldB = gpUndefinedValue;
    }
    else
    {
        return 0;
    }

    return defaultSortCompareFunc(&pFieldA, &pFieldB);
}

// ---------------------------------------------------------------------------
// scriptFunctionSortFunc @0x82AED2A8 -- the qsort comparator that calls a user AS
// compare function: push the two element values as arguments onto the global
// operand stack, invoke the script function (callFunction) bound to its context,
// then read the integer result back off the stack.
// ---------------------------------------------------------------------------
int AptArray::scriptFunctionSortFunc(AptValue* const* ppA, AptValue* const* ppB)
{
    if (!gpSortScriptFunction)
        return 0;

    AptValue* pA = *ppA;   // r30
    AptValue* pB = *ppB;   // r3

    // @0x82AED2A8: push a fresh register-block window (the console inlines
    // PushStaticData: v5 = off_8324E3D0; base += 4*count; count = 0), push pB then
    // pA onto the VM operand stack (the X360 argument order) AddRef'ing each
    // (vtbl[0] on the pushed value), invoke the latched script comparator over its
    // bound context (callFunction(ctx, func, 2, 0, 0)), read the integer result
    // off the stack top, pop it, then pop the register-block window (the console's
    // CleanupAfterExecution over the saved base).
    AptValue** const lppSavedRegs = AptScriptFunctionBase::PushStaticData();

    gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop++] = pB;
    pB->AddRef();
    gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop++] = pA;
    pA->AddRef();

    gAptActionInterpreter.callFunction(gpSortScriptContext, gpSortScriptFunction,
                                       2, nullptr, nullptr);
    const int nResult =
        gAptActionInterpreter.mpStack[gAptActionInterpreter.mnStackTop - 1]->toInteger();
    gAptActionInterpreter.stackPop();
    gAptActionInterpreter.CleanupAfterExecution(lppSavedRegs);
    return nResult;
}

// ---------------------------------------------------------------------------
// sMethod_sort @0x82AED3C8 -- AS Array.sort([compareFunction]). With no argument
// use the default lexical comparator; with one, latch it (+ its context) as the
// sort-state script function and use scriptFunctionSortFunc. Then qsort the slot
// array in place. Returns `undefined`.
// ---------------------------------------------------------------------------
AptValue* AptArray::sMethod_sort(AptArray* pThis, int nArgCount)
{
    if (!pThis->isArray())
        return gpUndefinedValue;

    int (*pfnCompare)(const void*, const void*);

    if (nArgCount == 0)
    {
        pfnCompare = reinterpret_cast<int (*)(const void*, const void*)>(&AptArray::defaultSortCompareFunc);
    }
    else
    {
        pfnCompare = reinterpret_cast<int (*)(const void*, const void*)>(&AptArray::scriptFunctionSortFunc);
        // Latch the script compare function (top of the native-arg stack).
        gpSortScriptFunction = gppAptNativeArgStack[gnAptNativeArgCount - 1];
        // X360 @0x82AED448: dword_8324E3FC = *(func + 0x20) -- the function value's
        // bound CIH (AptScriptFunctionBase::mpCIH, console +0x20), read by name
        // through GetCIH(); scriptFunctionSortFunc passes it to callFunction as the
        // call scope.
        gpSortScriptContext =
            static_cast<AptScriptFunctionBase*>(gpSortScriptFunction)->GetCIH();
    }

    qsort(pThis->mpArray, pThis->mnLength, sizeof(AptValue*), pfnCompare);

    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// sMethod_sortOn @0x82AFB938 -- AS Array.sortOn(fieldName). Render the field-name
// argument into the global gAptSortOnField string, qsort by that field
// (defaultSortOnCompareFunc), then release the field-name string. Returns `undefined`.
// ---------------------------------------------------------------------------
AptValue* AptArray::sMethod_sortOn(AptArray* pThis, int nArgCount)
{
    if (pThis->isArray() && nArgCount > 0)
    {
        // Stringify the field-name argument (top of the native-arg stack) into the
        // shared sortOn field-name string.
        gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&gAptSortOnField);

        qsort(pThis->mpArray, pThis->mnLength, sizeof(AptValue*),
              reinterpret_cast<int (*)(const void*, const void*)>(&AptArray::defaultSortOnCompareFunc));

        // Release the field-name string and reset it to the empty-string sentinel.
        gAptSortOnField = EAStringC();
    }

    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// objectMemberLookup @0x82AFDA50 -- resolve an array member.
//   - a recognised native method name (push/pop/.../slice): lazily build the shared
//     GC-rooted AptNativeFunction, then return it;
//   - "length": a fresh boxed AptInteger of the current length;
//   - a purely numeric name: index into the element vector (else `undefined`);
//   - anything else: fall through to the own-property hash.
// (The X360 dispatch is a gperf perfect-hash jump table over ArrayMembersIndex; the
// method-name -> case mapping is reproduced as the if-ladder below.)
// ---------------------------------------------------------------------------
AptValue* AptArray::objectMemberLookup(AptValue* const /*pThis*/,
                                       const AptNativeString* const pName) const
{
    // The console dispatch is a gperf perfect hash (ArrayMembersIndex) whose
    // asso/wordlist .rdata is not in the code-only exports; the strcmp ladder
    // below reproduces its name -> method mapping exactly (gperf itself confirms
    // each candidate with the same strcmp, so accept/reject sets are identical) --
    // the recognised-name set is transcribed from the lazy-method cache slots
    // above, and the recognizer TU is behaviourally subsumed.
    const char* szName = pName->GetBuffer();

    // --- Lazily build + return a recognised array native method. Each is the X360
    // `if(!cache){ cache = new AptNativeFunction(method); cache->setGCRoot(1);
    // cache->AddRef(); } return cache;` idiom (the new-returns-null guard restored
    // to a plain new). The strcmp ladder stands in for the gperf jump table.
    #define APT_ARRAY_LAZY_METHOD(slotVar, methodFn)                              \
        do {                                                                       \
            if (!(slotVar)) {                                                      \
                (slotVar) = new AptNativeFunction(                                 \
                    reinterpret_cast<AptExtFunctionPtr>(&methodFn));               \
                (slotVar)->setGCRoot(1);                                           \
                (slotVar)->AddRef();                                               \
            }                                                                      \
            return (slotVar);                                                      \
        } while (0)

    if (strcmp(szName, "concat") == 0)  APT_ARRAY_LAZY_METHOD(gpArrayMethod_concat,  AptArray::sMethod_concat);
    if (strcmp(szName, "join") == 0)    APT_ARRAY_LAZY_METHOD(gpArrayMethod_join,    AptArray::sMethod_join);
    if (strcmp(szName, "pop") == 0)     APT_ARRAY_LAZY_METHOD(gpArrayMethod_pop,     AptArray::sMethod_pop);
    if (strcmp(szName, "push") == 0)    APT_ARRAY_LAZY_METHOD(gpArrayMethod_push,    AptArray::sMethod_push);
    if (strcmp(szName, "shift") == 0)   APT_ARRAY_LAZY_METHOD(gpArrayMethod_shift,   AptArray::sMethod_shift);
    if (strcmp(szName, "unshift") == 0) APT_ARRAY_LAZY_METHOD(gpArrayMethod_unshift, AptArray::sMethod_unshift);
    if (strcmp(szName, "reverse") == 0) APT_ARRAY_LAZY_METHOD(gpArrayMethod_reverse, AptArray::sMethod_reverse);
    if (strcmp(szName, "sort") == 0)    APT_ARRAY_LAZY_METHOD(gpArrayMethod_sort,    AptArray::sMethod_sort);
    if (strcmp(szName, "splice") == 0)  APT_ARRAY_LAZY_METHOD(gpArrayMethod_splice,  AptArray::sMethod_splice);
    if (strcmp(szName, "slice") == 0)   APT_ARRAY_LAZY_METHOD(gpArrayMethod_slice,   AptArray::sMethod_slice);
    if (strcmp(szName, "sortOn") == 0)  APT_ARRAY_LAZY_METHOD(gpArrayMethod_sortOn,  AptArray::sMethod_sortOn);

    #undef APT_ARRAY_LAZY_METHOD

    // --- "length": a fresh boxed integer of the current length (X360 jump-table
    // case @loc_82AFDAC8: AptInteger::Create(*(this+40))).
    if (strcmp(szName, "length") == 0)
        return AptInteger::Create(mnLength);

    // --- numeric index: strtol the whole name; if it consumed the entire string
    // (and the name is non-empty), index the element vector.
    char* pEnd = nullptr;
    long  nIndex = strtol(szName, &pEnd, 10);
    if (pName->GetLength() != 0 && pEnd == szName + pName->GetLength())
    {
        AptValue* pElement;
        if (nIndex < 0 || nIndex >= mnLength || (pElement = mpArray[nIndex]) == nullptr)
            pElement = gpUndefinedValue;
        return pElement;
    }

    // --- own-property fallback (the array's named members).
    return const_cast<AptArray*>(this)->mHash.Lookup(*pName);
}

// ---------------------------------------------------------------------------
// objectMemberSet @0x82AE2A00 -- a numeric member name ("0", "12", ...) stores into
// the element vector (growing it); the leading-zero / atoi==0 check accepts the
// literal "0". Any non-numeric name is not handled here (returns false). A null
// value stores `undefined`.
// ---------------------------------------------------------------------------
bool AptArray::objectMemberSet(AptValue* const /*pThis*/,
                               const AptNativeString* const pName,
                               AptValue* const pValue)
{
    const char* szName = pName->GetBuffer();

    if (atoi(szName) != 0 || *szName == '0')
    {
        int       nIndex = atoi(szName);
        AptValue* pStore = pValue ? pValue : gpUndefinedValue;
        set(nIndex, pStore);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @0x82AD6A10 -- release every lazily-built array native
// method at Apt shutdown (AptUpdateShutdown). Each `if(p){ p->Release(); p=0; }`
// matches the X360 vtable-slot-+4 (Release) call. (The release order is the X360's
// exactly; the two sort-state slots are transient and are not touched here.)
// ---------------------------------------------------------------------------
void AptArray::CleanNativeFunctions()
{
    if (gpArrayMethod_concat)  { gpArrayMethod_concat->Release();  gpArrayMethod_concat  = nullptr; }
    if (gpArrayMethod_join)    { gpArrayMethod_join->Release();    gpArrayMethod_join    = nullptr; }
    if (gpArrayMethod_pop)     { gpArrayMethod_pop->Release();     gpArrayMethod_pop     = nullptr; }
    if (gpArrayMethod_push)    { gpArrayMethod_push->Release();    gpArrayMethod_push    = nullptr; }
    if (gpArrayMethod_shift)   { gpArrayMethod_shift->Release();   gpArrayMethod_shift   = nullptr; }
    if (gpArrayMethod_unshift) { gpArrayMethod_unshift->Release(); gpArrayMethod_unshift = nullptr; }
    if (gpArrayMethod_reverse) { gpArrayMethod_reverse->Release(); gpArrayMethod_reverse = nullptr; }
    if (gpArrayMethod_sort)    { gpArrayMethod_sort->Release();    gpArrayMethod_sort    = nullptr; }
    if (gpArrayMethod_splice)  { gpArrayMethod_splice->Release();  gpArrayMethod_splice  = nullptr; }
    if (gpArrayMethod_slice)   { gpArrayMethod_slice->Release();   gpArrayMethod_slice   = nullptr; }
    if (gpArrayMethod_sortOn)  { gpArrayMethod_sortOn->Release();  gpArrayMethod_sortOn  = nullptr; }
}

// (The AptValue_AptArrayToString thunk was RETIRED 2026-07-10: AptValue::toString's
// `case AptVFT_Array` now calls the private member AptArray::toString @0x82AED040
// directly -- the folded-TU private access is modelled by the AptValue friend
// declaration in AptArray.h. PS3 EXTERNAL 0xF39520 == the EAStringC& overload.)

// ===========================================================================
// AptFrameStackFirstLocal (HOMED 2026-07-02, retiring the stub null).
// Misnomer kept for the call-site contract: the CallFunction opcode's tag-14
// operand is an AptARRAY (the console reads value[10] = mnLength and
// *value[8] = mpArray[0]); a non-empty array's first element is the captured
// function value, null otherwise.
// ===========================================================================
AptValue* AptFrameStackFirstLocal(AptValue* pValue)
{
    AptArray* const pArray = static_cast<AptArray*>(pValue);
    if (pArray->mnLength > 0 && pArray->mpArray != nullptr && pArray->mpArray[0] != nullptr)
        return pArray->mpArray[0];
    return nullptr;
}
