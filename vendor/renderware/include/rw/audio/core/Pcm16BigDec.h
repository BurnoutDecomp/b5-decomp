#pragma once

// =====================================================================================
// rw::audio::core::Pcm16BigDec -- the EARenderWare "rwaudio" 16-bit big-endian PCM stream
// decoder plug-in. The simplest codec of the family: the "encoded" stream is already raw
// interleaved signed 16-bit PCM, so DecodeEvent merely de-interleaves each channel and
// normalises every sample to [-1,1) f32 (sample * 1/32768). Like every codec it derives
// from the shared streaming-decoder base rw::audio::core::Decoder (it supplies the +0x14
// decode callback and registers via its static DecoderDesc); the base owns the request
// ring + source-buffer bookkeeping (see Decoder.h). Confirmed by Decoder.h's own header
// roster ("... Xas1Dec, Layer3Dec, Pcm16BigDec, Pcm16LittleDec, ... each supply the +0x14
// decode callback").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Sibling to the committed rw::audio::core homes (Decoder, Xas1Dec, XasDec, ...).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). Fixed base header is Decoder's (see
// Decoder.h); Pcm16BigDec appends the same two members past the base as Xas1Dec/XasDec.
// Because the compile target is x64 PC the base's pointer members widen, so these X360
// offsets describe the console image only and are reached solely by named member:
//   +0x34  mpSampleCursor       (s16* : running pointer into the current interleaved PCM)
//   +0x38  miRemainingSamples   (s32  : samples still owed from the current request)
// =====================================================================================

#include "types.hpp" // s16, s32, u32, f32

#include "rw/audio/core/Decoder.h" // rw::audio::core::Decoder (base), DecoderBuffer

namespace rw
{
namespace audio
{
namespace core
{

// rwaudio codec registration descriptor (name / GUID / factory callbacks). Its full
// layout is not recovered here; GetDecoderDesc only hands back the address of this codec's
// static instance, so an incomplete type is sufficient. Referenced by the AEMS sample
// factories (CgsSound::Playback::GenericRwacFactory) and DecoderRegistry.
struct DecoderDesc;

// -------------------------------------------------------------------------------------
// Pcm16BigDec -- raw 16-bit big-endian PCM decoder plug-in. See the file header for the
// appended layout; the base (Decoder) carries the request ring and streaming state.
// -------------------------------------------------------------------------------------
class Pcm16BigDec : public Decoder
{
public:
    // @0x82B91E48 -- report the codec's per-instance allocation footprint: writes the
    // required alignment (16) to *puAlignment and returns the instance size (60 bytes).
    // The instance pointer is passed by the descriptor framework but unused here.
    static s32 GetSize(Pcm16BigDec *pDecoder, u32 *puAlignment);

    // @0x82B91E38 -- return the address of this codec's static registration descriptor.
    static DecoderDesc *GetDecoderDesc();

    // @0x82B951B8 -- the codec's decode entry (installed as Decoder::mpDecodeCallback).
    // Pulls the next request when the current run is drained, seeding the PCM cursor at the
    // request's start sample; then de-interleaves `iNumSamples` samples of every channel
    // out of the interleaved s16 stream into `pOutput` (planar, per-channel stride), each
    // normalised to f32 by 1/32768. Returns the number of samples produced (iNumSamples).
    s32 DecodeEvent(DecoderBuffer *pOutput, s32 iNumSamples);

private:
    s16 *mpSampleCursor;       // +0x34
    s32  miRemainingSamples;   // +0x38
};

} // namespace core
} // namespace audio
} // namespace rw
