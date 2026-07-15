// =====================================================================================
// CLocalHuffman -- length-limited Huffman decode table for the RTCMV/WMV video path.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CLocalHuffman::CLocalHuffman   @0x82AC1920
//   CLocalHuffman::~CLocalHuffman  @0x82AC1AD0
//
// Base pointer in the asm is byte-addressed (int* a1 with a1[N] == byte offset N*4).
// The constructor selects a (symbol-count, code-length-table) pair from the requested
// index-bit width, allocates one buffer of 8*numSymbols bytes via the raw XDK XMemAlloc
// (attributes 0x248C8000), zeroes it, and fills the second numSymbols-word half with the
// per-symbol max-code masks (1<<codeLength)-1. The destructor frees that one buffer.
//
// The five code-length tables (KAU_CODE_LENGTHS_*) and the two secondary byte tables
// (KAU_SECONDARY_LENGTHS_*) are static rodata blobs in the XEX (unk_82123DB0 / _82123DC0
// / _82123E10 / _82123E38 and byte_82F70CF8 / byte_82F70D90). Their byte contents are not
// reproduced here and are declared extern (unsized) so the definitions can be dropped in
// when the rodata is extracted -- note unk_82123DB0 is shared by the 4-bit (12-symbol) and
// 10-bit (64-symbol) cases, so it must NOT be bounded at 12.
// =====================================================================================
#pragma once

#include "types.hpp"

#include <cstring>   // memset

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm tail-calls the XDK XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD)
// directly (bl XMemAlloc / bl XMemFree, r3=size/pAddress, r4=attributes), with NO `this`
// argument -- i.e. the raw platform API, NOT the XAUDIO::CXMemMemoryManager wrapper.
// Declared here so this rwmovie TU stands alone.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// XMemAlloc/XMemFree attribute word used for this codec's tables (lis 0x248C / ori 0x8000).
static const u32 KU_HUFFMAN_XMEM_ATTRIBUTES = 0x248C8000u;

// ---- static code-length / secondary tables (XEX rodata) -----------------------------
extern const u8 KAU_CODE_LENGTHS_A0[];           // unk_82123DB0 (shared: 4-bit=12, 10-bit=64)
extern const u8 KAU_CODE_LENGTHS_C0[];           // unk_82123DC0 (bits < 4, 77 symbols)
extern const u8 KAU_CODE_LENGTHS_E10[];          // unk_82123E10 (default, 34 symbols)
extern const u8 KAU_CODE_LENGTHS_E38[];          // unk_82123E38 (8-bit, 73 symbols)
extern const u8 KAU_SECONDARY_LENGTHS_CF8[];     // byte_82F70CF8 (8-bit secondary table)
extern const u8 KAU_SECONDARY_LENGTHS_D90[];     // byte_82F70D90 (10-bit secondary table)

class CLocalHuffman
{
public:
    // luBits = index-bit width; lpuError receives 1 on allocation failure.
    CLocalHuffman(u32 luBits, u32* lpuError);
    ~CLocalHuffman();

    // ---- additive: reached through the derived CLocalHuffmanEncoder ---------------------
    // Number of coded symbols (base member, +0x00). CAltTablesEncoder::clear reads it as
    // 4 * numSymbols to size the per-table code-buffer memset.
    u32 GetNumSymbols() const { return muNumSymbols; }

    // CLocalHuffman::setCodes -- (re)builds this table's canonical code words for the
    // requested table-selection mode. It is a separate (not-yet-reconstructed) X360 TU;
    // declared here so CAltTablesEncoder::clear can call it through a CLocalHuffmanEncoder.
    int setCodes(int liMode);

protected:
    // Reached by name from the derived CLocalHuffmanEncoder encode path (WriteSymbol /
    // encodeSymbol / encodeHeader / checkFrame). These are the same base fields the X360
    // asm addresses as this[0..8]; the derived methods read them and checkFrame advances
    // mpaCodeLengths by muNumSymbols, so they are protected rather than private.
    u32       muNumSymbols;          // +0x00  number of coded symbols
    const u8* mpaCodeLengths;        // +0x04  per-symbol code-length table
    u32       muBits;                // +0x08  primary index bit width
    u32       muTableSize;           // +0x0C  1 << muBits
    u32       muSecondaryTableSize;  // +0x10  1 << muSecondaryBits (8/10-bit only)
    u32       muSecondaryBits;       // +0x14  secondary index bit width (8/10-bit only)
    const u8* mpNextByte;            // +0x18  cursor into the secondary byte table
    u32*      mpauMaxCodes;          // +0x1C  second half of mpuBuffer: (1<<len)-1 per symbol
    u32*      mpuBuffer;             // +0x20  owned 8*numSymbols-byte allocation
};
