// =====================================================================================
// rw::audio::core sample-buffer mixing kernel bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every store/coefficient. The Feb-2007 leak carries only the
// package's public headers (rwaudiocore 2.11.00) -- these kernels live in the library's
// private channel TU, so each body below is grounded in the disassembly.
//   CopyWithGain              @0x82B67200 -- VMX block kernel, lowered scalar
//   CopyWithGainRamp          @0x82B67400 -- VMX ramp kernel, lowered scalar
//   MixWithGain               @0x82B6A3F8 -- VMX block kernel, lowered scalar
//   MixWithGainRamp           @0x82B6A568 -- VMX ramp kernel, lowered scalar
//   ScaleSamples              @0x82B6B708 -- VMX block kernel, lowered scalar
//   ClipFloats                @0x82B64B68 -- store-for-store
//   DeClick                   @0x82B676F8 -- store-for-store
//   ReOrderRwAudioCoreToWave  @0x82B6B590 -- store-for-store
//   ReChannelGainWrite1x4/1x6/2x4/2x6/4x1/4x2/4x4/4x6/6x2/6x4/6x6
//                             @0x82B6ACA0..0x82B6B25C -- store-for-store
//   ReChannelGainWrite        @0x82B6B268 -- store-for-store (dispatcher)
//   ReChannelGainMix          @0x82B6AB48 -- store-for-store (routing tables FLAGGED)
//   ReChannelGainMixRamp      @0x82B6ABE8 -- store-for-store (routing tables FLAGGED)
//   GainVectorLinearAmplitude @0x82B69938 -- store-for-store
//   GainVectorLinearPower     @0x82B69AB0 -- store-for-store
//   GainVectorSine            @0x82B69C30 -- store-for-store
//
// VMX lowering note (the committed Fir64::MultiplyAccumulate idiom): the X360 gives the
// five block kernels a 128-byte-alignment test ((dst|src) & 0x7F, count & 0x3F) that
// selects between a 32-float lvx128/vmulfp128-vmaddfp/stvx128 block loop and a scalar
// fallback. Both paths compute the IDENTICAL per-sample product/accumulate (the vector
// lanes replicate the scalar gain; the ramp kernels' lane vectors are gain + step*i,
// derived below), so each is lowered to one faithful scalar loop -- the split selects
// the block width, never the values.
// =====================================================================================

#include "rw/audio/core/MixKernels.h"

#include <cmath>   // std::sin / std::sqrt (the X360 CRT sin / fsqrts)
#include <cstring> // std::memset / std::memcpy (the X360 XMemSet / XMemCpy)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// XMemSet / XMemCpy @ the X360 build = byte-wise memset / memcpy.
inline void XMemSet(void *dst, int value, u32 bytes) { std::memset(dst, value, bytes); }
inline void XMemCpy(void *dst, const void *src, u32 bytes) { std::memcpy(dst, src, bytes); }

// _blkmov -- the X360 CRT ASCENDING byte copy the GainVector fillers exploit as a
// "replicate the first element forward" idiom (dst == src + 4, overlapping). A memmove
// would NOT replicate (it copies backward on this overlap), so the ascending order is
// load-bearing; reproduced as an explicit forward byte loop.
inline void BlkMovAscending(u8 *dst, const u8 *src, u32 bytes)
{
    for (u32 i = 0; i < bytes; ++i)
        dst[i] = src[i];
}

// One source/destination channel-buffer node read out of the rwaudio Process graph: the
// channel-0 sample base at +0x04 and the 16-bit inter-channel stride (in samples) at
// +0x0E (the asm `lwz r,4(node)` / `lhz r,0xE(node)`). Same node shape Fir64::Filter
// and RawPuller2 read.
struct ChannelNode
{
    char *mpReserved0; // +0x00
    f32  *mpBase;      // +0x04 -- channel-0 sample base
    char  mGap08[0x0E - 0x08];
    u16   muStride16;  // +0x0E -- inter-channel stride, in samples
};

// flt_82011C14 == 0.70700002f -- the centre/mono fold-down gain scale.
const f32 KF_CHANNEL_FOLD = 0.70700002f;

// GAIN_DECLICK_FRAME_SIZE == 64 (rwaudiocore 2.11.00 base.h) -- the ramped head of the
// *WithGainRamp kernels (asm: the first do-while runs to src + 0x100 bytes; the flat
// gain is fmadds step * flt_8214AED8(64.0) + gain).
const s32 KI_GAIN_DECLICK_FRAME_SIZE = 64;

