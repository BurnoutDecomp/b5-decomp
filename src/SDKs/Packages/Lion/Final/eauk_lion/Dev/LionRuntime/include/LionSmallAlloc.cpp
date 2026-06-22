// LionSmallAlloc -- small-block allocator implementation.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x82908AE0 LionSmallAlloc::DeInit
//   0x82912D18 LionSmallAlloc::PageCreate
//   0x8290EFD0 LionSmallAlloc::sLSAPage::Init
//   0x8290AEF0 LionSmallAlloc::sLSAMerge::BinsBuild
//   0x8290AE70 LionSmallAlloc::sLSAPage::BinItemLink
// against the DWARF layouts in LionSmallAlloc.h. Store-for-store faithful; members by name.
// These five functions are this TU's full recon ledger.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSmallAlloc.h"

namespace LionSmallAlloc
{

// Module statics (LionSmallAlloc.cpp:114/115). Page list head + backing tagged allocator.
sLSAPage*                        mpPages    = nullptr;
EA::Allocator::ITaggedAllocator* mpAllocator = nullptr;

namespace
{
const char* const KPC_SMALLALLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/LionSmallAlloc.cpp";
const char* const KPC_SMALLALLOC_PAGE = "LionSmallAlloc::PageCreate::";

const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;

// The largest free block carved per pass and the per-page bin index it lands in.
const u32 KU_MAX_BLOCK   = 0xFE;   // 254 bytes
const u32 KU_SPLIT_LIMIT = 0x106;  // 262: above this, carve another full 254-byte block
}  // namespace

void DeInit()
{
    sLSAPage* lpPage = mpPages;
    while (lpPage)
    {
        sLSAPage* lpNext = lpPage->mpNext;
        mpAllocator->Free(lpPage, 0);
        lpPage = lpNext;
    }
}

sLSAPage* PageCreate(u32 aPageSize)
{
    EA::TagValuePair lName(KU_TAG_NAME, static_cast<const void*>(KPC_SMALLALLOC_PAGE));
    EA::TagValuePair lFile(KU_TAG_FILE, static_cast<const void*>(KPC_SMALLALLOC_FILE));
    EA::TagValuePair lLine(KU_TAG_LINE, static_cast<s32>(417));

    sLSAPage* lpPage = static_cast<sLSAPage*>(
        mpAllocator->Alloc(aPageSize, lLine + lFile + lName));
    if (!lpPage)
    {
        return lpPage;
    }

    lpPage->Init(aPageSize);

    // Append to the tail of the page list.
    sLSAPage** lppSlot = &mpPages;
    for (sLSAPage* lpCur = mpPages; lpCur; lpCur = lpCur->mpNext)
    {
        lppSlot = &lpCur->mpNext;
    }
    *lppSlot = lpPage;

    return lpPage;
}

void sLSAPage::Init(u32 aPageSize)
{
    for (s32 lIndex = 0; lIndex < 255; ++lIndex)
    {
        mpFreeBins[lIndex] = nullptr;
    }

    mAllocCount = 0;
    mAllocBytes = 0;
    mMainFlag   = 0;
    mpNext      = nullptr;

    // The sub-allocatable region follows the 0x414-byte header inline; mPageSize excludes it.
    mpData    = reinterpret_cast<u8*>(this) + 0x414;
    mPageSize = static_cast<s32>(aPageSize) - 0x414;

    // Front guard: an 8-byte in-use sentinel at the start of the region.
    sLSABinItem* lpFront = reinterpret_cast<sLSABinItem*>(mpData);
    mpData[0] = 8;       // size byte
    mpData[7] = 8;       // trailing size byte
    mpData[0] = static_cast<u8>(mpData[0] | 1);  // set in-use flag
    lpFront->mPrevOff = 0;
    lpFront->mNextOff = 0;

    // Back guard: an 8-byte in-use sentinel at the end of the region.
    u8* lpEnd = mpData + mPageSize;
    lpEnd[-8] = 8;
    lpEnd[-1] = 8;
    lpEnd[-8] = static_cast<u8>(lpEnd[-8] | 1);
    sLSABinItem* lpBack = reinterpret_cast<sLSABinItem*>(lpEnd - 8);
    lpBack->mPrevOff = 0;
    lpBack->mNextOff = 0;

    // Carve everything between the two guards into free bins.
    sLSAMerge lMerge;
    lMerge.mpBin  = reinterpret_cast<sLSABinItem*>(mpData + 8);
    lMerge.mSize  = static_cast<u32>(mPageSize - 16);
    lMerge.mpPage = this;
    lMerge.BinsBuild();
}

void sLSAPage::BinItemLink(sLSABinItem* apItem, u32 aSize)
{
    if (!apItem || aSize > KU_MAX_BLOCK)
    {
        return;
    }

    if ((aSize & 1) != 0)
    {
        // Odd sizes are invalid for the 2-byte-granular free list (X360 traps here).
        *reinterpret_cast<volatile int*>(0) = 0;
    }

    u8* lpBytes = reinterpret_cast<u8*>(apItem);
    lpBytes[0]          = static_cast<u8>(aSize & 0xFE);  // header size byte (even)
    lpBytes[aSize - 1]  = static_cast<u8>(aSize);         // trailing size byte
    lpBytes[0]          = static_cast<u8>(lpBytes[0] & ~1u);  // clear in-use flag

    sLSABinItem* lpHead = mpFreeBins[aSize];
    apItem->mPrevOff = 0;
    if (lpHead)
    {
        lpHead->mPrevOff = static_cast<u16>(reinterpret_cast<u8*>(apItem) - mpData);
        apItem->mNextOff = static_cast<u16>(reinterpret_cast<u8*>(lpHead) - mpData);
    }
    else
    {
        apItem->mNextOff = 0;
    }
    mpFreeBins[aSize] = apItem;
}

void sLSAMerge::BinsBuild()
{
    sLSABinItem* lpBin = mpBin;

    // Carve full 254-byte free blocks while more than KU_SPLIT_LIMIT bytes remain.
    while (mSize > KU_SPLIT_LIMIT)
    {
        u8* lpBytes = reinterpret_cast<u8*>(lpBin);
        if (lpBytes)
        {
            lpBytes[0]    = static_cast<u8>(KU_MAX_BLOCK);  // 254
            lpBytes[0xFD] = static_cast<u8>(KU_MAX_BLOCK);  // trailing byte at +253
            lpBytes[0]    = static_cast<u8>(lpBytes[0] & ~1u);
            lpBin->mPrevOff = 0;

            sLSABinItem* lpHead = mpPage->mpFreeBins[KU_MAX_BLOCK];
            if (lpHead)
            {
                lpHead->mPrevOff = static_cast<u16>(lpBytes - mpPage->mpData);
                lpBin->mNextOff  = static_cast<u16>(reinterpret_cast<u8*>(lpHead) - mpPage->mpData);
            }
            else
            {
                lpBin->mNextOff = 0;
            }
            mpPage->mpFreeBins[KU_MAX_BLOCK] = lpBin;
        }

        lpBin = reinterpret_cast<sLSABinItem*>(reinterpret_cast<u8*>(lpBin) + KU_MAX_BLOCK);
        mSize -= KU_MAX_BLOCK;
    }

    // Remainder: one block, or two halves if still larger than a single bin.
    u32 lSize = mSize;
    if (lSize > KU_MAX_BLOCK)
    {
        u32 lFirst  = lSize >> 1;
        u32 lSecond = lSize >> 1;
        if (((lSize >> 1) & 1) != 0)
        {
            ++lFirst;
            --lSecond;
        }
        mpPage->BinItemLink(lpBin, lFirst);
        lSize = lSecond;
        lpBin = reinterpret_cast<sLSABinItem*>(reinterpret_cast<u8*>(lpBin) + lFirst);
    }
    mpPage->BinItemLink(lpBin, lSize);
}

}  // namespace LionSmallAlloc
