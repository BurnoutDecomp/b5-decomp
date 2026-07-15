#pragma once

// ===========================================================================
// EATech Apt -- StringAsVectorPolicy::ReAlloc<T>.
//
// The EA "string-as-vector" idiom reuses the BasicString<Encoding, Policy>
// template as a generic growable vector:
//     BasicString<StringAsVectorEncoding<T>, StringAsVectorPolicy>
// StringAsVectorPolicy is the storage policy; ReAlloc<T> is the one growth /
// re-home primitive the compiler stamps out per element type T. Given the old
// element array, the number of live elements to carry over, and the new element
// count, it:
//   1. allocates a fresh count-prefixed backing block sized for the new count
//      (with the console's saturating overflow guard),
//   2. default-constructs every new element,
//   3. copy-assigns the first min(newCount, carryCount) elements from the old
//      array,
//   4. tears the old array down (its per-element destructors + frees the block),
//   5. returns the new element base.
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX (asm authoritative). This TU owns the
// two instantiations the shipped build emits:
//     StringAsVectorPolicy::ReAlloc<AptFileSavedInputState>   @ 0x82AEFC00
//     StringAsVectorPolicy::ReAlloc<AptSharedPtr<AptFile> >   @ 0x82AFD5E0
// The two bodies differ in the ways the compiler specialised them per T -- the
// element stride (8 vs 4), the default-construct/copy/destroy per element, and
// (materially) the allocator: the AptFileSavedInputState array comes from the
// global operator new with a one-word count prefix, while the AptSharedPtr array
// comes from the Apt shared-pointer DOGMA pool with a {blockSize,count} two-word
// prefix (so its teardown can size the pool free). Each instantiation is
// therefore reconstructed as its own explicit specialisation, faithful to its
// own asm; the primary template is intentionally left undefined (only these two
// instantiations have an attested body).
//
// The count-prefixed raw block, the saturating size guard, and the per-element
// construct/copy/destruct are exactly what the asm performs; they are the
// storage template's own logic, de-optimised to human C++. No named struct is
// laid out here (ReAlloc traffics in raw element arrays), so there is no LLP64
// offset to pin.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptFileSavedInputState.h"   // element type (@0x82AEFC00)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"             // AptSharedPtr<AptFile> element (@0x82AFD5E0)

// ---------------------------------------------------------------------------
// StringAsVectorPolicy -- the string-as-vector storage policy. ReAlloc<T> is a
// static member template (the X360 symbols carry no `this`: the old array is the
// first __fastcall arg, r3). Only the two shipped instantiations are defined; the
// primary template is declared but deliberately left without a body.
// ---------------------------------------------------------------------------
struct StringAsVectorPolicy
{
    // pOld       -- the current element array (nullptr when growing from empty).
    // nCarryCount-- how many live elements the old array holds (elements to carry
    //               over; the copy is capped to min(newCount, this)).
    // nNewCount  -- the requested new element count / capacity.
    // returns    -- the new element base (nullptr on an empty/failed allocation).
    template <typename T>
    static T* ReAlloc(T* pOld, int32_t nCarryCount, uint32_t nNewCount);
};

// The two shipped explicit specialisations (bodies in StringAsVectorPolicy.cpp).
template <>
AptFileSavedInputState* StringAsVectorPolicy::ReAlloc<AptFileSavedInputState>(
    AptFileSavedInputState* pOld, int32_t nCarryCount, uint32_t nNewCount);

template <>
AptSharedPtr<AptFile>* StringAsVectorPolicy::ReAlloc<AptSharedPtr<AptFile> >(
    AptSharedPtr<AptFile>* pOld, int32_t nCarryCount, uint32_t nNewCount);
