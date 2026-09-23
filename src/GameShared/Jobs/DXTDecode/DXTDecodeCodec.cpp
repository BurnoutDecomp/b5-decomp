// dxt_decode - the DXT1 decoder the network texture decode job runs.
//
// Each block's two endpoints are widened to 8 bits per channel; a four-colour block (colour 0
// the greater) interpolates its middle entries at 3/8 and 5/8, a three-colour block takes the
// midpoint and transparent black. Every pixel is written as the four bytes A, R, G, B.
//
// [PC platform layer] byte order. The endpoints are read as big-endian half-words, the way the
// console reads the byte-swapped blocks fdxt writes; everything else is byte access, so the
// decoded bytes are the console's.

#include "GameShared/Jobs/DXTDecode/DXTDecodeJob.h"

namespace
{
    inline u32 LoadU16(const u8* lpBytes)
    {
        return (static_cast<u32>(lpBytes[0]) << 8) | static_cast<u32>(lpBytes[1]);
    }

    // Decode one block into four rows of four pixels at lpDst (liPitch bytes apart).
    //   lbFiveFiveFive  the endpoints are 1:5:5:5 (fields low to high in bytes 1, 2, 3 of the
    //                   pixel) instead of 5:6:5 (red, green, blue in bytes 1, 2, 3)
    //   lbSwapped       the endpoint half-words are byte-swapped, and each row is written
    //                   right to left
    // dxt_decode passes neither.
    void DecodeColourBlock(u8* lpDst, s32 liPitch, const u8* lpSrc, s32 lbFiveFiveFive, s32 lbSwapped)
    {
        u32 luColour0 = LoadU16(lpSrc);
        u32 luColour1 = LoadU16(lpSrc + 2);
        if (lbSwapped != 0)
        {
            luColour0 = (((luColour0 & 0xFF) << 8) | ((luColour0 >> 8) & 0xFF)) & 0xFFFF;
            luColour1 = (((luColour1 & 0xFF) << 8) | ((luColour1 >> 8) & 0xFF)) & 0xFFFF;
        }

        // The endpoint fields in pixel byte order (bytes 1, 2, 3) and the middle field's
        // widening divisor.
        u32 lauField0[3];
        u32 lauField1[3];
        u32 luMiddleMax;
        if (lbFiveFiveFive == 0)
        {
            lauField0[0] = (luColour0 >> 11) & 0x1F;
            lauField0[1] = (luColour0 >> 5) & 0x3F;
            lauField0[2] = luColour0 & 0x1F;
            lauField1[0] = (luColour1 >> 11) & 0x1F;
            lauField1[1] = (luColour1 >> 5) & 0x3F;
            lauField1[2] = luColour1 & 0x1F;
            luMiddleMax  = 0x3F;
        }
        else
        {
            lauField0[0] = luColour0 & 0x1F;
            lauField0[1] = (luColour0 >> 5) & 0x1F;
            lauField0[2] = (luColour0 >> 10) & 0x1F;
            lauField1[0] = luColour1 & 0x1F;
            lauField1[1] = (luColour1 >> 5) & 0x1F;
            lauField1[2] = (luColour1 >> 10) & 0x1F;
            luMiddleMax  = 0x1F;
        }

        u32 lauChannel0[3];
        u32 lauChannel1[3];
        for (s32 liChannel = 0; liChannel < 3; ++liChannel)
        {
            const u32 luMax = (liChannel == 1) ? luMiddleMax : 0x1F;
            lauChannel0[liChannel] = ((lauField0[liChannel] * 255 + (luMax + 1) / 2) / luMax) & 0xFF;
            lauChannel1[liChannel] = ((lauField1[liChannel] * 255 + (luMax + 1) / 2) / luMax) & 0xFF;
        }

        u8 lauPalette[4][4];
        lauPalette[0][0] = 0xFF;
        lauPalette[1][0] = 0xFF;
        lauPalette[2][0] = 0xFF;
        for (s32 liChannel = 0; liChannel < 3; ++liChannel)
        {
            lauPalette[0][1 + liChannel] = static_cast<u8>(lauChannel0[liChannel]);
            lauPalette[1][1 + liChannel] = static_cast<u8>(lauChannel1[liChannel]);
        }

        if (luColour0 > luColour1)
        {
            lauPalette[3][0] = 0xFF;
            for (s32 liChannel = 0; liChannel < 3; ++liChannel)
            {
                const s32 liC0 = static_cast<s32>(lauChannel0[liChannel]);
                const s32 liC1 = static_cast<s32>(lauChannel1[liChannel]);
                lauPalette[2][1 + liChannel] = static_cast<u8>((5 * liC0 + 3 * liC1) >> 3);
                lauPalette[3][1 + liChannel] = static_cast<u8>((3 * liC0 + 5 * liC1) >> 3);
            }
        }
        else
        {
            lauPalette[3][0] = 0;
            for (s32 liChannel = 0; liChannel < 3; ++liChannel)
            {
                const s32 liC0 = static_cast<s32>(lauChannel0[liChannel]);
                const s32 liC1 = static_cast<s32>(lauChannel1[liChannel]);
                lauPalette[2][1 + liChannel] = static_cast<u8>((liC0 + liC1) >> 1);
                lauPalette[3][1 + liChannel] = 0;
            }
        }

        // One byte of 2-bit indices per row, the first pixel in the low bits.
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const u32 luIndices = lpSrc[4 + liRow];
            u8* lpRow = lpDst + liRow * liPitch;
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                const u8* lpEntry = lauPalette[(luIndices >> (liPixel * 2)) & 3];
                u8* lpPixel = lpRow + ((lbSwapped != 0) ? (3 - liPixel) : liPixel) * 4;
                lpPixel[0] = lpEntry[0];
                lpPixel[1] = lpEntry[1];
                lpPixel[2] = lpEntry[2];
                lpPixel[3] = lpEntry[3];
            }
        }
    }
}

s32 dxt_decode(char* lpDecodedPixels, const char* lpCompressedPixels, s32 liWidth, s32 liHeight)
{
    const s32 liPitch     = liWidth << 2;
    const s32 liRowStride = liPitch << 2;

    u8* lpDstRow = reinterpret_cast<u8*>(lpDecodedPixels);
    const u8* lpSrc = reinterpret_cast<const u8*>(lpCompressedPixels);

    if (liHeight > 0)
    {
        for (u32 luRows = ((static_cast<u32>(liHeight) - 1) >> 2) + 1; luRows != 0; --luRows)
        {
            u8* lpDst = lpDstRow;
            if (liWidth > 0)
            {
                for (u32 luBlocks = ((static_cast<u32>(liWidth) - 1) >> 2) + 1; luBlocks != 0; --luBlocks)
                {
                    DecodeColourBlock(lpDst, liPitch, lpSrc, 0, 0);
                    lpDst += 16;
                    lpSrc += 8;
                }
            }
            lpDstRow += liRowStride;
        }
    }
    return 0;
}
