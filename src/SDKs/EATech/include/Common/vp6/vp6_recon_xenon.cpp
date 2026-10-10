// On2 VP6 decoder, vector build: block reconstruction and the bilinear sub-pixel prediction filters.
//
// Pixel rows are 8 bytes inside 16-byte vector blocks. The reconstruction kernels pack two rows into
// one vector and store each row with two word-element stores; which half of the vector a store
// takes depends on the destination address, so the pack order follows the destination's alignment
// (8-pixel blocks start on a 16- or an 8-byte boundary). The filters work in float: unaligned rows
// are assembled from a left and a right partial load, widened to words, and combined with fused
// multiply-adds against the tap pair; +0.5 and truncation round the result.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include "SDKs/EATech/include/Common/vp6/vp6_xenon_vmx.h"

namespace
{
    // VP6_XenonConstants rows used here.
    const int KI_VEC_HALF = 0;          // 0.5f x 4
    const int KI_VEC_ZERO = 1;
    const int KI_VEC_BYTES_0_3 = 2;     // bytes 0..3 of the second operand as words
    const int KI_VEC_BYTES_4_7 = 3;     // bytes 4..7 as words
    const int KI_VEC_BYTE_8 = 4;        // byte 8 as word 0, the other words zero
    const int KI_VEC_BYTES_TO_HALVES = 5; // bytes 0..7 of the second operand as halfwords
    const int KI_VEC_128_HALVES = 6;    // 128 in every halfword

    inline VmxVector Constant(int liRow)
    {
        return VmxLoadImage(VP6_XenonConstants[liRow]);
    }

    // Store an 8-pixel row held in the half of lRow the destination address selects.
    inline void StoreRow(unsigned char* lpDest, const VmxVector& lRow)
    {
        VmxStoreWordElementU8(lpDest, lRow);
        VmxStoreWordElementU8(lpDest + 4, lRow);
    }

    // Two reconstructed rows, packed so the first row lands in the half its destination selects.
    inline VmxVector PackRows(const VmxVector& lRow0, const VmxVector& lRow1, bool lbAligned)
    {
        return lbAligned ? VmxPackSHUS(lRow0, lRow1) : VmxPackSHUS(lRow1, lRow0);
    }

    // Sixteen bytes from an arbitrary address.
    inline VmxVector LoadUnaligned(const unsigned char* lpSrc)
    {
        return VmxOr(VmxLoadLeftU8(lpSrc), VmxLoadRightU8(lpSrc + 16));
    }
}

