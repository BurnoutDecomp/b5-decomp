#pragma once

// =====================================================================================
// rw::audio::core sample-buffer mixing kernels -- the free-function DSP primitives the
// rwaudio Process graph plug-ins (Gain, GainFader, Send, Rechannel, SubMix, Dac,
// MatrixPanner, Pan2D/Pan2D1, the MP3 decoders) drive their per-frame work through.
//
// EARenderWare "rwaudio" (SDK package rwaudiocore 2.11.00 -- the Feb-2007 leak carries
// the package's public headers; these kernels live in the library's private channel
// translation unit, which is NOT in the leak, so every body is reconstructed from
// BURNOUT_X360_ARTIST.XEX asm). Constants that ARE in the leaked headers and match the
// asm: MIXER_FRAME_SIZE == 256, GAIN_DECLICK_FRAME_SIZE == 64, MAX_CHANNELS == 6
// (rw/audio/core/base.h, channel.h).
//
// The X360 bodies of CopyWithGain / CopyWithGainRamp / MixWithGain / MixWithGainRamp /
// ScaleSamples are VMX128 block kernels (lvx128/vmulfp128/vmaddfp/stvx128 over 32-float
// blocks) with scalar fallbacks for unaligned buffers. Per the committed family idiom
// (Fir64::MultiplyAccumulate), the VMX paths are lowered to faithful scalar loops that
// produce the identical per-sample arithmetic; the alignment split only selects the
// block width, never the values (see each body's comment in MixKernels.cpp).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. IDA prototypes
// carry dead register args (r5/r6 leftovers); those are omitted here, exactly as the
// committed AllPassFilterFunc declaration does (ReverbFilters.h).
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u16, u32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// CopyWithGain @0x82B67200 -- pDst[i] = pSrc[i] * gain for i in [0, numSamples).
// (IDA: (r3 dst, r4 src, f1 gain, r5 dead, r6 count).)
// -------------------------------------------------------------------------------------
void CopyWithGain(f32 *pDst, const f32 *pSrc, f32 gain, s32 numSamples);

// -------------------------------------------------------------------------------------
// CopyWithGainRamp @0x82B67400 -- de-clicked gain ramp:
//   pDst[i] = pSrc[i] * (lfGain + lfStep*i)      for i in [0, 64)   (GAIN_DECLICK_FRAME_SIZE)
//   pDst[i] = pSrc[i] * (lfGain + lfStep*64)     for i in [64, liCount & ~3)
// (r3 dst, r4 src, f1 gain, f2 step, r5/r6 dead, r7 count.) VMX-only on X360 (no scalar
// fallback; both buffers are mixer-owned 128-byte-aligned frames). Signature matches the
// committed consumer declaration in CgsGainArrayPlugin.cpp exactly.
// -------------------------------------------------------------------------------------
void CopyWithGainRamp(f32 *lpDst, f32 *lpSrc, float lfGain, float lfStep, int liCount);

// -------------------------------------------------------------------------------------
// MixWithGain @0x82B6A3F8 -- pDst[i] += pSrc[i] * gain.
// (r3 dst, r4 src, f1 gain, r5 dead, r6 count.)
// -------------------------------------------------------------------------------------
void MixWithGain(f32 *pDst, const f32 *pSrc, f32 gain, s32 numSamples);

// -------------------------------------------------------------------------------------
// MixWithGainRamp @0x82B6A568 -- accumulating variant of CopyWithGainRamp:
//   pDst[i] += pSrc[i] * (gain + step*i)     for i in [0, 64)
//   pDst[i] += pSrc[i] * (gain + step*64)    for i in [64, numSamples & ~3)
// (r3 dst, r4 src, f1 gain, f2 step, r5/r6 dead, r7 count.)
// -------------------------------------------------------------------------------------
void MixWithGainRamp(f32 *pDst, f32 *pSrc, f32 gain, f32 step, s32 numSamples);

// -------------------------------------------------------------------------------------
// ScaleSamples @0x82B6B708 -- pData[i] *= scale (in place). No-op when numSamples <= 0.
// (r3 data, f1 scale, r4 dead, r5 count.)
// -------------------------------------------------------------------------------------
void ScaleSamples(f32 *pData, f32 scale, s32 numSamples);

// -------------------------------------------------------------------------------------
// ClipFloats @0x82B64B68 -- clamp pData[i] into [minValue, maxValue] in place (only
// out-of-range samples are stored back). (r3 data, f1 min, f2 max, r4/r5 dead, r6 count.)
// -------------------------------------------------------------------------------------
void ClipFloats(f32 *pData, f32 minValue, f32 maxValue, s32 numSamples);

// -------------------------------------------------------------------------------------
// DeClick @0x82B676F8 -- fold the per-channel residual "click" values a SubMix carries
// (SubMixConnector::mDeClickValueTotal) into the first 16 samples of each channel of the
// mix buffer, with a linearly decaying (16-j)/17 coefficient per sample j, then zero the
// consumed click value. `pSampleBuffer` is the rwaudio channel-buffer node the kernels
// share (sample base @+0x04, 16-bit inter-channel stride @+0x0E -- the same node shape
// Fir64::Filter / RawPuller2 read); typed void* per the committed Fir64::Filter idiom.
// (r3 buffer node, r4 click values, r5 channel count.)
// -------------------------------------------------------------------------------------
void DeClick(void *pSampleBuffer, f32 *pDeClickValue, u32 numChannels);

