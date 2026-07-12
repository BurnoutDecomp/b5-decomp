// =====================================================================================
// CAltTablesEncoder.cpp -- alternate variable-length-code table encoder for the RTCMV/WMV
// video path. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC asm authoritative).
//
//   CAltTablesEncoder::CAltTablesEncoder   @0x82A7ECD0
//   CAltTablesEncoder::allocateSymbolArray @0x82A7E8E0
//   CAltTablesEncoder::clear               @0x82A7E970
//   CAltTablesEncoder::updateHistory       @0x82A7EA30
//   CAltTablesEncoder::checkInRTCMV        @0x82A7EAE8
//   CAltTablesEncoder::checkFrame          @0x82A7EB48
//   CAltTablesEncoder::encodeSymbol        @0x82A7EBD8
//   CAltTablesEncoder::encodeFrame         @0x82A7EC78
//   CAltTablesEncoder::~CAltTablesEncoder  @0x82A7EDF8
//
// XMemAlloc / XMemFree and KU_HUFFMAN_XMEM_ATTRIBUTES (0x248C8000) come from CLocalHuffman.h
// (via the encoder header) -- the raw XDK heap API, called with no `this`, the same
// convention every sibling rwmovie TU uses. On the X360 the object sizes are literals
// (0x4C per table); here they are sizeof() so the layout widens correctly on PC/x64.
// =====================================================================================

#include "SDKs/EATech/rwmovie/CAltTablesEncoder.h"

#include <new>       // placement new for the per-table CLocalHuffmanEncoder objects
#include <cstring>   // memset

#include "SDKs/EATech/rwmovie/CLocalHuffmanEncoder.h"

