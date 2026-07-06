// =====================================================================================
// COutBitStream -- MSB-first bit writer for the RTCMV/WMV (WMV9/VC-1) video encode path.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   COutBitStream::COutBitStream    @0x82A8BD38
//   COutBitStream::MassageData      @0x82A8BC60
//   COutBitStream::attach           @0x82A8BCF8
//   COutBitStream::flush            @0x82A8BF68
//   COutBitStream::reset            @0x82A8BD18  (sibling; not in this batch)
//   COutBitStream::putBits          @0x82A8BD78  (sibling; not in this batch)
//   COutBitStream::flushByteAlign   @0x82A8C078  (sibling; not in this batch)
//
// Base pointer in the asm is byte-addressed (_DWORD* this with this[N] == byte offset N*4).
// Layout (60 bytes, no vtable):
//   +0x00 mpBufferStart    start of the output buffer (a2); reset origin for mpCursor
//   +0x04 muByteCount      number of bytes emitted so far (checked against muByteLimit)
//   +0x08 mpCursor         current write cursor into the buffer
//   +0x0C muBitBuffer      32-bit accumulator; the top byte(s) are flushed out MSB-first
//   +0x10 muBitsFree       bits still free in muBitBuffer (starts at 32)
//   +0x14 muBufferSize     buffer length in bytes (a3)
//   +0x2C muEscapeEnabled  when non-zero, output bytes pass through MassageData (start-code
//                          emulation prevention: 00 00 0X -> 00 00 03 0X); a4
//   +0x30 muMassageState   MassageData run-length state (count of consecutive 0x00 bytes)
//   +0x34 muByteLimit      max byte count before the stream self-resets (0x7FFFFFFF)
//   +0x38 muOverflowed     set to 1 when muByteCount exceeds muByteLimit
//
// MassageData / putBits emit up to four accumulator bytes at a time; when escaping is on,
// each byte is routed through MassageData, which may insert an extra 0x03 escape byte.
// =====================================================================================
#pragma once

#include "types.hpp"

class COutBitStream
{
public:
    // a2=buffer start, a3=buffer size in bytes, a4=escape-enable flag.
    COutBitStream(u8* lpBuffer, u32 luSize, u32 luEscapeEnabled);

    // Re-point the stream at a fresh buffer without touching muBitBuffer/muBitsFree
    // (used between coded headers). Clears the escape state and overflow flag.
    COutBitStream* attach(u8* lpBuffer, u32 luSize, u32 luEscapeEnabled);

    // Emit one output byte, routing it through the start-code-emulation-prevention state
    // machine. Writes the (possibly substituted) byte through lpDst, and when a 00 00 0X
    // sequence is detected substitutes 00 00 03 and emits the original byte through
    // lpExtra. Returns the number of bytes written: 1 normally, 2 when an escape byte
    // was inserted.
    u32 MassageData(u8 lu8Byte, u8* lpDst, u8* lpExtra);

    // Flush any buffered bits, byte-padding the final partial byte, and reset the
    // accumulator. Emits a trailing stop bit first when escaping is enabled.
    COutBitStream* flush();

    // --- siblings, declared for completeness (bodies live in the batch that owns them) ---
    COutBitStream* reset();
    COutBitStream* putBits(u32 luValue, u32 luNumBits);
    COutBitStream* flushByteAlign();

private:
    u8*  mpBufferStart;    // +0x00
    u32  muByteCount;      // +0x04
    u8*  mpCursor;         // +0x08
    u32  muBitBuffer;      // +0x0C
    u32  muBitsFree;       // +0x10
    u32  muBufferSize;     // +0x14
    u32  muPad18;          // +0x18  (unused by this batch)
    u32  muPad1C;          // +0x1C
    u32  muPad20;          // +0x20
    u32  muPad24;          // +0x24
    u32  muPad28;          // +0x28
    u32  muEscapeEnabled;  // +0x2C
    u32  muMassageState;   // +0x30
    u32  muByteLimit;      // +0x34
    u32  muOverflowed;     // +0x38
};
