// =====================================================================================
// rw::audio::core::XasDec bodies.
//
// EARenderWare "rwaudio" XAS0 (block-ADPCM) stream decoder plug-in, "version 0" sibling
// of Xas1Dec. Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative
// for every offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist
// for this TU. See XasDec.h for the appended layout and Decoder.h for the shared base.
//   CreateInstanceEvent @0x82B91EA0
//   GetSize             @0x82B91E70
//   GetDecoderDesc      @0x82B91E80
//   DecodeEvent         @0x82B94118  -- the XAS0 block decode; its coefficient rodata
//                                       (flt_8215A7F0 / flt_8215A810) is dumped below.
//
// The `_savegprlr_24` / `_restgprlr_24` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped.
// =====================================================================================

#include "rw/audio/core/XasDec.h"

namespace rw
{
namespace audio
{
namespace core
{

// This codec's static registration descriptor (off_82F8A528 in the X360 rodata). Its
// bytes (name / GUID / factory callbacks) are not recovered here; GetDecoderDesc only
// returns its address, and DecoderDesc is an incomplete type, so no contents are needed
// (nor fabricated). Same pattern as the foreign statics reached from Xas1Dec.cpp.
extern "C" DecoderDesc off_82F8A528;

// -------------------------------------------------------------------------------------
// CreateInstanceEvent @0x82B91EA0
// Plug-in "create instance" event: reset the streaming state for a fresh XAS stream by
// clearing the encoded cursor and the remaining-sample counter, then report success.
// The asm operates on the instance passed in r3 and always returns 1.
// -------------------------------------------------------------------------------------
s32 XasDec::CreateInstanceEvent(XasDec *pDecoder)
{
    pDecoder->miRemainingSamples = 0;   // +0x38
    pDecoder->mpEncodedCursor = 0;      // +0x34
    return 1;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B91E70
// Report the codec's per-instance allocation footprint: the required alignment (4) is
// written through the out-param and the fixed X360 instance size (60 bytes / 0x3C) is
// returned. The descriptor framework passes the instance pointer in r3, but the asm
// overwrites r3 with the return value without reading it, so it is unused here.
// -------------------------------------------------------------------------------------
s32 XasDec::GetSize(XasDec *pDecoder, u32 *puAlignment)
{
    (void)pDecoder;
    *puAlignment = 4;
    return 60;
}

// -------------------------------------------------------------------------------------
// GetDecoderDesc @0x82B91E80
// Hand back the address of this codec's static registration descriptor. Callers
// (DecoderRegistry::RegisterStandardRunTimeDecoders, the AEMS sample factories) use it to
// register / instantiate the XAS0 decoder.
// -------------------------------------------------------------------------------------
DecoderDesc *XasDec::GetDecoderDesc()
{
    return &off_82F8A528;
}

// -------------------------------------------------------------------------------------
// XAS0 coefficient rodata, dumped from BURNOUT_X360_ARTIST.XEX (headless IDA, big-endian
// f32). These are the classic EA-XA ADPCM prediction filters and the residual scale
// ladder; the binary keeps an IDENTICAL second copy at 0x8215A844/0x8215A864 for
// Xas1Dec::DecodeChannel (per-TU `static const` duplication), which is why each TU
// carries its own copy here too.
//
// KF_XAS0_FILTER_COEF @0x8215A7F0 (flt_8215A7F0 / flt_8215A7F4, 8-byte pair stride).
// [i][0] multiplies out[n-1], [i][1] multiplies out[n-2]. Entry count 4 is MEASURED:
// the table runs exactly up to the scale ladder at +0x20 (0x8215A810), and the
// Xas1Dec duplicate at 0x8215A844 has the same 0x20-byte span.
static const f32 KF_XAS0_FILTER_COEF[4][2] =
{
    { 0.0f,      0.0f      }, // 0x00000000, 0x00000000
    { 0.9375f,   0.0f      }, // 0x3F700000, 0x00000000   ( 240/256, 0)
    { 1.796875f, -0.8125f  }, // 0x3FE60000, 0xBF500000   ( 460/256, -208/256)
    { 1.53125f,  -0.859375f}, // 0x3FC40000, 0xBF5C0000   ( 392/256, -220/256)
};

// KF_XAS0_RESIDUAL_SCALE @0x8215A810 (flt_8215A810): entry[e] = 2^-(31+e). A 4-bit
// residual nibble is pre-shifted to nibble<<28 before the multiply, so exponent e
// yields nibble * 2^-(3+e) -- full scale +/-1.0 at e=0. Entry count 13 is MEASURED
// three ways: (a) the next rodata item (Xas1Dec's duplicate pair table) starts at
// 0x8215A844 = 0x8215A810 + 13*4; (b) the Xas1Dec duplicate's own scale copy at
// 0x8215A864 is bounded by the pointer table off_8215A898 at exactly +13 entries;
// (c) the exact 2^-n geometric ladder stops after 13 steps (2^-31 .. 2^-43).
static const f32 KF_XAS0_RESIDUAL_SCALE[13] =
{
    4.65661287e-10f, // 0x30000000  2^-31
    2.32830644e-10f, // 0x2F800000  2^-32
    1.16415322e-10f, // 0x2F000000  2^-33
    5.82076609e-11f, // 0x2E800000  2^-34
    2.91038305e-11f, // 0x2E000000  2^-35
    1.45519152e-11f, // 0x2D800000  2^-36
    7.27595761e-12f, // 0x2D000000  2^-37
    3.63797881e-12f, // 0x2C800000  2^-38
    1.81898940e-12f, // 0x2C000000  2^-39
    9.09494702e-13f, // 0x2B800000  2^-40
    4.54747351e-13f, // 0x2B000000  2^-41
    2.27373675e-13f, // 0x2A800000  2^-42
    1.13686838e-13f, // 0x2A000000  2^-43
};

// Seed-sample dequantisation constant (flt_820ADBE8 = 0x38000000 = 2^-15 = 1/32768,
// shared image-wide): the two 16-bit seed samples per block are normalised to [-1, 1).
static const f32 KF_XAS0_SEED_SCALE = 3.0517578125e-05f;

// -------------------------------------------------------------------------------------
// DecodeEvent @0x82B94118
// Codec decode callback (installed as Decoder::mpDecodeCallback). Decodes one 32-sample
// XAS0 block for every interleaved channel into `pOutput` and always returns 32.
//
// When the current request is drained (miRemainingSamples <= 0), pull the next request
// descriptor and re-seed: the encoded cursor lands at the request's data base and the
// remaining-sample counter is loaded from the request's end sample. Unlike Xas1Dec
// (XAS1), XAS0 never seeks by the request's start sample and never trims leading
// samples -- every call yields a whole 32-sample block. The binary dereferences the
// GetCurrentRequestDesc result without a null guard (lbz r11,0x10(r3) @0x82B94140
// directly after the call), and that shape is reproduced verbatim.
//
// Frame layout, 0x13 (19) bytes per channel per block, channel-interleaved:
//   bytes [0      .. 2*ch)   per-channel 2-byte header0: seed sample out[0] in the
//                            upper 12 bits of a LITTLE-endian 16-bit word (the asm
//                            composes (s8)byte1<<8 | byte0&0xF0), filter index in the
//                            low nibble of byte0;
//   bytes [2*ch   .. 4*ch)   per-channel 2-byte header1: seed sample out[1], residual
//                            scale exponent in the low nibble of its byte0;
//   bytes [4*ch   .. 19*ch)  15 rows of `ch` residual bytes (row-major across
//                            channels); each byte holds two 4-bit residuals, HIGH
//                            nibble first.
// Recurrence per sample: out[n] = out[n-1]*c1 + out[n-2]*c2 + residual * scale.
//
// INDEX-RANGE NOTE: the asm extracts both table selectors as raw 4-bit nibbles
// (rlwinm masks, no bound check), while the rodata tables physically hold 4 pairs /
// 13 scales. A filter index > 3 or exponent > 12 read past the table into the
// neighbouring rodata on console (garbage audio, not a fault); valid XAS0 streams
// never encode them. The nibble extraction is reproduced as-is here, so malformed
// input indexes out of range exactly as the console build would.
// -------------------------------------------------------------------------------------
s32 XasDec::DecodeEvent(DecoderBuffer *pOutput)
{
    if (miRemainingSamples <= 0)
    {
        DecoderRequest *lpDesc = GetCurrentRequestDesc();

        // Flag byte at Request+0x10: when clear, reset the streaming state for a fresh
        // request. Both stores are re-written just below -- dead on this path, but they
        // are the binary's shape (stw @0x82B9414C/0x82B94150) and are kept.
        if (!lpDesc->muReserved10)
        {
            miRemainingSamples = 0;
            mpEncodedCursor = 0;
        }

        // Request+0x00 holds the base pointer of this request's encoded XAS0 data;
        // Request+0x0C seeds the remaining-sample counter directly (no start-sample
        // arithmetic -- the asm never reads Request+0x08).
        mpEncodedCursor = reinterpret_cast<u8 *>(
            static_cast<usize>(lpDesc->muReserved00));
        miRemainingSamples = lpDesc->miEndSample;
    }

    const u32 luChannels = mucChannelCount;
    for (u32 luChannel = 0; luChannel < luChannels; ++luChannel)
    {
        f32      *lpOut   = pOutput->mpData + pOutput->muStride * luChannel;
        const u8 *lpFrame = mpEncodedCursor;

        // header0: seed sample out[0] + this channel's filter selector.
        const u8  lucByte0     = lpFrame[2 * luChannel];
        const s8  lscByte1     = static_cast<s8>(lpFrame[2 * luChannel + 1]);
        const u32 luCoefIndex  = lucByte0 & 0x0F;
        const f32 lfCoefPrev1  = KF_XAS0_FILTER_COEF[luCoefIndex][0];
        const f32 lfCoefPrev2  = KF_XAS0_FILTER_COEF[luCoefIndex][1];
        lpOut[0] = static_cast<f32>((lscByte1 << 8) + (lucByte0 & 0xF0)) *
                   KF_XAS0_SEED_SCALE;

        // header1: seed sample out[1] + this channel's residual scale exponent.
        const u8  lucByte2 = lpFrame[2 * luChannels + 2 * luChannel];
        const s8  lscByte3 = static_cast<s8>(lpFrame[2 * luChannels + 2 * luChannel + 1]);
        const f32 lfScale  = KF_XAS0_RESIDUAL_SCALE[lucByte2 & 0x0F];
        lpOut[1] = static_cast<f32>((lscByte3 << 8) + (lucByte2 & 0xF0)) *
                   KF_XAS0_SEED_SCALE;

        // 15 residual bytes per channel -> 30 predicted samples (out[2] .. out[31]).
        // Each nibble is planted at bit 28 (nibble<<28, sign carried by nibble bit 3)
        // before the int->float convert, exactly as the asm's rlwinm+extsw+fcfid does.
        const u8 *lpResidual = lpFrame + 4 * luChannels + luChannel;
        u32 luSample = 2;
        for (s32 liPair = 15; liPair != 0; --liPair)
        {
            const u32 luByte = *lpResidual;
            lpResidual += luChannels;

            const s32 liHi = static_cast<s32>((luByte << 24) & 0xF0000000u);
            const s32 liLo = static_cast<s32>(luByte << 28);

            lpOut[luSample] = lpOut[luSample - 1] * lfCoefPrev1 +
                              lpOut[luSample - 2] * lfCoefPrev2 +
                              static_cast<f32>(liHi) * lfScale;
            ++luSample;
            lpOut[luSample] = lpOut[luSample - 1] * lfCoefPrev1 +
                              lpOut[luSample - 2] * lfCoefPrev2 +
                              static_cast<f32>(liLo) * lfScale;
            ++luSample;
        }
    }

    // Consume the block: 0x13 (19) bytes per channel (`mulli r10, r25, 0x13`), and a
    // fixed 32 samples off the remaining count. Runs even with zero channels.
    mpEncodedCursor += 19 * luChannels;
    miRemainingSamples -= 32;
    return 32;
}

} // namespace core
} // namespace audio
} // namespace rw
