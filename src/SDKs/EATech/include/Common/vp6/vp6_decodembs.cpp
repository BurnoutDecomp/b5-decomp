// On2 VP6 decoder: the macroblock loop, macroblock modes and motion vectors, DC prediction and the
// arithmetic-coded coefficient tokens.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

#include <cstring>

namespace
{
    // Macroblock coding modes.
    const int KI_CODE_INTER_NO_MV = 0;
    const int KI_CODE_INTRA = 1;
    const int KI_CODE_INTER_PLUS_MV = 2;
    const int KI_CODE_INTER_NEAREST_MV = 3;
    const int KI_CODE_INTER_NEAR_MV = 4;
    const int KI_CODE_USING_GOLDEN = 5;
    const int KI_CODE_GOLDEN_MV = 6;
    const int KI_CODE_INTER_FOURMV = 7;
    const int KI_CODE_GOLD_NEAREST_MV = 8;
    const int KI_CODE_GOLD_NEAR_MV = 9;

    bool MvIsZero(const MOTION_VECTOR& lMv)
    {
        return lMv.x == 0 && lMv.y == 0;
    }

    bool MvEqual(const MOTION_VECTOR& lA, const MOTION_VECTOR& lB)
    {
        return lA.x == lB.x && lA.y == lB.y;
    }

    // The token decoder's own copy of VP6_DecodeBool128, used for coefficient signs.
    int TokenDecodeBool128(BOOL_CODER* br)
    {
        unsigned int luRange = br->range;
        unsigned int luValue = br->value;
        int liCount = br->count;

        const unsigned int luSplit = (luRange + 1) >> 1;
        const unsigned int luBigSplit = luSplit << 24;

        int liBit;
        if (luValue >= luBigSplit)
        {
            luRange = luRange - luSplit;
            luValue = luValue - luBigSplit;
            liBit = 1;
        }
        else
        {
            luRange = luSplit;
            liBit = 0;
        }

        --liCount;
        luValue <<= 1;
        luRange <<= 1;
        if (liCount == 0)
        {
            liCount = 8;
            luValue |= br->buffer[br->pos++];
        }

        br->count = liCount;
        br->value = luValue;
        br->range = luRange;
        return liBit;
    }

    // A category token's magnitude: its smallest value plus the extra bits.
    int ReadCategoryValue(BOOL_CODER* br, int liCategory)
    {
        const VP6_TOKENEXTRABITS& lExtraBits = VP6_DctExtraBits[liCategory];
        int liValue = lExtraBits.MinVal;
        for (int liBit = lExtraBits.Length; liBit >= 0; --liBit)
            liValue += nDecodeBool(br, lExtraBits.Probs[liBit]) << liBit;
        return liValue;
    }
}

