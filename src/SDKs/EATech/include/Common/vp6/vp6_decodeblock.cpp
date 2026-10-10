// On2 VP6 decoder: one 8x8 block - its coefficient tokens (Huffman or arithmetic coded), DC
// prediction, the inverse transform and the reconstruction into the current frame.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

namespace
{
    const unsigned int KU_ZERO_TOKEN = 0;
    const unsigned int KU_DCT_EOB_TOKEN = 11;

    const int KI_CODE_INTER_NO_MV = 0;
    const int KI_CODE_USING_GOLDEN = 5;

    // Reference frame codes of VP6_Mode2Frame.
    const int KI_GOLDEN_FRAME = 2;

    // ---- the Huffman partition's bit reader ------------------------------------------------

    // Peek the next 6 or 8 bits without consuming them (may look into the next word).
    inline unsigned int HuffPeekBits(const HUFF_BIT_READER* br, int liBits)
    {
        const unsigned int luValid = br->Value & (Vp6Slw(1u, static_cast<unsigned int>(br->BitsLeft)) - 1u);
        if (static_cast<unsigned int>(br->BitsLeft) >= static_cast<unsigned int>(liBits))
            return Vp6Srw(luValid, static_cast<unsigned int>(br->BitsLeft - liBits));
        return Vp6Srw((luValid << 8) | *br->Position, static_cast<unsigned int>(br->BitsLeft + 8 - liBits));
    }

    inline void HuffConsume(HUFF_BIT_READER* br, int liBits)
    {
        br->BitsLeft -= liBits;
        if (br->BitsLeft < 0)
        {
            const unsigned char* lpWord = br->Position;
            br->BitsLeft += 32;
            br->Position = lpWord + 4;
            br->Value = Vp6ReadBigEndian32(lpWord);
        }
    }

    inline unsigned int HuffReadBit(HUFF_BIT_READER* br)
    {
        if (br->BitsLeft != 0)
        {
            --br->BitsLeft;
            return (br->Value >> br->BitsLeft) & 1;
        }

        const unsigned char* lpWord = br->Position;
        br->BitsLeft = 31;
        br->Position = lpWord + 4;
        br->Value = Vp6ReadBigEndian32(lpWord);
        return br->Value >> 31;
    }

    inline unsigned int HuffReadBits(HUFF_BIT_READER* br, int liBits)
    {
        br->Value &= VP6_LoMaskTbl[br->BitsLeft];

        unsigned int luHighBits = 0;
        int liMissing = liBits - br->BitsLeft;
        if (liMissing > 0)
        {
            luHighBits = br->Value << liMissing;
            liMissing -= 32;
            const unsigned char* lpWord = br->Position;
            br->Position = lpWord + 4;
            br->Value = Vp6ReadBigEndian32(lpWord);
        }

        br->BitsLeft = -liMissing;
        return (br->Value >> br->BitsLeft) | luHighBits;
    }

    // One token: six-bit look-ahead, then a bit-by-bit tree walk when the code is longer.
    unsigned int HuffDecodeToken(HUFF_BIT_READER* br, const HUFF_LUT_ENTRY* lpTable, const HUFF_NODE* lpTree)
    {
        const HUFF_LUT_ENTRY lEntry = lpTable[HuffPeekBits(br, 6)];
        HuffConsume(br, lEntry.Length);
        if (lEntry.Leaf)
            return lEntry.Value & 0x1F;

        HUFF_CHILD lNode;
        lNode.Value = lEntry.Value & 0x1F;
        do
        {
            const unsigned int luBit = HuffReadBit(br);
            const unsigned int luIndex = lNode.Value;
            lNode = luBit ? lpTree[luIndex].Right : lpTree[luIndex].Left;
        } while (!lNode.Leaf);
        return lNode.Value;
    }

    // Length of a run of blocks: 1..74 in a short prefix code.
    int HuffReadRunLength(HUFF_BIT_READER* br)
    {
        int liRun = static_cast<int>(HuffReadBits(br, 2)) + 1;
        if (liRun == 3)
        {
            liRun = static_cast<int>(HuffReadBits(br, 2)) + 3;
        }
        else if (liRun == 4)
        {
            if (HuffReadBit(br))
                liRun = static_cast<int>(HuffReadBits(br, 6)) + 11;
            else
                liRun = static_cast<int>(HuffReadBits(br, 2)) + 7;
        }
        return liRun;
    }

    // Magnitude of a value token: its smallest value plus the extra bits.
    int HuffReadTokenValue(HUFF_BIT_READER* br, unsigned int luToken)
    {
        int liValue = VP6_DctRangeMinVals[luToken];
        if (luToken > 4)
        {
            if (luToken <= 9)
                liValue += static_cast<int>(HuffReadBits(br, static_cast<int>(luToken) - 4));
            else
                liValue += static_cast<int>(HuffReadBits(br, 11));
        }
        return liValue;
    }

