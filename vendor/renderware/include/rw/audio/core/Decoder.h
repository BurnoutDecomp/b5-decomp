#pragma once

// =====================================================================================
// rw::audio::core::Decoder
//
// EARenderWare "rwaudio" streaming-decoder base. Every concrete codec plug-in derives
// from Decoder (rw::audio::core::EaLayer3DecBase : public Decoder, XasDec, Xas1Dec,
// Layer3Dec, Pcm16BigDec, Pcm16LittleDec, ... each supply the +0x14 decode callback and
// register via DecoderRegistry). The base owns:
//   * a small ring of "request" descriptors (sample ranges to hand out), reached through
//     a byte-offset stored in the object (mRequestQueueOffset) because the ring is
//     allocated inline behind the fixed header at instance-create time; and
//   * an optional internal "source" sample-buffer descriptor (mSourceBufferOffset), also
//     inline, used when the codec must decode through a scratch buffer before scattering
//     into the caller's output buffer.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Sibling to the committed rw::audio::core homes (PlugIn, Collection, TimerManager,
// EaLayer3Dec, ...). The rwaudio type name is confirmed verbatim by the EaLayer3Dec home
// (`EaLayer3DecBase : public Decoder`).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). Because the compile target is x64 PC,
// pointer members widen (vptr / mpFinaliser / mpAllocatedBlock / mpDecodeCallback), so
// the *fixed* header offsets below describe the X360 image and are reached only by named
// member -- never by hardcoded offset. The two inline sub-objects are reached by the
// runtime-stored byte offset the allocator wrote, so that arithmetic is width-agnostic.
//   +0x00  vtable pointer            (polymorphic base -> virtual destructor)
//   +0x0C  mpFinaliser               (codec teardown callback, or null)
//   +0x10  mpAllocatedBlock          (extra allocation owned by the codec, or null)
//   +0x14  mpDecodeCallback          (the codec's decode entry: fills a Buffer)
//   +0x1C  miCurrentSampleOffset     (decode cursor within the current request chunk)
//   +0x24  muRequestQueueOffset      (byte offset from `this` to Request[0])
//   +0x28  muSourceBufferOffset      (byte offset from `this` to the source Buffer)
//   +0x2C  muCarrySamples            (samples already sitting in the source buffer)
//   +0x2E  mucChannelCount           (interleaved channel count)
//   +0x30  mucRequestReadIndex       (ring cursor used by GetCurrentRequestDesc)
//   +0x31  mucRequestDecodeIndex     (ring cursor used by Decode / AdvanceDecodeState)
//   +0x32  mucRequestCount           (ring modulus)
//   +0x33  mucUsesSourceBuffer       (non-zero => decode through the source Buffer)
// =====================================================================================

#include "types.hpp" // s32, u32, u16, u8

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Decoder::Request -- one entry of the inline request ring. A request names a half-open
// sample range [miStartSample, miEndSample) the decoder still owes the consumer. The
// stride is a hard-coded 0x14 in the asm (`mulli r,r,0x14`), so this struct is pinned to
// exactly 20 bytes; every field is a 32-bit word (no pointers) so the stride survives the
// x64 widening. Only miStartSample/miEndSample are touched by the reconstructed methods;
// the leading/trailing words are opaque ring bookkeeping and preserved as padding.
// -------------------------------------------------------------------------------------
struct DecoderRequest
{
    u32 muReserved00;   // +0x00
    u32 muReserved04;   // +0x04
    s32 miStartSample;  // +0x08
    s32 miEndSample;    // +0x0C  (0 => empty slot; cleared when fully consumed)
    u32 muReserved10;   // +0x10
};

// -------------------------------------------------------------------------------------
// Decoder::Buffer -- an interleaved sample-buffer descriptor. Used both for the decoder's
// inline "source" scratch buffer and for the caller-supplied output buffer passed to
// Decode. `mpData` points at interleaved f32 frames; a channel `c` starts at
// mpData[muStride * c]; muSampleCursor is the count of valid samples currently staged in
// the buffer. Reached only by named member, so its (widening) size is not layout-critical.
// -------------------------------------------------------------------------------------
struct DecoderBuffer
{
    u32   muReserved00;   // +0x00
    f32  *mpData;         // +0x04  interleaved sample data
    u32   muReserved08;   // +0x08
    u16   muSampleCursor; // +0x0C  valid samples staged in the buffer
    u16   muStride;       // +0x0E  per-channel frame stride (in samples)
};