// =====================================================================================
// FLAG (undecoded rodata): the ReChannelGainMix / ReChannelGainMixRamp channel-routing
// descriptor block at 0x8214AFD0..0x8214B10x. The lookup CODE below is store-for-store,
// but the table CONTENTS are data-segment rodata with no dump in this export set, so
// they are zero-filled placeholders (family precedent: BandPassIir2's undecoded
// descriptor). A zero descriptor routes ppDst[0] += ppSrc[0] * 0.0 -- value-neutral.
//
//   flt_8214AFD0 -- the gain-scale slots a descriptor's bits 6-7 select (byte offset
//                   (b>>4)&0xC into the table). Slots 2/3 would alias the range table
//                   that starts 8 bytes later at 0x8214AFD8, so only slots 0/1 are
//                   addressable as floats in the shipped image.
//   unk_8214AFD8 -- u8[6][6][2] {first,last} descriptor-range pairs, indexed
//                   2 * (6*numSrcChannels + numDstChannels - 7) (72 bytes; the size is
//                   attested by the indexing range over src/dst in 1..6 and by the pair
//                   table starting exactly 72 bytes later at 0x8214B020).
//   unk_8214B020 -- the packed pair descriptors: dst = b & 7, src = (b >> 3) & 7,
//                   scale slot = (b >> 6) & 3. Upper size bound 0xE8 bytes (the next
//                   attested rodata, flt_8214B108, the denormal-flush bias).
// =====================================================================================
const f32 gReChannelGainScaleTable[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // flt_8214AFD0
const u8  gReChannelGainRangeTable[72] = { 0 };                     // unk_8214AFD8
const u8  gReChannelGainPairTable[0xE8] = { 0 };                    // unk_8214B020

} // namespace

// -------------------------------------------------------------------------------------
// CopyWithGain @0x82B67200 -- pDst[i] = pSrc[i] * gain.
//
// X360: if ((dst|src) & 0x7F) or (count & 0x3F) take the scalar loop (lfsx/fmuls/stfs
// over count samples); otherwise a dcbt/dcbz128-prefetched 32-float VMX block loop over
// the same count (the branch guarantees count % 64 == 0 there). Both paths store
// src[i] * gain to dst[i] for every i in [0, count) -- lowered to the one scalar loop.
// -------------------------------------------------------------------------------------
void CopyWithGain(f32 *pDst, const f32 *pSrc, f32 gain, s32 numSamples)
{
    for (s32 i = 0; i < numSamples; ++i)
        pDst[i] = pSrc[i] * gain; // fmuls f0,f0,f1 / vmulfp128
}

// -------------------------------------------------------------------------------------
// CopyWithGainRamp @0x82B67400 -- de-clicked gain ramp (VMX-only on X360; the mixer
// always hands it 128-byte-aligned frame buffers and count == MIXER_FRAME_SIZE(256)).
//
// The head loop's lane vector v0 = {g, g+s, g+2s, g+3s} advances by the rodata group
// multipliers {1..8} * (4s) per 4-sample group (vmaddfp vG = vK*v13 + v0 with
// v13 = {4s x4}; unk_8214B110..8214B180 hold the lane constants {1},{2},{3},{7},{8},
// {4},{5},{6}), so sample i multiplies by exactly gain + step*i. After the first 64
// samples (2 blocks, GAIN_DECLICK_FRAME_SIZE) the gain freezes at
// gain + step*64 (fmadds f11 = f2 * flt_8214AED8(64.0) + f1) for the remaining
// (4*count & ~0xF) bytes.
// -------------------------------------------------------------------------------------
void CopyWithGainRamp(f32 *lpDst, f32 *lpSrc, float lfGain, float lfStep, int liCount)
{
    // Head: per-sample ramp over the 64-sample de-click frame. The X360 do-while runs
    // these two 32-float blocks unconditionally (valid inputs have count >= 96;
    // the only shipped caller passes MIXER_FRAME_SIZE == 256).
    s32 i = 0;
    for (; i < KI_GAIN_DECLICK_FRAME_SIZE; ++i)
        lpDst[i] = lpSrc[i] * (lfGain + lfStep * static_cast<f32>(i));

    // Tail: flat gain for the rest of the frame (extlwi: (4*count) & ~0xF bytes).
    const f32 flatGain = lfStep * 64.0f + lfGain; // fmadds f11, f2, f13(64.0), f1
    const s32 end = liCount & ~3;
    for (; i < end; ++i)
        lpDst[i] = lpSrc[i] * flatGain;
}

// -------------------------------------------------------------------------------------
// MixWithGain @0x82B6A3F8 -- pDst[i] += pSrc[i] * gain. Same alignment split as
// CopyWithGain (scalar fmadds fallback / 32-float vmaddfp block loop), same values.
// -------------------------------------------------------------------------------------
void MixWithGain(f32 *pDst, const f32 *pSrc, f32 gain, s32 numSamples)
{
    for (s32 i = 0; i < numSamples; ++i)
        pDst[i] = pSrc[i] * gain + pDst[i]; // fmadds f0,f13,f1,f0 / vmaddfp
}

