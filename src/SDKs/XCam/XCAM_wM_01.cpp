#include "SDKs/XCam/XCamYUY2VideoOutput.h"

// Reuse the sibling I420 output's platform-boundary declarations (the shared
// X360 D3D9 XDK entry points plus the KI_XCAM_NO_BUFFER_READY status). Same
// include set the rest of XCamYUY2VideoOutput.cpp already uses -- nothing new.
#include "SDKs/XCam/XCamI420VideoOutput.h"

// ===========================================================================
// XCAM::CYUY2VideoOutput::GetNextBufferRGB -- reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x82984BD0 (raw asm; the Hex-Rays pseudocode is
// mostly inline __asm for this one and only corroborates the constants).
//
// PARTFILE: this body belongs at the end of XCamYUY2VideoOutput.cpp, after
// GetNextBuffer; it is landed here so the wave's implementers never collide on
// one file. Merging it is a straight append of the anonymous namespace + the
// method (a second unnamed-namespace block in the same TU is legal C++).
//
// The X360 body is VMX128: it builds the 4x4 BT.601 matrix in stack floats,
// transposes it with vmrghw/vmrglw, assembles the input vector [y,u,v,1] and
// dots it with vmsum4fp128, saturates with vmaxfp/vminfp and packs the four
// lanes into one A8R8G8B8 word with an insrwi chain. Everything below is the
// faithful scalar de-vectorisation of that. `XCAM` is an X360 SDK boundary, so
// its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

namespace
{
    // --- rodata consumed by GetNextBufferRGB -------------------------------
    // Every value below was dumped from the image (headless IDA, 2026-08-04;
    // table in scratchpad/waveM/YUY2Output.spec.md) and each decimal literal
    // round-trips bit-exactly to the raw dword it was measured from. MEASURED,
    // not inferred. Note these are the XDK/MSDN 3-decimal ROUNDED BT.601
    // family (1.164 / 1.596 / 0.813 / 0.391 / 2.018), NOT the full-precision
    // textbook coefficients (1.164383 / 1.596027 / ...): "correcting" them
    // would diverge from the binary. The luma bias is 0.0625 = 16/256.
    //
    // Kept file-local (internal linkage, no ODR exposure) rather than hoisted
    // into a shared header: the only other consumer of the same rodata,
    // XCAM::CI420VideoOutput::GetNextBufferRGB @ 0x82985360, is not in the
    // ledger and not yet reconstructed. Hoist when that body is written.

    const f32 KF_XCAM_BYTE_TO_UNIT   = 1.0f / 255.0f; // flt_82010C1C 0x3B808081
    const f32 KF_XCAM_UNIT_TO_BYTE   = 255.0f;        // flt_82010C20 0x437F0000
    const f32 KF_XCAM_LUMA_BIAS      = 0.0625f;       // flt_82046E00 0x3D800000
    const f32 KF_XCAM_CHROMA_BIAS    = 0.5f;          // flt_82001DA0 0x3F000000

    const f32 KF_XCAM_YUV_Y_SCALE    = 1.164f;        // flt_8210C330 0x3F94FDF4
    const f32 KF_XCAM_YUV_V_TO_R     = 1.596f;        // flt_8210C320 0x3FCC49BA
    const f32 KF_XCAM_YUV_V_TO_G     = -0.813f;       // flt_8210C324 0xBF5020C5
    const f32 KF_XCAM_YUV_U_TO_G     = -0.391f;       // flt_8210C328 0xBEC83127
    const f32 KF_XCAM_YUV_U_TO_B     = 2.018f;        // flt_8210C32C 0x400126E9

    // The w lane of the input vector [y, u, v, 1]. The matrix's alpha row is
    // {0, 0, 0, 1}, so alpha is always this value. (The matrix's remaining
    // zero lanes come from flt_82001CC0 = 0.0; a scalar form drops those
    // terms.)
    const f32 KF_XCAM_YUV_W          = 1.0f;          // flt_82001C98 0x3F800000

    // unk_8210C300: two 16-byte vectors, {0,0,0,0} at +0x00 and {1,1,1,1} at
    // +0x10, used as the vmaxfp/vminfp operands. MEASURED in full (the block
    // is bounded at 32 bytes by flt_8210C320 and both XCAM converters load
    // exactly these two vectors). Net semantics: saturate to [0, 1].
    const f32 KF_XCAM_SATURATE_MIN   = 0.0f;          // unk_8210C300 +0x00
    const f32 KF_XCAM_SATURATE_MAX   = 1.0f;          // unk_8210C300 +0x10

    // vmaxfp against {0,0,0,0} then vminfp against {1,1,1,1}. The inputs are
    // an affine combination of camera bytes, so they can never be NaN and the
    // ordered/unordered distinction (gotcha 4) does not arise here.
    f32 SaturateUnit(f32 fValue)
    {
        const f32 fLowClamped = (fValue < KF_XCAM_SATURATE_MIN) ? KF_XCAM_SATURATE_MIN : fValue;
        return (fLowClamped > KF_XCAM_SATURATE_MAX) ? KF_XCAM_SATURATE_MAX : fLowClamped;
    }

