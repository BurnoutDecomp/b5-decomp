// =====================================================================================
// SDKs/EATech/include/snd/CMpegBase_wO_01.cpp
//
// Part-file 01 of the class:Snd::CMpegBase TU. It carries the two bodies that were
// still missing from the committed CMpegBase.cpp:
//
//   Snd::CMpegBase::PolySynth  @0x82B771D8  (the poly-phase synthesis stage)
//   Snd::CMpegBase::Close      @0x82B74CF0  (X360 vtable slot +0x0C)
//   <file-static> Dct32        @sub_82B765E0 (PolySynth's 32-point matrixing helper)
//
// It lands in a part-file rather than in CMpegBase.cpp only because this wave's worker
// may not edit the already-committed file; the two bodies belong in CMpegBase.cpp and
// can be folded straight into it (the file-static tables and helper below simply move
// into that file's existing anonymous namespace).
//
// PROVENANCE. No Feb-2007 source and no DecFIGS DWARF exist for this TU, so the X360
// PowerPC asm of BURNOUT_X360_ARTIST.XEX is the only authority.
//   * PolySynth asm: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B771D8.json (182 insns,
//     re-walked instruction by instruction for this file).
//   * Dct32 asm: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B765E0.json (766 straight-line
//     insns, no loops and no calls). Its ONLY xref is PolySynth (measured, xrefs_to in
//     that same JSON), which is why it is homed here as a file-static.
//   * Close asm: dumped with headless IDA this wave (the address is absent from
//     .ida-exports -- an identity gap); the disassembly is transcribed in the body
//     comment below and reproduced in scratchpad/waveO/SndCMpegBase.spec.md section 6.
//   * Tables: scratchpad/waveO/SndCMpegBase.spec.md sections 2-3 dump unk_8214B6C0
//     (544 f32 synthesis window) and flt_8214BF40 (31 f32 DCT coefficients) out of the
//     image as raw big-endian dwords; every literal below round-trips to those dwords.
//
// TRANSFER FROM WAVE M (measured here, not assumed). Normalized instruction diff of
// this TU's pair against the wave-M interpreter-verified rw::audio::core twins:
//   * sub_82B765E0 vs sub_82B8E258       -> 766 vs 766 insns, IDENTICAL after masking
//     addresses and rodata symbol names.
//   * PolySynth @0x82B771D8 vs @0x82B8EE50 -> 182 vs 182 insns, ONE differing line:
//         Snd : lwz r9, 0x50(r3)   (mpPolySynthHistory)
//         core: lwz r9, 0x4C(r3)   (mpPolySynthWork)
//     Everything else, including all three windowing loops, is line-for-line equal.
//   Wave M verified its body by executing the real PPC asm in an interpreter for all
//   16 phases x 2 channels; scratchpad/waveM/probe_mpeg/refcheck.py was re-run for this
//   file and reports "cases: 32  worst out rel err: 3.29e-05  struct_bad: 0
//   hist_bad: 0" -- all 32 outputs, the phase byte and all 576 history floats agree.
//   The tolerance is relative (1e-4) rather than bit-exact because the model evaluates
//   the fast DCT in a different f32 rounding order than the console's unrolled
//   schedule; the structure, the indices and the side effects are exact.
//
// HOST-vs-CONSOLE. The console immediates reproduced here are 2304 / 576 / 288 / 240 /
// 16 / 32 / 15 / 496 / 512 (the last four being PolySynth's centre-sample window/bank
// bases, the reverse-half window and bank bases, and the 15-iteration counts), and
// EVERY ONE of them is a FLOAT-COUNT (or float-count x 4 byte) fact of the poly-synth
// history layout: f32 is 4 bytes on the X360 and on x64 alike, so they carry over
// unchanged. Every class member is reached by NAME and never by a hardcoded offset --
// the console offsets quoted in comments (+0x38 phase bytes, +0x47 mucIsOpen, +0x4C
// muHeaderWord, +0x50 mpPolySynthHistory) are documentation only. There is no
// floating-point compare in either function, so the PPC NaN-polarity trap
// (bge == bc 4,LT) does not arise; both are straight-line arithmetic plus counted
// `addic.` / `bne` loops.
//
// NOT DONE HERE (owned by files this worker may not edit):
//   * The stale "PolySynth ... NOT YET reconstructed" / "Close ... definition pending"
//     comment blocks at CMpegBase.cpp lines 17-31 and the matching doc-comments in
//     CMpegBase.h (lines 96-101 and 134-138) are wrong as of this file and should be
//     rewritten when this part-file is folded in.
//   * Snd::CMpegBase::Close @0x82B74CF0 is missing from progress/identity.json (it is
//     named in the IDB and occupies vtable slot +0x0C). Conductor: add it; it is NOT
//     one of this TU's 9 ledger functions.
// =====================================================================================

