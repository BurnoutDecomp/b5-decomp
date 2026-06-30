#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtrIncRef / AptSharedPtr<AptFile>::Dispose

// ===========================================================================
//  MakeLinkerThingy -- AptLinkerThingy::AptLinkerThingy @0x82ADBF58, wrapped as the
//  AptLinker::Load factory (the caller's gpAptSharedPtrPool->Allocate(16) feeds the
//  ctor's `this`). The X360 ctor is folded (no own export symbol); disassembled from
//  the decrypted ARTIST.XEX @0x82ADBF58 (r3=this, r4=&AptFilePtr rFile, r5=AptValue*):
//      *(this+0)   = 0;                       // mnRefCount = 0
//      r11 = *(rFile+0);                      // rFile.pData
//      *(this+4)   = r11;                     // mpFile.pData = rFile.pData
//      if (r11) lwarx/+1/stwcx. on *r11;      // AptSharedPtrIncRef(rFile.pData)
//      *(this+8)   = r5;                      // mpValue = pValue
//      *(this+0xC) = 0  (byte);               // mbLinked = false
//      r3 = *(rFile+0); *(rFile+0) = 0;       // null the source handle ...
//      AptSharedPtr<AptFile>::Dispose(r3);    // ... and drop its reference
//      return this;
//  Net effect: the held file reference is MOVED from rFile into mpFile (incref into
//  the thingy, then dispose the now-emptied source). Faithful x64 widths.
//
//  The Make* convention owns the allocation (the caller's separate Allocate(16) is a
//  presence test the X360 folds into this ctor's prologue): pool-allocate the 16-byte
//  node, then run the ctor stores. Null on allocation failure.
// ===========================================================================
AptLinkerThingy* MakeLinkerThingy(AptFilePtr& rFile, AptValue* pValue)
{
    void* lpMem = gpAptSharedPtrPool->Allocate(sizeof(AptLinkerThingy));   // Allocate(off_8324D808, 0x10)
    if (lpMem == nullptr)
        return nullptr;

    AptLinkerThingy* pThingy = static_cast<AptLinkerThingy*>(lpMem);

    pThingy->mnRefCount = 0;                       // *(this+0) = 0

    AptFile* lpFile = rFile.pData;                 // r11 = *(rFile+0)
    pThingy->mpFile.pData = lpFile;                // *(this+4) = r11
    if (lpFile != nullptr)
        AptSharedPtrIncRef(lpFile);                // incref the held file (lwarx/+1/stwcx.)

    pThingy->mpValue  = pValue;                    // *(this+8) = r5
    pThingy->mbLinked = false;                     // *(this+0xC) = 0

    // Consume the source handle: null it, then dispose its reference (the move's
    // matching decref -- AptSharedPtr<AptFile>::Dispose decrements + frees at zero).
    AptFile* lpSource = rFile.pData;               // r3 = *(rFile+0)
    rFile.pData = nullptr;                         // *(rFile+0) = 0
    AptSharedPtr<AptFile>::Dispose(lpSource);      // AptSharedPtr<AptFile>::Dispose(r3)

    return pThingy;                                // ctor returns `this`
}

// ===========================================================================
//  AptLinkerThingy::`scalar deleting destructor` -- @ 0x82AE5128.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. X360 asm is
//  authoritative. See AptLinkerThingy.h for the member layout.
//
//  The X360 body (registers r3=this, r4=flags):
//      v4 = *(this+4);                                  // lwz  r3, 4(r31)
//      *(this+4) = 0;                                   // stw  r11(=0), 4(r31)
//      AptFile_::Dispose(v4);                           // bl   AptFile___Dispose
//      if (flags & 1)                                   // clrlwi. r11, r30, 31
//          Deallocate(off_8324D808, this, 0x10);        // 16-byte pool block
//      return this;                                     // mr r3, r31
//
//  AptFile_::Dispose is AptSharedPtr<AptFile>::Dispose(AptFile*) (the X360 takes
//  the AptFile object pointer directly in r3 -- a single argument; the Hex-Rays
//  3-arg shape is a misread of the debug-thunk arguments inside Dispose itself,
//  not of this call). The load-then-zero-then-dispose order is preserved.
//
//  off_8324D808 is the shared Apt DOGMA pool (declared as gpAptSharedPtrPool in
//  AptSharedPtr.h -- the same underlying pool object every fixed-size Apt node
//  references). Deallocate frees the 16-byte block.
// ===========================================================================

void* AptLinkerThingy::ScalarDeletingDestructor(char flags)
{
    AptFile* lpFile = mpFile.pData;   // v4 = *(this+4)
    mpFile.pData = nullptr;     // *(this+4) = 0

    AptSharedPtr<AptFile>::Dispose(lpFile);   // AptFile_::Dispose(v4)

    if (flags & 1)              // clrlwi. r11, r30, 31; beq ...
    {
        // X360 frees the literal 0x10 (16) -- the 32-bit-ABI byte size of an
        // AptLinkerThingy. Sized by the type here (semantic "free one node"); on
        // a 64-bit host this widens with the wider pointers, meaning unchanged.
        gpAptSharedPtrPool->Deallocate(this, sizeof(AptLinkerThingy));   // Deallocate(off_8324D808, this, 0x10)
    }

    return this;
}
