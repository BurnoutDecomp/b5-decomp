// CgsNetwork::NetworkImageConverter - the network mugshot/texture image pipeline.
//
// This TU reconstructs the seven X360-attested methods that live in this source file:
//   SetupPerfmons                       @ 0x82871860  register the eight CPU perfmons
//   Convert                             @ 0x828943A8  same-size convert dispatch
//   Copy                                @ 0x82871B08  same-format blit
//   ConvertX8R8G8B8ToA1R5G5B5           @ 0x8288EC98  same-size 32bpp -> 16bpp pack
//   ConvertAndResize                    @ 0x828944C8  validate + dispatch the resize convert
//   ConvertAndResizeX8R8G8B8ToA1R5G5B5  @ 0x8288EDD8  the bilinear/point resize + pack
//   UnpackFromNetworkTexture            @ 0x8288F498  blit into a locked GPU surface
// (The first three landed with the intro wave: BrnProgression::Profile::SetPlayerLicencePicture
// @0x8235A020 calls Convert, so mounting the Profile TU needed them.)
// Behaviour is taken from the X360 ARTIST asm; the geometry
// getters the X360 inlined (GetWidth/GetHeight/GetFormat/GetTexture/GetBitsPerPixel/GetTextureSize)
// are restored as NetworkTexture method calls.

#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"

#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"   // CgsNetwork::NetworkTexture
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT + the assert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu

#include <cstring>   // std::memset / std::memcpy (the X360 XMemSet / XMemCpy)

namespace CgsNetwork
{
    // ---- static perfmon handle storage ----------------------------------------------------------
    s32 NetworkImageConverter::miConvertPM       = -1;
    s32 NetworkImageConverter::miCopyPM          = -1;
    s32 NetworkImageConverter::miResizePM        = -1;
    s32 NetworkImageConverter::miUnpackToBufferPM = -1;
    s32 NetworkImageConverter::miXToRPM          = -1;
    s32 NetworkImageConverter::miCompressPM      = -1;
    s32 NetworkImageConverter::miUnpackToImage   = -1;
    s32 NetworkImageConverter::miPackToTexture   = -1;

    // ---- SetupPerfmons @ 0x82871860 -------------------------------------------------------------
    // Register the eight CPU perfmons the image pipeline brackets its regions with, then assert each
    // handle came back valid (>= 0). The X360 passes colour 8, minimum 0, budget 1.0 ms, no parent,
    // no flags to every AddMonitor (r6/parent is left at the call-site default, 0).
    void NetworkImageConverter::SetupPerfmons()
    {
        miConvertPM        = CgsDev::PerfMonCpu::AddMonitor("NIC - Convert",  8, 0, 1.0, 0, 0);
        miCopyPM           = CgsDev::PerfMonCpu::AddMonitor("NIC - Copy",     8, 0, 1.0, 0, 0);
        miResizePM         = CgsDev::PerfMonCpu::AddMonitor("NIC - Resize",   8, 0, 1.0, 0, 0);
        miUnpackToBufferPM = CgsDev::PerfMonCpu::AddMonitor("NIC - Unpack",   8, 0, 1.0, 0, 0);
        miXToRPM           = CgsDev::PerfMonCpu::AddMonitor("NIC - X8 to R5", 8, 0, 1.0, 0, 0);
        miCompressPM       = CgsDev::PerfMonCpu::AddMonitor("DXT1 Compress",  8, 0, 1.0, 0, 0);
        miUnpackToImage    = CgsDev::PerfMonCpu::AddMonitor("DXT1 Unpack",    8, 0, 1.0, 0, 0);
        miPackToTexture    = CgsDev::PerfMonCpu::AddMonitor("DXT1 Pack",      8, 0, 1.0, 0, 0);

        CGS_ASSERT(miConvertPM        >= 0, "miConvertPM >= 0");
        CGS_ASSERT(miCopyPM           >= 0, "miCopyPM >= 0");
        CGS_ASSERT(miResizePM         >= 0, "miResizePM >= 0");
        CGS_ASSERT(miUnpackToBufferPM >= 0, "miUnpackToBufferPM >= 0");
        CGS_ASSERT(miXToRPM           >= 0, "miXToRPM >= 0");
        CGS_ASSERT(miCompressPM       >= 0, "miCompressPM >= 0");
        CGS_ASSERT(miUnpackToImage    >= 0, "miUnpackToImage >= 0");
        CGS_ASSERT(miPackToTexture    >= 0, "miPackToTexture >= 0");
    }

