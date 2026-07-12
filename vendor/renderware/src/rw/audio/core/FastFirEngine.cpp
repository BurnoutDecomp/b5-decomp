// =====================================================================================
// rw::audio::core::FastFirEngine bodies -- the partitioned-convolution FIR engine behind
// the impulse-response reverb plug-in (rw::audio::core::ReverbIR1).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet-08 rwaudio PDB entry exist for this type. See FastFirEngine.h for the byte-exact
// layout and the per-function X360 addresses.
//
// RECONSTRUCTED HERE (grounded store-for-store in the disassembly):
//   FastFirEngine_ctor   @0x82B68120   clear the running-state slots
//   FastFirEngine_dtor   @0x82B6E758   free the scratch block + the FFT context
//   Reset                @0x82B6E7C0   dtor's frees, plus zero the running state
//   SetChannels          @0x82B68150   record the two channel counts
//   Configure            @0x82B6F048   size + carve the one scratch block, alloc the FFT
//   Filter               @0x82B6D068   the per-frame partition/FFT/MAC/IFFT/overlap-add driver
//
// THREE ledger functions in this TU are only DECLARED (bodies omitted, not stubbed):
//   * MultiplyAccumulateComplex @0x82B684C0 -- a hand-written VMX complex MAC kernel: its
//       body is entirely `vperm`/`vmulfp128`/`vmaddfp` driven by permute-control vectors
//       loaded from un-recovered rodata (unk_83271C80/90/A0/B0) and cached under a bitmask
//       into dword_83271CC0. The permute tables are not in the dossier, so a faithful
//       reconstruction is impossible -- BLOCKED rather than guessed.
//   * LoadDistributionCalc @0x82B682A0 -- fills the per-pass LoadRecord table from a float
//       cost model. Its Hex-Rays output is flagged "local variable allocation has failed,
//       the output may be wrong!" and tangles an __int64/double greedy accumulator over
//       rodata coefficients (flt_8214AFB4/B8/BC) whose exact values are NOT expanded in this
//       function's dossier. A wrong distribution silently corrupts Filter's per-pass counts,
//       so the body is BLOCKED.
//   * EstimateLoad @0x82B68168 -- a CPU-cost estimate that multiplies four un-attested rodata
//       float coefficients (flt_8214AFB4/B8/BC, dbl_82047D50) not expanded in its dossier;
//       BLOCKED rather than fabricated.
// The bodied functions below reference the three only through their declarations, so this TU
// compiles under the per-TU gate.
// =====================================================================================

#include "rw/audio/core/FastFirEngine.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (the shared allocator @+0x14)

#include <cstring> // memset / memcpy (the X360 XMemSet / XMemCpy)

