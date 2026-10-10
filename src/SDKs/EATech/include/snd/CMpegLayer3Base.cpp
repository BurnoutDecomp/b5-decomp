// ============================================================================
// SDKs/EATech/include/snd/CMpegLayer3Base.cpp
//
// Snd::CMpegLayer3Base -- the MPEG Layer III spectral stages (see the header).
//
//   ~CMpegLayer3Base, Dequantize, Stereo, Reorder, AntiAlias, Hybrid,
//   i_stereo_k_values, and the file-static kernels they call:
//   scalesamples (Dequantize), imdct36 / imdct12 / overlapadd (Hybrid).
//
// Reconstructed from the console assembly. Every table is dumped from the image
// (big-endian), never retyped from the ISO tables: the image's scale-factor-band
// rows, IMDCT windows and alias coefficients are its own variants. The IMDCT
// kernels are straight-line butterfly networks; they are written out in the
// console's operation order with its literal constants. The console fuses
// multiply-adds; the host evaluates them as separate products.
// ============================================================================

#include "SDKs/EATech/include/snd/CMpegLayer3Base.h"

#include <cstring> // std::memset

namespace Snd
{

// The Snd system's memory callbacks: the allocator / free function pair every
// Snd decoder allocates through (CMpegBase::OpenSynth and CMpegBase::Close call
// the same pair). Installed by the Snd system start-up.
extern void* (*gpfnSndAlloc)(u32 auSize);
extern void  (*gpfnSndFree)(void* apBlock);

namespace
{

// Spectral lines per channel per granule, and per sub-band.
const s32 KI_GRANULE_LINES = 576;
const s32 KI_SUBBAND_LINES = 18;

// Scale-factor band boundaries for the nine sample-rate rows: 23 long-block
// boundaries (lines) and 14 short-block boundaries (lines per window).
struct SfBandTable
{
    s16 masLong[23];
    u8  maucShort[14];
};

const SfBandTable kaSfBands[9] =
{
    { { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576 }, { 0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192 } },
    { { 0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576 }, { 0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192 } },
    { { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576 }, { 0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192 } },
    { { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 }, { 0, 4, 8, 12, 18, 24, 32, 42, 56, 74, 100, 132, 174, 192 } },
    { { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 330, 394, 464, 540, 576 }, { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 136, 180, 192 } },
    { { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 }, { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192 } },
    { { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 580 }, { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 164, 194 } },
    { { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 580 }, { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 164, 194 } },
    { { 0, 12, 24, 36, 48, 60, 72, 88, 108, 132, 160, 192, 232, 280, 336, 400, 476, 566, 568, 570, 572, 574, 576 }, { 0, 8, 16, 24, 36, 52, 72, 96, 124, 160, 162, 164, 166, 168 } },
};

// Pure-short-block band widths of short bands 3..12 per sample-rate row (the
// first three short bands are handled as four lines wide).
const u8 kauShortWidth[9][10] =
{
    { 4, 6, 8, 10, 12, 14, 18, 22, 30, 56 },
    { 4, 6, 6, 10, 12, 14, 16, 20, 26, 66 },
    { 4, 6, 8, 12, 16, 20, 26, 34, 42, 12 },
    { 6, 6, 8, 10, 14, 18, 26, 32, 42, 18 },
    { 6, 8, 10, 12, 14, 18, 24, 32, 44, 12 },
    { 6, 8, 10, 12, 14, 18, 24, 30, 40, 18 },
    { 6, 6, 8, 10, 14, 18, 26, 32, 42, 18 },
    { 6, 8, 10, 12, 14, 18, 24, 32, 44, 12 },
    { 6, 8, 10, 12, 14, 18, 24, 30, 40, 18 },
};

// Pre-emphasis added to the long-block scale factors when preflag is set.
const s8 kacPretab[22] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0,
};

// Dequantisation gains 2^((45 - i) / 4), i = 0..255; Dequantize indexes them
// through a pointer to the unit entry (i = 45), two entries per scale-factor step.
const f32 kafGainTable[256] =
{
    2435.496f, 2048.0f, 1722.1559f, 1448.1547f, 1217.748f, 1024.0f, 861.07794f, 724.07733f,
    608.874f, 512.0f, 430.53897f, 362.03867f, 304.437f, 256.0f, 215.26949f, 181.01933f,
    152.2185f, 128.0f, 107.63474f, 90.50967f, 76.10925f, 64.0f, 53.81737f, 45.254833f,
    38.054626f, 32.0f, 26.908686f, 22.627417f, 19.027313f, 16.0f, 13.454343f, 11.313708f,
    9.513657f, 8.0f, 6.7271714f, 5.656854f, 4.7568283f, 4.0f, 3.3635857f, 2.828427f,
    2.3784142f, 2.0f, 1.6817929f, 1.4142135f, 1.1892071f, 1.0f, 0.8408964f, 0.70710677f,
    0.59460354f, 0.5f, 0.4204482f, 0.35355338f, 0.29730177f, 0.25f, 0.2102241f, 0.17677669f,
    0.14865088f, 0.125f, 0.10511205f, 0.088388346f, 0.07432544f, 0.0625f, 0.052556027f, 0.044194173f,
    0.03716272f, 0.03125f, 0.026278013f, 0.022097087f, 0.01858136f, 0.015625f, 0.013139007f, 0.011048543f,
    0.00929068f, 0.0078125f, 0.0065695033f, 0.0055242716f, 0.00464534f, 0.00390625f, 0.0032847517f, 0.0027621358f,
    0.00232267f, 0.001953125f, 0.0016423758f, 0.0013810679f, 0.001161335f, 0.0009765625f, 0.0008211879f, 0.00069053395f,
    0.0005806675f, 0.00048828125f, 0.00041059396f, 0.00034526698f, 0.00029033376f, 0.00024414062f, 0.00020529698f, 0.00017263349f,
    0.00014516688f, 0.00012207031f, 0.00010264849f, 8.6316744e-05f, 7.258344e-05f, 6.1035156e-05f, 5.1324245e-05f, 4.3158372e-05f,
    3.629172e-05f, 3.0517578e-05f, 2.5662122e-05f, 2.1579186e-05f, 1.814586e-05f, 1.5258789e-05f, 1.2831061e-05f, 1.0789593e-05f,
    9.07293e-06f, 7.6293945e-06f, 6.4155306e-06f, 5.3947965e-06f, 4.536465e-06f, 3.8146973e-06f, 3.2077653e-06f, 2.6973983e-06f,
    2.2682325e-06f, 1.9073486e-06f, 1.6038827e-06f, 1.3486991e-06f, 1.1341162e-06f, 9.536743e-07f, 8.019413e-07f, 6.7434956e-07f,
    5.670581e-07f, 4.7683716e-07f, 4.0097066e-07f, 3.3717478e-07f, 2.8352906e-07f, 2.3841858e-07f, 2.0048533e-07f, 1.6858739e-07f,
    1.4176453e-07f, 1.1920929e-07f, 1.00242666e-07f, 8.4293696e-08f, 7.0882265e-08f, 5.9604645e-08f, 5.0121333e-08f, 4.2146848e-08f,
    3.5441133e-08f, 2.9802322e-08f, 2.5060666e-08f, 2.1073424e-08f, 1.7720566e-08f, 1.4901161e-08f, 1.2530333e-08f, 1.0536712e-08f,
    8.860283e-09f, 7.450581e-09f, 6.2651666e-09f, 5.268356e-09f, 4.4301416e-09f, 3.7252903e-09f, 3.1325833e-09f, 2.634178e-09f,
    2.2150708e-09f, 1.8626451e-09f, 1.5662917e-09f, 1.317089e-09f, 1.1075354e-09f, 9.313226e-10f, 7.831458e-10f, 6.585445e-10f,
    5.537677e-10f, 4.656613e-10f, 3.915729e-10f, 3.2927225e-10f, 2.7688385e-10f, 2.3283064e-10f, 1.9578646e-10f, 1.6463612e-10f,
    1.3844192e-10f, 1.1641532e-10f, 9.789323e-11f, 8.231806e-11f, 6.922096e-11f, 5.820766e-11f, 4.8946614e-11f, 4.115903e-11f,
    3.461048e-11f, 2.910383e-11f, 2.4473307e-11f, 2.0579516e-11f, 1.730524e-11f, 1.4551915e-11f, 1.22366535e-11f, 1.0289758e-11f,
    8.65262e-12f, 7.275958e-12f, 6.1183268e-12f, 5.144879e-12f, 4.32631e-12f, 3.637979e-12f, 3.0591634e-12f, 2.5724394e-12f,
    2.163155e-12f, 1.8189894e-12f, 1.5295817e-12f, 1.2862197e-12f, 1.0815775e-12f, 9.094947e-13f, 7.6479085e-13f, 6.4310986e-13f,
    5.4078877e-13f, 4.5474735e-13f, 3.8239542e-13f, 3.2155493e-13f, 2.7039438e-13f, 2.2737368e-13f, 1.9119771e-13f, 1.6077747e-13f,
    1.3519719e-13f, 1.1368684e-13f, 9.5598856e-14f, 8.038873e-14f, 6.7598596e-14f, 5.684342e-14f, 4.7799428e-14f, 4.0194366e-14f,
    3.3799298e-14f, 2.842171e-14f, 2.3899714e-14f, 2.0097183e-14f, 1.6899649e-14f, 1.4210855e-14f, 1.1949857e-14f, 1.00485916e-14f,
    8.4498245e-15f, 7.1054274e-15f, 5.9749285e-15f, 5.0242958e-15f, 4.2249122e-15f, 3.5527137e-15f, 2.9874642e-15f, 2.5121479e-15f,
    2.1124561e-15f, 1.7763568e-15f, 1.4937321e-15f, 1.2560739e-15f, 1.0562281e-15f, 8.881784e-16f, 7.4686606e-16f, 6.2803697e-16f,
    5.2811403e-16f, 4.440892e-16f, 3.7343303e-16f, 3.1401849e-16f, 2.6405702e-16f, 2.220446e-16f, 1.8671652e-16f, 1.5700924e-16f,
};

const f32* spGainTable = &kafGainTable[45];

// MPEG-1 intensity-stereo ratios tan(is_pos * pi / 12), as dumped.
const f32 kafIsRatio[16] =
{
    0.0f, 0.2679492f, 0.57735026f, 1.0f, 1.7320508f, 3.732051f, 1e+11f, -3.732051f,
    -1.7320508f, -1.0f, -0.57735026f, -0.2679492f, 0.0f, 0.2679492f, 0.57735026f, 1.0f,
};

// LSF intensity-stereo scales: row 0 = 2^(-n/4), row 1 = 2^(-n/2).
const f32 kafIsLsf[2][32] =
{
    {
        1.0f, 0.8408964f, 0.70710677f, 0.59460354f, 0.5f, 0.4204482f, 0.35355338f, 0.29730177f,
        0.25f, 0.2102241f, 0.17677669f, 0.14865088f, 0.125f, 0.10511205f, 0.088388346f, 0.07432544f,
        0.0625f, 0.052556027f, 0.044194173f, 0.03716272f, 0.03125f, 0.026278013f, 0.022097087f, 0.01858136f,
        0.015625f, 0.013139007f, 0.011048543f, 0.00929068f, 0.0078125f, 0.0065695033f, 0.0055242716f, 0.00464534f,
    },
    {
        1.0f, 0.70710677f, 0.5f, 0.35355338f, 0.25f, 0.17677669f, 0.125f, 0.088388346f,
        0.0625f, 0.044194173f, 0.03125f, 0.022097087f, 0.015625f, 0.011048543f, 0.0078125f, 0.0055242716f,
        0.00390625f, 0.0027621358f, 0.001953125f, 0.0013810679f, 0.0009765625f, 0.00069053395f, 0.00048828125f, 0.00034526698f,
        0.00024414062f, 0.00017263349f, 0.00012207031f, 8.6316744e-05f, 6.1035156e-05f, 4.3158372e-05f, 3.0517578e-05f, 2.1579186e-05f,
    },
};

// Alias-reduction butterfly coefficients (cs, ca).
const f32 kafAliasCs[8] =
{
    0.8574929f, 0.881742f, 0.94962865f, 0.9833146f, 0.9955178f, 0.9991606f, 0.9998992f, 0.99999315f,
};
const f32 kafAliasCa[8] =
{
    -0.51449573f, -0.47173196f, -0.31337744f, -0.1819132f, -0.09457419f, -0.040965583f, -0.014198569f, -0.0036999746f,
};

// IMDCT window per block type (0 normal, 1 start, 2 short, 3 stop).
const f32 kafImdctWindow[4][36] =
{
    {
        0.016141215f, 0.05360318f, 0.100707136f, 0.16280818f, 0.5f, 0.38388735f,
        0.6206114f, 1.1659756f, 3.8720753f, -4.225629f, -1.519529f, -0.97416484f,
        -0.73744076f, -1.2071068f, -0.5163616f, -0.45426053f, -0.40715656f, -0.3696946f,
        -0.3387627f, -0.31242222f, -0.28939587f, -0.26880082f, -0.5f, -0.23251417f,
        -0.21596715f, -0.20004979f, -0.18449493f, -0.16905846f, -0.15350361f, -0.13758625f,
        -0.12103922f, -0.20710678f, -0.084752575f, -0.06415752f, -0.041131172f, -0.014790705f,
    },
    {
        0.016141215f, 0.05360318f, 0.100707136f, 0.16280818f, 0.5f, 0.38388735f,
        0.6206114f, 1.1659756f, 3.8720753f, -4.225629f, -1.519529f, -0.97416484f,
        -0.73744076f, -1.2071068f, -0.5163616f, -0.45426053f, -0.40715656f, -0.3696946f,
        -0.33908543f, -0.3151181f, -0.29642227f, -0.28184548f, -0.5411961f, -0.2621323f,
        -0.25387916f, -0.2329629f, -0.19852729f, -0.15233535f, -0.0964964f, -0.03342383f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    },
    {
        0.0483008f, 0.15715657f, 0.28325045f, 0.42953748f, 1.2071068f, 0.8242648f,
        1.1451749f, 1.769529f, 4.5470223f, -3.489053f, -0.7329629f, -0.15076515f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    },
    {
        -0.0f, -0.0f, -0.0f, -0.0f, -0.0f, -0.0f,
        0.15076514f, 0.7329629f, 3.489053f, -4.5470223f, -1.769529f, -1.1451749f,
        -0.8313774f, -1.306563f, -0.54142016f, -0.46528974f, -0.4106699f, -0.3700468f,
        -0.3387627f, -0.31242222f, -0.28939587f, -0.26880082f, -0.5f, -0.23251417f,
        -0.21596715f, -0.20004979f, -0.18449493f, -0.16905846f, -0.15350361f, -0.13758625f,
        -0.12103922f, -0.20710678f, -0.084752575f, -0.06415752f, -0.041131172f, -0.014790705f,
    },
};

const f32 KF_SQRT_HALF = 0.70710677f;

// Multiply aiCount samples by afGain in place. The console peels one leading pair
// when the buffer is not 16-byte aligned, then runs 16- and 2-sample strides; the
// bands it is handed are always an even number of lines wide, so every line of
// the band is scaled.
void scalesamples(f32* apSamples, f32 afGain, s32 aiCount)
{
    for (s32 li = 0; li + 1 < aiCount; li += 2)
    {
        apSamples[li]     = apSamples[li] * afGain;
        apSamples[li + 1] = apSamples[li + 1] * afGain;
    }
}

// 36-point IMDCT of one sub-band (18 lines, stride 4 in the four-sub-band
// interleave), windowed by apWin, written to apOut (36 outputs, stride 4). The
// input is first prefix-summed in place (in[k] += in[k-1], then odd in[k] +=
// in[k-2]), as the butterfly network expects.
void imdct36(f32* apIn, f32* apOut, const f32* apWin)
{
    for (s32 li = 17; li >= 1; --li)
        apIn[li * 4] = apIn[(li - 1) * 4] + apIn[li * 4];
    for (s32 li = 17; li >= 3; li -= 2)
        apIn[li * 4] = apIn[(li - 2) * 4] + apIn[li * 4];

    const f32 lfT1 = apIn[0 * 4] * 2.0f;
    const f32 lfT2 = apIn[12 * 4] + lfT1;
    const f32 lfT3 = apIn[6 * 4] * 1.7320508f;
    const f32 lfT4 = apIn[1 * 4] * 2.0f;
    const f32 lfT5 = ((apIn[2 * 4] - apIn[10 * 4]) - apIn[14 * 4]) * 1.7320508f;
    const f32 lfT6 = (((apIn[0 * 4] - apIn[4 * 4]) + apIn[8 * 4]) - apIn[12 * 4]) + apIn[16 * 4];
    const f32 lfT7 = apIn[14 * 4] * 0.6840403f + (apIn[10 * 4] * 1.2855753f + (apIn[2 * 4] * 1.9696155f + lfT3));
    const f32 lfT8 = apIn[14 * 4] * 1.9696155f + ((apIn[2 * 4] * 1.2855753f - lfT3) - apIn[10 * 4] * 0.6840403f);
    const f32 lfT9 = (apIn[10 * 4] * 1.9696155f + (apIn[2 * 4] * 0.6840403f - lfT3)) - apIn[14 * 4] * 1.2855753f;
    const f32 lfT10 = (apIn[16 * 4] * 0.34729636f + (apIn[4 * 4] * 1.8793852f + (apIn[8 * 4] * 1.5320889f))) + lfT2;
    const f32 lfT11 = ((((apIn[4 * 4] + lfT1) - apIn[8 * 4]) - apIn[12 * 4]) - apIn[12 * 4]) - apIn[16 * 4];
    const f32 lfT12 = apIn[16 * 4] * 1.5320889f + ((lfT2 - apIn[4 * 4] * 0.34729636f) - apIn[8 * 4] * 1.8793852f);
    const f32 lfT13 = (apIn[8 * 4] * 0.34729636f + (lfT2 - apIn[4 * 4] * 1.5320889f)) - apIn[16 * 4] * 1.8793852f;
    const f32 lfT14 = apIn[7 * 4] * 1.7320508f;
    const f32 lfT15 = apIn[13 * 4] + lfT4;
    const f32 lfT16 = apIn[15 * 4] * 0.6840403f + (apIn[11 * 4] * 1.2855753f + (apIn[3 * 4] * 1.9696155f + lfT14));
    const f32 lfT17 = ((apIn[3 * 4] - apIn[11 * 4]) - apIn[15 * 4]) * 1.7320508f;
    const f32 lfT18 = apIn[15 * 4] * 1.9696155f + ((apIn[3 * 4] * 1.2855753f - lfT14) - apIn[11 * 4] * 0.6840403f);
    const f32 lfT19 = ((((apIn[5 * 4] + lfT4) - apIn[9 * 4]) - apIn[13 * 4]) - apIn[13 * 4]) - apIn[17 * 4];
    const f32 lfT20 = ((((apIn[1 * 4] - apIn[5 * 4]) + apIn[9 * 4]) - apIn[13 * 4]) + apIn[17 * 4]) * 0.70710677f;
    const f32 lfT21 = (apIn[17 * 4] * 0.34729636f + (apIn[5 * 4] * 1.8793852f + (apIn[9 * 4] * 1.5320889f))) + lfT15;
    const f32 lfT22 = apIn[17 * 4] * 1.5320889f + ((lfT15 - apIn[5 * 4] * 0.34729636f) - apIn[9 * 4] * 1.8793852f);
    const f32 lfT23 = (apIn[9 * 4] * 0.34729636f + (lfT15 - apIn[5 * 4] * 1.5320889f)) - apIn[17 * 4] * 1.8793852f;
    const f32 lfT24 = lfT7 + lfT10;
    const f32 lfT25 = lfT6 - lfT20;
    const f32 lfT26 = lfT20 + lfT6;
    const f32 lfT27 = (lfT16 + lfT21) * 0.5019099f;
    const f32 lfT28 = (apIn[11 * 4] * 1.9696155f + (apIn[3 * 4] * 0.6840403f - lfT14)) - apIn[15 * 4] * 1.2855753f;
    const f32 lfT29 = (lfT17 + lfT19) * 0.5176381f;
    const f32 lfT30 = lfT24 - lfT27;
    const f32 lfT31 = lfT27 + lfT24;
    const f32 lfT32 = lfT5 + lfT11;
    const f32 lfT33 = lfT8 + lfT12;
    const f32 lfT34 = lfT32 - lfT29;
    const f32 lfT35 = lfT29 + lfT32;
    const f32 lfT36 = (lfT18 + lfT22) * 0.55168897f;
    const f32 lfT37 = (lfT28 + lfT23) * 0.61038727f;
    const f32 lfT38 = lfT33 - lfT36;
    const f32 lfT39 = lfT36 + lfT33;
    const f32 lfT40 = lfT9 + lfT13;
    const f32 lfT41 = (lfT23 - lfT28) * 0.8717234f;
    const f32 lfT42 = (lfT22 - lfT18) * 1.1831008f;
    const f32 lfT43 = lfT40 - lfT37;
    const f32 lfT44 = lfT37 + lfT40;
    const f32 lfT45 = lfT13 - lfT9;
    const f32 lfT46 = (lfT19 - lfT17) * 1.9318516f;
    const f32 lfT47 = (lfT21 - lfT16) * 5.7368565f;
    const f32 lfT48 = lfT45 - lfT41;
    const f32 lfT49 = lfT41 + lfT45;
    const f32 lfT50 = lfT12 - lfT8;
    const f32 lfT51 = lfT50 - lfT42;
    const f32 lfT52 = lfT42 + lfT50;
    const f32 lfT53 = lfT11 - lfT5;
    const f32 lfT54 = lfT53 - lfT46;
    const f32 lfT55 = lfT46 + lfT53;
    const f32 lfT56 = lfT10 - lfT7;
    const f32 lfT57 = lfT56 - lfT47;
    const f32 lfT58 = lfT47 + lfT56;
    apOut[0 * 4] = lfT57 * apWin[0];
    apOut[1 * 4] = apWin[1] * lfT54;
    apOut[2 * 4] = apWin[2] * lfT51;
    apOut[3 * 4] = apWin[3] * lfT48;
    apOut[4 * 4] = apWin[4] * lfT25;
    apOut[5 * 4] = apWin[5] * lfT43;
    apOut[6 * 4] = apWin[6] * lfT38;
    apOut[7 * 4] = apWin[7] * lfT34;
    apOut[8 * 4] = apWin[8] * lfT30;
    apOut[9 * 4] = apWin[9] * lfT30;
    apOut[10 * 4] = apWin[10] * lfT34;
    apOut[11 * 4] = apWin[11] * lfT38;
    apOut[12 * 4] = apWin[12] * lfT43;
    apOut[13 * 4] = apWin[13] * lfT25;
    apOut[14 * 4] = apWin[14] * lfT48;
    apOut[15 * 4] = apWin[15] * lfT51;
    apOut[16 * 4] = apWin[16] * lfT54;
    apOut[17 * 4] = apWin[17] * lfT57;
    apOut[18 * 4] = apWin[18] * lfT58;
    apOut[19 * 4] = apWin[19] * lfT55;
    apOut[20 * 4] = apWin[20] * lfT52;
    apOut[21 * 4] = apWin[21] * lfT49;
    apOut[22 * 4] = apWin[22] * lfT26;
    apOut[23 * 4] = apWin[23] * lfT44;
    apOut[24 * 4] = apWin[24] * lfT39;
    apOut[25 * 4] = apWin[25] * lfT35;
    apOut[26 * 4] = apWin[26] * lfT31;
    apOut[27 * 4] = apWin[27] * lfT31;
    apOut[28 * 4] = apWin[28] * lfT35;
    apOut[29 * 4] = apWin[29] * lfT39;
    apOut[30 * 4] = apWin[30] * lfT44;
    apOut[31 * 4] = apWin[31] * lfT26;
    apOut[32 * 4] = apWin[32] * lfT49;
    apOut[33 * 4] = apWin[33] * lfT52;
    apOut[34 * 4] = apWin[34] * lfT55;
    apOut[35 * 4] = apWin[35] * lfT58;
}

// Three 12-point IMDCTs of one short-block sub-band (6 lines per window, the
// windows interleaved line by line, stride 4), each windowed and overlap-added
// into apOut at 6 + 6 * window (36 outputs, stride 4, cleared first).
void imdct12(f32* apIn, f32* apOut)
{
    for (s32 li = 0; li < 36; ++li)
        apOut[li * 4] = 0.0f;

    for (s32 liWin = 0; liWin < 3; ++liWin)
    {
        f32* lpIn = &apIn[liWin * 4];

        // In-place prefix sums over this window's six lines (lpIn[3k * 4]).
        for (s32 li = 5; li >= 1; --li)
            lpIn[li * 12] = lpIn[(li - 1) * 12] + lpIn[li * 12];
        lpIn[15 * 4] = lpIn[9 * 4] + lpIn[15 * 4];
        lpIn[9 * 4]  = lpIn[3 * 4] + lpIn[9 * 4];

        f32 lafOut[12];
        const f32 lfT1 = lpIn[6 * 4] * 0.8660254f;
        const f32 lfT2 = lpIn[0 * 4] - lpIn[12 * 4];
        const f32 lfT3 = lpIn[12 * 4] * 0.5f + lpIn[0 * 4];
        const f32 lfT4 = lfT3 + lfT1;
        const f32 lfT5 = lfT3 - lfT1;
        const f32 lfT6 = lpIn[15 * 4] * 0.5f + lpIn[3 * 4];
        const f32 lfT7 = lpIn[9 * 4] * 0.8660254f;
        const f32 lfT8 = (lpIn[3 * 4] - lpIn[15 * 4]) * 0.70710677f;
        const f32 lfT9 = (lfT6 + lfT7) * 0.5176381f;
        const f32 lfT10 = (lfT6 - lfT7) * 1.9318516f;
        const f32 lfT11 = (lfT9 + lfT4) * 0.5043145f;
        const f32 lfT12 = (lfT8 + lfT2) * 0.5411961f;
        const f32 lfT13 = (lfT4 - lfT9) * 3.830649f;
        const f32 lfT14 = (lfT2 - lfT8) * 1.306563f;
        const f32 lfT15 = (lfT10 + lfT5) * 0.6302362f;
        const f32 lfT16 = (lfT5 - lfT10) * 0.8213398f;
        lafOut[0] = lfT16 * 0.13052619f;
        lafOut[1] = lfT14 * 0.38268343f;
        lafOut[2] = lfT13 * 0.6087614f;
        lafOut[3] = lfT13 * -0.7933533f;
        lafOut[4] = lfT14 * -0.9238795f;
        lafOut[5] = lfT16 * -0.9914449f;
        lafOut[6] = lfT15 * -0.9914449f;
        lafOut[7] = lfT12 * -0.9238795f;
        lafOut[8] = lfT11 * -0.7933533f;
        lafOut[9] = lfT11 * -0.6087614f;
        lafOut[10] = lfT12 * -0.38268343f;
        lafOut[11] = lfT15 * -0.13052619f;

        f32* lpOut = &apOut[(6 + 6 * liWin) * 4];
        for (s32 li = 0; li < 12; ++li)
            lpOut[li * 4] = lpOut[li * 4] + lafOut[li];
    }
}

// Overlap-add four interleaved sub-bands: apOut gets the first 18 IMDCT outputs
// plus the saved overlap, and the overlap is replaced by the last 18 outputs.
void overlapadd(f32* apOut, const f32* apRaw, f32* apOverlap)
{
    for (s32 liLane = 0; liLane < 4; ++liLane)
    {
        for (s32 li = 0; li < KI_SUBBAND_LINES; ++li)
            apOut[li * 4 + liLane] = apRaw[li * 4 + liLane] + apOverlap[li * 4 + liLane];
        for (s32 li = 0; li < KI_SUBBAND_LINES; ++li)
            apOverlap[li * 4 + liLane] = apRaw[(KI_SUBBAND_LINES + li) * 4 + liLane];
    }
}

inline bool IsShortBlock(const CMpegLayer3Base::GranuleInfo& arInfo)
{
    return arInfo.mucWindowSwitching != 0 && arInfo.mucBlockType == 2;
}

} // namespace