extern "C"
{

// Destination = reference + residual, saturated (no motion vector).
void ScalarReconInter_Xenon(unsigned char* ReconPtr, const unsigned char* RefPtr, const short* ChangePtr,
                            unsigned int LineStep)
{
    const bool lbAligned = (reinterpret_cast<uintptr_t>(ReconPtr) & 15) == 0;
    const VmxVector lUnpack = Constant(KI_VEC_BYTES_TO_HALVES);
    const VmxVector lZero = Constant(KI_VEC_ZERO);

    for (int liRow = 0; liRow < 8; liRow += 2)
    {
        const VmxVector lRef0 = VmxPermute(lZero, VmxLoadLeftU8(RefPtr + liRow * LineStep), lUnpack);
        const VmxVector lRef1 = VmxPermute(lZero, VmxLoadLeftU8(RefPtr + (liRow + 1) * LineStep), lUnpack);
        const VmxVector lSum0 = VmxAddSHS(lRef0, VmxLoadS16(ChangePtr + liRow * 8));
        const VmxVector lSum1 = VmxAddSHS(lRef1, VmxLoadS16(ChangePtr + (liRow + 1) * 8));

        const VmxVector lPacked = PackRows(lSum0, lSum1, lbAligned);
        StoreRow(ReconPtr + liRow * LineStep, lPacked);
        StoreRow(ReconPtr + (liRow + 1) * LineStep, VmxShiftLeftDoubleOctet(lPacked, lPacked, 8));
    }
}

// Destination = 128 + residual, saturated (intra block).
void ScalarReconIntra_Xenon(unsigned char* ReconPtr, const short* ChangePtr, unsigned int LineStep)
{
    const bool lbAligned = (reinterpret_cast<uintptr_t>(ReconPtr) & 15) == 0;
    const VmxVector lBias = Constant(KI_VEC_128_HALVES);

    for (int liRow = 0; liRow < 8; liRow += 2)
    {
        const VmxVector lSum0 = VmxAddSHS(VmxLoadS16(ChangePtr + liRow * 8), lBias);
        const VmxVector lSum1 = VmxAddSHS(VmxLoadS16(ChangePtr + (liRow + 1) * 8), lBias);

        const VmxVector lPacked = PackRows(lSum0, lSum1, lbAligned);
        StoreRow(ReconPtr + liRow * LineStep, lPacked);
        StoreRow(ReconPtr + (liRow + 1) * LineStep, VmxShiftLeftDoubleOctet(lPacked, lPacked, 8));
    }
}

// Destination = 16-bit prediction + residual, saturated.
void ReconBlock_Xenon(const short* SrcPtr, const short* ChangePtr, unsigned char* DestPtr, unsigned int LineStep)
{
    const bool lbAligned = (reinterpret_cast<uintptr_t>(DestPtr) & 15) == 0;

    for (int liRow = 0; liRow < 8; liRow += 2)
    {
        const VmxVector lSum0 = VmxAddSHS(VmxLoadS16(SrcPtr + liRow * 8), VmxLoadS16(ChangePtr + liRow * 8));
        const VmxVector lSum1 =
            VmxAddSHS(VmxLoadS16(SrcPtr + (liRow + 1) * 8), VmxLoadS16(ChangePtr + (liRow + 1) * 8));

        const VmxVector lPacked = PackRows(lSum0, lSum1, lbAligned);
        StoreRow(DestPtr + liRow * LineStep, lPacked);
        StoreRow(DestPtr + (liRow + 1) * LineStep, VmxShiftLeftDoubleOctet(lPacked, lPacked, 8));
    }
}

// Horizontal pass of the 2-D filter: OutputHeight rows of eight rounded floats (two 16-byte halves
// per row). The pixel step and output width are fixed at 1 and 8.
void FilterBlock2dBil_FirstPass_Xenon(const unsigned char* SrcPtr, float* OutputPtr,
                                      unsigned int SrcPixelsPerLine, unsigned int /*PixelStep*/,
                                      unsigned int OutputHeight, unsigned int /*OutputWidth*/,
                                      const float* VpFilter)
{
    const VmxVector lTaps = VmxLoadF32(VpFilter);
    const VmxVector lTap0 = VmxSplatW(lTaps, 0);
    const VmxVector lTap1 = VmxSplatW(lTaps, 1);
    if (OutputHeight == 0)
        return;

    const VmxVector lZero = Constant(KI_VEC_ZERO);
    const VmxVector lByte8 = Constant(KI_VEC_BYTE_8);
    const VmxVector lHalf = Constant(KI_VEC_HALF);
    const VmxVector lBytes03 = Constant(KI_VEC_BYTES_0_3);
    const VmxVector lBytes47 = Constant(KI_VEC_BYTES_4_7);

    do
    {
        const VmxVector lRow = LoadUnaligned(SrcPtr);
        SrcPtr += SrcPixelsPerLine;

        const VmxVector lPixels03 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes03), 0);
        const VmxVector lPixels47 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes47), 0);
        const VmxVector lPixel8 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lByte8), 0);

        const VmxVector lLeft = VmxMaddFP(lPixels03, lTap0, lHalf);
        const VmxVector lRight = VmxMaddFP(lPixels47, lTap0, lHalf);
        const VmxVector lPixels14 = VmxShiftLeftDoubleOctet(lPixels03, lPixels47, 4);
        const VmxVector lPixels58 = VmxShiftLeftDoubleOctet(lPixels47, lPixel8, 4);

        VmxStoreF32(OutputPtr, VmxRoundToZeroFP(VmxMaddFP(lPixels14, lTap1, lLeft)));
        VmxStoreF32(OutputPtr + 4, VmxRoundToZeroFP(VmxMaddFP(lPixels58, lTap1, lRight)));
        OutputPtr += 8;
    } while (--OutputHeight != 0);
}

// One-dimensional filter between each row and the row PixelStep bytes on, to 16-bit pixels. The
// output width is fixed at 8.
void FilterBlock1dBilV_Xenon(const unsigned char* SrcPtr, short* OutputPtr, unsigned int SrcPixelsPerLine,
                             unsigned int PixelStep, unsigned int OutputHeight, unsigned int /*OutputWidth*/,
                             const float* VpFilter)
{
    const VmxVector lTaps = VmxLoadF32(VpFilter);
    const VmxVector lTap0 = VmxSplatW(lTaps, 0);
    const VmxVector lTap1 = VmxSplatW(lTaps, 1);
    if (OutputHeight == 0)
        return;

    const VmxVector lBytes47 = Constant(KI_VEC_BYTES_4_7);
    const VmxVector lHalf = Constant(KI_VEC_HALF);
    const VmxVector lZero = Constant(KI_VEC_ZERO);
    const VmxVector lBytes03 = Constant(KI_VEC_BYTES_0_3);

    const unsigned char* lpNext = SrcPtr + PixelStep;
    do
    {
        const VmxVector lRow = LoadUnaligned(SrcPtr);
        SrcPtr += SrcPixelsPerLine;
        const VmxVector lNextRow = LoadUnaligned(lpNext);
        lpNext += SrcPixelsPerLine;

        const VmxVector lRow03 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes03), 0);
        const VmxVector lRow47 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes47), 0);
        const VmxVector lNext47 = VmxConvertFromUXW(VmxPermute(lZero, lNextRow, lBytes47), 0);
        const VmxVector lNext03 = VmxConvertFromUXW(VmxPermute(lZero, lNextRow, lBytes03), 0);

        const VmxVector lLeft = VmxMaddFP(lRow03, lTap0, lHalf);
        const VmxVector lRight = VmxMaddFP(lRow47, lTap0, lHalf);
        const VmxVector lLeftSum = VmxConvertToUXWSat(VmxMaddFP(lNext03, lTap1, lLeft), 0);
        const VmxVector lRightSum = VmxConvertToUXWSat(VmxMaddFP(lNext47, lTap1, lRight), 0);

        VmxStoreS16(OutputPtr, VmxPackUWUS(lLeftSum, lRightSum));
        OutputPtr += 8;
    } while (--OutputHeight != 0);
}