    // ---- Convert @ 0x828943A8 -------------------------------------------------------------------
    // Same-size convert: identical formats are a straight Copy; X8/A8 R8G8B8 -> A1R5G5B5 is the one
    // real conversion; anything else is the streamed "Cannot convert these formats!" assert
    // (CgsNetworkImageConverter.cpp:123). The whole body is bracketed by the Convert perfmon.
    // (X360 raw format words: 405274758 == 0x18280086 A8R8G8B8, 673710214 == 0x28280086 X8R8G8B8,
    //  405274691 == 0x18280043 A1R5G5B5.)
    void NetworkImageConverter::Convert(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture)
    {
        CgsDev::PerfMonCpu::StartMonitor(miConvertPM);

        const renderengine::PixelFormat leSrcFormat = lpSrcTexture->GetFormat();
        const renderengine::PixelFormat leDstFormat = lpDstTexture->GetFormat();

        if (leSrcFormat == leDstFormat)
        {
            Copy(lpSrcTexture, lpDstTexture);
        }
        else if ((leSrcFormat == renderengine::PIXELFORMAT_A8R8G8B8
                  && leDstFormat == renderengine::PIXELFORMAT_A1R5G5B5)
              || (leSrcFormat == renderengine::PIXELFORMAT_X8R8G8B8
                  && leDstFormat == renderengine::PIXELFORMAT_A1R5G5B5))
        {
            ConvertX8R8G8B8ToA1R5G5B5(lpSrcTexture, lpDstTexture);
        }
        else
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Cannot convert these formats!";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        CgsDev::PerfMonCpu::StopMonitor(miConvertPM);
    }

    // ---- Copy @ 0x82871B08 ----------------------------------------------------------------------
    // Same-format blit: three geometry/format asserts (cpp:588/589/590), then one XMemCpy of
    // GetTextureSize() SOURCE bytes -- (srcBitsPerPixel * srcHeight * srcWidth + 7) / 8 -- from the
    // source pixel buffer to the destination's. Bracketed by the Copy perfmon.
    void NetworkImageConverter::Copy(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture)
    {
        CgsDev::PerfMonCpu::StartMonitor(miCopyPM);

        CGS_ASSERT(lpDstTexture->GetWidth()  >= lpSrcTexture->GetWidth(),
                   "lpDstTexture->GetWidth() >= lpSrcTexture->GetWidth()");
        CGS_ASSERT(lpDstTexture->GetHeight() >= lpSrcTexture->GetHeight(),
                   "lpDstTexture->GetHeight() >= lpSrcTexture->GetHeight()");
        CGS_ASSERT(lpDstTexture->GetFormat() == lpSrcTexture->GetFormat(),
                   "lpDstTexture->GetFormat() == lpSrcTexture->GetFormat()");

        std::memcpy(lpDstTexture->GetTexture(), lpSrcTexture->GetTexture(),
                    static_cast<usize>(lpSrcTexture->GetTextureSize()));

        CgsDev::PerfMonCpu::StopMonitor(miCopyPM);
    }

