// On2 VP6 decoder: dequantisation tables.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

extern "C"
{

void (*VP6_BuildQuantIndex)(QUANTIZER* pbi);

// Scan position of every raster coefficient.
void VP6_BuildQuantIndex_Generic(QUANTIZER* pbi)
{
    for (int liScanPosition = 0; liScanPosition < 64; ++liScanPosition)
    {
        pbi->QuantIndex[VP6_ZigZag[liScanPosition]] = static_cast<unsigned char>(liScanPosition);
    }
}

// Build the luma and chroma dequantisation factors of the current quantiser index (scaled by 4),
// stored in raster order through QuantIndex, then the same factors as floats for the vector
// transform.
void VP6_init_dequantizer(QUANTIZER* pbi)
{
    for (int liCoeff = 1; liCoeff < 64; ++liCoeff)
    {
        pbi->DequantCoeffs[0][pbi->QuantIndex[liCoeff]] =
            static_cast<short>(VP6_QThreshTable[pbi->FrameQIndex] << 2);
    }
    pbi->DequantCoeffs[0][0] = static_cast<short>(VP6_DcQuant[pbi->FrameQIndex] << 2);

    for (int liCoeff = 1; liCoeff < 64; ++liCoeff)
    {
        pbi->DequantCoeffs[1][pbi->QuantIndex[liCoeff]] =
            static_cast<short>(VP6_UvQThreshTable[pbi->FrameQIndex] << 2);
    }
    pbi->DequantCoeffs[1][0] = static_cast<short>(VP6_UvDcQuant[pbi->FrameQIndex] << 2);

    for (int liCoeff = 0; liCoeff < 64; ++liCoeff)
    {
        pbi->FloatDequantCoeffs[0][liCoeff] = static_cast<float>(pbi->DequantCoeffs[0][liCoeff]);
        pbi->FloatDequantCoeffs[1][liCoeff] = static_cast<float>(pbi->DequantCoeffs[1][liCoeff]);
    }
}

// Rebuild the tables when the quantiser changed since they were last built.
void VP6_UpdateQ(QUANTIZER* pbi)
{
    if (static_cast<unsigned int>(VP6_QThreshTable[pbi->FrameQIndex]) !=
        static_cast<unsigned int>(pbi->LastFrameQuantizerValue))
    {
        pbi->LastFrameQuantizerValue = pbi->ThisFrameQuantizerValue;
        VP6_BuildQuantIndex(pbi);
        VP6_init_dequantizer(pbi);
    }
}

static void DeleteQuantizerTables(QUANTIZER* pbi)
{
    if (pbi->DequantCoeffs[0])
        duck_freeAlign(pbi->DequantCoeffs[0]);
    pbi->DequantCoeffs[0] = 0;

    if (pbi->DequantCoeffs[1])
        duck_freeAlign(pbi->DequantCoeffs[1]);
    pbi->DequantCoeffs[1] = 0;

    if (pbi->FloatDequantCoeffs[0])
        duck_freeAlign(pbi->FloatDequantCoeffs[0]);
    pbi->FloatDequantCoeffs[0] = 0;

    if (pbi->FloatDequantCoeffs[1])
        duck_freeAlign(pbi->FloatDequantCoeffs[1]);
    pbi->FloatDequantCoeffs[1] = 0;
}

static int AllocateQuantizerTables(QUANTIZER* pbi)
{
    DeleteQuantizerTables(pbi);

    pbi->DequantCoeffs[0] = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->DequantCoeffs[0])
    {
        DeleteQuantizerTables(pbi);
        return 0;
    }

    pbi->DequantCoeffs[1] = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->DequantCoeffs[1])
    {
        DeleteQuantizerTables(pbi);
        return 0;
    }

    pbi->FloatDequantCoeffs[0] = static_cast<float*>(duck_mallocAlign(64 * sizeof(float), 128, 0));
    pbi->FloatDequantCoeffs[1] = static_cast<float*>(duck_mallocAlign(64 * sizeof(float), 128, 0));
    return 1;
}

void VP6_DeleteQuantizer(QUANTIZER** pbi)
{
    if (*pbi)
    {
        DeleteQuantizerTables(*pbi);
        duck_free(*pbi);
        *pbi = 0;
    }
}

QUANTIZER* VP6_CreateQuantizer(void)
{
    QUANTIZER* pbi = static_cast<QUANTIZER*>(duck_malloc(sizeof(QUANTIZER), 0));
    if (!pbi)
        return 0;

    memset(pbi, 0, sizeof(QUANTIZER));
    if (!AllocateQuantizerTables(pbi))
    {
        VP6_DeleteQuantizer(&pbi);
    }
    return pbi;
}

}