extern "C"
{

// Points at the macroblock row being decoded, for the debugger.
unsigned int* VP6_DecodeMbRow;

void VP6_DecodeFrameMbs(PB_INSTANCE* pbi)
{
    unsigned int luMbRow;
    const unsigned int luMbRows = pbi->MBRows;
    const unsigned int luMbCols = pbi->MBCols;
    VP6_DecodeMbRow = &luMbRow;

    if (pbi->FrameType != 0)
    {
        VP6_DecodeModeProbs(pbi);
        VP6_ConfigureMvEntropyDecoder(pbi, pbi->FrameType);
        pbi->LastMode = KI_CODE_INTER_NO_MV;
    }
    else
    {
        memcpy(pbi->ModeProbs, VP6_DefaultModeProbs, sizeof(pbi->ModeProbs));
        memcpy(pbi->IsMvShortProb, VP6_DefaultIsShortProbs, sizeof(pbi->IsMvShortProb));
        memcpy(pbi->MvShortProbs, VP6_DefaultMvShortProbs, sizeof(pbi->MvShortProbs));
        memcpy(pbi->MvSignProbs, VP6_DefaultSignProbs, sizeof(pbi->MvSignProbs));
        memcpy(pbi->MvSizeProbs, VP6_DefaultMvLongProbs, sizeof(pbi->MvSizeProbs));
        memset(pbi->MbModes, KI_CODE_INTRA, pbi->MacroBlocks);

        memcpy(pbi->ScanBands,
               (pbi->Configuration.Interlaced == 1) ? VP6_DefaultInterlacedScanBands : VP6_DefaultScanBands,
               sizeof(pbi->ScanBands));
        BuildScanOrder(pbi, pbi->ScanBands);
    }

    VP6_ConfigureEntropyDecoder(pbi, pbi->FrameType);
    memcpy(pbi->ScanOrder, pbi->ModifiedScanOrder, sizeof(pbi->ScanOrder));

    if (pbi->UseHuffman)
        ConvertBoolTrees(pbi);

    if (pbi->Configuration.Interlaced == 1)
    {
        unsigned int luProb = 0;
        for (int liBit = 7; liBit >= 0; --liBit)
            luProb |= VP6_DecodeBool128(&pbi->br) << liBit;
        pbi->ProbInterlaced = luProb & 0xFF;
    }

    VP6_ResetAboveContext(pbi);
    memset(pbi->Coeffs, 0, 6 * 64 * sizeof(short));

    pbi->CurrentDcRunLen[0] = 0;
    pbi->CurrentDcRunLen[1] = 0;
    pbi->CurrentAc1RunLen[0] = 0;
    pbi->CurrentAc1RunLen[1] = 0;

    // The outer three macroblocks of every side are border.
    for (luMbRow = 3; luMbRow < luMbRows - 3; ++luMbRow)
    {
        VP6_ResetLeftContext(pbi);
        for (unsigned int luMbCol = 3; luMbCol < luMbCols - 3; ++luMbCol)
            VP6_DecodeMacroBlock(pbi, luMbRow, luMbCol);
    }
}

void VP6_DecodeMacroBlock(PB_INSTANCE* pbi, unsigned int MBrow, unsigned int MBcol)
{
    // Field coding flag, its probability skewed by the previous macroblock's choice.
    if (pbi->Configuration.Interlaced)
    {
        unsigned char lucProb = static_cast<unsigned char>(pbi->ProbInterlaced);
        if (MBcol > 3)
        {
            if (pbi->MbInterlaced)
                lucProb = static_cast<unsigned char>(lucProb - ((lucProb >> 1) & 0x7F));
            else
                lucProb = static_cast<unsigned char>(lucProb + ((256 - lucProb) >> 1));
        }
        pbi->MbInterlaced = nDecodeBool(&pbi->br, lucProb);
    }
    else
    {
        pbi->MbInterlaced = 0;
    }

    if (pbi->FrameType == 0)
    {
        pbi->Mode = KI_CODE_INTRA;
    }
    else
    {
        int liType;
        VP6_FindNearestandNextNearest(pbi, MBrow, MBcol, 1, &liType);

        const int liMode = VP6_DecodeMode(pbi, pbi->LastMode, liType);
        pbi->LastMode = liMode;
        pbi->MbModes[MBrow * pbi->MBCols + MBcol] = static_cast<signed char>(liMode);
        pbi->Mode = liMode;

        if (liMode == KI_CODE_INTER_FOURMV)
        {
            pbi->BlockMode[0] = VP6_DecodeBlockMode(pbi);
            pbi->BlockMode[1] = VP6_DecodeBlockMode(pbi);
            pbi->BlockMode[2] = VP6_DecodeBlockMode(pbi);
            pbi->BlockMode[3] = VP6_DecodeBlockMode(pbi);
            pbi->BlockMode[4] = KI_CODE_INTER_FOURMV;
            pbi->BlockMode[5] = KI_CODE_INTER_FOURMV;

            int liSumX = 0;
            int liSumY = 0;
            for (int liBlock = 0; liBlock < 4; ++liBlock)
            {
                MOTION_VECTOR& lMv = pbi->Mv[liBlock];
                switch (pbi->BlockMode[liBlock])
                {
                case KI_CODE_INTER_NO_MV:
                    lMv.x = 0;
                    lMv.y = 0;
                    break;
                case KI_CODE_INTER_NEAREST_MV:
                    lMv = pbi->NearestInterMv;
                    liSumX += pbi->NearestInterMv.x;
                    liSumY += pbi->NearestInterMv.y;
                    break;
                case KI_CODE_INTER_NEAR_MV:
                    lMv = pbi->NearInterMv;
                    liSumX += pbi->NearInterMv.x;
                    liSumY += pbi->NearInterMv.y;
                    break;
                case KI_CODE_INTER_PLUS_MV:
                {
                    MOTION_VECTOR lDecoded;
                    VP6_decodeMotionVector(pbi, &lDecoded, KI_CODE_INTER_PLUS_MV);
                    liSumX += lDecoded.x;
                    lMv.x = lDecoded.x;
                    liSumY += lDecoded.y;
                    lMv.y = lDecoded.y;
                    break;
                }
                default:
                    break;
                }
            }

            pbi->MbMotionVectors[MBrow * pbi->MBCols + MBcol].x = pbi->Mv[3].x;
            pbi->MbMotionVectors[MBrow * pbi->MBCols + MBcol].y = pbi->Mv[3].y;

            // Chroma uses the rounded average of the four luma vectors.
            const short lsChromaX = static_cast<short>((liSumX + (liSumX >= 0 ? 1 : 0) + 1) >> 2);
            const short lsChromaY = static_cast<short>((liSumY + (liSumY >= 0 ? 1 : 0) + 1) >> 2);
            pbi->Mv[4].x = lsChromaX;
            pbi->Mv[4].y = lsChromaY;
            pbi->Mv[5].x = lsChromaX;
            pbi->Mv[5].y = lsChromaY;
        }
        else
        {
            MOTION_VECTOR lDecoded;
            short lsX;
            short lsY;
            switch (liMode)
            {
            case KI_CODE_INTER_PLUS_MV:
                VP6_decodeMotionVector(pbi, &lDecoded, KI_CODE_INTER_PLUS_MV);
                lsX = lDecoded.x;
                lsY = lDecoded.y;
                break;
            case KI_CODE_INTER_NEAREST_MV:
                lsX = pbi->NearestInterMv.x;
                lsY = pbi->NearestInterMv.y;
                break;
            case KI_CODE_INTER_NEAR_MV:
                lsX = pbi->NearInterMv.x;
                lsY = pbi->NearInterMv.y;
                break;
            case KI_CODE_GOLDEN_MV:
                VP6_FindNearestandNextNearest(pbi, MBrow, MBcol, 2, &liType);
                VP6_decodeMotionVector(pbi, &lDecoded, KI_CODE_GOLDEN_MV);
                lsX = lDecoded.x;
                lsY = lDecoded.y;
                break;
            case KI_CODE_GOLD_NEAREST_MV:
                VP6_FindNearestandNextNearest(pbi, MBrow, MBcol, 2, &liType);
                lsX = pbi->NearestGoldMv.x;
                lsY = pbi->NearestGoldMv.y;
                break;
            case KI_CODE_GOLD_NEAR_MV:
                VP6_FindNearestandNextNearest(pbi, MBrow, MBcol, 2, &liType);
                lsX = pbi->NearGoldMv.x;
                lsY = pbi->NearGoldMv.y;
                break;
            default:
                lsX = 0;
                lsY = 0;
                break;
            }

            pbi->MbMotionVectors[MBrow * pbi->MBCols + MBcol].x = lsX;
            pbi->MbMotionVectors[MBrow * pbi->MBCols + MBcol].y = lsY;
            for (int liBlock = 0; liBlock < 6; ++liBlock)
            {
                pbi->Mv[liBlock].x = lsX;
                pbi->Mv[liBlock].y = lsY;
                pbi->BlockMode[liBlock] = liMode;
            }
        }
    }

    // Luma: four 8x8 blocks; a field-coded macroblock interleaves its two fields, so the lower
    // blocks start one line down and every block steps two lines.
    int liLowerRowOffset;
    if (pbi->MbInterlaced == 0)
    {
        liLowerRowOffset = 8;
        pbi->ReconStride = pbi->Configuration.YStride;
    }
    else
    {
        pbi->ReconStride = pbi->Configuration.YStride * 2;
        liLowerRowOffset = 1;
    }

    pbi->CurrentStride = pbi->Configuration.YStride;
    pbi->CurrentPlane = 0;
    pbi->PixelRow = MBrow * 16;
    pbi->PixelColumn = MBcol * 16;
    const int liYOffset = MBrow * 16 * pbi->Configuration.YStride + pbi->ReconYDataOffset + MBcol * 16;
    pbi->ReconOffset = liYOffset;
    pbi->LastDc = pbi->LastDcValues[0];
    pbi->AboveContext = &pbi->AboveBlockContexts[0][MBcol * 2];
    pbi->LeftContext = &pbi->LeftBlockContexts[0];
    pbi->MvShift = 2;
    pbi->MvMask = 3;
    VP6_DecodeBlock(pbi, MBrow, MBcol, 0);

    pbi->LeftContext = &pbi->LeftBlockContexts[0];
    pbi->AboveContext = &pbi->AboveBlockContexts[0][MBcol * 2 + 1];
    pbi->ReconOffset += 8;
    pbi->PixelColumn += 8;
    VP6_DecodeBlock(pbi, MBrow, MBcol, 1);

    pbi->LeftContext = &pbi->LeftBlockContexts[1];
    pbi->ReconOffset = liLowerRowOffset * pbi->Configuration.YStride + liYOffset;
    pbi->AboveContext = &pbi->AboveBlockContexts[0][MBcol * 2];
    pbi->PixelColumn -= 8;
    pbi->PixelRow += liLowerRowOffset;
    VP6_DecodeBlock(pbi, MBrow, MBcol, 2);

    pbi->LeftContext = &pbi->LeftBlockContexts[1];
    pbi->AboveContext = &pbi->AboveBlockContexts[0][MBcol * 2 + 1];
    pbi->ReconOffset += 8;
    pbi->PixelColumn += 8;
    VP6_DecodeBlock(pbi, MBrow, MBcol, 3);

    // Chroma: one 8x8 block per plane.
    const unsigned int luUVStride = pbi->Configuration.UVStride;
    pbi->MvShift = 3;
    pbi->MvMask = 7;
    pbi->CurrentPlane = 1;
    pbi->CurrentStride = luUVStride;
    pbi->PixelRow = MBrow * 8;
    pbi->AboveContext = &pbi->AboveBlockContexts[1][MBcol];
    pbi->LeftContext = &pbi->LeftBlockContexts[2];
    pbi->LastDc = pbi->LastDcValues[1];
    pbi->PixelColumn = MBcol * 8;
    pbi->ReconStride = luUVStride;
    pbi->ReconOffset = MBrow * 8 * luUVStride + pbi->ReconUDataOffset + MBcol * 8;
    VP6_DecodeBlock(pbi, MBrow, MBcol, 4);

    pbi->LastDc = pbi->LastDcValues[2];
    pbi->LeftContext = &pbi->LeftBlockContexts[3];
    pbi->CurrentPlane = 2;
    pbi->ReconOffset = pbi->PixelRow * luUVStride + pbi->ReconVDataOffset + pbi->PixelColumn;
    pbi->AboveContext = &pbi->AboveBlockContexts[2][MBcol];
    VP6_DecodeBlock(pbi, MBrow, MBcol, 5);
}

// Mode of an inter macroblock: either the previous macroblock's mode again, or one of the other
// nine through the context's mode tree.
int VP6_DecodeMode(PB_INSTANCE* pbi, int LastMode, int Context)
{
    if (nDecodeBool(&pbi->br, pbi->ProbModeSame[Context][LastMode]))
        return LastMode;

    const unsigned char* lpProbs = pbi->ProbMode[Context][LastMode];
    if (nDecodeBool(&pbi->br, lpProbs[0]))
    {
        if (nDecodeBool(&pbi->br, lpProbs[2]))
        {
            if (nDecodeBool(&pbi->br, lpProbs[6]))
                return KI_CODE_GOLD_NEAREST_MV + nDecodeBool(&pbi->br, lpProbs[8]);
            return KI_CODE_USING_GOLDEN + nDecodeBool(&pbi->br, lpProbs[7]);
        }
        return nDecodeBool(&pbi->br, lpProbs[5]) ? KI_CODE_INTER_FOURMV : KI_CODE_INTRA;
    }

    if (nDecodeBool(&pbi->br, lpProbs[1]))
        return KI_CODE_INTER_NEAREST_MV + nDecodeBool(&pbi->br, lpProbs[4]);
    return nDecodeBool(&pbi->br, lpProbs[3]) << 1;
}

// Mode of one luma block of a four-vector macroblock.
int VP6_DecodeBlockMode(PB_INSTANCE* pbi)
{
    const int liHigh = VP6_DecodeBool128(&pbi->br);
    const int liLow = VP6_DecodeBool128(&pbi->br);

    switch (liHigh * 2 + liLow)
    {
    case 1:
        return KI_CODE_INTER_PLUS_MV;
    case 2:
        return KI_CODE_INTER_NEAREST_MV;
    case 3:
        return KI_CODE_INTER_NEAR_MV;
    default:
        return KI_CODE_INTER_NO_MV;
    }
}

// A motion vector coded relative to the nearest vector of the mode's reference frame (when that
// came from one of the two closest neighbours).
void VP6_decodeMotionVector(PB_INSTANCE* pbi, MOTION_VECTOR* mv, int Mode)
{
    int liBaseX = 0;
    int liBaseY = 0;
    if (Mode == KI_CODE_INTER_PLUS_MV)
    {
        if (pbi->NearestMvIndex < 2)
        {
            liBaseX = pbi->NearestInterMv.x;
            liBaseY = pbi->NearestInterMv.y;
        }
    }
    else
    {
        if (pbi->NearestGoldMvIndex < 2)
        {
            liBaseX = pbi->NearestGoldMv.x;
            liBaseY = pbi->NearestGoldMv.y;
        }
    }

    for (unsigned int luComponent = 0; luComponent < 2; ++luComponent)
    {
        int liDelta;
        if (nDecodeBool(&pbi->br, pbi->IsMvShortProb[luComponent]))
        {
            // Long form: eight magnitude bits; bit 3 is implied when no higher bit is set.
            const unsigned char* lpBits = pbi->MvSizeProbs[luComponent];
            liDelta = nDecodeBool(&pbi->br, lpBits[0]);
            liDelta += nDecodeBool(&pbi->br, lpBits[1]) << 1;
            liDelta += nDecodeBool(&pbi->br, lpBits[2]) << 2;
            liDelta += nDecodeBool(&pbi->br, lpBits[7]) << 7;
            liDelta += nDecodeBool(&pbi->br, lpBits[6]) << 6;
            liDelta += nDecodeBool(&pbi->br, lpBits[5]) << 5;
            liDelta += nDecodeBool(&pbi->br, lpBits[4]) << 4;
            if (liDelta & 0xF0)
                liDelta += nDecodeBool(&pbi->br, lpBits[3]) << 3;
            else
                liDelta += 8;
        }
        else
        {
            const unsigned char* lpTree = pbi->MvShortProbs[luComponent];
            if (nDecodeBool(&pbi->br, lpTree[0]))
            {
                if (nDecodeBool(&pbi->br, lpTree[4]))
                    liDelta = 6 + nDecodeBool(&pbi->br, lpTree[6]);
                else
                    liDelta = 4 + nDecodeBool(&pbi->br, lpTree[5]);
            }
            else
            {
                if (nDecodeBool(&pbi->br, lpTree[1]))
                    liDelta = 2 + nDecodeBool(&pbi->br, lpTree[3]);
                else
                    liDelta = nDecodeBool(&pbi->br, lpTree[2]);
            }
        }

        if (liDelta != 0 && nDecodeBool(&pbi->br, pbi->MvSignProbs[luComponent]))
            liDelta = -liDelta;

        if (luComponent == 0)
            mv->x = static_cast<short>(liBaseX + liDelta);
        else
            mv->y = static_cast<short>(liBaseY + liDelta);
    }
}

// The first two distinct non-zero vectors among the neighbours that use reference Frame. Type is
// 0 when both exist, 2 when only the nearest does and 1 when neither does.
void VP6_FindNearestandNextNearest(PB_INSTANCE* pbi, unsigned int MBrow, unsigned int MBcol,
                                   unsigned char Frame, int* Type)
{
    const unsigned int luMb = MBrow * pbi->MBCols + MBcol;
    MOTION_VECTOR lNearest = { 0, 0 };
    MOTION_VECTOR lNear = { 0, 0 };
    int liType = 1;

    int liNeighbour;
    for (liNeighbour = 0; liNeighbour < 12; ++liNeighbour)
    {
        const unsigned int luOffset = pbi->mvNearOffset[liNeighbour] + luMb;
        if (VP6_Mode2Frame[pbi->MbModes[luOffset]] == Frame && !MvIsZero(pbi->MbMotionVectors[luOffset]))
        {
            lNearest = pbi->MbMotionVectors[luOffset];
            liType = 2;
            break;
        }
    }

    const int liNearestIndex = liNeighbour;
    for (++liNeighbour; liNeighbour < 12; ++liNeighbour)
    {
        const unsigned int luOffset = pbi->mvNearOffset[liNeighbour] + luMb;
        if (VP6_Mode2Frame[pbi->MbModes[luOffset]] == Frame)
        {
            const MOTION_VECTOR& lCandidate = pbi->MbMotionVectors[luOffset];
            if (!MvEqual(lCandidate, lNearest) && !MvIsZero(lCandidate))
            {
                lNear = lCandidate;
                liType = 0;
                break;
            }
        }
    }

    if (Frame == 1)
    {
        *Type = liType;
        pbi->NearestMvIndex = liNearestIndex;
        pbi->NearestInterMv = lNearest;
        pbi->NearInterMv = lNear;
    }
    else
    {
        pbi->NearestGoldMvIndex = liNearestIndex;
        pbi->NearestGoldMv = lNearest;
        pbi->NearGoldMv = lNear;
    }
}

// Add the predicted DC (from the left and above blocks that share the reference frame, else the
// last DC of that frame) and remember the result.
void VP6_PredictDC(PB_INSTANCE* pbi, unsigned int bp, short* LastDc, BLOCK_CONTEXT* Above, BLOCK_CONTEXT* Left)
{
    const unsigned int luFrame = static_cast<unsigned char>(VP6_Mode2Frame[pbi->Mode]);
    unsigned char lucCount = 0;
    int liPrediction = 0;

    if (luFrame == Left->Frame)
    {
        liPrediction = Left->Dc;
        lucCount = 1;
    }
    if (luFrame == Above->Frame)
    {
        liPrediction += Above->Dc;
        lucCount = static_cast<unsigned char>(lucCount + 1);
    }

    if (lucCount == 0)
        liPrediction = LastDc[luFrame];
    else if (lucCount == 2)
        liPrediction = (liPrediction + ((liPrediction >> 15) & 1)) >> 1;

    pbi->Coeffs[bp * 64] = static_cast<short>(pbi->Coeffs[bp * 64] + liPrediction);
    LastDc[luFrame] = pbi->Coeffs[bp * 64];
}

// Arithmetic-coded tokens of one block (DC with its neighbour context, then the AC run), returning
// the end-of-block position.
unsigned char VP6_ReadTokensPredictA(PB_INSTANCE* pbi, short* Coeffs, unsigned int Plane,
                                     unsigned char* AboveToken, unsigned char* LeftToken)
{
    BOOL_CODER* br = &pbi->br2;
    const unsigned char* lpDcNode = pbi->DcNodeContexts[Plane][*AboveToken + *LeftToken];
    const unsigned char* lpDcProbs = pbi->DcProbs[Plane];
    unsigned char (*lpAcProbs)[6][11] = pbi->AcProbs[Plane];

    unsigned char lucPrec;
    if (!nDecodeBool(br, lpDcNode[0]))
    {
        *LeftToken = 0;
        lucPrec = 0;
        *AboveToken = 0;
    }
    else
    {
        *LeftToken = 1;
        *AboveToken = 1;

        int liValue;
        if (!nDecodeBool(br, lpDcNode[2]))
        {
            lucPrec = 1;
            const int liSign = TokenDecodeBool128(br);
            liValue = (1 ^ -liSign) + liSign;
        }
        else
        {
            lucPrec = 2;
            if (!nDecodeBool(br, lpDcNode[3]))
            {
                const int liMagnitude = nDecodeBool(br, lpDcNode[4]) ? 3 + nDecodeBool(br, lpDcProbs[5]) : 2;
                const int liSign = TokenDecodeBool128(br);
                liValue = (liMagnitude ^ -liSign) + liSign;
            }
            else
            {
                int liCategory;
                if (nDecodeBool(br, lpDcProbs[6]))
                {
                    if (nDecodeBool(br, lpDcProbs[8]))
                        liCategory = 9 + nDecodeBool(br, lpDcProbs[10]);
                    else
                        liCategory = 7 + nDecodeBool(br, lpDcProbs[9]);
                }
                else
                {
                    liCategory = 5 + nDecodeBool(br, lpDcProbs[7]);
                }

                const int liMagnitude = ReadCategoryValue(br, liCategory);
                const int liSign = TokenDecodeBool128(br);
                liValue = (liMagnitude ^ -liSign) + liSign;
            }
        }
        Coeffs[0] = static_cast<short>(liValue);
    }

    unsigned char lucPosition = 1;
    do
    {
        const unsigned char* lpProbs = lpAcProbs[lucPrec][VP6_CoeffToBand[lucPosition]];

        // After a zero run the next token cannot be another zero or the end of block.
        if (lucPosition <= 1 || lucPrec != 0)
        {
            if (!nDecodeBool(br, lpProbs[0]))
            {
                if (!nDecodeBool(br, lpProbs[1]))
                {
                    ++lucPosition;
                    break;
                }

                const unsigned char* lpRun = pbi->ZeroRunProbs[lucPosition >= 6 ? 1 : 0];
                lucPrec = 0;

                int liRun;
                if (!nDecodeBool(br, lpRun[0]))
                {
                    if (!nDecodeBool(br, lpRun[1]))
                        liRun = 1 + nDecodeBool(br, lpRun[2]);
                    else
                        liRun = 3 + nDecodeBool(br, lpRun[3]);
                }
                else if (!nDecodeBool(br, lpRun[4]))
                {
                    if (!nDecodeBool(br, lpRun[5]))
                        liRun = 5 + nDecodeBool(br, lpRun[6]);
                    else
                        liRun = 7 + nDecodeBool(br, lpRun[7]);
                }
                else
                {
                    liRun = nDecodeBool(br, lpRun[8]);
                    liRun += nDecodeBool(br, lpRun[9]) << 1;
                    liRun += nDecodeBool(br, lpRun[10]) << 2;
                    liRun += nDecodeBool(br, lpRun[11]) << 3;
                    liRun += nDecodeBool(br, lpRun[12]) << 4;
                    liRun += nDecodeBool(br, lpRun[13]) << 5;
                    liRun += 9;
                }

                lucPosition = static_cast<unsigned char>(lucPosition + liRun);
                continue;
            }
        }

        int liValue;
        if (!nDecodeBool(br, lpProbs[2]))
        {
            lucPrec = 1;
            const int liSign = TokenDecodeBool128(br);
            liValue = (1 ^ -liSign) + liSign;
        }
        else
        {
            lucPrec = 2;
            if (!nDecodeBool(br, lpProbs[3]))
            {
                const int liMagnitude = nDecodeBool(br, lpProbs[4]) ? 3 + nDecodeBool(br, lpProbs[5]) : 2;
                const int liSign = TokenDecodeBool128(br);
                liValue = (liMagnitude ^ -liSign) + liSign;
            }
            else
            {
                int liCategory;
                if (nDecodeBool(br, lpProbs[6]))
                {
                    if (nDecodeBool(br, lpProbs[8]))
                        liCategory = 9 + nDecodeBool(br, lpProbs[10]);
                    else
                        liCategory = 7 + nDecodeBool(br, lpProbs[9]);
                }
                else
                {
                    liCategory = 5 + nDecodeBool(br, lpProbs[7]);
                }

                const int liMagnitude = ReadCategoryValue(br, liCategory);
                const int liSign = TokenDecodeBool128(br);
                liValue = (liMagnitude ^ -liSign) + liSign;
            }
        }

        Coeffs[pbi->ModifiedScanOrder[lucPosition]] = static_cast<short>(liValue);
        ++lucPosition;
    } while (lucPosition < 64);

    return pbi->EobOffsetTable[static_cast<unsigned char>(lucPosition - 1)];
}

}