    // ---- ConvertX8R8G8B8ToA1R5G5B5 @ 0x8288EC98 -------------------------------------------------
    // Straight (no resize) 32bpp -> 16bpp pack, row by row through each texture's own stride.
    // Same-size only (the two dimension asserts, cpp:195/196). The pack is the same one the resize
    // path uses: dst16 = 0x8000 | (R5 << 10) | (G5 << 5) | B5, taking the top five bits of each
    // source channel (the X360 forms it as 32*(32*((r&0x1F)-32) + (g&0x1F)) + (b&0x1F), whose
    // -32 term IS the forced alpha bit once truncated to 16 bits). Bracketed by the X8->R5 perfmon.
    void NetworkImageConverter::ConvertX8R8G8B8ToA1R5G5B5(const NetworkTexture* lpSrcTexture,
                                                          NetworkTexture* lpDstTexture)
    {
        CgsDev::PerfMonCpu::StartMonitor(miXToRPM);

        const char* lpcSrcRow = lpSrcTexture->GetTexture();
        char*       lpcDstRow = lpDstTexture->GetTexture();
        const s32   liSrcStride = lpSrcTexture->GetStride();
        const s32   liDstStride = lpDstTexture->GetStride();

        CGS_ASSERT(lpSrcTexture->GetWidth()  == lpDstTexture->GetWidth(),
                   "lpSrcTexture->GetWidth() == lpDstTexture->GetWidth()");
        CGS_ASSERT(lpSrcTexture->GetHeight() == lpDstTexture->GetHeight(),
                   "lpSrcTexture->GetHeight() == lpDstTexture->GetHeight()");

        for (s32 liRow = 0; liRow < lpDstTexture->GetHeight(); ++liRow)
        {
            const u32* lpuSrc = reinterpret_cast<const u32*>(lpcSrcRow);
            u16*       lpuDst = reinterpret_cast<u16*>(lpcDstRow);

            for (s32 liColumn = 0; liColumn < lpDstTexture->GetWidth(); ++liColumn)
            {
                const u32 luSource = *lpuSrc++;
                const u32 luRed    = (luSource >> 19) & 0x1Fu;
                const u32 luGreen  = (luSource >> 11) & 0x1Fu;
                const u32 luBlue   = (luSource >>  3) & 0x1Fu;
                *lpuDst++ = static_cast<u16>(0x8000u | (luRed << 10) | (luGreen << 5) | luBlue);
            }

            lpcSrcRow += liSrcStride;
            lpcDstRow += liDstStride;
        }

        CgsDev::PerfMonCpu::StopMonitor(miXToRPM);
    }

    // ---- ConvertAndResize @ 0x828944C8 ----------------------------------------------------------
    // Validate the destination column range, then dispatch the only supported resize-convert
    // (X8R8G8B8 source -> A1R5G5B5 destination). Any other format pair fires a streamed assert.
    void NetworkImageConverter::ConvertAndResize(const NetworkTexture* lpSrcTexture,
                                                 NetworkTexture* lpDstTexture,
                                                 s32 liStartColumn, s32 liEndColumn)
    {
        CGS_ASSERT(liStartColumn >= 0,                          "(liStartColumn >= 0)");
        CGS_ASSERT(liStartColumn < liEndColumn,                 "(liStartColumn < liEndColumn)");
        CGS_ASSERT(liStartColumn < lpDstTexture->GetWidth(),    "(liStartColumn < lpDstTexture->GetWidth() )");
        CGS_ASSERT(liEndColumn > 0,                             "(liEndColumn > 0)");
        CGS_ASSERT(liEndColumn <= lpDstTexture->GetWidth(),     "(liEndColumn <= lpDstTexture->GetWidth() )");

        if (lpSrcTexture->GetFormat() == renderengine::PIXELFORMAT_X8R8G8B8 &&
            lpDstTexture->GetFormat() == renderengine::PIXELFORMAT_A1R5G5B5)
        {
            ConvertAndResizeX8R8G8B8ToA1R5G5B5(lpSrcTexture, lpDstTexture, liStartColumn, liEndColumn);
        }
        else
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Cannot convert and resize these formats!";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
    }

