// =====================================================================================
// COutBitStream.cpp -- MSB-first bit writer for the RTCMV/WMV video encode path.
//   COutBitStream::COutBitStream    @0x82A8BD38
//   COutBitStream::MassageData      @0x82A8BC60
//   COutBitStream::attach           @0x82A8BCF8
//   COutBitStream::flush            @0x82A8BF68
// Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative). See COutBitStream.h
// for the byte-offset layout note.
// =====================================================================================

#include "SDKs/EATech/rwmovie/COutBitStream.h"

namespace
{
    // count-leading-zeros of a 32-bit word (PPC cntlzw), portable: cntlzw(0)==32.
    inline u32 CountLeadingZeros32(u32 luValue)
    {
        if (luValue == 0)
            return 32;
        u32 luCount = 0;
        while ((luValue & 0x80000000u) == 0)
        {
            ++luCount;
            luValue <<= 1;
        }
        return luCount;
    }
}

// COutBitStream::COutBitStream @0x82A8BD38 -- construct the bit writer over a buffer.
// mpBufferStart/mpCursor point at the buffer start; the accumulator is empty (32 bits
// free), no bytes emitted, escaping per the caller flag, and the byte limit is the
// signed-max sentinel with overflow cleared.
COutBitStream::COutBitStream(u8* lpBuffer, u32 luSize, u32 luEscapeEnabled)
{
    muBufferSize    = luSize;          // +0x14
    mpBufferStart   = lpBuffer;        // +0x00
    mpCursor        = lpBuffer;        // +0x08
    muEscapeEnabled = luEscapeEnabled; // +0x2C
    muBitsFree      = 32;              // +0x10
    muBitBuffer     = 0;              // +0x0C
    muByteCount     = 0;              // +0x04
    muMassageState  = 0;              // +0x30
    muOverflowed    = 0;              // +0x38
    muByteLimit     = 0x7FFFFFFF;      // +0x34
}

// COutBitStream::MassageData @0x82A8BC60 -- WMV start-code emulation-prevention filter.
// Writes lu8Byte through lpDst, then advances a run-length state machine over the count of
// consecutive leading 0x00 bytes. When the state reaches 2 (two prior zero bytes) and the
// incoming byte is one of {0x00,0x01,0x02,0x03}, it rewrites the just-written byte to the
// 0x03 escape byte and emits the original byte through lpExtra. Returns the number of
// bytes written: 1 normally, 2 when the escape byte was inserted.
u32 COutBitStream::MassageData(u8 lu8Byte, u8* lpDst, u8* lpExtra)
{
    *lpDst = lu8Byte;

    u32 luBytesWritten = 1;   // r3: bytes emitted (1, or 2 when an escape byte is inserted)
    u32 luState = muMassageState;

    if (luState == 0)
    {
        // No leading zeros yet: a zero byte starts the run.
        if ((lu8Byte & 0xFF) == 0)
        {
            muMassageState = 1;
        }
    }
    else if (luState == 1)
    {
        // One zero seen: another zero advances to two, anything else resets.
        muMassageState = ((CountLeadingZeros32((u32)(lu8Byte & 0xFF)) >> 4) & 2);
    }
    else if (luState < 3)   // luState == 2
    {
        u32 lu8 = lu8Byte & 0xFF;
        if (lu8 == 0 || lu8 == 1 || lu8 == 2 || lu8 == 3)
        {
            luBytesWritten = 2;
            *lpDst   = 3;
            *lpExtra = lu8Byte;
        }
        muMassageState = ((CountLeadingZeros32(lu8) >> 5) & 1);  // 1 iff byte was 0x00, else 0
    }
    // luState >= 3: fall through and return luBytesWritten (1) unchanged.

    return luBytesWritten;
}

// COutBitStream::attach @0x82A8BCF8 -- re-point the writer at a new buffer between coded
// units. Resets the buffer/cursor/size/escape fields and clears the massage state and
// overflow flag, but deliberately leaves muBitBuffer/muBitsFree/muByteCount as-is.
COutBitStream* COutBitStream::attach(u8* lpBuffer, u32 luSize, u32 luEscapeEnabled)
{
    muBufferSize    = luSize;          // +0x14
    mpBufferStart   = lpBuffer;        // +0x00
    mpCursor        = lpBuffer;        // +0x08
    muEscapeEnabled = luEscapeEnabled; // +0x2C
    muMassageState  = 0;              // +0x30
    muOverflowed    = 0;              // +0x38
    return this;
}

// COutBitStream::flush @0x82A8BF68 -- flush buffered bits and byte-align.
// If the byte count has run past the limit the stream self-resets (empty accumulator,
// cursor rewound to the buffer start) and marks itself overflowed. When escaping is on a
// trailing 1-bit stop marker is emitted via putBits. Any partial accumulator bytes are
// then drained MSB-first -- through MassageData when escaping, otherwise raw -- and the
// accumulator is finally cleared to 32 free bits.
COutBitStream* COutBitStream::flush()
{
    COutBitStream* lpResult = this;

    if (muByteCount > muByteLimit)
    {
        u8* lpStart = mpBufferStart;
        muBitsFree     = 32;
        muBitBuffer    = 0;
        muByteCount    = 0;
        mpCursor       = lpStart;
        muOverflowed   = 1;
    }

    if (muEscapeEnabled)
    {
        lpResult = putBits(1, 1);
    }

    u32 luBitsFree = muBitsFree;
    if (luBitsFree != 32)
    {
        s32 liPendingBits = 32 - (s32)luBitsFree;
        if (liPendingBits > 0)
        {
            u32 luByteCount = (u32)(((liPendingBits - 1) >> 3) + 1);
            do
            {
                if (muEscapeEnabled)
                {
                    u8* lpCursor = mpCursor;   // r5: cursor before the write
                    u32 luBytesWritten = MassageData((u8)muBitBuffer, lpCursor, lpCursor + 1);
                    u8* lpNewCursor = lpCursor + luBytesWritten;
                    // Faithful to the asm: count = (count - oldCursor) + newCursor == count + written.
                    muByteCount = (muByteCount - (u32)(uintptr_t)lpCursor) + (u32)(uintptr_t)lpNewCursor;
                    mpCursor = lpNewCursor;
                }
                else
                {
                    *mpCursor = (u8)muBitBuffer;
                    ++mpCursor;
                    ++muByteCount;
                }
                --luByteCount;
                muBitBuffer <<= 8;
            }
            while (luByteCount);
        }
    }

    muBitsFree  = 32;
    muBitBuffer = 0;
    return lpResult;
}
