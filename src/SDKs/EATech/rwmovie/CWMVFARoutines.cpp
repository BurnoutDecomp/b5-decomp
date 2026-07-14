// =====================================================================================
// CWMVFARoutines -- frame-analysis helper routines for the WMV video encoder pipeline.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CWMVFARoutines::calcMean4x4     @0x82A46610   (bodied here)
//   CWMVFARoutines::calcMean4x4x4   @0x82A46710   (bodied here)
//   CWMVFARoutines::calcMean8x8B    @0x82A46AE8   (bodied here)
//   CWMVFARoutines::calcGradient8x1B@0x82A47050   (bodied here)
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

// Human form of the asm's branchless abs (srawi/xor/subf) used by the SAD accumulators.
static inline int liAbs32(int liValue)
{
    return liValue < 0 ? -liValue : liValue;
}

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

// -------------------------------------------------------------------------------------
// CWMVFARoutines::calcMean8x8B @0x82A46AE8.
// Mean of an 8x8 block of byte samples: sum 64 values, >>6, store the low byte to
// *lpMeanOut. The block base is liSampleBase+liColumn; each of the 8 rows steps by
// liRowStride and reads 8 consecutive columns. The asm walks four rows per do-while pass
// (two passes) but the reduction is order-independent. Returns the column-2 cursor
// advanced past the block (liSampleBase+liColumn+2 + 8*liRowStride) -- the residual r3,
// which the callers discard.
// -------------------------------------------------------------------------------------
unsigned char* CWMVFARoutines::calcMean8x8B(u8* lpMeanOut, int liSampleBase, int liColumn, int liRowStride)
{
    u8* lpBlock = reinterpret_cast<u8*>(liSampleBase + liColumn);

    u32 luSum = 0;
    for (int liRow = 0; liRow < 8; ++liRow)
    {
        const u8* lpRow = lpBlock + liRow * liRowStride;
        luSum += lpRow[0] + lpRow[1] + lpRow[2] + lpRow[3]
               + lpRow[4] + lpRow[5] + lpRow[6] + lpRow[7];
    }

    *lpMeanOut = static_cast<u8>(luSum >> 6);
    return lpBlock + 2 + 8 * liRowStride;
}

// -------------------------------------------------------------------------------------
// CWMVFARoutines::calcGradient8x1B @0x82A47050.
// Walks an 8-column strip and, at each column, sums 4-connected byte gradients: the centre
// sample minus its up/down/left/right neighbours, where up/down are +/-liColStride and
// left/right are +/-1. Output B (lpaGradientB[col], cursor advanced each column) sums the
// gradient over all three rows liRowA4/liRowA5/liRowA6; output A (*lpaGradientA -- the asm
// never advances this cursor, so it is overwritten each column) sums it over liRowA4 only.
// Returns the last column's |liRowA4 centre - left neighbour| residual, which callers
// discard.
// -------------------------------------------------------------------------------------
int CWMVFARoutines::calcGradient8x1B(u32* lpaGradientA, u32* lpaGradientB,
                                     int liRowA4, int liRowA5, int liRowA6,
                                     int liColOffset, int liColStride)
{
    const u8* lpRowA4 = reinterpret_cast<const u8*>(liRowA4 + liColOffset);
    const u8* lpRowA6 = reinterpret_cast<const u8*>(liRowA6 + liColOffset);
    const u8* lpRowA5 = reinterpret_cast<const u8*>(liRowA5 + liColOffset);
    const int liUp   = -liColStride;   // v11 - 1
    const int liDown =  liColStride;   // v12 + 1

    int liReturn = 0;
    for (int liCol = 8; liCol != 0; --liCol)
    {
        const int liCentreA5 = *lpRowA5;
        const int liCentreA6 = *lpRowA6;
        const int liCentreA4 = *lpRowA4;

        *lpaGradientB = liAbs32(liCentreA6 - lpRowA6[liUp])
                      + liAbs32(liCentreA5 - lpRowA5[liUp])
                      + liAbs32(liCentreA6 - lpRowA6[liDown])
                      + liAbs32(liCentreA5 - lpRowA5[liDown])
                      + liAbs32(liCentreA6 - lpRowA6[1])
                      + liAbs32(liCentreA5 - lpRowA5[1])
                      + liAbs32(liCentreA4 - lpRowA4[liUp])
                      + liAbs32(liCentreA4 - lpRowA4[1])
                      + liAbs32(liCentreA6 - lpRowA6[-1])
                      + liAbs32(liCentreA5 - lpRowA5[-1])
                      + liAbs32(liCentreA4 - lpRowA4[liDown])
                      + liAbs32(liCentreA4 - lpRowA4[-1]);

        ++lpRowA5;
        ++lpRowA6;
        ++lpaGradientB;

        const int liA4Up    = liCentreA4 - lpRowA4[liUp];
        const int liA4Right = liCentreA4 - lpRowA4[1];
        const int liA4Left  = liCentreA4 - lpRowA4[-1];
        const int liA4Down  = liCentreA4 - lpRowA4[liDown];
        ++lpRowA4;

        *lpaGradientA = liAbs32(liA4Up) + liAbs32(liA4Right)
                      + liAbs32(liA4Down) + liAbs32(liA4Left);
        liReturn = liA4Left ^ (liA4Left >> 31);
    }

    return liReturn;
}
