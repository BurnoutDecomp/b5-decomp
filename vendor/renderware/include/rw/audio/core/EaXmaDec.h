#pragma once

// =====================================================================================
// rw::audio::core::EaXmaDec -- the EARenderWare "rwaudio" XMA (Xbox 360 hardware-format)
// stream decoder plug-in. Like every codec it derives from the shared streaming-decoder
// base rw::audio::core::Decoder and registers its static DecoderDesc into the audio-core
// DecoderRegistry (see DecoderRegistry::RegisterStandardRunTimeDecoders). Referenced by
// Unpack0.h ("... packed-stream readers (PackedColumnReader, EaXmaDec)") and by the PC
// movie-audio glue (CgsMovieAudioPC), which replaces the X360 XMA hardware with FFmpeg.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No Feb-2007
// source and no DecFIGS DWARF exist for this TU. This is a MINIMAL home: only the static
// GetDecoderDesc accessor that DecoderRegistry::RegisterStandardRunTimeDecoders calls is
// declared. EaXmaDec's appended per-instance members (the decode cursor / state past the
// Decoder base) are NOT recovered here -- expand this header, and add the decode-event body
// in its own TU, when those are worked. Sibling to the committed rw::audio::core codec
// homes (Xas1Dec, XasDec, Decoder, ...).
// =====================================================================================

#include "rw/audio/core/Decoder.h" // rw::audio::core::Decoder (base)

namespace rw
{
namespace audio
{
namespace core
{

// rwaudio codec registration descriptor (name / GUID / factory callbacks). Its full home
// is DecoderRegistry.h; forward-declared here (GetDecoderDesc only returns its address).
struct DecoderDesc;

// -------------------------------------------------------------------------------------
// EaXmaDec -- XMA decoder plug-in. Only the static registration accessor is modelled here
// (see the file header). The base (Decoder) carries the request ring and streaming state.
// -------------------------------------------------------------------------------------
class EaXmaDec : public Decoder
{
public:
    // Return the address of this codec's static registration descriptor. Called by
    // DecoderRegistry::RegisterStandardRunTimeDecoders to register the XMA decoder.
    static DecoderDesc *GetDecoderDesc();
};

} // namespace core
} // namespace audio
} // namespace rw
