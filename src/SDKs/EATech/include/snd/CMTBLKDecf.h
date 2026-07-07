#ifndef SDKS_EATECH_SND_CMTBLKDECF_H
#define SDKS_EATECH_SND_CMTBLKDECF_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.h
//
// EATech "Snd" MicroTalk (MT) block decoder support. This header homes the small
// LSB-first bit reader the MT decoder walks its coded block with, plus the free
// helper that consumes bits from it.
//
//   - Snd::discardbits   @ 0x82B79600   (defined in CMTBLKDecf.cpp)
//
// The bit reader's three-field shape is attested by discardbits and by the MT
// decode path (readsamples / decodemut read the accumulator low byte at +4):
//   +0 source byte cursor, +4 bit accumulator, +8 valid-bit count. On the PC
// target the pointer widens to 8 bytes; access is by named member so the widened
// layout stays parity-correct (semantic parity, not byte offsets).
//
// FLAG (BLOCKED, un-recovered rodata): the sibling MT functions Snd::readsamples
// (0x82B79648) and Snd::decodemut (0x82B79FE8) are NOT reconstructed here -- both
// hinge on codebook / dequantiser tables that are not present in the dossier
// (readsamples: byte_82F88928 decode LUT, dword_82F88B28/dword_82F88B2C length
// tables, flt_82F88B30 dequant table; decodemut: flt_82F88828/flt_82F88868
// dequant tables, the off_82F883DC memcpy indirection, and the un-homed
// synthesis helper sub_82B79820). They are left blocked rather than fabricated.
// ============================================================================

namespace Snd
{
    // LSB-first bit reader that the MicroTalk decoder feeds its coded block into.
    // discardbits pulls bits out of muAccum and refills a byte at a time from
    // mpSource. In the MT decode state this reader occupies the first fields of
    // the larger decoder object (which is why readsamples/decodemut pass the same
    // pointer to both this and the shared getbits helper).
    struct MtBitReader
    {
        const u8* mpSource;   // +0x0  next input byte
        u32       muAccum;    // +0x4  bit accumulator (low bits consumed first)
        s32       miBitsLeft; // +0x8  number of valid bits still in muAccum
    };

    // 0x82B79600. Drop aiBits from the accumulator (shift them out of the low
    // end), decrementing the valid-bit count; if fewer than 8 bits remain, pull
    // one more byte from mpSource and splice it in above the surviving bits.
    // Returns apBits (the X360 leaves the reader pointer in r3).
    MtBitReader* discardbits(MtBitReader* apBits, s32 aiBits);
} // namespace Snd

#endif // SDKS_EATECH_SND_CMTBLKDECF_H