// -------------------------------------------------------------------------------------
// MixWithGainRamp @0x82B6A568 -- accumulating CopyWithGainRamp (identical ramp lane
// construction from unk_8214B1E0..8214B250, vmaddfp accumulate instead of vmulfp write).
// -------------------------------------------------------------------------------------
void MixWithGainRamp(f32 *pDst, f32 *pSrc, f32 gain, f32 step, s32 numSamples)
{
    s32 i = 0;
    for (; i < KI_GAIN_DECLICK_FRAME_SIZE; ++i)
        pDst[i] += pSrc[i] * (gain + step * static_cast<f32>(i));

    const f32 flatGain = step * 64.0f + gain; // fmadds f11, f2, f13(64.0), f1
    const s32 end = numSamples & ~3;
    for (; i < end; ++i)
        pDst[i] += pSrc[i] * flatGain;
}

// -------------------------------------------------------------------------------------
// ScaleSamples @0x82B6B708 -- pData[i] *= scale, guarded by count > 0 (blelr). Same
// alignment split (scalar fmuls fallback / 32-float vmulfp128 block loop).
// -------------------------------------------------------------------------------------
void ScaleSamples(f32 *pData, f32 scale, s32 numSamples)
{
    if (numSamples <= 0)
        return; // cmpwi r5,0 ; blelr

    for (s32 i = 0; i < numSamples; ++i)
        pData[i] = pData[i] * scale;
}

// -------------------------------------------------------------------------------------
// ClipFloats @0x82B64B68 -- clamp into [minValue, maxValue]; only out-of-range samples
// are stored back (the in-range path never rewrites, exactly as the asm branches).
// -------------------------------------------------------------------------------------
void ClipFloats(f32 *pData, f32 minValue, f32 maxValue, s32 numSamples)
{
    f32 *p = pData;                 // r11
    f32 *const end = pData + numSamples; // r10 = r3 + (count << 2)
    for (; p < end; ++p)            // unsigned pointer compare (cmplw)
    {
        const f32 v = *p;
        if (v >= minValue)          // fcmpu f0,f1 ; bge
        {
            if (v > maxValue)       // fcmpu f0,f2 ; ble skips
                *p = maxValue;
        }
        else
        {
            *p = minValue;
        }
    }
}

// -------------------------------------------------------------------------------------
// DeClick @0x82B676F8 -- fold each channel's residual click value into the first 16
// output samples with linearly decaying coefficients (16-j)/17, then clear it.
//
// Float literals are the X360 rodata values verbatim (flt_8214B190..flt_8214B1C8, with
// 9/17 sourced from flt_820581AC -- same value, shared rodata slot). The asm reloads
// pDeClickValue[ch] before every fmadds (alias-exact); re-reading the array element
// per tap preserves that.
// -------------------------------------------------------------------------------------
void DeClick(void *pSampleBuffer, f32 *pDeClickValue, u32 numChannels)
{
    // (16-j)/17 for j = 0..15.
    static const f32 KAF_DECLICK[16] = {
        0.94117647f, 0.88235295f, 0.82352942f, 0.7647059f,
        0.70588237f, 0.64705884f, 0.58823532f, 0.52941179f,
        0.47058824f, 0.41176471f, 0.35294119f, 0.29411766f,
        0.23529412f, 0.17647059f, 0.11764706f, 0.05882353f,
    };

    ChannelNode *node = static_cast<ChannelNode *>(pSampleBuffer);

    for (u32 ch = 0; ch < numChannels; ++ch)
    {
        // lhz 0xE(r3) * ch, scaled to floats; lwz 4(r3) base.
        f32 *pOut = node->mpBase + static_cast<u32>(node->muStride16) * ch;

        for (int j = 0; j < 16; ++j)
            pOut[j] = pDeClickValue[ch] * KAF_DECLICK[j] + pOut[j]; // fmadds

        pDeClickValue[ch] = 0.0f; // stfs flt_82001CC0(0.0)
    }
}

