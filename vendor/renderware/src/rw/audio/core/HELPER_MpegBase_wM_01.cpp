// =====================================================================================
// rw::audio::core::HELPER_CMpegBase -- part-file 01: the TU's one remaining ledger body,
//
//   rw::audio::core::HELPER_CMpegBase::PolySynth  @0x82B87080  (poly-phase synthesis)
//   <file-static> Dct32                           @sub_82B86488 (its 32-point DCT helper)
//
// It lands in this part-file rather than in the committed HELPER_CMpegBase.cpp because
// that file is owned by another worker this wave. Everything the two bodies need beyond
// the class itself (the two const tables, the .data scratch and Dct32) therefore lives in
// THIS translation unit's anonymous namespace -- internal linkage, so there is no ODR
// interaction with the sibling CMpegBase part-file, which defines its own copies of the
// same-shaped tables and its own Dct32 for sub_82B8E258.
//
// NAMING NOTE: the wave brief named this part-file MpegBase_wM_01.cpp, but that exact path
// was already taken by the sibling TU's worker (it holds rw::audio::core::CMpegBase::
// PolySynth @0x82B8EE50). Overwriting it would have destroyed that body, so this file
// carries the HELPER_ prefix of its own class instead. Nothing else about the assignment
// changed; see the report for the conductor.
//
// PROVENANCE. No Feb-2007 source and no DecFIGS DWARF covers this TU, so the X360 PowerPC
// asm of BURNOUT_X360_ARTIST.XEX is the only authority. MEASURED here means "re-derived
// for this file", not "quoted from the spec":
//   * .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B87080.json and 0x82B86488.json -- the raw
//     assembly of both functions, walked instruction by instruction.
//   * scratchpad/waveM/HelperMpegBase.spec.md sections 4.1-4.4 -- the recovered contract
//     and the rodata dumps. Every literal in the two tables below was re-checked against
//     the raw big-endian dwords in scratchpad/waveM/probe_mpeg/probe.txt: all 544 window
//     entries and all 31 cosine entries round-trip to the identical IEEE-754 f32 bits.
//   * Dct32's shape was verified, not assumed: sub_82B86488 is branch-free (766
//     instructions; only fadds/fsubs/fmuls/fmr/lfs/stfs plus the __savefpr_14 /
//     __restfpr helper calls), so it was executed numerically over 94 inputs -- 60 random,
//     all-zero, all-one and the 32 unit impulses -- and compared against the C++ below.
//     All 33 stores matched BIT-EXACTLY on every input (relative deviation 0.0), which is
//     what licenses the recursive form: it is a re-encoding of the measured dataflow, NOT
//     an mpg123 dct64 recalled from memory.
//
// HOST-vs-CONSOLE (gotcha 1). No console pointer, offset or object size is reproduced
// here. Every class member is reached BY NAME, and the history block is indexed through
// the header's typed `f32 (*)[2][288]`, so the console's 0x900 / 0x480 / 0x120 / 0x40 /
// 0x80 byte strides fall out of the type on both platforms instead of being written down.
// The only numeric constants below (16, 32, 288, 31, 64, 544) are FLOAT COUNTS, and f32 is
// 4 bytes on the X360 and on x64 alike, so they carry over unchanged.
//
// NaN POLARITY (gotcha 4): not applicable. There is no fcmpu, and no branch of any kind,
// in either function -- PolySynth's only branches are its two `addic.`/`bne` counted
// loops. Both bodies are straight-line float arithmetic.
// =====================================================================================

