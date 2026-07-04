// CBaseEndianBuffer.cpp -- Xam endian-aware read/write cursor bodies.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The +0x00 span base is spelled
// mpData and the +0x0C cursor is spelled muOffset throughout (the X360 renders them mpBase/
// muPosition on the read paths and mpData/muOffset on the write paths -- same two fields).
//
//   CBaseEndianBuffer::Bind                 @ 0x8297FE18
//   CBaseEndianBuffer::SeekTo               @ 0x8297FE38
//   CBaseEndianBuffer::GetPointerAndAdvance @ 0x8297FE60
//   CBaseEndianBuffer::WriteBytes           @ 0x8297FF90
//   CBaseEndianBuffer::WriteWords           @ 0x82980008
//   CBaseEndianBuffer::WriteDwords          @ 0x82980088
//   CBaseEndianBuffer::WriteQwords          @ 0x82980110
//   CBaseEndianBuffer::AppendBytes          @ 0x829801B8
//   CBaseEndianBuffer::GetByte              @ 0x829801F0
//   CBaseEndianBuffer::GetWord              @ 0x82980248
//   CBaseEndianBuffer::GetData              @ 0x82980278
//   CBaseEndianBuffer::GetDataPointer       @ 0x829802C8

#include "SDKs/Xam/CBaseEndianBuffer.h"

