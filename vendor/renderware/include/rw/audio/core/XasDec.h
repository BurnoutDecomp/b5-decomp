#pragma once

// =====================================================================================
// rw::audio::core::XasDec -- the EARenderWare "rwaudio" XAS (block-ADPCM) stream decoder
// plug-in, "version 0" sibling of Xas1Dec. Where Xas1Dec (XAS1) packs 128-sample blocks
// into 76-byte-per-channel frames, XasDec (XAS0) packs 32-sample blocks into 19-byte-per-
// channel frames (the asm's `mulli r,r,0x13` advance and the fixed `return 32` sample
// count). Like every codec it derives from the shared streaming-decoder base
// rw::audio::core::Decoder (it supplies the +0x14 decode callback and registers via its
// static DecoderDesc); the base owns the request ring + source-buffer bookkeeping
// (see Decoder.h). Confirmed by Decoder.h's own header roster ("... XasDec, Xas1Dec ...
// each supply the +0x14 decode callback").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Sibling to the committed rw::audio::core homes (Decoder, Xas1Dec, EaLayer3Dec, ...).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). Fixed base header is Decoder's (see
// Decoder.h); XasDec appends the same two members past the base as Xas1Dec. Because the
// compile target is x64 PC the base's pointer members widen, so these X360 offsets
// describe the console image only and are reached solely by named member:
//   +0x34  mpEncodedCursor      (u8*  : running pointer into the current XAS frame data)
//   +0x38  miRemainingSamples   (s32  : samples still owed from the current request)
// =====================================================================================

#include "types.hpp" // s32, u8

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
// XasDec -- XAS0 block-ADPCM decoder plug-in. See the file header for the appended
// layout; the base (Decoder) carries the request ring and streaming state. The three
// trivial descriptor callbacks (create / size / desc) are reconstructed here; the decode
// entry indexes un-recovered coefficient rodata and is left to its own TU (see below).
// -------------------------------------------------------------------------------------
class XasDec : public Decoder
{
public:
    // @0x82B91EA0 -- plug-in "create instance" event: seed a fresh XAS stream by clearing
    // the encoded cursor + remaining-sample counter. Always returns 1 (success). Installed
    // as a descriptor callback, so it receives the instance explicitly.
    static s32 CreateInstanceEvent(XasDec *pDecoder);

    // @0x82B91E70 -- report the codec's per-instance allocation footprint: writes the
    // required alignment (4) to *puAlignment and returns the instance size (60 bytes).
    // The instance pointer is passed by the descriptor framework but unused here.
    static s32 GetSize(XasDec *pDecoder, u32 *puAlignment);

    // @0x82B91E80 -- return the address of this codec's static registration descriptor.
    static DecoderDesc *GetDecoderDesc();

    // @0x82B94118 -- FLAGGED / BLOCKED: the codec's decode entry (installed as
    // Decoder::mpDecodeCallback). Pulls the next request when the current one is drained,
    // then decodes one 32-sample XAS0 block per interleaved channel into `pOutput`,
    // returning 32. Its per-channel decode is inlined here (XasDec has no separate
    // DecodeChannel) and indexes the un-recovered XAS ADPCM coefficient rodata tables
    // (flt_8215A7F0 / flt_8215A7F4 -- interleaved filter-coefficient pairs; flt_8215A810 --
    // per-exponent scale) whose bytes are not in the dossier, so the body is left to its
    // own TU rather than fabricated. Declared here so the class shape stays faithful.
    s32 DecodeEvent(DecoderBuffer *pOutput);

private:
    u8  *mpEncodedCursor;      // +0x34
    s32  miRemainingSamples;   // +0x38
};

} // namespace core
} // namespace audio
} // namespace rw