    // Huffman-coded tokens of one block, returning the end-of-block position. Runs of blocks with a
    // zero DC, or with no AC coefficient at all, are carried across blocks per plane.
    unsigned char HuffReadTokens(PB_INSTANCE* pbi, short* Coeffs, int liPlane)
    {
        HUFF_BIT_READER* br = &pbi->br3;
        int liPrec;

        if (pbi->CurrentDcRunLen[liPlane] > 0)
        {
            --pbi->CurrentDcRunLen[liPlane];
            liPrec = 0;
        }
        else
        {
            const TOKEN_LUT_ENTRY& lEntry = pbi->DcTokenLUT[liPlane][HuffPeekBits(br, 8)];
            if (lEntry.Bits)
            {
                HuffConsume(br, lEntry.Bits);
                if (lEntry.Eob)
                    return pbi->EobOffsetTable[0];

                pbi->CurrentDcRunLen[liPlane] = lEntry.Run;
                Coeffs[0] = lEntry.Value;
                liPrec = lEntry.Context;
            }
            else
            {
                const unsigned int luToken = HuffDecodeToken(br, pbi->DcHuffLUT[liPlane], pbi->DcHuffTree[liPlane]);
                if (luToken == KU_DCT_EOB_TOKEN)
                    return pbi->EobOffsetTable[0];

                if (luToken == KU_ZERO_TOKEN)
                {
                    pbi->CurrentDcRunLen[liPlane] = HuffReadRunLength(br) - 1;
                    liPrec = 0;
                }
                else
                {
                    const int liValue = HuffReadTokenValue(br, luToken);
                    const int liSign = static_cast<int>(HuffReadBit(br));
                    Coeffs[0] = static_cast<short>((liValue ^ -liSign) + liSign);
                    liPrec = (liValue > 1) ? 2 : 1;
                }
            }
        }

        if (pbi->CurrentAc1RunLen[liPlane] > 0)
        {
            --pbi->CurrentAc1RunLen[liPlane];
            return pbi->EobOffsetTable[0];
        }

        int liPosition = 1;
        do
        {
            const int liBand = VP6_CoeffToHuffBand[liPosition];
            const unsigned int luToken = HuffDecodeToken(br, pbi->AcHuffLUT[liPrec][liPlane][liBand],
                                                         pbi->AcHuffTree[liPrec][liPlane][liBand]);

            if (luToken == KU_ZERO_TOKEN)
            {
                const int liRunClass = (liPosition >= 6) ? 1 : 0;
                const unsigned int luRunToken =
                    HuffDecodeToken(br, pbi->ZeroHuffLUT[liRunClass], pbi->ZeroHuffTree[liRunClass]);
                if (luRunToken < 8)
                    liPosition = static_cast<int>(luRunToken) + liPosition + 1;
                else
                    liPosition = static_cast<int>(HuffReadBits(br, 6)) + liPosition + 8 + 1;
                liPrec = 0;
            }
            else if (luToken == KU_DCT_EOB_TOKEN)
            {
                if (liPosition == 1)
                    pbi->CurrentAc1RunLen[liPlane] = HuffReadRunLength(br) - 1;
                break;
            }
            else
            {
                const int liValue = HuffReadTokenValue(br, luToken);
                const int liSign = static_cast<int>(HuffReadBit(br));
                Coeffs[pbi->ScanOrder[liPosition]] = static_cast<short>((liValue ^ -liSign) + liSign);
                liPrec = (liValue > 1) ? 2 : 1;
                ++liPosition;
            }
        } while (liPosition < 64);

        return pbi->EobOffsetTable[liPosition - 1];
    }
}

