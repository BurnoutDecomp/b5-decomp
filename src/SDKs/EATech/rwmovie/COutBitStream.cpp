// =====================================================================================
// COutBitStream.cpp -- MSB-first bit writer for the RTCMV/WMV video encode path.
//   COutBitStream::COutBitStream    @0x82A8BD38
//   COutBitStream::MassageData      @0x82A8BC60
//   COutBitStream::attach           @0x82A8BCF8
//   COutBitStream::flush            @0x82A8BF68
//   COutBitStream::reset            @0x82A8BD18
//   COutBitStream::putBits          @0x82A8BD78
//   COutBitStream::flushByteAlign   @0x82A8C078
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

    // Low-`luBits`-set mask, i.e. the PPC rodata table dword_82F60618[]: (1<<n)-1, with the
    // n>=32 entry saturating to all-ones (matches the binary's slw-by-32 giving 0 -> ~0).
    // putBits uses this table for its value masks and the equivalent inline ~(-1<<n) form.
    inline u32 LowBitMask(u32 luBits)
    {
        return (luBits >= 32) ? 0xFFFFFFFFu : ((1u << luBits) - 1u);
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

// COutBitStream::reset @0x82A8BD18 -- rewind the accumulator and cursor without disturbing
// the buffer pointer, size, escape flag, byte limit or overflow state. Cursor returns to the
// buffer start, the accumulator is emptied (32 bits free) and the byte count is cleared.
COutBitStream* COutBitStream::reset()
{
    u8* lpStart = mpBufferStart;
    muBitBuffer = 0;              // +0x0C
    muBitsFree  = 32;             // +0x10
    mpCursor    = lpStart;        // +0x08
    muByteCount = 0;              // +0x04
    return this;
}

// COutBitStream::putBits @0x82A8BD78 -- append the low luNumBits of luValue to the stream,
// MSB-first. Bits accumulate in muBitBuffer until it fills; on a fill the four accumulator
// bytes are drained MSB-first (raw, or through MassageData when escaping) and the leftover
// low bits are reloaded into a fresh accumulator. Self-resets first if the byte count has
// run past the limit, exactly like flush().
COutBitStream* COutBitStream::putBits(u32 luValue, u32 luNumBits)
{
    COutBitStream* lpResult = this;

    if (muByteCount > muByteLimit)
    {
        u8* lpStart = mpBufferStart;
        muBitsFree   = 32;
        muBitBuffer  = 0;
        muByteCount  = 0;
        mpCursor     = lpStart;
        muOverflowed = 1;
    }

    u32 luBitsFree = muBitsFree;
    if (luBitsFree <= luNumBits)
    {
        // The new bits fill (and overflow) the accumulator. Fold in the top luBitsFree bits,
        // drain the four accumulator bytes, then reload the leftover low bits below.
        u32 luLeftover = luNumBits - luBitsFree;   // bits that won't fit the current word
        muBitBuffer ^= (luValue >> luLeftover) & LowBitMask(luBitsFree);

        if (muEscapeEnabled)
        {
            // Route each accumulator byte (MSB-first) through the start-code emulation-
            // prevention filter, advancing the cursor by the 1-or-2 bytes it writes. The byte
            // count grows by the total cursor delta across the four calls.
            u8* lpCursor0 = mpCursor;
            u8* lpCursor  = mpCursor;
            u32 luWritten;

            luWritten = MassageData((u8)(muBitBuffer >> 24), lpCursor, lpCursor + 1);
            lpCursor += luWritten;
            mpCursor = lpCursor;
            luWritten = MassageData((u8)(muBitBuffer >> 16), lpCursor, lpCursor + 1);
            lpCursor += luWritten;
            mpCursor = lpCursor;
            luWritten = MassageData((u8)(muBitBuffer >> 8), lpCursor, lpCursor + 1);
            lpCursor += luWritten;
            mpCursor = lpCursor;
            luWritten = MassageData((u8)muBitBuffer, lpCursor, lpCursor + 1);
            u8* lpEnd = lpCursor + luWritten;
            muByteCount = ((u32)(uintptr_t)lpEnd - (u32)(uintptr_t)lpCursor0) + muByteCount;
            mpCursor = lpEnd;
            // Faithful to the asm: r3 is left holding MassageData's last return on this path
            // (never reloaded to `this`); callers on the escape path ignore the result.
            lpResult = reinterpret_cast<COutBitStream*>((uintptr_t)luWritten);
        }
        else
        {
            // Escaping off: emit the four accumulator bytes raw, MSB-first.
            *mpCursor = (u8)(muBitBuffer >> 24);
            ++mpCursor;
            *mpCursor = (u8)(muBitBuffer >> 16);
            ++mpCursor;
            *mpCursor = (u8)(muBitBuffer >> 8);
            ++mpCursor;
            *mpCursor = (u8)muBitBuffer;
            ++mpCursor;
            muByteCount += 4;
        }

        if (luLeftover)
        {
            muBitsFree  = 32 - luLeftover;
            muBitBuffer = (LowBitMask(luLeftover) & luValue) << (32 - luLeftover);
        }
        else
        {
            muBitBuffer = 0;
            muBitsFree  = 32;
        }
    }
    else
    {
        // The bits fit entirely in the current accumulator word.
        u32 luOldBuffer = muBitBuffer;
        u32 luNewFree   = luBitsFree - luNumBits;
        muBitsFree  = luNewFree;
        muBitBuffer = ((LowBitMask(luNumBits) & luValue) << luNewFree) ^ luOldBuffer;
    }

    return lpResult;
}

// COutBitStream::flushByteAlign @0x82A8C078 -- pad the current partial byte out to a byte
// boundary by writing zero bits. muBitsFree & 7 gives the number of pad bits needed; when
// non-zero they are appended via putBits, otherwise the stream is already aligned.
COutBitStream* COutBitStream::flushByteAlign()
{
    u32 luPad = muBitsFree & 7;
    if (luPad)
        return putBits(0, luPad);
    return this;
}