#include "rw/audio/core/HELPER_CMpegBase.h"

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// -------------------------------------------------------------------------------------
// The poly-phase synthesis window -- unk_82156740 .. 0x82156FBC in the X360 rodata, 544
// f32. This TU's private copy; the sibling CMpegBase TU has a byte-identical one at
// 0x82159D70 (the linker kept both, so each was a `static const` in its own source file).
//
// The count is BOUNDED, not guessed: the preceding rodata head is the "PolySynthHistoryF"
// allocation-tag string @0x8215672C, the following head is the cosine table @0x82156FC0,
// and the maximum byte the consumers can reach is 0x82156FB4 -- the windowing code's
// widest span is index (16 - iCol) + 526 with iCol >= 1, i.e. 541 < 544.
//
// The content is ISO 11172-3 Table B.3 scaled by 32768, laid out as 17 rows of 32 in which
// EACH 16-float half-row is stored TWICE (measured: every row satisfies w[32r+k] ==
// w[32r+16+k]). That duplication is what lets the [16 - iCol] base index every rotation
// phase without wrapping.
// -------------------------------------------------------------------------------------
static const f32 KAF_SynthWindow[544] = {
    0.0f, 14.5f, -106.5f, 229.5f, -1018.5f, 2576.5f, -3287.0f, 18744.5f,  // [0..15]
    -37519.0f, -18744.5f, -3287.0f, -2576.5f, -1018.5f, -229.5f, -106.5f, -14.5f,
    0.0f, 14.5f, -106.5f, 229.5f, -1018.5f, 2576.5f, -3287.0f, 18744.5f,  // [16..31]
    -37519.0f, -18744.5f, -3287.0f, -2576.5f, -1018.5f, -229.5f, -106.5f, -14.5f,
    0.5f, 15.5f, -109.0f, 259.5f, -1000.0f, 2758.5f, -2979.5f, 19668.0f,  // [32..47]
    -37496.0f, -17820.0f, -3567.0f, -2394.0f, -1031.5f, -200.5f, -104.0f, -13.0f,
    0.5f, 15.5f, -109.0f, 259.5f, -1000.0f, 2758.5f, -2979.5f, 19668.0f,  // [48..63]
    -37496.0f, -17820.0f, -3567.0f, -2394.0f, -1031.5f, -200.5f, -104.0f, -13.0f,
    0.5f, 17.5f, -111.0f, 290.5f, -976.0f, 2939.5f, -2644.0f, 20588.0f,  // [64..79]
    -37428.0f, -16895.5f, -3820.0f, -2212.5f, -1040.0f, -173.5f, -101.0f, -12.0f,
    0.5f, 17.5f, -111.0f, 290.5f, -976.0f, 2939.5f, -2644.0f, 20588.0f,  // [80..95]
    -37428.0f, -16895.5f, -3820.0f, -2212.5f, -1040.0f, -173.5f, -101.0f, -12.0f,
    0.5f, 19.0f, -112.5f, 322.5f, -946.5f, 3118.5f, -2280.5f, 21503.0f,  // [96..111]
    -37315.0f, -15973.5f, -4046.0f, -2031.5f, -1043.5f, -147.0f, -98.0f, -10.5f,
    0.5f, 19.0f, -112.5f, 322.5f, -946.5f, 3118.5f, -2280.5f, 21503.0f,  // [112..127]
    -37315.0f, -15973.5f, -4046.0f, -2031.5f, -1043.5f, -147.0f, -98.0f, -10.5f,
    0.5f, 20.5f, -113.5f, 355.5f, -911.0f, 3294.5f, -1888.0f, 22410.5f,  // [128..143]
    -37156.5f, -15056.0f, -4246.0f, -1852.5f, -1042.5f, -122.0f, -95.0f, -9.5f,
    0.5f, 20.5f, -113.5f, 355.5f, -911.0f, 3294.5f, -1888.0f, 22410.5f,  // [144..159]
    -37156.5f, -15056.0f, -4246.0f, -1852.5f, -1042.5f, -122.0f, -95.0f, -9.5f,
    0.5f, 22.5f, -114.0f, 389.5f, -869.5f, 3467.5f, -1467.5f, 23308.5f,  // [160..175]
    -36954.0f, -14144.5f, -4420.0f, -1675.5f, -1037.5f, -98.5f, -91.5f, -8.5f,
    0.5f, 22.5f, -114.0f, 389.5f, -869.5f, 3467.5f, -1467.5f, 23308.5f,  // [176..191]
    -36954.0f, -14144.5f, -4420.0f, -1675.5f, -1037.5f, -98.5f, -91.5f, -8.5f,
    0.5f, 24.5f, -114.0f, 424.0f, -822.0f, 3635.5f, -1018.5f, 24195.0f,  // [192..207]
    -36707.5f, -13241.0f, -4569.5f, -1502.0f, -1028.5f, -76.5f, -88.0f, -8.0f,
    0.5f, 24.5f, -114.0f, 424.0f, -822.0f, 3635.5f, -1018.5f, 24195.0f,  // [208..223]
    -36707.5f, -13241.0f, -4569.5f, -1502.0f, -1028.5f, -76.5f, -88.0f, -8.0f,
    1.0f, 26.5f, -113.5f, 459.5f, -767.5f, 3798.5f, -541.0f, 25068.5f,  // [224..239]
    -36417.5f, -12347.0f, -4694.5f, -1331.5f, -1016.0f, -55.5f, -84.5f, -7.0f,
    1.0f, 26.5f, -113.5f, 459.5f, -767.5f, 3798.5f, -541.0f, 25068.5f,  // [240..255]
    -36417.5f, -12347.0f, -4694.5f, -1331.5f, -1016.0f, -55.5f, -84.5f, -7.0f,
    1.0f, 29.0f, -112.0f, 495.5f, -707.0f, 3955.0f, -35.0f, 25926.5f,  // [256..271]
    -36084.5f, -11464.5f, -4796.0f, -1165.0f, -1000.5f, -36.0f, -80.5f, -6.5f,
    1.0f, 29.0f, -112.0f, 495.5f, -707.0f, 3955.0f, -35.0f, 25926.5f,  // [272..287]
    -36084.5f, -11464.5f, -4796.0f, -1165.0f, -1000.5f, -36.0f, -80.5f, -6.5f,
    1.0f, 31.5f, -110.5f, 532.0f, -640.0f, 4104.5f, 499.0f, 26767.0f,  // [288..303]
    -35710.0f, -10594.5f, -4875.0f, -1003.0f, -981.0f, -18.0f, -77.0f, -5.5f,
    1.0f, 31.5f, -110.5f, 532.0f, -640.0f, 4104.5f, 499.0f, 26767.0f,  // [304..319]
    -35710.0f, -10594.5f, -4875.0f, -1003.0f, -981.0f, -18.0f, -77.0f, -5.5f,
    1.0f, 34.0f, -107.5f, 568.5f, -565.5f, 4245.5f, 1061.0f, 27589.0f,  // [320..335]
    -35295.0f, -9739.0f, -4931.5f, -846.0f, -959.5f, -1.0f, -73.5f, -5.0f,
    1.0f, 34.0f, -107.5f, 568.5f, -565.5f, 4245.5f, 1061.0f, 27589.0f,  // [336..351]
    -35295.0f, -9739.0f, -4931.5f, -846.0f, -959.5f, -1.0f, -73.5f, -5.0f,
    1.5f, 36.5f, -104.0f, 605.0f, -485.0f, 4377.5f, 1650.0f, 28389.0f,  // [352..367]
    -34839.5f, -8899.5f, -4967.5f, -694.0f, -935.0f, 14.5f, -69.5f, -4.5f,
    1.5f, 36.5f, -104.0f, 605.0f, -485.0f, 4377.5f, 1650.0f, 28389.0f,  // [368..383]
    -34839.5f, -8899.5f, -4967.5f, -694.0f, -935.0f, 14.5f, -69.5f, -4.5f,
    1.5f, 39.5f, -100.0f, 641.5f, -397.0f, 4499.0f, 2266.5f, 29166.5f,  // [384..399]
    -34346.0f, -8077.5f, -4983.0f, -547.5f, -908.5f, 28.5f, -66.0f, -4.0f,
    1.5f, 39.5f, -100.0f, 641.5f, -397.0f, 4499.0f, 2266.5f, 29166.5f,  // [400..415]
    -34346.0f, -8077.5f, -4983.0f, -547.5f, -908.5f, 28.5f, -66.0f, -4.0f,
    2.0f, 42.5f, -94.5f, 678.0f, -302.5f, 4609.5f, 2909.0f, 29919.0f,  // [416..431]
    -33814.5f, -7274.0f, -4979.5f, -407.0f, -879.5f, 41.5f, -62.5f, -3.5f,
    2.0f, 42.5f, -94.5f, 678.0f, -302.5f, 4609.5f, 2909.0f, 29919.0f,  // [432..447]
    -33814.5f, -7274.0f, -4979.5f, -407.0f, -879.5f, 41.5f, -62.5f, -3.5f,
    2.0f, 45.5f, -88.5f, 714.0f, -201.0f, 4708.0f, 3577.0f, 30644.5f,  // [448..463]
    -33247.0f, -6490.0f, -4958.0f, -272.5f, -849.0f, 53.0f, -58.5f, -3.5f,
    2.0f, 45.5f, -88.5f, 714.0f, -201.0f, 4708.0f, 3577.0f, 30644.5f,  // [464..479]
    -33247.0f, -6490.0f, -4958.0f, -272.5f, -849.0f, 53.0f, -58.5f, -3.5f,
    2.5f, 48.5f, -81.5f, 749.0f, -92.5f, 4792.5f, 4270.0f, 31342.0f,  // [480..495]
    -32645.0f, -5727.5f, -4919.0f, -144.0f, -817.0f, 63.5f, -55.5f, -3.0f,
    2.5f, 48.5f, -81.5f, 749.0f, -92.5f, 4792.5f, 4270.0f, 31342.0f,  // [496..511]
    -32645.0f, -5727.5f, -4919.0f, -144.0f, -817.0f, 63.5f, -55.5f, -3.0f,
    2.5f, 52.0f, -73.0f, 783.5f, 22.5f, 4863.5f, 4987.5f, 32009.5f,  // [512..527]
    -32009.5f, -4987.5f, -4863.5f, -22.5f, -783.5f, 73.0f, -52.0f, -2.5f,
    2.5f, 52.0f, -73.0f, 783.5f, 22.5f, 4863.5f, 4987.5f, 32009.5f,  // [528..543]
    -32009.5f, -4987.5f, -4863.5f, -22.5f, -783.5f, 73.0f, -52.0f, -2.5f,
};