    // ---- ConvertAndResizeX8R8G8B8ToA1R5G5B5 @ 0x8288EDD8 ----------------------------------------
    // Resize an X8R8G8B8 source into an A1R5G5B5 destination over the column range
    // [liStartColumn, liEndColumn). Two paths:
    //   * If the destination is larger than the source in either axis, or more than 2x smaller in
    //     either axis (i.e. a magnify or a steep minify), POINT-sample with float source ratios.
    //   * Otherwise (a gentle minify, <= 2x), BILINEAR-filter with 8.8 fixed-point weights.
    // The destination is first cleared to zero. Every output pixel is packed A1R5G5B5 with the
    // alpha bit forced set: dst16 = 0x8000 | (R5 << 10) | (G5 << 5) | B5.
    //
    // NOTE (reviewer): the X360 Hex-Rays output for this body is flagged "local variable allocation
    // has failed" - the fixed-point inner loops below are reconstructed from the PPC asm
    // (0x8288EDD8..0x8288F494), preserving the 8.8 weight arithmetic and the bounds assert. The
    // float-ratio point path mirrors the asm's fcfid/fdivs/fctiwz sequence.
    void NetworkImageConverter::ConvertAndResizeX8R8G8B8ToA1R5G5B5(const NetworkTexture* lpSrcTexture,
                                                                   NetworkTexture* lpDstTexture,
                                                                   s32 liStartColumn, s32 liEndColumn)
    {
        CgsDev::PerfMonCpu::StartMonitor(miXToRPM);

        const s32 liSrcWidth   = lpSrcTexture->GetWidth();
        const s32 liSrcHeight  = lpSrcTexture->GetHeight();
        const s32 liDstWidth   = liEndColumn - liStartColumn;   // columns of the destination filled
        const s32 liDstHeight  = lpDstTexture->GetHeight();
        char*     lpcDstBase    = lpDstTexture->GetTexture();

        // Clear the destination buffer.
        std::memset(lpcDstBase, 0, lpDstTexture->GetStride() * liDstHeight);

        const bool lbPointSample = (liSrcWidth  > liDstWidth)  ||
                                   (liSrcHeight > liDstHeight) ||
                                   (2 * liSrcWidth  < liDstWidth) ||
                                   (2 * liSrcHeight < liDstHeight);

        if (lbPointSample)
        {
            // ---- POINT-sample path (magnify / steep minify) ------------------------------------
            // Source step per destination pixel, as a float ratio. The asm builds these from the
            // 8-bit-aligned src/dst spans: rows by (srcHeight / dstHeight), columns by the span
            // (dstWidth - (dstFullWidth - liEndColumn)) so the right margin is preserved.
            const s32 liDstFullWidth   = lpDstTexture->GetWidth();
            const s32 liRightMargin    = liDstFullWidth - liEndColumn;   // columns past liEndColumn
            const s32 liColumnSpan      = liDstFullWidth - liRightMargin - liStartColumn;

            const s32 liSrcBytesPerPixel = lpSrcTexture->GetBitsPerPixel() / 8;
            const s32 liDstBytesPerPixel = lpDstTexture->GetBitsPerPixel() / 8;
            CGS_ASSERT(0 == (lpSrcTexture->GetBitsPerPixel() % 8), "0 == (lpSrcTexture->GetBitsPerPixel() % 8)");
            CGS_ASSERT(0 == (lpDstTexture->GetBitsPerPixel() % 8), "0 == (lpDstTexture->GetBitsPerPixel() % 8)");

            const float lfRowRatio    = static_cast<float>(liSrcHeight) / static_cast<float>(liDstHeight);
            const float lfColumnRatio = static_cast<float>(liSrcWidth)  / static_cast<float>(liColumnSpan);

            char*       lpcSrcBase = lpSrcTexture->GetTexture();
            const s32   liSrcStride = lpSrcTexture->GetStride();
            const s32   liDstStride = lpDstTexture->GetStride();

            for (s32 liDstRow = 0; liDstRow < liDstHeight; ++liDstRow)
            {
                const s32 liSrcRow = static_cast<s32>(static_cast<float>(liDstRow) * lfRowRatio);
                const char* lpcSrcRow = lpcSrcBase + liSrcStride * liSrcRow;
                u16* lpDstPixel = reinterpret_cast<u16*>(lpcDstBase + liDstStride * liDstRow
                                                                    + liDstBytesPerPixel * liStartColumn);

                // The destination spans the absolute columns [liStartColumn, liEndColumn); the source
                // column is the point-sample of a 0-based counter from the row's start, scaled by the
                // column ratio (matching the asm: var_EC counts 0,1,2.. and is multiplied by f31).
                s32 liLocalColumn = 0;
                for (s32 liDstColumn = liStartColumn; liDstColumn < liEndColumn; ++liDstColumn)
                {
                    const s32 liSrcColumn = static_cast<s32>(static_cast<float>(liLocalColumn) * lfColumnRatio);
                    const u32 luSrcPixel = *reinterpret_cast<const u32*>(lpcSrcRow + liSrcBytesPerPixel * liSrcColumn);

                    const u32 luR = (luSrcPixel >> 19) & 0x1F;
                    const u32 luG = (luSrcPixel >> 11) & 0x1F;
                    const u32 luB = (luSrcPixel >> 3)  & 0x1F;
                    *lpDstPixel = static_cast<u16>(0x8000u | (luR << 10) | (luG << 5) | luB);
                    ++lpDstPixel;
                    ++liLocalColumn;
                }
            }
        }
        else
        {
            // ---- BILINEAR path (gentle minify, <= 2x) -------------------------------------------
            // 8.8 fixed-point box/bilinear filter. Reconstructed store-for-store from the asm: the
            // per-row/per-column weights are the fractional source coverage of each destination
            // pixel, and the four sampled source texels are blended with the cross weights. Padding
            // to the next row/pixel is via the precomputed source-stride deltas.
            const s32 liSrcRowPitch    = lpSrcTexture->GetStride() / 4 - liSrcWidth;   // src pixels skipped per row
            const s32 liDstRowPitch    = lpDstTexture->GetStride() / 2 - liDstWidth;   // dst pixels skipped per row

            CGS_ASSERT((lpSrcTexture->GetStride() % 4) == 0, "(lpSrcTexture->GetStride() % 4) == 0");
            CGS_ASSERT((lpDstTexture->GetStride() % 4) == 0, "(lpDstTexture->GetStride() % 4) == 0");
            CGS_ASSERT(lpSrcTexture->GetBitsPerPixel() == 32, "lpSrcTexture->GetBitsPerPixel() == 32");
            CGS_ASSERT(lpDstTexture->GetBitsPerPixel() == 16, "lpDstTexture->GetBitsPerPixel() == 16");

            const u32* lpuSource = reinterpret_cast<const u32*>(lpSrcTexture->GetTexture());
            u16*       lpDst     = reinterpret_cast<u16*>(lpDstTexture->GetTexture()) + liStartColumn;

            // End-of-source guard (src pixel index must stay inside the source pixel buffer).
            const u32* lpuSourceEnd = reinterpret_cast<const u32*>(
                lpSrcTexture->GetTexture() + lpSrcTexture->GetTextureSize());
            const char* lpcBoundsMsg =
                "(lpuSource + liPixelOffsetY + liPixelOffsetX) < (uint32_t *) "
                "(lpSrcTexture->GetTexture() + lpSrcTexture->GetTextureSize())";

            if (liDstHeight > 0)
            {
                // Running 8.8 source-row coordinate (v22 = dstHeight<<8 start, advanced by srcHeight<<8).
                s32 liRowCoordCur  = liSrcHeight << 8;
                s32 liRowCoordPrev = 0;
                s32 liNextRowSpan  = liDstHeight - 1;

                for (s32 liDstRow = liDstHeight; liDstRow != 0; --liDstRow)
                {
                    // Vertical coverage: how many whole source rows this destination row spans (1..),
                    // and the fractional vertical weight (8.8) of the leading partial row.
                    const s32 liRowsCovered = ((liRowCoordCur  / liDstHeight) >> 8)
                                            - ((liRowCoordPrev / liDstHeight) >> 8);
                    const s32 liRowFracLo = (liRowCoordCur / liDstHeight) & 0xFF;     // leading partial coverage
                    s32 liRowCover = liRowFracLo * liRowsCovered;
                    if (liRowCover > 1) liRowCover = 1;

                    const s32 liRowWeightTop = (liRowFracLo - 256) * liRowCover + 256; // top-row weight (8.8)
                    const s32 liRowWeightBot = 256 - liRowWeightTop;                   // bottom-row weight

                    if (liDstWidth > 0)
                    {
                        s32 liColCoordCur  = liSrcWidth << 8;
                        s32 liColCoordPrev = 0;
                        s32 liColumnsRemaining = liDstWidth - 1;   // counts down per destination column

                        for (s32 liDstColumn = liDstWidth; liDstColumn != 0; --liDstColumn)
                        {
                            const s32 liColsCovered = ((liColCoordCur  / liDstWidth) >> 8)
                                                    - ((liColCoordPrev / liDstWidth) >> 8);
                            const s32 liColFracLo  = (liColCoordCur / liDstWidth) & 0xFF;
                            s32 liColCover = liColFracLo * liColsCovered;
                            if (liColCover > 1) liColCover = 1;

                            const s32 liColWeightL = (liColFracLo - 256) * liColCover + 256; // left weight (8.8)
                            const s32 liColWeightR = 256 - liColWeightL;                     // right weight

                            // Cross weights of the four sampled texels: each corner is the product
                            // of its column weight (left/right) and row weight (top/bottom).
                            const s32 liWeightTL = liColWeightL * liRowWeightTop;
                            const s32 liWeightTR = liColWeightR * liRowWeightTop;
                            const s32 liWeightBL = liColWeightL * liRowWeightBot;
                            const s32 liWeightBR = liColWeightR * liRowWeightBot;

                            // Source texel offsets: the next column / next row step shrink to 1, but
                            // clamp to the columns/rows still remaining so the bottom-right corner of
                            // the filter never samples past the source edge.
                            s32 liNextColOffset = 1;
                            if (liColumnsRemaining <= 1) liNextColOffset = liColumnsRemaining;
                            s32 liNextRowOffset = 1;
                            if (liNextRowSpan <= 1) liNextRowOffset = liNextRowSpan;

                            const u32* lpuSample = &lpuSource[liNextRowOffset * liSrcWidth + liNextColOffset];
                            CGS_ASSERT(lpuSample < lpuSourceEnd, lpcBoundsMsg);

                            const u32 luTexelTL = lpuSource[0];
                            const u32 luTexelTR = lpuSource[liNextColOffset];
                            const u32 luTexelBL = lpuSource[liNextRowOffset * liSrcWidth];
                            const u32 luTexelBR = *lpuSample;

                            // Per-channel 8.8 blend of the four corners (X8R8G8B8: R = bits 16-23,
                            // G = bits 8-15, B = bits 0-7). Each texel pairs with its own corner weight.
                            const u32 luBlendR =  ((luTexelTL >> 16) & 0xFF) * liWeightTL
                                                + ((luTexelTR >> 16) & 0xFF) * liWeightTR
                                                + ((luTexelBL >> 16) & 0xFF) * liWeightBL
                                                + ((luTexelBR >> 16) & 0xFF) * liWeightBR;
                            const u32 luBlendG =  ((luTexelTL >> 8)  & 0xFF) * liWeightTL
                                                + ((luTexelTR >> 8)  & 0xFF) * liWeightTR
                                                + ((luTexelBL >> 8)  & 0xFF) * liWeightBL
                                                + ((luTexelBR >> 8)  & 0xFF) * liWeightBR;
                            const u32 luBlendB =  (luTexelTL        & 0xFF) * liWeightTL
                                                + (luTexelTR        & 0xFF) * liWeightTR
                                                + (luTexelBL        & 0xFF) * liWeightBL
                                                + (luTexelBR        & 0xFF) * liWeightBR;

                            const u32 luR5 = (luBlendR >> 19) & 0x1F;
                            const u32 luG5 = (luBlendG >> 19) & 0x1F;
                            const u32 luB5 = (luBlendB >> 19) & 0x1F;
                            *lpDst = static_cast<u16>(0x8000u | (luR5 << 10) | (luG5 << 5) | luB5);

                            lpuSource += liColsCovered;
                            ++lpDst;
                            --liColumnsRemaining;

                            liColCoordPrev = liColCoordCur;
                            liColCoordCur += liSrcWidth << 8;
                        }
                    }

                    // Advance to the next destination row: skip the dst row padding, and step the
                    // source pointer by the rows we consumed (or rewind one row if none).
                    lpDst += liDstRowPitch;
                    lpuSource += liRowsCovered * liSrcRowPitch - (1 - liRowsCovered) * liSrcWidth;

                    liRowCoordPrev = liRowCoordCur;
                    liRowCoordCur += liSrcHeight << 8;
                    --liNextRowSpan;
                }
            }
        }

        CgsDev::PerfMonCpu::StopMonitor(miXToRPM);
    }

