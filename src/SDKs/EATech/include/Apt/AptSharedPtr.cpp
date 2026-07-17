#include "SDKs/EATech/include/Apt/AptSharedPtr.h"

#include <intrin.h>   // _InterlockedIncrement / _InterlockedDecrement (MSVC)

#include "SDKs/EATech/include/Apt/AptFile.h"     // AptFile + ~AptFile (Delete)
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager (Delete frees here)

// ===========================================================================
// AptSharedPtr<AptFile> -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE from the Feb-2007 Apt.h DWARF; BODIES store-for-store from the X360
// pseudocode + assembly. The reference count is the first 32-bit word of the
// shared pointee (the lwarx/stwcx. operate on `*pData`). On X360 the count is
// mutated under an interrupt-masked load-linked/store-conditional loop
// (mfmsr/mtmsree/lwarx/stwcx.); the host-portable equivalent is the matching
// interlocked increment/decrement, which yields the SAME post-value used by the
// delete-when-zero test.
// ===========================================================================

// ---------------------------------------------------------------------------
// AptSharedPtr<AptFile>::Dispose(AptFile*)   @0x8284D158
//
//   STUB(a1, a2, a3);                 // entry debug hook (args passed through)
//   if ( pData ) {
//       count = AptSharedPtrDecRef(pData);
//       if ( !count )
//           AptSharedPtrDelete(pData);
//   }
//   STUB(...);                        // exit debug hook
//
// The two out-of-line helpers and the bracketing debug hook are FLAGGED in the
// header (un-homed / not recoverable). Store + branch order preserved.
// ---------------------------------------------------------------------------
void AptSharedPtr<AptFile>::Dispose(AptFile* pData)
{
    AptSharedPtrDisposeDebugHook(pData, 0, nullptr);

    if (pData)
    {
        int count = AptSharedPtrDecRef(pData);
        if (!count)
            count = AptSharedPtrDelete(pData);
    }

    AptSharedPtrDisposeDebugHook(nullptr, 0, nullptr);
}

// ---------------------------------------------------------------------------
// AptSharedPtr<AptFile>::operator=(const AptSharedPtr<AptFile>&)  @0x82AFD548
//
// asm:
//   if ( &rhs != this ) {
//       newp = rhs.pData;            // r11
//       oldp = this->pData;          // r3  (saved BEFORE the store)
//       this->pData = rhs.pData;     // store new first
//       if ( newp ) ++(*newp);       // atomically incr NEW count
//       if ( oldp ) {                // atomically decr OLD count
//           if ( --(*oldp) == 0 )
//               AptSharedPtrDelete(oldp);
//       }
//   }
//   return this;
//
// The count word lives at offset 0 of the pointee; the asm dereferences the
// raw pData value (lwarx r,0,pData). Reinterpret pData as the count cell.
// ---------------------------------------------------------------------------
AptSharedPtr<AptFile>& AptSharedPtr<AptFile>::operator=(const AptSharedPtr<AptFile>& rhs)
{
    if (&rhs != this)
    {
        AptFile* newp = rhs.pData;
        AptFile* oldp = this->pData;
        this->pData = rhs.pData;

        if (newp)
            _InterlockedIncrement(reinterpret_cast<volatile long*>(newp));

        if (oldp)
        {
            if (_InterlockedDecrement(reinterpret_cast<volatile long*>(oldp)) == 0)
                AptSharedPtrDelete(oldp);
        }
    }
    return *this;
}

// ---------------------------------------------------------------------------
// AptSharedPtr<AptFile>::`vector deleting destructor'   @0x82AEC678
//
// Standard MSVC array deleting destructor. The heap array of
// AptSharedPtr<AptFile> (each element a single AptFile*) carries an MSVC array
// cookie one word BEFORE the returned base: cookie[-1] = element count. The
// allocation block actually starts two words back (cookie + count), and its
// size is `*(base-2) + 4` (the asm reads -4(r29) where r29 = base-1).
//
//   flags bit1 (a2 & 2): array form.
//   flags bit0 (a2 & 1): also free the backing block via the pool manager.
//
// asm (array branch):
//   count = *(base - 1);
//   cookieBase = base - 1;                  // r29
//   for ( p = &base[count]; --count-pass... ) { Dispose(*--p); *p = 0; }
//   if ( flags & 1 )
//       gpAptSharedPtrPool->Deallocate( (base-2), *(base-2) + 4 );
//   return cookieBase;                        // base - 1
//
// asm (scalar branch):
//   Dispose(*base); *base = 0;
//   if ( flags & 1 ) gpAptSharedPtrPool->Deallocate( base, 4 );
//   return base;
//
// Element store-order: load the AptFile* (r3), zero the slot (stw 0), THEN call
// Dispose -- preserved below.
// ---------------------------------------------------------------------------
void* AptSharedPtr<AptFile>::VectorDeletingDestructor(char flags)
{
    // The object is laid out as a contiguous array of AptFile* slots.
    AptFile** const base = reinterpret_cast<AptFile**>(this);

    if ((flags & 2) != 0)
    {
        // Array form. Element count is stored one word before the base.
        uint32_t* const pCount = reinterpret_cast<uint32_t*>(base) - 1;
        const uint32_t count = *pCount;

        AptFile** p = base + count;
        for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; --i)
        {
            --p;
            AptFile* elem = *p;
            *p = nullptr;
            AptSharedPtr<AptFile>::Dispose(elem);
        }

        if ((flags & 1) != 0)
        {
            // Backing block begins two words back; its byte size is the word at
            // (base-2) plus 4 (the cookie word itself).
            uint32_t* const pBlock = reinterpret_cast<uint32_t*>(base) - 2;
            gpAptSharedPtrPool->Deallocate(pBlock, static_cast<size_t>(*pBlock) + 4);
        }

        return pCount;
    }
    else
    {
        // Scalar form: a single AptFile* slot.
        AptFile* elem = *base;
        *base = nullptr;
        AptSharedPtr<AptFile>::Dispose(elem);

        if ((flags & 1) != 0)
            gpAptSharedPtrPool->Deallocate(base, 4);

        return base;
    }
}

// ---------------------------------------------------------------------------
// The shared AptFile reference-count primitives. The count is the first 32-bit
// word of the AptFile (mnRefCount); the console mutates it under an
// interrupt-masked lwarx/stwcx. loop, the host equivalent is the interlocked
// op, which yields the same post-value the callers test.
//
//   AptSharedPtrIncRef  @0x7E9210   AptSharedPtrDecRef  @0x7E9240
//   AptSharedPtrDelete  @0x80CC64
// ---------------------------------------------------------------------------
int AptSharedPtrIncRef(AptFile* pData)
{
    return static_cast<int>(_InterlockedIncrement(reinterpret_cast<volatile long*>(pData)));
}

int AptSharedPtrDecRef(AptFile* pData)
{
    return static_cast<int>(_InterlockedDecrement(reinterpret_cast<volatile long*>(pData)));
}

int AptSharedPtrDelete(AptFile* pData)
{
    if (pData)
    {
        pData->~AptFile();
        // Console frees a literal 28 bytes; sizeof(AptFile) is the width-correct
        // equivalent on x64 (the block was allocated by Load as sizeof(AptFile)).
        gpNonGCPoolManager->Deallocate(pData, sizeof(AptFile));
    }
    return 0;
}