// -------------------------------------------------------------------------------------
// The DCT-32 cosine table -- flt_82156FC0 .. flt_82157038, 31 f32, read by sub_82B86488 as
// five consecutive rows of 0.5 / cos((2i+1) * pi / (2N)). In the image this table is
// physically SHARED with the sibling's sub_82B8E258 (identical data, folded by the
// linker); in source each TU carried its own static, so this file-static copy is the
// faithful reconstruction and the sibling part-file defines its own.
// -------------------------------------------------------------------------------------
static const f32 KAF_Dct32Cos[31] = {
    // [0..15]  stage 0, N=32: 0.5/cos((2i+1)*pi/64)   (asm base flt_82156FC0)
    0.50060302f, 0.505470932f, 0.515447319f, 0.531042576f,
    0.553103924f, 0.582934976f, 0.622504115f, 0.674808323f,
    0.744536281f, 0.839349627f, 0.972568214f, 1.16943991f,
    1.4841646f, 2.05778098f, 3.40760851f, 10.1900082f,
    // [16..23] stage 1, N=16: 0.5/cos((2i+1)*pi/32)   (flt_82157000)
    0.502419293f, 0.522498608f, 0.566944063f, 0.646821797f,
    0.788154602f, 1.06067765f, 1.72244716f, 5.10114861f,
    // [24..27] stage 2, N=8:  0.5/cos((2i+1)*pi/16)   (flt_82157020)
    0.509795606f, 0.601344883f, 0.899976194f, 2.56291556f,
    // [28..29] stage 3, N=4:  0.5/cos((2i+1)*pi/8)    (flt_82157030)
    0.541196108f, 1.30656302f,
    // [30]     stage 4, N=2:  0.5/cos(pi/4) = 1/sqrt(2)  (flt_82157038)
    0.707106769f,
};

