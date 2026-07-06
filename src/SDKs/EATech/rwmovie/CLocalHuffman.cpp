// =====================================================================================
// CLocalHuffman.cpp -- length-limited Huffman decode table for the RTCMV/WMV video path.
//   CLocalHuffman::CLocalHuffman   @0x82AC1920
//   CLocalHuffman::~CLocalHuffman  @0x82AC1AD0
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative).
// =====================================================================================

#include "SDKs/EATech/rwmovie/CLocalHuffman.h"

// CLocalHuffman::CLocalHuffman @0x82AC1920 -- build a canonical (length-limited)
// Huffman decode table for the RTCMV/WMV video path.
//
//   * muBits(+0x08)=luBits; muTableSize(+0x0C)=1<<luBits.
//   * The (numSymbols, code-length table) pair is picked from luBits:
//       luBits < 4 -> 77 symbols, KAU_CODE_LENGTHS_C0
//       luBits ==4 -> 12 symbols, KAU_CODE_LENGTHS_A0
//       luBits ==8 -> 73 symbols, KAU_CODE_LENGTHS_E38 (+ secondary byte table)
//       luBits ==10-> 64 symbols, KAU_CODE_LENGTHS_A0  (+ secondary byte table)
//       otherwise  -> 34 symbols, KAU_CODE_LENGTHS_E10
//   * For the 8/10 cases a single byte is consumed from a secondary length table
//     (mpNextByte, +0x18) giving muSecondaryBits(+0x14) and muSecondaryTableSize
//     (+0x10)=1<<that; the cursor is advanced one byte past the consumed entry.
//   * Allocates 8*numSymbols bytes via XMemAlloc(0x248C8000), zeroes it, splits it
//     into two numSymbols-word halves: the second half (mpauMaxCodes, +0x1C) is
//     filled with (1<<codeLength[i])-1 -- the per-symbol max-code mask.
//   * On allocation failure it sets *lpuError = 1 and returns.
CLocalHuffman::CLocalHuffman(u32 luBits, u32* lpuError)
{
    mpuBuffer = 0;
    muBits = luBits;
    muTableSize = 1u << luBits;

    switch (luBits)
    {
    case 4:
        muNumSymbols = 12;
        mpaCodeLengths = KAU_CODE_LENGTHS_A0;
        break;

    case 8:
        muNumSymbols = 73;
        mpaCodeLengths = KAU_CODE_LENGTHS_E38;
        mpNextByte = KAU_SECONDARY_LENGTHS_CF8;
        muSecondaryBits = *mpNextByte;
        ++mpNextByte;
        muSecondaryTableSize = 1u << muSecondaryBits;
        break;

    case 10:
        muNumSymbols = 64;
        mpaCodeLengths = KAU_CODE_LENGTHS_A0;
        mpNextByte = KAU_SECONDARY_LENGTHS_D90;
        muSecondaryBits = *mpNextByte;
        ++mpNextByte;
        muSecondaryTableSize = 1u << muSecondaryBits;
        break;

    default:
        if (luBits < 4)
        {
            muNumSymbols = 77;
            mpaCodeLengths = KAU_CODE_LENGTHS_C0;
        }
        else
        {
            muNumSymbols = 34;
            mpaCodeLengths = KAU_CODE_LENGTHS_E10;
        }
        break;
    }

    // Guard the 32-bit size computation: if 2*numSymbols would exceed 0x3FFFFFFF
    // the 8*numSymbols byte count has overflowed, so request an impossible -1.
    u32 luByteSize = 8u * muNumSymbols;
    if ((2u * muNumSymbols) > 0x3FFFFFFFu)
    {
        luByteSize = 0xFFFFFFFFu;
    }

    mpuBuffer = static_cast<u32*>(XMemAlloc(luByteSize, KU_HUFFMAN_XMEM_ATTRIBUTES));
    if (!mpuBuffer)
    {
        *lpuError = 1;
        return;
    }

    memset(mpuBuffer, 0, 8u * muNumSymbols);

    // Second numSymbols-word half of the buffer holds the per-symbol max-code masks.
    mpauMaxCodes = mpuBuffer + muNumSymbols;
    for (u32 luSymbol = 0; luSymbol < muNumSymbols; ++luSymbol)
    {
        u8 lu8Length = mpaCodeLengths[luSymbol];
        mpauMaxCodes[luSymbol] = (1u << lu8Length) - 1u;
    }
}

// CLocalHuffman::~CLocalHuffman @0x82AC1AD0 -- free the single owned buffer.
CLocalHuffman::~CLocalHuffman()
{
    if (mpuBuffer)
    {
        XMemFree(mpuBuffer, KU_HUFFMAN_XMEM_ATTRIBUTES);
        mpuBuffer = 0;
    }
}
