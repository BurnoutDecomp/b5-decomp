// =====================================================================================
// rw::audio::core::CMpegBase -- part-file 01 of the TU: the one remaining ledger body,
//
//   rw::audio::core::CMpegBase::PolySynth   @0x82B8EE50   (the poly-phase synthesis stage)
//   <file-static> Dct32                     @sub_82B8E258 (its 32-point matrixing helper)
//
// It lands in this part-file rather than in the committed CMpegBase.cpp because that file
// is owned by another worker this wave; the tables and Dct32 below therefore live in THIS
// translation unit's own anonymous namespace (internal linkage -- no ODR interaction with
// anything, and no second definition of PolySynth exists anywhere in the tree: grepped).
//
// PROVENANCE. No Feb-2007 source and no DecFIGS DWARF exist for this TU, so the X360
// PowerPC asm of BURNOUT_X360_ARTIST.XEX is the only authority, and everything below is
// MEASURED from it rather than reasoned:
//   * scratchpad/waveM/CMpegBase.spec.md sections 3-5 -- the recovered contract.
//   * scratchpad/waveM/probe_mpeg/0x82B8EE50.asm.txt and 0x82B8E258.asm.txt -- the raw
//     asm of both functions (re-walked instruction by instruction for this file).
//   * scratchpad/waveM/probe_mpeg/probe.txt -- the headless-IDA rodata dumps that the
//     KF_SynthWindow / KF_Dct32Cos* literals below are copied from (every literal
//     round-trips to the XEX dword; they were NOT retyped by hand).
//   * scratchpad/waveM/probe_mpeg/interp.py + interp_results.json -- the real asm executed
//     in a PPC interpreter for all 16 phases x 2 channels, and refcheck.py comparing it
//     against the reference model this body implements: all 32 outputs, the phase byte and
//     all 576 per-channel history floats matched in every case.
//
// OWNERSHIP (measured -- an older comment block in CMpegBase.cpp claims PolySynth is
// blocked because it "shares" an address with HELPER_CMpegBase; that claim is FALSE).
// There are three separate PolySynth bodies with three separate DCT helpers:
// rw::audio::core::CMpegBase @0x82B8EE50 / sub_82B8E258 (this one, window unk_82159D70),
// rw::audio::core::HELPER_CMpegBase @0x82B87080 / sub_82B86488 (window unk_82156740), and
// Snd::CMpegBase @0x82B771D8 / sub_82B765E0. sub_82B8E258 has exactly one code caller --
// this PolySynth (its other xref, 0x821E7738, is the .pdata unwind table, not a call) --
// which is why it is homed here as a file-static.
//
// HOST-vs-CONSOLE. The only console immediates reproduced here are 2304 / 576 / 288 / 16,
// and those are FLOAT-COUNT (or float-count x 4 byte) facts of the serialized poly-synth
// history layout, not pointer-width facts: f32 is 4 bytes on the X360 and on x64 alike,
// so they carry over unchanged. Every class member is reached by NAME (mucPolySynthPhase
// at console +0x38, mpPolySynthWork at console +0x4C) and never by a hardcoded offset.
// There is no floating-point compare anywhere in either function, so the PPC NaN-polarity
// trap (bge == bc 4,LT) does not arise -- both bodies are straight-line arithmetic plus
// counted loops (`addic.` / `bne`).
//
// NOT DONE HERE -- two defects this wave's spec (section 7) asks for that this worker is
// not permitted to land, because both live in already-committed files:
//   D1  GetBits(0) host divergence, in b5-decomp/vendor/renderware/src/rw/audio/core/
//       CMpegBase.cpp:106 and .../HELPER_CMpegBase.cpp (same code), plus a matching check
//       of b5-decomp/src/SDKs/EATech/include/snd/CMpegBase.cpp. `return uBits >> (32 -
//       iNumBits);` is 0 on the console (PPC `srw` uses a 6-bit shift count, so a shift of
//       32 clears the register) but UB on x64, where the count is masked to 0 and the whole
//       accumulator comes back. iNumBits == 0 is reachable in real MP3 data (a scalefactor
//       slen of 0), so this corrupts scalefactors on the host. Fix: `if (iNumBits == 0)
//       return 0;` at entry.
//   D2  The stale "PolySynth ... BLOCKED, NOT reconstructed" comment blocks -- CMpegBase.cpp
//       lines 13-20, HELPER_CMpegBase.cpp lines ~17-24, and the PolySynth doc-comments in
//       both headers. They are wrong as of this file and should be rewritten.
// =====================================================================================

#include "rw/audio/core/CMpegBase.h"

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// Poly-phase synthesis window -- unk_82159D70 in BURNOUT_X360_ARTIST.XEX (544 f32 = 0x880).
// ISO 11172-3 Table B.3 D[] x 32768, sign-folded for the alternating-sign window loops.
// Byte-identical second copy: unk_82156740 (HELPER TU). Index 0 is never read at runtime.
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