// ----------------------------------------------------------------------------
// The class vtable store and the base-class destructor call (vtable store +
// Close() when a stream is open) are compiler-emitted around this body.
// ----------------------------------------------------------------------------
CMpegLayer3Base::~CMpegLayer3Base()
{
    if (mpOverlap != 0)
        gpfnSndFree(mpOverlap);
}

// ----------------------------------------------------------------------------
// Long bands: all 22 for long blocks, the first 8 (MPEG-1 rows) or 6 (LSF rows)
// for mixed blocks, none for pure short blocks. Short bands follow from band 3
// (mixed) or 0 (pure short); long blocks have none. A band whose gain is exactly
// 1.0 is left alone.
// ----------------------------------------------------------------------------
void CMpegLayer3Base::Dequantize(s32 aiChannel, s32 aiGranule, f32* apSamples)
{
    const GranuleInfo&  lrInfo  = maGranule[aiChannel][aiGranule];
    const ScaleFactors& lrScale = maScaleFactors[aiChannel];
    const SfBandTable&  lrBands = kaSfBands[mucSampleRateIndex2];

    s32 liLongBands;
    if (IsShortBlock(lrInfo))
    {
        if (lrInfo.mucMixedBlock != 0)
            liLongBands = (mucSampleRateIndex2 < 3) ? 8 : 6;
        else
            liLongBands = 0;
    }
    else
    {
        liLongBands = 22;
    }

    for (s32 liSfb = 0; liSfb < liLongBands; ++liSfb)
    {
        const s32 liStart = lrBands.masLong[liSfb];
        const s32 liWidth = lrBands.masLong[liSfb + 1] - liStart;

        s32 liFactor = lrScale.masLong[liSfb];
        if (lrInfo.mucPreflag != 0)
            liFactor += kacPretab[liSfb];

        const f32 lfGain = spGainTable[(liFactor << lrInfo.muScalefacScale) * 2];
        if (lfGain != 1.0f)
            scalesamples(&apSamples[liStart], lfGain, liWidth);
    }

    s32 liFirstShort;
    if (liLongBands == 22)
        liFirstShort = 13;
    else if (liLongBands != 0)
        liFirstShort = 3;
    else
        liFirstShort = 0;

    for (s32 liSfb = liFirstShort; liSfb < 13; ++liSfb)
    {
        const s32 liStart = lrBands.maucShort[liSfb];
        const s32 liWidth = lrBands.maucShort[liSfb + 1] - liStart;

        s32 liPos = liStart * 3;
        for (s32 liWin = 0; liWin < 3; ++liWin)
        {
            const s32 liIndex = (lrScale.masShort[liWin][liSfb] << lrInfo.muScalefacScale)
                              + lrInfo.maucSubblockGain[liWin] * 4;
            const f32 lfGain = spGainTable[liIndex * 2];
            if (lfGain != 1.0f)
                scalesamples(&apSamples[liPos], lfGain, liWidth);
            liPos += liWidth;
        }
    }
}

