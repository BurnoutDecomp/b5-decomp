// On2 VP6 decoder: generic block kernels, the bilinear sub-pixel filters, frame border extension
// and the machine-specific kernel selection.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

extern "C"
{

void (*FilterBlockBil_8)(unsigned char* ReconPtr1, unsigned char* ReconPtr2, unsigned char* ReconRefPtr,
                         unsigned int PixelsPerLine, int ModX, int ModY);
void (*ClearSysState)(void);
void (*SubtractBlock)(unsigned char* SrcPtr, short* DestPtr, unsigned int SrcPixelsPerLine);

// This target has no processor state to restore after the vector kernels.
static void ClearSysState_C(void)
{
}

// Widen an 8x8 block of pixels to 16 bits.
void UnpackBlock_C(unsigned char* ReconPtr, short* ReconRefPtr, unsigned int ReconPixelsPerLine)
{
    for (int liRow = 8; liRow != 0; --liRow)
    {
        for (int liColumn = 0; liColumn < 8; ++liColumn)
            ReconRefPtr[liColumn] = ReconPtr[liColumn];

        ReconPtr += ReconPixelsPerLine;
        ReconRefPtr += 8;
    }
}

// Replace each 16-bit prediction with the difference between the source pixel and it.
void SubtractBlock_C(unsigned char* SrcPtr, short* DestPtr, unsigned int SrcPixelsPerLine)
{
    for (int liRow = 8; liRow != 0; --liRow)
    {
        for (int liColumn = 0; liColumn < 8; ++liColumn)
            DestPtr[liColumn] = static_cast<short>(SrcPtr[liColumn] - static_cast<unsigned short>(DestPtr[liColumn]));

        SrcPtr += SrcPixelsPerLine;
        DestPtr += 8;
    }
}

void InitVPUtil(void)
{
    ClearSysState_C();
    UtilMachineSpecificConfig();
}

// First (horizontal or vertical) pass of a 2-D bilinear filter into 32-bit intermediates.
void FilterBlock2dBil_FirstPass(unsigned char* SrcPtr, int* OutputPtr, unsigned int SrcPixelsPerLine,
                                unsigned int PixelStep, unsigned int OutputHeight,
                                unsigned int OutputWidth, const int* VpFilter)
{
    for (; OutputHeight != 0; --OutputHeight)
    {
        for (unsigned int luColumn = 0; luColumn < OutputWidth; ++luColumn)
        {
            OutputPtr[luColumn] = (static_cast<int>(SrcPtr[PixelStep]) * VpFilter[1] +
                                   static_cast<int>(SrcPtr[0]) * VpFilter[0] + 64) >> 7;
            ++SrcPtr;
        }

        SrcPtr += SrcPixelsPerLine - OutputWidth;
        OutputPtr += OutputWidth;
    }
}

// One-dimensional bilinear filter straight to pixels.
void FilterBlock1dBil_8(unsigned char* SrcPtr, unsigned char* OutputPtr, unsigned int SrcPixelsPerLine,
                        unsigned int PixelStep, unsigned int OutputHeight,
                        unsigned int OutputWidth, const int* VpFilter)
{
    for (; OutputHeight != 0; --OutputHeight)
    {
        for (unsigned int luColumn = 0; luColumn < OutputWidth; ++luColumn)
        {
            OutputPtr[luColumn] = static_cast<unsigned char>(
                (static_cast<int>(SrcPtr[PixelStep]) * VpFilter[1] + static_cast<int>(SrcPtr[0]) * VpFilter[0] + 64) >> 7);
            ++SrcPtr;
        }

        SrcPtr += SrcPixelsPerLine - OutputWidth;
        OutputPtr += OutputWidth;
    }
}

// Second pass of a 2-D bilinear filter, from the 32-bit intermediates to pixels.
void FilterBlock2dBil_SecondPass_8(int* SrcPtr, unsigned char* OutputPtr, unsigned int SrcPixelsPerLine,
                                   unsigned int PixelStep, unsigned int OutputHeight,
                                   unsigned int OutputWidth, const int* VpFilter)
{
    for (; OutputHeight != 0; --OutputHeight)
    {
        for (unsigned int luColumn = 0; luColumn < OutputWidth; ++luColumn)
        {
            OutputPtr[luColumn] = static_cast<unsigned char>(
                (SrcPtr[PixelStep] * VpFilter[1] + SrcPtr[0] * VpFilter[0] + 64) >> 7);
            ++SrcPtr;
        }

        SrcPtr += SrcPixelsPerLine - OutputWidth;
        OutputPtr += OutputWidth;
    }
}

void FilterBlock2dBil_8(unsigned char* SrcPtr, unsigned char* OutputPtr, unsigned int SrcPixelsPerLine,
                        const int* HFilter, const int* VFilter)
{
    int laIntermediate[9 * 8];

    FilterBlock2dBil_FirstPass(SrcPtr, laIntermediate, SrcPixelsPerLine, 1, 9, 8, HFilter);
    FilterBlock2dBil_SecondPass_8(laIntermediate, OutputPtr, 8, 8, 8, 8, VFilter);
}

// Bilinear prediction of an 8x8 block between two reference pointers one pixel, one line or one
// diagonal step apart; ModX / ModY select the eighth-pel taps.
void FilterBlockBil_8_C(unsigned char* ReconPtr1, unsigned char* ReconPtr2, unsigned char* ReconRefPtr,
                        unsigned int PixelsPerLine, int ModX, int ModY)
{
    unsigned char* lpSrc = ReconPtr1;
    ptrdiff_t liDiff = ReconPtr2 - ReconPtr1;
    if (liDiff < 0)
    {
        lpSrc = ReconPtr2;
        liDiff = ReconPtr1 - ReconPtr2;
    }

    const ptrdiff_t liStride = static_cast<int>(PixelsPerLine);

    if (liDiff == 1)
    {
        FilterBlock1dBil_8(lpSrc, ReconRefPtr, PixelsPerLine, 1, 8, 8, VP6_BilinearFilters[ModX]);
    }
    else if (liDiff == liStride)
    {
        FilterBlock1dBil_8(lpSrc, ReconRefPtr, PixelsPerLine, PixelsPerLine, 8, 8, VP6_BilinearFilters[ModY]);
    }
    else if (liDiff == liStride - 1)
    {
        FilterBlock2dBil_8(lpSrc - 1, ReconRefPtr, PixelsPerLine, VP6_BilinearFilters[ModX], VP6_BilinearFilters[ModY]);
    }
    else if (liDiff == liStride + 1)
    {
        FilterBlock2dBil_8(lpSrc, ReconRefPtr, PixelsPerLine, VP6_BilinearFilters[ModX], VP6_BilinearFilters[ModY]);
    }
}

// Replicate a plane's edge pixels into its border: each row's first and last pixel sideways, then
// the first and last full rows (border included) upwards and downwards.
static void ExtendPlaneBorders(unsigned char* lpPlane, int liWidth, int liHeight, unsigned int luStride,
                               int liBorder)
{
    unsigned char* lpLeftSrc = lpPlane;
    unsigned char* lpRightSrc = lpPlane + liWidth - 1;
    unsigned char* lpLeftDst = lpPlane - liBorder;
    unsigned char* lpRightDst = lpRightSrc + 1;

    if (liHeight > 0)
    {
        for (int liRow = liHeight; liRow != 0; --liRow)
        {
            memset(lpLeftDst, *lpLeftSrc, liBorder);
            memset(lpRightDst, *lpRightSrc, liBorder);
            lpLeftSrc += luStride;
            lpRightSrc += luStride;
            lpLeftDst += luStride;
            lpRightDst += luStride;
        }
    }

    unsigned char* lpFirstRow = lpPlane - liBorder;
    unsigned char* lpTopDst = lpFirstRow - liBorder * static_cast<int>(luStride);
    unsigned char* lpLastRow = lpFirstRow + (liHeight - 1) * static_cast<int>(luStride);
    unsigned char* lpBottomDst = lpLastRow + luStride;

    if (liBorder > 0)
    {
        const ptrdiff_t liBottomOffset = lpBottomDst - lpTopDst;
        for (int liRow = liBorder; liRow != 0; --liRow)
        {
            memcpy(lpTopDst, lpFirstRow, luStride);
            memcpy(lpTopDst + liBottomOffset, lpLastRow, luStride);
            lpTopDst += luStride;
        }
    }
}

void UpdateUMVBorder(FRAME_INFO* FrameInfo, unsigned char* DestReconPtr)
{
    const int liBorder = static_cast<int>(FrameInfo->UMVBorder);
    const int liChromaBorder = static_cast<int>(FrameInfo->UMVBorder >> 1);

    ExtendPlaneBorders(DestReconPtr + FrameInfo->YDataOffset, FrameInfo->HFragments * 8,
                       FrameInfo->VFragments * 8, FrameInfo->YStride, liBorder);
    ExtendPlaneBorders(DestReconPtr + FrameInfo->UDataOffset, FrameInfo->HFragments * 4,
                       FrameInfo->VFragments * 4, FrameInfo->UVStride, liChromaBorder);
    ExtendPlaneBorders(DestReconPtr + FrameInfo->VDataOffset, FrameInfo->HFragments * 4,
                       FrameInfo->VFragments * 4, FrameInfo->UVStride, liChromaBorder);
}

void UtilMachineSpecificConfig(void)
{
    ClearSysState = ClearSysState_C;
    FilterBlockBil_8 = FilterBlockBil_8_C;
    SubtractBlock = SubtractBlock_C;
}

void VP6_DMachineSpecificConfig(void)
{
    VP6_BuildQuantIndex = VP6_BuildQuantIndex_Generic;
}

}