// The first cosine index of each decomposition stage (the asm reaches these rows as fixed
// displacements off its flt_82157038 anchor: -0x78, -0x38, -0x18, -0x08, 0).
static const s32 KAI_Dct32CosStage[5] = { 0, 16, 24, 28, 30 };

// -------------------------------------------------------------------------------------
// The DCT-32 scratch block -- flt_8327A328 .. 0x8327A424 in the X360 .data, 64 f32. All 64
// slots are touched by sub_82B86488 (measured: the slot indices the asm addresses off the
// flt_8327A328 base cover 0..63 with no gaps). The low 32 hold the block being
// transformed, the high 32 are the butterfly staging area.
//
// This is genuinely a global in the image, not a stack frame, so it is reproduced as one:
// it is written before it is read on every call, so it carries no state between calls, but
// -- exactly like the console build -- this makes Dct32 non-reentrant. The sibling helper
// owns the adjacent block at 0x8327A428.
// -------------------------------------------------------------------------------------
static f32 s_afDct32Scratch[64]; // == flt_8327A328 on the console

// -------------------------------------------------------------------------------------
// Dct32Split -- one level of the Lee fast-DCT decomposition, in place over
// apBlock[0 .. iCount-1], staging through apTemp (iCount floats):
//
//     s[i] = x[i] + x[n-1-i]                                    i = 0 .. n/2-1
//     d[i] = (x[i] - x[n-1-i]) * KAF_Dct32Cos[stage row + i]
//     S = dct(s);  D = dct(d)
//     y[2i] = S[i];   y[2i+1] = D[i] + D[i+1]   (D[n/2] taken as 0)
//
// MEASURED: this is the structure that reproduces sub_82B86488's 33 output stores
// bit-exactly (see the file header for the numeric verification). The asm is fully
// unrolled and software-pipelined across all five stages -- the recursion is the compact
// encoding of that measured dataflow, not a guess at the original source's shape.
// -------------------------------------------------------------------------------------
void Dct32Split(f32 *apBlock, s32 iCount, s32 iStage, f32 *apTemp)
{
    if (iCount < 2)
        return;

    const s32 iHalf = iCount >> 1;
    const f32 *const pCos = &KAF_Dct32Cos[KAI_Dct32CosStage[iStage]];

    for (s32 i = 0; i < iHalf; ++i)
    {
        const f32 lfLow = apBlock[i];
        const f32 lfHigh = apBlock[iCount - 1 - i];
        apTemp[i] = lfLow + lfHigh;
        apTemp[iHalf + i] = (lfLow - lfHigh) * pCos[i];
    }
    for (s32 i = 0; i < iCount; ++i)
        apBlock[i] = apTemp[i];

    Dct32Split(apBlock, iHalf, iStage + 1, apTemp);
    Dct32Split(apBlock + iHalf, iHalf, iStage + 1, apTemp);

    for (s32 i = 0; i < iHalf; ++i)
    {
        apTemp[2 * i] = apBlock[i];
        apTemp[2 * i + 1] = (i + 1 < iHalf)
                                ? (apBlock[iHalf + i] + apBlock[iHalf + i + 1])
                                : apBlock[iHalf + i];
    }
    for (s32 i = 0; i < iCount; ++i)
        apBlock[i] = apTemp[i];
}