// -------------------------------------------------------------------------------------
// ReOrderRwAudioCoreToWave @0x82B6B590 -- planar rwaudio channels -> interleaved WAV.
// rwaudio channel order (attested by this map + the 6x2/6x4 fold-downs):
//   ch0 = FL, ch1 = C, ch2 = FR, ch3 = BL, ch4 = BR, ch5 = LFE
// WAV 5.1 order: { FL, FR, C, LFE, BL, BR } = { ch0, ch2, ch1, ch5, ch3, ch4 }.
// -------------------------------------------------------------------------------------
void ReOrderRwAudioCoreToWave(f32 *pDst, void *pSampleBuffer, s32 numChannels,
                              s32 numSamples)
{
    ChannelNode *node = static_cast<ChannelNode *>(pSampleBuffer);
    const u32 stride = node->muStride16; // lhz 0xE -- inter-channel stride in samples
    const f32 *base = node->mpBase;      // lwz 4

    switch (numChannels)
    {
    case 6:
    {
        const f32 *ch0 = base;
        const f32 *ch1 = base + stride;     // rotlwi *4  bytes
        const f32 *ch2 = base + 2 * stride; // rotlwi *8
        const f32 *ch3 = base + 3 * stride; // mulli 0xC
        const f32 *ch4 = base + 4 * stride; // rotlwi *16
        const f32 *ch5 = base + 5 * stride; // mulli 0x14
        for (s32 i = 0; i < numSamples; ++i)
        {
            pDst[6 * i + 2] = ch1[i]; // stfs 8(r3)
            pDst[6 * i + 1] = ch2[i]; // stfs 4(r3)
            pDst[6 * i + 5] = ch4[i]; // stfs 0x14(r3)
            pDst[6 * i + 4] = ch3[i]; // stfs 0x10(r3)
            pDst[6 * i + 0] = ch0[i]; // stfs 0(r3)
            pDst[6 * i + 3] = ch5[i]; // stfs 0xC(r3)
        }
        break;
    }
    case 4:
    {
        const f32 *ch0 = base;
        const f32 *ch1 = base + stride;
        const f32 *ch2 = base + 2 * stride;
        const f32 *ch3 = base + 3 * stride;
        for (s32 i = 0; i < numSamples; ++i)
        {
            pDst[4 * i + 1] = ch1[i];
            pDst[4 * i + 3] = ch3[i];
            pDst[4 * i + 2] = ch2[i];
            pDst[4 * i + 0] = ch0[i];
        }
        break;
    }
    case 2:
    {
        const f32 *ch0 = base;
        const f32 *ch1 = base + stride;
        for (s32 i = 0; i < numSamples; ++i)
        {
            pDst[2 * i + 1] = ch1[i];
            pDst[2 * i + 0] = ch0[i];
        }
        break;
    }
    case 1:
        XMemCpy(pDst, base, static_cast<u32>(numSamples) << 2); // b XMemCpy
        break;
    default:
        break; // bnelr -- any other channel count returns untouched
    }
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite1x4 @0x82B6ACA0 -- mono -> quad: equal-power split into FL/FR
// (both at gain * 0.707; the second CopyWithGain reuses f1, which the first call
// preserves), rears zeroed.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite1x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    const f32 scaled = gain * KF_CHANNEL_FOLD; // fmuls f1, f1, flt_82011C14
    CopyWithGain(ppDst[0], ppSrc[0], scaled, numSamples);
    CopyWithGain(ppDst[1], ppSrc[0], scaled, numSamples); // f1 unchanged across the call
    const u32 bytes = static_cast<u32>(numSamples) << 2;
    XMemSet(ppDst[2], 0, bytes);
    XMemSet(ppDst[3], 0, bytes);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite1x6 @0x82B6AD10 -- mono -> 5.1: straight into the centre channel
// (ch1, no fold gain), all other channels zeroed.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite1x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[1], ppSrc[0], gain, numSamples);
    const u32 bytes = static_cast<u32>(numSamples) << 2;
    XMemSet(ppDst[0], 0, bytes);
    XMemSet(ppDst[2], 0, bytes);
    XMemSet(ppDst[3], 0, bytes);
    XMemSet(ppDst[4], 0, bytes);
    XMemSet(ppDst[5], 0, bytes);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite2x4 @0x82B6ADA8 -- stereo -> quad: L/R into fronts, rears zeroed.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite2x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[1], ppSrc[1], gain, numSamples);
    const u32 bytes = static_cast<u32>(numSamples) << 2;
    XMemSet(ppDst[2], 0, bytes);
    XMemSet(ppDst[3], 0, bytes);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite2x6 @0x82B6AE08 -- stereo -> 5.1: L -> FL (ch0), R -> FR (ch2);
