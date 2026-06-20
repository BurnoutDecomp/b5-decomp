#pragma once

#include "types.hpp"

// CgsNetwork::BitStream
//
// MSB-first bit-packing utility over an 8-byte (u64) word buffer. Values are
// packed most-significant-bit first; fields may straddle two adjacent 64-bit
// words. AddBits/GetBits are exact bitwise inverses: GetBits reads back exactly
// what AddBits wrote (including the two-word-spanning and 64-bit-aligned cases).
//
// This is the canonical class home. The .cpp in this directory defines only the
// three reconstructed functions; the remaining four methods (Release,
// GetBitLength, GetBitLengthRemaining, DebugPrint) are reconstructed in their
// own translation units.

namespace CgsNetwork
{
    struct BitStream
    {
        // --- members (DWARF order) ---
        s32  miBitWritePosition;   // *a1   — next write bit position
        s32  miBitReadPosition;    // a1[1] — next read bit position

    private:
        static const s32 KI_MAX_BITS = 64;

        u64* mpuBuffer;            // a1[2] — 8-byte-aligned word buffer
        s32  miBufferSizeInBits;   // a1[3] — usable length in bits

    public:
        bool Prepare(u8* lpu8Buffer, s32 liBufferReadOffsetInBits,
                     s32 liBufferWriteOffsetInBits, s32 liBufferLengthInBits);
        bool Release();
        bool AddBits(u64 luValue, s32 liNumBits);
        u64  GetBits(s32 liNumBits);
        s32  GetBitLength() const;
        s32  GetBitLengthRemaining() const;
        void DebugPrint();
    };
}
