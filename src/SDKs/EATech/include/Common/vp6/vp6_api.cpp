// On2 VP6 decoder: library set-up and playback-instance allocation.

// XDK size optimization matches the instance buffer allocation at 0x82B58758.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

extern "C"
{

// ---- library and instance ------------------------------------------------------------------

void VP6_VPInitLibrary(void)
{
    VP6_DMachineSpecificConfig();
    InitVPUtil();
}

void VP6_DeleteTmpBuffers(PB_INSTANCE* pbi)
{
    if (pbi->ReconDataBuffer)
        duck_freeAlign(pbi->ReconDataBuffer);
    if (pbi->PredictionBuffer)
        duck_freeAlign(pbi->PredictionBuffer);
    if (pbi->TmpDataBuffer)
        duck_freeAlign(pbi->TmpDataBuffer);
    if (pbi->FilterTmpBuffer)
        duck_freeAlign(pbi->FilterTmpBuffer);

    pbi->ReconDataBuffer = 0;
    pbi->PredictionBuffer = 0;
    pbi->TmpDataBuffer = 0;
    pbi->FilterTmpBuffer = 0;
}

int VP6_AllocateTmpBuffers(PB_INSTANCE* pbi)
{
    VP6_DeleteTmpBuffers(pbi);

    pbi->ReconDataBuffer = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->ReconDataBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->PredictionBuffer = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->PredictionBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->TmpDataBuffer = duck_mallocAlign(64 * sizeof(short), 128, 0);
    if (!pbi->TmpDataBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->FilterTmpBuffer = duck_mallocAlign(512, 128, 0);
    if (!pbi->FilterTmpBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    return 1;
}

void VP6_DeletePBInstance(xPB_INST* pbi)
{
    if (*pbi)
    {
        VP6_DeleteTmpBuffers(*pbi);
        VP6_DeleteQuantizer(&(*pbi)->quantizer);
        DeleteFrameInfoInstance(&(*pbi)->ReconFrameInfo);
    }

    duck_freeAlign(*pbi);
    *pbi = 0;
}

PB_INSTANCE* VP6_CreatePBInstance(void)
{
    CONFIG_TYPE lConfiguration = {};

    PB_INSTANCE* pbi = static_cast<PB_INSTANCE*>(duck_mallocAlign(sizeof(PB_INSTANCE), 16, 0));
    if (!pbi)
        return 0;

    memset(pbi, 0, sizeof(PB_INSTANCE));
    memcpy(&pbi->Configuration, &lConfiguration, sizeof(CONFIG_TYPE));

    if (!VP6_AllocateTmpBuffers(pbi))
    {
        duck_freeAlign(pbi);
        return 0;
    }

    pbi->PostProcessingLevel = 0;
    memset(pbi->DcProbs, 0, sizeof(pbi->DcProbs));
    memset(pbi->AcProbs, 0, sizeof(pbi->AcProbs));
    pbi->CreateCodeArrays = 0;
    return pbi;
}

}
