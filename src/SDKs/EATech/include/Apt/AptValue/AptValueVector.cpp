// ===========================================================================
// EATech Apt -- AptValueVector method bodies.
//
// Reconstructed store-for-store from the X360 ARTIST.XEX pseudocode/asm.
//
// Family (A) -- the `AptValue>` operand-stack template (top@+0, capacity@+4):
//     PopAndPush  @ 0x82ADBAB8   SafePop  @ 0x82ADBB58   pop @ 0x82ADBBD0
//     Shutdown    @ 0x82AE14F0   shutdown @ 0x82AE15A0
//
// Family (B) -- the real `AptValueVector::*` symbols (capacity@+0, top@+4):
//     ctor (ConstructAptValueVector) @ 0x82AE32F0   GetAt   @ 0x82ADBFD0
//     SetAt          @ 0x82ADBFE0                    RemoveAt@ 0x82ADBFF0
//     ReleaseValues  @ 0x82ADCF60
//   (GetNumValues @0x82AD5228 / IsVectorFull @0x82ADC130 are inline in the .h.)
//
// The two families share this 12-byte storage but swap the role of +0/+4; see
// the layout-collision NOTE in AptValueVector.h. No leak/DWARF body exists for
// either class.
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
// PopValue -- the x64 export's pop-WITHOUT-Release twin of pop @0x82ADBBD0
// (phase-0 signature-parity item): pre-decrement the live top and hand the
// popped value to the caller, who now owns the reference (no vtbl Release call
// -- the ONE difference from pop()). No bounds check, matching pop().
// ---------------------------------------------------------------------------
AptValue* AptValueVector::PopValue()
{
    return mppItems[--mnTop];
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
        gpAptOperandStackPool->Deallocate(mppItems, sizeof(AptValue*) * mnCapacity);
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
        gpAptOperandStackPool->Deallocate(mppItems, sizeof(AptValue*) * mnCapacity);
    }

    mnCapacity = 0;
    mnTop = 0;
    mppItems = nullptr;
}

// ---------------------------------------------------------------------------
// AptValueVector::ConstructAptValueVector  (ctor @0x82AE32F0)
//
// asm:
//   r31=this r11=nCapacity
//   *(+0) = nCapacity            // family-(B) capacity -> mnTop member (+0)
//   *(+4) = 0                    // family-(B) top      -> mnCapacity member (+4)
//   *(+8) = Allocate(pool, 4*nCapacity)
// Family-(B) layout (capacity@+0, top@+4) -- the OPPOSITE of the
// `AptValue>` operand-stack class; see the AptValueVector.h collision note.
// Allocation order matches the binary: store capacity, store top=0, then the
// allocated pointer (the X360 zeroes +8 first then overwrites; the visible
// result is the allocated pointer, reproduced directly).
// ---------------------------------------------------------------------------
AptValueVector AptValueVector::ConstructAptValueVector(int32_t nCapacity)
{
    AptValueVector lvVec;
    lvVec.mnTop      = nCapacity;   // [c:+0x00] family-(B) capacity
    lvVec.mnCapacity = 0;           // [c:+0x04] family-(B) live top
    lvVec.mppItems   = static_cast<AptValue**>(
        gpAptOperandStackPool->Allocate(sizeof(AptValue*) * nCapacity));   // [c:+0x08] (X360: 4*nCapacity for 4-byte ptrs; x64 needs 8-byte slots)
    return lvVec;
}

// ---------------------------------------------------------------------------
// AptValueVector::GetAt  @0x82ADBFD0
//
// asm:
//   r11 = *(+8) (items); r10 = 4*nIndex; r3 = items[nIndex]
// ---------------------------------------------------------------------------
AptValue* AptValueVector::GetAt(int32_t nIndex)
{
    return mppItems[nIndex];   // [c:+0x08]
}

// ---------------------------------------------------------------------------
// AptValueVector::SetAt  @0x82ADBFE0
//
// asm:
//   r11 = *(+8) (items); r10 = 4*nIndex; items[nIndex] = pValue
// ---------------------------------------------------------------------------
void AptValueVector::SetAt(int32_t nIndex, AptValue* pValue)
{
    mppItems[nIndex] = pValue;   // [c:+0x08]
}

