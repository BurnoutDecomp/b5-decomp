#pragma once

// =====================================================================================
// rw::audio::core::FastFirEngine -- the partitioned-convolution FIR engine that drives
// the impulse-response reverb plug-in (rw::audio::core::ReverbIR1). It is the "fast FIR"
// counterpart of the single-block Fir64 kernel (see Fir64.h): where Fir64 runs one
// polyphase FIR block per call, FastFirEngine splits a long impulse response into
// `numBlocks` partitions, spreads each frame's FFT / complex-multiply-accumulate / IFFT
// work across `numPasses` sub-passes (uniform-partitioned overlap-save convolution), and
// double-buffers its input/output partitions between passes.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member store. No Feb-2007 leak source and no DecFIGS DWARF exist
// for this type, and it is absent from the ProStreet-08 rwaudio PDB, so every member below
// is grounded directly in the disassembly of the bodied members (FastFirEngine.cpp). Member
// NAMES are reconstructed to match observed semantics; the +0xNN annotations are the X360
// (32-bit-pointer) offsets the asm uses and are documentary only -- members are declared
// with x64 widths so only the ORDER is load-bearing and every access is by name (same rule
// as PlugIn.h's System layout). The instance is 0x8C bytes on the X360 image.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp" // f32, s32, s16, u16, u8

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// The real-FFT service the engine convolves through. These are free functions in
// rw::audio::core with their own (not-yet-reconstructed) home TU; declared here because
// Configure / Filter / Reset / the destructor call them. Signatures grounded in the
// FastFirEngine asm:
//   FFT_Alloc(sizeLog2, flag, &handle)          bl @0x82B6F2A0 (r3=log2, r4=0, r5=&handle)
//   FFT_Init(handle)                            bl @0x82B6F2A8 (r3=handle)
//   FFT_Free(&handle)                           bl @0x82B6E818 / @0x82B6E7A8 (r3=&handle)
//   FFT_ForwardReal(handle, buffer)             bl @0x82B6D178 (r3=handle, r4=buffer)
//   FFT_InverseReal(handle, buffer)             bl @0x82B6D370 (r3=handle, r4=buffer)
// -------------------------------------------------------------------------------------
void  FFT_Alloc(s32 sizeLog2, char flag, void **outHandle);
void  FFT_Init(void *handle);
void *FFT_Free(void **handle);
void *FFT_ForwardReal(void *handle, f32 *buffer);
void *FFT_InverseReal(void *handle, f32 *buffer);

class FastFirEngine
{
public:
    // Per-pass work-distribution record. The engine pre-computes, for each of the
    // `miNumPasses` sub-passes, how many forward-FFTs, complex MACs, and inverse-FFTs to
    // run in that pass, so the per-frame cost stays flat. 12 bytes on X360 and x64 (the asm
    // strides the table by 12); Filter reads .miFftCount @+4 and .miIfftCount @+8.
    struct LoadRecord
    {
        s32 miMacCount;  // +0x00 -- complex-multiply-accumulate ops in this pass
        s32 miFftCount;  // +0x04 -- forward real-FFTs in this pass
        s32 miIfftCount; // +0x08 -- inverse real-FFTs in this pass
    };

    // The rwaudio Process-graph channel node the block driver threads: the channel-0 sample
    // base at +0x04 and the 16-bit inter-channel stride at +0x0E (asm `lwz r,4(node)` /
    // `lhz r,0xE(node)`). Same shape Fir64::Filter / RawPuller2 read.
    struct ChannelNode
    {
        char *mpReserved0; // +0x00
        f32  *mpBase;      // +0x04 -- channel-0 sample base
        char  mGap08[0x0E - 0x08];
        u16   mu16Stride;  // +0x0E -- inter-channel stride in samples
    };

    // ---- ctor / dtor / lifecycle ----
    static FastFirEngine *FastFirEngine_ctor(FastFirEngine *self); // @0x82B68120
    static void          *FastFirEngine_dtor(FastFirEngine *self); // @0x82B6E758
    static FastFirEngine *Reset(FastFirEngine *self);              // @0x82B6E7C0

    // Record the input/output channel counts (@+0x74 / @+0x78); returns 1. @0x82B68150.
    static int SetChannels(FastFirEngine *self, s32 inputChannels, s32 outputChannels);

    // Lay out the one contiguous scratch block, size the partitions, allocate the FFT, and
    // pre-compute the per-pass load distribution. @0x82B6F048. Register-grounded signature:
    // r4=channels, r5=blockSize, r6=a4, r7=a5, r8=a6, r9=impulseSamples, r10=pImpulse, and a
    // trailing stack char `primeHistory`. (The X360 caller reserves further stack argument
    // slots @a8..a26 that the body never reads; they are omitted from this reconstruction.)
    static int Configure(FastFirEngine *self, s32 channels, s32 blockSize, s32 a4, s32 a5,
                         s32 a6, s32 impulseSamples, s16 *pImpulse, char primeHistory);