extern "C"
{

void VP6_DecodeBlock(PB_INSTANCE* pbi, unsigned int /*MBrow*/, unsigned int /*MBcol*/, unsigned int bp)
{
    short* lpCoeffs = pbi->Coeffs + bp * 64;

    unsigned char lucEob;
    if (pbi->UseHuffman)
    {
        lucEob = HuffReadTokens(pbi, lpCoeffs, pbi->CurrentPlane != 0 ? 1 : 0);
    }
    else
    {
        lucEob = VP6_ReadTokensPredictA(pbi, lpCoeffs, pbi->CurrentPlane != 0 ? 1u : 0u,
                                        &pbi->AboveContext->Token, &pbi->LeftContext->Token);
    }

    VP6_PredictDC(pbi, bp, pbi->LastDc, pbi->AboveContext, pbi->LeftContext);

    // Leave this block's context for its neighbours.
    pbi->LeftContext->BlockMode = pbi->BlockMode[bp];
    pbi->AboveContext->BlockMode = pbi->LeftContext->BlockMode;
    pbi->LeftContext->Dc = lpCoeffs[0];
    pbi->AboveContext->Dc = pbi->LeftContext->Dc;
    pbi->LeftContext->Frame = static_cast<unsigned short>(VP6_Mode2Frame[pbi->Mode]);
    pbi->AboveContext->Frame = pbi->LeftContext->Frame;

    // Inverse transform into the residual, sized by how far the coefficients reach.
    const unsigned int luPlane = VP6_BlockToPlane[bp];
    if (lucEob <= 1)
    {
        IDct1_Xenon(lpCoeffs, pbi->quantizer->DequantCoeffs[luPlane], pbi->ReconDataBuffer);
    }
    else if (lucEob <= 10)
    {
        IDct10_Xenon(lpCoeffs, pbi->quantizer->FloatDequantCoeffs[luPlane], pbi->ReconDataBuffer,
                     static_cast<unsigned char*>(pbi->FilterTmpBuffer));
    }
    else
    {
        IDctSlow_Xenon(lpCoeffs, pbi->quantizer->FloatDequantCoeffs[luPlane], pbi->ReconDataBuffer,
                       static_cast<unsigned char*>(pbi->FilterTmpBuffer));
    }

    // Reconstruction.
    const int liMode = pbi->Mode;
    unsigned char* lpDest = pbi->ThisFrameRecon + pbi->ReconOffset;

    if (liMode == KI_CODE_INTER_NO_MV)
    {
        ScalarReconInter_Xenon(lpDest, pbi->LastFrameRecon + pbi->ReconOffset, pbi->ReconDataBuffer,
                               pbi->ReconStride);
    }
    else if (VP6_ModeUsesMC[liMode])
    {
        unsigned char* lpReference = (VP6_Mode2Frame[liMode] == KI_GOLDEN_FRAME) ? pbi->GoldenFrame : pbi->LastFrameRecon;
        const int liStride = pbi->CurrentStride;
        const int liMask = pbi->MvMask;
        const int liShift = pbi->MvShift;

        // Whole-pixel part of the vector (rounded toward zero) and its sub-pixel fraction.
        const int liMvX = pbi->Mv[bp].x;
        const int liMvY = pbi->Mv[bp].y;
        const int liPixelX = (liMvX + ((liMvX >> 31) & liMask)) >> liShift;
        const int liPixelY = (liMvY + ((liMvY >> 31) & liMask)) >> liShift;
        int liFractionX = liMvX & liMask;
        int liFractionY = liMvY & liMask;

        const unsigned char* lpSource = lpReference + liPixelY * liStride + liPixelX + pbi->ReconOffset;

        // Offset of the second reference sample towards the fraction.
        int liFilterOffset = 0;
        if (liFractionX)
            liFilterOffset = (pbi->Mv[bp].x > 0 ? 1 : 0) * 2 - 1;
        if (liFractionY)
            liFilterOffset += ((pbi->Mv[bp].y > 0 ? 1 : 0) * 2 - 1) * liStride;

        if (liFilterOffset != 0)
        {
            // Luma vectors are quarter-pel: double them to the eighth-pel tap index.
            if (bp < 4)
            {
                liFractionX <<= 1;
                liFractionY <<= 1;
            }

            const unsigned char* lpFirst;
            const unsigned char* lpSecond;
            if (liFilterOffset > 0)
            {
                lpFirst = lpSource;
                lpSecond = lpSource + liFilterOffset;
            }
            else
            {
                lpFirst = lpSource + liFilterOffset;
                lpSecond = lpSource;
            }

            FilterBlock_Xenon(lpFirst, lpSecond, pbi->PredictionBuffer, liStride, liFractionX, liFractionY, 0,
                              static_cast<float*>(pbi->FilterTmpBuffer));
        }
        else
        {
            UnpackBlock_C(const_cast<unsigned char*>(lpSource), pbi->PredictionBuffer, liStride);
        }

        ReconBlock_Xenon(pbi->PredictionBuffer, pbi->ReconDataBuffer, lpDest, pbi->ReconStride);
    }
    else if (liMode == KI_CODE_USING_GOLDEN)
    {
        ScalarReconInter_Xenon(lpDest, pbi->GoldenFrame + pbi->ReconOffset, pbi->ReconDataBuffer, pbi->ReconStride);
    }
    else
    {
        ScalarReconIntra_Xenon(lpDest, pbi->ReconDataBuffer, pbi->ReconStride);
    }
}

}
