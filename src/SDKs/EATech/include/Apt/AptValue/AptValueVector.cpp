// ===========================================================================
// EATech Apt -- AptValueVector method bodies.
//
// Reconstructed store-for-store from the X360 ARTIST.XEX pseudocode/asm:
//     AptValueVector::PopAndPush  @ 0x82ADBAB8
//     AptValueVector::SafePop     @ 0x82ADBB58
//     AptValueVector::Shutdown    @ 0x82AE14F0
// No leak/DWARF body exists for this class; see AptValueVector.h for the
// layout derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"

#include <cstring>   // memmove (RemoveAt tail shift)

// ---------------------------------------------------------------------------
// PopAndPush @ 0x82ADBAB8
//
// asm:
//   r31=this r30=nCount r28=pProducer
//   if (this->mnTop < nCount) skip
//   r3 = pProducer->vtbl[0](pProducer)          // produce
//   for (i=1; i<=nCount; ++i)
//       item = mppItems[mnTop - i]
//       item->vtbl[+4](item)                     // Release
//   mppItems[mnTop - nCount] = pProducer         // (stores r28, the producer)
//   mnTop = mnTop - nCount + 1
// The slot store writes r28 (the producer object itself), not the r3 produce
// result -- the producer constructs the pushed value in place and the binary
// reuses its pointer. Modelled verbatim.
// ---------------------------------------------------------------------------
AptValue* AptValueVector::PopAndPush(int32_t nCount, AptValueProducer* pProducer)
{
    AptValue* result = nullptr;

    if (mnTop >= nCount)
    {
        result = pProducer->Produce();

        for (int32_t i = 1; i <= nCount; ++i)
        {
            AptValue* pItem = mppItems[mnTop - i];
            pItem->Release();
            result = pItem;
        }

        mppItems[mnTop - nCount] = reinterpret_cast<AptValue*>(pProducer);
        mnTop = mnTop - nCount + 1;
    }

    return result;
}

// ---------------------------------------------------------------------------
// SafePop @ 0x82ADBB58
//
// asm:
//   r30=this r29=nCount
//   if (nCount <= 0)         skip
//   if (mnTop < nCount)      skip
//   for (i=1; i<=nCount; ++i)
//       item = mppItems[mnTop - i]
//       item->vtbl[+4](item)                     // Release
//   mnTop -= nCount
// ---------------------------------------------------------------------------
void AptValueVector::SafePop(int32_t nCount)
{
    if (nCount > 0 && mnTop >= nCount)
    {
        for (int32_t i = 1; i <= nCount; ++i)
        {
            AptValue* pItem = mppItems[mnTop - i];
            pItem->Release();
        }

        mnTop -= nCount;
    }
}

// ---------------------------------------------------------------------------
// pop @ 0x82ADBBD0
//
// asm:
//   r31=this
//   item = mppItems[mnTop - 1]   ( *(4*mnTop + items - 4) )
//   r3 = item->vtbl[+4](item)    // Release
//   --mnTop
//   return r3
// No bounds check (the caller guarantees a live top); store order: Release
// then decrement.
// ---------------------------------------------------------------------------
AptValue* AptValueVector::pop()
{
    // X360 returns the r3 left by the (void) Release virtual; faithfully this
    // is the popped item pointer (Release is declared void in the leak).
    AptValue* pItem = mppItems[mnTop - 1];
    pItem->Release();
    --mnTop;
    return pItem;
}

// ---------------------------------------------------------------------------
// Shutdown @ 0x82AE14F0
//
// asm:
//   r31=this
//   r4 = mppItems (+8)
//   if (r4) Deallocate(off_8324D808, mppItems, 4 * mnCapacity)
//   mnCapacity = 0 (+4); mnTop = 0 (+0); mppItems = 0 (+8)
// Store order matches the binary: free first, then clear capacity, top, items.
// ---------------------------------------------------------------------------
void AptValueVector::Shutdown()
{
    if (mppItems)
    {
        gpAptOperandStackPool->Deallocate(mppItems, 4 * mnCapacity);
    }

    mnCapacity = 0;
    mnTop = 0;
    mppItems = nullptr;
}

// ---------------------------------------------------------------------------
// shutdown @ 0x82AE15A0
//
// Byte-for-byte identical to Shutdown @ 0x82AE14F0 (same free + clear, same
// store order). The X360 emits it as a second symbol reached via
// AptActionInterpreter::shutdown; reproduced as its own method.
// ---------------------------------------------------------------------------
void AptValueVector::shutdown()
{
    if (mppItems)
    {
        gpAptOperandStackPool->Deallocate(mppItems, 4 * mnCapacity);
    }

    mnCapacity = 0;
    mnTop = 0;
    mppItems = nullptr;
}

void AptValueVector::RemoveAt(int32_t nIndex)
{
    // Drop one element; the live count shrinks by one first.
    const int32_t nNewTop = mnTop - 1;
    mnTop = nNewTop;

    // Shift the tail [nIndex+1 .. nNewTop) down by one slot, unless we removed
    // the (new) last element or the vector is now empty -- in those cases the
    // memmove would be a no-op / out of range, so it is skipped.
    if (nNewTop != 0 && nIndex != nNewTop)
    {
        memmove(&mppItems[nIndex],
                &mppItems[nIndex + 1],
                sizeof(AptValue*) * (nNewTop - nIndex));
    }

    // Clear the now-vacated top slot.
    mppItems[mnTop] = nullptr;
}
