#pragma once

// =====================================================================
//  AptLinkerThingy.h  --  EA APT (ActionScript Player Technology) middleware
//
//  AptLinkerThingy is the small (16-byte) pool-allocated node the APT linker
//  (AptLinker) tracks per loaded movie/file. It owns one AptFile reference (a
//  reference-counted AptSharedPtr<AptFile> control object) which it disposes on
//  destruction; the remaining slots are linker bookkeeping touched by the
//  AptLinker Load/Update/ConvertToZombie paths (not by destruction).
//
//  NO DWARF exists for this class (X360-only; not in the PS3 DecFIGS dump nor
//  the EATech public Apt.h). The shape below is derived STRICTLY from the X360
//  binary scalar-deleting-destructor:
//      AptLinkerThingy::`scalar deleting destructor'  @ 0x82AE5128
//
//  That body reads only +0x04 (the AptFile reference) and the object size
//  (16 == 0x10, the pool block size). The +0x00 / +0x08 / +0x0C slots are
//  preserved as named bookkeeping fields the linker fills in elsewhere; they
//  are untouched by destruction.
//
//  LAYOUT (16 bytes):
//      +0x00  mpLinkPrev  linker list link / owner (untouched by dtor)
//      +0x04  mpFile      owned AptFile reference (disposed, then zeroed)
//      +0x08  muState     linker bookkeeping (untouched by dtor)
//      +0x0C  mpLinkNext  linker list link (untouched by dtor)
// =====================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtr<AptFile>::Dispose, gpAptSharedPtrPool
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager

struct AptFile;

struct AptLinkerThingy
{
    // ---- layout (16 bytes; only +0x04 is read by the dtor) ----
    void*    mpLinkPrev;   // [0x00] linker list link / owner
    AptFile* mpFile;       // [0x04] owned AptFile reference
    u32      muState;      // [0x08] linker bookkeeping
    void*    mpLinkNext;   // [0x0C] linker list link

    // X360 @0x82AE5128 -- MSVC `scalar deleting destructor`. Disposes the owned
    // AptFile reference (load it, zero +0x04, then AptSharedPtr<AptFile>::Dispose
    // it -- store/zero before the call is preserved), then, when bit0 of flags is
    // set, frees the 16-byte block back to the Apt pool. Returns `this`.
    //   flags: the hidden MSVC second parameter (bit0 = also free the block).
    void* ScalarDeletingDestructor(char flags);
};