// centre/rears/LFE zeroed.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite2x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[2], ppSrc[1], gain, numSamples);
    const u32 bytes = static_cast<u32>(numSamples) << 2;
    XMemSet(ppDst[1], 0, bytes);
    XMemSet(ppDst[3], 0, bytes);
    XMemSet(ppDst[4], 0, bytes);
    XMemSet(ppDst[5], 0, bytes);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite4x1 @0x82B6AE88 -- quad -> mono: sum all four into dst channel 0.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite4x1(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    f32 *dst0 = ppDst[0]; // lwz r29, 0(r3)
    CopyWithGain(dst0, ppSrc[0], gain, numSamples);
    MixWithGain(dst0, ppSrc[1], gain, numSamples);
    MixWithGain(dst0, ppSrc[2], gain, numSamples);
    MixWithGain(dst0, ppSrc[3], gain, numSamples);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite4x2 @0x82B6AEE8 -- quad -> stereo: L = FL + BL, R = FR + BR.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite4x2(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    f32 *dst0 = ppDst[0];
    CopyWithGain(dst0, ppSrc[0], gain, numSamples);
    MixWithGain(dst0, ppSrc[2], gain, numSamples);
    f32 *dst1 = ppDst[1];
    CopyWithGain(dst1, ppSrc[1], gain, numSamples);
    MixWithGain(dst1, ppSrc[3], gain, numSamples);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite4x4 @0x82B6AF50 -- quad -> quad: four straight copies.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite4x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[1], ppSrc[1], gain, numSamples);
    CopyWithGain(ppDst[2], ppSrc[2], gain, numSamples);
    CopyWithGain(ppDst[3], ppSrc[3], gain, numSamples);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite4x6 @0x82B6AFB0 -- quad -> 5.1: FL -> ch0, FR -> ch2, BL -> ch3,
// BR -> ch4; centre (ch1) and LFE (ch5) zeroed.
// -------------------------------------------------------------------------------------
void ReChannelGainWrite4x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[2], ppSrc[1], gain, numSamples);
    CopyWithGain(ppDst[3], ppSrc[2], gain, numSamples);
    CopyWithGain(ppDst[4], ppSrc[3], gain, numSamples);
    const u32 bytes = static_cast<u32>(numSamples) << 2;
    XMemSet(ppDst[1], 0, bytes);
    XMemSet(ppDst[5], 0, bytes);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite6x2 @0x82B6B0A0 -- 5.1 -> stereo fold-down:
//   L = 0.707*C + FL + BL,  R = 0.707*C + FR + BR  (LFE dropped).
// -------------------------------------------------------------------------------------
void ReChannelGainWrite6x2(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    const f32 foldGain = gain * KF_CHANNEL_FOLD; // fmuls f11, f12, flt_82011C14
    f32 *dst0 = ppDst[0];
    CopyWithGain(dst0, ppSrc[1], foldGain, numSamples); // centre
    MixWithGain(dst0, ppSrc[0], gain, numSamples);      // FL
    MixWithGain(dst0, ppSrc[3], gain, numSamples);      // BL (f1 preserved)
    f32 *dst1 = ppDst[1];
    CopyWithGain(dst1, ppSrc[1], foldGain, numSamples); // centre
    MixWithGain(dst1, ppSrc[2], gain, numSamples);      // FR
    MixWithGain(dst1, ppSrc[4], gain, numSamples);      // BR
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite6x4 @0x82B6B148 -- 5.1 -> quad fold-down:
//   FL' = 0.707*C + FL, FR' = 0.707*C + FR, BL' = BL, BR' = BR (LFE dropped).
// -------------------------------------------------------------------------------------
void ReChannelGainWrite6x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    const f32 foldGain = gain * KF_CHANNEL_FOLD;
    CopyWithGain(ppDst[0], ppSrc[1], foldGain, numSamples);
    MixWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[1], ppSrc[1], foldGain, numSamples);
    MixWithGain(ppDst[1], ppSrc[2], gain, numSamples);
    CopyWithGain(ppDst[2], ppSrc[3], gain, numSamples); // f1 = gain after the Mix
    CopyWithGain(ppDst[3], ppSrc[4], gain, numSamples);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite6x6 @0x82B6B1E8 -- 5.1 -> 5.1: six straight copies (channel 1
// first, exactly as the asm orders them).
// -------------------------------------------------------------------------------------
void ReChannelGainWrite6x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples)
{
    CopyWithGain(ppDst[1], ppSrc[1], gain, numSamples);
    CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
    CopyWithGain(ppDst[2], ppSrc[2], gain, numSamples);
    CopyWithGain(ppDst[3], ppSrc[3], gain, numSamples);
    CopyWithGain(ppDst[4], ppSrc[4], gain, numSamples);
    CopyWithGain(ppDst[5], ppSrc[5], gain, numSamples);
}

