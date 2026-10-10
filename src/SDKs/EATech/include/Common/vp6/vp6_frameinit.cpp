// On2 VP6 decoder: frame geometry and the per-frame / per-macroblock buffers.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

namespace
{
    // Round a heap block up to a 32-byte boundary.
    template <typename T>
    T* AlignUp32(void* lpBlock)
    {
        return reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(lpBlock) + 31) & ~static_cast<uintptr_t>(31));
    }
}

extern "C"
{

void VP6_DeleteFragmentInfo(PB_INSTANCE* pbi)
{
    if (pbi->Coeffs)
        duck_freeAlign(pbi->Coeffs);
    pbi->Coeffs = 0;

    for (int liPlane = 0; liPlane < 3; ++liPlane)
    {
        if (pbi->AboveBlockContextsAlloc[liPlane])
            duck_free(pbi->AboveBlockContextsAlloc[liPlane]);
        pbi->AboveBlockContextsAlloc[liPlane] = 0;
        pbi->AboveBlockContexts[liPlane] = 0;
    }

    if (pbi->MbMotionVectorsAlloc)
        duck_free(pbi->MbMotionVectorsAlloc);
    pbi->MbMotionVectorsAlloc = 0;
    pbi->MbMotionVectors = 0;

    if (pbi->MbModesAlloc)
        duck_free(pbi->MbModesAlloc);
    pbi->MbModesAlloc = 0;
    pbi->MbModes = 0;

    if (pbi->CreateCodeArrays)
    {
        if (pbi->MbFlagsAlloc)
            duck_free(pbi->MbFlagsAlloc);
        pbi->MbFlagsAlloc = 0;
        pbi->MbFlags = 0;

        if (pbi->FragmentValuesAlloc)
            duck_free(pbi->FragmentValuesAlloc);
        pbi->FragmentValuesAlloc = 0;
        pbi->FragmentValues = 0;
    }
}

int VP6_AllocateFragmentInfo(PB_INSTANCE* pbi)
{
    VP6_DeleteFragmentInfo(pbi);

    pbi->Coeffs = static_cast<short*>(duck_mallocAlign(6 * 64 * sizeof(short), 128, 0));
    if (!pbi->Coeffs)
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }

    // Above contexts: two luma blocks per macroblock, one chroma block, plus the border.
    pbi->AboveBlockContextsAlloc[0] = duck_malloc((pbi->HFragments + 10) * sizeof(BLOCK_CONTEXT), 0);
    if (!pbi->AboveBlockContextsAlloc[0])
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }
    pbi->AboveBlockContexts[0] = AlignUp32<BLOCK_CONTEXT>(pbi->AboveBlockContextsAlloc[0]);

    pbi->AboveBlockContextsAlloc[1] = duck_malloc(((pbi->HFragments >> 1) + 10) * sizeof(BLOCK_CONTEXT), 0);
    if (!pbi->AboveBlockContextsAlloc[1])
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }
    pbi->AboveBlockContexts[1] = AlignUp32<BLOCK_CONTEXT>(pbi->AboveBlockContextsAlloc[1]);

    pbi->AboveBlockContextsAlloc[2] = duck_malloc(((pbi->HFragments >> 1) + 10) * sizeof(BLOCK_CONTEXT), 0);
    if (!pbi->AboveBlockContextsAlloc[2])
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }
    pbi->AboveBlockContexts[2] = AlignUp32<BLOCK_CONTEXT>(pbi->AboveBlockContextsAlloc[2]);

    pbi->MbModesAlloc = duck_malloc(pbi->MacroBlocks + 32, 0);
    if (!pbi->MbModesAlloc)
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }
    pbi->MbModes = AlignUp32<signed char>(pbi->MbModesAlloc);

    pbi->MbMotionVectorsAlloc = duck_malloc((pbi->MacroBlocks + 8) * sizeof(MOTION_VECTOR), 0);
    if (!pbi->MbMotionVectorsAlloc)
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }
    pbi->MbMotionVectors = AlignUp32<MOTION_VECTOR>(pbi->MbMotionVectorsAlloc);

    if (pbi->CreateCodeArrays)
    {
        pbi->MbFlagsAlloc = duck_malloc(pbi->MacroBlocks + 32, 0);
        if (!pbi->MbFlagsAlloc)
        {
            VP6_DeleteFragmentInfo(pbi);
            return 0;
        }
        pbi->MbFlags = AlignUp32<unsigned char>(pbi->MbFlagsAlloc);

        pbi->FragmentValuesAlloc = duck_malloc((pbi->UnitFragments + 8) * sizeof(int), 0);
        if (!pbi->FragmentValuesAlloc)
        {
            VP6_DeleteFragmentInfo(pbi);
            return 0;
        }
        pbi->FragmentValues = AlignUp32<int>(pbi->FragmentValuesAlloc);
    }

    return 1;
}

void VP6_DeleteFrameInfo(PB_INSTANCE* pbi)
{
    if (pbi->ThisFrameReconAlloc)
        duck_freeAlign(pbi->ThisFrameReconAlloc);
    if (pbi->GoldenFrameAlloc)
        duck_freeAlign(pbi->GoldenFrameAlloc);
    if (pbi->LastFrameReconAlloc)
        duck_freeAlign(pbi->LastFrameReconAlloc);

    pbi->ThisFrameReconAlloc = 0;
    pbi->GoldenFrameAlloc = 0;
    pbi->LastFrameReconAlloc = 0;
    pbi->ThisFrameRecon = 0;
    pbi->GoldenFrame = 0;
    pbi->LastFrameRecon = 0;
}

