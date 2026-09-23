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
// The X360 emitted AddBits/GetBits register-mangled ("local variable allocation
// has failed"); the bodies here are now reconciled against the clean PS3 DecFIGS
// decompilations (same source, B5_FIGS): BitStream::AddBits @ 0xBD9A0C,
// BitStream::GetBits @ 0xBD9B64, BitStream::Prepare @ 0xBD9D0C. GetBits matches
// the prior reconstruction exactly; AddBits's two-word case was corrected to the
// PS3's BLIND second-word store (a forward-only writer, no read-modify-write).
// They remain verified bitwise inverses. The safe 64-bit mask guard
// (liNumBits >= 64 ? ~0) is a PC reconstruction: PPC `1<<64` wraps mod-64 to a
// 0 mask, which is UB on x64. (FLAG: PC-impossible shift behaviour.)

namespace CgsNetwork
{
namespace
{
    // The stream is a byte buffer that the console reads and writes as big-endian 64-bit
    // words, so stream bit i lives in byte i >> 3 at bit 7 - (i & 7) whatever the buffer's
    // alignment. The host keeps that byte image by loading and storing every word
    // big-endian: a packet packed here unpacks on the console and vice versa, and a stream
    // copied to a buffer of different alignment (Pack packs into aligned scratch, UnPack
    // reads straight out of the received packet) still reads back.
    // Host-port correction, not console behaviour: the console's word loads and stores are
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

    // Detach the stream from its buffer. The shipping build has no standalone copy: it is
    // inlined into Message::Pack / Message::UnPack as four zero stores over the whole
    // stream (+0x0 write position, +0x4 read position, +0x8 buffer, +0xC size). The
    // out-of-line build of the same source returns true.
    bool BitStream::Release()
    {
        miBufferSizeInBits = 0;
        miBitWritePosition = 0;
        miBitReadPosition  = 0;
        mpuBuffer          = nullptr;
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
            StoreWord(&mpuBuffer[liWordNum],
                      (LoadWord(&mpuBuffer[liWordNum]) & ~(luMask << liShift)) | (luMaskedValue << liShift));
        }
        else
        {
            // Field straddles two words. liBitsInFirstWord is in [1,63] here, so
            // every shift below is well defined.
            s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;

            // Low liBitsInFirstWord bits of the current word take the TOP bits of the
            // value (read-modify-write: the high bits of this word were already written).
            u64 luFirstMask = (1ull << liBitsInFirstWord) - 1ull;
            StoreWord(&mpuBuffer[liWordNum],
                      (LoadWord(&mpuBuffer[liWordNum]) & ~luFirstMask) | (luMaskedValue >> liBitsInSecondWord));

            // The next word is stored blind with the BOTTOM bits of the value at its top:
            // a forward-only writer, so that word has no bits of its own to keep yet.
            StoreWord(&mpuBuffer[liWordNum + 1], luMaskedValue << (64 - liBitsInSecondWord));
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
            return (LoadWord(&mpuBuffer[liWordNum]) >> liShift) & luMask;
        }

        // Field straddles two words. liBitsInFirstWord / liBitsInSecondWord are in
        // [1,63] here, so every shift below is well defined.
        s32 liBitsInSecondWord = liNumBits - liBitsInFirstWord;

        u64 luFirstMask  = (1ull << liBitsInFirstWord) - 1ull;
        u64 luFirstValue = (LoadWord(&mpuBuffer[liWordNum]) & luFirstMask) << liBitsInSecondWord;
        u64 luSecondValue = LoadWord(&mpuBuffer[liWordNum + 1]) >> (64 - liBitsInSecondWord);
        return luFirstValue | luSecondValue;
    }
}
