#pragma once

#include "types.hpp"

// CgsNetwork::BitStream
//
// MSB-first bit-packing utility over an 8-byte (u64) word buffer. Values are
// packed most-significant-bit first; fields may straddle two adjacent 64-bit
// words. AddBits/GetBits are exact bitwise inverses: GetBits reads back exactly
// what AddBits wrote (including the two-word-spanning and 64-bit-aligned cases).
//
// Prepare, AddBits and GetBits have standalone entries. Message::Pack and
// Message::UnPack inline cursor setup and release.

namespace CgsNetwork
{
    struct BitStream
    {
        // --- members (DWARF order) ---
        s32  miBitWritePosition;   // *a1   — next write bit position
        s32  miBitReadPosition;    // a1[1] — next read bit position

    private:
        friend struct Message;
        static const s32 KI_MAX_BITS = 64;

        // ARTIST uses the same cursor alignment in the standalone and message forms.
        void PrepareCursors(u8* lpu8Buffer, s32 liBufferReadOffsetInBits,
                            s32 liBufferWriteOffsetInBits, s32 liBufferLengthInBits)
        {
            const u32 luExcessBytes = static_cast<u32>(reinterpret_cast<uintptr_t>(lpu8Buffer) & 7);
            const u32 luExcessBits = luExcessBytes * 8;
            mpuBuffer = reinterpret_cast<u64*>(lpu8Buffer - luExcessBytes);
            miBitWritePosition = liBufferWriteOffsetInBits + luExcessBits;
            miBitReadPosition = liBufferReadOffsetInBits + luExcessBits;
            miBufferSizeInBits = liBufferLengthInBits + luExcessBits;
        }

        u64* mpuBuffer;            // a1[2] — 8-byte-aligned word buffer
        s32  miBufferSizeInBits;   // a1[3] — usable length in bits

    public:
        bool Prepare(u8* lpu8Buffer, s32 liBufferReadOffsetInBits,
                     s32 liBufferWriteOffsetInBits, s32 liBufferLengthInBits);
        bool Release()
        {
            miBitWritePosition = 0;
            miBitReadPosition = 0;
            mpuBuffer = 0;
            miBufferSizeInBits = 0;
            return true;
        }
        bool AddBits(u64 luValue, s32 liNumBits);
        u64  GetBits(s32 liNumBits);
        s32  GetBitLength() const;
        s32  GetBitLengthRemaining() const;
        void DebugPrint();
    };
}
