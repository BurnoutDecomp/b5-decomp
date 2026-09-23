// fdxt - the fast DXT block compressor the network texture job runs, and its helpers.
//
// A block is read as sixteen 32-bit A,R,G,B pixels. Formats other than 32-bit ARGB are first
// expanded into a 64-byte scratch block. The colour endpoints start as the block's bounding-box
// corners; above quality 4 FindColourEndpoints searches around them, scoring each candidate
// pair with the index error of a full encode and caching the pairs it has already tried. The
// search effort is set by liQuality (5, 10, 20, 25, 30, 50, 60, 70 and 100 are the steps).
//
// [PC platform layer] byte order. The codec reads and writes pixels and blocks as big-endian
// words and half-words. Every such access goes through LoadU32/StoreU32/LoadU16 below, so the
// bytes it produces are the bytes the console produces from the same input bytes. The
// arithmetic between the accesses is the console's, in 32-bit unsigned registers.

#include "GameShared/Jobs/DXTCompress/DXTCompressAlgorithmFast.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "pc/gcm/renderengine/pixelformat.h"         // renderengine::PIXELFORMAT_*

#include <cmath>   // std::fmaf (the fused single-precision multiply-adds of ConvertYUVPixel)

namespace
{
    // ---- [PC platform layer] big-endian memory access ----
    inline u32 LoadU32(const u8* lpBytes)
    {
        return (static_cast<u32>(lpBytes[0]) << 24) | (static_cast<u32>(lpBytes[1]) << 16) |
               (static_cast<u32>(lpBytes[2]) << 8) | static_cast<u32>(lpBytes[3]);
    }

    inline void StoreU32(u8* lpBytes, u32 luValue)
    {
        lpBytes[0] = static_cast<u8>(luValue >> 24);
        lpBytes[1] = static_cast<u8>(luValue >> 16);
        lpBytes[2] = static_cast<u8>(luValue >> 8);
        lpBytes[3] = static_cast<u8>(luValue);
    }

    inline u32 LoadU16(const u8* lpBytes)
    {
        return (static_cast<u32>(lpBytes[0]) << 8) | static_cast<u32>(lpBytes[1]);
    }

    // Source formats fdxt accepts.
    const s32 KI_FORMAT_A1R5G5B5 = static_cast<s32>(renderengine::PIXELFORMAT_A1R5G5B5);
    const s32 KI_FORMAT_YUY2     = static_cast<s32>(renderengine::PIXELFORMAT_G8B8);   // the camera's packed 4:2:2 plane
    const s32 KI_FORMAT_ARGB32   = static_cast<s32>(renderengine::PIXELFORMAT_X8R8G8B8);

    const u32 KU_FLAG_EXPLICIT_ALPHA     = 0x100;
    const u32 KU_FLAG_INTERPOLATED_ALPHA = 0x200;
    const u32 KU_FLAG_ALPHA_MASK         = 0x300;
    const u32 KU_FLAG_NO_COLOUR_BLOCK    = 0x10;

    const u32 KU_WORST_ERROR = 0x7FFFFFFF;

    // Interpolated-alpha code for each of the eight evenly spaced steps between the block's
    // minimum and maximum alpha (step 0 is the minimum, which is code 1).
    const u32 KAU_ALPHA_STEP_CODES[8] = { 1, 7, 6, 5, 4, 3, 2, 0 };

    // BT.601 studio-swing YUV to RGB.
    const f32 KF_YUV_LUMA_OFFSET   = 16.0f;
    const f32 KF_YUV_CHROMA_OFFSET = 128.0f;
    const f32 KF_YUV_LUMA_SCALE    = 1.16438305f;
    const f32 KF_YUV_V_TO_R        = 1.59602702f;
    const f32 KF_YUV_U_TO_G        = 0.391761988f;
    const f32 KF_YUV_V_TO_G        = 0.812968016f;
    const f32 KF_YUV_U_TO_B        = 2.01723194f;
    const f32 KF_ROUND_HALF        = 0.5f;