    // ---- UnpackFromNetworkTexture @ 0x8288F498 --------------------------------------------------
    // Blit lpcSrcTexture's pixels into a locked GPU texture surface. The number of rows is the
    // smaller of the source height (in rows, or DXT blocks for a compressed format) and the locked
    // surface's height; the per-row byte count is the smaller of the source and destination strides.
    // If the strides are equal the whole image copies in one block.
    char* NetworkImageConverter::UnpackFromNetworkTexture(const NetworkTexture* lpcSrcTexture,
                                                          renderengine::Texture::Locked* lpDstLocked)
    {
        const s32 liSrcStride  = lpcSrcTexture->GetStride();
        const s32 liDstStride  = static_cast<s32>(lpDstLocked->muStride);
        const s32 liBytesToCopy = (liDstStride < liSrcStride) ? liDstStride : liSrcStride;

        const char* lpcSrcPointer = lpcSrcTexture->GetTexture();
        char*       lpcDstPointer = static_cast<char*>(lpDstLocked->mpPixelData);

        const renderengine::PixelFormat leFormat = lpcSrcTexture->GetFormat();
        const s32 liLockedHeight = static_cast<s32>(lpDstLocked->muHeight);

        s32 liRowsToCopy;
        if (leFormat == renderengine::PIXELFORMAT_A1R5G5B5 ||
            leFormat == renderengine::PIXELFORMAT_G8B8 ||
            leFormat == renderengine::PIXELFORMAT_A8R8G8B8)
        {
            liRowsToCopy = lpcSrcTexture->GetHeight();
            if (liRowsToCopy >= liLockedHeight)
                liRowsToCopy = liLockedHeight;
        }
        else if (leFormat == renderengine::PIXELFORMAT_DXT1)
        {
            liRowsToCopy = liLockedHeight;
            const s32 liSrcHeightInBlocks = lpcSrcTexture->GetHeightInBlocks();
            if (liSrcHeightInBlocks < liRowsToCopy)
                liRowsToCopy = liSrcHeightInBlocks;
        }
        else
        {
            liRowsToCopy = 0;
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Unexpected texture format: " << static_cast<u32>(leFormat) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        char* lpcResult = lpcDstPointer;
        if (liSrcStride == liDstStride)
        {
            // Same pitch: one contiguous copy of the whole image.
            lpcResult = static_cast<char*>(
                std::memcpy(lpcDstPointer, lpcSrcPointer, static_cast<size_t>(liDstStride) * liRowsToCopy));
        }
        else
        {
            // Differing pitch: copy row by row, advancing each pointer by its own stride.
            for (s32 liRowIndex = 0; liRowIndex < liRowsToCopy; ++liRowIndex)
            {
                lpcResult = static_cast<char*>(
                    std::memcpy(lpcDstPointer, lpcSrcPointer, static_cast<size_t>(liBytesToCopy)));
                lpcSrcPointer += liSrcStride;
                lpcDstPointer += liDstStride;
            }
        }

        return lpcResult;
    }
}
