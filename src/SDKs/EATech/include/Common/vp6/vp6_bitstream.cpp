// On2 VP6 decoder: the boolean (arithmetic) decoder, the raw header bit reader and the frame
// header.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

extern "C"
{

void VP6_StartDecode(BOOL_CODER* br, const unsigned char* source)
{
    br->buffer = source;
    br->pos = 0;
    br->lowvalue = 0;
    br->range = 255;
    br->count = 8;
    br->pos = 4;
    br->value = Vp6ReadBigEndian32(source);
}

void InitHeaderBuffer(HEADER_BUFFER* Header, const unsigned char* Buffer)
{
    Header->buffer = Buffer;
    Header->bits_available = 32;
    Header->pos = 4;
    Header->value = Vp6ReadBigEndian32(Buffer);
}

// Read BitsToRead (up to 32) bits, most significant first.
unsigned int ReadHeaderBits(HEADER_BUFFER* Header, unsigned int BitsToRead)
{
    unsigned int luPos = Header->pos;
    unsigned int luBitsAvailable = Header->bits_available;
    unsigned int luValue = Header->value;
    unsigned int luHighBits = 0;

    if (luBitsAvailable < BitsToRead)
    {
        // Take what is left of the current word, then continue in the next one.
        BitsToRead -= luBitsAvailable;
        luHighBits = Vp6Slw(Vp6Srw(luValue, 32 - luBitsAvailable), BitsToRead);
        luValue = Vp6ReadBigEndian32(Header->buffer + luPos);
        luPos += 4;
        luBitsAvailable = 32;
    }

    Header->pos = luPos;
    Header->bits_available = luBitsAvailable - BitsToRead;
    Header->value = Vp6Slw(luValue, BitsToRead);
    return Vp6Srw(luValue, 32 - BitsToRead) | luHighBits;
}

// Decode one bool of the given probability (of a 0, in 1/256).
int VP6_DecodeBool(BOOL_CODER* br, int probability)
{
    int liBit = 0;
    unsigned int luRange = br->range;
    unsigned int luValue = br->value;
    int liCount = br->count;

    const unsigned int luSplit = 1 + (((luRange - 1) * probability) >> 8);
    const unsigned int luBigSplit = luSplit << 24;

    if (luValue >= luBigSplit)
    {
        luRange = luRange - luSplit;
        luValue = luValue - luBigSplit;
        liBit = 1;
    }
    else
    {
        luRange = luSplit;
    }

    if (luRange < 0x80)
    {
        do
        {
            --liCount;
            luRange <<= 1;
            luValue <<= 1;
            if (liCount == 0)
            {
                liCount = 8;
                luValue |= br->buffer[br->pos++];
            }
        } while (luRange < 0x80);
        br->count = liCount;
    }

    br->range = luRange;
    br->value = luValue;
    return liBit;
}

// Decode one bool of probability one half; the range always needs exactly one shift.
int VP6_DecodeBool128(BOOL_CODER* br)
{
    int liBit;
    unsigned int luRange = br->range;
    unsigned int luValue = br->value;
    int liCount = br->count;

    const unsigned int luSplit = (luRange + 1) >> 1;
    const unsigned int luBigSplit = luSplit << 24;

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

    luRange <<= 1;
    luValue <<= 1;
    if (--liCount == 0)
    {
        liCount = 8;
        luValue |= br->buffer[br->pos++];
    }

    br->count = liCount;
    br->value = luValue;
    br->range = luRange;
    return liBit;
}

// The token decoder's copy of VP6_DecodeBool.
int nDecodeBool(BOOL_CODER* br, int probability)
{
    int liBit = 0;
    unsigned int luRange = br->range;
    unsigned int luValue = br->value;
    int liCount = br->count;

    const unsigned int luSplit = 1 + (((luRange - 1) * probability) >> 8);
    const unsigned int luBigSplit = luSplit << 24;

    if (luValue >= luBigSplit)
    {
        luRange = luRange - luSplit;
        luValue = luValue - luBigSplit;
        liBit = 1;
    }
    else
    {
        luRange = luSplit;
    }

    while (luRange < 0x80)
    {
        --liCount;
        luRange <<= 1;
        luValue <<= 1;
        if (liCount == 0)
        {
            liCount = 8;
            luValue |= br->buffer[br->pos++];
        }
    }

    br->count = liCount;
    br->value = luValue;
    br->range = luRange;
    return liBit;
}

// Parse the frame header: frame type, quantiser and partition offset from the raw bits; on a key
// frame the picture size (re-initialising the instance when it changed), otherwise the golden-frame
// refresh flag; then whether the coefficients are Huffman coded.
static int VP6_LoadFrameHeader(PB_INSTANCE* pbi)
{
    HEADER_BUFFER* lpHeader = &pbi->HeaderBits;

    pbi->FrameType = static_cast<unsigned char>(ReadHeaderBits(lpHeader, 1));
    const unsigned char lucQIndex = static_cast<unsigned char>(ReadHeaderBits(lpHeader, 6));
    pbi->MultiStream = static_cast<unsigned char>(ReadHeaderBits(lpHeader, 1));

    if (pbi->FrameType == 0)
    {
        ReadHeaderBits(lpHeader, 5);   // codec version
        ReadHeaderBits(lpHeader, 2);   // profile
        pbi->Configuration.Interlaced = static_cast<unsigned char>(ReadHeaderBits(lpHeader, 1));

        VP6_StartDecode(&pbi->br, lpHeader->buffer + 4);
        pbi->Buff2Offset = ReadHeaderBits(lpHeader, 16);

        unsigned int luMbRows = 0;
        for (int liBit = 7; liBit >= 0; --liBit)
            luMbRows |= VP6_DecodeBool128(&pbi->br) << liBit;
        const unsigned int luVFragments = (luMbRows & 0xFF) << 1;

        unsigned int luMbCols = 0;
        for (int liBit = 7; liBit >= 0; --liBit)
            luMbCols |= VP6_DecodeBool128(&pbi->br) << liBit;
        const unsigned int luHFragments = (luMbCols & 0xFF) << 1;

        // Displayed size and scaling mode: not used by this decoder.
        for (int liBit = 7; liBit >= 0; --liBit)
            VP6_DecodeBool128(&pbi->br);
        for (int liBit = 7; liBit >= 0; --liBit)
            VP6_DecodeBool128(&pbi->br);
        for (int liBit = 1; liBit >= 0; --liBit)
            VP6_DecodeBool128(&pbi->br);

        if (luVFragments != pbi->VFragments || luHFragments != pbi->HFragments)
        {
            pbi->Configuration.VideoFrameWidth = luHFragments << 3;
            pbi->Configuration.VideoFrameHeight = luVFragments << 3;
            VP6_InitFrameDetails(pbi);
        }
    }
    else
    {
        VP6_StartDecode(&pbi->br, lpHeader->buffer + 3);
        pbi->Buff2Offset = ReadHeaderBits(lpHeader, 16);
        pbi->RefreshGoldenFrame = VP6_DecodeBool(&pbi->br, 128);
    }

    pbi->UseHuffman = VP6_DecodeBool(&pbi->br, 128);

    pbi->quantizer->FrameQIndex = lucQIndex;
    pbi->quantizer->ThisFrameQuantizerValue = VP6_QThreshTable[lucQIndex];
    VP6_UpdateQ(pbi->quantizer);
    return 1;
}

int VP6_LoadFrame(PB_INSTANCE* pbi)
{
    int liResult = 1;
    if (!VP6_LoadFrameHeader(pbi))
        liResult = 0;
    return liResult;
}

}
