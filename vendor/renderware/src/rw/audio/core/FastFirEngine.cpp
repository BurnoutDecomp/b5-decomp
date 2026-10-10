// =====================================================================================
// rw::audio::core::FastFirEngine bodies -- the partitioned-convolution FIR engine behind
// the impulse-response reverb plug-in (rw::audio::core::ReverbIR1).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet-08 rwaudio PDB entry exist for this type. See FastFirEngine.h for the byte-exact
// layout and the per-function X360 addresses.
//
// Members: the constructor and destructor, Reset, SetChannels, Configure, Filter (the
// per-frame partition/FFT/MAC/IFFT driver), LoadDistributionCalc (the per-pass work
// split), MultiplyAccumulateComplex (the complex MAC kernel, hand-written VMX on the
// console) and EstimateLoad (the CPU-cost model ReverbIR1 budgets with).
// =====================================================================================

#include "rw/audio/core/FastFirEngine.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (the shared allocator @+0x14)

#include <cmath>   // std::fma / std::log (the cost model, the VMX fused lanes)
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

// The cost model's rodata (LoadDistributionCalc / EstimateLoad / MultiplyAccumulateComplex),
// dumped from the image.
const f32 KF_ONE = 1.0f;
const f32 KF_ZERO = 0.0f;
const f32 KF_HALF = 0.5f;
const f32 KF_PERCENT = 0.01f;          // 0x3C23D70A
const f32 KF_FFT_COST = 25.15f;        // 0x41C93333 -- per channel per (N log2 N)
const f32 KF_MAC_COST = 19.22f;        // 0x4199C28F -- per complex MAC sample
const f32 KF_OVERLAP_COST = 50.09f;    // 0x42485C29 -- per output channel per sample
const f64 KD_TWO = 2.0;                // the log base of EstimateLoad's log2

// Complex bins the MAC kernel processes per step (one 128-byte frequency group).
const s32 KI_MAC_BINS_PER_STEP = 16;
} // namespace

// -------------------------------------------------------------------------------------
// FastFirEngine::FastFirEngine -- clear only the running-state / handle slots the console
// writes (the sizing/pointer members are set later by Configure). Store order matches.
// -------------------------------------------------------------------------------------
FastFirEngine::FastFirEngine()
{
    mpFft = nullptr;    // +0x6C
    miField70 = 0;      // +0x70
    mpBuffer = nullptr; // +0x00
    miCurPass = 0;      // +0x5C
    miWriteBlock = 0;   // +0x30
    miPingA = 0;        // +0x64
    miPingB = 0;        // +0x68
    miFftDone = 0;      // +0x80
    miMacDone = 0;      // +0x84
    miIfftDone = 0;     // +0x88
}