#include <cstring> // memcpy

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::Bind @ 0x8297FE18
// Point the buffer at a caller-owned span and rewind the cursor. The +0x04 field is
// stored from the 2nd argument but never loaded in scope; preserved as an opaque word.
// ---------------------------------------------------------------------------
CBaseEndianBuffer* CBaseEndianBuffer::Bind(u8* lpData, u32 luContext, u32 luSize, u32 lbSwapEndian)
{
    mpData       = lpData;
    mReserved4   = luContext;
    muSize       = luSize;
    mbSwapEndian = lbSwapEndian;
    muOffset     = 0;
    return this;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::SeekTo @ 0x8297FE38
// Seeking exactly to muSize is legal (ble); one past is the error boundary.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::SeekTo(u32 luOffset)
{
    if (luOffset > muSize)
    {
        return KI_E_READ_PAST_END;
    }

    muOffset = luOffset;
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::GetPointerAndAdvance @ 0x8297FE60
// Hand back a raw pointer into the bound span at the cursor and advance the cursor by
// *lpSize. On underflow it writes the number of bytes actually left back through lpSize
// and returns E_ENDOFSTREAM without moving the cursor. Compare is unsigned.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::GetPointerAndAdvance(void** ppOut, u32* lpSize)
{
    u32 luAvail = muSize - muOffset;
    if (luAvail < *lpSize)
    {
        *lpSize = luAvail;
        return KI_E_ENDOFSTREAM;
    }

    *ppOut = mpData + muOffset;
    muOffset += *lpSize;
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::WriteBytes @ 0x8297FF90
// Bytes are copied verbatim -- no endian swap for a raw byte span.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::WriteBytes(const void* lpSrc, u32 luLength)
{
    if (muSize - muOffset < luLength)
    {
        return KI_E_INSUFFICIENT_BUFFER;
    }

    memcpy(mpData + muOffset, lpSrc, luLength);
    muOffset += luLength;
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::WriteWords @ 0x82980008
// 16-bit byte swap per element when the buffer is in swap mode.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::WriteWords(const u16* lpSrc, s32 luCount)
{
    if (muSize - muOffset < (u32)(2 * luCount))
    {
        return KI_E_INSUFFICIENT_BUFFER;
    }

    u16* lpDst = reinterpret_cast<u16*>(mpData + muOffset);
    while (luCount)
    {
        u16 luValue;
        if (mbSwapEndian)
        {
            u16 x = *lpSrc;
            luValue = (u16)((x << 8) | (x >> 8));
        }
        else
        {
            luValue = *lpSrc;
        }
        *lpDst = luValue;
        ++lpSrc;
        --luCount;
        ++lpDst;
        muOffset += 2;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::WriteDwords @ 0x82980088
// Full 32-bit byte reversal of each dword when the buffer is in swap mode.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::WriteDwords(const u32* lpSrc, s32 luCount)
{
    if (muSize - muOffset < (u32)(4 * luCount))
    {
        return KI_E_INSUFFICIENT_BUFFER;
    }

    u32* lpDst = reinterpret_cast<u32*>(mpData + muOffset);
    while (luCount)
    {
        u32 luValue;
        if (mbSwapEndian)
        {
            u32 x = *lpSrc;
            luValue = (x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24);
        }
        else
        {
            luValue = *lpSrc;
        }
        *lpDst = luValue;
        ++lpSrc;
        --luCount;
        ++lpDst;
        muOffset += 4;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::WriteQwords @ 0x82980110
// FULL 8-byte reversal: bswap32 of the LOW word is placed in the HIGH half and vice
// versa (the halves ARE exchanged) -- equivalent to bswap64.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::WriteQwords(const u64* lpSrc, s32 luCount)
{
    if (muSize - muOffset < (u32)(8 * luCount))
    {
        return KI_E_INSUFFICIENT_BUFFER;
    }

    u64* lpDst = reinterpret_cast<u64*>(mpData + muOffset);
    while (luCount)
    {
        u64 luValue = *lpSrc++;
        if (mbSwapEndian)
        {
            u32 hi = (u32)(luValue >> 32);
            u32 lo = (u32)luValue;
            u32 sh = (hi << 24) | ((hi << 8) & 0x00FF0000) | ((hi >> 8) & 0x0000FF00) | (hi >> 24);
            u32 sl = (lo << 24) | ((lo << 8) & 0x00FF0000) | ((lo >> 8) & 0x0000FF00) | (lo >> 24);
            // Halves ARE exchanged: swapped LOW word occupies the HIGH 32 bits.
            luValue = ((u64)sl << 32) | (u64)sh;
        }
        *lpDst = luValue;
        --luCount;
        ++lpDst;
        muOffset += 8;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::AppendBytes @ 0x829801B8
// Reserves luCount bytes at the cursor without touching the data (WriteBytes does the
// copy); the compare is unsigned.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::AppendBytes(u32 luCount)
{
    if (muSize - muOffset < luCount)
    {
        return KI_E_BUFFER_OVERFLOW;
    }

    muOffset += luCount;
    return 0;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::GetByte @ 0x829801F0
// Fetch a single byte from the cursor into *lpDst. Delegates the bounds check + cursor
// advance to GetPointerAndAdvance with a fixed size of 1, then copies. The size is
// re-read after the call so a GetPointerAndAdvance clamp is honored.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::GetByte(void* lpDst)
{
    u32   luSize = 1;
    void* lpSrc;

    s32 lhr = GetPointerAndAdvance(&lpSrc, &luSize);
    if (lhr >= 0)
    {
        memcpy(lpDst, lpSrc, luSize);
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::GetWord @ 0x82980248
// Thin wrapper: pins the element size to 2 and forwards the object's own endian-swap
// flag as the swap argument.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::GetWord(void* lpOut)
{
    u32 luSize = 2;
    return GetDataAndAdvance(lpOut, &luSize, mbSwapEndian);
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::GetData @ 0x82980278
// Copy *lpSize bytes from the cursor into lpDst. The size comes from the caller's lpSize
// word: handed to GetPointerAndAdvance (which may clamp on underflow) then re-read.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::GetData(void* lpDst, u32* lpSize)
{
    void* lpSrc;

    s32 lhr = GetPointerAndAdvance(&lpSrc, lpSize);
    if (lhr >= 0)
    {
        memcpy(lpDst, lpSrc, *lpSize);
    }
    return lhr;
}

// ---------------------------------------------------------------------------
// CBaseEndianBuffer::GetDataPointer @ 0x829802C8
// A pure forwarding alias for GetPointerAndAdvance.
// ---------------------------------------------------------------------------
s32 CBaseEndianBuffer::GetDataPointer(void** ppOut, u32* lpSize)
{
    return GetPointerAndAdvance(ppOut, lpSize);
}