// Bilinear 8x8 prediction between two reference pointers one pixel, one line or one diagonal step
// apart, to 16-bit pixels; ModX / ModY select the eighth-pel taps.
void FilterBlock_Xenon(const unsigned char* ReconPtr1, const unsigned char* ReconPtr2, short* ReconRefPtr,
                       unsigned int PixelsPerLine, int ModX, int ModY, int /*UseBicubic*/, float* TmpBuffer)
{
    const ptrdiff_t liDiff = ReconPtr2 - ReconPtr1;
    const ptrdiff_t liStride = static_cast<int>(PixelsPerLine);

    if (liDiff == 1)
    {
        const VmxVector lTaps = VmxLoadF32(VP6_BilinearFiltersXenon[ModX]);
        const VmxVector lTap0 = VmxSplatW(lTaps, 0);
        const VmxVector lTap1 = VmxSplatW(lTaps, 1);
        const VmxVector lZero = Constant(KI_VEC_ZERO);
        const VmxVector lBytes03 = Constant(KI_VEC_BYTES_0_3);
        const VmxVector lByte8 = Constant(KI_VEC_BYTE_8);
        const VmxVector lBytes47 = Constant(KI_VEC_BYTES_4_7);
        const VmxVector lHalf = Constant(KI_VEC_HALF);

        for (int liRow = 8; liRow != 0; --liRow)
        {
            const VmxVector lRow = LoadUnaligned(ReconPtr1);
            ReconPtr1 += PixelsPerLine;

            const VmxVector lPixels03 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes03), 0);
            const VmxVector lPixels47 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lBytes47), 0);
            const VmxVector lPixel8 = VmxConvertFromUXW(VmxPermute(lZero, lRow, lByte8), 0);

            const VmxVector lLeft = VmxMaddFP(lPixels03, lTap0, lHalf);
            const VmxVector lRight = VmxMaddFP(lPixels47, lTap0, lHalf);
            const VmxVector lPixels58 = VmxShiftLeftDoubleOctet(lPixels47, lPixel8, 4);
            const VmxVector lPixels14 = VmxShiftLeftDoubleOctet(lPixels03, lPixels47, 4);

            const VmxVector lLeftSum = VmxConvertToUXWSat(VmxMaddFP(lPixels14, lTap1, lLeft), 0);
            const VmxVector lRightSum = VmxConvertToUXWSat(VmxMaddFP(lPixels58, lTap1, lRight), 0);

            VmxStoreS16(ReconRefPtr, VmxPackUWUS(lLeftSum, lRightSum));
            ReconRefPtr += 8;
        }
        return;
    }

    if (liDiff == liStride)
    {
        FilterBlock1dBilV_Xenon(ReconPtr1, ReconRefPtr, PixelsPerLine, PixelsPerLine, 8, 8,
                                VP6_BilinearFiltersXenon[ModY]);
        return;
    }

    if (liDiff == liStride - 1)
        ReconPtr1 -= 1;

    // Horizontal pass into nine rows of floats, then the vertical pass between consecutive rows.
    FilterBlock2dBil_FirstPass_Xenon(ReconPtr1, TmpBuffer, PixelsPerLine, 1, 9, 8, VP6_BilinearFiltersXenon[ModX]);

    const VmxVector lTaps = VmxLoadF32(VP6_BilinearFiltersXenon[ModY]);
    const VmxVector lTap0 = VmxSplatW(lTaps, 0);
    const VmxVector lTap1 = VmxSplatW(lTaps, 1);
    const VmxVector lHalf = Constant(KI_VEC_HALF);

    const float* lpRow = TmpBuffer;
    for (int liRow = 8; liRow != 0; --liRow)
    {
        const VmxVector lLeft = VmxMaddFP(VmxLoadF32(lpRow), lTap0, lHalf);
        const VmxVector lRight = VmxMaddFP(VmxLoadF32(lpRow + 4), lTap0, lHalf);
        const VmxVector lRightSum = VmxConvertToUXWSat(VmxMaddFP(VmxLoadF32(lpRow + 12), lTap1, lRight), 0);
        const VmxVector lLeftSum = VmxConvertToUXWSat(VmxMaddFP(VmxLoadF32(lpRow + 8), lTap1, lLeft), 0);
        lpRow += 8;

        VmxStoreS16(ReconRefPtr, VmxPackUWUS(lLeftSum, lRightSum));
        ReconRefPtr += 8;
    }
}

}
