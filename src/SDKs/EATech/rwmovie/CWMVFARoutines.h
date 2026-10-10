// =====================================================================================
// CWMVFARoutines -- frame-analysis block-statistic kernels for the WMV video encoder.
//
// Reconstructed from the console executable; the PowerPC asm is authoritative. No
// reference source and no debug-info declaration exists for this class.
//
// The class is polymorphic: one shared instance (gpWMVFARoutines, 4 bytes on the console,
// i.e. only the vptr) is created by the first CWMVideoPerceptionModel and released by its
// destructor, and CVideoFrameAnalyst dispatches its block kernels through that instance's
// vtable. The console vtable holds exactly four slots, in this order:
//   slot 0  calcMean4x4x4      slot 1  calcMean8x8B
//   slot 2  calcMean16x16B     slot 3  calcGradient8x1B
// calcMean4x4 is a plain (non-virtual) member. The class has no data and no virtual
// destructor (the instance is released with XMemFree, no destructor call).
//
// Every kernel leaves `this` untouched; they are stateless block statistics over
// caller-supplied sample buffers passed as integer addresses. The block-mean kernels over
// 32-bit samples read ints; the 8x8 / 16x16 kernels and the gradient kernel read bytes.
// All of them return residual register values that the callers discard.
// =====================================================================================
#pragma once

#include "types.hpp"

class CWMVFARoutines
{
public:
    // Mean of a 4x4 block of int32 samples -> *lpMeanOut (sum 16, >>4).
    int calcMean4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Four adjacent 4x4 block-means -> lpMeanOut[0..3].
    virtual int calcMean4x4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Mean of an 8x8 block of byte samples -> *lpMeanOut (sum 64, >>6). Not yet verified.
    virtual unsigned char* calcMean8x8B(u8* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Mean of a 16x16 block of byte samples -> *lpMeanOut (sum 256, >>8).
    virtual unsigned char* calcMean16x16B(u8* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Two per-row gradient sums over an 8-row column strip. Not yet verified.
    virtual int calcGradient8x1B(u32* lpaGradientA, u32* lpaGradientB,
                                 int liRowA4, int liRowA5, int liRowA6,
                                 int liColOffset, int liColStride);
};

// The shared analysis-routine instance (null until the first perception model is built).
extern CWMVFARoutines* gpWMVFARoutines;