// Fast-DCT32 coefficients -- flt_82156FC0..flt_82157038 (31 f32), == 0.5/cos((2i+1)*pi/2N).
// One live copy in the XEX (both rw DCT helpers bind to it); a dead identical copy sits at
// 0x8215A5F0 in this TU's rodata block. Reconstruct as per-TU file-statics.
static const f32 KF_Dct32Cos64[16] = {   // N=32 stage: 0.5/cos((2i+1)*pi/64)
       0.50060302f,   0.505470932f,   0.515447319f,   0.531042576f,   // [0..3]
      0.553103924f,   0.582934976f,   0.622504115f,   0.674808323f,   // [4..7]
      0.744536281f,   0.839349627f,   0.972568214f,    1.16943991f,   // [8..11]
        1.4841646f,    2.05778098f,    3.40760851f,    10.1900082f   // [12..15]
};
static const f32 KF_Dct32Cos32[8] = {    // N=16 stage: 0.5/cos((2i+1)*pi/32)
      0.502419293f,   0.522498608f,   0.566944063f,   0.646821797f,   // [0..3]
      0.788154602f,    1.06067765f,    1.72244716f,    5.10114861f   // [4..7]
};
static const f32 KF_Dct32Cos16[4] = {    // N=8 stage: 0.5/cos((2i+1)*pi/16)
      0.509795606f,   0.601344883f,   0.899976194f,    2.56291556f   // [0..3]
};
static const f32 KF_Dct32Cos8[2] = {     // N=4 stage: 0.5/cos((2i+1)*pi/8)
      0.541196108f,    1.30656302f   // [0..1]
};
static const f32 KF_Dct32Cos4 = 0.707106769f;  // N=2 stage: 0.5/cos(pi/4) = 1/sqrt(2)

// Stage-indexed view of the five coefficient rows above: stage 0 is the 32-point split,
// stage 4 the final 2-point one. (Source shape only -- the X360 build addressed the same
// 31 floats as one contiguous rodata run at flt_82156FC0.)
static const f32 *const KAP_Dct32Cos[5] =
{
    KF_Dct32Cos64, KF_Dct32Cos32, KF_Dct32Cos16, KF_Dct32Cos8, &KF_Dct32Cos4
};

// -------------------------------------------------------------------------------------
// Dct32Split -- one level of the classic Lee fast-DCT decomposition, in place over
// `apBlock[0 .. iCount-1]` using `apTemp` (iCount floats) as the butterfly staging area:
//
//     s[i] = x[i] + x[n-1-i]                              i = 0 .. n/2-1
//     d[i] = (x[i] - x[n-1-i]) * KAP_Dct32Cos[stage][i]
//     S = fdct(s);  D = fdct(d)
//     y[2i] = S[i];  y[2i+1] = D[i] + D[i+1]              (D[n/2] taken as 0)
//
// MEASURED, not assumed: this is the structure symbolic execution of sub_82B8E258
// recovered (scratchpad/waveM/probe_mpeg/symexec.out.txt holds the 33 exact output
// expressions, in terms of the same c32/c16/c8/c4 rows), and the composition reproduces
// the direct O(N^2) transform below to ~1e-12 in double precision.
// -------------------------------------------------------------------------------------
void Dct32Split(f32 *apBlock, s32 iCount, s32 iStage, f32 *apTemp)
{
    if (iCount < 2)
        return;

    const s32 iHalf = iCount >> 1;
    const f32 *const pCos = KAP_Dct32Cos[iStage];

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
        apTemp[2 * i + 1] = (i + 1 < iHalf) ? (apBlock[iHalf + i] + apBlock[iHalf + i + 1])
                                            : apBlock[iHalf + i];
    }
    for (s32 i = 0; i < iCount; ++i)
        apBlock[i] = apTemp[i];
}

// -------------------------------------------------------------------------------------
// Dct32 -- sub_82B8E258. The 32-point MPEG matrixing DCT-II
//
//     dct[k] = sum(n = 0 .. 31) apInput[n] * cos((2n+1) * k * pi / 64)
//
// scattered into the two rotating poly-synth history columns the caller picked, with a
// 16-float row stride:
//
//     apColumnA[16 * r] = dct[16 - r]   r = 0 .. 16   (17 stores)
//     apColumnB[16 * r] = dct[16 + r]   r = 0 .. 15   (16 stores; dct[16] lands in BOTH)
//
// Both the transform and the scatter are MEASURED: every one of the helper's 766
// straight-line instructions was symbolically executed, and the resulting 33 store
// expressions are the file named in Dct32Split's comment.
//
// `lafScratch` stands in for the X360 build's per-TU zero-init .data scratch block
// flt_8327A428 .. flt_8327A524 (64 f32; the HELPER TU's copy of this helper owns the
// adjacent flt_8327A328 block). It is written before it is read on every call, so a
// function-local array is behaviour-identical -- and, unlike the .data block, re-entrant.
// -------------------------------------------------------------------------------------
void Dct32(f32 *apColumnA, f32 *apColumnB, const f32 *apInput)
{
    f32 lafScratch[64]; // == flt_8327A428 on the console

    for (s32 n = 0; n < 32; ++n)
        lafScratch[n] = apInput[n];

    Dct32Split(lafScratch, 32, 0, lafScratch + 32);

    for (s32 r = 0; r <= 16; ++r)
        apColumnA[16 * r] = lafScratch[16 - r];
    for (s32 r = 0; r < 16; ++r)
        apColumnB[16 * r] = lafScratch[16 + r];
}
} // namespace

