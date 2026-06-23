#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"

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
    AptFile* lpFile = mpFile;   // v4 = *(this+4)
    mpFile = nullptr;           // *(this+4) = 0

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
