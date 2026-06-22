#pragma once

// cLionBlockAlloc -- Lion runtime fixed-size block allocator
// (SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBlockAlloc.h).
//
// A bitmap pool of mItemCount fixed-size items (mItemSize bytes each). One 32-bit word per
// 32 items in mpBits tracks occupancy; the backing item storage is mpData. Init allocates
// both arrays through the tagged allocator, Alloc claims the first free slot, DeAlloc clears
// its bit, DeInit frees the two arrays.
//
// Layout recovered from the X360 DWARF (LionBlockAlloc.h) and the Init/Alloc/DeInit asm:
//   +0x00 mpAllocator   +0x04 mpBits   +0x08 mpData   +0x0C mItemCount
//   +0x10 mItemSize     +0x14 mAllocCount   +0x18 mIndexLast   +0x1C mBitBlockCount
//
// Vendor code (eauk_lion); reconstructed in its canonical Lion home.

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

struct cLionBlockAlloc
{
public:
    void  Init(EA::Allocator::ITaggedAllocator* apAllocator, u32 aItemSize, u32 aItemCount);
    void  DeInit();
    void  Flush();

    void* Alloc();
    void  DeAlloc(void* apEntry);

    s32   GetAllocCount() const { return mAllocCount; }
    s32   GetItemCount() const { return mItemCount; }

private:
    EA::Allocator::ITaggedAllocator* mpAllocator;  // +0x00
    u32* mpBits;                                   // +0x04 occupancy bitmap (1 bit/item)
    u8*  mpData;                                   // +0x08 backing item storage
    s32  mItemCount;                               // +0x0C number of items
    s32  mItemSize;                                // +0x10 bytes per item
    s32  mAllocCount;                              // +0x14 live allocations
    s32  mIndexLast;                               // +0x18 search cursor (bit-block index)
    s32  mBitBlockCount;                           // +0x1C number of 32-bit bitmap words
};