// ----------------------------------------------------------------------------
// is_pos 0: both scales 1.0. Odd is_pos: the left channel is attenuated by
// row[(is_pos + 1) / 2]. Even is_pos: the right channel by row[is_pos / 2].
// ----------------------------------------------------------------------------
void CMpegLayer3Base::i_stereo_k_values(s32 aiIsPos, s32 aiIoType, s32 aiIndex, f32 (*apK)[576])
{
    const u32 luIsPos = static_cast<u32>(aiIsPos);
    if (luIsPos == 0)
    {
        apK[1][aiIndex] = 1.0f;
        apK[0][aiIndex] = 1.0f;
    }
    else if ((luIsPos & 1) != 0)
    {
        apK[0][aiIndex] = kafIsLsf[aiIoType][(luIsPos + 1) >> 1];
        apK[1][aiIndex] = 1.0f;
    }
    else
    {
        apK[0][aiIndex] = 1.0f;
        apK[1][aiIndex] = kafIsLsf[aiIoType][luIsPos >> 1];
    }
}

// ----------------------------------------------------------------------------
// Joint stereo for one granule (left = apSamples[0..575], right = [576..1151]).
//
// Intensity stereo (mode 1, mode-extension bit 0): every line starts with
// intensity position 7 ("not intensity coded"). The intensity positions come
// from the RIGHT channel's scale factors (channel 1) and the block layout from
// channel 0's side information:
//   * short blocks, per window: find the highest short band with a non-zero
//     right line (bands 12..3 for mixed blocks, 12..0 for pure short), give the
//     bands above it (below 12) their scale factor as position, then copy band
//     10's position/ratio over band 11. For mixed blocks, when no window had a
//     non-zero line above band 3, the long bands above the highest non-zero line
//     of sub-bands 0..2 (below band 8) are intensity coded too.
//   * long blocks: find the highest non-zero right line, code the long bands
//     above it (below band 21), and extend band 20's position to the top of the
//     spectrum.
// Each coded line records a ratio (MPEG-1, tangent table) or a k-value pair (LSF).
// Then every line: position 7 -> mid-side when enabled, else untouched; otherwise
// the left signal is split by the k pair (LSF) or by ratio/(1+ratio) and
// 1/(1+ratio) (MPEG-1).
//
// Mid-side only (bit 1 without bit 0): L = (M+S)/sqrt2, R = (M-S)/sqrt2 over all
// 576 lines.
// ----------------------------------------------------------------------------
void CMpegLayer3Base::Stereo(s32 aiGranule, f32* apSamples)
{
    const bool lbIntensity = (mucMode == 1) && ((mucModeExtension & 1) != 0);
    const bool lbMidSide   = (mucMode == 1) && ((mucModeExtension & 2) != 0);

    f32* lpLeft  = apSamples;
    f32* lpRight = apSamples + KI_GRANULE_LINES;

    if (!lbIntensity)
    {
        if (lbMidSide)
        {
            for (s32 li = 0; li < KI_GRANULE_LINES; ++li)
            {
                const f32 lfMid  = lpLeft[li];
                const f32 lfSide = lpRight[li];
                lpLeft[li]  = (lfSide + lfMid) * KF_SQRT_HALF;
                lpRight[li] = (lfMid - lfSide) * KF_SQRT_HALF;
            }
        }
        return;
    }

    const GranuleInfo&  lrInfo  = maGranule[0][aiGranule];
    const ScaleFactors& lrScale = maScaleFactors[1];
    const s32 liIoType = lrInfo.muScalefacCompress & 1;

    s32 laiIsPos[KI_GRANULE_LINES];
    f32 lafIsRatio[KI_GRANULE_LINES];
    f32 lafK[2][KI_GRANULE_LINES];
    for (s32 li = 0; li < KI_GRANULE_LINES; ++li)
        laiIsPos[li] = 7;

    const SfBandTable& lrBands = kaSfBands[mucSampleRateIndex2];

    if (IsShortBlock(lrInfo))
    {
        const s32 liLowestBand = (lrInfo.mucMixedBlock != 0) ? 3 : 0;
        s32 liMaxSfb = 0;

        const s32 liBand10 = lrBands.maucShort[10];
        const s32 liBand11 = lrBands.maucShort[11];
        const s32 liBand12 = lrBands.maucShort[12];

        for (s32 liWin = 0; liWin < 3; ++liWin)
        {
            s32 liHighest = liLowestBand - 1;
            for (s32 liSfb = 12; liSfb >= liLowestBand; --liSfb)
            {
                s32 liLines = lrBands.maucShort[liSfb + 1] - lrBands.maucShort[liSfb];
                s32 liLine = 3 * lrBands.maucShort[liSfb] + (liWin + 1) * liLines - 1;
                while (liLines > 0)
                {
                    if (lpRight[liLine] != 0.0f)
                    {
                        liHighest = liSfb;
                        liSfb = -10;
                        liLines = -10;
                    }
                    --liLines;
                    --liLine;
                }
            }

            s32 liSfb = liHighest + 1;
            if (lrInfo.mucMixedBlock != 0 && liSfb > liMaxSfb)
                liMaxSfb = liSfb;

            for (; liSfb < 12; ++liSfb)
            {
                const s32 liLines = lrBands.maucShort[liSfb + 1] - lrBands.maucShort[liSfb];
                s32 liLine = 3 * lrBands.maucShort[liSfb] + liWin * liLines;
                if (liLines <= 0)
                    continue;

                const s32 liPos = lrScale.masShort[liWin][liSfb];
                for (s32 li = 0; li < liLines; ++li)
                    laiIsPos[liLine + li] = liPos;

                for (s32 li = 0; li < liLines; ++li, ++liLine)
                {
                    if (liPos != 7)
                    {
                        if (mucIsLsf != 0)
                            i_stereo_k_values(liPos, liIoType, liLine, lafK);
                        else
                            lafIsRatio[liLine] = kafIsRatio[liPos];
                    }
                }
            }

            const s32 liSrc = 3 * liBand10 + liWin * (liBand11 - liBand10);
            s32 liCount = liBand12 - liBand11;
            s32 liDst = 3 * liBand11 + liWin * liCount;
            for (; liCount > 0; --liCount, ++liDst)
            {
                laiIsPos[liDst] = laiIsPos[liSrc];
                if (mucIsLsf != 0)
                {
                    lafK[0][liDst] = lafK[0][liSrc];
                    lafK[1][liDst] = lafK[1][liSrc];
                }
                else
                {
                    lafIsRatio[liDst] = lafIsRatio[liSrc];
                }
            }
        }

        if (lrInfo.mucMixedBlock != 0 && liMaxSfb <= 3)
        {
            s32 liSub = 2;
            s32 liSs = 17;
            s32 liHighestLine = -1;
            while (liSub >= 0)
            {
                if (lpRight[liSub * KI_SUBBAND_LINES + liSs] != 0.0f)
                {
                    liHighestLine = liSub * KI_SUBBAND_LINES + liSs;
                    liSub = -1;
                }
                else
                {
                    --liSs;
                    if (liSs < 0)
                    {
                        --liSub;
                        liSs = 17;
                    }
                }
            }

            s32 liSfb = 0;
            while (lrBands.masLong[liSfb] <= liHighestLine)
                ++liSfb;

            s32 liLine = lrBands.masLong[liSfb];
            for (; liSfb < 8; ++liSfb)
            {
                const s32 liLines = lrBands.masLong[liSfb + 1] - lrBands.masLong[liSfb];
                if (liLines <= 0)
                    continue;

                const s32 liPos = lrScale.masLong[liSfb];
                for (s32 li = 0; li < liLines; ++li)
                    laiIsPos[liLine + li] = liPos;

                for (s32 li = 0; li < liLines; ++li, ++liLine)
                {
                    if (liPos != 7)
                    {
                        if (mucIsLsf != 0)
                            i_stereo_k_values(liPos, liIoType, liLine, lafK);
                        else
                            lafIsRatio[liLine] = kafIsRatio[liPos];
                    }
                }
            }
        }
    }
    else
    {
        s32 liSub = 31;
        s32 liSs = 17;
        s32 liHighestLine = 0;
        while (liSub >= 0)
        {
            if (lpRight[liSub * KI_SUBBAND_LINES + liSs] != 0.0f)
            {
                liHighestLine = liSub * KI_SUBBAND_LINES + liSs;
                liSub = -1;
            }
            else
            {
                --liSs;
                if (liSs < 0)
                {
                    --liSub;
                    liSs = 17;
                }
            }
        }

        s32 liSfb = 0;
        while (lrBands.masLong[liSfb] <= liHighestLine)
            ++liSfb;

        s32 liLine = lrBands.masLong[liSfb];
        for (; liSfb < 21; ++liSfb)
        {
            const s32 liLines = lrBands.masLong[liSfb + 1] - lrBands.masLong[liSfb];
            if (liLines <= 0)
                continue;

            const s32 liPos = lrScale.masLong[liSfb];
            for (s32 li = 0; li < liLines; ++li)
                laiIsPos[liLine + li] = liPos;

            for (s32 li = 0; li < liLines; ++li, ++liLine)
            {
                if (liPos != 7)
                {
                    if (mucIsLsf != 0)
                        i_stereo_k_values(liPos, liIoType, liLine, lafK);
                    else
                        lafIsRatio[liLine] = kafIsRatio[liPos];
                }
            }
        }

        const s32 liSrc = lrBands.masLong[20];
        for (s32 liCount = KI_GRANULE_LINES - lrBands.masLong[21];
             liCount > 0 && liLine < KI_GRANULE_LINES;
             --liCount, ++liLine)
        {
            laiIsPos[liLine] = laiIsPos[liSrc];
            if (mucIsLsf != 0)
            {
                lafK[0][liLine] = lafK[0][liSrc];
                lafK[1][liLine] = lafK[1][liSrc];
            }
            else
            {
                lafIsRatio[liLine] = lafIsRatio[liSrc];
            }
        }
    }

    for (s32 liSb = 0; liSb < KI_GRANULE_LINES; liSb += KI_SUBBAND_LINES)
    {
        for (s32 liSs = 0; liSs < KI_SUBBAND_LINES; ++liSs)
        {
            const s32 li = liSb + liSs;
            if (laiIsPos[li] == 7)
            {
                if (lbMidSide)
                {
                    const f32 lfMid  = lpLeft[li];
                    const f32 lfSide = lpRight[li];
                    lpRight[li] = (lfMid - lfSide) * KF_SQRT_HALF;
                    lpLeft[li]  = (lfSide + lfMid) * KF_SQRT_HALF;
                }
            }
            else if (mucIsLsf != 0)
            {
                const f32 lfSignal = lpLeft[li];
                lpLeft[li]  = lafK[0][li] * lfSignal;
                lpRight[li] = lafK[1][li] * lfSignal;
            }
            else
            {
                const f32 lfSignal = lpLeft[li];
                const f32 lfRatio  = lafIsRatio[li];
                const f32 lfRight  = lfSignal / (lfRatio + 1.0f);
                lpLeft[li]  = lfRatio * lfRight;
                lpRight[li] = lfRight;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Only short and mixed blocks are reordered. Mixed: the two long sub-bands (36
// lines) are copied, then short bands 3..12 from the band table. Pure short: the
// first three bands (four lines each) and then bands 3..12 from the width table.
// Within a band, out[3j + w] = in[w * width + j].
// ----------------------------------------------------------------------------
void CMpegLayer3Base::Reorder(s32 aiChannel, s32 aiGranule, const f32* apIn, f32* apOut)
{
    const GranuleInfo& lrInfo = maGranule[aiChannel][aiGranule];
    if (!IsShortBlock(lrInfo))
        return;

    if (lrInfo.mucMixedBlock != 0)
    {
        for (s32 li = 0; li < 36; ++li)
            apOut[li] = apIn[li];

        for (s32 liSfb = 3; liSfb < 13; ++liSfb)
        {
            const SfBandTable& lrBands = kaSfBands[mucSampleRateIndex2];
            const s32 liStart = lrBands.maucShort[liSfb];
            const s32 liWidth = lrBands.maucShort[liSfb + 1] - liStart;
            const s32 liBase  = liStart * 3;
            for (s32 lj = 0; lj < liWidth; ++lj)
            {
                apOut[liBase + 3 * lj]     = apIn[liBase + lj];
                apOut[liBase + 3 * lj + 1] = apIn[liBase + liWidth + lj];
                apOut[liBase + 3 * lj + 2] = apIn[liBase + 2 * liWidth + lj];
            }
        }
        return;
    }

    for (s32 liSfb = 0; liSfb < 3; ++liSfb)
    {
        const s32 liBase = liSfb * 12;
        for (s32 lj = 0; lj < 4; ++lj)
        {
            apOut[liBase + 3 * lj]     = apIn[liBase + lj];
            apOut[liBase + 3 * lj + 1] = apIn[liBase + 4 + lj];
            apOut[liBase + 3 * lj + 2] = apIn[liBase + 8 + lj];
        }
    }

    const u8* lpWidths = kauShortWidth[mucSampleRateIndex2];
    s32 liBase = 36;
    for (s32 liBand = 0; liBand < 10; ++liBand)
    {
        const s32 liWidth = lpWidths[liBand];
        for (s32 lj = 0; lj < liWidth; lj += 2)
        {
            apOut[liBase + 3 * lj]     = apIn[liBase + lj];
            apOut[liBase + 3 * lj + 1] = apIn[liBase + liWidth + lj];
            apOut[liBase + 3 * lj + 2] = apIn[liBase + 2 * liWidth + lj];
            apOut[liBase + 3 * lj + 3] = apIn[liBase + lj + 1];
            apOut[liBase + 3 * lj + 4] = apIn[liBase + liWidth + lj + 1];
            apOut[liBase + 3 * lj + 5] = apIn[liBase + 2 * liWidth + lj + 1];
        }
        liBase += 3 * liWidth;
    }
}

// ----------------------------------------------------------------------------
// Pure short blocks are skipped. Mixed short blocks only get the boundary between
// sub-bands 0 and 1; everything else gets all 31 boundaries.
// ----------------------------------------------------------------------------
void CMpegLayer3Base::AntiAlias(s32 aiChannel, s32 aiGranule, f32* apSamples)
{
    const GranuleInfo& lrInfo = maGranule[aiChannel][aiGranule];
    if (IsShortBlock(lrInfo) && lrInfo.mucMixedBlock == 0)
        return;

    f32* lpEnd;
    if (lrInfo.mucWindowSwitching != 0 && lrInfo.mucMixedBlock != 0 && lrInfo.mucBlockType == 2)
        lpEnd = apSamples + KI_SUBBAND_LINES;
    else
        lpEnd = apSamples + 31 * KI_SUBBAND_LINES;

    for (f32* lpBand = apSamples; lpBand < lpEnd; lpBand += KI_SUBBAND_LINES)
    {
        for (s32 li = 0; li < 8; ++li)
        {
            const f32 lfLow  = lpBand[17 - li];
            const f32 lfHigh = lpBand[18 + li];
            lpBand[17 - li] = lfLow * kafAliasCs[li] - lfHigh * kafAliasCa[li];
            lpBand[18 + li] = lfHigh * kafAliasCs[li] + lfLow * kafAliasCa[li];
        }
    }
}

// ----------------------------------------------------------------------------
// The overlap buffer is allocated (and cleared) on first use for all channels.
// Mixed blocks: sub-bands 0 and 1 use the normal long window, 2 and 3 the short
// transform. The remaining groups of four sub-bands use the short transform for
// block type 2 and the block type's long window otherwise.
// ----------------------------------------------------------------------------
void CMpegLayer3Base::Hybrid(s32 aiChannel, s32 aiGranule, f32* apSamples)
{
    if (mpOverlap == 0)
    {
        const u32 luBytes = static_cast<u32>(mucChannelCount) * KI_GRANULE_LINES * sizeof(f32);
        mpOverlap = static_cast<f32*>(gpfnSndAlloc(luBytes));
        if (mpOverlap != 0)
            std::memset(mpOverlap, 0, luBytes);
        else
            apSamples[0] = 0.0f;
    }

    f32* lpOverlap = &mpOverlap[aiChannel * KI_GRANULE_LINES];
    const GranuleInfo& lrInfo = maGranule[aiChannel][aiGranule];

    f32 lafRaw[36 * 4];
    s32 liGroup = 0;
    if (lrInfo.mucWindowSwitching != 0 && lrInfo.mucMixedBlock != 0)
    {
        imdct36(apSamples + 0, lafRaw + 0, kafImdctWindow[0]);
        imdct36(apSamples + 1, lafRaw + 1, kafImdctWindow[0]);
        imdct12(apSamples + 2, lafRaw + 2);
        imdct12(apSamples + 3, lafRaw + 3);
        overlapadd(apSamples, lafRaw, lpOverlap);
        liGroup = 1;
    }

    if (lrInfo.mucBlockType == 2)
    {
        for (; liGroup < 8; ++liGroup)
        {
            f32* lpGroup = apSamples + liGroup * 72;
            for (s32 liLane = 0; liLane < 4; ++liLane)
                imdct12(lpGroup + liLane, lafRaw + liLane);
            overlapadd(lpGroup, lafRaw, lpOverlap + liGroup * 72);
        }
    }
    else
    {
        for (; liGroup < 8; ++liGroup)
        {
            f32* lpGroup = apSamples + liGroup * 72;
            const f32* lpWin = kafImdctWindow[lrInfo.mucBlockType];
            for (s32 liLane = 0; liLane < 4; ++liLane)
                imdct36(lpGroup + liLane, lafRaw + liLane, lpWin);
            overlapadd(lpGroup, lafRaw, lpOverlap + liGroup * 72);
        }
    }
}

} // namespace Snd
