#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsBitStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint>   // uintptr_t

// Reconstructed from BURNOUT_X360_*.XEX
//   CgsNetwork::BitStream::Prepare  @ 0x828712A8  — align byte buffer down to a
//                                                   u64 boundary, fold the byte
//                                                   misalignment into the bit
//                                                   read/write/size positions.
//   CgsNetwork::BitStream::AddBits  @ 0x828712E0  — pack liNumBits of luValue
//                                                   MSB-first at the write
//                                                   cursor (may span two words);
//                                                   returns false if it would
//                                                   overflow the buffer.
//   CgsNetwork::BitStream::GetBits  @ 0x82871420  — read liNumBits MSB-first at
//                                                   the read cursor (may span two
//                                                   words); exact inverse of
//                                                   AddBits.
//
// AddBits and GetBits were reconstructed from the decoded algorithm rather than
// the X360 pseudocode: Hex-Rays emitted both with "local variable allocation has
// failed, the output may be wrong!" (register-mangled). They are verified bitwise
// inverses — GetBits returns exactly (luValue & mask) for the field AddBits wrote
// from a matching cursor position. Prepare's pseudocode was clean and is matched
// directly.

namespace CgsNetwork
{
    bool BitStream::Prepare(u8* lpu8Buffer, s32 liBufferReadOffsetInBits,
                            s32 liBufferWriteOffsetInBits, s32 liBufferLengthInBits)
    {
        uintptr_t lBuf       = reinterpret_cast<uintptr_t>(lpu8Buffer);
        s32       liExcessBits = 8 * static_cast<s32>(lBuf & 7);   // 8 bits per misaligned byte

        mpuBuffer          = reinterpret_cast<u64*>(lBuf - (lBuf & 7));   // align down to 8
        miBitReadPosition  = liExcessBits + liBufferReadOffsetInBits;
        miBitWritePosition = liExcessBits + liBufferWriteOffsetInBits;
        miBufferSizeInBits = liExcessBits + liBufferLengthInBits;
        return true;
    }

    bool BitStream::AddBits(u64 luValue, s32 liNumBits)
    {
        CGS_ASSERT(liNumBits > 0 && liNumBits <= KI_MAX_BITS,
                   "liNumBits > 0 && liNumBits <= KI_MAX_BITS");

        // Safe mask (avoid 1<<64 UB when liNumBits == 64).
        u64 luMask        = (liNumBits >= 64) ? ~0ull : ((1ull << liNumBits) - 1ull);
        u64 luMaskedValue = luValue & luMask;

        // Fit check — AddBits returns false rather than asserting on overflow.
        if (miBitWritePosition + liNumBits > miBufferSizeInBits)
        {
            return false;
        }

        s32 liWritePos = miBitWritePosition;
        miBitWritePosition += liNumBits;

        s32 liBitOffset       = liWritePos & 0x3F;
        s32 liWordNum         = liWritePos >> 6;
        s32 liBitsInFirstWord = 64 - liBitOffset;

        if (liNumBits <= liBitsInFirstWord)
        {
            // Field fits entirely in one word; place it MSB-first.
            s32 liShift = liBitsInFirstWord - liNumBits;
            mpuBuffer[liWordNum] =
                (mpuBuffer[liWordNum] & ~(luMask << liShift)) | (luMaskedValue << liShift);
        }
        else
        {
            // Field straddles two words. liBitsInFirstWord is in [1,63] here, so
            // every shift below is well defined.
            s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;

            // Low liBitsInFirstWord bits of the current word take the TOP bits of
            // the value.
            u64 luFirstMask = (1ull << liBitsInFirstWord) - 1ull;
            mpuBuffer[liWordNum] =
                (mpuBuffer[liWordNum] & ~luFirstMask) | (luMaskedValue >> liBitsInSecondWord);

            // Top liBitsInSecondWord bits of the next word take the BOTTOM bits of
            // the value. liBitsInSecondWord is in [1,63] here.
            s32 liShift2     = 64 - liBitsInSecondWord;
            u64 luSecondBits = luMaskedValue & ((1ull << liBitsInSecondWord) - 1ull);
            u64 luSecondMask = ((1ull << liBitsInSecondWord) - 1ull) << liShift2;
            mpuBuffer[liWordNum + 1] =
                (mpuBuffer[liWordNum + 1] & ~luSecondMask) | (luSecondBits << liShift2);
        }

        return true;
    }

    u64 BitStream::GetBits(s32 liNumBits)
    {
        CGS_ASSERT(liNumBits > 0 && liNumBits <= KI_MAX_BITS,
                   "liNumBits > 0 && liNumBits <= KI_MAX_BITS");
        CGS_ASSERT(miBitReadPosition + liNumBits <= miBufferSizeInBits,
                   "miBitReadPosition + liNumBits <= miBufferSizeInBits");
        CGS_ASSERT(miBitReadPosition + liNumBits <= miBitWritePosition,
                   "miBitReadPosition + liNumBits <= miBitWritePosition");

        s32 liReadPos = miBitReadPosition;
        miBitReadPosition += liNumBits;

        s32 liBitOffset       = liReadPos & 0x3F;
        s32 liWordNum         = liReadPos >> 6;
        s32 liBitsInFirstWord = 64 - liBitOffset;

        // Safe mask (avoid 1<<64 UB when liNumBits == 64).
        u64 luMask = (liNumBits >= 64) ? ~0ull : ((1ull << liNumBits) - 1ull);

        if (liNumBits <= liBitsInFirstWord)
        {
            // Field fits entirely in one word.
            s32 liShift = liBitsInFirstWord - liNumBits;
            return (mpuBuffer[liWordNum] >> liShift) & luMask;
        }

        // Field straddles two words. liBitsInFirstWord / liBitsInSecondWord are in
        // [1,63] here, so every shift below is well defined.
        s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;

        u64 luFirstMask  = (1ull << liBitsInFirstWord) - 1ull;
        u64 luFirstValue = (mpuBuffer[liWordNum] & luFirstMask) << liBitsInSecondWord;
        u64 luSecondValue = mpuBuffer[liWordNum + 1] >> (64 - liBitsInSecondWord);
        return luFirstValue | luSecondValue;
    }
}