    // Run one frame through the engine: stage the input partition, advance this frame's
    // slice of FFT/MAC/IFFT work, mix the finished output partition, and copy it out.
    // @0x82B6D068. r3=self, r4=input node, r5=output node.
    static void *Filter(FastFirEngine *self, ChannelNode *inNode, ChannelNode *outNode);

    // Fill the miNumPasses LoadRecord table so the per-frame FFT/MAC/IFFT cost is spread
    // evenly across the sub-passes. @0x82B682A0. DECLARED ONLY -- see FastFirEngine.cpp for
    // why the body is BLOCKED (garbled decompiler output over an un-attested float formula).
    static FastFirEngine *LoadDistributionCalc(FastFirEngine *self, s32 sizeLog2, s32 numBlocks);

    // The VMX complex multiply-accumulate over one partition (out += impulse (x) freq).
    // @0x82B684C0. DECLARED ONLY -- the body is a hand-written VMX `vperm`/`vmaddfp` kernel
    // driven by un-recovered permute-control rodata; BLOCKED (see FastFirEngine.cpp).
    static void *MultiplyAccumulateComplex(FastFirEngine *self, f32 *pFreq, s16 *pImpulse,
                                           f32 *pAcc);

    // Estimate the CPU load of a given block/partition configuration. @0x82B68168.
    // DECLARED ONLY -- reads un-attested rodata float coefficients; BLOCKED.
    static float EstimateLoad(FastFirEngine *self, s32 a2, s32 a3, s32 a4, u32 a5);

    // ---- layout (X360 offsets are documentary; access by name) ----
    void *mpBuffer;           // +0x00  the one allocated scratch block (partition storage)
    f32  *mpInput[2];         // +0x04  double-buffered input-partition bases (idx1/idx2)
    f32  *mpFreq;             // +0x0C  frequency-domain partition ring
    s16  *mpImpulse;          // +0x10  the impulse-response coefficient data (caller-owned)
    f32  *mpConv;             // +0x14  per-output convolution accumulator
    f32  *mpOutput[2];        // +0x18  double-buffered output-partition bases (idx6/idx7)
    s32   miConvBytes;        // +0x20  4 * outputChannels * blockFftLen (mpConv span)
    s32   miBlockFftLen;      // +0x24  16-rounded transform length (per-partition f32 count)
    s32   miField28;          // +0x28  a6 (block-index selector; 1 == single impulse row)
    s32   miNumBlocks;        // +0x2C  partition/impulse block count
    s32   miWriteBlock;       // +0x30  current write-partition index (ring cursor)
    s32   miField34;          // +0x34  block-size remainder (last partial partition)
    s32   miFrameLen;         // +0x38  samples delivered per Filter call (== `channels` arg)
    s32   miBlockLen;         // +0x3C  FFT block / partition length (== `blockSize` arg)
    s32   miFftClearLen;      // +0x40  2*blockSize + 2 (zero-pad target for each forward FFT)
    s32   miField44;          // +0x44  a5
    s32   miBlockStride;      // +0x48  per-partition f32 stride (== miBlockFftLen)
    s32   miField4C;          // +0x4C  a5
    s32   miField50;          // +0x50  blockSize
    s32   miNumPasses;        // +0x54  sub-passes per frame (blockSize / channels)
    s32   miField58;          // +0x58  a5 + 8
    s32   miCurPass;          // +0x5C  current sub-pass index (0 .. miNumPasses-1)
    f32   mfField60;          // +0x60  ((blockSize-a4)/blockSize) * 100 (overlap %)
    s32   miPingA;            // +0x64  output double-buffer toggle
    s32   miPingB;            // +0x68  input double-buffer toggle
    void *mpFft;              // +0x6C  the FFT context handle (FFT_Alloc/FFT_Free)
    s32   miField70;          // +0x70  (cleared by ctor; unused by the bodied members)
    s32   miInputChannels;    // +0x74  SetChannels arg1
    s32   miOutputChannels;   // +0x78  SetChannels arg2
    LoadRecord *mpDist;       // +0x7C  per-pass work-distribution table (into mpBuffer)
    s32   miFftDone;          // +0x80  running forward-FFT progress across passes
    s32   miMacDone;          // +0x84  running MAC progress across passes
    s32   miIfftDone;         // +0x88  running inverse-FFT progress across passes
};

} // namespace core
} // namespace audio
} // namespace rw