// -------------------------------------------------------------------------------------
// ReChannelGainWrite @0x82B6B268 -- the channel-remap dispatcher (see MixKernels.h).
// -------------------------------------------------------------------------------------
void ReChannelGainWrite(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                        u32 numSrcChannels, s32 numSamples, f32 gain)
{
    switch (numSrcChannels) // outer cmplwi r7 chain
    {
    case 1u:
        switch (numDstChannels) // inner cmplwi r28 chain
        {
        case 1u:
            CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
            return;
        case 2u:
        {
            // Mono -> stereo: both sides at gain * 0.707 (the second call reuses f1).
            const f32 scaled = gain * KF_CHANNEL_FOLD; // fmuls f1, f1, flt_82011C14
            CopyWithGain(ppDst[0], ppSrc[0], scaled, numSamples);
            CopyWithGain(ppDst[1], ppSrc[0], scaled, numSamples);
            return;
        }
        case 4u:
            ReChannelGainWrite1x4(ppDst, ppSrc, gain, numSamples);
            return;
        case 6u:
            ReChannelGainWrite1x6(ppDst, ppSrc, gain, numSamples);
            return;
        default:
            break;
        }
        break;
    case 2u:
        switch (numDstChannels)
        {
        case 1u:
            // Stereo -> mono: L + R summed into dst channel 0.
            CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
            MixWithGain(ppDst[0], ppSrc[1], gain, numSamples);
            return;
        case 2u:
            CopyWithGain(ppDst[0], ppSrc[0], gain, numSamples);
            CopyWithGain(ppDst[1], ppSrc[1], gain, numSamples);
            return;
        case 4u:
            ReChannelGainWrite2x4(ppDst, ppSrc, gain, numSamples);
            return;
        case 6u:
            ReChannelGainWrite2x6(ppDst, ppSrc, gain, numSamples);
            return;
        default:
            break;
        }
        break;
    case 4u:
        switch (numDstChannels)
        {
        case 1u:
            ReChannelGainWrite4x1(ppDst, ppSrc, gain, numSamples);
            return;
        case 2u:
            ReChannelGainWrite4x2(ppDst, ppSrc, gain, numSamples);
            return;
        case 4u:
            ReChannelGainWrite4x4(ppDst, ppSrc, gain, numSamples);
            return;
        case 6u:
            ReChannelGainWrite4x6(ppDst, ppSrc, gain, numSamples);
            return;
        default:
            break;
        }
        break;
    case 6u:
        switch (numDstChannels)
        {
        case 1u:
            ReChannelGainWrite6x1(ppDst, ppSrc, gain, numSamples);
            return;
        case 2u:
            ReChannelGainWrite6x2(ppDst, ppSrc, gain, numSamples);
            return;
        case 4u:
            ReChannelGainWrite6x4(ppDst, ppSrc, gain, numSamples);
            return;
        case 6u:
            ReChannelGainWrite6x6(ppDst, ppSrc, gain, numSamples);
            return;
        default:
            break;
        }
        break;
    default:
        break;
    }

    // Generic fallback (loc_82B6B48C): per-channel copies; surplus destination
    // channels are zero-filled.
    if (numSrcChannels >= numDstChannels) // cmplw r7, r28 ; bge
    {
        for (u32 ch = 0; ch < numDstChannels; ++ch)
            CopyWithGain(ppDst[ch], ppSrc[ch], gain, numSamples);
    }
    else
    {
        u32 copied = 0; // r26
        for (; copied < numSrcChannels; ++copied)
            CopyWithGain(ppDst[copied], ppSrc[copied], gain, numSamples);

        const u32 bytes = static_cast<u32>(numSamples) << 2; // slwi r29, 2
        for (u32 ch = copied; ch < numDstChannels; ++ch)
            XMemSet(ppDst[ch], 0, bytes);
    }
}

// -------------------------------------------------------------------------------------
// ReChannelGainMix @0x82B6AB48 -- table-driven accumulate remap (Send::Process).
// Descriptor decode, bit-exact to the asm's rlwinm forms:
//   dst channel = b & 7            (clrlslwi r11, 29,2 -> word index)
//   src channel = (b >> 3) & 7     (rlwinm 31,27,29    -> word index)
//   gain scale  = table[(b>>6)&3]  (rlwinm 28,4,29     -> float index)
// The asm re-reads the range table's `last` byte after every call; the table is const
// rodata so a hoisted read is value-identical.
// -------------------------------------------------------------------------------------
void ReChannelGainMix(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                      u32 numSrcChannels, s32 numSamples, f32 gain)
{
    // mulli r11, r7, 6 ; add r6 ; addi -7 ; slwi 1.
    const u8 *range =
        &gReChannelGainRangeTable[2 * (6 * numSrcChannels + numDstChannels - 7)];
    u32 first = range[0];      // lbz 0(r30)
    const u32 last = range[1]; // lbz 1(r30)

    for (; first <= last; ++first)
    {
        const u8 b = gReChannelGainPairTable[first];
        MixWithGain(ppDst[b & 7], ppSrc[(b >> 3) & 7],
                    gReChannelGainScaleTable[(b >> 6) & 3] * gain, numSamples);
    }
}