    // -------------------------------------------------------------------------------------
    // The four-entry palette of a 5:6:5 endpoint pair, as 0xAARRGGBB words. Four colours
    // (two interpolated at thirds) when colour 0 is the greater, else three colours and
    // transparent black. The same expansion is inlined in every endpoint scorer and in the
    // block encoder.
    // -------------------------------------------------------------------------------------
    inline void BuildColourPalette(u32 luColour0, u32 luColour1, u32* lpuPalette)
    {
        const u32 luR0 = (((luColour0 >> 11) & 0x1F) * 255 + 16) / 31;
        const u32 luG0 = (((luColour0 >> 5) & 0x3F) * 255 + 32) / 63;
        const u32 luB0 = ((luColour0 & 0x1F) * 255 + 16) / 31;
        const u32 luR1 = (((luColour1 >> 11) & 0x1F) * 255 + 16) / 31;
        const u32 luG1 = (((luColour1 >> 5) & 0x3F) * 255 + 32) / 63;
        const u32 luB1 = ((luColour1 & 0x1F) * 255 + 16) / 31;

        const u32 luEntry0 = ((0xFFFF0000u | ((luR0 & 0xFF) << 8) | luG0) << 8) | luB0;
        const u32 luEntry1 = ((0xFFFF0000u | ((luR1 & 0xFF) << 8) | luG1) << 8) | luB1;
        lpuPalette[0] = luEntry0;
        lpuPalette[1] = luEntry1;

        const u32 luER0 = (luEntry0 >> 16) & 0xFF;
        const u32 luEG0 = (luEntry0 >> 8) & 0xFF;
        const u32 luEB0 = luEntry0 & 0xFF;
        const u32 luER1 = (luEntry1 >> 16) & 0xFF;
        const u32 luEG1 = (luEntry1 >> 8) & 0xFF;
        const u32 luEB1 = luEntry1 & 0xFF;

        if (luColour0 > luColour1)
        {
            lpuPalette[2] = ((0xFFFF0000u | ((((2 * luER0 + luER1) / 3) & 0xFF) << 8) |
                              ((2 * luEG0 + luEG1) / 3)) << 8) | ((2 * luEB0 + luEB1) / 3);
            lpuPalette[3] = ((0xFFFF0000u | ((((2 * luER1 + luER0) / 3) & 0xFF) << 8) |
                              ((2 * luEG1 + luEG0) / 3)) << 8) | ((2 * luEB1 + luEB0) / 3);
        }
        else
        {
            const u32 luRG = (((luER0 + luER1) << 15) & 0xFFFF0000u) | ((luEG0 + luEG1) << 7);
            lpuPalette[2] = (luRG & 0xFFFFFF00u) | ((luEB0 + luEB1) >> 1) | 0xFF000000u;
            lpuPalette[3] = 0;
        }
    }

    // 0xAARRGGBB to 5:6:5 by truncation.
    inline u32 ToColour565(u32 luArgb)
    {
        const u32 luStep = (luArgb & ~0x001F0000u) | (((luArgb >> 3) | (luArgb << 29)) & 0x001F0000u);
        const u32 luPacked = (luArgb & ~0x0007FF00u) | (((luStep >> 2) | (luStep << 30)) & 0x0007FF00u);
        return (luPacked >> 3) & 0xFFFF;
    }

    // Squared RGB distance to each palette entry; the nearest index goes to *lpiIndex and its
    // distance is returned (ties keep the lower index).
    u32 FindClosestColourRGB(u32 luColour, s32* lpiIndex, const u32* lpuPalette)
    {
        const s32 liR = static_cast<s32>((luColour >> 16) & 0xFF);
        const s32 liG = static_cast<s32>((luColour >> 8) & 0xFF);
        const s32 liB = static_cast<s32>(luColour & 0xFF);

        u32 luBest  = KU_WORST_ERROR;
        s32 liIndex = 0;
        for (s32 liEntry = 0; liEntry < 4; ++liEntry)
        {
            const u32 luEntry = lpuPalette[liEntry];
            const s32 liDR = liR - static_cast<s32>((luEntry >> 16) & 0xFF);
            const s32 liDG = liG - static_cast<s32>((luEntry >> 8) & 0xFF);
            const s32 liDB = liB - static_cast<s32>(luEntry & 0xFF);
            const u32 luDistance = static_cast<u32>(liDG * liDG) + static_cast<u32>(liDB * liDB) +
                                   static_cast<u32>(liDR * liDR);
            if (luDistance < luBest)
            {
                luBest  = luDistance;
                liIndex = liEntry;
            }
        }
        *lpiIndex = liIndex;
        return luBest;
    }

    // As FindClosestColourRGB, with the alpha channel in the distance.
    u32 FindClosestColourARGB(u32 luColour, s32* lpiIndex, const u32* lpuPalette)
    {
        const s32 liA = static_cast<s32>(luColour >> 24);
        const s32 liR = static_cast<s32>((luColour >> 16) & 0xFF);
        const s32 liG = static_cast<s32>((luColour >> 8) & 0xFF);
        const s32 liB = static_cast<s32>(luColour & 0xFF);

        u32 luBest  = KU_WORST_ERROR;
        s32 liIndex = 0;
        for (s32 liEntry = 0; liEntry < 4; ++liEntry)
        {
            const u32 luEntry = lpuPalette[liEntry];
            const s32 liDA = liA - static_cast<s32>(luEntry >> 24);
            const s32 liDR = liR - static_cast<s32>((luEntry >> 16) & 0xFF);
            const s32 liDG = liG - static_cast<s32>((luEntry >> 8) & 0xFF);
            const s32 liDB = liB - static_cast<s32>(luEntry & 0xFF);
            const u32 luDistance = static_cast<u32>(liDG * liDG) + static_cast<u32>(liDB * liDB) +
                                   static_cast<u32>(liDR * liDR) + static_cast<u32>(liDA * liDA);
            if (luDistance < luBest)
            {
                luBest  = luDistance;
                liIndex = liEntry;
            }
        }
        *lpiIndex = liIndex;
        return luBest;
    }

