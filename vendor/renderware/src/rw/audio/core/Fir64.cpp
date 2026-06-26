// =====================================================================================
// rw::audio::core::Fir64 -- polyphase FIR (finite-impulse-response) filter kernel bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store and every multiply-accumulate. Reconstructed solely from
// the disassembly at the @0x82xxxxxx addresses below; see Fir64.h for the byte-exact
// work-buffer layout.
//   ClearBuffer        @0x82B69808 -- store-for-store
//   CreateInstance     @0x82B6D688 -- store-for-store
//   GetSize            @0x82B6D678 -- store-for-store
//   HammingWindow      @0x82B69758 -- store-for-store (cos Hamming window)
//   MultiplyAccumulate @0x82B69828 -- the FIR convolution, lowered from the X360 VMX
//                                     vmaddfp MAC loop to a faithful scalar accumulate
//                                     over the committed coefficient/sample rows.
//   Filter             @0x82B6D6E0 -- the polyphase block driver, store-for-store.
// =====================================================================================

#include "rw/audio/core/Fir64.h"

#include <cmath>   // cos (the X360 cos)
#include <cstring> // memset / memcpy (the X360 XMemSet / XMemCpy)

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

// One source/destination channel-buffer node read out of the rwaudio Process graph: the
// channel-0 sample base at +0x04 and the 16-bit inter-channel stride at +0x0E (the asm
// `lwz r,4(node)` / `lhz r,0xE(node)`). Same node shape Filter and RawPuller2 read.
struct ChannelNode
{
    char *mpReserved0; // +0x00
    f32  *mpBase;      // +0x04 -- channel/phase-0 sample base
    char  mGap08[0x0E - 0x08];
    u16   mu16Stride;  // +0x0E -- inter-phase stride in samples
};

// The rwaudio Process context (IDA's `a2`). Filter touches the two ping-pong channel
// slots at +0x3000C / +0x30010 (the same slots the Iir2/RawPuller2 graph nodes use).
enum
{
    KU_CTX_SLOT_SRC = 0x3000C, // 196620
    KU_CTX_SLOT_DST = 0x30010  // 196624
};
} // namespace

