// cLionBlockAlloc -- Lion runtime fixed-size block allocator implementation.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x8290A888 cLionBlockAlloc::Init
//   0x829082A0 cLionBlockAlloc::DeInit
//   0x82908328 cLionBlockAlloc::Alloc
// against the DWARF layout in LionBlockAlloc.h. Behaviour is store-for-store faithful to
// the X360 build; members are addressed by name.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBlockAlloc.h"

namespace
{
// The X360 build tags every Lion allocation with a (group, file, name) TagValuePair chain.
const char* const KPC_BLOCKALLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/LionBlockAlloc.cpp";
const char* const KPC_BLOCKALLOC_BITS = "cLionBlockAlloc::bits::";
const char* const KPC_BLOCKALLOC_DATA = "cLionBlockAlloc::data::";

// Tag ids used by the chain (matching the X360 TagValuePair construction order).
const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;
}  // namespace

void cLionBlockAlloc::Init(EA::Allocator::ITaggedAllocator* apAllocator, u32 aItemSize, u32 aItemCount)
{
    mpAllocator    = apAllocator;
    mItemSize      = static_cast<s32>(aItemSize);
    mItemCount     = static_cast<s32>(aItemCount);
    mBitBlockCount = static_cast<s32>((aItemCount + 31) / 32);
    mIndexLast     = 0;
    mAllocCount    = 0;

    // Bitmap: one 32-bit word per 32 items.
    EA::TagValuePair lBitsName(KU_TAG_NAME, static_cast<const void*>(KPC_BLOCKALLOC_BITS));
    EA::TagValuePair lBitsFile(KU_TAG_FILE, static_cast<const void*>(KPC_BLOCKALLOC_FILE));
    EA::TagValuePair lBitsLine(KU_TAG_LINE, static_cast<s32>(35));
    mpBits = static_cast<u32*>(mpAllocator->Alloc(
        4u * static_cast<u32>(mBitBlockCount),
        lBitsLine + lBitsFile + lBitsName));

    // Item storage: mItemCount * mItemSize bytes.
    EA::TagValuePair lDataName(KU_TAG_NAME, static_cast<const void*>(KPC_BLOCKALLOC_DATA));
    EA::TagValuePair lDataFile(KU_TAG_FILE, static_cast<const void*>(KPC_BLOCKALLOC_FILE));
    EA::TagValuePair lDataLine(KU_TAG_LINE, static_cast<s32>(39));
    mpData = static_cast<u8*>(mpAllocator->Alloc(
        static_cast<size_t>(mItemCount) * static_cast<size_t>(mItemSize),
        lDataLine + lDataFile + lDataName));

    for (s32 lIndex = 0; lIndex < mBitBlockCount; ++lIndex)
    {
        mpBits[lIndex] = 0;
    }

    mAllocCount = 0;
    mIndexLast  = 0;
}

void cLionBlockAlloc::DeInit()
{
    if (mpBits)
    {
        mpAllocator->Free(mpBits, 0);
        mpBits = nullptr;
    }
    if (mpData)
    {
        mpAllocator->Free(mpData, 0);
        mpData = nullptr;
    }
}

void* cLionBlockAlloc::Alloc()
{
    s32 lIndex = mIndexLast;
    s32 lLeft  = mBitBlockCount;
    if (!lLeft)
    {
        return nullptr;
    }

    s32 lBits;
    for (;;)
    {
        lBits = static_cast<s32>(mpBits[lIndex]);
        if (lBits != -1)
        {
            break;
        }
        if (++lIndex >= mBitBlockCount)
        {
            lIndex = 0;
        }
        if (!--lLeft)
        {
            return nullptr;
        }
    }

    // Find the first clear bit in this word.
    u32 lMask = 1;
    s32 lOff  = 0;
    while ((lMask & static_cast<u32>(lBits)) != 0)
    {
        lMask *= 2;
        ++lOff;
    }

    mpBits[lIndex] |= lMask;

    mIndexLast  = lIndex;
    mAllocCount = mAllocCount + 1;

    return mpData + (32 * lIndex + lOff) * mItemSize;
}

void cLionBlockAlloc::DeAlloc(void* apEntry)
{
    if (!apEntry)
    {
        return;
    }

    s32 lOff = static_cast<s32>((static_cast<u8*>(apEntry) - mpData) / mItemSize);
    u32 lMask = 1u << (lOff & 31);
    mpBits[lOff >> 5] &= ~lMask;
    mAllocCount = mAllocCount - 1;
}

void cLionBlockAlloc::Flush()
{
    for (s32 lIndex = 0; lIndex < mBitBlockCount; ++lIndex)
    {
        mpBits[lIndex] = 0;
    }
    mAllocCount = 0;
    mIndexLast  = 0;
}
