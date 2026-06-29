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
#include "SDKs/EATech/include/Apt/AptDefine.h"        // gpNonGCPoolManager / gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"    // AptValueGC_PoolManager::AllocateAptValueGC / DeallocateAptValueGC
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <cstring>   // memcpy

// GC-pool operator new/delete @0x82AE6088 -- AptArray is a garbage-collected value
// (AptValueGC base), so its block comes from the GC pool (gpGCPoolManager), the GC
// analogue of the non-GC leaves' gpNonGCPoolManager route. AllocateAptValueGC =
// DOGMA Allocate + AptValueGC_MemItem::SetIsAllocated (the X360 inlines that pair
// into every GC type's operator new). Reached as AptArray::operator new(44) from
// AptValueFactory::CreateArray. (FLAG: gpGCPoolManager is null until AptInit wires it.)
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
        newArray[i] = nullptr;

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
            AptValue::sReferenceRegistrationCb(this, &mpArray[i], "", 0);
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

// FLAG (homed by the apt VM native-call dispatch): the global native-method arg
// stack (X360 off_8324E768 = gAptActionInterpreter.mpStack, dword_8324E760 = its
// mnStackTop). The i-th AS argument (i=0 = last pushed) is
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
