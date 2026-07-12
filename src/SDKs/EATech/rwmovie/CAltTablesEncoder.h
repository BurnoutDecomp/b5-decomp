// =====================================================================================
// CAltTablesEncoder -- the "alternate tables" variable-length-code encoder for the RTCMV/WMV
// video-object encoder (CWMVideoObjectEncoder). It owns eleven per-table CLocalHuffmanEncoder
// objects plus one flat symbol-record buffer. A frame is first measured (checkFrame), then
// every buffered record is emitted through the code table its selector field names
// (encodeFrame -> encodeSymbol); checkInRTCMV is the RTCMV per-record variant.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No reference
// source and no DecFIGS DWARF hints exist for this TU.
//
//   CAltTablesEncoder::CAltTablesEncoder   @0x82A7ECD0
//   CAltTablesEncoder::~CAltTablesEncoder  @0x82A7EDF8
//   CAltTablesEncoder::allocateSymbolArray @0x82A7E8E0
//   CAltTablesEncoder::clear               @0x82A7E970
//   CAltTablesEncoder::updateHistory       @0x82A7EA30
//   CAltTablesEncoder::checkInRTCMV        @0x82A7EAE8
//   CAltTablesEncoder::checkFrame          @0x82A7EB48
//   CAltTablesEncoder::encodeSymbol        @0x82A7EBD8
//   CAltTablesEncoder::encodeFrame         @0x82A7EC78
//
// Byte-addressed in the asm (this[N] == byte offset N*4); pointer members widen on this
// PC/x64 target, so the offsets below are documentary and access is by name.
//   +0x00 muReady          set to 1 once the tables are prepared (checkFrame / clear)
//   +0x04 muSymbolCapacity capacity (in records) of the symbol buffer
//   +0x08 muFrameFlag      set to (checkFrame's flag argument != 0)
//   +0x0C muNumTables      always 11
//   +0x10 muFrameParam     checkFrame's frame argument
//   +0x14 muCounter14      zeroed by clear
//   +0x18 muCounter18      zeroed by clear
//   +0x28 mpaTables[11]    per-table CLocalHuffmanEncoder*
//   +0x54 mpSymbolBase     owned symbol-record buffer (4 * capacity bytes)
//   +0x58 mpSymbolCursor   walk cursor into the buffer
//   +0x5C muSymbolCount    record count = (cursor - base), set by checkFrame
// =====================================================================================
#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwmovie/CLocalHuffmanEncoder.h"

class CAltTablesEncoder
{
public:
    // liCapacity = symbol-buffer capacity in records (a negative value skips the buffer
    // allocation); lpuError receives 1 on any allocation failure.
    CAltTablesEncoder(int liCapacity, u32* lpuError);
    ~CAltTablesEncoder();

    // (re)size the symbol buffer to hold at least luCount records; *lpuError = 1 on failure.
    void allocateSymbolArray(u32 luCount, u32* lpuError);

    // Reset for a new frame at the given table mode: (re)sizes the buffer to liSymbolCount,
    // clears every table's code buffer and rebuilds its codes. Returns the last table's
    // setCodes result.
    int  clear(int liMode, int liSymbolCount);

    // Measure the frame: sum every table's checkFrame(), record the buffered symbol count,
    // rewind the cursor. Returns the total symbol count contributed by the tables.
    int  checkFrame(int liFrameParam, u32 luFrameFlag);

    // Emit one RTCMV symbol record through table liTableIndex; returns the record's leading
    // byte (advancing the cursor).
    int  checkInRTCMV(int liTableIndex, void* lpContext);

    // Emit every buffered symbol to lpStream; returns the symbol count.
    int  encodeFrame(void* lpStream);

    // Emit the record at the cursor to lpStream via the table its selector field selects;
    // returns the record's leading byte, then advances the cursor.
    int  encodeSymbol(void* lpStream);

    // Snapshot each table's current code into its history slot when the table is dirty.
    void updateHistory();

private:
    static const int KI_NUM_TABLES = 11;

    u32                   muReady;            // +0x00
    u32                   muSymbolCapacity;   // +0x04
    u32                   muFrameFlag;        // +0x08
    u32                   muNumTables;        // +0x0C  (= 11)
    u32                   muFrameParam;       // +0x10
    u32                   muCounter14;        // +0x14
    u32                   muCounter18;        // +0x18
    u32                   muPad1C;            // +0x1C  (unobserved)
    u32                   muPad20;            // +0x20  (unobserved)
    u32                   muPad24;            // +0x24  (unobserved)
    CLocalHuffmanEncoder* mpaTables[KI_NUM_TABLES];  // +0x28
    u32*                  mpSymbolBase;       // +0x54
    u32*                  mpSymbolCursor;     // +0x58
    u32                   muSymbolCount;      // +0x5C
};