// -------------------------------------------------------------------------------------
// PolySynth @0x82B8EE50
//
// One poly-phase synthesis stage for `iChannel`: rotate that channel's history phase
// backwards, run the 32-point matrixing DCT into the two history columns the new phase
// selects, then window 32 PCM output samples out of the odd-phase history bank.
//
// The history block is 2304 bytes (576 f32) per channel = two banks of 288 f32, each bank
// 18 rows of 16 rotating columns; the derived decoders (Layer3Dec::Decode,
// EALayer3Core::Decode) arm mpPolySynthWork from mpPolySynthHistory before each pass.
// Those counts are float counts, identical on the console and on x64.
//
// Index algebra below is taken straight from the asm (0x82B8EE64 .. 0x82B8EEC8 for the
// column selection, then the three windowing loops at loc_82B8EEE0 / 0x82B8EFB8 /
// loc_82B8F034) and was confirmed against the PPC-interpreter run for all 16 phases.
// -------------------------------------------------------------------------------------
void CMpegBase::PolySynth(s32 iChannel, f32 *pOutSamples, const f32 *pInSamples)
{
    // phase = (phase - 1) & 0xF, stored back before anything else reads it.
    const s32 iPhase = (static_cast<s32>(mucPolySynthPhase[iChannel]) - 1) & 0xF;
    mucPolySynthPhase[iChannel] = static_cast<u8>(iPhase);

    const s32 iOdd = iPhase & 1;
    const s32 iOddColumn = iPhase | 1; // asm: phase + (odd ^ 1) -- always odd, 1 .. 15

    // 576 f32 = 2304 bytes per channel (the asm's `mulli r4, r4, 0x900`).
    f32 *const pHistory = static_cast<f32 *>(mpPolySynthWork) + 576 * iChannel;

    // The DCT writes one column of each bank: the even-phase bank gets the low half of the
    // spectrum reversed, the odd-phase bank the high half.
    f32 *const pColumnA = pHistory + 288 * iOdd + ((iPhase + iOdd) & 0xF);
    f32 *const pColumnB = pHistory + 288 * (iOdd ^ 1) + iOddColumn;

    Dct32(pColumnA, pColumnB, pInSamples);

    // Windowing always walks the bank the odd column just landed in.
    const f32 *const pBank = pHistory + 288 * (iOdd ^ 1);
    const f32 *const pWindow = KF_SynthWindow + (16 - iOddColumn);

    // out[0 .. 15]: 16 taps per sample, alternating sign, window row stride 32.
    // (The asm evaluates the taps in the order t = 1, 0, 2, 3 .. 15 with fused
    // fmsubs/fmadds/fnmsubs; the sum is the same, only the f32 rounding order differs.)
    for (s32 j = 0; j < 16; ++j)
    {
        f32 lfAcc = 0.0f;
        for (s32 t = 0; t < 16; ++t)
        {
            const f32 lfTerm = pWindow[32 * j + t] * pBank[16 * j + t];
            lfAcc = ((t & 1) != 0) ? (lfAcc - lfTerm) : (lfAcc + lfTerm);
        }
        pOutSamples[j] = lfAcc;
    }

    // out[16]: the centre sample -- 8 even-indexed taps, all added.
    {
        f32 lfAcc = 0.0f;
        for (s32 u = 0; u < 8; ++u)
            lfAcc += pWindow[512 + 2 * u] * pBank[256 + 2 * u];
        pOutSamples[16] = lfAcc;
    }

    // out[17 .. 31]: the mirrored half -- window walked backwards from KF_SynthWindow[496 +
    // odd column] (`(po1 - 16) * 8` bytes past the row-16 window pointer in the asm), history
    // walked backwards from row 15, whole accumulator negated.
    const f32 *const pWindowRev = KF_SynthWindow + 496 + iOddColumn;
    for (s32 k = 0; k < 15; ++k)
    {
        f32 lfAcc = pWindowRev[-32 * k] * pBank[240 - 16 * k + 15];
        for (s32 t = 0; t < 15; ++t)
            lfAcc += pWindowRev[-32 * k - (t + 1)] * pBank[240 - 16 * k + t];
        pOutSamples[17 + k] = -lfAcc;
    }
}

} // namespace core
} // namespace audio
} // namespace rw