    // One pixel of the BT.601 conversion: dot [y, u, v, 1] with the four
    // matrix columns, saturate, scale to bytes and pack A8R8G8B8.
    //
    // Lane->channel assignment is proven from the asm's vmrghw shuffle chain:
    // lane0 = R, lane1 = G, lane2 = B, lane3 = A; the insrwi chain then builds
    // A<<24 | R<<16 | G<<8 | B and stores it as one word (stw).
    u32 ConvertYuvToArgb(f32 fY, f32 fU, f32 fV)
    {
        const f32 fRed   = SaturateUnit(KF_XCAM_YUV_Y_SCALE * fY + KF_XCAM_YUV_V_TO_R * fV);
        const f32 fGreen = SaturateUnit(KF_XCAM_YUV_Y_SCALE * fY + KF_XCAM_YUV_U_TO_G * fU
                                                                 + KF_XCAM_YUV_V_TO_G * fV);
        const f32 fBlue  = SaturateUnit(KF_XCAM_YUV_Y_SCALE * fY + KF_XCAM_YUV_U_TO_B * fU);
        const f32 fAlpha = SaturateUnit(KF_XCAM_YUV_W);

        // fctidz = convert-to-integer rounding toward ZERO, so this truncates;
        // there is no +0.5 rounding bias in the binary. The saturate above
        // already bounds every channel to 0..255, so the 8-bit fields the
        // insrwi chain inserts are exact.
        const u32 uRed   = static_cast<u32>(fRed   * KF_XCAM_UNIT_TO_BYTE);
        const u32 uGreen = static_cast<u32>(fGreen * KF_XCAM_UNIT_TO_BYTE);
        const u32 uBlue  = static_cast<u32>(fBlue  * KF_XCAM_UNIT_TO_BYTE);
        const u32 uAlpha = static_cast<u32>(fAlpha * KF_XCAM_UNIT_TO_BYTE);

        return (uAlpha << 24) | ((uRed & 0xFFu) << 16) | ((uGreen & 0xFFu) << 8) | (uBlue & 0xFFu);
    }
}

// @ 0x82984BD0
int CYUY2VideoOutput::GetNextBufferRGB(SVideoBuffer* pDst)
{
    const u32 uIndex = static_cast<u32>(GetNextBufferHelper());
    if (uIndex >= 3)
        return KI_XCAM_NO_BUFFER_READY;

    // Console offsets seen in the asm, for cross-reference only -- all member
    // access here is by name: this+0x74 miWidth, this+0x78 miHeight,
    // 8*(idx+17) = 0x88+8*idx mBuffers[idx].muPitch, +0x8C+8*idx .mpData.
    const SVideoBuffer& rSrc = mBuffers[uIndex];

    // The asm re-loads miWidth/miHeight and pDst->muPitch/mpData every
    // iteration; hoisting them is ordinary semantic-parity cleanup (the loop
    // itself creates no aliasing). The loop comparisons are cmplw/cmplwi --
    // unsigned -- so the counters are u32 and a zero width/height simply skips.
    const u32 uWidth  = static_cast<u32>(miWidth);
    const u32 uHeight = static_cast<u32>(miHeight);

    for (u32 uRow = 0; uRow < uHeight; ++uRow)
    {
        const u8* pSrcRow = static_cast<const u8*>(rSrc.mpData) + uRow * rSrc.muPitch;
        u8*       pDstRow = static_cast<u8*>(pDst->mpData) + uRow * pDst->muPitch;

        // One YUY2 macropixel carries two pixels: 4 source bytes -> 8
        // destination bytes. The bound is `uCol < uWidth` stepping 2, exactly
        // as the binary has it -- an odd width therefore still writes the full
        // trailing pair, which is faithful and deliberately not "fixed".
        for (u32 uCol = 0; uCol < uWidth; uCol += 2)
        {
            const u8* pMacroPixel = pSrcRow + 2u * uCol; // 2 bytes per pixel
            u32*      pOut = reinterpret_cast<u32*>(pDstRow + 4u * uCol); // 4 bytes per pixel

            // YUY2 fourcc byte order: Y0 U Y1 V (all lbz byte loads).
            const f32 fLuma0  = static_cast<f32>(pMacroPixel[0]) * KF_XCAM_BYTE_TO_UNIT - KF_XCAM_LUMA_BIAS;
            const f32 fChromaU = static_cast<f32>(pMacroPixel[1]) * KF_XCAM_BYTE_TO_UNIT - KF_XCAM_CHROMA_BIAS;
            const f32 fLuma1  = static_cast<f32>(pMacroPixel[2]) * KF_XCAM_BYTE_TO_UNIT - KF_XCAM_LUMA_BIAS;
            const f32 fChromaV = static_cast<f32>(pMacroPixel[3]) * KF_XCAM_BYTE_TO_UNIT - KF_XCAM_CHROMA_BIAS;

            // Both pixels share the macropixel's chroma pair; the asm rebuilds
            // the input vector by overwriting only its y lane for pixel 2.
            pOut[0] = ConvertYuvToArgb(fLuma0, fChromaU, fChromaV);
            pOut[1] = ConvertYuvToArgb(fLuma1, fChromaU, fChromaV);
        }
    }

    return 0;
}

} // namespace XCAM
