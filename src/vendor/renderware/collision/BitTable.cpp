#include "vendor/renderware/collision/BitTable.hpp"

// ===========================================================================
// rw::BitTable -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BitTable::Initialize  @ 0x82BA7F38
//
// Faithful transcription of the PPC integer asm (no SIMD): write the
// width/height/word-count header into the caller-supplied backing store, then
// zero-fill the packed bit words. See BitTable.hpp for the store layout.
// ===========================================================================

namespace rw
{

// ---------------------------------------------------------------------------
// BitTable::Initialize @ 0x82BA7F38
//
//   lwz   r11, 0(r3)            -> lpStorage = *lppStorage
//   cmplwi r11, 0 ; beq ...     -> if non-null:
//     mullw r10, r4, r5         -> width * height
//     stw   r4, 0(r11)          ->   muWidth  = width
//     addi  r10, r10, 0x1F      ->   + 31
//     stw   r5, 4(r11)          ->   muHeight = height
//     srwi  r10, r10, 5         ->   wordCount = (width*height + 31) >> 5
//     stw   r10, 8(r11)         ->   muWordCount = wordCount
//   else lpStorage = 0
//   then: zero maBits[0 .. muWordCount-1]   (the do/while at +0xC)
// ---------------------------------------------------------------------------
BitTable::Storage* BitTable::Initialize(Storage** lppStorage, int liWidth, int liHeight)
{
    Storage* lpStorage = *lppStorage;
    if (lpStorage != nullptr)
    {
        lpStorage->muWidth     = static_cast<u32>(liWidth);
        lpStorage->muHeight    = static_cast<u32>(liHeight);
        lpStorage->muWordCount = static_cast<u32>(liWidth * liHeight + 31) >> 5;
    }

    // Zero-fill the packed words. The asm re-reads muWordCount (+8) from the
    // backing store each iteration of the loop guard; preserved here.
    u32 luIndex = 0;
    if (lpStorage->muWordCount != 0)
    {
        u32* lpWord = lpStorage->maBits;
        do
        {
            ++luIndex;
            *lpWord++ = 0;
        }
        while (luIndex < lpStorage->muWordCount);
    }

    return lpStorage;
}

} // namespace rw
