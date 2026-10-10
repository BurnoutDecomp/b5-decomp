// On2 VP6 decoder: per-frame probability updates, coefficient scan order, block contexts and the
// macroblock mode trees.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

namespace
{
    // A probability sent as a seven-bit literal: doubled, never zero.
    unsigned char ReadProbability(BOOL_CODER* br)
    {
        unsigned int luValue = 0;
        for (int liBit = 6; liBit >= 0; --liBit)
            luValue |= VP6_DecodeBool128(br) << liBit;

        const unsigned char lucProb = static_cast<unsigned char>(luValue << 1);
        return static_cast<unsigned char>(lucProb + (lucProb == 0 ? 1 : 0));
    }
}

extern "C"
{

// Scan order from the band of each coefficient (bands in order, raster order within a band), and
// the end-of-block position each scan position implies.
void BuildScanOrder(PB_INSTANCE* pbi, const unsigned char* ScanBands)
{
    pbi->ModifiedScanOrder[0] = 0;

    unsigned int luScanPosition = 1;
    for (unsigned int luBand = 0; luBand < 16; ++luBand)
    {
        for (unsigned int luCoeff = 1; luCoeff < 64; ++luCoeff)
        {
            if (ScanBands[luCoeff] == luBand)
                pbi->ModifiedScanOrder[luScanPosition++] = static_cast<unsigned char>(luCoeff);
        }
    }

    unsigned int luHighest = 0;
    for (int liScanPosition = 0; liScanPosition < 64; ++liScanPosition)
    {
        if (pbi->ModifiedScanOrder[liScanPosition] > luHighest)
            luHighest = pbi->ModifiedScanOrder[liScanPosition];
        pbi->EobOffsetTable[liScanPosition] = static_cast<unsigned char>(luHighest + 1);
    }
}

// Read the frame's coefficient probability updates. On a key frame a probability that is not
// updated takes the last value read for the same node.
void VP6_ConfigureEntropyDecoder(PB_INSTANCE* pbi, unsigned char FrameType)
{
    unsigned char laPrevProb[11];
    memset(laPrevProb, 0x80, sizeof(laPrevProb));

    for (int liPlane = 0; liPlane < 2; ++liPlane)
    {
        for (unsigned int luNode = 0; luNode < 11; ++luNode)
        {
            if (nDecodeBool(&pbi->br, VP6_DcUpdateProbs[liPlane][luNode]))
            {
                laPrevProb[luNode] = ReadProbability(&pbi->br);
                pbi->DcProbs[liPlane][luNode] = laPrevProb[luNode];
            }
            else if (FrameType == 0)
            {
                pbi->DcProbs[liPlane][luNode] = laPrevProb[luNode];
            }
        }
    }

    if (FrameType == 0)
        memcpy(pbi->ZeroRunProbs, VP6_DefaultZeroRunProbs, sizeof(pbi->ZeroRunProbs));

    if (nDecodeBool(&pbi->br, 128))
    {
        for (unsigned int luCoeff = 1; luCoeff < 64; ++luCoeff)
        {
            if (nDecodeBool(&pbi->br, VP6_ScanBandUpdateProbs[luCoeff]))
            {
                unsigned int luBand = 0;
                for (int liBit = 3; liBit >= 0; --liBit)
                    luBand |= VP6_DecodeBool128(&pbi->br) << liBit;
                pbi->ScanBands[luCoeff] = static_cast<unsigned char>(luBand);
            }
        }
        BuildScanOrder(pbi, pbi->ScanBands);
    }

    for (int liPlane = 0; liPlane < 2; ++liPlane)
    {
        for (unsigned int luNode = 0; luNode < 14; ++luNode)
        {
            if (nDecodeBool(&pbi->br, VP6_ZeroRunUpdateProbs[liPlane][luNode]))
                pbi->ZeroRunProbs[liPlane][luNode] = ReadProbability(&pbi->br);
        }
    }

    for (int liPrec = 0; liPrec < 3; ++liPrec)
    {
        for (int liPlane = 0; liPlane < 2; ++liPlane)
        {
            for (int liBand = 0; liBand < 6; ++liBand)
            {
                for (unsigned int luNode = 0; luNode < 11; ++luNode)
                {
                    if (nDecodeBool(&pbi->br, VP6_AcUpdateProbs[liPrec][liPlane][liBand][luNode]))
                    {
                        laPrevProb[luNode] = ReadProbability(&pbi->br);
                        pbi->AcProbs[liPlane][liPrec][liBand][luNode] = laPrevProb[luNode];
                    }
                    else if (FrameType == 0)
                    {
                        pbi->AcProbs[liPlane][liPrec][liBand][luNode] = laPrevProb[luNode];
                    }
                }
            }
        }
    }

    VP6_ConfigureContexts(pbi);
}

// DC node probabilities for each neighbour context, a linear function of the frame's DC
// probabilities clamped to 1..255.
void VP6_ConfigureContexts(PB_INSTANCE* pbi)
{
    ClearSysState();

    for (int liPlane = 0; liPlane < 2; ++liPlane)
    {
        for (int liContext = 0; liContext < 3; ++liContext)
        {
            for (unsigned int luNode = 0; luNode < 5; ++luNode)
            {
                int liProb = ((pbi->DcProbs[liPlane][luNode] * VP6_DcNodeEqs[luNode][liContext][0] + 128) >> 8) +
                             VP6_DcNodeEqs[luNode][liContext][1];
                if (liProb > 255)
                    liProb = 255;
                if (liProb < 1)
                    liProb = 1;
                pbi->DcNodeContexts[liPlane][liContext][luNode] = static_cast<unsigned char>(liProb);
            }
        }
    }
}

void VP6_ResetLeftContext(PB_INSTANCE* pbi)
{
    for (int liBlock = 0; liBlock < 4; ++liBlock)
        memset(&pbi->LeftBlockContexts[liBlock], 0, sizeof(BLOCK_CONTEXT));

    for (int liBlock = 0; liBlock < 4; ++liBlock)
    {
        pbi->LeftBlockContexts[liBlock].BlockMode = -1;
        pbi->LeftBlockContexts[liBlock].Frame = 4;
    }
}

void VP6_ResetAboveContext(PB_INSTANCE* pbi)
{
    for (unsigned int luBlock = 0; luBlock < pbi->HFragments + 8; ++luBlock)
    {
        BLOCK_CONTEXT& lContext = pbi->AboveBlockContexts[0][luBlock];
        lContext.BlockMode = -1;
        lContext.Frame = 4;
        lContext.Dc = 0;
        lContext.Token = 0;
    }

    for (unsigned int luBlock = 0; luBlock < (pbi->HFragments >> 1) + 8; ++luBlock)
    {
        for (int liPlane = 1; liPlane < 3; ++liPlane)
        {
            BLOCK_CONTEXT& lContext = pbi->AboveBlockContexts[liPlane][luBlock];
            lContext.BlockMode = -1;
            lContext.Frame = 4;
            lContext.Token = 0;
            lContext.Dc = 0;
        }
    }

    pbi->LastDcValues[0][0] = 0;
    pbi->LastDcValues[1][0] = 128;
    pbi->LastDcValues[2][0] = 128;
    for (int liFrame = 1; liFrame < 3; ++liFrame)
    {
        pbi->LastDcValues[0][liFrame] = 0;
        pbi->LastDcValues[1][liFrame] = 0;
        pbi->LastDcValues[2][liFrame] = 0;
    }
}

// Read the frame's motion-vector probability updates (a zero update becomes 1).
void VP6_ConfigureMvEntropyDecoder(PB_INSTANCE* pbi, unsigned char /*FrameType*/)
{
    for (int liComponent = 0; liComponent < 2; ++liComponent)
    {
        if (VP6_DecodeBool(&pbi->br, VP6_MvUpdateProbs[liComponent][0]))
        {
            unsigned int luValue = 0;
            for (int liBit = 6; liBit >= 0; --liBit)
                luValue |= VP6_DecodeBool128(&pbi->br) << liBit;
            pbi->IsMvShortProb[liComponent] = static_cast<unsigned char>(luValue << 1);
            if (!pbi->IsMvShortProb[liComponent])
                pbi->IsMvShortProb[liComponent] = 1;
        }

        if (VP6_DecodeBool(&pbi->br, VP6_MvUpdateProbs[liComponent][1]))
        {
            unsigned int luValue = 0;
            for (int liBit = 6; liBit >= 0; --liBit)
                luValue |= VP6_DecodeBool128(&pbi->br) << liBit;
            pbi->MvSignProbs[liComponent] = static_cast<unsigned char>(luValue << 1);
            if (!pbi->MvSignProbs[liComponent])
                pbi->MvSignProbs[liComponent] = 1;
        }
    }

    for (int liComponent = 0; liComponent < 2; ++liComponent)
    {
        for (unsigned int luNode = 0; luNode < 7; ++luNode)
        {
            if (VP6_DecodeBool(&pbi->br, VP6_MvUpdateProbs[liComponent][2 + luNode]))
            {
                unsigned int luValue = 0;
                for (int liBit = 6; liBit >= 0; --liBit)
                    luValue |= VP6_DecodeBool128(&pbi->br) << liBit;
                pbi->MvShortProbs[liComponent][luNode] = static_cast<unsigned char>(luValue << 1);
                if (!pbi->MvShortProbs[liComponent][luNode])
                    pbi->MvShortProbs[liComponent][luNode] = 1;
            }
        }
    }

    for (int liComponent = 0; liComponent < 2; ++liComponent)
    {
        for (unsigned int luBit = 0; luBit < 8; ++luBit)
        {
            if (VP6_DecodeBool(&pbi->br, VP6_MvUpdateProbs[liComponent][9 + luBit]))
            {
                unsigned int luValue = 0;
                for (int liBit = 6; liBit >= 0; --liBit)
                    luValue |= VP6_DecodeBool128(&pbi->br) << liBit;
                pbi->MvSizeProbs[liComponent][luBit] = static_cast<unsigned char>(luValue << 1);
                if (!pbi->MvSizeProbs[liComponent][luBit])
                    pbi->MvSizeProbs[liComponent][luBit] = 1;
            }
        }
    }
}

// For every context and previous mode: the probability of repeating the previous mode, and the
// node probabilities of the tree over the other nine modes.
void VP6_BuildModeTree(PB_INSTANCE* pbi)
{
    for (int liLastMode = 0; liLastMode < 10; ++liLastMode)
    {
        for (int liContext = 0; liContext < 3; ++liContext)
        {
            unsigned int p[10];
            unsigned int luTotal = 0;
            for (int liMode = 0; liMode < 10; ++liMode)
            {
                p[liMode] = (liLastMode == liMode) ? 0u : pbi->ModeProbs[liContext][0][liMode] * 100u;
                luTotal += p[liMode];
            }
            luTotal += 1;

            const int liModeProb = pbi->ModeProbs[liContext][0][liLastMode];
            const int liSameProb = pbi->ModeProbs[liContext][1][liLastMode];
            pbi->ProbModeSame[liContext][liLastMode] =
                static_cast<unsigned char>(255 - (liSameProb * 255) / (liModeProb + liSameProb + 1));

            unsigned char* lpNode = pbi->ProbMode[liContext][liLastMode];
            lpNode[0] = static_cast<unsigned char>((p[4] + p[3] + p[2] + p[0]) * 255 / luTotal + 1);
            lpNode[1] = static_cast<unsigned char>((p[2] + p[0]) * 255 / (p[4] + p[3] + p[2] + p[0] + 1) + 1);
            lpNode[2] = static_cast<unsigned char>((p[7] + p[1]) * 255 /
                                                   (p[9] + p[8] + p[6] + p[5] + p[7] + p[1] + 1) + 1);
            lpNode[3] = static_cast<unsigned char>(p[0] * 255 / (p[2] + p[0] + 1) + 1);
            lpNode[4] = static_cast<unsigned char>(p[3] * 255 / (p[4] + p[3] + 1) + 1);
            lpNode[5] = static_cast<unsigned char>(p[1] * 255 / (p[7] + p[1] + 1) + 1);
            lpNode[6] = static_cast<unsigned char>((p[6] + p[5]) * 255 / (p[9] + p[8] + p[6] + p[5] + 1) + 1);
            lpNode[7] = static_cast<unsigned char>(p[5] * 255 / (p[6] + p[5] + 1) + 1);
            lpNode[8] = static_cast<unsigned char>(p[8] * 255 / (p[9] + p[8] + 1) + 1);
        }
    }
}

// A signed mode-probability delta.
int VP6_decodeModeDiff(PB_INSTANCE* pbi)
{
    if (!VP6_DecodeBool(&pbi->br, 205))
        return 0;

    const int liSign = 1 - 2 * VP6_DecodeBool128(&pbi->br);

    if (!VP6_DecodeBool(&pbi->br, 171))
        return liSign * (1 << (3 - VP6_DecodeBool(&pbi->br, 83)));

    if (!VP6_DecodeBool(&pbi->br, 199))
    {
        if (VP6_DecodeBool(&pbi->br, 140))
            return liSign * 12;
        if (VP6_DecodeBool(&pbi->br, 125))
            return liSign * 16;
        if (VP6_DecodeBool(&pbi->br, 104))
            return liSign * 20;
        return liSign * 24;
    }

    int liMagnitude = 0;
    for (int liBit = 6; liBit >= 0; --liBit)
        liMagnitude |= VP6_DecodeBool128(&pbi->br) << liBit;
    return (liMagnitude * liSign) * 4;
}

// Read the frame's mode probabilities: optionally a vector-quantised set per context, then optional
// deltas, and rebuild the mode trees.
void VP6_DecodeModeProbs(PB_INSTANCE* pbi)
{
    for (int liContext = 0; liContext < 3; ++liContext)
    {
        if (VP6_DecodeBool(&pbi->br, 174))
        {
            int liSet = 0;
            for (int liBit = 3; liBit >= 0; --liBit)
                liSet |= VP6_DecodeBool128(&pbi->br) << liBit;

            const unsigned char* lpSet = VP6_ModeVq[liContext][liSet];
            for (int liMode = 0; liMode < 10; ++liMode)
            {
                pbi->ModeProbs[liContext][1][liMode] = lpSet[liMode * 2];
                pbi->ModeProbs[liContext][0][liMode] = lpSet[liMode * 2 + 1];
            }
        }

        if (VP6_DecodeBool(&pbi->br, 254))
        {
            for (int liMode = 0; liMode < 10; ++liMode)
            {
                int liProb = pbi->ModeProbs[liContext][1][liMode] + VP6_decodeModeDiff(pbi);
                if (liProb < 0)
                    liProb = 0;
                else if (liProb > 255)
                    liProb = 255;
                pbi->ModeProbs[liContext][1][liMode] = static_cast<unsigned char>(liProb);

                liProb = pbi->ModeProbs[liContext][0][liMode] + VP6_decodeModeDiff(pbi);
                if (liProb < 0)
                    liProb = 0;
                else if (liProb > 255)
                    liProb = 255;
                pbi->ModeProbs[liContext][0][liMode] = static_cast<unsigned char>(liProb);
            }
        }
    }

    VP6_BuildModeTree(pbi);
}

}
