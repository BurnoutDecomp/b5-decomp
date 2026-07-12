#ifndef SDKS_EATECH_SND_CMTBLKDECF_H
#define SDKS_EATECH_SND_CMTBLKDECF_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.h
//
// EATech "Snd" MicroTalk (MT) block decoder support. This header homes the small
// LSB-first bit reader the MT decoder walks its coded block with, the free helper
// that consumes bits from it, and the CMTBLKDecf decoder object plus its three
// public methods:
//
//   - Snd::discardbits          @ 0x82B79600  (defined in CMTBLKDecf.cpp)
//   - Snd::CMTBLKDecf::Feed     @ 0x82B7A3B8  (defined in CMTBLKDecf.cpp)
//   - Snd::CMTBLKDecf::Decode   @ 0x82B84F28  (defined in CMTBLKDecf.cpp)
//   - Snd::CMTBLKDecf::initmut  @ 0x82B79EC0  (defined in CMTBLKDecf.cpp)
//
// The bit reader's three-field shape is attested by discardbits and by the MT
// decode path (readsamples / decodemut read the accumulator low byte at +4):
//   +0 source byte cursor, +4 bit accumulator, +8 valid-bit count. On the PC
// target the pointer widens to 8 bytes; access is by named member so the widened
// layout stays parity-correct (semantic parity, not byte offsets).
//
// CMTBLKDecf embeds that reader at offset 0 (there is NO vtable pointer -- initmut
// stores the source cursor straight to +0, so the class is a plain, non-polymorphic
// decoder object, unlike the CShortDestDecoder PCM family). Its member layout is
// pinned entirely by the X360 displacements in Feed/Decode/initmut (see the offset
// comments on each member below); on PC the absolute offsets widen with the
// pointers, so parity is by named member, not byte offset.
//
// FLAG (BLOCKED bodies, forward-declared here): the MT bit-reader Snd::getbits
// (X360 sub_82B795A0, consumed by initmut) and Snd::decodemut (0x82B79FE8, the
// per-frame window refill called by Decode) are NOT reconstructed -- both hinge on
// codebook / dequantiser tables that are not present in the dossier (readsamples:
// byte_82F88928 decode LUT, dword_82F88B28/dword_82F88B2C length tables,
// flt_82F88B30 dequant table; decodemut: flt_82F88828/flt_82F88868 dequant tables
// and the un-homed synthesis helper sub_82B79820). They are declared so Feed /
// Decode / initmut compile and link against their real homes, and left blocked
// rather than fabricated. Snd::readsamples (0x82B79648) is likewise un-reconstructed.
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

    // X360 sub_82B795A0. Pull aiCount bits out of the reader (advancing it) and
    // return them. BLOCKED body (needs the MT codebook tables, not in the dossier);
    // forward-declared so initmut can call it. The X360 passes the whole decoder
    // object in r3, whose first member IS the reader, so &decoder == &decoder.mReader.
    s32 getbits(MtBitReader* apBits, s32 aiCount);

    class CMTBLKDecf;

    // ------------------------------------------------------------------------
    // CMTBLKDecf -- MicroTalk coded-block decoder. Feed() primes it with a coded
    // block; Decode() streams decoded f32 samples out of an internal window,
    // refilling the window from the block via decodemut() as it is drained.
    //
    // Layout is fixed by the X360 member displacements (offsets in comments are the
    // X360 byte offsets; on PC the pointer members widen and the offsets grow, but
    // every access is by name so parity holds). No constructor is reconstructed --
    // none is a ledger target and no ctor asm is in the dossier, so the members are
    // left with their attested types and unattested slack is not fabricated.
    // ------------------------------------------------------------------------
    class CMTBLKDecf
    {
    public:
        // 0x82B7A3B8. Prime the decoder with a coded MicroTalk block. Fails (-1)
        // when apSource is null or a previous block is still pending
        // (miBlockRemaining != 0). On success stores the block span, resets the
        // window, notes a leading 0xEE frame marker (mode 1 only) and calls initmut.
        s32 Feed(u8* apSource, s32 aiCapacity, s32 aiRemaining);

        // 0x82B84F28. Emit up to aiCount decoded samples into *appDest, streaming
        // them out of mafWindow and calling decodemut() to refill the window (and,
        // in mode 1, re-parsing the per-frame header) whenever it empties. Returns
        // the sample count actually produced.
        s32 Decode(f32* const* appDest, s32 aiCount);

        // 0x82B79EC0. Prime the bit reader from apSource and, when aiInit is set,
        // build the per-block scale table and zero the working buffers. The X360
        // passes the operated-on object explicitly in r3 (this) AND r5 (apDec) --
        // Feed passes the same pointer for both; the body works through apDec, so
        // it is kept as an explicit parameter (this is unused, matching the asm).
        void initmut(u8* apSource, CMTBLKDecf* apDec, s32 aiInit);

        MtBitReader mReader;        // 0x000  inline LSB-first bit reader
        s32   miReadA;              // 0x00C  getbits(1) from the block prologue
        s32   miReadB;              // 0x010  32 - getbits(4) from the block prologue
        f32   mafScale[64];         // 0x014  geometric scale table (ratio recurrence)
        f32   mafGainA[12];         // 0x114  working gains (zeroed by initmut)
        f32   mafGainB[12];         // 0x144  working gains (zeroed by initmut)
        u32   mauState[324];        // 0x174  synthesis state (zeroed by initmut)
        f32   mafWindow[432];       // 0x684  decoded-sample output window
        s32   miWindowCount;        // 0xD44  samples still staged in mafWindow
        const u8* mpBlock;          // 0xD48  current position in the coded block
        f32*  mpOutCursor;          // 0xD4C  caller output cursor (set each Decode)
        s16   miHeaderA;            // 0xD50  per-frame sample count (BE header)
        s16   miHeaderB;            // 0xD52  per-frame window offset (BE header)
        s32   miBlockRemaining;     // 0xD54  samples still to emit from this block
        s32   miCapacity;           // 0xD58  block capacity handed to Feed
        s32   miInitFlag;           // 0xD5C  full-init selector passed to initmut
        s32   miFrameFlag;          // 0xD60  set when a 0xEE frame boundary is seen
        s32   miMode;               // 0xD64  decode mode (==1 drives per-frame reload)
    };

    // 0x82B79FE8. Refill mafWindow with the next frame of decoded samples. BLOCKED
    // body (needs the MT dequantiser tables + sub_82B79820 synthesis helper, none in
    // the dossier); forward-declared so Decode can call it against its real home.
    s32 decodemut(CMTBLKDecf* apDec);
} // namespace Snd

#endif // SDKS_EATECH_SND_CMTBLKDECF_H
