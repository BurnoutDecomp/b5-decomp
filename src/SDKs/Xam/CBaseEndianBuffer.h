#pragma once

// CBaseEndianBuffer.h -- Xam endian-aware read/write cursor over a caller-owned span.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. A CBaseEndianBuffer is bound to a raw byte
// span (Bind) and carries a cursor (muOffset). Read accessors hand back pointers into the
// span and advance the cursor (GetPointerAndAdvance and its Get* wrappers); write accessors
// copy typed data in, byte-swapping words/dwords/qwords when the buffer is in swap mode
// (mbSwapEndian). Bounds failures return HRESULT-style negative error codes rather than
// trapping.
//
// Layout (X360-attested field offsets; the leading pointer widens on the 64-bit host):
//   mpData       @ 0x00  u8*   (the bound span base)
//   mReserved4   @ 0x04  u32   (stored from Bind's 2nd arg; never read in scope)
//   muSize       @ 0x08  u32   (span length in bytes)
//   muOffset     @ 0x0C  u32   (the read/write cursor)
//   mbSwapEndian @ 0x10  u32   (non-zero => byte-swap on typed read/write)

#include "types.hpp"

// AttribSys/Xam error codes the buffer bodies return (X360 rodata constants):
//   0x8007007A == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) -- overflow / no room.
//   0x80182001 -- Xam end-of-stream (read underflow).
static const s32 KI_E_BUFFER_OVERFLOW     = static_cast<s32>(0x8007007A);
static const s32 KI_E_INSUFFICIENT_BUFFER = static_cast<s32>(0x8007007A);
static const s32 KI_E_ENDOFSTREAM         = static_cast<s32>(0x80182001);
static const s32 KI_E_READ_PAST_END       = static_cast<s32>(0x80182001);

class CBaseEndianBuffer
{
public:
    // Point the buffer at a caller-owned span and rewind the cursor.       @ 0x8297FE18
    CBaseEndianBuffer* Bind(u8* lpData, u32 luContext, u32 luSize, u32 lbSwapEndian);

    // Cursor movement / reservation.
    s32 SeekTo(u32 luOffset);                                               // 0x8297FE38
    s32 AppendBytes(u32 luCount);                                          // 0x829801B8

    // Raw-pointer read accessors: hand back a pointer into the span at the cursor
    // and advance it.
    s32 GetPointerAndAdvance(void** ppOut, u32* lpSize);                    // 0x8297FE60
    s32 GetDataPointer(void** ppOut, u32* lpSize);                         // 0x829802C8

    // Copy-out read accessors.
    s32 GetByte(void* lpDst);                                              // 0x829801F0
    s32 GetData(void* lpDst, u32* lpSize);                                 // 0x82980278
    s32 GetWord(void* lpOut);                                             // 0x82980248

    // Copy-in write accessors (byte-swapping per mbSwapEndian).
    s32 WriteBytes(const void* lpSrc, u32 luLength);                       // 0x8297FF90
    s32 WriteWords(const u16* lpSrc, s32 luCount);                        // 0x82980008
    s32 WriteDwords(const u32* lpSrc, s32 luCount);                       // 0x82980088
    s32 WriteQwords(const u64* lpSrc, s32 luCount);                       // 0x82980110

    // The size-and-swap read primitive GetWord forwards to (fixes the element size and
    // hands the object's own swap flag as the swap argument). Its body lives in its own TU;
    // declared here so GetWord links against the real member.
    s32 GetDataAndAdvance(void* lpOut, u32* lpSize, u32 lbSwapEndian);

    // Cursor / span inspectors. CMarshaller reads the bound span base (+0x00), its length
    // (+0x08) and the live cursor (+0x0C) directly while marshalling (e.g. computing the
    // current output pointer mpData+muOffset, or saving/restoring the schema read position).
    u8* GetBase()   const { return mpData; }   // +0x00 bound span base
    u32 GetSize()   const { return muSize; }   // +0x08 span length
    u32 GetOffset() const { return muOffset; } // +0x0C read/write cursor

    // Clear the bound span + cursor (mpData / mReserved4 / muSize / muOffset), preserving
    // mbSwapEndian. CMarshaller::Initialize inlines exactly this 4-word zeroing of its
    // embedded buffer and schema cursor when a schema bind fails (it leaves +0x10 alone).
    void Reset() { mpData = 0; mReserved4 = 0; muSize = 0; muOffset = 0; }

private:
    u8* mpData;       // +0x00  bound span base
    u32 mReserved4;   // +0x04  stored from Bind (opaque; never read in scope)
    u32 muSize;       // +0x08  span length
    u32 muOffset;     // +0x0C  read/write cursor (aka muPosition)
    u32 mbSwapEndian; // +0x10  non-zero => byte-swap typed reads/writes
};
