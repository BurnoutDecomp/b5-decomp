#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint>   // uintptr_t

// Reconstructed from BURNOUT_X360_*.XEX
//   CgsNetwork::SmartBitStream::AddRawData @ 0x82882ED8 — copy a raw byte block
//                                                         into the bitstream:
//                                                         emit a byte head until
//                                                         the data pointer is
//                                                         8-byte aligned, then a
//                                                         u64 bulk, then a byte
//                                                         tail. Returns false on
//                                                         the first AddBits
//                                                         overflow.
//   CgsNetwork::SmartBitStream::GetRawData @ 0x82883028 — inverse: read the
//                                                         block back with the
//                                                         same head/bulk/tail
//                                                         split keyed on the
//                                                         destination pointer's
//                                                         alignment.
//
// GetRawData was reconstructed from the decoded algorithm rather than the X360
// pseudocode: Hex-Rays emitted it with "local variable allocation has failed,
// the output may be wrong!" (register-mangled). It is the inverse of AddRawData.
// The head loop aligns on the DATA pointer's absolute address, so the sender's and the
// receiver's head/bulk/tail splits differ whenever their buffers differ in alignment;
// the bulk words are therefore loaded and stored big-endian, as the console's are, so
// every data byte lands in the stream in memory order and the split does not matter.

namespace CgsNetwork
{
namespace
{
    // Big-endian 64-bit access to a raw data block (see CgsBitStream.cpp). Host-port
    // correction, not console behaviour: the console's word loads and stores are
    // big-endian natively.
    inline u64 LoadWord(const u64* lpuWord)
    {
        const u8* lpu8 = reinterpret_cast<const u8*>(lpuWord);
        return (static_cast<u64>(lpu8[0]) << 56) | (static_cast<u64>(lpu8[1]) << 48)
             | (static_cast<u64>(lpu8[2]) << 40) | (static_cast<u64>(lpu8[3]) << 32)
             | (static_cast<u64>(lpu8[4]) << 24) | (static_cast<u64>(lpu8[5]) << 16)
             | (static_cast<u64>(lpu8[6]) << 8)  |  static_cast<u64>(lpu8[7]);
    }

    inline void StoreWord(u64* lpuWord, u64 luValue)
    {
        u8* lpu8 = reinterpret_cast<u8*>(lpuWord);
        lpu8[0] = static_cast<u8>(luValue >> 56);
        lpu8[1] = static_cast<u8>(luValue >> 48);
        lpu8[2] = static_cast<u8>(luValue >> 40);
        lpu8[3] = static_cast<u8>(luValue >> 32);
        lpu8[4] = static_cast<u8>(luValue >> 24);
        lpu8[5] = static_cast<u8>(luValue >> 16);
        lpu8[6] = static_cast<u8>(luValue >> 8);
        lpu8[7] = static_cast<u8>(luValue);
    }
}

    bool SmartBitStream::AddRawData(const char* lpcData, s32 liNumBytes)
    {
        CGS_ASSERT(lpcData != nullptr, "lpcData != NULL");
        CGS_ASSERT(liNumBytes >= 0, "liNumBytes >= 0");

        const char* lpcRemaining = lpcData;

        // Head: emit leading bytes until the data pointer is 8-byte aligned (or
        // the data is exhausted).
        while (liNumBytes > 0 && (reinterpret_cast<uintptr_t>(lpcRemaining) & 7) != 0)
        {
            if (!AddByte(static_cast<u8>(*lpcRemaining)))
            {
                return false;
            }
            --liNumBytes;
            ++lpcRemaining;
        }

        // Bulk: 64 bits at a time from the now-aligned pointer.
        const u64* lpuData = reinterpret_cast<const u64*>(lpcRemaining);
        while (liNumBytes >= 8)
        {
            if (!AddUInt(LoadWord(lpuData)))
            {
                return false;
            }
            ++lpuData;
            liNumBytes -= 8;
        }
        lpcRemaining = reinterpret_cast<const char*>(lpuData);

        // Tail: remaining bytes.
        while (liNumBytes > 0)
        {
            if (!AddByte(static_cast<u8>(*lpcRemaining)))
            {
                return false;
            }
            --liNumBytes;
            ++lpcRemaining;
        }

        CGS_ASSERT(liNumBytes == 0, "liNumBytes == 0");
        return true;
    }