// -------------------------------------------------------------------------------------
// ReChannelGainMixRamp @0x82B6ABE8 -- ramped ReChannelGainMix. Ramp step =
// (targetGain - currentGain) * flt_8214B268 (0.015384615f == 1/65); each routed pair
// runs MixWithGainRamp from scale*currentGain with scale*step.
// -------------------------------------------------------------------------------------
void ReChannelGainMixRamp(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                          u32 numSrcChannels, s32 numSamples, f32 targetGain,
                          f32 currentGain)
{
    const f32 step = (targetGain - currentGain) * 0.015384615f; // fmuls f8, f13, flt_8214B268

    const u8 *range =
        &gReChannelGainRangeTable[2 * (6 * numSrcChannels + numDstChannels - 7)];
    u32 first = range[0];
    const u32 last = range[1];

    for (; first <= last; ++first)
    {
        const u8 b = gReChannelGainPairTable[first];
        const f32 scale = gReChannelGainScaleTable[(b >> 6) & 3];
        MixWithGainRamp(ppDst[b & 7], ppSrc[(b >> 3) & 7], scale * currentGain,
                        scale * step, numSamples);
    }
}

// -------------------------------------------------------------------------------------
// GainVectorLinearAmplitude @0x82B69938 -- linear gain ramp filler (see MixKernels.h).
//
// Shared skeleton (all three fillers):
//   last  (r26) = numSamples + startIndex - 1   -- last ramp position to render
//   limit (r28) = min(last, rampLength - 1)     -- last position still ON the ramp
//   startIndex < 0: out[0] = startGain, blkmov-replicate over the -startIndex leading
//   entries, clamp index to 0 and advance the output cursor.
//   Ramp loop: positions index+1 .. limit+1 (the asm evaluates the curve at i = pos+1).
//   Tail: positions past the ramp filled with endGain via the same blkmov replicate.
// The delta test is fcmpu/blt: NaN deltas take the fade-up body, hence !(delta < 0).
// -------------------------------------------------------------------------------------
void GainVectorLinearAmplitude(f32 *pGainVector, s32 numSamples, f32 startGain,
                               f32 endGain, s32 startIndex, s32 rampLength)
{
    s32 index = startIndex;                        // r31
    const s32 last = numSamples + startIndex - 1;  // r26
    f32 *out = pGainVector;                        // r4
    s32 limit = rampLength - 1;                    // r28
    const f32 delta = endGain - startGain;         // fsubs f31

    if (last <= limit)
        limit = last;

    if (startIndex < 0)
    {
        const s32 lead = -startIndex; // r29
        pGainVector[0] = startGain;
        BlkMovAscending(reinterpret_cast<u8 *>(pGainVector + 1),
                        reinterpret_cast<u8 *>(pGainVector),
                        (static_cast<u32>(4 * lead) - 1u) & ~3u);
        index = 0;
        out = pGainVector + lead;
    }

    const f32 rampLenF = static_cast<f32>(rampLength); // fcfid/frsp
    const f32 step = delta / rampLenF;                 // fdivs f13

    if (!(delta < 0.0f)) // fcmpu vs flt_82001CC0(0.0) ; blt -> fade-down
    {
        if (index <= limit)
        {
            s32 i = index + 1;             // r10
            s32 n = limit - index + 1;     // r11
            index = limit + 1;             // r31 += n
            do
            {
                *out++ = static_cast<f32>(i) * step + startGain; // fmadds
                ++i;
            } while (--n);
        }
    }
    else if (index <= limit)
    {
        s32 i = index + 1;
        s32 n = limit - index + 1;
        index = limit + 1;
        do
        {
            // fnmsubs: endGain - (rampLenF - i) * step.
            *out++ = endGain - (rampLenF - static_cast<f32>(i)) * step;
            ++i;
        } while (--n);
    }

    if (index <= last)
    {
        *out = endGain;
        BlkMovAscending(reinterpret_cast<u8 *>(out + 1), reinterpret_cast<u8 *>(out),
                        (static_cast<u32>(4 * (last - index)) + 3u) & ~3u);
    }
}