// -------------------------------------------------------------------------------------
// Dct32 -- sub_82B86488, called ONLY by PolySynth below (its sole code xref; the other
// xref, into .pdata, is the unwind table). The 32-point MPEG matrixing DCT-II
//
//     dct[k] = sum(n = 0 .. 31) pIn[n] * cos((2n+1) * k * pi / 64)
//
// scattered into the two rotating history columns the caller selected, at a 16-float row
// stride (MEASURED store offsets: pOutA gets 17 stores at byte 0x000 .. 0x400, pOutB 16 at
// 0x000 .. 0x3C0, and dct[16] is stored to BOTH pOutA[0] and pOutB[0]):
//
//     pOutA[16 * r] = dct[16 - r]    r = 0 .. 16
//     pOutB[16 * r] = dct[16 + r]    r = 0 .. 15
//
// The DCT-II identity is INFERENCE from the recovered coefficients (it reproduces the
// direct O(N^2) transform to ~2e-6 relative in f32, i.e. to rounding); the scatter and the
// arithmetic itself are measured.
// -------------------------------------------------------------------------------------
void Dct32(f32 *pOutA, f32 *pOutB, const f32 *pIn)
{
    for (s32 n = 0; n < 32; ++n)
        s_afDct32Scratch[n] = pIn[n];

    Dct32Split(s_afDct32Scratch, 32, 0, &s_afDct32Scratch[32]);

    for (s32 r = 0; r <= 16; ++r)
        pOutA[16 * r] = s_afDct32Scratch[16 - r];
    for (s32 r = 0; r < 16; ++r)
        pOutB[16 * r] = s_afDct32Scratch[16 + r];
}
} // namespace