// Foreign codec global (X360 dword_8322C3F0) written by clear(): 1 when the requested table
// mode is "short" (<= 5), else 0. It is owned by another TU; declared extern here so the
// store side-effect is preserved.
extern int g_iAltTablesShortCodeFlag;

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::CAltTablesEncoder  @0x82A7ECD0
//
// Zero the eleven table slots, then -- when liCapacity >= 0 -- allocate the flat
// 4*capacity-byte symbol buffer (bailing with *lpuError = 1 on failure). Finally, unless an
// error is already flagged, allocate + construct one CLocalHuffmanEncoder per table slot,
// stopping the moment a sub-construction flags *lpuError.
// -------------------------------------------------------------------------------------
CAltTablesEncoder::CAltTablesEncoder(int liCapacity, u32* lpuError)
{
    muNumTables = KI_NUM_TABLES;
    muFrameFlag = 0;
    for (u32 luTable = 0; luTable < muNumTables; ++luTable)
        mpaTables[luTable] = nullptr;
    mpSymbolBase     = nullptr;
    muSymbolCapacity = 0;

    if (liCapacity >= 0)
    {
        muSymbolCapacity = static_cast<u32>(liCapacity);
        mpSymbolCursor   = nullptr;

        // Guard the 32-bit byte-size computation: 4*capacity overflowing 0x3FFFFFFF requests
        // an impossible -1 so the allocation fails deterministically.
        u32 luByteSize = static_cast<u32>(liCapacity) * sizeof(u32);
        if (static_cast<u32>(liCapacity) > 0x3FFFFFFFu)
            luByteSize = 0xFFFFFFFFu;

        mpSymbolBase   = static_cast<u32*>(XMemAlloc(luByteSize, KU_HUFFMAN_XMEM_ATTRIBUTES));
        mpSymbolCursor = mpSymbolBase;
        if (mpSymbolBase == nullptr)
        {
            *lpuError = 1;
            return;
        }
    }

    if (*lpuError == 0)
    {
        for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)
        {
            void* lpMem = XMemAlloc(sizeof(CLocalHuffmanEncoder), KU_HUFFMAN_XMEM_ATTRIBUTES);
            mpaTables[liTable] = lpMem
                ? new (lpMem) CLocalHuffmanEncoder(liTable, this, lpuError)
                : nullptr;
            if (*lpuError != 0)
                break;
        }
    }
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::allocateSymbolArray  @0x82A7E8E0
//
// Grow the symbol buffer to hold luCount records when the request exceeds the current
// capacity: free any existing buffer, record the new capacity, and allocate 4*luCount bytes
// (same overflow guard as the ctor). *lpuError = 1 on failure. A request within the current
// capacity is a no-op. (The X360 leaves the allocation pointer in the return register; no
// caller consumes it, so this is void.)
// -------------------------------------------------------------------------------------
void CAltTablesEncoder::allocateSymbolArray(u32 luCount, u32* lpuError)
{
    if (luCount < muSymbolCapacity)
        return;

    if (mpSymbolBase != nullptr)
    {
        XMemFree(mpSymbolBase, KU_HUFFMAN_XMEM_ATTRIBUTES);
        mpSymbolBase = nullptr;
    }

    muSymbolCapacity = luCount;
    mpSymbolCursor   = nullptr;

    u32 luByteSize = luCount * sizeof(u32);
    if (luCount > 0x3FFFFFFFu)
        luByteSize = 0xFFFFFFFFu;

    mpSymbolBase   = static_cast<u32*>(XMemAlloc(luByteSize, KU_HUFFMAN_XMEM_ATTRIBUTES));
    mpSymbolCursor = mpSymbolBase;
    if (mpSymbolBase == nullptr)
        *lpuError = 1;
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::clear  @0x82A7E970
//
// Prepare for a fresh frame at table mode liMode: publish the short-code global, (re)size the
// buffer to liSymbolCount records, rewind the cursor, reset the two counters and the ready
// flag, then for every table clear its code buffer and rebuild its code words. Returns the
// last table's setCodes result.
// -------------------------------------------------------------------------------------
int CAltTablesEncoder::clear(int liMode, int liSymbolCount)
{
    g_iAltTablesShortCodeFlag = (liMode <= 5) ? 1 : 0;

    if (liSymbolCount > 0)
    {
        u32 luError = 0;
        allocateSymbolArray(static_cast<u32>(liSymbolCount), &luError);
    }

    mpSymbolCursor = mpSymbolBase;
    muCounter18    = 0;
    muCounter14    = 0;
    muReady        = 1;

    int liResult = 0;
    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)
    {
        CLocalHuffmanEncoder* lpEnc = mpaTables[liTable];

        u32 luByteSize = 4u * lpEnc->GetNumSymbols();
        lpEnc->muDirtyFlag = 0;
        memset(lpEnc->mpauCodeBuffer, 0, luByteSize);
        liResult = lpEnc->setCodes(liMode);
    }
    return liResult;
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::updateHistory  @0x82A7EA30
//
// For every table, snapshot its current code into its history slot when the table is dirty
// -- either its own dirty flag is set, or the object it references (mpState) is dirty at
// +0x08. The mpState dereference is guarded by short-circuit exactly as the asm branches.
// -------------------------------------------------------------------------------------
void CAltTablesEncoder::updateHistory()
{
    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)
    {
        CLocalHuffmanEncoder* lpEnc = mpaTables[liTable];
        if (lpEnc->muDirtyFlag != 0 || lpEnc->mpState->muDirty != 0)
            lpEnc->muHistoryCode = lpEnc->muCurrentCode;
    }
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::checkInRTCMV  @0x82A7EAE8
//
// Clear the record at the cursor, emit one RTCMV symbol through table liTableIndex, then read
// back the record's leading byte and advance the cursor. On the big-endian console the leading
// (low-address) byte is the packed word's most-significant byte, i.e. bits 24..31 here.
// -------------------------------------------------------------------------------------
int CAltTablesEncoder::checkInRTCMV(int liTableIndex, void* lpContext)
{
    *mpSymbolCursor = 0;
    mpaTables[liTableIndex]->checkInRTCMV(lpContext);

    int liResult = static_cast<u8>(*mpSymbolCursor >> 24);
    ++mpSymbolCursor;
    return liResult;
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::checkFrame  @0x82A7EB48
//
// Measure a frame: set the frame flag from luFrameFlag, sum every table's checkFrame(), then
// record the frame parameter, mark ready, rewind the cursor to the buffer base, and store the
// buffered record count = (cursor - base). Returns the summed table symbol count.
// -------------------------------------------------------------------------------------
int CAltTablesEncoder::checkFrame(int liFrameParam, u32 luFrameFlag)
{
    muFrameFlag = (luFrameFlag != 0) ? 1u : 0u;

    int liTotal = 0;
    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)
        liTotal += mpaTables[liTable]->checkFrame();

    u32* lpBase   = mpSymbolBase;
    u32* lpCursor = mpSymbolCursor;

    muFrameParam   = static_cast<u32>(liFrameParam);
    muReady        = 1;
    mpSymbolCursor = lpBase;
    muSymbolCount  = static_cast<u32>(lpCursor - lpBase);
    return liTotal;
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::encodeSymbol  @0x82A7EBD8
//
// Emit the record at the cursor. Its selector field (bits 18..23 of the packed word) picks the
// code table: selectors 8 and 10 are literal/escape records emitted via WriteSymbol; every
// other selector is a VLC record emitted via CLocalHuffmanEncoder::encodeSymbol. The returned
// byte is the record's leading (MS) byte, except for selectors < 4 which return 1. The cursor
// is advanced one record afterwards.
// -------------------------------------------------------------------------------------
int CAltTablesEncoder::encodeSymbol(void* lpStream)
{
    u32* lpCursor   = mpSymbolCursor;
    u32  luSelector = (*lpCursor >> 18) & 0x3Fu;

    int liResult;
    if (luSelector == 8 || luSelector == 10)
    {
        mpaTables[luSelector]->WriteSymbol(lpStream);
        liResult = static_cast<u8>(*mpSymbolCursor >> 24);
    }
    else
    {
        CLocalHuffmanEncoder* lpEnc = mpaTables[luSelector];
        if (luSelector < 4)
            liResult = 1;
        else
            liResult = static_cast<u8>(*lpCursor >> 24);
        lpEnc->encodeSymbol(lpStream);
    }

    ++mpSymbolCursor;
    return liResult;
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::encodeFrame  @0x82A7EC78
//
// Rewind the cursor to the buffer base and emit every buffered symbol to lpStream. Returns the
// symbol count.
// -------------------------------------------------------------------------------------
int CAltTablesEncoder::encodeFrame(void* lpStream)
{
    mpSymbolCursor = mpSymbolBase;

    for (int liSymbol = 0; liSymbol < static_cast<int>(muSymbolCount); ++liSymbol)
        encodeSymbol(lpStream);

    return static_cast<int>(muSymbolCount);
}

// -------------------------------------------------------------------------------------
// CAltTablesEncoder::~CAltTablesEncoder  @0x82A7EDF8
//
// For every constructed table: free its two encoder-side scratch buffers, run the base
// CLocalHuffman destructor (which frees the code table), free the encoder object itself, and
// null the slot. Finally free the symbol buffer. The base destructor is invoked explicitly --
// matching the asm, which open-codes the per-table teardown rather than a derived dtor.
// -------------------------------------------------------------------------------------
CAltTablesEncoder::~CAltTablesEncoder()
{
    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)
    {
        CLocalHuffmanEncoder* lpEnc = mpaTables[liTable];
        if (lpEnc != nullptr)
        {
            if (lpEnc->mpauCodeData != nullptr)
            {
                XMemFree(lpEnc->mpauCodeData, KU_HUFFMAN_XMEM_ATTRIBUTES);
                lpEnc->mpauCodeData = nullptr;
            }
            if (lpEnc->mpauCodeBuffer != nullptr)
            {
                XMemFree(lpEnc->mpauCodeBuffer, KU_HUFFMAN_XMEM_ATTRIBUTES);
                lpEnc->mpauCodeBuffer = nullptr;
            }
            lpEnc->CLocalHuffman::~CLocalHuffman();
            XMemFree(lpEnc, KU_HUFFMAN_XMEM_ATTRIBUTES);
            mpaTables[liTable] = nullptr;
        }
    }

    if (mpSymbolBase != nullptr)
    {
        XMemFree(mpSymbolBase, KU_HUFFMAN_XMEM_ATTRIBUTES);
        mpSymbolBase = nullptr;
    }
}
