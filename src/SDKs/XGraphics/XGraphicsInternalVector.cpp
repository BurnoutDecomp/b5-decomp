#include "SDKs/XGraphics/XGraphicsInternalVector.h"

#include <cstring> // memcpy

// ===========================================================================
// XGRAPHICS::InternalVector -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// XGraphicsInternalVector.h for the attested layout. Each store/branch below is
// store-for-store with the X360 asm. Storage is a run of u32 words carved from
// mpArena via Arena::Malloc; Grow reallocates and frees LIFO through the same
// arena.
// ===========================================================================

namespace XGRAPHICS
{

InternalVector::InternalVector(Arena* apArena)
{
    // asm order: stash arena, zero count, capacity = 2, then Malloc(2*4) bytes.
    mpArena    = apArena;
    muCount    = 0;
    muCapacity = 2;
    mpElements = static_cast<u32*>(mpArena->Malloc(2 * static_cast<int>(sizeof(u32))));
}

u32* InternalVector::Grow(u32 auIndex)
{
    // Extend the live count so index is covered.
    if (auIndex + 1 > muCount)
        muCount = auIndex + 1;

    // Double the capacity until index is in range.
    while (auIndex >= muCapacity)
        muCapacity *= 2;

    // Reallocate storage from the arena, copy the live prefix, free the old.
    u32* lpOld = mpElements;
    u32* lpNew = static_cast<u32*>(
        mpArena->Malloc(static_cast<int>(muCapacity) * static_cast<int>(sizeof(u32))));
    mpElements = lpNew;
    memcpy(lpNew, lpOld, muCount * sizeof(u32));
    mpArena->Free(lpOld);

    return &mpElements[auIndex];
}

u32* InternalVector::Insert(u32 auIndex)
{
    // Extend the count to reach the insertion point, then make room for one.
    if (auIndex > muCount)
        muCount = auIndex;
    ++muCount;

    // Grow if the new element would overflow the current capacity.
    if (muCount > muCapacity)
        Grow(muCount - 1);

    // Slot for the inserted element (read mpElements AFTER a possible Grow).
    u32* lpSlot = &mpElements[auIndex];

    // Shift the tail up one slot, from the last element down toward the hole.
    u32  luShift = muCount - auIndex - 1;
    u32* lpDst   = lpSlot + luShift; // == &mpElements[muCount - 1]
    if (muCount - auIndex != 1)
    {
        do
        {
            --luShift;
            *lpDst = *(lpDst - 1);
            --lpDst;
        } while (luShift);
    }

    *lpDst = 0; // zero the freshly opened slot
    return lpSlot;
}

void InternalVector::Remove(u32 auIndex)
{
    // Out-of-range removals are a no-op.
    if (auIndex >= muCount)
        return;

    // Shift the tail down one slot over the removed element, then shrink.
    u32* lpDst = &mpElements[auIndex];
    muCount -= 1;
    // Overlapping forward copy (dst < src) exactly as the X360 asm tail-calls
    // memcpy; the count is the number of elements after the removed one.
    memcpy(lpDst, lpDst + 1, (muCount - auIndex) * sizeof(u32));
}

} // namespace XGRAPHICS