// -------------------------------------------------------------------------------------
// PolySynth @0x82B87080
//
// One poly-phase synthesis stage for `iChannel`: rotate that channel's band offset
// backwards, run the 32-point matrixing DCT into the two history columns the new offset
// selects, then window 32 PCM samples out of the bank the odd column landed in.
//
// The per-channel history is f32[2][288] -- two banks of 18 rows x 16 rotating columns.
// The DCT fills one column of each bank (rows 0..16 of one, rows 0..15 of the other); the
// windowing pass then reads rows 0..16 forwards and rows 15..1 backwards out of the second
// bank, so the 18-row bank is never overrun (widest touch: row 16, element 14).
//
// Index algebra taken straight from the asm at 0x82B87094 .. 0x82B870F8, the windowing
// from loc_82B87110 (loop 1), 0x82B871E8 (the centre sample) and loc_82B87264 (loop 2).
//
// VERIFIED, with one stated caveat: the whole body -- both counted loops included -- was
// executed instruction by instruction against this exact C++ for all 16 starting band
// offsets x both channels over random history and input, and all 32 output samples, all
// 1152 history floats and both band-offset bytes matched to the bit. The caveat is that
// the interpreter models `fmadds` / `fmsubs` / `fnmsubs` the way C++ must -- rounding the
// product before the add -- so what that run proves is the STRUCTURE (tap order, signs,
// indices, addressing, the phase rotation and the history writes), not that the last bit
// survives the console's genuinely fused multiply-add. See the note on ordering below.
//
// The accumulation ORDER below deliberately mirrors the asm's fused chains rather than
// being rewritten as a tidy sum: f32 addition is not associative, so evaluating the taps
// in a different order would move the low bits of the PCM output. Loop 1 starts from tap 1
// (`fmuls`) and folds tap 0 in with `fmsubs` before running taps 2..15 alternately
// `fmadds` / `fnmsubs`; loop 2 negates after its first product and subtracts every
// remaining term. The console's fused multiply-add cannot be expressed portably in C++, so
// the products round separately here -- that is the one deliberate (and unavoidable)
// numeric divergence from the X360 build.
// -------------------------------------------------------------------------------------
void HELPER_CMpegBase::PolySynth(s32 iChannel, f32 *pOutSamples, f32 *pInSamples)
{
    // Rotate this channel's band offset down and store it back before anything reads it
    // (the asm re-loads the byte it just stored).
    const s32 iOffset = (static_cast<s32>(mucBandOffset[iChannel]) - 1) & 0xF;
    mucBandOffset[iChannel] = static_cast<u8>(iOffset);

    const s32 iOdd = iOffset & 1;
    const s32 iNotOdd = iOdd ^ 1;
    const s32 iCol = iOffset + iNotOdd; // always odd, and always in [1..15] -- never 0/16

    f32 *const pColumnA = &mpPolySynthHistoryF[iChannel][iOdd][(iOffset + iOdd) & 0xF];
    f32 *const pColumnB = &mpPolySynthHistoryF[iChannel][iNotOdd][iCol];

    Dct32(pColumnA, pColumnB, pInSamples);

    // The windowing pass always walks the bank the odd column just landed in.
    const f32 *const pBank = &mpPolySynthHistoryF[iChannel][iNotOdd][0];
    const f32 *const pWindow = &KAF_SynthWindow[16 - iCol];

    // out[0..15]: 16 taps each, alternating sign; window row stride 32, history stride 16.
    for (s32 j = 0; j < 16; ++j)
    {
        const f32 *const pW = &pWindow[32 * j];
        const f32 *const pH = &pBank[16 * j];

        f32 lfAcc = pW[1] * pH[1];     // fmuls
        lfAcc = pW[0] * pH[0] - lfAcc; // fmsubs
        for (s32 t = 2; t < 16; ++t)
        {
            const f32 lfTerm = pW[t] * pH[t];
            lfAcc = ((t & 1) != 0) ? (lfAcc - lfTerm)  // fnmsubs
                                   : (lfTerm + lfAcc); // fmadds
        }
        pOutSamples[j] = lfAcc;
    }

    // out[16]: the centre sample -- the 8 even-indexed taps of row 16, all added.
    {
        const f32 *const pW = &pWindow[512];
        const f32 *const pH = &pBank[256];

        f32 lfAcc = pW[0] * pH[0];
        for (s32 t = 2; t < 16; t += 2)
            lfAcc = pW[t] * pH[t] + lfAcc;
        pOutSamples[16] = lfAcc;
    }

    // out[17..31]: the mirrored half. The window is rebased to KAF_SynthWindow[496 + iCol]
    // (the asm forms it as `windowAfterLoop1 + 8 * (iCol - 16)` bytes) and walks backwards
    // 32 floats per output; the history walks backwards from row 15 down to row 1; every
    // term is subtracted. NOTE the last term pairs pW2[0] -- not pW2[-16] -- with pH[15];
    // that is verbatim from the asm (`lfs f17, 0(r10)` against `lfs f18, 0x3C(r11)`), not a
    // transcription slip.
    const f32 *const pWindowRev = &KAF_SynthWindow[496 + iCol];
    for (s32 k = 0; k < 15; ++k)
    {
        const f32 *const pW2 = &pWindowRev[-32 * k];
        const f32 *const pH = &pBank[240 - 16 * k];

        f32 lfAcc = pW2[-1] * pH[0]; // fmuls
        lfAcc = -lfAcc;              // fneg
        for (s32 t = 1; t < 15; ++t)
            lfAcc = lfAcc - pW2[-1 - t] * pH[t]; // fnmsubs
        lfAcc = lfAcc - pW2[0] * pH[15];         // fnmsubs -- the pW2[0] quirk noted above

        pOutSamples[17 + k] = lfAcc;
    }
}

} // namespace core
} // namespace audio
} // namespace rw