// -------------------------------------------------------------------------------------
// GainVectorLinearPower @0x82B69AB0 -- equal-power (sqrt) gain ramp filler. Identical
// skeleton; curve = sqrt(i) * delta/sqrt(rampLength) (fsqrts on the fcfid-converted
// integers).
// -------------------------------------------------------------------------------------
void GainVectorLinearPower(f32 *pGainVector, s32 numSamples, f32 startGain,
                           f32 endGain, s32 startIndex, s32 rampLength)
{
    s32 index = startIndex;
    const s32 last = numSamples + startIndex - 1;
    f32 *out = pGainVector;
    s32 limit = rampLength - 1;
    const f32 delta = endGain - startGain;

    if (last <= limit)
        limit = last;

    if (startIndex < 0)
    {
        const s32 lead = -startIndex;
        pGainVector[0] = startGain;
        BlkMovAscending(reinterpret_cast<u8 *>(pGainVector + 1),
                        reinterpret_cast<u8 *>(pGainVector),
                        (static_cast<u32>(4 * lead) - 1u) & ~3u);
        index = 0;
        out = pGainVector + lead;
    }

    const f32 rampLenF = static_cast<f32>(rampLength);
    // fsqrts of the fcfid-converted length, then fdivs.
    const f32 step = delta / std::sqrt(rampLenF);

    if (!(delta < 0.0f))
    {
        if (index <= limit)
        {
            s32 i = index + 1;
            s32 n = limit - index + 1;
            index = limit + 1;
            do
            {
                *out++ = std::sqrt(static_cast<f32>(i)) * step + startGain; // fsqrts/fmadds
                ++i;
            } while (--n);
        }
    }
    else if (index <= limit)
    {
        s32 i = index + 1;
        s32 n = limit - index + 1;
        index = limit + 1;
        do
        {
            // fsubs/fsqrts/fnmsubs: endGain - sqrt(rampLenF - i) * step.
            *out++ = endGain - std::sqrt(rampLenF - static_cast<f32>(i)) * step;
            ++i;
        } while (--n);
    }

    if (index <= last)
    {
        *out = endGain;
        BlkMovAscending(reinterpret_cast<u8 *>(out + 1), reinterpret_cast<u8 *>(out),
                        (static_cast<u32>(4 * (last - index)) + 3u) & ~3u);
    }
}

// -------------------------------------------------------------------------------------
// GainVectorSine @0x82B69C30 -- quarter-sine gain ramp filler. Identical skeleton;
// curve angle = i * PI/(2*rampLength) with PI = flt_8214AED4 (3.1415927f) and the CRT
// double `sin` (bl sin, frsp on the result).
// -------------------------------------------------------------------------------------
void GainVectorSine(f32 *pGainVector, s32 numSamples, f32 startGain,
                    f32 endGain, s32 startIndex, s32 rampLength)
{
    s32 index = startIndex;
    const s32 last = numSamples + startIndex - 1;
    f32 *out = pGainVector;
    s32 limit = rampLength - 1;
    const f32 delta = endGain - startGain; // fsubs f30

    if (last <= limit)
        limit = last;

    if (startIndex < 0)
    {
        const s32 lead = -startIndex;
        pGainVector[0] = startGain;
        BlkMovAscending(reinterpret_cast<u8 *>(pGainVector + 1),
                        reinterpret_cast<u8 *>(pGainVector),
                        (static_cast<u32>(4 * lead) - 1u) & ~3u);
        index = 0;
        out = pGainVector + lead;
    }

    const f32 rampLenF = static_cast<f32>(rampLength);         // fcfid/frsp f31
    const f32 angle = 3.1415927f / (rampLenF * 2.0f);          // fmuls/fdivs f29

    if (!(delta < 0.0f))
    {
        if (index <= limit)
        {
            s32 i = index + 1;
            s32 n = limit - index + 1;
            index = limit + 1;
            do
            {
                const f32 s = static_cast<f32>(
                    std::sin(static_cast<f64>(static_cast<f32>(i) * angle))); // bl sin/frsp
                *out++ = s * delta + startGain;                               // fmadds
                ++i;
            } while (--n);
        }
    }
    else if (index <= limit)
    {
        s32 i = index + 1;
        s32 n = limit - index + 1;
        index = limit + 1;
        do
        {
            const f32 s = static_cast<f32>(std::sin(
                static_cast<f64>((rampLenF - static_cast<f32>(i)) * angle)));
            *out++ = endGain - s * delta; // fnmsubs
            ++i;
        } while (--n);
    }

    if (index <= last)
    {
        *out = endGain;
        BlkMovAscending(reinterpret_cast<u8 *>(out + 1), reinterpret_cast<u8 *>(out),
                        (static_cast<u32>(4 * (last - index)) + 3u) & ~3u);
    }
}

} // namespace core
} // namespace audio
} // namespace rw