// -------------------------------------------------------------------------------------
// FastFirEngine::~FastFirEngine -- release the scratch block through the System allocator
// and free the FFT context. Unlike Reset it neither nulls the slots nor clears the running
// state; it is the teardown path of ReverbIR1's deleting destructor.
// -------------------------------------------------------------------------------------
FastFirEngine::~FastFirEngine()
{
    if (mpBuffer)
        System::Free(off_83271928, mpBuffer, nullptr); // allocator->Free(buf, 0)

    if (mpFft)
        FFT_Free(&mpFft);
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

    // Straight to the System allocator's aligned Alloc (flags 1, align 16, offset 0); the
    // console does not go through System::Alloc here.
    void *buffer = off_83271928->mpAllocator->Alloc(totalBytes, "Reverb IR Buffer", 1, 16, 0);
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

                MultiplyAccumulateComplex(self, pFreq, pImp, pAcc);
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

// -------------------------------------------------------------------------------------
// FastFirEngine::LoadDistributionCalc -- split the frame's work across the miNumPasses
// sub-passes. Costs are in forward-FFT units: a forward or inverse FFT costs 1, the
// overlap-add costs outCh * 50.09 / ((log2 N - 1) * 25.15), and each MAC costs the share
// macTotal / numBlocks of macTotal = (1 - overlap%/100) * numBlocks * outCh * 19.22 /
// ((log2 N - 1) * 25.15). Each pass takes remaining / passesLeft, then greedily assigns the
// forward FFTs, then the MACs, then the inverse FFTs while at least half the next item's
// cost is left in its budget; whatever it did not spend carries over. The last pass picks up
// any inverse FFTs still unassigned. The counts accumulate onto the table Configure zeroed.
// (The comparisons keep the console's NaN behaviour.)
// -------------------------------------------------------------------------------------
void FastFirEngine::LoadDistributionCalc(FastFirEngine *self, s32 sizeLog2, s32 numBlocks)
{
    const f32 lfOutCh = static_cast<f32>(self->miOutputChannels);
    const f32 lfNumBlocks = static_cast<f32>(numBlocks);
    const f32 lfFftUnit = static_cast<f32>(sizeLog2 - 1) * KF_FFT_COST;
    const f32 lfDryShare = std::fma(-self->mfField60, KF_PERCENT, KF_ONE); // fnmsubs

    const f32 lfOverlapCost = (lfOutCh * KF_OVERLAP_COST) / lfFftUnit;
    const f32 lfMacTotal = ((lfDryShare * lfNumBlocks) * lfOutCh * KF_MAC_COST) / lfFftUnit;
    const f32 lfMacCost = lfMacTotal / lfNumBlocks;
    f32 lfRemaining =
        ((lfOverlapCost + static_cast<f32>(self->miInputChannels)) + lfOutCh) + lfMacTotal;

    s32 liFftAssigned = 0;
    s32 liMacAssigned = 0;
    s32 liIfftAssigned = 0;
    f32 lfNextCost = KF_ONE;

    for (s32 liPass = 0; liPass < self->miNumPasses; ++liPass)
    {
        LoadRecord &lrRecord = self->mpDist[liPass];
        const f32 lfBudget = lfRemaining / static_cast<f32>(self->miNumPasses - liPass);
        f32 lfLeft = lfBudget;

        if (!(lfBudget < lfNextCost * KF_HALF))
        {
            do
            {
                if (liFftAssigned < self->miInputChannels)
                {
                    ++liFftAssigned;
                    lfLeft = lfLeft - KF_ONE;
                    ++lrRecord.miFftCount;
                    if (!(liFftAssigned < self->miInputChannels))
                        lfNextCost = lfMacCost;
                }
                else if (liMacAssigned < numBlocks)
                {
                    ++liMacAssigned;
                    lfLeft = lfLeft - lfMacCost;
                    ++lrRecord.miMacCount;
                    if (!(liMacAssigned < numBlocks))
                        lfNextCost = KF_ONE;
                }
                else if (liIfftAssigned < self->miOutputChannels)
                {
                    ++liIfftAssigned;
                    lfLeft = lfLeft - KF_ONE;
                    ++lrRecord.miIfftCount;
                }
                else
                {
                    lfLeft = KF_ZERO;
                }
            } while (lfLeft >= lfNextCost * KF_HALF);
        }

        lfRemaining = lfRemaining - (lfBudget - lfLeft);

        if (liPass == self->miNumPasses - 1 && liIfftAssigned < self->miOutputChannels)
        {
            LoadRecord &lrLast = self->mpDist[self->miNumPasses - 1];
            lrLast.miIfftCount = lrLast.miIfftCount - liIfftAssigned + self->miOutputChannels;
        }
    }
}

// -------------------------------------------------------------------------------------
// FastFirEngine::MultiplyAccumulateComplex -- acc[k] += (imp[k] * scale) * freq[k] over
// complex bins, 16 bins per step, miField44 / 32 steps (signed division).
//
// The console kernel loads eight 16-byte vectors of each stream per step, widens the
// 16-bit impulse halves (vupkhsh / vupklsh + vcfsx), scales them by the splatted 1/imp[0]
// (fdivs), splits real and imaginary lanes with two cached vperm controls, multiplies, and
// re-interleaves with two more before the accumulate. Every lane runs the same operations,
// so the per-bin form below is bit-exact with it:
//   re = Ir*Fr - Ii*Fi          (two vmulfp128, one vsubfp)
//   im = fma(Ir, Fi, Ii*Fr)     (vmulfp128 then a fused vmaddfp)
//   acc += {re, im}             (vaddfp)
// The four permute controls (00010203 08090A0B 10111213 18191A1B: even lanes;
// 04050607 0C0D0E0F 14151617 1C1D1E1F: odd lanes; and the two merges back) are built from
// immediates and cached in function statics under a bit mask; as index arithmetic they
// need no table. The dcbt prefetches are cache hints only.
// -------------------------------------------------------------------------------------
void FastFirEngine::MultiplyAccumulateComplex(FastFirEngine *self, f32 *pFreq, s16 *pImpulse,
                                              f32 *pAcc)
{
    const f32 lfScale = KF_ONE / static_cast<f32>(pImpulse[0]);
    const s16 *lpBins = pImpulse + 8; // the bins start 16 bytes into the partition

    for (s32 liStep = self->miField44 / 32; liStep > 0; --liStep)
    {
        for (s32 liBin = 0; liBin < KI_MAC_BINS_PER_STEP; ++liBin)
        {
            const f32 lfIr = static_cast<f32>(lpBins[2 * liBin]) * lfScale;
            const f32 lfIi = static_cast<f32>(lpBins[2 * liBin + 1]) * lfScale;
            const f32 lfFr = pFreq[2 * liBin];
            const f32 lfFi = pFreq[2 * liBin + 1];

            const f32 lfRe = lfIr * lfFr - lfIi * lfFi;
            const f32 lfIm = std::fma(lfIr, lfFi, lfIi * lfFr);
            pAcc[2 * liBin] = lfRe + pAcc[2 * liBin];
            pAcc[2 * liBin + 1] = lfIm + pAcc[2 * liBin + 1];
        }
        lpBins += 2 * KI_MAC_BINS_PER_STEP;
        pFreq += 2 * KI_MAC_BINS_PER_STEP;
        pAcc += 2 * KI_MAC_BINS_PER_STEP;
    }
}

// -------------------------------------------------------------------------------------
// FastFirEngine::EstimateLoad -- with N = blockSize:
//   fftUnit = (N * log(N)) / log(2)                      (N log2 N, the double CRT log)
//   cost    = outCh*N*50.09 + outCh*fftUnit*25.15
//           + ((impulseSamples/N)*macLen*outCh)*19.22 + inCh*fftUnit*25.15
//   return cost / (N / frameLen)                          (integer passes)
// The three weighted terms accumulate through fused multiply-adds in the order written
// in the body. The compiler's divide-by-zero / overflow traps on frameLen are expressed by
// the division.
// -------------------------------------------------------------------------------------
f32 FastFirEngine::EstimateLoad(FastFirEngine *self, s32 macLen, s32 blockSize,
                                s32 impulseSamples, s32 frameLen)
{
    const f32 lfN = static_cast<f32>(blockSize);
    const f32 lfNLogN = lfN * static_cast<f32>(std::log(static_cast<f64>(lfN)));
    const f32 lfFftUnit = lfNLogN / static_cast<f32>(std::log(KD_TWO));
    const s32 liPasses = blockSize / frameLen;

    const f32 lfOutCh = static_cast<f32>(self->miOutputChannels);
    const f32 lfInCh = static_cast<f32>(self->miInputChannels);

    const f32 lfInFft = (lfInCh * lfFftUnit) * KF_FFT_COST;
    const f32 lfMacs = ((static_cast<f32>(impulseSamples) / lfN) * static_cast<f32>(macLen))
                     * lfOutCh;

    f32 lfCost = std::fma(lfMacs, KF_MAC_COST, lfInFft);
    lfCost = std::fma(lfOutCh * lfFftUnit, KF_FFT_COST, lfCost);
    lfCost = std::fma(lfOutCh * lfN, KF_OVERLAP_COST, lfCost);
    return lfCost / static_cast<f32>(liPasses);
}

} // namespace core
} // namespace audio
} // namespace rw
