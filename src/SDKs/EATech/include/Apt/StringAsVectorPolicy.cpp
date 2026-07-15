// ===========================================================================
// EATech Apt -- StringAsVectorPolicy::ReAlloc<T> (the string-as-vector growth op).
//
// DECOMPILED from BURNOUT_X360_ARTIST.XEX (asm authoritative):
//     StringAsVectorPolicy::ReAlloc<AptFileSavedInputState>   @ 0x82AEFC00
//     StringAsVectorPolicy::ReAlloc<AptSharedPtr<AptFile> >   @ 0x82AFD5E0
//
// See StringAsVectorPolicy.h for the per-instantiation lineage (the two bodies
// diverge in element stride, per-element construct/copy/destroy, and the
// allocator + count-prefix layout, so each is its own explicit specialisation).
//
// SHARED SHAPE (both bodies), straight from the asm:
//   pNew = null
//   if (pOld) {                                   // re-home an existing array
//       if (newCount) { pNew = alloc+init(newCount) }
//       carry = min(newCount, carryCount)
//       copy-assign carry elements old -> new
//       old-array deleting destructor (flags=3)   // destruct all + free block
//       return pNew
//   } else {                                      // grow from empty
//       return alloc+init(newCount)
//   }
//
// NOTE on the teardown pointer: Hex-Rays shows the old-array destructor being
// called on the source cursor AFTER the carry loop has advanced it. That is a
// register-reuse artifact -- the deleting destructor (flags=3) reads the element
// count from the block's count prefix and frees the whole backing block, so it
// must run on the ORIGINAL array base (destroying every old element, which
// conserves the ref/string counts the carried copies took). The reconstruction
// tears the whole old array down from its base, the faithful net effect.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/StringAsVectorPolicy.h"

#include <new>   // placement new + nothrow operator new

// ===========================================================================
// ReAlloc<AptFileSavedInputState>  @0x82AEFC00
//
// Element stride 8 (EAStringC mInputName + int32 meState). Backing block from the
// global operator new (console sub_82C08C00), laid out as [count, elem0, elem1, ...]
// -- a one-word count prefix. Per-element default construct = EAStringC() (empty,
// = the asm's `*i = &s_EmptyInternalData`) plus a zeroed state word; carry copy =
// EAStringC::operator= on the name + a raw copy of the state word.
// ===========================================================================

// Saturating allocation size for the count-prefixed block (asm v14 / v15):
//   v15 = 8*count; if count > 0x1FFFFFFF v15 = 0xFFFFFFFF; result = -1;
//   if v15 <= 0xFFFFFFFB result = v15 + 4  (room for the leading count word).
static int32_t StringAsVector_InputStateBlockSize(uint32_t nNewCount)
{
    uint32_t nBytes = 8u * nNewCount;
    if (nNewCount > 0x1FFFFFFFu)
        nBytes = 0xFFFFFFFFu;
    int32_t nAlloc = -1;
    if (nBytes <= 0xFFFFFFFBu)
        nAlloc = static_cast<int32_t>(nBytes + 4u);
    return nAlloc;
}

// Allocate + default-construct a count-prefixed AptFileSavedInputState array.
// Returns the element base (nullptr on a failed allocation), matching the asm's
// `if (block) { init; base = block+1 } else base = 0`.
static AptFileSavedInputState* StringAsVector_AllocInputStateArray(uint32_t nNewCount)
{
    const int32_t nAlloc = StringAsVector_InputStateBlockSize(nNewCount);

    // Console sub_82C08C00 == the global operator new; it returns null (not throw)
    // on failure, so the nothrow form matches the asm's explicit null check.
    int32_t* pBlock = static_cast<int32_t*>(
        ::operator new(static_cast<size_t>(nAlloc), std::nothrow));
    if (pBlock == nullptr)
        return nullptr;

    pBlock[0] = static_cast<int32_t>(nNewCount);   // count prefix (*v16 = v5)
    AptFileSavedInputState* pElems =
        reinterpret_cast<AptFileSavedInputState*>(pBlock + 1);

    for (uint32_t i = 0; i < nNewCount; ++i)
    {
        // asm: i[0] = &s_EmptyInternalData (EAStringC()), i[1] = 0 (state word).
        ::new (&pElems[i]) AptFileSavedInputState();
        pElems[i].meState = static_cast<E_APT_SAVED_INPUT_STATE>(0);
    }
    return pElems;
}

// Old-array deleting destructor (flags=3): destruct every element (releasing its
// EAStringC) back-to-front, then free the count-prefixed block.
static void StringAsVector_DestroyInputStateArray(AptFileSavedInputState* pElems)
{
    if (pElems == nullptr)
        return;
    int32_t* pBlock = reinterpret_cast<int32_t*>(pElems) - 1;   // count prefix
    const int32_t nCount = pBlock[0];
    for (int32_t i = nCount - 1; i >= 0; --i)
        pElems[i].~AptFileSavedInputState();
    ::operator delete(pBlock, std::nothrow);
}

