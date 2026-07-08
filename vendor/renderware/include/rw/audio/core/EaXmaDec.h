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
// source and no DecFIGS DWARF exist for this TU.
//
// PARTIAL HOME. Only the functions that can be grounded WITHOUT the (un-recovered) XMA
// hardware-context layout, the shared XMA context free-list pool, and the codec's own
// appended per-instance decode state are modelled here:
//   * GetDecoderDesc  -- static registration accessor (declared; body needs the static
//                        DecoderDesc bytes + AllocateResources -> its own slice).
//   * GetSize         -- per-instance footprint sizing (pure arithmetic).
//   * FeedEvent       -- thin forwarder onto the internal Service pump.
//   * LeftShiftInBits / ParseToNextFrame -- XMA bitstream frame-walk helpers; they operate
//                        purely on the packed XMA byte stream, not on instance state.
//   * XMAmemcpy       -- VMX block copy used to refill the XMA input buffers.
// The decode-driving members (AllocateResources, CreateInstanceEvent, DecodeEvent, Service,
// Reset, StartPair, ReleaseEvent, EnterRecoveryMode) reach the XMA hardware contexts and
// the codec's appended state by byte offset; their layout is not recovered, so they are
// left for a future slice. Sibling to the committed rw::audio::core codec homes (Xas1Dec,
// XasDec, Pcm16BigDec, Decoder, ...).
// =====================================================================================

#include "types.hpp" // u8, s32, u32

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
// EaXmaDec -- XMA decoder plug-in. See the file header for what is / isn't modelled. The
// base (Decoder) carries the request ring and streaming state; the appended per-instance
// decode state (the XMA context-pair records, the streaming cursors) is NOT recovered.
// -------------------------------------------------------------------------------------
class EaXmaDec : public Decoder
{
public:
    // @0x82B93B88 -- return the address of this codec's static registration descriptor.
    // Called by DecoderRegistry::RegisterStandardRunTimeDecoders to register the XMA decoder.
    static DecoderDesc *GetDecoderDesc();

    // @0x82B93C78 -- report the codec's per-instance allocation footprint: writes the
    // required alignment (16) through *puAlignment and returns the instance size, which
    // grows with the channel count -- 88 bytes of fixed state plus one 28-byte XMA
    // context-pair record per stereo pair (ceil(channels/2)). The descriptor framework
    // passes the channel count in the first register (not an instance pointer).
    static u32 GetSize(u32 uNumChannels, u32 *puAlignment);

    // @0x82B96140 -- decoder "feed more input" callback; a thin tail-call onto Service.
    s32 FeedEvent();

private:
    // Internal streaming pump: drains decoded XMA output and refills the XMA hardware input
    // buffers. Declared so FeedEvent can forward to it; the body reaches the XMA contexts by
    // byte offset and lives in its own (not-yet-recovered) slice.
    s32 Service();

    // ----- XMA packed-bitstream frame-walk helpers (no instance state) -----------------

    // @0x82B8FB20 -- pull `iNumBits` big-endian bits starting at bit position `iBitPos`
    // out of the packed byte stream `pStream`, shifting them into the low end of the 32-bit
    // accumulator *puAccum (the prior contents are shifted up by `iNumBits` first). Returns
    // the accumulator pointer. No-op for iNumBits <= 0.
    static u32 *LeftShiftInBits(u32 *puAccum, const u8 *pStream, s32 iBitPos, s32 iNumBits);

    // @0x82B8FB88 -- advance a (byte-cursor, bit-offset) pair across one XMA frame, decoding
    // the frame-length field with LeftShiftInBits and wrapping the cursor onto the next
    // 2048-byte XMA packet when the bit offset crosses a packet boundary.
    static void ParseToNextFrame(const u8 **ppCursor, s32 *piBitOffset);

    // @0x82B8FA18 -- copy `iSize` bytes from `pSrc` to `pDst` (used to top up the XMA input
    // buffers). The X360 body is a hand-unrolled VMX unaligned block copy; semantically a
    // plain byte copy. A member (the instance is passed in r3 by every call site) but the
    // asm overwrites r3 with pDst before use, so `this` is unreferenced. Returns pDst.
    void *XMAmemcpy(void *pDst, const void *pSrc, s32 iSize);
};

} // namespace core
} // namespace audio
} // namespace rw
