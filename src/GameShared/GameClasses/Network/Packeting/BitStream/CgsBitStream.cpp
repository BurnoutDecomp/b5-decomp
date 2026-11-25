#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsBitStream.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "types.hpp"   // uintptr_t

#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\BitStream\\CgsBitStream.cpp"

namespace CgsNetwork
{
    bool BitStream::Prepare(u8* lpu8Buffer, s32 liBufferReadOffsetInBits,
                            s32 liBufferWriteOffsetInBits, s32 liBufferLengthInBits)
    {
        const u32 luExcessBytes = static_cast<u32>(reinterpret_cast<uintptr_t>(lpu8Buffer) & 7);
        const u32 luExcessBits = luExcessBytes * 8;
        mpuBuffer = reinterpret_cast<u64*>(lpu8Buffer - luExcessBytes);
        miBitWritePosition = liBufferWriteOffsetInBits + luExcessBits;
        miBitReadPosition = liBufferReadOffsetInBits + luExcessBits;
        miBufferSizeInBits = liBufferLengthInBits + luExcessBits;
        return true;
    }

    // Detach the stream from its buffer. The shipping build has no standalone copy: it is
    // inlined into Message::Pack / Message::UnPack as four zero stores over the whole
    // stream (+0x0 write position, +0x4 read position, +0x8 buffer, +0xC size). The
    // out-of-line build of the same source returns true.
    bool BitStream::Release()
    {
        miBufferSizeInBits = 0;
        miBitWritePosition = 0;
        miBitReadPosition  = 0;
        mpuBuffer          = 0;
        return true;
    }

    bool BitStream::AddBits(u64 luValue, s32 liNumBits)
    {
        CGS_ASSERT_AT(liNumBits > 0 && liNumBits <= KI_MAX_BITS,
                      "liNumBits > 0 && liNumBits <= KI_MAX_BITS", CGS_ARTIST_FILE, 107);

        // A full-width field shifts the all-ones mask by zero.
        const u64 luMask = ~0ull >> (KI_MAX_BITS - liNumBits);
        const u64 luMaskedValue = luValue & luMask;
        const s32 liEndPosition = miBitWritePosition + liNumBits;
        if (liEndPosition > miBufferSizeInBits)
        {
            return false;
        }

        const s32 liWordNum = miBitWritePosition >> 6;
        const s32 liBitOffset = miBitWritePosition & 63;
        miBitWritePosition = liEndPosition;

        const s32 liBitsInFirstWord = 64 - liBitOffset;
        const s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;
        if (liBitsInSecondWord > 0)
        {
            mpuBuffer[liWordNum] &= ~((1ull << liBitsInFirstWord) - 1);
            mpuBuffer[liWordNum] |= luMaskedValue >> liBitsInSecondWord;
            mpuBuffer[liWordNum + 1] = luMaskedValue << (64 - liBitsInSecondWord);
        }
        else
        {
            const s32 liShift = liBitsInFirstWord - liNumBits;
            mpuBuffer[liWordNum] &= ~(luMask << liShift);
            mpuBuffer[liWordNum] |= luMaskedValue << liShift;
        }
        return true;
    }

    u64 BitStream::GetBits(s32 liNumBits)
    {
        CGS_ASSERT_AT(liNumBits > 0 && liNumBits <= KI_MAX_BITS,
                      "liNumBits > 0 && liNumBits <= KI_MAX_BITS", CGS_ARTIST_FILE, 160);
        CGS_ASSERT_AT(miBitReadPosition + liNumBits <= miBufferSizeInBits,
                      "miBitReadPosition + liNumBits <= miBufferSizeInBits", CGS_ARTIST_FILE, 163);
        CGS_ASSERT_AT(miBitReadPosition + liNumBits <= miBitWritePosition,
                      "miBitReadPosition + liNumBits <= miBitWritePosition", CGS_ARTIST_FILE, 164);

        const s32 liReadPosition = miBitReadPosition;
        const s32 liBitOffset = liReadPosition & 63;
        const s32 liWordNum = liReadPosition >> 6;
        const s32 liEndPosition = liReadPosition + liNumBits;
        const s32 liBitsInFirstWord = 64 - liBitOffset;
        const s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;
        miBitReadPosition = liEndPosition;

        if (liBitsInSecondWord > 0)
        {
            const u64 luFirstValue = mpuBuffer[liWordNum] & ((1ull << liBitsInFirstWord) - 1);
            const u64 luSecondValue = mpuBuffer[liWordNum + 1] >> (64 - liBitsInSecondWord);
            return (luFirstValue << liBitsInSecondWord) + luSecondValue;
        }
        const u64 luMask = ~0ull >> (KI_MAX_BITS - liNumBits);
        return (mpuBuffer[liWordNum] >> (liBitsInFirstWord - liNumBits)) & luMask;
    }
}