#include "SDKs/EATech/include/snd/CMpegBase.h"

namespace Snd
{

namespace
{
// dword_83277834 -- the Snd free hook (a function pointer), the pair of the allocator
// hook dword_83277830 that the committed OpenSynth calls (830 allocates, 834 frees; the
// two globals are adjacent). Owned by the Snd allocator TU; referenced by extern here,
// never fabricated. Measured: `lwz r11, dword_83277834@l(r10); mtctr r11; bctrl` with
// r3 = mpPolySynthHistory, and the callee's result is discarded (`li r3, 0` follows), so
// the void return. 16 code xrefs across the Snd audio module (dumped this wave).
extern "C" void (*dword_83277834)(void* apBlock);

// -------------------------------------------------------------------------------------
// Poly-phase synthesis window -- unk_8214B6C0 in BURNOUT_X360_ARTIST.XEX (544 f32 =
// 0x880 bytes; the next rodata object, the DCT row flt_8214BF40, starts at +544).
// ISO 11172-3 Annex B Table B.3 D[] x 32768, sign-folded for the alternating-sign
// windowing loops -- NOT the textbook fractional D[], which is why it must never be
// filled in from the standard. Byte-identical second copy at unk_82159D70 (the
// rw::audio::core twin; compared dword-for-dword in the image). Only consumer:
// Snd::CMpegBase::PolySynth. Index 0 is never read at runtime (the window pointer is
// KF_SynthWindow + 16 - odd column, and the odd column is >= 1).
// -------------------------------------------------------------------------------------
static const f32 KF_SynthWindow[544] = {
              0.0f,          14.5f,        -106.5f,         229.5f,   // [0..3]
          -1018.5f,        2576.5f,       -3287.0f,       18744.5f,   // [4..7]
         -37519.0f,      -18744.5f,       -3287.0f,       -2576.5f,   // [8..11]
          -1018.5f,        -229.5f,        -106.5f,         -14.5f,   // [12..15]
              0.0f,          14.5f,        -106.5f,         229.5f,   // [16..19]
          -1018.5f,        2576.5f,       -3287.0f,       18744.5f,   // [20..23]
         -37519.0f,      -18744.5f,       -3287.0f,       -2576.5f,   // [24..27]
          -1018.5f,        -229.5f,        -106.5f,         -14.5f,   // [28..31]
              0.5f,          15.5f,        -109.0f,         259.5f,   // [32..35]
          -1000.0f,        2758.5f,       -2979.5f,       19668.0f,   // [36..39]
         -37496.0f,      -17820.0f,       -3567.0f,       -2394.0f,   // [40..43]
          -1031.5f,        -200.5f,        -104.0f,         -13.0f,   // [44..47]
              0.5f,          15.5f,        -109.0f,         259.5f,   // [48..51]
          -1000.0f,        2758.5f,       -2979.5f,       19668.0f,   // [52..55]
         -37496.0f,      -17820.0f,       -3567.0f,       -2394.0f,   // [56..59]
          -1031.5f,        -200.5f,        -104.0f,         -13.0f,   // [60..63]
              0.5f,          17.5f,        -111.0f,         290.5f,   // [64..67]
           -976.0f,        2939.5f,       -2644.0f,       20588.0f,   // [68..71]
         -37428.0f,      -16895.5f,       -3820.0f,       -2212.5f,   // [72..75]
          -1040.0f,        -173.5f,        -101.0f,         -12.0f,   // [76..79]
              0.5f,          17.5f,        -111.0f,         290.5f,   // [80..83]
           -976.0f,        2939.5f,       -2644.0f,       20588.0f,   // [84..87]
         -37428.0f,      -16895.5f,       -3820.0f,       -2212.5f,   // [88..91]
          -1040.0f,        -173.5f,        -101.0f,         -12.0f,   // [92..95]
              0.5f,          19.0f,        -112.5f,         322.5f,   // [96..99]
           -946.5f,        3118.5f,       -2280.5f,       21503.0f,   // [100..103]
         -37315.0f,      -15973.5f,       -4046.0f,       -2031.5f,   // [104..107]
          -1043.5f,        -147.0f,         -98.0f,         -10.5f,   // [108..111]
              0.5f,          19.0f,        -112.5f,         322.5f,   // [112..115]
           -946.5f,        3118.5f,       -2280.5f,       21503.0f,   // [116..119]
         -37315.0f,      -15973.5f,       -4046.0f,       -2031.5f,   // [120..123]
          -1043.5f,        -147.0f,         -98.0f,         -10.5f,   // [124..127]
              0.5f,          20.5f,        -113.5f,         355.5f,   // [128..131]
           -911.0f,        3294.5f,       -1888.0f,       22410.5f,   // [132..135]
         -37156.5f,      -15056.0f,       -4246.0f,       -1852.5f,   // [136..139]
          -1042.5f,        -122.0f,         -95.0f,          -9.5f,   // [140..143]
              0.5f,          20.5f,        -113.5f,         355.5f,   // [144..147]
           -911.0f,        3294.5f,       -1888.0f,       22410.5f,   // [148..151]
         -37156.5f,      -15056.0f,       -4246.0f,       -1852.5f,   // [152..155]
          -1042.5f,        -122.0f,         -95.0f,          -9.5f,   // [156..159]
              0.5f,          22.5f,        -114.0f,         389.5f,   // [160..163]
           -869.5f,        3467.5f,       -1467.5f,       23308.5f,   // [164..167]
         -36954.0f,      -14144.5f,       -4420.0f,       -1675.5f,   // [168..171]
          -1037.5f,         -98.5f,         -91.5f,          -8.5f,   // [172..175]
              0.5f,          22.5f,        -114.0f,         389.5f,   // [176..179]
           -869.5f,        3467.5f,       -1467.5f,       23308.5f,   // [180..183]
         -36954.0f,      -14144.5f,       -4420.0f,       -1675.5f,   // [184..187]
          -1037.5f,         -98.5f,         -91.5f,          -8.5f,   // [188..191]
              0.5f,          24.5f,        -114.0f,         424.0f,   // [192..195]
           -822.0f,        3635.5f,       -1018.5f,       24195.0f,   // [196..199]
         -36707.5f,      -13241.0f,       -4569.5f,       -1502.0f,   // [200..203]
          -1028.5f,         -76.5f,         -88.0f,          -8.0f,   // [204..207]
              0.5f,          24.5f,        -114.0f,         424.0f,   // [208..211]
           -822.0f,        3635.5f,       -1018.5f,       24195.0f,   // [212..215]
         -36707.5f,      -13241.0f,       -4569.5f,       -1502.0f,   // [216..219]
          -1028.5f,         -76.5f,         -88.0f,          -8.0f,   // [220..223]
              1.0f,          26.5f,        -113.5f,         459.5f,   // [224..227]
           -767.5f,        3798.5f,        -541.0f,       25068.5f,   // [228..231]
         -36417.5f,      -12347.0f,       -4694.5f,       -1331.5f,   // [232..235]
          -1016.0f,         -55.5f,         -84.5f,          -7.0f,   // [236..239]
              1.0f,          26.5f,        -113.5f,         459.5f,   // [240..243]
           -767.5f,        3798.5f,        -541.0f,       25068.5f,   // [244..247]
         -36417.5f,      -12347.0f,       -4694.5f,       -1331.5f,   // [248..251]
          -1016.0f,         -55.5f,         -84.5f,          -7.0f,   // [252..255]
              1.0f,          29.0f,        -112.0f,         495.5f,   // [256..259]
           -707.0f,        3955.0f,         -35.0f,       25926.5f,   // [260..263]
         -36084.5f,      -11464.5f,       -4796.0f,       -1165.0f,   // [264..267]
          -1000.5f,         -36.0f,         -80.5f,          -6.5f,   // [268..271]
              1.0f,          29.0f,        -112.0f,         495.5f,   // [272..275]
           -707.0f,        3955.0f,         -35.0f,       25926.5f,   // [276..279]
         -36084.5f,      -11464.5f,       -4796.0f,       -1165.0f,   // [280..283]
          -1000.5f,         -36.0f,         -80.5f,          -6.5f,   // [284..287]
              1.0f,          31.5f,        -110.5f,         532.0f,   // [288..291]
           -640.0f,        4104.5f,         499.0f,       26767.0f,   // [292..295]
         -35710.0f,      -10594.5f,       -4875.0f,       -1003.0f,   // [296..299]
           -981.0f,         -18.0f,         -77.0f,          -5.5f,   // [300..303]
              1.0f,          31.5f,        -110.5f,         532.0f,   // [304..307]
           -640.0f,        4104.5f,         499.0f,       26767.0f,   // [308..311]
         -35710.0f,      -10594.5f,       -4875.0f,       -1003.0f,   // [312..315]
           -981.0f,         -18.0f,         -77.0f,          -5.5f,   // [316..319]
              1.0f,          34.0f,        -107.5f,         568.5f,   // [320..323]
           -565.5f,        4245.5f,        1061.0f,       27589.0f,   // [324..327]
         -35295.0f,       -9739.0f,       -4931.5f,        -846.0f,   // [328..331]
           -959.5f,          -1.0f,         -73.5f,          -5.0f,   // [332..335]
              1.0f,          34.0f,        -107.5f,         568.5f,   // [336..339]
           -565.5f,        4245.5f,        1061.0f,       27589.0f,   // [340..343]
         -35295.0f,       -9739.0f,       -4931.5f,        -846.0f,   // [344..347]
           -959.5f,          -1.0f,         -73.5f,          -5.0f,   // [348..351]
              1.5f,          36.5f,        -104.0f,         605.0f,   // [352..355]
           -485.0f,        4377.5f,        1650.0f,       28389.0f,   // [356..359]
         -34839.5f,       -8899.5f,       -4967.5f,        -694.0f,   // [360..363]
           -935.0f,          14.5f,         -69.5f,          -4.5f,   // [364..367]
              1.5f,          36.5f,        -104.0f,         605.0f,   // [368..371]
           -485.0f,        4377.5f,        1650.0f,       28389.0f,   // [372..375]
         -34839.5f,       -8899.5f,       -4967.5f,        -694.0f,   // [376..379]
           -935.0f,          14.5f,         -69.5f,          -4.5f,   // [380..383]
              1.5f,          39.5f,        -100.0f,         641.5f,   // [384..387]
           -397.0f,        4499.0f,        2266.5f,       29166.5f,   // [388..391]
         -34346.0f,       -8077.5f,       -4983.0f,        -547.5f,   // [392..395]
           -908.5f,          28.5f,         -66.0f,          -4.0f,   // [396..399]
              1.5f,          39.5f,        -100.0f,         641.5f,   // [400..403]
           -397.0f,        4499.0f,        2266.5f,       29166.5f,   // [404..407]
         -34346.0f,       -8077.5f,       -4983.0f,        -547.5f,   // [408..411]
           -908.5f,          28.5f,         -66.0f,          -4.0f,   // [412..415]
              2.0f,          42.5f,         -94.5f,         678.0f,   // [416..419]
           -302.5f,        4609.5f,        2909.0f,       29919.0f,   // [420..423]
         -33814.5f,       -7274.0f,       -4979.5f,        -407.0f,   // [424..427]
           -879.5f,          41.5f,         -62.5f,          -3.5f,   // [428..431]
              2.0f,          42.5f,         -94.5f,         678.0f,   // [432..435]
           -302.5f,        4609.5f,        2909.0f,       29919.0f,   // [436..439]
         -33814.5f,       -7274.0f,       -4979.5f,        -407.0f,   // [440..443]
           -879.5f,          41.5f,         -62.5f,          -3.5f,   // [444..447]
              2.0f,          45.5f,         -88.5f,         714.0f,   // [448..451]
           -201.0f,        4708.0f,        3577.0f,       30644.5f,   // [452..455]
         -33247.0f,       -6490.0f,       -4958.0f,        -272.5f,   // [456..459]
           -849.0f,          53.0f,         -58.5f,          -3.5f,   // [460..463]
              2.0f,          45.5f,         -88.5f,         714.0f,   // [464..467]
           -201.0f,        4708.0f,        3577.0f,       30644.5f,   // [468..471]
         -33247.0f,       -6490.0f,       -4958.0f,        -272.5f,   // [472..475]
           -849.0f,          53.0f,         -58.5f,          -3.5f,   // [476..479]
              2.5f,          48.5f,         -81.5f,         749.0f,   // [480..483]
            -92.5f,        4792.5f,        4270.0f,       31342.0f,   // [484..487]
         -32645.0f,       -5727.5f,       -4919.0f,        -144.0f,   // [488..491]
           -817.0f,          63.5f,         -55.5f,          -3.0f,   // [492..495]
              2.5f,          48.5f,         -81.5f,         749.0f,   // [496..499]
            -92.5f,        4792.5f,        4270.0f,       31342.0f,   // [500..503]
         -32645.0f,       -5727.5f,       -4919.0f,        -144.0f,   // [504..507]
           -817.0f,          63.5f,         -55.5f,          -3.0f,   // [508..511]
              2.5f,          52.0f,         -73.0f,         783.5f,   // [512..515]
             22.5f,        4863.5f,        4987.5f,       32009.5f,   // [516..519]
         -32009.5f,       -4987.5f,       -4863.5f,         -22.5f,   // [520..523]
           -783.5f,          73.0f,         -52.0f,          -2.5f,   // [524..527]
              2.5f,          52.0f,         -73.0f,         783.5f,   // [528..531]
             22.5f,        4863.5f,        4987.5f,       32009.5f,   // [532..535]
         -32009.5f,       -4987.5f,       -4863.5f,         -22.5f,   // [536..539]
           -783.5f,          73.0f,         -52.0f,          -2.5f   // [540..543]
};

// -------------------------------------------------------------------------------------
// Fast-DCT32 coefficients -- flt_8214BF40 .. flt_8214BFB8 (31 f32). sub_82B765E0 keeps
// 0x8214BFB8 in a base register and reaches exactly these 31 slots at displacements
// -0x78 .. 0, which is where the entry count comes from (not from the closed form).
// The dumped values do happen to round-trip 0.5/cos((2i+1)*pi/2N) for N = 32/16/8/4/2,
// but the DUMPED BYTES are what ships. Byte-identical copy at flt_82156FC0 (the
// rw::audio::core twins' table); the console gave this class its own physical copy, so
// per-TU file-statics are the faithful shape and the internal linkage means no ODR
// interaction with the wave-M copies.
// -------------------------------------------------------------------------------------
static const f32 KF_Dct32Cos64[16] = {   // stage 0, N=32
       0.50060302f,   0.505470932f,   0.515447319f,   0.531042576f,   // [0..3]
      0.553103924f,   0.582934976f,   0.622504115f,   0.674808323f,   // [4..7]
      0.744536281f,   0.839349627f,   0.972568214f,    1.16943991f,   // [8..11]
        1.4841646f,    2.05778098f,    3.40760851f,    10.1900082f    // [12..15]
};
static const f32 KF_Dct32Cos32[8] = {    // stage 1, N=16
      0.502419293f,   0.522498608f,   0.566944063f,   0.646821797f,   // [0..3]
      0.788154602f,    1.06067765f,    1.72244716f,    5.10114861f    // [4..7]
};
static const f32 KF_Dct32Cos16[4] = {    // stage 2, N=8
      0.509795606f,   0.601344883f,   0.899976194f,    2.56291556f    // [0..3]
};
static const f32 KF_Dct32Cos8[2] = {     // stage 3, N=4
      0.541196108f,    1.30656302f                                    // [0..1]
};
static const f32 KF_Dct32Cos4 = 0.707106769f;  // stage 4, N=2 -- 1/sqrt(2)

// Stage-indexed view of the five coefficient rows above. Source shape only: the X360
// build addressed the same 31 floats as one contiguous rodata run at flt_8214BF40.
static const f32* const KAP_Dct32Cos[5] =
{
    KF_Dct32Cos64, KF_Dct32Cos32, KF_Dct32Cos16, KF_Dct32Cos8, &KF_Dct32Cos4
};

// -------------------------------------------------------------------------------------
// Dct32Split -- one level of the classic Lee fast-DCT decomposition, in place over
// `apBlock[0 .. aiCount-1]`, using `apTemp` (aiCount floats) as the butterfly staging
// area:
//
//     s[i] = x[i] + x[n-1-i]                              i = 0 .. n/2-1
//     d[i] = (x[i] - x[n-1-i]) * KAP_Dct32Cos[stage][i]
//     S = fdct(s);  D = fdct(d)
//     y[2i] = S[i];  y[2i+1] = D[i] + D[i+1]              (D[n/2] taken as 0)
//
// STRUCTURAL MODEL, measured not assumed: sub_82B765E0 is 766 fully unrolled
// straight-line instructions with no loops and no calls; wave M symbolically executed
// the identical instruction stream of the twin sub_82B8E258 and recovered the 33 exact
// output expressions (scratchpad/waveM/probe_mpeg/symexec.out.txt), and this recursion
// reproduces them. The recursion is therefore a source-shape INFERENCE over MEASURED
// output expressions -- it evaluates the same sums in a different f32 rounding order
// than the console's unrolled schedule (see the file header for the measured error).
// -------------------------------------------------------------------------------------
void Dct32Split(f32* apBlock, s32 aiCount, s32 aiStage, f32* apTemp)
{
    if (aiCount < 2)
        return;

    const s32 liHalf = aiCount >> 1;
    const f32* const lpCos = KAP_Dct32Cos[aiStage];

    for (s32 i = 0; i < liHalf; ++i)
    {
        const f32 lfLow = apBlock[i];
        const f32 lfHigh = apBlock[aiCount - 1 - i];
        apTemp[i] = lfLow + lfHigh;
        apTemp[liHalf + i] = (lfLow - lfHigh) * lpCos[i];
    }
    for (s32 i = 0; i < aiCount; ++i)
        apBlock[i] = apTemp[i];

    Dct32Split(apBlock, liHalf, aiStage + 1, apTemp);
    Dct32Split(apBlock + liHalf, liHalf, aiStage + 1, apTemp);

    for (s32 i = 0; i < liHalf; ++i)
    {
        apTemp[2 * i] = apBlock[i];
        apTemp[2 * i + 1] = (i + 1 < liHalf) ? (apBlock[liHalf + i] + apBlock[liHalf + i + 1])
                                             : apBlock[liHalf + i];
    }
    for (s32 i = 0; i < aiCount; ++i)
        apBlock[i] = apTemp[i];
}

// -------------------------------------------------------------------------------------
// Dct32 -- sub_82B765E0. The 32-point MPEG matrixing DCT-II
//
//     dct[k] = sum(n = 0 .. 31) apInput[n] * cos((2n+1) * k * pi / 64)
//
// scattered into the two rotating poly-synth history columns the caller picked, with a
// 16-float row stride (float counts -- console and host agree):
//
//     apColumnA[16 * r] = dct[16 - r]   r = 0 .. 16   (17 stores)
//     apColumnB[16 * r] = dct[16 + r]   r = 0 .. 15   (16 stores; dct[16] lands in BOTH)
//
// `lafScratch` stands in for the console's per-TU zero-init .data scratch block
// flt_83277CC0 .. 0x83277DBC (64 f32; every xref to it comes from this helper -- dumped
// this wave, and the next .data object at 0x83277DC0 belongs to SNDVOICEI_alloc). It is
// written before it is read on every call (proved by wave M's symbolic execution of the
// identical instruction stream), so a function-local array is behaviour-identical -- and,
// unlike the .data block, re-entrant.
// -------------------------------------------------------------------------------------
void Dct32(f32* apColumnA, f32* apColumnB, const f32* apInput)
{
    f32 lafScratch[64]; // == flt_83277CC0 on the console

    for (s32 n = 0; n < 32; ++n)
        lafScratch[n] = apInput[n];

    Dct32Split(lafScratch, 32, 0, lafScratch + 32);

    for (s32 r = 0; r <= 16; ++r)
        apColumnA[16 * r] = lafScratch[16 - r];
    for (s32 r = 0; r < 16; ++r)
        apColumnB[16 * r] = lafScratch[16 + r];
}
} // namespace

// ----------------------------------------------------------------------------
// PolySynth @0x82B771D8
//
// One poly-phase synthesis stage for `aiChannel`: rotate that channel's history phase
// backwards, run the 32-point matrixing DCT into the two history columns the new phase
// selects, then window 32 PCM output samples out of the bank the odd column landed in.
//
// The history block is 2304 bytes (576 f32) per channel -- exactly what OpenSynth
// allocates -- laid out as two banks of 288 f32, each bank 18 rows of 16 rotating
// columns. Those are float counts, identical on the console and on x64.
//
// Index algebra taken straight from the asm (0x82B771EC .. 0x82B77250 for the column
// selection, then the three windowing loops at loc_82B77268 / 0x82B77340 /
// loc_82B773BC). The X360 leaves the DCT helper's return value in r3 at `blr`; no
// caller consumes it (the callers are Snd::CEALayer3::DecodeMono / DecodeStereo), so
// the header declares this void.
// ----------------------------------------------------------------------------
void CMpegBase::PolySynth(s32 aiChannel, f32* apOutSamples, const f32* apInSamples)
{
    // phase = (phase - 1) & 0xF, stored back before anything else reads it.
    // The asm addresses the phase byte as 0x38(this + channel), i.e. the two adjacent
    // u8 members mucSynthPhase0 / mucSynthPhase1 indexed by channel; selecting the
    // member by channel is the same thing for the only two channels that exist, and
    // does not depend on the host laying the two bytes out adjacently.
    u8& lucPhase = (aiChannel == 0) ? mucSynthPhase0 : mucSynthPhase1;

    const s32 liPhase = (static_cast<s32>(lucPhase) - 1) & 0xF;
    lucPhase = static_cast<u8>(liPhase);

    const s32 liOdd = liPhase & 1;
    const s32 liOddColumn = liPhase | 1; // asm: phase + (odd ^ 1) -- always odd, 1 .. 15

    // 576 f32 = 2304 bytes per channel (the asm's `mulli r4, r4, 0x900`). THE ONE
    // INSTRUCTION THAT DIFFERS FROM THE rw::audio::core TWIN: this class reads
    // mpPolySynthHistory (console +0x50); the twin reads its mpPolySynthWork (+0x4C).
    f32* const lpHistory = static_cast<f32*>(mpPolySynthHistory) + 576 * aiChannel;

    // The DCT writes one column of each bank: the even-phase bank gets the low half of
    // the spectrum reversed, the odd-phase bank the high half.
    f32* const lpColumnA = lpHistory + 288 * liOdd + ((liPhase + liOdd) & 0xF);
    f32* const lpColumnB = lpHistory + 288 * (liOdd ^ 1) + liOddColumn;

    Dct32(lpColumnA, lpColumnB, apInSamples);

    // Windowing always walks the bank the odd column just landed in.
    const f32* const lpBank = lpHistory + 288 * (liOdd ^ 1);
    const f32* const lpWindow = KF_SynthWindow + (16 - liOddColumn);

    // out[0 .. 15]: 16 taps per sample, alternating sign, window row stride 32.
    // (The asm evaluates the taps in the order t = 1, 0, 2, 3 .. 15 with fused
    // fmsubs/fmadds/fnmsubs; the sum is the same, only the f32 rounding order differs.)
    for (s32 j = 0; j < 16; ++j)
    {
        f32 lfAcc = 0.0f;
        for (s32 t = 0; t < 16; ++t)
        {
            const f32 lfTerm = lpWindow[32 * j + t] * lpBank[16 * j + t];
            lfAcc = ((t & 1) != 0) ? (lfAcc - lfTerm) : (lfAcc + lfTerm);
        }
        apOutSamples[j] = lfAcc;
    }

    // out[16]: the centre sample -- 8 even-indexed taps, all added.
    {
        f32 lfAcc = 0.0f;
        for (s32 u = 0; u < 8; ++u)
            lfAcc += lpWindow[512 + 2 * u] * lpBank[256 + 2 * u];
        apOutSamples[16] = lfAcc;
    }

    // out[17 .. 31]: the mirrored half -- window walked backwards from
    // KF_SynthWindow[496 + odd column] (the asm's `(po1 - 16) * 8` bytes past the row-16
    // window pointer), history walked backwards from row 15, whole accumulator negated.
    const f32* const lpWindowRev = KF_SynthWindow + 496 + liOddColumn;
    for (s32 k = 0; k < 15; ++k)
    {
        f32 lfAcc = lpWindowRev[-32 * k] * lpBank[240 - 16 * k + 15];
        for (s32 t = 0; t < 15; ++t)
            lfAcc += lpWindowRev[-32 * k - (t + 1)] * lpBank[240 - 16 * k + t];
        apOutSamples[17 + k] = -lfAcc;
    }
}

// ----------------------------------------------------------------------------
// Close @0x82B74CF0 (virtual, X360 vtable slot +0x0C)
//
// Release the poly-synth history through the paired Snd free hook and mark the stream
// shut. Always returns 0. Full disassembly (dumped this wave -- the address is absent
// from .ida-exports):
//
//   mflr r12; stw r12, var_8(r1); stwu r1, back_chain(r1)
//   mr     r11, r3
//   lbz    r10, 0x47(r11)              ; mucIsOpen
//   cmplwi r10, 0
//   beq    loc_82B74D2C
//   li     r9, 0
//   lwz    r3, 0x50(r11)               ; mpPolySynthHistory -> free arg
//   lis    r10, dword_83277834@ha
//   stw    r9, 0x4C(r11)               ; muHeaderWord = 0
//   stb    r9, 0x47(r11)               ; mucIsOpen = 0
//   lwz    r11, dword_83277834@l(r10)  ; the Snd free hook
//   mtctr  r11; bctrl                  ; free(history)
//   loc_82B74D2C: li r3, 0 ; ...       ; return 0
//
// MEASURED and deliberately preserved: the history pointer is loaded into the call
// argument BEFORE the two stores, and mpPolySynthHistory is NOT nulled afterwards --
// the member is left dangling. Only mucIsOpen (which OpenSynth/Open gate on) is
// cleared, so a re-Open reallocates and overwrites it.
// ----------------------------------------------------------------------------
s32 CMpegBase::Close()
{
    if (mucIsOpen)
    {
        void* const lpHistory = mpPolySynthHistory;
        muHeaderWord = 0;
        mucIsOpen = 0;
        dword_83277834(lpHistory);
    }

    return 0;
}

} // namespace Snd