    void SmartBitStream::GetRawData(char* lpcBuffer, s32 liNumBytes)
    {
        CGS_ASSERT(lpcBuffer != nullptr, "lpcBuffer != NULL");
        CGS_ASSERT(liNumBytes >= 0, "liNumBytes >= 0");

        char* lpcDst = lpcBuffer;

        // Head: read leading bytes until the dest pointer is 8-byte aligned.
        while (liNumBytes > 0 && (reinterpret_cast<uintptr_t>(lpcDst) & 7) != 0)
        {
            *lpcDst = static_cast<char>(GetByte());
            ++lpcDst;
            --liNumBytes;
        }

        // Bulk: 64 bits at a time into the now-aligned pointer.
        u64* lpuDst = reinterpret_cast<u64*>(lpcDst);
        while (liNumBytes >= 8)
        {
            StoreWord(lpuDst, GetUInt());
            ++lpuDst;
            liNumBytes -= 8;
        }
        lpcDst = reinterpret_cast<char*>(lpuDst);

        // Tail: remaining bytes.
        while (liNumBytes > 0)
        {
            *lpcDst = static_cast<char>(GetByte());
            ++lpcDst;
            --liNumBytes;
        }
    }

    // ---- GetQuantisedFloat @ 0x8264AFA0 (liNumBits overload) -------------------
    // Read liNumBits from the stream, then hand the packed bits plus the range to
    // FloatQuantiser::UnPack, which reconstructs the float into *lpfValue.
    void SmartBitStream::GetQuantisedFloat(float* lpfValue, float lfMin, float lfMax,
                                           s32 liNumBits)
    {
        u32 luPackedValue = static_cast<u32>(GetBits(liNumBits));
        FloatQuantiser::UnPack(lpfValue, lfMin, lfMax, liNumBits, luPackedValue);
    }

    // ---- GetQuantisedFloat (resolution overload) --------------------------------
    // The bit count is inlined: the smallest n with (1 << n) > the range's step count
    // ((max - min) / (2 * resolution), rounded half up in double precision). Read that
    // many bits and reconstruct through the resolution form of FloatQuantiser::UnPack.
    void SmartBitStream::GetQuantisedFloat(float* lpfValue, float lfMin, float lfMax,
                                           float lfResolution)
    {
        const s32 liMaxSteps =
            static_cast<s32>(static_cast<double>((lfMax - lfMin) / (lfResolution * 2.0f)) + 0.5);
        const u32 luNumValues = static_cast<u32>(liMaxSteps) + 1u;
        s32 liNumBits = 0;
        if (luNumValues != 0)
        {
            do
            {
                ++liNumBits;
            }
            while ((1u << liNumBits) < luNumValues);
        }

        const u32 luPackedValue = static_cast<u32>(GetBits(liNumBits));
        FloatQuantiser::UnPack(lpfValue, lfMin, lfMax, lfResolution, luPackedValue);
    }

    // ---- GetQuantisedInt @ 0x82880098 -----------------------------------------
    // The X360 build inlines IntQuantiser::GetNumBits here: count the right-shifts
    // it takes to drive the unsigned span (max-min) to zero. Read that many bits,
    // then reconstruct the value through IntQuantiser::UnPack.
    void SmartBitStream::GetQuantisedInt(s32* lpiValue, s32 liMin, s32 liMax)
    {
        // Inlined GetNumBits(liMin, liMax): bit_width(liMax - liMin), 0 when equal.
        u32 luSpan    = static_cast<u32>(liMax) - static_cast<u32>(liMin);
        s32 liNumBits = 0;
        if (liMax != liMin)
        {
            do
            {
                luSpan >>= 1;
                ++liNumBits;
            }
            while (luSpan);
        }

        u32 luPackedValue = static_cast<u32>(GetBits(liNumBits));
        IntQuantiser::UnPack(lpiValue, liMin, liMax, luPackedValue);
    }
}