namespace rw
{
namespace audio
{
namespace core
{

// The shared rwaudio System singleton (off_83271928); its ICoreAllocator (slot +0x14) backs
// the one scratch allocation this engine makes. Defined/owned by the System TU. Configure /
// Reset / the destructor reach it exactly as ReverbModel1.cpp does.
extern "C" System *off_83271928;

namespace
{
// XMemSet / XMemCpy @ the X360 build == byte-wise memset / memcpy. They return the
// destination pointer (like the C library), matching the asm's r3 carry-out.
inline void *XMemSet(void *dst, int value, u32 bytes) { return std::memset(dst, value, bytes); }
inline void *XMemCpy(void *dst, const void *src, u32 bytes) { return std::memcpy(dst, src, bytes); }

// flt_8214AED0 == 100.0f -- Configure scales the partition-overlap ratio into a percentage.
const f32 KF_HUNDRED = 100.0f;
} // namespace

// -------------------------------------------------------------------------------------
// FastFirEngine::FastFirEngine @0x82B68120 -- clear only the running-state / handle slots the
// asm writes (the sizing/pointer members are set later by Configure). Store order matches.
// -------------------------------------------------------------------------------------
FastFirEngine *FastFirEngine::FastFirEngine_ctor(FastFirEngine *self)
{
    self->mpFft = nullptr;    // stw 0 @ +0x6C
    self->miField70 = 0;      // stw 0 @ +0x70
    self->mpBuffer = nullptr; // stw 0 @ +0x00
    self->miCurPass = 0;      // stw 0 @ +0x5C
    self->miWriteBlock = 0;   // stw 0 @ +0x30
    self->miPingA = 0;        // stw 0 @ +0x64
    self->miPingB = 0;        // stw 0 @ +0x68
    self->miFftDone = 0;      // stw 0 @ +0x80
    self->miMacDone = 0;      // stw 0 @ +0x84
    self->miIfftDone = 0;     // stw 0 @ +0x88
    return self;
}

// -------------------------------------------------------------------------------------
// FastFirEngine::~FastFirEngine @0x82B6E758 -- release the scratch block through the System
// allocator and free the FFT context. (Unlike Reset it does not null the slots or clear the
// running state -- it is the teardown path from ReverbIR1's scalar-deleting destructor.)
// -------------------------------------------------------------------------------------
void *FastFirEngine::FastFirEngine_dtor(FastFirEngine *self)
{
    if (self->mpBuffer)
        System::Free(off_83271928, self->mpBuffer, nullptr); // allocator->Free(buf, 0)

    if (self->mpFft)
        return FFT_Free(&self->mpFft);
    return &self->mpFft; // asm returns r3 = &mpFft when the handle is already null
}

// -------------------------------------------------------------------------------------
// FastFirEngine::Reset @0x82B6E7C0 -- free + null the scratch block and the FFT context, then
// zero the per-frame running state so the engine can be re-Configured or restarted cleanly.
// -------------------------------------------------------------------------------------
FastFirEngine *FastFirEngine::Reset(FastFirEngine *self)
{
    void *result = self;

    if (self->mpBuffer)
    {
        System::Free(off_83271928, self->mpBuffer, nullptr);
        self->mpBuffer = nullptr;
    }
    if (self->mpFft)
    {
        result = FFT_Free(&self->mpFft);
        self->mpFft = nullptr;
    }

    self->miCurPass = 0;    // +0x5C
    self->miWriteBlock = 0; // +0x30
    self->miPingA = 0;      // +0x64
    self->miPingB = 0;      // +0x68
    self->miFftDone = 0;    // +0x80
    self->miMacDone = 0;    // +0x84
    self->miIfftDone = 0;   // +0x88

    (void)result;
    return self;
}

// -------------------------------------------------------------------------------------
// FastFirEngine::SetChannels @0x82B68150 -- record the input/output channel counts; ret 1.
// -------------------------------------------------------------------------------------
int FastFirEngine::SetChannels(FastFirEngine *self, s32 inputChannels, s32 outputChannels)
{
    self->miInputChannels = inputChannels;   // stw r4 @ +0x74
    self->miOutputChannels = outputChannels; // stw r5 @ +0x78
    return 1;
}

// -------------------------------------------------------------------------------------
// FastFirEngine::Configure @0x82B6F048 -- (re)build the engine for a `blockSize`-sample FFT
// partition scheme over an `impulseSamples`-long response:
//   * partition count       = ceil(impulseSamples / blockSize)         (+ its remainder)
//   * transform length       = round-up(2*blockSize + 2, 16)            (real FFT + guard)
//   * one contiguous scratch block holds, in order: the two input partitions, the frequency
//     ring, the convolution accumulator, the two output partitions, and the LoadRecord table.
// All arithmetic below is store-for-store from the asm; the only float, mfField60, is
// (blockSize-a4)/blockSize * 100.0 (flt_8214AED0). The dead compiler trap checks (__twllei /
// __twlgei divide guards) are dropped -- the divisions express them.
// -------------------------------------------------------------------------------------
int FastFirEngine::Configure(FastFirEngine *self, s32 channels, s32 blockSize, s32 a4, s32 a5,
                             s32 a6, s32 impulseSamples, s16 *pImpulse, char primeHistory)
{
    if (self->mpBuffer)
        Reset(self);

    // Partition the impulse response into blockSize-sample blocks; a partial tail becomes
    // its own (shorter) block.
    if (impulseSamples % blockSize)
    {
        self->miField34 = impulseSamples % blockSize;
        self->miNumBlocks = impulseSamples / blockSize + 1;
    }
    else
    {
        self->miField34 = blockSize;
        self->miNumBlocks = impulseSamples / blockSize;
    }

    self->miFrameLen = channels;   // +0x38 (samples delivered per Filter call)
    self->miBlockLen = blockSize;  // +0x3C
    self->miField50 = blockSize;   // +0x50

    const s32 fftClearLen = 2 * blockSize + 2;
    self->miFftClearLen = fftClearLen; // +0x40

    // mfField60 = ((blockSize - a4) / blockSize) * 100  (overlap percentage).
    self->mfField60 = (static_cast<f32>(blockSize - a4) / static_cast<f32>(blockSize)) * KF_HUNDRED;

    // Transform length rounded up to a multiple of 16.
    s32 v37 = fftClearLen / 16;
    if (fftClearLen % 16)
        ++v37;
    const s32 blockFftLen = 16 * v37;

    const s32 outputChannels = self->miOutputChannels; // v38 (a1[30])
    const s32 inputChannels = self->miInputChannels;   // v40 (a1[29])

    self->miField44 = a5;              // +0x44
    self->miField4C = a5;              // +0x4C
    self->miBlockStride = blockFftLen; // +0x48
    self->miBlockFftLen = blockFftLen; // +0x24
    self->miNumPasses = blockSize / channels; // +0x54

    // Byte spans of the sub-regions inside the single scratch block.
    const s32 numBlocks = self->miNumBlocks;
    const s32 numPasses = blockSize / channels;                        // v42
    const s32 outPartBytes = 8 * outputChannels * blockSize;           // v44
    const s32 inFreqBytes = 4 * inputChannels * blockFftLen;           // v45
    const s32 convBytes = 4 * outputChannels * blockFftLen;            // v46
    const s32 impFreqBytes = 4 * numBlocks * inputChannels * a5;       // v47

    self->miConvBytes = convBytes; // +0x20

    const u32 totalBytes = static_cast<u32>(
        2 * (6 * numPasses + inFreqBytes) + convBytes + outPartBytes + impFreqBytes); // v48

    void *buffer = System::Alloc(off_83271928, totalBytes, "Reverb IR Buffer", 16, 1);
    self->mpBuffer = buffer;
    XMemSet(buffer, 0, totalBytes);

    // Carve the sub-region bases (the asm walks a running byte cursor).
    char *cursor = static_cast<char *>(buffer);
    self->mpInput[0] = reinterpret_cast<f32 *>(cursor);
    cursor += inFreqBytes;
    self->mpInput[1] = reinterpret_cast<f32 *>(cursor);
    cursor += inFreqBytes;
    self->mpFreq = reinterpret_cast<f32 *>(cursor);
    cursor += impFreqBytes;
    self->mpConv = reinterpret_cast<f32 *>(cursor);
    cursor += convBytes;
    self->mpOutput[0] = reinterpret_cast<f32 *>(cursor);
    cursor += outPartBytes / 2;
    self->mpOutput[1] = reinterpret_cast<f32 *>(cursor);
    cursor += outPartBytes / 2;
    self->mpDist = reinterpret_cast<LoadRecord *>(cursor);

    // log2 of the (2*blockSize) transform size for the FFT allocation.
    s32 sizeLog2 = 0;
    for (s32 n = 2 * blockSize; n > 1; n /= 2)
        ++sizeLog2;

    if (!self->mpFft)
    {
        FFT_Alloc(sizeLog2, 0, &self->mpFft);
        FFT_Init(self->mpFft);
    }

    self->miField28 = a6;             // +0x28
    self->mpImpulse = pImpulse;       // +0x10
    const s32 impStride = a5 + 8;     // v60
    self->miField58 = impStride;      // +0x58

    // History-prime walk. In the X360 build this loop carries NO memory side effect (the
    // body was folded away by the compiler); reproduced faithfully for parity.
    if (primeHistory == 1)
    {
        const s32 guard = self->miNumBlocks * impStride * a6;
        if (guard > 0)
        {
            s16 *pWalk = pImpulse;
            s32 count = 0;
            do
            {
                ++count;
                pWalk += 2;
            } while (count < self->miField58 * self->miNumBlocks * self->miField28);
            (void)pWalk;
        }
    }

    LoadDistributionCalc(self, sizeLog2, self->miNumBlocks);
    return 1;
}

// -------------------------------------------------------------------------------------
// FastFirEngine::Filter @0x82B6D068 -- process one frame:
//   1. stage this frame's input samples into the current input partition slot;
//   2. run this pass's forward FFTs (zero-padding then transforming the fresh partitions);
//   3. run this pass's complex MACs (impulse-partition x frequency-partition -> accumulator);
//   4. run this pass's inverse FFTs of finished accumulators;
//   5. on the last pass: overlap-add the finished output partition, advance the write ring,
//      flip the double buffers, and reset the running counters;
//   6. copy the finished output partition out to the output node.
// r3=self, r4=input node, r5=output node. `result` mirrors the asm's r3 carry-out.
// -------------------------------------------------------------------------------------
void *FastFirEngine::Filter(FastFirEngine *self, ChannelNode *inNode, ChannelNode *outNode)
{
    void *result = self;

    // (1) stage input into mpInput[pingB].
    for (s32 i = 0; i < self->miInputChannels; ++i)
    {
        f32 *pDest = self->mpInput[self->miPingB]
                   + (self->miFrameLen * self->miCurPass + self->miBlockStride * i);
        const f32 *pSrc = inNode->mpBase + inNode->mu16Stride * i;
        result = XMemCpy(pDest, pSrc, static_cast<u32>(4 * self->miFrameLen));
    }

    // (2) forward FFTs for this pass.
    if (self->mpDist[self->miCurPass].miFftCount > 0)
    {
        s32 block = self->miFftDone;
        void *fft = self->mpFft;
        const s32 inSel = (self->miPingB == 0) ? 1 : 0; // mpInput[!pingB]
        do
        {
            f32 *pPart = self->mpInput[inSel] + self->miBlockStride * block;
            XMemSet(pPart + self->miBlockLen, 0,
                    static_cast<u32>(4 * (self->miFftClearLen - self->miBlockLen)));
            FFT_ForwardReal(fft, pPart);
            result = XMemCpy(self->mpFreq
                             + (self->miWriteBlock * self->miInputChannels + block) * self->miField4C,
                             pPart, static_cast<u32>(4 * self->miField44));
            ++block;
        } while (block < self->mpDist[self->miCurPass].miFftCount + self->miFftDone);
        self->miFftDone += self->mpDist[self->miCurPass].miFftCount;
    }

    // (3) complex multiply-accumulates for this pass.
    if (self->mpDist[self->miCurPass].miMacCount > 0)
    {
        for (s32 outCh = 0; outCh < self->miOutputChannels; ++outCh)
        {
            f32 *pAcc = self->mpConv + self->miBlockFftLen * outCh;
            if (self->miMacDone == 0)
                result = XMemSet(pAcc, 0, static_cast<u32>(4 * self->miBlockFftLen));

            s16 *pImp = self->mpImpulse; // recomputed below (kept across iters when guarded)
            f32 *pFreq = self->mpFreq;
            for (s32 tap = self->miMacDone;
                 tap < self->mpDist[self->miCurPass].miMacCount + self->miMacDone; ++tap)
            {
                s32 blockIdx = self->miWriteBlock - tap;
                if (blockIdx < 0)
                    blockIdx += self->miNumBlocks;

                // impulse-partition pointer (only re-derived on the first output channel when
                // miField28 == 1, else every iteration).
                if (self->miField28 == 1)
                {
                    if (outCh == 0)
                        pImp = self->mpImpulse + self->miField58 * tap;
                }
                else
                {
                    pImp = self->mpImpulse + (self->miField28 * tap + outCh) * self->miField58;
                }

                // frequency-partition pointer (same guard on miInputChannels).
                if (self->miInputChannels == 1)
                {
                    if (outCh == 0)
                        pFreq = self->mpFreq + self->miField4C * blockIdx;
                }
                else
                {
                    pFreq = self->mpFreq
                          + (self->miInputChannels * blockIdx + outCh) * self->miField4C;
                }

                result = MultiplyAccumulateComplex(self, pFreq, pImp, pAcc);
            }
        }
        self->miMacDone += self->mpDist[self->miCurPass].miMacCount;
    }

    // (4) inverse FFTs for this pass.
    if (self->mpDist[self->miCurPass].miIfftCount > 0)
    {
        s32 block = self->miIfftDone;
        void *fft = self->mpFft;
        for (; block < self->mpDist[self->miCurPass].miIfftCount + self->miIfftDone; ++block)
            result = FFT_InverseReal(fft, self->mpConv + self->miBlockFftLen * block);
        self->miIfftDone += self->mpDist[self->miCurPass].miIfftCount;
    }

    // (5) advance / finalize.
    if (self->miCurPass < self->miNumPasses - 1)
    {
        self->miCurPass = self->miCurPass + 1;
    }
    else
    {
        // Overlap-add the finished partition into the alternate output buffer and copy this
        // frame's fresh output into the current one.
        for (s32 outCh = 0; outCh < self->miOutputChannels; ++outCh)
        {
            f32 *pAlt = self->mpOutput[(self->miPingA == 0) ? 1 : 0]; // mpOutput[!pingA]
            f32 *pAcc = self->mpConv + self->miBlockFftLen * outCh;
            f32 *pCur = self->mpOutput[self->miPingA] + self->miBlockLen * outCh;
            f32 *pTail = pAlt + self->miBlockLen * outCh;
            for (s32 n = 0; n < self->miBlockLen; ++n)
            {
                pTail[n] = pAcc[n] + pTail[n];
                pCur[n] = pAcc[self->miBlockLen + n];
            }
        }

        self->miWriteBlock = self->miWriteBlock + 1;
        if (self->miWriteBlock >= self->miNumBlocks)
            self->miWriteBlock = 0;

        if (self->miPingA == 0)
        {
            self->miPingA = 1;
            self->miPingB = 1;
        }
        else
        {
            self->miPingA = 0;
            self->miPingB = 0;
        }

        self->miCurPass = 0;
        self->miFftDone = 0;
        self->miMacDone = 0;
        self->miIfftDone = 0;
    }

    // (6) copy the finished output partition out.
    for (s32 m = 0; m < self->miOutputChannels; ++m)
    {
        f32 *pDest = outNode->mpBase + outNode->mu16Stride * m;
        const f32 *pSrc = self->mpOutput[self->miPingA]
                        + (self->miFrameLen * self->miCurPass + self->miBlockLen * m);
        result = XMemCpy(pDest, pSrc, static_cast<u32>(4 * self->miFrameLen));
    }

    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
