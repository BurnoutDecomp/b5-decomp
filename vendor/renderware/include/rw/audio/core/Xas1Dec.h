#pragma once

// =====================================================================================
// rw::audio::core::Xas1Dec -- the EARenderWare "rwaudio" XAS (block-ADPCM) stream
// decoder plug-in. XAS packs 128-sample blocks per channel into fixed 76-byte frames:
// four 4-sample sub-blocks, each prefixed by a byte pair (filter-coefficient index +
// per-sub-block exponent) followed by the 4-bit residuals. Xas1Dec derives from the
// shared streaming-decoder base rw::audio::core::Decoder (it supplies the +0x14 decode
// callback, DecodeEvent, and registers via its static DecoderDesc); the base owns the
// request ring + source-buffer bookkeeping (see Decoder.h).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Sibling to the committed rw::audio::core homes (Decoder, EaLayer3Dec, Unpack0, ...).
// The rwaudio type name / base is confirmed by Decoder.h's own header roster
// (".. XasDec, Xas1Dec, Layer3Dec .. each supply the +0x14 decode callback").
//
// LAYOUT NOTE (X360 byte offsets, from the asm). Fixed base header is Decoder's (see
// Decoder.h); Xas1Dec appends two members past the base. Because the compile target is
// x64 PC the base's pointer members widen, so these X360 offsets describe the console
// image only and are reached solely by named member:
//   +0x34  mpEncodedCursor      (u8*  : running pointer into the current XAS frame data)
//   +0x38  miRemainingSamples   (s32  : samples still owed from the current request)
// =====================================================================================

#include "types.hpp" // s32, u8, f32

#include "rw/audio/core/Decoder.h" // rw::audio::core::Decoder (base), DecoderBuffer

namespace rw
{
namespace audio
{
namespace core
{

// rwaudio codec registration descriptor (name / GUID / factory callbacks). Its full
// layout is not recovered here; GetDecoderDesc only hands back the address of this
// codec's static instance, so an incomplete type is sufficient. Referenced by
// DecoderRegistry::RegisterStandardRunTimeDecoders and the AEMS sample factories.
struct DecoderDesc;

// -------------------------------------------------------------------------------------
// Xas1Dec -- XAS block-ADPCM decoder plug-in. See the file header for the appended
// layout; the base (Decoder) carries the request ring and streaming state.
// -------------------------------------------------------------------------------------
class Xas1Dec : public Decoder
{
public:
    // @0x82B91E90 -- return the address of this codec's static registration descriptor.
    static DecoderDesc *GetDecoderDesc();

    // @0x82B94308 -- the codec's decode entry (installed as Decoder::mpDecodeCallback).
    // Pulls the next request when the current one is drained, decodes every interleaved
    // channel of one 128-sample block into `pOutput`, discards the samples that precede
    // the request's start, and returns the number of samples produced.
    s32 DecodeEvent(DecoderBuffer *pOutput);

    // @0x82B91EB8 -- FLAGGED / BLOCKED: decode one channel's 128-sample XAS block from
    // `pEncoded` into `pOutput`. Its body indexes the un-recovered XAS ADPCM coefficient
    // @0x82B91EB8 -- decode one 76-byte / 128-sample XAS1 channel block (four parallel
    // 32-sample sub-streams). Bodied in Xas1Dec.cpp (2026-08-25: was declared-only
    // pending the coefficient rodata, since recovered -- the XasDec.cpp tables'
    // byte-identical per-TU duplicates at flt_8215A844/flt_8215A864).
    u8 *DecodeChannel(u8 *pEncoded, f32 *pOutput);

private:
    u8  *mpEncodedCursor;      // +0x34
    s32  miRemainingSamples;   // +0x38
};

} // namespace core
} // namespace audio
} // namespace rw