// -------------------------------------------------------------------------------------
// ReOrderRwAudioCoreToWave @0x82B6B590 -- interleave the planar rwaudio channel buffer
// into WAV channel order:
//   1 ch: straight memcpy
//   2 ch: out = { ch0, ch1 }
//   4 ch: out = { ch0, ch1, ch2, ch3 }
//   6 ch: out = { ch0, ch2, ch1, ch5, ch3, ch4 }
// `pSampleBuffer` is the same channel-buffer node DeClick reads (base @+0x04, stride
// @+0x0E). (r3 dst, r4 buffer node, r5 channel count, r6 sample count.)
// -------------------------------------------------------------------------------------
void ReOrderRwAudioCoreToWave(f32 *pDst, void *pSampleBuffer, s32 numChannels,
                              s32 numSamples);

// -------------------------------------------------------------------------------------
// ReChannelGainWrite<S>x<D> @0x82B6ACA0..0x82B6B25C -- fixed source-to-destination
// channel-count remap kernels (S source channels into D destination channels).
// `ppDst`/`ppSrc` are per-channel sample-pointer arrays. Silent destination channels
// are memset to zero; centre-channel folds scale the gain by 0.707 (flt_82011C14 ==
// 0.70700002f). All (r3 dst array, r4 src array, [f1 gain,] r5 dead, r6 count).
// -------------------------------------------------------------------------------------
void ReChannelGainWrite1x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6ACA0
void ReChannelGainWrite1x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AD10
void ReChannelGainWrite2x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6ADA8
void ReChannelGainWrite2x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AE08
void ReChannelGainWrite4x1(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AE88
void ReChannelGainWrite4x2(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AEE8
void ReChannelGainWrite4x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AF50
void ReChannelGainWrite4x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6AFB0
void ReChannelGainWrite6x1(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6B030
void ReChannelGainWrite6x2(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6B0A0
void ReChannelGainWrite6x4(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6B148
void ReChannelGainWrite6x6(f32 **ppDst, f32 **ppSrc, f32 gain, s32 numSamples); // @0x82B6B1E8

// -------------------------------------------------------------------------------------
// ReChannelGainWrite @0x82B6B268 -- the channel-remap dispatcher. Routes every
// {numSrcChannels, numDstChannels} pair in {1,2,4,6}x{1,2,4,6} to its fixed kernel
// (plus inline 1x1/1x2/2x1/2x2 forms), and falls back for other pairs to a straight
// per-channel CopyWithGain (zero-filling any surplus destination channels).
// (r3 dst array, r4 src array, r5 dead, r6 dst channels, r7 src channels, r8 count,
//  f1 gain.)
// -------------------------------------------------------------------------------------
void ReChannelGainWrite(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                        u32 numSrcChannels, s32 numSamples, f32 gain);

// -------------------------------------------------------------------------------------
// ReChannelGainMix @0x82B6AB48 -- table-driven accumulate remap (Send::Process): for
// each routing descriptor of the {src,dst} channel-count pair, MixWithGain the encoded
// source channel into the encoded destination channel with gain scaled by the encoded
// gain-scale slot. Descriptor byte b: dst = b & 7, src = (b >> 3) & 7,
// scale = gScaleTable[(b >> 6) & 3]. (r3 dst array, r4 src array, r5 dead,
// r6 dst channels, r7 src channels, r8 count, f1 gain.)
// -------------------------------------------------------------------------------------
void ReChannelGainMix(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                      u32 numSrcChannels, s32 numSamples, f32 gain);

// -------------------------------------------------------------------------------------
// ReChannelGainMixRamp @0x82B6ABE8 -- ramped variant of ReChannelGainMix, stepping from
// currentGain to targetGain with step = (targetGain - currentGain) / 65
// (flt_8214B268 == 0.015384615f == 1/(GAIN_DECLICK_FRAME_SIZE + 1)) through
// MixWithGainRamp. (r3 dst array, r4 src array, r5/r6 dead, r7 dst channels,
// r8 src channels, r9 count, f1 target gain, f2 current gain.)
// -------------------------------------------------------------------------------------
void ReChannelGainMixRamp(f32 **ppDst, f32 **ppSrc, u32 numDstChannels,
                          u32 numSrcChannels, s32 numSamples, f32 targetGain,
                          f32 currentGain);

// -------------------------------------------------------------------------------------
// GainVector* @0x82B69938 / @0x82B69AB0 / @0x82B69C30 -- fill a per-sample gain vector
// for a GainFader ramp of `rampLength` samples, of which this call renders `numSamples`
// entries starting at ramp position `startIndex` (negative startIndex pre-fills the
// leading -startIndex entries with startGain). Positions past the ramp end are filled
// with endGain. The three shapes differ only in the ramp curve:
//   LinearAmplitude: gain(i) = startGain + i * (endGain-startGain)/rampLength
//   LinearPower:     gain(i) = startGain + sqrt(i) * (endGain-startGain)/sqrt(rampLength)
//   Sine:            gain(i) = startGain + sin(i * PI/(2*rampLength)) * (endGain-startGain)
// each with the mirrored "fade-down" form when endGain < startGain (the asm's two loop
// bodies). All return 1 on X360; reconstructed as void per the family's dead-return
// idiom. (r3 vector, r4 count, f1 start, f2 end, r5/r6 dead, r7 start index,
// r8 ramp length.)
// -------------------------------------------------------------------------------------
void GainVectorLinearAmplitude(f32 *pGainVector, s32 numSamples, f32 startGain,
                               f32 endGain, s32 startIndex, s32 rampLength);
void GainVectorLinearPower(f32 *pGainVector, s32 numSamples, f32 startGain,
                           f32 endGain, s32 startIndex, s32 rampLength);
void GainVectorSine(f32 *pGainVector, s32 numSamples, f32 startGain,
                    f32 endGain, s32 startIndex, s32 rampLength);

} // namespace core
} // namespace audio
} // namespace rw
