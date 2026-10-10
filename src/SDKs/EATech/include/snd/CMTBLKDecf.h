#ifndef SDKS_EATECH_SND_CMTBLKDECF_H
#define SDKS_EATECH_SND_CMTBLKDECF_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.h
//
// EATech "Snd" MicroTalk (MT) block decoder. This header homes the LSB-first bit
// reader the MT decoder walks its coded block with, the free helpers that consume
// bits from it, the per-frame decode path and the CMTBLKDecf decoder object:
//
//   - Snd::discardbits          drop bits from the reader
//   - Snd::getbits              pull bits from the reader
//   - Snd::readsamples          entropy-decode one 108-sample excitation subframe
//   - Snd::decodemut            decode one 432-sample frame into the window
//   - Snd::CMTBLKDecf::Feed / Decode / initmut
//
// All of them are defined in CMTBLKDecf.cpp. The codebook, dequantiser and
// reflection-coefficient tables those bodies index are dumped from the image
// into that file.
//
// The bit reader's three-field shape is attested by discardbits/getbits and by
// the MT decode path (readsamples reads the accumulator low byte at +4):
//   +0 source byte cursor, +4 bit accumulator, +8 valid-bit count. On the PC
// target the pointer widens to 8 bytes; access is by named member so the widened
// layout stays parity-correct (semantic parity, not byte offsets).
//
// CMTBLKDecf embeds that reader at offset 0 (there is NO vtable pointer -- initmut
// stores the source cursor straight to +0, so the class is a plain, non-polymorphic
// decoder object, unlike the CShortDestDecoder PCM family). Its member layout is
// pinned by the console displacements in Feed/Decode/initmut/decodemut (see the
// offset comments on each member below); on PC the absolute offsets widen with the
// pointers, so parity is by named member, not byte offset.
// ============================================================================

namespace Snd
{
    // LSB-first bit reader that the MicroTalk decoder feeds its coded block into.
    // discardbits/getbits pull bits out of muAccum and refill a byte at a time
    // from mpSource. In the MT decode state this reader occupies the first fields
    // of the larger decoder object.
    struct MtBitReader
    {
        const u8* mpSource;   // +0x0  next input byte
        u32       muAccum;    // +0x4  bit accumulator (low bits consumed first)
        s32       miBitsLeft; // +0x8  number of valid bits still in muAccum
    };

    // Drop aiBits from the accumulator (shift them out of the low end),
    // decrementing the valid-bit count; if fewer than 8 bits remain, pull one
    // more byte from mpSource and splice it in above the surviving bits.
    // Returns apBits (the console leaves the reader pointer in the return register).
    MtBitReader* discardbits(MtBitReader* apBits, s32 aiBits);

    // Pull aiCount (0..8) bits out of the low end of the accumulator, advancing
    // the reader exactly like discardbits, and return them right-justified.
    s32 getbits(MtBitReader* apBits, s32 aiCount);

    class CMTBLKDecf;

    // ------------------------------------------------------------------------
    // CMTBLKDecf -- MicroTalk coded-block decoder. Feed() primes it with a coded
    // block; Decode() streams decoded f32 samples out of an internal window,
    // refilling the window from the block via decodemut() as it is drained.
    //
    // Layout is fixed by the console member displacements (offsets in comments are
    // the console byte offsets; on PC the pointer members widen and the offsets
    // grow, but every access is by name so parity holds). mafExcitation and
    // mafWindow are adjacent f32 arrays with no pointer between them, so the
    // long-term predictor in decodemut can address them as one contiguous
    // 756-sample excitation history on both targets.
    // ------------------------------------------------------------------------
    class CMTBLKDecf
    {
    public:
        // Prime the decoder with a coded MicroTalk block. Fails (-1) when
        // apSource is null or a previous block is still pending
        // (miBlockRemaining != 0). On success stores the block span, resets the
        // window, notes a leading 0xEE frame marker (mode 1 only) and calls initmut.
        s32 Feed(u8* apSource, s32 aiCapacity, s32 aiRemaining);

        // Emit up to aiCount decoded samples into *appDest, streaming them out of
        // mafWindow and calling decodemut() to refill the window (and, in mode 1,
        // re-parsing the per-frame header) whenever it empties. Returns the
        // sample count actually produced.
        s32 Decode(f32* const* appDest, s32 aiCount);

        // Prime the bit reader from apSource and, when aiInit is set, build the
        // per-block scale table and zero the filter state. The console passes the
        // operated-on object explicitly as `this` AND as apDec -- Feed passes the
        // same pointer for both; the body works through apDec, so it is kept as an
        // explicit parameter (this is unused, matching the asm).
        void initmut(u8* apSource, CMTBLKDecf* apDec, s32 aiInit);

        MtBitReader mReader;        // 0x000  inline LSB-first bit reader
        s32   miReadA;              // 0x00C  getbits(1): two-phase (decimated) excitation
        s32   miReadB;              // 0x010  32 - getbits(4): codebook-selector threshold
        f32   mafScale[64];         // 0x014  geometric excitation gain table
        f32   mafReflection[12];    // 0x114  current reflection coefficients
        f32   mafSynthHistory[12];  // 0x144  all-pole synthesis filter history
        f32   mafExcitation[324];   // 0x174  long-term predictor excitation history
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

    // Entropy-decode one 108-sample excitation subframe into apDest at
    // aiStride-sample spacing. aiCodebook selects the variable-length codebook
    // (non-zero) or the 1/2-bit ternary code (zero).
    void readsamples(CMTBLKDecf* apDec, s32 aiCodebook, f32* apDest, s32 aiStride);

    // Decode the next 432-sample frame into mafWindow: reflection coefficients,
    // four 108-sample subframes of long-term-predicted excitation, then the
    // interpolated 12-pole synthesis filter.
    void decodemut(CMTBLKDecf* apDec);
} // namespace Snd

#endif // SDKS_EATECH_SND_CMTBLKDECF_H