// -------------------------------------------------------------------------------------
// Decoder -- see the file header for the byte layout. Polymorphic base (virtual dtor).
// -------------------------------------------------------------------------------------
class Decoder
{
public:
    // --- virtuals, in X360 vtable-slot order ---------------------------------------
    // The X360 Decoder vtable (off_8214B1CC, .rdata) is two slots:
    //   [0] @+0x00 -> 0x82AD5078, the image's shared empty thunk (a lone `blr`): the
    //       base "feed more input" callback has an empty body the linker ICF-folded
    //       onto that thunk (same fold Delay.cpp documents for trivial destructors).
    //   [1] @+0x04 -> the `vector deleting destructor' @0x82B678D8.
    // The EaXmaDec override vtable (off_8215A928) confirms the slot meaning:
    //   { [0] EaXmaDec::FeedEvent @0x82B96140, [1] EaXmaDec deleting dtor @0x82B93ED0 }.
    // FeedEvent is therefore declared BEFORE the destructor so the host vtable keeps the
    // console slot order. The base body is genuinely empty in the binary (bare `blr`),
    // and void -- it neither computes nor returns anything; EaXmaDec overrides it with
    // its Service tail-call.
    virtual void FeedEvent() {}

    virtual ~Decoder();

    // @0x82691528 -- run the teardown callback, free the owned block, free `this`.
    void Release();

    // @0x82B67A50 -- decode up to `iNumSamples` samples into `pOutput`; returns the
    // number actually produced.
    s32 Decode(DecoderBuffer *pOutput, s32 iNumSamples);

    // @0x82B679D8 -- advance the decode cursor by `iCount`, rolling to the next request
    // slot when the current one is exhausted.
    void AdvanceDecodeState(s32 iCount);

    // @0x82B92050 -- return the request at the read cursor (advancing it), or null when
    // that slot is empty.
    DecoderRequest *GetCurrentRequestDesc();

    // @0x826914D0 -- samples still owed by request `ucIndex`, or 0 if that slot is empty.
    s32 GetSamplesRemaining(u8 ucIndex);

protected:
    // Base of the inline request ring: `this` + muRequestQueueOffset. Protected: codec
    // subclasses index the ring directly in the binary (EaXmaDec::DecodeEvent @0x82B96380
    // computes `this + muRequestQueueOffset + 0x14 * mucRequestDecodeIndex` itself).
    DecoderRequest *RequestQueue()
    {
        return reinterpret_cast<DecoderRequest *>(
            reinterpret_cast<u8 *>(this) + muRequestQueueOffset);
    }
    // The inline source Buffer: `this` + muSourceBufferOffset.
    DecoderBuffer *SourceBuffer()
    {
        return reinterpret_cast<DecoderBuffer *>(
            reinterpret_cast<u8 *>(this) + muSourceBufferOffset);
    }

    // --- fixed header (X360 offsets in the file-header comment) ---
    u8    mPad04[8];                // +0x04 .. +0x0B  (opaque)
public:
    // The codec callbacks live at +0x0C / +0x14 in the X360 image; subclasses install them.
    void (*mpFinaliser)(Decoder *self);                          // +0x0C
    void *mpAllocatedBlock;                                      // +0x10
    s32 (*mpDecodeCallback)(Decoder *self, DecoderBuffer *pDst,  // +0x14
                            s32 iCount);
protected:
    // Fixed-header state, exposed to codec subclasses (Xas1Dec, EaLayer3DecBase, ...)
    // that derive from Decoder and read/advance it from their own decode callbacks.
    u8    mPad18[4];                // +0x18 .. +0x1B  (opaque)
    s32   miCurrentSampleOffset;    // +0x1C
    u8    mPad20[4];                // +0x20 .. +0x23  (opaque)
    u32   muRequestQueueOffset;     // +0x24
    u32   muSourceBufferOffset;     // +0x28
    u16   muCarrySamples;           // +0x2C
    u8    mucChannelCount;          // +0x2E
    u8    mPad2F;                   // +0x2F
    u8    mucRequestReadIndex;      // +0x30
    u8    mucRequestDecodeIndex;    // +0x31
    u8    mucRequestCount;          // +0x32
    u8    mucUsesSourceBuffer;      // +0x33
};

} // namespace core
} // namespace audio
} // namespace rw
