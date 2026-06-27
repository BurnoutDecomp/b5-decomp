#pragma once

// =====================================================================================
// rw::audio::core::Fir64 -- the shared polyphase FIR (finite-impulse-response) filter
// kernel home, the FIR counterpart of the single-biquad Iir2 family (see Iir2.h).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every member offset and every multiply-accumulate. Every offset and
// constant below is grounded directly in the disassembly of the bodied members (see
// Fir64.cpp). Layout grounded in:
//   CreateInstance @0x82B6D688   (lays out the header + an 8-aligned coeff/sample buffer)
//   GetSize        @0x82B6D678   (4*(numTaps*numPhases) + 8-byte header)
//   ClearBuffer    @0x82B69808   (zeros numPhases * (4*numTaps) bytes of the buffer)
//   HammingWindow  @0x82B69758   (in-place Hamming window over the design taps)
//   MultiplyAccumulate @0x82B69828 (the polyphase FIR convolution sum tap[k]*sample[k])
//   Filter         @0x82B6D6E0   (the polyphase block driver around MultiplyAccumulate)
//
// Fir64 is the kernel shared by the concrete shapes BandPassFir64 / HighPassFir64 /
// LowPassFir64 (each calls CreateInstance/GetSize/ClearBuffer/HammingWindow/Filter). Those
// shapes design 64-tap sinc coefficients, window them with HammingWindow, and run the
// polyphase MultiplyAccumulate through Filter from their Process() entries. The shapes are
// modelled in their own TUs; only the shared kernel + its work-buffer header live here.
//
// The work-buffer header (the first 8 bytes of a Fir64 instance) records the relative
// offset to an 8-aligned scratch buffer of (numPhases * numTaps) f32 and the tap/phase
// dimensions. HammingWindow windows the design coefficients in place; MultiplyAccumulate
// convolves a tap row against a sample row; Filter drives the per-phase passes and
// ping-pongs the two ctx channel-buffer slots (the same +0x3000C / +0x30010 slots the
// rwaudio Process graph uses; see Iir2Filters.h / RawPuller2.cpp).
// =====================================================================================

#include "types.hpp" // f32, s32, u16, u32, u8

namespace rw
{
namespace audio
{
namespace core
{

class Fir64
{
public:
    // 64-tap design used by every concrete shape (HammingWindow/Filter are invoked with
    // a count of 64; CreateInstance is invoked with numTaps == 64).
    enum { KI_NUM_TAPS = 64 };

    // ---- the 8-byte work-buffer header at +0x00 of a Fir64 instance ----
    // FLAG (rwaudio PDB reconcile): names from the NFS ProStreet 08 X360 PDB
    // (class rw::audio::core::Fir64 [sizeof = 8]); layout/order/offsets/types all match
    // the ARTIST reconstruction. The "numTaps/numPhases" semantics in the asm map onto the
    // PDB's filter-order/channels naming (mFilterOrder == numTaps, mChannels == numPhases,
    // mHistoryBytesPerChannel == 4*numTaps, mInHistoryOffset == scratch-buffer offset).
    // CreateInstance writes:
    //   +0x00  mInHistoryOffset        = (alignedBuffer - self)   (relative byte offset)
    //   +0x02  mHistoryBytesPerChannel = 4 * numTaps              (one phase row's byte stride)
    //   +0x04  mFilterOrder            = numTaps
    //   +0x06  mChannels               = numPhases
    // followed (at the 8-aligned offset recorded in mInHistoryOffset) by a scratch buffer of
    // (numPhases * numTaps) f32, zeroed by CreateInstance and re-zeroed by ClearBuffer.
    u16 mInHistoryOffset;        // +0x00 -- relative byte offset to the 8-aligned scratch buffer
    u16 mHistoryBytesPerChannel; // +0x02 -- 4 * numTaps (per-phase byte stride)
    u16 mFilterOrder;            // +0x04 -- tap count (== 64 for every shape)
    u8  mChannels;               // +0x06 -- phase count (the per-phase loop bound)
    u8  mbPad07;                 // +0x07 -- header padding (PDB: 1 byte trailing padding)

    // -------------------------------------------------------------------------------------
    // Zero the (numPhases * numTaps) f32 scratch buffer. No standalone X360 export exists
    // (the call name rw::audio::core::Fir64::ClearBuffer is folded into its call sites);
    // each shape's Process clears the running buffer when the filter turns inactive.
    // -------------------------------------------------------------------------------------
    static void *ClearBuffer(Fir64 *self); // @0x82B69808

    // -------------------------------------------------------------------------------------
    // Lay out a Fir64 instance over `self`: place an 8-aligned scratch buffer just past the
    // header (recording its relative offset), store the tap/phase dimensions, and zero the
    // buffer. (IDA renders the leading argument unused; the asm register usage --
    // r4=numPhases, r5=numTaps, r6=self -- is authoritative.)
    // -------------------------------------------------------------------------------------
    static Fir64 *CreateInstance(int unused, int numPhases, int numTaps, Fir64 *self); // @0x82B6D688

    // -------------------------------------------------------------------------------------
    // Bytes needed for a Fir64 of `numPhases` phases and `numTaps` taps:
    //   4*(numTaps*numPhases) + 8   (the scratch buffer plus the 8-byte header).
    // -------------------------------------------------------------------------------------
    static int GetSize(int numPhases, int numTaps); // @0x82B6D678

    // -------------------------------------------------------------------------------------
    // Window the design coefficients in place with a Hamming window:
    //   coeff[i] *= 0.54 - 0.46*cos(2*pi*i / numTaps)     for i in 0 .. numTaps/2
    // (Symmetric window; only the first half + centre tap are touched, matching the asm's
    // numTaps/2 + 1 iteration count.) The first argument is forwarded unused; the asm reads
    // r4 = coeff pointer, r5 = numTaps.
    // -------------------------------------------------------------------------------------
    static void *HammingWindow(void *unused, f32 *coeffs, int numTaps); // @0x82B69758

    // -------------------------------------------------------------------------------------
    // The polyphase FIR convolution: for each of `count` outputs, accumulate
    // sum_k coeffs[k] * samples[k] over the 64-tap window, walking the coefficient row
    // (`pCoeffs`) and the sample row (`pSamples`) at the exact strides the X360 VMX MAC
    // uses, and store each sum to `pOut`. (IDA drops the trailing count argument; the asm
    // register usage -- r3=self, r4=pOut, r5=pSamples, r6=pCoeffs, r7=count -- is
    // authoritative.)
    // -------------------------------------------------------------------------------------
    static Fir64 *MultiplyAccumulate(Fir64 *self, f32 *pOut, const f32 *pSamples,
                                     const f32 *pCoeffs, int count); // @0x82B69828

    // -------------------------------------------------------------------------------------
    // The polyphase block driver: stage the input samples into the per-phase scratch rows,
    // run MultiplyAccumulate for each phase (two passes -- the leading numTaps phases, then
    // the trailing 256-numTaps phases), and ping-pong the ctx src/dst channel-buffer slots.
    // (IDA renders the register args via the _savefpr/_savegpr prologue; r3=self, r4=ctx,
    // r5=coeffs is authoritative.)
    // -------------------------------------------------------------------------------------
    static Fir64 *Filter(Fir64 *self, void *ctx, const f32 *coeffs); // @0x82B6D6E0
};

} // namespace core
} // namespace audio
} // namespace rw
