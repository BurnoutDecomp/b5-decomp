#include "SDKs/Realmc/RealmcContainers.h"

#include <cstring>   // std::memcpy / std::memmove (the EASTL bodies' element moves)

// ===========================================================================
// RealmcCore container instantiations -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcContainers.h for the per-container layout maps. Every allocation routes
// through the shared Realmc backend (RealmcCore::allocator::allocate stamps the
// "RealmcCore::allocator" tag; frees go through g_pRealmcAllocator->Free), exactly
// like the sibling RealmcCore.cpp / RealmcAllocator64.cpp reconstructions.
// ===========================================================================

namespace RealmcCore
{

// The shared empty char16 singleton (X360 unk_8324FFF4). AllocateSelf seats an
// empty String16's begin/end here and its capEnd one char16 past it -- the two
// X360 globals unk_8324FFF4 / unk_8324FFF6 are exactly 2 bytes (one char16)
// apart, i.e. a single-element zero buffer, the EASTL empty-string terminator.
static char16_t gRealmcEmptyString16[1] = { 0 };

// ---------------------------------------------------------------------------
// String16 special members -- the eastl::basic_string ctor/dtor the X360 folds
// inline into its owners (RealmcCore::Trc), de-inlined here (see RealmcContainers.h).
//
// The default ctor reproduces the empty-string stores the Trc copy ctor emits per
// option string (begin = end = &empty, capEnd = &empty + 1); the range ctor is the
// member copy-construction the Trc ctor does for mMessage (RangeInitialize); the
// destructor is the per-string free the Trc destructor loops over. The allocator
// name word is a stateless-allocator slot never read on this platform (AllocateSelf
// leaves it too); it is zeroed here for defined behaviour.
// ---------------------------------------------------------------------------
String16::String16()
{
    mpBegin         = gRealmcEmptyString16;
    mpEnd           = gRealmcEmptyString16;
    mpCapEnd        = gRealmcEmptyString16 + 1;
    mpAllocatorName = nullptr;
}

String16::String16(const char16_t* pFirst, const char16_t* pLast)
{
    mpAllocatorName = nullptr;
    RangeInitialize(pFirst, pLast);   // seats begin/end/capEnd (allocates when non-empty)
}

String16::~String16()
{
    // capacity in char16 elements; the shared empty singleton has capacity 1, so a
    // never-allocated (sentinel-empty) string never frees -- exactly the X360 guard.
    const std::ptrdiff_t nCapacity = mpCapEnd - mpBegin;
    if (nCapacity > 1 && mpBegin)
        g_pRealmcAllocator->Free(
            mpBegin, static_cast<std::size_t>(nCapacity) * sizeof(char16_t));
}

// ---------------------------------------------------------------------------
// String16::AllocateSelf @ 0x82B55508
//
//   if (nCapacity <= 1) { begin = end = &empty ; capEnd = &empty + 1 }   (no alloc)
//   else { buf = allocate(2*nCapacity) ; begin = end = buf ; capEnd = buf + nCapacity }
//
// The X360 allocate call passes `this + 0xC` as the (stateless) allocator's
// implicit `this`; it is ignored, so the static adaptor is called directly. The
// small path never reassigns r3, so the X360 returns the container pointer there.
// ---------------------------------------------------------------------------
void* String16::AllocateSelf(unsigned int nCapacity)
{
    if (nCapacity <= 1u)
    {
        mpBegin  = gRealmcEmptyString16;
        mpEnd    = gRealmcEmptyString16;
        mpCapEnd = gRealmcEmptyString16 + 1;
        return this;                                        // X360 leaves r3 == this
    }

    char16_t* pBuffer =
        static_cast<char16_t*>(allocator::allocate(2u * nCapacity, 0));  // 2 bytes / char16
    mpBegin  = pBuffer;
    mpEnd    = pBuffer;
    mpCapEnd = pBuffer + nCapacity;                         // buf + 2*nCapacity bytes
    return pBuffer;
}

// ---------------------------------------------------------------------------
// String16::RangeInitialize @ 0x82B55580
//
//   count = (pLast - pFirst)           (X360: (a3 - a2) >> 1 over char16)
//   AllocateSelf(count + 1)
//   memcpy(mpBegin, pFirst, count * 2)
//   mpEnd = mpBegin + count ; mpBegin[count] = 0    (X360 sthx: 16-bit NUL store)
// ---------------------------------------------------------------------------
void String16::RangeInitialize(const char16_t* pFirst, const char16_t* pLast)
{
    const std::size_t nCount = static_cast<std::size_t>(pLast - pFirst);

    AllocateSelf(static_cast<unsigned int>(nCount) + 1u);

    std::memcpy(mpBegin, pFirst, nCount * sizeof(char16_t));
    mpEnd = mpBegin + nCount;
    *mpEnd = 0;                                             // NUL-terminate
}

// ---------------------------------------------------------------------------
// String16::erase @ 0x82B57758
//
//   if (pFirst != pLast) {
//     memmove(pFirst, pLast, ((mpEnd - pLast) + 1) * 2)    (tail incl. the NUL)
//     mpEnd -= (pLast - pFirst)
//   }
//   return pFirst;
// ---------------------------------------------------------------------------
char16_t* String16::erase(char16_t* pFirst, char16_t* pLast)
{
    if (pFirst != pLast)
    {
        const std::size_t nTail = static_cast<std::size_t>(mpEnd - pLast) + 1u;
        std::memmove(pFirst, pLast, nTail * sizeof(char16_t));
        mpEnd -= (pLast - pFirst);
    }
    return pFirst;
}

// ---------------------------------------------------------------------------
// MessageList::DoClear @ 0x82C46770
//
//   for (node = sentinel.next; node != &sentinel; ) {
//     cur = node ; node = node->next ;
//     cur->value.~MessagePtr() ; backend->Free(cur, 16) ;
//   }
//
// The next link is read BEFORE the node is freed (matching the asm), so the walk
// survives the free. The X360 node size is 16; on the host `sizeof` is used so the
// backend frees the host-correct node footprint.
// ---------------------------------------------------------------------------
void MessageList::DoClear()
{
    MessageListNodeBase* pNode = mSentinel.mpNext;
    while (pNode != &mSentinel)
    {
        MessageListNode* pCur = static_cast<MessageListNode*>(pNode);
        pNode = pNode->mpNext;                              // advance before free
        pCur->maValue.~MessagePtr();
        g_pRealmcAllocator->Free(pCur, sizeof(MessageListNode));
    }
}

// ---------------------------------------------------------------------------
// MessageList::DoErase @ 0x82C46110
//
//   node->prev->next = node->next ;                        (*a2[1] = *a2)
//   node->next->prev = node->prev ;                        ((*a2)[1] = a2[1])
//   node->value.~MessagePtr() ;                            (a2 + 8)
//   backend->Free(node, 16) ;
//
// The list `this` (a1) is unused -- the node carries its own links -- exactly as
// the X360 leaves r3 untouched.
// ---------------------------------------------------------------------------
void MessageList::DoErase(MessageListNode* pNode)
{
    pNode->mpPrev->mpNext = pNode->mpNext;
    pNode->mpNext->mpPrev = pNode->mpPrev;
    pNode->maValue.~MessagePtr();
    g_pRealmcAllocator->Free(pNode, sizeof(MessageListNode));
}

// ---------------------------------------------------------------------------
// IntVector::DoInsertValue @ 0x82C46DE8
//
// FULL path (mpEnd == mpCapEnd) -- reallocate:
//   oldSize = mpEnd - mpBegin ; newCap = oldSize ? 2*oldSize : 1
//   buf = newCap ? allocate(4*newCap) : 0
//   memcpy(buf, mpBegin, (pPos - mpBegin) * 4)             -> prefix
//   gap = buf + (pPos - mpBegin) ; if (gap) *gap = *pValue
//   memcpy(gap + 1, pPos, (mpEnd - pPos) * 4)              -> suffix
//   newEnd = (gap + 1) + (mpEnd - pPos)
//   if (mpBegin) backend->Free(mpBegin, (mpCapEnd - mpBegin) * 4)
//   mpBegin = buf ; mpEnd = newEnd ; mpCapEnd = buf + newCap
//
// NON-FULL path -- shift right by one:
//   src = pValue ; if (pValue >= pPos && pValue < mpEnd) src = pValue + 1   (alias fix)
//   if (mpEnd) *mpEnd = *(mpEnd - 1)                       (extend by the old back)
//   memmove(pPos + 1, pPos, (mpEnd - 1 - pPos) * 4)
//   *pPos = *src ; ++mpEnd
//
// *pValue is read into the gap BEFORE the old buffer is freed, so a pValue that
// aliases the old storage is safe (matches the asm ordering).
// ---------------------------------------------------------------------------
void IntVector::DoInsertValue(int* pPos, const int* pValue)
{
    if (mpEnd == mpCapEnd)
    {
        const std::size_t nOldSize = static_cast<std::size_t>(mpEnd - mpBegin);
        std::size_t nNewCap = 2u * nOldSize;
        if (nOldSize == 0)
            nNewCap = 1;

        int* pNew = nNewCap
            ? static_cast<int*>(allocator::allocate(4u * nNewCap, 0))
            : nullptr;

        const std::size_t nPrefix = static_cast<std::size_t>(pPos - mpBegin);
        std::memcpy(pNew, mpBegin, nPrefix * sizeof(int));

        int* pGap = pNew + nPrefix;
        if (pGap)
            *pGap = *pValue;

        const std::size_t nSuffix = static_cast<std::size_t>(mpEnd - pPos);
        std::memcpy(pGap + 1, pPos, nSuffix * sizeof(int));
        int* pNewEnd = (pGap + 1) + nSuffix;

        if (mpBegin)
            g_pRealmcAllocator->Free(
                mpBegin, static_cast<std::size_t>(mpCapEnd - mpBegin) * sizeof(int));

        mpBegin  = pNew;
        mpEnd    = pNewEnd;
        mpCapEnd = pNew + nNewCap;
    }
    else
    {
        const int* pSrc = pValue;
        if (pValue >= pPos && pValue < mpEnd)
            pSrc = pValue + 1;                              // pValue aliases the shifted range

        if (mpEnd)
            *mpEnd = *(mpEnd - 1);                          // seed the new back slot

        std::memmove(pPos + 1, pPos,
                     static_cast<std::size_t>(mpEnd - 1 - pPos) * sizeof(int));
        *pPos = *pSrc;
        ++mpEnd;
    }
}

// ---------------------------------------------------------------------------
// IntVector::DoRealloc<int*> @ 0x82C46F60
//
//   buf = nCapacity ? allocate(4*nCapacity) : 0                 (4 bytes / int)
//   memcpy(buf, pBegin, pEnd - pBegin)                          (copy live bytes)
//   return buf
//
// The X360 allocate call is the same backend vtable slot +8 (plain allocate,
// tag "RealmcCore::allocator", flags 0) that allocator::allocate wraps. The
// memcpy span a4 - a3 is the byte distance pEnd - pBegin, i.e. the live element
// count * sizeof(int); the fresh buffer is uninitialised past that. reserve()
// owns the old-buffer free and the begin/end/capEnd reseat, so this routine only
// allocates and moves.
// ---------------------------------------------------------------------------
int* IntVector::DoRealloc(unsigned int nCapacity, int* pBegin, int* pEnd)
{
    int* pNew = nCapacity
        ? static_cast<int*>(allocator::allocate(4u * nCapacity, 0))
        : nullptr;

    std::memcpy(pNew, pBegin,
                static_cast<std::size_t>(pEnd - pBegin) * sizeof(int));
    return pNew;
}

// ---------------------------------------------------------------------------
// IntVector::reserve @ 0x82C470C8
//
//   if (nCapacity > (mpCapEnd - mpBegin)) {                -> capacity in elements
//     buf = DoRealloc(nCapacity, mpBegin, mpEnd)
//     oldSize = mpEnd - mpBegin
//     if (mpBegin) backend->Free(mpBegin, (mpCapEnd - mpBegin) * 4)
//     mpBegin = buf ; mpCapEnd = buf + nCapacity ; mpEnd = buf + oldSize
//   }
//
// The old size is captured from mpEnd - mpBegin (the X360 recomputes it from the
// pre-store registers) and re-applied to the fresh buffer.
// ---------------------------------------------------------------------------
void IntVector::reserve(unsigned int nCapacity)
{
    const std::size_t nCapacityNow = static_cast<std::size_t>(mpCapEnd - mpBegin);
    if (nCapacity > nCapacityNow)
    {
        int* pNew = DoRealloc(nCapacity, mpBegin, mpEnd);

        const std::size_t nOldSize = static_cast<std::size_t>(mpEnd - mpBegin);
        if (mpBegin)
            g_pRealmcAllocator->Free(
                mpBegin, static_cast<std::size_t>(mpCapEnd - mpBegin) * sizeof(int));

        mpBegin  = pNew;
        mpCapEnd = pNew + nCapacity;
        mpEnd    = pNew + nOldSize;
    }
}

} // namespace RealmcCore
