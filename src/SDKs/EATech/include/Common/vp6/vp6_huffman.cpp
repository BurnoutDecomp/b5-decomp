// On2 VP6 decoder: Huffman trees for the Huffman-coded coefficient partition, derived from the
// arithmetic coder's token probabilities.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

namespace
{
    const int KI_DCT_TOKENS = 12;        // ZERO, ONE .. CATEGORY6, end-of-block
    const int KI_ZERO_RUN_TOKENS = 9;
    const int KI_DCT_EOB_TOKEN = 11;
    const int KI_ZERO_TOKEN = 0;

    // Entry of the frequency-sorted list the tree is built from.
    struct SORT_NODE
    {
        int        Next;    // index of the next (more frequent) entry, -1 at the end
        int        Freq;
        HUFF_CHILD Value;   // the token, or the tree node this entry stands for
    };
}

extern "C"
{

// Token frequencies (in 1/256) of the DCT token tree from its eleven node probabilities.
void BoolTreeToHuffCodes(const unsigned char* BoolTreeProbs, unsigned int* HuffProbs)
{
    const unsigned char* p = BoolTreeProbs;

    HuffProbs[11] = (p[0] * p[1]) >> 8;
    HuffProbs[0] = ((255 - p[1]) * p[0]) >> 8;
    HuffProbs[1] = (p[2] * (255 - p[0])) >> 8;

    const unsigned int luNode2 = ((255 - p[2]) * (255 - p[0])) >> 8;
    const unsigned int luNode3 = (p[3] * luNode2) >> 8;
    HuffProbs[2] = (p[4] * luNode3) >> 8;

    const unsigned int luNode4 = ((255 - p[4]) * luNode3) >> 8;
    HuffProbs[3] = (p[5] * luNode4) >> 8;
    HuffProbs[4] = ((255 - p[5]) * luNode4) >> 8;

    const unsigned int luNode5 = ((255 - p[3]) * luNode2) >> 8;
    const unsigned int luNode6 = (p[6] * luNode5) >> 8;
    HuffProbs[5] = (p[7] * luNode6) >> 8;
    HuffProbs[6] = ((255 - p[7]) * luNode6) >> 8;

    const unsigned int luNode7 = ((255 - p[6]) * luNode5) >> 8;
    const unsigned int luNode8 = (p[8] * luNode7) >> 8;
    HuffProbs[7] = (p[9] * luNode8) >> 8;
    HuffProbs[8] = ((255 - p[9]) * luNode8) >> 8;

    const unsigned int luNode9 = ((255 - p[8]) * luNode7) >> 8;
    HuffProbs[9] = (p[10] * luNode9) >> 8;
    HuffProbs[10] = ((255 - p[10]) * luNode9) >> 8;
}

// Run frequencies (in 1/256) of the zero-run tree from its node probabilities.
void ZerosBoolTreeToHuffCodes(const unsigned char* BoolTreeProbs, unsigned int* HuffProbs)
{
    const unsigned char* p = BoolTreeProbs;

    const unsigned int luNode1 = (p[0] * p[1]) >> 8;
    HuffProbs[0] = (p[2] * luNode1) >> 8;
    HuffProbs[1] = ((255 - p[2]) * luNode1) >> 8;

    const unsigned int luNode3 = ((255 - p[1]) * p[0]) >> 8;
    HuffProbs[2] = (p[3] * luNode3) >> 8;
    HuffProbs[3] = ((255 - p[3]) * luNode3) >> 8;

    const unsigned int luNode5 = ((((255 - p[0]) * p[4]) >> 8) * p[5]) >> 8;
    HuffProbs[4] = (p[6] * luNode5) >> 8;
    HuffProbs[5] = ((255 - p[6]) * luNode5) >> 8;

    const unsigned int luNode7 = ((((255 - p[0]) * p[4]) >> 8) * (255 - p[5])) >> 8;
    HuffProbs[6] = (p[7] * luNode7) >> 8;
    HuffProbs[7] = ((255 - p[7]) * luNode7) >> 8;

    HuffProbs[8] = ((255 - p[0]) * (255 - p[4])) >> 8;
}

// Rebuild every Huffman tree and look-up table from the current probabilities.
void ConvertBoolTrees(PB_INSTANCE* pbi)
{
    if (pbi->CreateCodeArrays)
    {
        for (int liPlane = 0; liPlane < 2; ++liPlane)
        {
            BoolTreeToHuffCodes(pbi->DcProbs[liPlane], pbi->DcHuffProbs[liPlane]);
            VP6_BuildHuffTree(pbi->DcHuffTree[liPlane], pbi->DcHuffProbs[liPlane], KI_DCT_TOKENS);
            VP6_BuildHuffLookupTable(pbi->DcHuffTree[liPlane], pbi->DcHuffLUT[liPlane]);
            VP6_BuildDCUnpackLookupTable(pbi->DcHuffTree[liPlane], pbi->DcTokenLUT[liPlane]);
            VP6_CreateCodeArray(pbi->DcHuffTree[liPlane], 0, pbi->DcHuffCode[liPlane], pbi->DcHuffLength[liPlane], 0, 0);
        }

        for (int liPlane = 0; liPlane < 2; ++liPlane)
        {
            ZerosBoolTreeToHuffCodes(pbi->ZeroRunProbs[liPlane], pbi->ZeroHuffProbs[liPlane]);
            VP6_BuildHuffTree(pbi->ZeroHuffTree[liPlane], pbi->ZeroHuffProbs[liPlane], KI_ZERO_RUN_TOKENS);
            VP6_BuildHuffLookupTable(pbi->ZeroHuffTree[liPlane], pbi->ZeroHuffLUT[liPlane]);
            VP6_CreateCodeArray(pbi->ZeroHuffTree[liPlane], 0, pbi->ZeroHuffCode[liPlane], pbi->ZeroHuffLength[liPlane], 0, 0);
        }

        for (int liPrec = 0; liPrec < 3; ++liPrec)
        {
            for (int liPlane = 0; liPlane < 2; ++liPlane)
            {
                for (int liBand = 0; liBand < 6; ++liBand)
                {
                    BoolTreeToHuffCodes(pbi->AcProbs[liPlane][liPrec][liBand], pbi->AcHuffProbs[liPrec][liPlane][liBand]);
                    VP6_BuildHuffTree(pbi->AcHuffTree[liPrec][liPlane][liBand], pbi->AcHuffProbs[liPrec][liPlane][liBand],
                                      KI_DCT_TOKENS);
                    VP6_BuildHuffLookupTable(pbi->AcHuffTree[liPrec][liPlane][liBand], pbi->AcHuffLUT[liPrec][liPlane][liBand]);
                    VP6_CreateCodeArray(pbi->AcHuffTree[liPrec][liPlane][liBand], 0, pbi->AcHuffCode[liPrec][liPlane][liBand],
                                        pbi->AcHuffLength[liPrec][liPlane][liBand], 0, 0);
                }
            }
        }
    }
    else
    {
        for (int liPlane = 0; liPlane < 2; ++liPlane)
        {
            BoolTreeToHuffCodes(pbi->DcProbs[liPlane], pbi->DcHuffProbs[liPlane]);
            VP6_BuildHuffTree(pbi->DcHuffTree[liPlane], pbi->DcHuffProbs[liPlane], KI_DCT_TOKENS);
            VP6_BuildHuffLookupTable(pbi->DcHuffTree[liPlane], pbi->DcHuffLUT[liPlane]);
            VP6_BuildDCUnpackLookupTable(pbi->DcHuffTree[liPlane], pbi->DcTokenLUT[liPlane]);
        }

        for (int liPlane = 0; liPlane < 2; ++liPlane)
        {
            ZerosBoolTreeToHuffCodes(pbi->ZeroRunProbs[liPlane], pbi->ZeroHuffProbs[liPlane]);
            VP6_BuildHuffTree(pbi->ZeroHuffTree[liPlane], pbi->ZeroHuffProbs[liPlane], KI_ZERO_RUN_TOKENS);
            VP6_BuildHuffLookupTable(pbi->ZeroHuffTree[liPlane], pbi->ZeroHuffLUT[liPlane]);
        }

        for (int liPrec = 0; liPrec < 3; ++liPrec)
        {
            for (int liPlane = 0; liPlane < 2; ++liPlane)
            {
                for (int liBand = 0; liBand < 6; ++liBand)
                {
                    BoolTreeToHuffCodes(pbi->AcProbs[liPlane][liPrec][liBand], pbi->AcHuffProbs[liPrec][liPlane][liBand]);
                    VP6_BuildHuffTree(pbi->AcHuffTree[liPrec][liPlane][liBand], pbi->AcHuffProbs[liPrec][liPlane][liBand],
                                      KI_DCT_TOKENS);
                    VP6_BuildHuffLookupTable(pbi->AcHuffTree[liPrec][liPlane][liBand], pbi->AcHuffLUT[liPrec][liPlane][liBand]);
                }
            }
        }
    }
}

// Build a Huffman tree over Values tokens from their frequencies (a zero frequency counts as 1).
// Internal nodes are filled from HuffTreeRoot[Values - 2] down to the root at HuffTreeRoot[0].
void VP6_BuildHuffTree(HUFF_NODE* HuffTreeRoot, unsigned int* Counts, int Values)
{
    SORT_NODE laList[64];
    int liHead = 0;
    int liNextNode = Values - 1;

    for (int liToken = 0; liToken < Values; ++liToken)
    {
        laList[liToken].Value.Value = liToken;
        laList[liToken].Value.Leaf = 1;
        if (Counts[liToken] == 0)
            Counts[liToken] = 1;
        laList[liToken].Next = -1;
        laList[liToken].Freq = static_cast<int>(Counts[liToken]);
    }

    // Sort the leaves into a list of ascending frequency.
    for (int liEntry = 1; liEntry < Values; ++liEntry)
    {
        int liCurrent = liHead;
        int liPrevious = liHead;
        if (liHead != -1)
        {
            while (laList[liEntry].Freq > laList[liCurrent].Freq)
            {
                liPrevious = liCurrent;
                liCurrent = laList[liCurrent].Next;
                if (liCurrent == -1)
                    break;
            }
        }

        if (liCurrent == liHead)
            liHead = liEntry;
        else
            laList[liPrevious].Next = liEntry;
        laList[liEntry].Next = liCurrent;
    }

    // Repeatedly join the two least frequent entries under a new node.
    int liFree = Values;
    while (laList[liHead].Next != -1)
    {
        const int liSecond = laList[liHead].Next;
        --liNextNode;

        HUFF_NODE& lNode = HuffTreeRoot[liNextNode];
        lNode.Left = laList[liHead].Value;

        const int liSum = laList[liHead].Freq + laList[liSecond].Freq;
        laList[liFree].Value.Value = liNextNode;
        laList[liFree].Value.Leaf = 0;
        laList[liFree].Next = -1;
        laList[liFree].Freq = liSum;

        lNode.Right = laList[liSecond].Value;
        lNode.Freq = static_cast<unsigned char>((laList[liHead].Freq << 8) / liSum);

        const int liStart = laList[liSecond].Next;
        int liCurrent = liStart;
        int liPrevious = liStart;
        if (liStart != -1)
        {
            while (liSum > laList[liCurrent].Freq)
            {
                liPrevious = liCurrent;
                liCurrent = laList[liCurrent].Next;
                if (liCurrent == -1)
                    break;
            }
        }

        if (liCurrent == liStart)
        {
            liHead = liFree;
        }
        else
        {
            laList[liPrevious].Next = liFree;
            liHead = liStart;
        }
        laList[liFree].Next = liCurrent;
        ++liFree;
    }
}

// Where a walk of up to six bits from the root ends, for every six-bit pattern.
void VP6_BuildHuffLookupTable(const HUFF_NODE* HuffTreeRoot, HUFF_LUT_ENTRY* HuffTable)
{
    for (int liPattern = 0; liPattern < 64; ++liPattern)
    {
        HUFF_CHILD lNode;
        lNode.Value = 0;
        lNode.Leaf = 0;

        int liBit = 6;
        int liLength = 0;
        do
        {
            --liBit;
            const unsigned int luIndex = lNode.Value;
            ++liLength;
            lNode = ((liPattern >> liBit) & 1) ? HuffTreeRoot[luIndex].Right : HuffTreeRoot[luIndex].Left;
        } while (!lNode.Leaf && liBit > 0);

        HuffTable[liPattern].Leaf = lNode.Leaf;
        HuffTable[liPattern].Value = lNode.Value;
        HuffTable[liPattern].Length = liLength;
    }
}

// Fully decode every eight-bit pattern that holds a whole DC token (with its extra bits and sign,
// or its zero-run length).
void VP6_BuildDCUnpackLookupTable(const HUFF_NODE* HuffTreeRoot, TOKEN_LUT_ENTRY* DcTable)
{
    for (int liPattern = 0; liPattern < 256; ++liPattern)
    {
        TOKEN_LUT_ENTRY& lEntry = DcTable[liPattern];
        int liUsed = 0;
        lEntry.Value = 0;
        lEntry.Eob = 0;
        lEntry.Run = 0;
        lEntry.Context = 0;
        lEntry.Bits = 0;

        HUFF_CHILD lNode;
        lNode.Value = 0;
        lNode.Leaf = 0;

        int liBit = 8;
        do
        {
            --liBit;
            ++liUsed;
            const unsigned int luIndex = lNode.Value;
            lNode = ((liPattern >> liBit) & 1) ? HuffTreeRoot[luIndex].Right : HuffTreeRoot[luIndex].Left;
        } while (!lNode.Leaf && liBit > 0);

        if (!lNode.Leaf)
        {
            lEntry.Value = 0;
            lEntry.Eob = 0;
            lEntry.Run = 0;
            lEntry.Context = 0;
            lEntry.Bits = 0;
            continue;
        }

        const int liToken = lNode.Value;
        if (liToken == KI_DCT_EOB_TOKEN)
        {
            lEntry.Eob = 1;
            lEntry.Run = 0;
            lEntry.Context = 0;
            lEntry.Bits = liUsed;
        }
        else if (liToken == KI_ZERO_TOKEN)
        {
            if (8 - liUsed < 2)
                continue;
            liUsed += 2;
            int liRun = ((liPattern >> (8 - liUsed)) & 3) + 1;
            if (liRun == 3)
            {
                if (8 - liUsed < 2)
                    continue;
                liUsed += 2;
                liRun = ((liPattern >> (8 - liUsed)) & 3) + 3;
            }
            else if (liRun == 4)
            {
                if (8 - liUsed < 3)
                    continue;
                liUsed += 1;
                if ((liPattern >> (8 - liUsed)) & 1)
                {
                    if (8 - liUsed < 6)
                        continue;
                    liUsed += 6;
                    liRun = ((liPattern >> (8 - liUsed)) & 0x3F) + 11;
                }
                else
                {
                    liUsed += 2;
                    liRun = ((liPattern >> (8 - liUsed)) & 3) + 7;
                }
            }

            lEntry.Run = liRun - 1;
            lEntry.Bits = liUsed;
        }
        else
        {
            int liValue = VP6_DctRangeMinVals[liToken];
            if (liToken > 4)
            {
                if (liToken <= 9)
                {
                    const int liExtraBits = liToken - 4;
                    if (8 - liUsed < liExtraBits)
                        continue;
                    liUsed += liExtraBits;
                    liValue += (liPattern >> (8 - liUsed)) & ((1 << liExtraBits) - 1);
                }
                else
                {
                    if (8 - liUsed < 11)
                        continue;
                    liUsed += 11;
                    liValue += (liPattern >> (8 - liUsed)) & 0x7FF;
                }
            }

            if (8 - liUsed < 1)
                continue;
            ++liUsed;
            const int liSign = (liPattern >> (8 - liUsed)) & 1;
            lEntry.Value = static_cast<short>((liValue ^ -liSign) + liSign);
            lEntry.Context = (liValue > 1) ? 2 : 1;
            lEntry.Bits = liUsed;
        }
    }
}

// Code word and length of every token, by walking the tree.
void VP6_CreateCodeArray(const HUFF_NODE* HuffRoot, int HuffTreeIndex, unsigned int* HuffCodeArray,
                         unsigned char* HuffCodeLengthArray, unsigned int CodeValue, int CodeLength)
{
    const HUFF_NODE& lNode = HuffRoot[HuffTreeIndex];

    if (lNode.Left.Leaf)
    {
        HuffCodeArray[lNode.Left.Value] = CodeValue << 1;
        HuffCodeLengthArray[lNode.Left.Value] = static_cast<unsigned char>(CodeLength + 1);
    }
    else
    {
        VP6_CreateCodeArray(HuffRoot, lNode.Left.Value, HuffCodeArray, HuffCodeLengthArray, CodeValue << 1,
                            CodeLength + 1);
    }

    if (lNode.Right.Leaf)
    {
        HuffCodeArray[lNode.Right.Value] = (CodeValue << 1) + 1;
        HuffCodeLengthArray[lNode.Right.Value] = static_cast<unsigned char>(CodeLength + 1);
    }
    else
    {
        VP6_CreateCodeArray(HuffRoot, lNode.Right.Value, HuffCodeArray, HuffCodeLengthArray, (CodeValue << 1) + 1,
                            CodeLength + 1);
    }
}

}
