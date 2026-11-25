#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsSmartBitStream.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "types.hpp"   // uintptr_t

#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\BitStream\\CgsSmartBitStream.cpp"

namespace CgsNetwork
{
    bool SmartBitStream::AddRawData(const char* lpcData, s32 liNumBytes)
    {
        CGS_ASSERT(lpcData != 0, CGS_ASSERT_MESSAGE_AT(
            "lpcData != NULL",
            CGS_ARTIST_FILE, 47 /* ARTIST 0x82882F14 */));
        CGS_ASSERT(liNumBytes >= 0, CGS_ASSERT_MESSAGE_AT(
            "liNumBytes >= 0",
            CGS_ARTIST_FILE, 48 /* ARTIST 0x82882F38 */));

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
            if (!AddUInt(*lpuData))
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

        CGS_ASSERT_AT(liNumBytes == 0, "liNumBytes == 0", CGS_ARTIST_FILE, 85);
        return true;
    }

    void SmartBitStream::GetRawData(char* lpcBuffer, s32 liNumBytes)
    {
        CGS_ASSERT(lpcBuffer != 0, CGS_ASSERT_MESSAGE_AT(
            "lpcBuffer != NULL",
            CGS_ARTIST_FILE, 100 /* ARTIST 0x82883064 */));
        CGS_ASSERT(liNumBytes >= 0, CGS_ASSERT_MESSAGE_AT(
            "liNumBytes >= 0",
            CGS_ARTIST_FILE, 101 /* ARTIST 0x82883088 */));

        char* lpcDst = lpcBuffer;

        // Head: read leading bytes until the dest pointer is 8-byte aligned.
        while (liNumBytes > 0 && (reinterpret_cast<uintptr_t>(lpcDst) & 7) != 0)
        {
            *lpcDst = static_cast<char>(GetByte());
            ++lpcDst;
            --liNumBytes;
        }

        u64* lpuDst = reinterpret_cast<u64*>(lpcDst);
        const uintptr_t luTailOffset = static_cast<u32>(liNumBytes) & ~7u;
        lpcDst = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(lpcDst) + luTailOffset);
        if (liNumBytes >= 8)
        {
            u32 luWordCount = static_cast<u32>(liNumBytes) >> 3;
            liNumBytes -= static_cast<s32>(luWordCount * 8);
            do
            {
                *lpuDst = GetUInt();
                ++lpuDst;
            }
            while (--luWordCount);
        }

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
            SaturateToS32(static_cast<double>((lfMax - lfMin) / (lfResolution * 2.0f)) + 0.5);
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
        const u32 luPackedValue = static_cast<u32>(GetBits(IntQuantiser::GetNumBits(liMin, liMax)));
        IntQuantiser::UnPack(lpiValue, liMin, liMax, luPackedValue);
    }

    // ARTIST 0x828800F0. Return the quantised value directly for a Message field.
    s32 SmartBitStream::GetQuantisedInt(s32 liMin, s32 liMax)
    {
        const u32 luPackedValue = static_cast<u32>(GetBits(IntQuantiser::GetNumBits(liMin, liMax)));
        s32 liValue;
        IntQuantiser::UnPack(&liValue, liMin, liMax, luPackedValue);
        return liValue;
    }
}
