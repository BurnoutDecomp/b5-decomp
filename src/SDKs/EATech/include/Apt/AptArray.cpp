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
#include "SDKs/EATech/include/Apt/AptDefine.h"     // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <cstring>   // memcpy

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