    // The squared RGB distance to the nearest palette entry.
    u32 GetClosestColourErrorRGB(u32 luColour, const u32* lpuPalette)
    {
        const s32 liR = static_cast<s32>((luColour >> 16) & 0xFF);
        const s32 liG = static_cast<s32>((luColour >> 8) & 0xFF);
        const s32 liB = static_cast<s32>(luColour & 0xFF);

        u32 luBest = 0;
        for (s32 liEntry = 0; liEntry < 4; ++liEntry)
        {
            const u32 luEntry = lpuPalette[liEntry];
            const s32 liDR = liR - static_cast<s32>((luEntry >> 16) & 0xFF);
            const s32 liDG = liG - static_cast<s32>((luEntry >> 8) & 0xFF);
            const s32 liDB = liB - static_cast<s32>(luEntry & 0xFF);
            const u32 luDistance = static_cast<u32>(liDG * liDG) + static_cast<u32>(liDB * liDB) +
                                   static_cast<u32>(liDR * liDR);
            if (liEntry == 0 || luDistance < luBest)
            {
                luBest = luDistance;
            }
        }
        return luBest;
    }

    // Explicit alpha: each pixel's alpha rounded to four bits, four pixels to a little-endian
    // half-word per row.
    void CompressExplicitAlphaBlock(const u8* lpBlock, s32 liPitch, u8* lpDst)
    {
        const u8* lpRow = lpBlock;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            u32 lauAlpha4[4];
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                const u32 luAlpha = lpRow[liPixel * 4];
                lauAlpha4[liPixel] = ((luAlpha << 4) - luAlpha + 0x80) / 255;
            }
            const u32 luHigh = (((lauAlpha4[3] << 4) | lauAlpha4[2]) << 4) | lauAlpha4[1];
            const u32 luWord = ((luHigh & 0xFFF) << 4) | (lauAlpha4[0] & 0xFFFF);
            lpDst[liRow * 2]     = static_cast<u8>(luWord);
            lpDst[liRow * 2 + 1] = static_cast<u8>(luWord >> 8);
            lpRow += liPitch;
        }
    }

    // Interpolated alpha: the block's maximum and minimum alpha, then a 3-bit code per pixel
    // (all zero when the block's alpha is flat).
    void CompressInterpolatedAlphaBlock(const u8* lpBlock, s32 liPitch, u8* lpDst)
    {
        u32 luMin = 0xFF;
        u32 luMax = 0;
        const u8* lpRow = lpBlock;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            for (s32 liOffset = 0; liOffset < 16; liOffset += 4)
            {
                const u32 luAlpha = lpRow[liOffset];
                if (!(luMin < luAlpha))
                {
                    luMin = luAlpha;
                }
                if (!(luMax > luAlpha))
                {
                    luMax = luAlpha;
                }
            }
            lpRow += liPitch;
        }

        u32 lauRowCodes[4];
        lpRow = lpBlock;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            u32 lauCode[4];
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                if (luMax == luMin)
                {
                    lauCode[liPixel] = 0;
                }
                else
                {
                    const u32 luRange = luMax - luMin;
                    const u32 luDelta = lpRow[liPixel * 4] - luMin;
                    const u32 luStep  = ((luDelta << 3) - luDelta + ((luRange + 1) >> 1)) / luRange;
                    lauCode[liPixel] = KAU_ALPHA_STEP_CODES[luStep];
                }
            }
            lauRowCodes[liRow] = (((((lauCode[3] << 3) | lauCode[2]) << 3) | lauCode[1]) << 3) | lauCode[0];
            lpRow += liPitch;
        }

        lpDst[0] = static_cast<u8>(luMax);
        lpDst[1] = static_cast<u8>(luMin);

        const u32 luBits0 = ((lauRowCodes[1] & 0xF) << 12) | lauRowCodes[0];
        const u32 luBits1 = ((lauRowCodes[2] & 0xFF) << 8) | (lauRowCodes[1] >> 4);
        const u32 luBits2 = (lauRowCodes[3] << 4) | (lauRowCodes[2] >> 8);
        lpDst[2] = static_cast<u8>(luBits0);
        lpDst[3] = static_cast<u8>(luBits0 >> 8);
        lpDst[4] = static_cast<u8>(luBits1);
        lpDst[5] = static_cast<u8>(luBits1 >> 8);
        lpDst[6] = static_cast<u8>(luBits2);
        lpDst[7] = static_cast<u8>(luBits2 >> 8);
    }

    // One YUV pixel to opaque A,R,G,B bytes. The multiply-adds are single-precision fused
    // operations; the channels are truncated after adding one half, then clamped to 0..255.
    void ConvertYUVPixel(u8* lpDst, f32 lfY, f32 lfU, f32 lfV)
    {
        const f32 lfLuma = lfY - KF_YUV_LUMA_OFFSET;
        const f32 lfV0   = lfV - KF_YUV_CHROMA_OFFSET;
        const f32 lfU0   = lfU - KF_YUV_CHROMA_OFFSET;

        const f32 lfScaledLuma = lfLuma * KF_YUV_LUMA_SCALE;
        const f32 lfRed        = std::fmaf(lfV0, KF_YUV_V_TO_R, lfScaledLuma);
        const f32 lfGreenU     = -std::fmaf(lfU0, KF_YUV_U_TO_G, -lfScaledLuma);
        const f32 lfBlue       = std::fmaf(lfU0, KF_YUV_U_TO_B, lfScaledLuma);
        const f32 lfGreen      = -std::fmaf(lfV0, KF_YUV_V_TO_G, -lfGreenU);

        lpDst[0] = 0xFF;

        const f32 lafChannel[3] = { lfRed + KF_ROUND_HALF, lfGreen + KF_ROUND_HALF, lfBlue + KF_ROUND_HALF };
        for (s32 liChannel = 0; liChannel < 3; ++liChannel)
        {
            s32 liValue = static_cast<s32>(lafChannel[liChannel]);
            if (liValue < 0)
            {
                liValue = 0;
            }
            else if (liValue > 0xFF)
            {
                liValue = 0xFF;
            }
            lpDst[1 + liChannel] = static_cast<u8>(liValue);
        }
    }

    // A packed 4:2:2 block (Y0 U Y1 V per pixel pair) to sixteen ARGB pixels.
    void CreateARGBBlockFromYUY2(u8* lpDst, const u8* lpSrc, s32 liPitch)
    {
        const u8* lpRow = lpSrc;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const u8* lpPair = lpRow;
            for (s32 liPair = 0; liPair < 2; ++liPair)
            {
                ConvertYUVPixel(lpDst, static_cast<f32>(lpPair[0]), static_cast<f32>(lpPair[1]),
                                static_cast<f32>(lpPair[3]));
                ConvertYUVPixel(lpDst + 4, static_cast<f32>(lpPair[2]), static_cast<f32>(lpPair[1]),
                                static_cast<f32>(lpPair[3]));
                lpDst  += 8;
                lpPair += 4;
            }
            lpRow += liPitch;
        }
    }

    // A block of 32-bit (-, Y, U, V) pixels to sixteen ARGB pixels.
    void CreateARGBBlockFromUncompressedYUYV(u8* lpDst, const u8* lpSrc, s32 liPitch)
    {
        const u8* lpRow = lpSrc;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const u8* lpPixel = lpRow;
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                ConvertYUVPixel(lpDst, static_cast<f32>(lpPixel[1]), static_cast<f32>(lpPixel[2]),
                                static_cast<f32>(lpPixel[3]));
                lpDst   += 4;
                lpPixel += 4;
            }
            lpRow += liPitch;
        }
    }

    // A block of 16-bit 1:5:5:5 pixels to sixteen ARGB pixels (alpha always 0xFF, each 5-bit
    // channel widened by repeating its top three bits). liPitch counts pixels.
    void CreateARGBBlockFromA1R5G5B5(u8* lpDst, const u8* lpSrc, s32 liPitch)
    {
        const u8* lpRow = lpSrc;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                const u32 luPixel = static_cast<u32>(static_cast<s32>(static_cast<s16>(LoadU16(lpRow + liPixel * 2))));

                u32 luValue = (luPixel & 0x7C00) - 0x8000;
                luValue = (luValue << 3) + (luPixel & 0x3E0);
                luValue = (luValue << 2) + (luPixel & 0x7000);
                luValue = (luValue << 1) + (luPixel & 0x1F);
                luValue = (luValue << 2) + (luPixel & 0x380);
                luValue = (luValue << 1) + (static_cast<u32>(static_cast<s32>(luPixel) >> 2) & 7);

                StoreU32(lpDst + liPixel * 4, luValue);
            }
            lpDst += 16;
            lpRow += liPitch * 2;
        }
    }

    // Score an endpoint pair against the sixteen packed pixels: the summed squared RGB error of
    // every pixel's nearest palette entry, abandoned once it passes luLimit. A pair already in
    // the cache scores KU_WORST_ERROR; a new pair is added while the cache has room.
    u32 EvaluateEndpoints(u32* lpuCache, const u8* lpPixels, u32 luColour0, u32 luColour1, u32 luLimit)
    {
        const u32 luKey = (luColour0 << 16) | luColour1;
        const s32 liCount = static_cast<s32>(lpuCache[0]);
        if (liCount > 0)
        {
            for (s32 liEntry = 0; liEntry < static_cast<s32>(lpuCache[0]); ++liEntry)
            {
                if (lpuCache[1 + liEntry] == luKey)
                {
                    return KU_WORST_ERROR;
                }
            }
        }
        if (liCount < 256)
        {
            lpuCache[liCount + 1] = luKey;
            lpuCache[0] = lpuCache[0] + 1;
        }

        u32 lauPalette[4];
        BuildColourPalette(luColour0, luColour1, lauPalette);

        u32 luError = 0;
        const u8* lpPixel = lpPixels;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            for (s32 liColumn = 0; liColumn < 4; ++liColumn)
            {
                luError += GetClosestColourErrorRGB(LoadU32(lpPixel), lauPalette);
                if (luError > luLimit)
                {
                    return luError;
                }
                lpPixel += 4;
            }
        }
        return luError;
    }

    // The red-only score of a pair of 5-bit red levels: each level is passed in the blue field
    // of a 5:6:5 colour, so the palette's blue channel carries the expanded red, and it is
    // measured against the pixels' red byte.
    u32 EvaluateRedEndpoints(const u8* lpPixels, u32 luColour0, u32 luColour1)
    {
        u32 lauPalette[4];
        BuildColourPalette(luColour0, luColour1, lauPalette);

        const s32 laiLevel[4] = { static_cast<s32>(lauPalette[0] & 0xFF), static_cast<s32>(lauPalette[1] & 0xFF),
                                  static_cast<s32>(lauPalette[2] & 0xFF), static_cast<s32>(lauPalette[3] & 0xFF) };

        u32 luError = 0;
        for (s32 liPixel = 0; liPixel < 16; ++liPixel)
        {
            const s32 liRed = lpPixels[liPixel * 4 + 1];
            u32 luBest = KU_WORST_ERROR;
            for (s32 liEntry = 0; liEntry < 4; ++liEntry)
            {
                const s32 liDelta = liRed - laiLevel[liEntry];
                const u32 luDistance = static_cast<u32>(liDelta * liDelta);
                if (luDistance < luBest)
                {
                    luBest = luDistance;
                }
            }
            luError += luBest;
        }
        return luError;
    }

    // The red+blue score of a pair of (red << 11 | blue) colours.
    u32 EvaluateRedBlueEndpoints(const u8* lpPixels, u32 luColour0, u32 luColour1)
    {
        u32 lauPalette[4];
        BuildColourPalette(luColour0, luColour1, lauPalette);

        s32 laiRed[4];
        s32 laiBlue[4];
        for (s32 liEntry = 0; liEntry < 4; ++liEntry)
        {
            laiRed[liEntry]  = static_cast<s32>((lauPalette[liEntry] >> 16) & 0xFF);
            laiBlue[liEntry] = static_cast<s32>(lauPalette[liEntry] & 0xFF);
        }

        u32 luError = 0;
        for (s32 liPixel = 0; liPixel < 16; ++liPixel)
        {
            const u32 luPixel = LoadU32(lpPixels + liPixel * 4);
            const s32 liBlue = static_cast<s32>(luPixel & 0xFF);
            const s32 liRed  = static_cast<s32>((luPixel >> 16) & 0xFF);
            u32 luBest = KU_WORST_ERROR;
            for (s32 liEntry = 0; liEntry < 4; ++liEntry)
            {
                const s32 liDB = liBlue - laiBlue[liEntry];
                const s32 liDR = liRed - laiRed[liEntry];
                const u32 luDistance = static_cast<u32>(liDR * liDR) + static_cast<u32>(liDB * liDB);
                if (luDistance < luBest)
                {
                    luBest = luDistance;
                }
            }
            luError += luBest;
        }
        return luError;
    }

    // Write the colour block: both endpoints as little-endian half-words, then a byte of 2-bit
    // palette indices per row. Returns the summed error. With no alpha block (luAlphaMode 0)
    // the alpha channel takes part in the index choice, so transparent pixels pick the
    // transparent-black entry of a three-colour block.
    u32 EncodeColourBlock(const u8* lpBlock, s32 liPitch, u8* lpDst, u32 luColour0, u32 luColour1,
                          u32 luAlphaMode)
    {
        u32 lauPalette[4];
        BuildColourPalette(luColour0, luColour1, lauPalette);

        lpDst[0] = static_cast<u8>(luColour0);
        lpDst[1] = static_cast<u8>(luColour0 >> 8);
        lpDst[2] = static_cast<u8>(luColour1);
        lpDst[3] = static_cast<u8>(luColour1 >> 8);

        u32 luError = 0;
        const u8* lpPixel = lpBlock;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            u32 luIndices = 0;
            for (s32 liShift = 0; liShift < 8; liShift += 2)
            {
                s32 liIndex = 0;
                if (luAlphaMode == 0)
                {
                    luError += FindClosestColourARGB(LoadU32(lpPixel), &liIndex, lauPalette);
                }
                else
                {
                    luError += FindClosestColourRGB(LoadU32(lpPixel), &liIndex, lauPalette);
                }
                luIndices |= static_cast<u32>(liIndex) << liShift;
                lpPixel += 4;
            }
            lpDst[4 + liRow] = static_cast<u8>(luIndices);
            lpPixel += liPitch - 16;
        }
        return luError;
    }

    // Choose the colour endpoints for one block (see the TU banner). Returns the winning pair's
    // score; *lpuColour0 / *lpuColour1 receive the 5:6:5 endpoints.
    u32 FindColourEndpoints(const u8* lpBlock, s32 liPitch, u32* lpuColour0, u32* lpuColour1,
                            u32 luFlags, s32 liQuality)
    {
        // Channel bounds, indexed A, R, G, B.
        u8 lauMin[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        u8 lauMax[4] = { 0, 0, 0, 0 };

        // The block repacked as sixteen consecutive pixels.
        u8 lauPixels[16 * 4];

        // Endpoint pairs already scored: [0] = count, then up to 256 keys.
        u32 lauCache[1 + 256];

        const u8* lpRow = lpBlock;
        u8* lpPacked = lauPixels;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            for (s32 liPixel = 0; liPixel < 4; ++liPixel)
            {
                for (s32 liChannel = 0; liChannel < 4; ++liChannel)
                {
                    const u8 lbValue = lpRow[liPixel * 4 + liChannel];
                    lpPacked[liChannel] = lbValue;
                    if (!(lauMin[liChannel] < lbValue))
                    {
                        lauMin[liChannel] = lbValue;
                    }
                    if (!(lauMax[liChannel] > lbValue))
                    {
                        lauMax[liChannel] = lbValue;
                    }
                }
                lpPacked += 4;
            }
            lpRow += liPitch;
        }

        lauCache[0] = 0;

        // A candidate within this error ends the search.
        u32 luGoodEnough = 4;
        if (liQuality >= 40)
        {
            luGoodEnough = 3;
        }
        if (liQuality >= 60)
        {
            luGoodEnough = 1;
        }
        if (liQuality >= 70)
        {
            luGoodEnough = 0;
        }

        const u32 luMaxColour = ToColour565(LoadU32(lauMax));
        const u32 luMinColour = ToColour565(LoadU32(lauMin));

        u32 luBest    = KU_WORST_ERROR;
        u32 luColour0 = luMaxColour;
        u32 luColour1 = luMinColour;

        // Stage 1: the bounding box, its reverse (quality 5), or every ordered pair of the
        // block's own colours (quality 10 and up).
        if (liQuality >= 5)
        {
            luBest = EvaluateEndpoints(lauCache, lauPixels, luColour0, luColour1, KU_WORST_ERROR);
            if (luBest != 0)
            {
                if (liQuality == 5)
                {
                    const u32 luError = EvaluateEndpoints(lauCache, lauPixels, luMinColour, luMaxColour, luBest);
                    if (luError < luBest)
                    {
                        luBest    = luError;
                        luColour0 = luMinColour;
                        luColour1 = luMaxColour;
                        if (luError <= luGoodEnough)
                        {
                            goto Done;
                        }
                    }
                }
                else if (liQuality >= 10)
                {
                    for (s32 liFirst = 0; liFirst < 16; ++liFirst)
                    {
                        const u32 luFirst = ToColour565(LoadU32(lauPixels + liFirst * 4));
                        for (s32 liSecond = 0; liSecond < 16; ++liSecond)
                        {
                            const u32 luSecond = ToColour565(LoadU32(lauPixels + liSecond * 4));
                            if (luFirst > luSecond)
                            {
                                const u32 luError = EvaluateEndpoints(lauCache, lauPixels, luFirst, luSecond, luBest);
                                if (luError < luBest)
                                {
                                    luBest    = luError;
                                    luColour0 = luFirst;
                                    luColour1 = luSecond;
                                    if (luError <= luGoodEnough)
                                    {
                                        goto Done;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Stage 2 (quality above 10): search 5:6:5 boxes around the endpoints. The loop runs
        // one pass.
        for (s32 liPass = 0; liPass < 1; ++liPass)
        {
            if (liQuality <= 10)
            {
                continue;
            }

            // The bounding box stretched to double its extent about each corner, clamped to
            // 0..255, then widened by one level down and two up in 5:6:5.
            u8 lauLow[3];
            u8 lauHigh[3];
            for (s32 liChannel = 0; liChannel < 3; ++liChannel)
            {
                const s32 liMin = lauMin[1 + liChannel];
                const s32 liMax = lauMax[1 + liChannel];
                s32 liLow = 2 * liMin - liMax;
                if (liLow < 0)
                {
                    liLow = 0;
                }
                s32 liHigh = 2 * liMax - liMin;
                if (liHigh > 0xFF)
                {
                    liHigh = 0xFF;
                }
                lauLow[liChannel]  = static_cast<u8>(liLow);
                lauHigh[liChannel] = static_cast<u8>(liHigh);
            }

            const s32 laiShift[3] = { 3, 2, 3 };
            const s32 laiLimit[3] = { 0x1F, 0x3F, 0x1F };
            const u32 laiFieldMask[3] = { 0x1F, 0x3F, 0x1F };
            u8 lauLevelLow[3];
            u8 lauLevelHigh[3];
            for (s32 liChannel = 0; liChannel < 3; ++liChannel)
            {
                s32 liLow = static_cast<s32>((lauLow[liChannel] >> laiShift[liChannel]) & laiFieldMask[liChannel]) - 1;
                if (liLow < 0)
                {
                    liLow = 0;
                }
                s32 liHigh = static_cast<s32>((lauHigh[liChannel] >> laiShift[liChannel]) & laiFieldMask[liChannel]) + 2;
                if (liHigh > laiLimit[liChannel])
                {
                    liHigh = laiLimit[liChannel];
                }
                lauLevelLow[liChannel]  = static_cast<u8>(liLow);
                lauLevelHigh[liChannel] = static_cast<u8>(liHigh);
            }

            // Per-endpoint search ranges, indexed R, G, B. At quality 100 both endpoints range
            // over the whole widened box; below it each range is the box clipped to
            // [endpoint - quality/50, endpoint + quality/40 + 1].
            u8 lauC0Low[3];
            u8 lauC0High[3];
            u8 lauC1Low[3];
            u8 lauC1High[3];
            if (liQuality >= 100)
            {
                for (s32 liChannel = 0; liChannel < 3; ++liChannel)
                {
                    lauC0Low[liChannel]  = lauLevelLow[liChannel];
                    lauC1Low[liChannel]  = lauLevelLow[liChannel];
                    lauC0High[liChannel] = lauLevelHigh[liChannel];
                    lauC1High[liChannel] = lauLevelHigh[liChannel];
                }
            }
            else
            {
                const u32 luShrink = static_cast<u32>(liQuality / 50);
                const u32 luGrow   = static_cast<u32>(liQuality / 40 + 1);
                const u32 laiFieldShift[3] = { 11, 5, 0 };

                const u32 lauEndpoint[2] = { luColour0, luColour1 };
                u8* const lapLow[2]  = { lauC0Low, lauC1Low };
                u8* const lapHigh[2] = { lauC0High, lauC1High };
                for (s32 liEnd = 0; liEnd < 2; ++liEnd)
                {
                    for (s32 liChannel = 0; liChannel < 3; ++liChannel)
                    {
                        const u32 luField = (lauEndpoint[liEnd] >> laiFieldShift[liChannel]) & laiFieldMask[liChannel];

                        u32 luLow = luField - luShrink;
                        if (lauLevelLow[liChannel] > luLow)
                        {
                            luLow = lauLevelLow[liChannel];
                        }
                        lapLow[liEnd][liChannel] = static_cast<u8>(luLow);

                        const u32 luGrown = luField + luGrow;
                        u32 luHigh = (lauLevelHigh[liChannel] < luGrown) ? lauLevelHigh[liChannel] : luGrown;
                        if (luHigh > static_cast<u32>(laiLimit[liChannel]))
                        {
                            luHigh = static_cast<u32>(laiLimit[liChannel]);
                        }
                        lapHigh[liEnd][liChannel] = static_cast<u8>(luHigh);
                    }
                }
            }

            // Colour 0 over its box against the current colour 1 (quality 20 and up).
            if (liQuality >= 20)
            {
                for (u32 luR = lauC0Low[0]; luR <= lauC0High[0]; ++luR)
                {
                    for (u32 luB = lauC0Low[2]; luB <= lauC0High[2]; ++luB)
                    {
                        for (u32 luG = lauC0Low[1]; luG <= lauC0High[1]; ++luG)
                        {
                            const u32 luCandidate = (((luR << 6) | luG) << 5) | luB;
                            if (luCandidate > luColour1)
                            {
                                const u32 luError = EvaluateEndpoints(lauCache, lauPixels, luCandidate, luColour1, luBest);
                                if (luError < luBest)
                                {
                                    luBest    = luError;
                                    luColour0 = luCandidate;
                                    if (luError <= luGoodEnough)
                                    {
                                        goto Done;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Colour 1 over its box against the current colour 0 (quality 25 and up).
            if (liQuality >= 25)
            {
                for (u32 luR = lauC1Low[0]; luR <= lauC1High[0]; ++luR)
                {
                    for (u32 luB = lauC1Low[2]; luB <= lauC1High[2]; ++luB)
                    {
                        for (u32 luG = lauC1Low[1]; luG <= lauC1High[1]; ++luG)
                        {
                            const u32 luCandidate = (((luR << 6) | luG) << 5) | luB;
                            if (luColour0 > luCandidate)
                            {
                                const u32 luError = EvaluateEndpoints(lauCache, lauPixels, luColour0, luCandidate, luBest);
                                if (luError < luBest)
                                {
                                    luBest    = luError;
                                    luColour1 = luCandidate;
                                    if (luError <= luGoodEnough)
                                    {
                                        goto Done;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Both endpoints together (quality 30 and up): red pairs first, then red+blue, then
            // the full colours; a stage is entered only while its partial score beats the best
            // total. Candidates far worse than the best skip one level ahead.
            if (liQuality >= 30)
            {
                for (u32 luR0 = lauC0Low[0]; luR0 <= lauC0High[0]; ++luR0)
                {
                    for (u32 luR1 = lauC1Low[0]; luR1 <= lauC1High[0]; ++luR1)
                    {
                        if (!(luR0 > luR1) && liQuality < 50)
                        {
                            continue;
                        }
                        if (!(EvaluateRedEndpoints(lauPixels, luR0, luR1) < luBest))
                        {
                            continue;
                        }

                        for (u32 luB0 = lauC0Low[2]; luB0 <= lauC0High[2]; ++luB0)
                        {
                            for (u32 luB1 = lauC1Low[2]; luB1 <= lauC1High[2]; ++luB1)
                            {
                                const u32 luRedBlueError =
                                    EvaluateRedBlueEndpoints(lauPixels, (luR0 << 11) | luB0, (luR1 << 11) | luB1);
                                if (!(luRedBlueError < luBest))
                                {
                                    if (luRedBlueError - luBest > 0x1000)
                                    {
                                        ++luB1;
                                    }
                                    continue;
                                }

                                for (u32 luG0 = lauC0Low[1]; luG0 <= lauC0High[1]; ++luG0)
                                {
                                    const u32 luCandidate0 = (((luR0 << 6) | luG0) << 5) | luB0;
                                    for (u32 luG1 = lauC1Low[1]; luG1 <= lauC1High[1]; ++luG1)
                                    {
                                        const u32 luCandidate1 = (((luR1 << 6) | luG1) << 5) | luB1;
                                        if (!(luCandidate0 > luCandidate1) && liQuality < 60)
                                        {
                                            continue;
                                        }

                                        const u32 luError =
                                            EvaluateEndpoints(lauCache, lauPixels, luCandidate0, luCandidate1, luBest);
                                        if (luError < luBest)
                                        {
                                            luBest    = luError;
                                            luColour0 = luCandidate0;
                                            luColour1 = luCandidate1;
                                            if (luError <= luGoodEnough)
                                            {
                                                goto Done;
                                            }
                                        }
                                        else if (luError - luBest > 0x10)
                                        {
                                            ++luG1;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (liQuality == 100)
            {
                break;
            }
        }

    Done:
        // Without an alpha block, a block holding any pixel under half alpha is written in
        // three-colour order so its transparent-black entry is available.
        if (luFlags == 0 && lauMin[0] < 0x80 && luColour0 > luColour1)
        {
            const u32 luSwap = luColour0;
            luColour0 = luColour1;
            luColour1 = luSwap;
        }

        *lpuColour0 = luColour0;
        *lpuColour1 = luColour1;
        return luBest;
    }

    // Byte-swap both 16-bit halves of each of the block's two words.
    inline void SwapBlockHalfWords(u8* lpBlock)
    {
        for (s32 liWord = 0; liWord < 2; ++liWord)
        {
            const u32 luWord = LoadU32(lpBlock + liWord * 4);
            StoreU32(lpBlock + liWord * 4, ((luWord >> 8) & 0xFFFF00FFu) | ((luWord << 8) & 0xFF00FFFFu));
        }
    }
}

void fdxt(u8* lpDst, const u8* lpSrc, s32 liSrcPitch, s32 liNumBlocks, u32 luFlags,
          s32 liQuality, s32 leFormat, bool lbInputIsUncompressedYUYV)
{
    u8 lauBlock[16 * 4];

    // The two endpoint slots. The A1R5G5B5 arm also parks the pixel pitch in the second slot
    // before the loop reads it; the console leaves both uninitialised otherwise, and only the
    // unsupported-format arm reads them before the first block writes them.
    u32 luColour0 = 0;
    u32 luColour1 = 0;

    s32 liBlockPitch = liSrcPitch;   // pitch of the block the compressors read
    s32 liSrcAdvance = 0;            // source bytes per block
    const u8* lpBlock = nullptr;

    if (leFormat == KI_FORMAT_A1R5G5B5)
    {
        luColour1    = static_cast<u32>(liSrcPitch / 2);
        liSrcAdvance = 8;
        lpBlock      = lauBlock;
        liBlockPitch = 16;
    }
    else if (leFormat == KI_FORMAT_YUY2)
    {
        liSrcAdvance = 8;
        lpBlock      = lauBlock;
        liBlockPitch = 16;
    }
    else if (leFormat == KI_FORMAT_ARGB32)
    {
        if (lbInputIsUncompressedYUYV)
        {
            liBlockPitch = 16;
            lpBlock      = lauBlock;
            liSrcAdvance = 16;
        }
        else
        {
            liSrcAdvance = 16;
        }
    }
    else
    {
        CGS_ASSERT(false, " Unsupported image format passed to DXT compression job");
        liSrcAdvance = static_cast<s32>(luColour0);
    }

    if (liNumBlocks <= 0)
    {
        return;
    }

    const s32 liA1R5G5B5Pitch = static_cast<s32>(luColour1);
    const u32 luExplicitAlpha = luFlags & KU_FLAG_EXPLICIT_ALPHA;
    const u32 luColourBlock   = ~luFlags & KU_FLAG_NO_COLOUR_BLOCK;

    for (s32 liBlock = liNumBlocks; liBlock != 0; --liBlock)
    {
        if (leFormat == KI_FORMAT_A1R5G5B5)
        {
            CreateARGBBlockFromA1R5G5B5(lauBlock, lpSrc, liA1R5G5B5Pitch);
        }
        else if (leFormat == KI_FORMAT_YUY2)
        {
            CreateARGBBlockFromYUY2(lauBlock, lpSrc, liSrcPitch);
        }
        else if (lbInputIsUncompressedYUYV)
        {
            CreateARGBBlockFromUncompressedYUYV(lauBlock, lpSrc, liSrcPitch);
        }
        else
        {
            lpBlock = lpSrc;
        }

        if (luExplicitAlpha != 0)
        {
            CompressExplicitAlphaBlock(lpBlock, liBlockPitch, lpDst);
            SwapBlockHalfWords(lpDst);
            lpDst += 8;
        }
        else if ((luFlags & KU_FLAG_INTERPOLATED_ALPHA) != 0)
        {
            CompressInterpolatedAlphaBlock(lpBlock, liBlockPitch, lpDst);
            SwapBlockHalfWords(lpDst);
            lpDst += 8;
        }

        if (luColourBlock != 0)
        {
            FindColourEndpoints(lpBlock, liBlockPitch, &luColour0, &luColour1, luFlags, liQuality);
            EncodeColourBlock(lpBlock, liBlockPitch, lpDst, luColour0, luColour1, luFlags & KU_FLAG_ALPHA_MASK);
            SwapBlockHalfWords(lpDst);
            lpDst += 8;
        }

        lpSrc += liSrcAdvance;
    }
}