// -------------------------------------------------------------------------------------
// GetSize @0x82B6D678
//   4*(numTaps*numPhases) + 8 -- the (numPhases*numTaps) f32 scratch buffer plus the
//   8-byte header (the buffer starts 8-aligned just past the header).
// -------------------------------------------------------------------------------------
int Fir64::GetSize(int numPhases, int numTaps)
{
    return 4 * (numPhases * numTaps + 2);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82B6D688
// Place an 8-aligned scratch buffer just past the 8-byte header, record its relative
// offset at +0x00, store the per-phase byte stride (4*numTaps) at +0x02, the tap count at
// +0x04 and the phase count at +0x06, then zero the (numPhases*numTaps) f32 buffer.
// (IDA renders the leading argument unused; r4=numPhases, r5=numTaps, r6=self.)
// -------------------------------------------------------------------------------------
Fir64 *Fir64::CreateInstance(int /*unused*/, int numPhases, int numTaps, Fir64 *self)
{
    char *base = reinterpret_cast<char *>(self);

    // Scratch buffer: first 8-aligned address >= self + 15 (the asm `addi r,self,0xF;
    // clrrwi r,r,3`). uintptr_t keeps the X360 32-bit align-mask semantics on the host.
    char *buffer = reinterpret_cast<char *>(
        (reinterpret_cast<uintptr_t>(base) + 15u) & ~static_cast<uintptr_t>(7u));

    self->muBufferOffset = static_cast<u16>(buffer - base);   // sth r,0(self)
    XMemSet(buffer, 0, 4u * numPhases * numTaps);             // zero numPhases*numTaps f32
    self->muNumTaps   = static_cast<u16>(numTaps);            // sth r,4(self)
    self->mbNumPhases = static_cast<u8>(numPhases);           // stb r,6(self)
    self->mu4xNumTaps = static_cast<u16>(4 * numTaps);        // sth r,2(self)
    return self;
}

// -------------------------------------------------------------------------------------
// ClearBuffer @0x82B69808
// Re-zero the scratch buffer: numPhases * (4*numTaps) bytes starting at the recorded
// relative offset (asm: XMemSet(self + *self, 0, *(self+6) * self[1])).
// -------------------------------------------------------------------------------------
void *Fir64::ClearBuffer(Fir64 *self)
{
    char *buffer = reinterpret_cast<char *>(self) + self->muBufferOffset;
    XMemSet(buffer, 0, static_cast<u32>(self->mbNumPhases) * self->mu4xNumTaps);
    return buffer;
}

// -------------------------------------------------------------------------------------
// HammingWindow @0x82B69758
// Window the design coefficients in place:
//   coeff[i] *= 0.54 - 0.46*cos(2*pi*i / numTaps)    for i in 0 .. numTaps/2
// The asm runs numTaps/2 + 1 iterations (the window is symmetric, so only the first half
// plus the centre tap are touched). Constants are the standard Hamming pair (flt_8214B1DC
// = 0.54, flt_8214B1D8 = 0.46) and 2*pi (flt_82001C94 = 6.2831855). The leading argument
// is forwarded unused; r4 = coeff pointer, r5 = numTaps.
// -------------------------------------------------------------------------------------
void *Fir64::HammingWindow(void * /*unused*/, f32 *coeffs, int numTaps)
{
    const f32 KF_HAMMING_A0 = 0.54000002f; // flt_8214B1DC
    const f32 KF_HAMMING_A1 = 0.46000001f; // flt_8214B1D8
    const f32 KF_TWO_PI     = 6.2831855f;  // flt_82001C94

    const int count = numTaps / 2 + 1; // asm: srawi/addze (numTaps>>1) then +1
    const f32 fNumTaps = static_cast<f32>(numTaps); // frsp(fcfid(numTaps))

    for (int i = 0; i < count; ++i)
    {
        // The asm forms the angle in single precision (fmuls/fdivs) before the cos call.
        const f32 angle = (static_cast<f32>(i) * KF_TWO_PI) / fNumTaps;
        // fnmsubs f0,cos,0.46,0.54 == 0.54 - 0.46*cos(angle); then fmuls f0,f0,coeff.
        const f32 window = KF_HAMMING_A0 - KF_HAMMING_A1 * static_cast<f32>(std::cos(static_cast<f64>(angle)));
        coeffs[i] = window * coeffs[i];
    }
    return coeffs;
}

// -------------------------------------------------------------------------------------
// MultiplyAccumulate @0x82B69828 -- the polyphase FIR convolution.
//
// For each of `count` outputs, the X360 kernel accumulates sum_k coeffs[k]*samples[k]
// over the 64-tap window. The X360 body is a VMX vmaddfp (a*b+c) multiply-accumulate
// chain unrolled into two tap groups; lowered here to a faithful scalar accumulate that
// reads the SAME committed coefficient row (`pCoeffs`) and sample row (`pSamples`) at the
// SAME byte offsets / strides the asm uses, and writes each sum to `pOut` (asm: stfs
// f0,0(r4)). Not a no-op -- the convolution arithmetic is reproduced fmadds-for-fmadds.
//
// Register mapping (IDA drops the trailing count): r3=self, r4=pOut, r5=pSamples (the a3
// row), r6=pCoeffs (the a4 row), r7=count. The kernel derives, once:
//   numTaps   = self->muNumTaps                          (lhz r11,4(r3))
//   pA3Window = pSamples - 39 floats   (a3 row window;   addi r9,r5,-0x9C; +1/output)
//   pA4Group1 = pCoeffs  + 1 float     (a4 group-1 base; addi r3,r6,4)
//   pA4Group2 = pCoeffs  + (numTaps-39) floats (a4 group-2 base; add r6,4*(numTaps-39),r6)
// then per output walks two unrolled fmadds groups (11 x 3-tap, then 4 x 8-tap) over those
// rows, advancing the a3 window by one float per output.
// -------------------------------------------------------------------------------------
Fir64 *Fir64::MultiplyAccumulate(Fir64 *self, f32 *pOut, const f32 *pSamples,
                                 const f32 *pCoeffs, int count)
{
    if (count <= 0)
        return reinterpret_cast<Fir64 *>(const_cast<f32 *>(pCoeffs) + 1); // r3 = pCoeffs+4

    const int numTaps = self->muNumTaps; // lhz r11,4(r3)

    // The two operand rows (asm: r3=pOut/a2, r5=pA3, r6=pA4). Group-1 walks the a3 row
    // forward by 3 and the a4 row back by 3; group-2 walks both rows back by 8. The
    // products are identical to the X360 VMX vmaddfp (a*b+c) MAC chain.
    //   pA4Group1 = a4 + 1 float            (asm: r3 = r6+4; also the returned r3)
    //   pA4Group2 = a4 + (numTaps-39) f.     (asm: r6 += 4*(numTaps-39))
    //   pA3Window = a3 - 39 floats           (asm: r9 = r5 - 0x9C; advances +1/output)
    f32 *const pA4Group1 = const_cast<f32 *>(pCoeffs) + 1;          // r3 = r6+4 (also returned)
    const f32 *const pA4Group2 = pCoeffs + (numTaps - 39);         // r6 += 4*(numTaps-39)
    const f32 *pA3Window = pSamples - 39;                          // r9 (advances +1/output)

    f32 *out = pOut; // r4

    for (int n = count; n != 0; --n)
    {
        f32 acc = 0.0f; // flt_82001CC0 == 0.0 (fmr f0,f13 after lfs f13)

        // ---- group 1: 11 iterations, three taps each (asm fmadds chain f12/f10/f8) ----
        // a3 row starts at pA3Window+38 and steps back 3/iter (asm v10 = v6+38, v10 -= 3);
        // a4 row starts at pA4Group1 and steps forward 3/iter (asm v9 = r3, v9 += 3).
        const f32 *a3 = pA3Window + 38; // v10
        const f32 *a4 = pA4Group1;      // v9
        for (int k = 11; k != 0; --k)
        {
            const f32 prod = (a3[0] * a4[0]) + (a3[1] * a4[-1]) + (a3[-1] * a4[1]);
            a3 -= 3;
            a4 += 3;
            acc = prod + acc;
        }

        // ---- group 2: 4 iterations, eight taps each (asm fmadds chain over r10/r11) ----
        // a3 row starts at pA3Window (asm v13 = v6); a4 row at pA4Group2 (asm v14 = v7);
        // both step back 8/iter.
        const f32 *g2a3 = pA3Window; // v13 (== v6 base each iter)
        const f32 *g2a4 = pA4Group2; // v14 (== v7 base each iter)
        for (int k = 4; k != 0; --k)
        {
            const f32 prod = (g2a4[0] * g2a3[0])
                           + (g2a4[1] * g2a3[1])
                           + (g2a4[-1] * g2a3[-1])
                           + (g2a4[2] * g2a3[2])
                           + (g2a4[3] * g2a3[3])
                           + (g2a4[4] * g2a3[4])
                           + (g2a4[6] * g2a3[6])
                           + (g2a4[5] * g2a3[5]);
            g2a4 -= 8;
            g2a3 -= 8;
            acc = prod + acc;
        }

        *out = acc;   // stfs f0,0(r4)
        ++pA3Window;  // ++v6 (advance the a3 window one float per output)
        ++out;        // ++a2
    }

    return reinterpret_cast<Fir64 *>(pA4Group1);
}

// -------------------------------------------------------------------------------------
// Filter @0x82B6D6E0 -- the polyphase block driver.
//
// Stage the scratch rows into the dst channel node, copy the src node into the scratch
// tail, run MultiplyAccumulate for each phase in two passes (the leading numTaps phases,
// then the trailing 256-numTaps phases, copying the second pass's tail back), then
// ping-pong the ctx src/dst channel-buffer slots. (r3=self, r4=ctx, r5=coeffs; IDA hides
// the register args behind the _savegpr prologue.)
//
// Per-phase sample pointer into a channel node = node.base + 4*node.stride*phase (bytes,
// i.e. base + stride*phase in f32). The scratch row stride is 4*numTaps bytes
// (self->mu4xNumTaps == ROL(self->muNumTaps,2)).
// -------------------------------------------------------------------------------------
Fir64 *Fir64::Filter(Fir64 *self, void *ctx, const f32 *coeffs)
{
    char *const ctxBase = reinterpret_cast<char *>(ctx);
    ChannelNode **const srcSlot = reinterpret_cast<ChannelNode **>(ctxBase + KU_CTX_SLOT_SRC); // v8
    ChannelNode **const dstSlot = reinterpret_cast<ChannelNode **>(ctxBase + KU_CTX_SLOT_DST); // v9
    ChannelNode *const srcNode = *srcSlot; // v13 (r27)
    ChannelNode *const dstNode = *dstSlot; // v14 (r29)

    f32 *const scratch = reinterpret_cast<f32 *>(
        reinterpret_cast<char *>(self) + self->muBufferOffset); // v12 = self + *self

    const u32 rowBytes  = self->mu4xNumTaps; // self[1] -- XMemCpy byte count per phase row
    const int rowFloats = self->muNumTaps;   // ROL(self[2],2)/4 -- scratch advance per phase
    const int numPhases = self->mbNumPhases; // self[6]
    const int numTaps   = self->muNumTaps;   // self[4]

    Fir64 *result = self;

    // ---- pass 1: stage the scratch rows out to dst, copy src rows into the scratch tail ----
    {
        f32 *row = scratch; // i
        for (int phase = 0; phase < numPhases; ++phase)
        {
            f32 *dstSamples = dstNode->mpBase + dstNode->mu16Stride * phase; // 4*stride*phase + base
            XMemCpy(dstSamples, row, rowBytes);

            f32 *srcSamples = srcNode->mpBase + srcNode->mu16Stride * phase;
            XMemCpy(reinterpret_cast<char *>(dstSamples) + rowFloats * 4, srcSamples, rowBytes);
            row += rowFloats;
        }
    }

    // ---- pass 2: MultiplyAccumulate the leading numTaps phases (count = numTaps) ----
    for (int phase = 0; phase < numPhases; ++phase)
    {
        f32 *out = dstNode->mpBase + dstNode->mu16Stride * phase; // v23
        result = MultiplyAccumulate(self, out, out + numTaps, coeffs, numTaps);
    }

    // ---- pass 3: MultiplyAccumulate the trailing (256-numTaps) phases + copy the tail ----
    // The MAC reads the src node's phase row (a3 = srcPtr + numTaps) and writes into the
    // dst node's phase row shifted by numTaps (out = base29 + stride29*phase + numTaps);
    // then the (256-numTaps)-shifted tail of the src row is staged back into the scratch.
    {
        f32 *scratchRow = scratch; // v12 / r28 (advances by numTaps floats per phase)
        for (int phase = 0; phase < numPhases; ++phase)
        {
            f32 *srcPtr = srcNode->mpBase + srcNode->mu16Stride * phase;            // r26 = base27 + stride27*phase
            f32 *out    = dstNode->mpBase + (dstNode->mu16Stride * phase + numTaps); // r4 = base29 + stride29*phase + numTaps
            MultiplyAccumulate(self, out, srcPtr + numTaps, coeffs, 256 - numTaps);

            // Stage the (256-numTaps)-shifted tail of the src row back into the scratch row.
            XMemCpy(scratchRow, srcPtr + (256 - numTaps), rowBytes);
            scratchRow += rowFloats;
        }
    }

    // ---- ping-pong the ctx src/dst channel-buffer slots ----
    ChannelNode *const tmp = *srcSlot;
    *srcSlot = *dstSlot;
    *dstSlot = tmp;

    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
