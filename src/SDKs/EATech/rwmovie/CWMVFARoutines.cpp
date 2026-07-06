// =====================================================================================
// CWMVFARoutines -- frame-analysis helper routines for the WMV video encoder pipeline.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CWMVFARoutines::calcMean4x4     @0x82A46610   (bodied here)
//   CWMVFARoutines::calcMean4x4x4   @0x82A46710   (bodied here)
//   CWMVFARoutines::calcMean8x8B    @0x82A46AE8   (declared-only; body not yet verified)
//   CWMVFARoutines::calcGradient8x1B@0x82A47050   (declared-only; body not yet verified)
//
// Every routine is a non-static member whose implicit `this` (a1/r3) is never touched --
// they are stateless block statistics over caller-supplied sample buffers passed as raw
// integer addresses. The block-mean kernels read 32-bit samples; the 8x8 kernel and the
// gradient kernel read bytes. All of them return residual register values (the caller
// discards the return of the mean/gradient kernels), reproduced faithfully from the asm.
// =====================================================================================

#include "types.hpp"

class CWMVFARoutines
{
public:
    // Mean of a 4x4 block of int32 samples -> *lpMeanOut (sum 16, >>4).
    int calcMean4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Four adjacent 4x4 block-means -> lpMeanOut[0..3].
    int calcMean4x4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Mean of an 8x8 block of byte samples -> *lpMeanOut (sum 64, >>6). Not yet verified.
    unsigned char* calcMean8x8B(u8* lpMeanOut, int liSampleBase, int liColumn, int liRowStride);

    // Two per-row gradient sums over an 8-row column strip. Not yet verified.
    int calcGradient8x1B(u32* lpaGradientA, u32* lpaGradientB,
                         int liRowA4, int liRowA5, int liRowA6,
                         int liColOffset, int liColStride);
};

// -------------------------------------------------------------------------------------
// CWMVFARoutines::calcMean4x4 @0x82A46610.
// Mean of a 4x4 block of int32 samples: sum 16 values, >>4, store to *lpMeanOut.
// Rows are liColumn, liColumn+stride, +2*stride, +3*stride; each row reads cols +0..+3.
// Returns (liColumn + 3*liRowStride) + 2 (residual r3).
// -------------------------------------------------------------------------------------
int CWMVFARoutines::calcMean4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride)
{
    const int* lpSamples = reinterpret_cast<const int*>(liSampleBase);
    const int liRow1 = liColumn + liRowStride;
    const int liRow2 = liRow1 + liRowStride;
    const int liRow3 = liRow2 + liRowStride;

    const int liSum = lpSamples[liColumn]     + lpSamples[liColumn + 1]
                    + lpSamples[liColumn + 2] + lpSamples[liColumn + 3]
                    + lpSamples[liRow1]       + lpSamples[liRow1 + 1]
                    + lpSamples[liRow1 + 2]   + lpSamples[liRow1 + 3]
                    + lpSamples[liRow2]       + lpSamples[liRow2 + 1]
                    + lpSamples[liRow2 + 2]   + lpSamples[liRow2 + 3]
                    + lpSamples[liRow3]       + lpSamples[liRow3 + 1]
                    + lpSamples[liRow3 + 2]   + lpSamples[liRow3 + 3];

    *lpMeanOut = static_cast<u32>(liSum) >> 4;
    return liRow3 + 2;
}

// -------------------------------------------------------------------------------------
// CWMVFARoutines::calcMean4x4x4 @0x82A46710.
// Four adjacent 4x4 block-means (columns liColumn+4*N for N=0..3), same sum-16 >>4 kernel
// as calcMean4x4, repeated four times, storing to lpMeanOut[0..3].
// Returns 4 * ((liColumn+12) + 3*liRowStride) (residual r3).
// -------------------------------------------------------------------------------------
int CWMVFARoutines::calcMean4x4x4(u32* lpMeanOut, int liSampleBase, int liColumn, int liRowStride)
{
    const int* lpSamples = reinterpret_cast<const int*>(liSampleBase);
    int liC3Row3 = 0;

    for (int liBlock = 0; liBlock < 4; ++liBlock)
    {
        const int liCol  = liColumn + 4 * liBlock;
        const int liRow1 = liCol  + liRowStride;
        const int liRow2 = liRow1 + liRowStride;
        const int liRow3 = liRow2 + liRowStride;

        const int liSum = lpSamples[liCol]      + lpSamples[liCol + 1]
                        + lpSamples[liCol + 2]  + lpSamples[liCol + 3]
                        + lpSamples[liRow1]     + lpSamples[liRow1 + 1]
                        + lpSamples[liRow1 + 2] + lpSamples[liRow1 + 3]
                        + lpSamples[liRow2]     + lpSamples[liRow2 + 1]
                        + lpSamples[liRow2 + 2] + lpSamples[liRow2 + 3]
                        + lpSamples[liRow3]     + lpSamples[liRow3 + 1]
                        + lpSamples[liRow3 + 2] + lpSamples[liRow3 + 3];

        lpMeanOut[liBlock] = static_cast<u32>(liSum) >> 4;
        liC3Row3 = liRow3;
    }

    return 4 * liC3Row3;
}

// calcMean8x8B @0x82A46AE8 and calcGradient8x1B @0x82A47050 are NOT bodied here: their
// reconstructions did not pass verification this wave (calcMean8x8B dropped a column and
// mis-modelled the row cadence; calcGradient8x1B fabricated a cursor advance). They remain
// declared above so callers resolve, and their bodies land once re-verified.