int VP6_AllocateFrameInfo(PB_INSTANCE* pbi, unsigned int FrameSize)
{
    VP6_DeleteFrameInfo(pbi);

    pbi->ThisFrameReconAlloc = static_cast<unsigned char*>(
        duck_mallocAlign(FrameSize + pbi->Configuration.YStride, 128, 0));
    if (!pbi->ThisFrameReconAlloc)
    {
        VP6_DeleteFrameInfo(pbi);
        return 0;
    }
    pbi->ThisFrameRecon = pbi->ThisFrameReconAlloc;

    pbi->GoldenFrameAlloc = static_cast<unsigned char*>(
        duck_mallocAlign(FrameSize + pbi->Configuration.YStride, 128, 0));
    if (!pbi->GoldenFrameAlloc)
    {
        VP6_DeleteFrameInfo(pbi);
        return 0;
    }
    pbi->GoldenFrame = pbi->GoldenFrameAlloc;

    pbi->LastFrameReconAlloc = static_cast<unsigned char*>(
        duck_mallocAlign(FrameSize + pbi->Configuration.YStride, 128, 0));
    if (!pbi->LastFrameReconAlloc)
    {
        VP6_DeleteFrameInfo(pbi);
        return 0;
    }
    pbi->LastFrameRecon = pbi->LastFrameReconAlloc;
    return 1;
}

// Derive every size and offset from the configured picture size, then (re)allocate the buffers.
// Reconstruction frames carry a 48-pixel luma border on every side.
int VP6_InitFrameDetails(PB_INSTANCE* pbi)
{
    const unsigned int luWidth = pbi->Configuration.VideoFrameWidth;
    const unsigned int luHeight = pbi->Configuration.VideoFrameHeight;

    pbi->YDataOffset = 0;
    pbi->ReconYDataOffset = 0;

    pbi->YPlaneSize = luWidth * luHeight;
    pbi->UDataOffset = luWidth * luHeight;
    pbi->VFragments = luHeight >> 3;
    pbi->HFragments = luWidth >> 3;

    pbi->MBRows = (luHeight >> 4) + ((luHeight & 15) != 0 ? 1 : 0) + 6;
    pbi->MBCols = (luWidth >> 4) + ((luWidth & 15) != 0 ? 1 : 0) + 6;
    pbi->MacroBlocks = pbi->MBRows * pbi->MBCols;

    pbi->Configuration.YStride = luWidth + 96;
    pbi->Configuration.UVStride = static_cast<int>(luWidth + 96) / 2;

    pbi->YPlaneFragments = pbi->VFragments * pbi->HFragments;
    pbi->UVPlaneFragments = pbi->YPlaneFragments >> 2;
    pbi->UnitFragments = (pbi->YPlaneFragments * 3) >> 1;

    pbi->UVPlaneSize = (luWidth * luHeight) >> 2;
    pbi->VDataOffset = pbi->UVPlaneSize + pbi->YPlaneSize;

    pbi->ReconYPlaneSize = (luHeight + 96) * (luWidth + 96);
    pbi->ReconUDataOffset = pbi->ReconYPlaneSize;
    pbi->ReconUVPlaneSize = pbi->ReconYPlaneSize >> 2;
    pbi->ReconVDataOffset = pbi->ReconUVPlaneSize + pbi->ReconYPlaneSize;

    const unsigned int luFrameSize = pbi->ReconUVPlaneSize * 2 + pbi->ReconYPlaneSize;

    for (int liNeighbour = 0; liNeighbour < 12; ++liNeighbour)
    {
        pbi->mvNearOffset[liNeighbour] = VP6_NearMacroBlockPositions[liNeighbour][0] * pbi->MBCols +
                                         VP6_NearMacroBlockPositions[liNeighbour][1];
    }

    ChangeFrameInfoConfiguration(pbi->ReconFrameInfo, &pbi->Configuration);

    if (!VP6_AllocateFragmentInfo(pbi))
        return 0;

    if (!VP6_AllocateFrameInfo(pbi, luFrameSize))
    {
        VP6_DeleteFragmentInfo(pbi);
        return 0;
    }

    return 1;
}

void ChangeFrameInfoConfiguration(FRAME_INFO* FrameInfo, const CONFIG_TYPE* Config)
{
    FrameInfo->HFragments = Config->VideoFrameWidth >> 3;
    FrameInfo->VFragments = Config->VideoFrameHeight >> 3;
    FrameInfo->YStride = Config->YStride;
    FrameInfo->UVStride = Config->UVStride;

    const unsigned int luBorder = (Config->YStride - FrameInfo->HFragments * 8) >> 1;
    const unsigned int luHalfBorder = luBorder >> 1;
    FrameInfo->UMVBorder = luBorder;

    FrameInfo->YDataOffset = (Config->YStride + 1) * luBorder;
    FrameInfo->UDataOffset = (Config->VideoFrameHeight + luBorder * 2) * Config->YStride +
                             (Config->UVStride + 1) * luHalfBorder;
    FrameInfo->VDataOffset = (luBorder * 2 + Config->VideoFrameHeight) * Config->YStride +
                             ((Config->VideoFrameHeight >> 1) + luBorder) * Config->UVStride +
                             luHalfBorder * Config->UVStride + luHalfBorder;
}

FRAME_INFO* CreateFrameInfoInstance(const CONFIG_TYPE* Config)
{
    FRAME_INFO* lpFrameInfo = static_cast<FRAME_INFO*>(duck_malloc(sizeof(FRAME_INFO), 0));
    if (!lpFrameInfo)
        return 0;

    memset(lpFrameInfo, 0, sizeof(FRAME_INFO));
    ChangeFrameInfoConfiguration(lpFrameInfo, Config);
    return lpFrameInfo;
}

void DeleteFrameInfoInstance(FRAME_INFO** FrameInfo)
{
    if (*FrameInfo)
    {
        duck_free(*FrameInfo);
        *FrameInfo = 0;
    }
}

}