// ---------------------------------------------------------------------------
// AptValueVector::ReleaseValues  @0x82ADCF60
//
// asm:
//   r31=this
//   while (*(+4) != 0)                     // family-(B) live top (mnCapacity)
//   {
//       items = *(+8);
//       top   = *(+4) - 1;  *(+4) = top;
//       v4    = items[top];
//       if ( (v4->mnValueData & 0x03FFC000) != 0 )   // refcount field nonzero
//           result = v4->ClearReleaseAtEnd();         // bl AptValue__ClearReleaseAtEnd
//       else
//           result = (*(*v4 + 0x2C))(v4);             // vtbl[+0x2C] = ForceDelete
//   }
//   return result;
//
// Family-(B) live count lives at +0x04 (the mnCapacity member). The
// `rlwinm. r11, r11, 0, 6, 17` masks PPC bits 6..17 of mValueData -- the
// 12-bit mnReferenceCount field -- so the branch is "refcount != 0"; rendered
// via getRefCount() to stay endianness-agnostic on x64. The else branch's
// vtable slot +0x2C is AptValue::ForceDelete (12th virtual: AddRef, Release,
// GetNativeHashVirtual, ContainsNativeHashVirtual, GetHasClass, SetHasClass,
// objectMemberLookup, objectMemberSet, DeleteThis, PreDestroy,
// DestroyGCPointers, ForceDelete -> 11*4 = 0x2C).
//
// ClearReleaseAtEnd is declared void in the leak; the X360 leaves the v4
// pointer in r3 across the call, so the faithful "result" of that arm is the
// value pointer itself. Both arms therefore yield the last touched value.
// ---------------------------------------------------------------------------
AptValue* AptValueVector::ReleaseValues()
{
    AptValue* result = nullptr;

    while (mnCapacity != 0)   // [c:+0x04] family-(B) live top
    {
        AptValue** items = mppItems;                 // [c:+0x08]
        const int32_t top = mnCapacity - 1;
        mnCapacity = top;                            // [c:+0x04] store decremented top

        AptValue* v4 = items[top];
        if (v4->getRefCount() != 0)                  // mValueData & 0x03FFC000
        {
            v4->ClearReleaseAtEnd();
            result = v4;
        }
        else
        {
            v4->ForceDelete();                       // vtbl[+0x2C]
            result = v4;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// AptValueVector::RemoveAt  @0x82ADBFF0
//
// asm (family-(B): live count at +0x04 == mnCapacity member):
//   r11 = *(+4); addic. r11, -1; *(+4) = r11        // top -= 1, CR0 = (top==0)
//   beq skip                                         // if (top == 0) skip memmove
//   cmpw nIndex, top; beq skip                       // if (nIndex == top) skip
//   items = *(+8);
//   memmove(items + 4*nIndex, items + 4*nIndex + 4, 4*(top - nIndex))
// skip:
//   items = *(+8); items[*(+4)] = 0                  // clear vacated top slot
//
// Does not Release the removed value -- the caller owns it (matches X360).
// Family-(B) live count is +0x04 (mnCapacity member), not +0x00.
// ---------------------------------------------------------------------------
void AptValueVector::RemoveAt(int32_t nIndex)
{
    // Drop one element; the live count (+0x04) shrinks by one first.
    const int32_t nNewTop = mnCapacity - 1;   // [c:+0x04]
    mnCapacity = nNewTop;

    // Shift the tail (nIndex .. nNewTop) down by one slot, unless we removed the
    // (new) last element or the vector is now empty -- in those cases the
    // memmove would be a no-op / out of range, so it is skipped (the asm's two
    // early branches).
    if (nNewTop != 0 && nIndex != nNewTop)
    {
        memmove(&mppItems[nIndex],          // [c:+0x08]
                &mppItems[nIndex + 1],
                sizeof(AptValue*) * (nNewTop - nIndex));
    }

    // Clear the now-vacated top slot (items[live count]).
    mppItems[mnCapacity] = nullptr;   // [c:+0x08]
}
