#pragma once

// LionSmallAlloc -- Lion runtime small-block (sub-page) allocator
// (SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSmallAlloc.h).
//
// A module-level allocator that carves fixed pages from the tagged allocator and sub-
// allocates small blocks within each page using a boundary-tag free list bucketed by size
// (mpFreeBins[255], one bucket per byte size 0..254). Items are linked through 16-bit
// offsets relative to the page's mpData, so a whole page is position-independent.
//
// Layouts recovered from the X360 DWARF (LionSmallAlloc.cpp) and the
// Init/PageCreate/BinsBuild/BinItemLink/sLSAPage::Init asm:
//   sLSABinItem (boundary tag): +0 mSize (low bit = alloc flag) | +1 mPad |
//                               +2 mPrevOff (u16, rel mpData) | +4 mNextOff (u16, rel mpData)
//                               trailing size byte at item[size-1]
//   sLSAPage:  +0x000 mpFreeBins[255]  +0x3FC mAllocCount  +0x400 mAllocBytes
//              +0x404 mPageSize  +0x408 mMainFlag  +0x40C mpData (= page+0x414)  +0x410 mpNext
//   sLSAMerge (transient): +0 mpBin  +4 mSize  +8 mpPage
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home.

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

namespace LionSmallAlloc
{

// A free-block boundary tag. Header lives at the block start; a duplicate size byte lives
// at block[size-1]. Free blocks are doubly linked through mPrevOff/mNextOff which are byte
// offsets relative to the owning page's mpData (0 == none).
struct sLSABinItem
{
    u8  mSize;     // +0x00 size in bytes; low bit is the in-use flag
    u8  mPad;      // +0x01
    u16 mPrevOff;  // +0x02 previous free block, offset from page mpData
    u16 mNextOff;  // +0x04 next free block, offset from page mpData
};

struct sLSAPage
{
    sLSABinItem* mpFreeBins[255];  // +0x000 free-list head per size class
    s32          mAllocCount;      // +0x3FC
    s32          mAllocBytes;      // +0x400
    s32          mPageSize;        // +0x404 usable bytes (requested size - header)
    u32          mMainFlag;        // +0x408
    u8*          mpData;           // +0x40C base of the sub-allocatable region (= this+0x414)
    sLSAPage*    mpNext;           // +0x410 next page in the list

    void Init(u32 aPageSize);
    void BinItemLink(sLSABinItem* apItem, u32 aSize);
};

// Transient cursor used while carving a fresh page's data region into free bins.
struct sLSAMerge
{
    sLSABinItem* mpBin;   // +0x00 current block being carved
    u32          mSize;   // +0x04 bytes remaining to carve
    sLSAPage*    mpPage;  // +0x08 owning page

    void BinsBuild();
};

// LionSmallAlloc::Init (DWARF LionSmallAlloc.cpp:143). Bind the backing allocator and the
// two page sizes, then carve the first ("main") page. ⭐ THERE IS NO STANDALONE BODY IN THE
// X360 IMAGE: the compiler inlined it into cLionFX::Init @0x82914A98, where the whole of it
// is visible store-for-store (0x82914AA8..0x82914B04). Re-outlined here as the source's own
// function -- the DWARF names it, its parameters and its two globals.
void      Init(EA::Allocator::ITaggedAllocator* apAllocator, u32 aMainSize, u32 aGrowthSize);
void      DeInit();
sLSAPage* PageCreate(u32 aPageSize);

// Module statics (LionSmallAlloc.cpp:114/115/117/118 -- the DWARF names all four).
//   mpAllocator @0x83121D90   mpPages @0x83121D88
//   mGrowthSize @0x83121D84   mMainSize @0x83121D8C
// ⚠ WHICH SIZE IS WHICH IS SETTLED BY THE CALL, NOT BY THE ADDRESS ORDER. cLionFX::Init
// loads r3 = 0x10000 (`lis r3,1` @0x82914ABC), stores 0x10000 into +0x1D8C and 0x1000 into
// +0x1D84, and passes the UNTOUCHED r3 to PageCreate @0x82914AF0 -- and the page that comes
// back is immediately stamped mMainFlag = 1. So the 64 KB word is mMainSize and the 4 KB word
// is mGrowthSize, not the reverse.
extern sLSAPage*                        mpPages;
extern EA::Allocator::ITaggedAllocator* mpAllocator;
extern u32                              mMainSize;
extern u32                              mGrowthSize;

}  // namespace LionSmallAlloc