template <>
AptFileSavedInputState* StringAsVectorPolicy::ReAlloc<AptFileSavedInputState>(
    AptFileSavedInputState* pOld, int32_t nCarryCount, uint32_t nNewCount)
{
    if (pOld == nullptr)
        return StringAsVector_AllocInputStateArray(nNewCount);

    AptFileSavedInputState* pNew = nullptr;
    if (nNewCount != 0)
        pNew = StringAsVector_AllocInputStateArray(nNewCount);

    int32_t nCarry = static_cast<int32_t>(nNewCount);   // min(newCount, carryCount)
    if (nCarry >= nCarryCount)
        nCarry = nCarryCount;
    for (int32_t i = 0; i < nCarry; ++i)
    {
        pNew[i].mInputName = pOld[i].mInputName;   // EAStringC::operator=
        pNew[i].meState    = pOld[i].meState;      // raw state word copy
    }

    StringAsVector_DestroyInputStateArray(pOld);
    return pNew;
}

// ===========================================================================
// ReAlloc<AptSharedPtr<AptFile> >  @0x82AFD5E0
//
// Element stride 4 (a single AptFile*). Backing block from the Apt shared-pointer
// DOGMA pool (gpAptSharedPtrPool / off_8324D808), laid out as [blockSize, count,
// elem0, ...] -- a two-word prefix so the pool free can be sized. Per-element
// default construct = a zeroed pointer slot; carry copy = AptSharedPtr::operator=
// (increments the carried AptFile's reference count). Old-array teardown reuses
// the committed AptSharedPtr<AptFile> vector deleting destructor.
// ===========================================================================

// Saturating block-size word for the two-word-prefixed pool block (asm v14/v15):
//   v15 = 4*count; if count > 0x3FFFFFFF v15 = 0xFFFFFFFF; result = -1;
//   if v15 <= 0xFFFFFFFB result = v15 + 4. Stored at block[0]; the pool
//   allocation itself is (result + 4) bytes (blockSize word + count word + data).
static int32_t StringAsVector_SharedPtrBlockSize(uint32_t nNewCount)
{
    uint32_t nBytes = 4u * nNewCount;
    if (nNewCount > 0x3FFFFFFFu)
        nBytes = 0xFFFFFFFFu;
    int32_t nAlloc = -1;
    if (nBytes <= 0xFFFFFFFBu)
        nAlloc = static_cast<int32_t>(nBytes + 4u);
    return nAlloc;
}

// Allocate + zero-init a two-word-prefixed AptSharedPtr<AptFile> array through the
// pool. Returns the element base (nullptr on the pool's failure sentinel).
static AptSharedPtr<AptFile>* StringAsVector_AllocSharedPtrArray(uint32_t nNewCount)
{
    const int32_t nBlockSize = StringAsVector_SharedPtrBlockSize(nNewCount);

    int32_t* pBlock = static_cast<int32_t*>(
        gpAptSharedPtrPool->Allocate(static_cast<size_t>(nBlockSize + 4)));

    pBlock[0] = nBlockSize;   // *v16 = v14 (the asm stores the size word first)

    // Console: DOGMA_PoolManager::Allocate signals failure with the sentinel
    // (void*)-4; portable equivalent is a sentinel guard -> empty array.
    if (reinterpret_cast<intptr_t>(pBlock) == -4)
        return nullptr;

    pBlock[1] = static_cast<int32_t>(nNewCount);   // count word (v16[1] = v5)
    AptSharedPtr<AptFile>* pElems =
        reinterpret_cast<AptSharedPtr<AptFile>*>(pBlock + 2);

    for (uint32_t i = 0; i < nNewCount; ++i)
        pElems[i].pData = nullptr;   // asm zero-inits each pointer slot
    return pElems;
}

template <>
AptSharedPtr<AptFile>* StringAsVectorPolicy::ReAlloc<AptSharedPtr<AptFile> >(
    AptSharedPtr<AptFile>* pOld, int32_t nCarryCount, uint32_t nNewCount)
{
    if (pOld == nullptr)
        return StringAsVector_AllocSharedPtrArray(nNewCount);

    AptSharedPtr<AptFile>* pNew = nullptr;
    if (nNewCount != 0)
        pNew = StringAsVector_AllocSharedPtrArray(nNewCount);

    int32_t nCarry = static_cast<int32_t>(nNewCount);   // min(newCount, carryCount)
    if (nCarry >= nCarryCount)
        nCarry = nCarryCount;
    for (int32_t i = 0; i < nCarry; ++i)
        pNew[i] = pOld[i];   // AptSharedPtr<AptFile>::operator= (ref-count inc)

    // Old-array deleting destructor (flags=3): Dispose() every element + free the
    // pooled block (see the teardown-pointer note at the top of this file).
    pOld->VectorDeletingDestructor(3);
    return pNew;
}
