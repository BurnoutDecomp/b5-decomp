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

class System;
class DecoderRegistry;

// -------------------------------------------------------------------------------------
// Decoder::Request -- one entry of the inline request ring. A request names a half-open
// sample range [miStartSample, miEndSample) the decoder still owes the consumer, together
// with the chunk the producer handed in.
//
// ⭐ NAMED 2026-08-28 (phase E) by Decoder::Feed @0x82B67920, the PRODUCER that fills these
// slots -- an exporter gap, raw-decoded from the XEX. Its stores identify the two leading
// words and the two flag bytes, and five independent consumers corroborate both readings.
//
// ⚠️ THE STRIDE NO LONGER SURVIVES x64, and this comment used to claim the opposite. The
// console stride is a hard-coded 0x14 and the record was believed "every field a 32-bit
// word (no pointers)" -- but TWO of its leading words are pointers (Feed stores the fed
// chunk with `stw r4, 0(r30)` @0x82B6795C and the seek-table base with `stw r8, 4(r30)`
// @0x82B67964), so the host record grows to 32 bytes and every console offset past +0x00
// moves. ⭐ The second pointer was found on 2026-08-29; the revision before that had typed
// only the first and put the host size at 24.
// Consequences, all of which are already handled but must stay handled:
//   * every consumer reaches the ring as RequestQueue()[i] (host array indexing), so they
//     all stay in step automatically -- do NOT reintroduce a literal 0x14 anywhere;
//   * whoever ALLOCATES the ring must size it with sizeof(DecoderRequest). That is
//     DecoderRegistry::DecoderFactory @0x82B6C778 (`mulli r21, r26, 0x14`), which uses
//     sizeof(DecoderRequest) on the widened host;
//   * SndPlayer1::Process indexes this ring directly (`mulli r10, r9, 0x14` @0x82BA0830
//     and @0x82BA0AB4) and must use sizeof(DecoderRequest) when it is bodied;
//   * Decoder+0x20 carries the whole decoder-instance size, which StartRequest truncates
//     into a u16 (`lwz r11,0x20(r3)` / `sth r11,0x28(r28)`). That total grows on the host
//     -- the widened Decoder header, 20 ring slots at +4 bytes each, and the buffer -- so
//     the truncation is a real overflow risk to check when SndPlayer1 lands, not a
//     theoretical one.
// The layout pins that enforce all this live in Decoder.cpp; see the note there.
struct DecoderRequest
{
    const void *mpFedData; // +0x00 -- the chunk bytes Feed was handed (Feed: stw r4)
    // ⭐ NAMED AND RETYPED 2026-08-29. This was `u32 muReserved04`, described as "role not
    // yet attested because every committed call site passes 0". It IS attested, and it is a
    // POINTER: the seek table's base. The chain, verified instruction by instruction:
    //   SeekTableParser::mpSeekData (a u8*, PDB void*)
    //     -> SndPlayer1::SetSeekData  `lwz r11,0x50(r1)` / `stw r11,0x38(r31)` @0x82B9C0E0
    //     -> RequestExternal +0x38
    //     -> SndPlayer1::SubmitChunk  `lwz r8,0x38(r29)` @0x82BA464C   (Feed's 5th argument)
    //     -> Decoder::Feed            `stw r8,4(r30)`    @0x82B67964   (this field)
    // ⚠️ It was ALREADY being dereferenced as a pointer by committed code:
    // EaXmaDec_wG_04.cpp laundered it back through `static_cast<usize>` and even said so in
    // its comment ("holds the seek table's base pointer (again in a 32-bit ring word)").
    // Storing a host pointer in a u32 TRUNCATES it on x64, so that path was reading a
    // corrupted base the moment a real table was ever fed. Typed honestly, the truncation
    // and the cast wart both disappear.
    const u8 *mpSeekData;  // +0x04 -- seek-table base (Feed: stw r8)
    s32 miStartSample;     // +0x08 -- Feed: stw r7
    s32 miEndSample;       // +0x0C  (0 => empty slot; Feed REFUSES a slot whose value is
                           //          non-zero, so this doubles as the busy flag)
    u8  mucContinue;       // +0x10 -- Feed: stb r6. The stream players pass "NOT a decoder
                           //          reset", i.e. true means continue the current stream
    u8  mucFlag11;         // +0x11 -- Feed: stb r9; 0 at every committed call site
    u8  mucPad12[2];       // +0x12 -- pads the record back to its 20-byte stride
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
    System *mpSystem;        // +0x00 -- owning audio System
    f32    *mpData;          // +0x04 -- interleaved sample storage
    usize   mpTempStore;     // +0x08 -- pointer-sized temporary-store word (factory leaves it stale)
    u16     muSampleCursor;  // +0x0C -- valid samples staged in the buffer
    u16     muStride;        // +0x0E -- per-channel frame stride (in samples)
    u8      mucChannelCount; // +0x10 -- number of channels backed by this descriptor
};

// -------------------------------------------------------------------------------------
// Decoder -- see the file header for the byte layout. Polymorphic base (virtual dtor).
// -------------------------------------------------------------------------------------
class Decoder
{
public:
    friend class DecoderRegistry;
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

    // @0x82B67920 -- hand the decoder one more chunk to decode. Fills the request slot at
    // the WRITE cursor, notifies the codec through FeedEvent, advances the cursor, and
    // returns the index of the slot just filled -- which is the "decoder request handle"
    // the sound players store in their feed descriptors.
    //
    // Returns 0 WITHOUT feeding when the slot is still busy. ⚠️ That failure is
    // indistinguishable from successfully filling slot 0; the console has no separate
    // status, and both players call it only after their own GetFeedSlot has confirmed a
    // free slot. Reproduced as-is.
    //
    // (Exporter gap: this address has no dossier, so the body was raw-decoded from the XEX
    // and hand-disassembled. The argument names come from its stores plus the committed
    // call sites in the two SndPlayer1 families' SubmitChunk.)
    // ⚠️ The fifth argument was `u32 uReserved04`. It is the seek-table base pointer (r8),
    // so on x64 that declaration truncated it. See mpSeekData above.
    u8 Feed(const void *pData, s32 iNumSamples, u8 ucContinue, s32 iStartSample,
            const u8 *apSeekData, u8 ucFlag11);

    u32 GetInstanceSize() const { return muInstanceSize; }

protected:
    // Base of the inline request ring: `this` + muRequestQueueOffset. Protected: codec
    // subclasses index the ring directly in the binary (EaXmaDec::DecodeEvent @0x82B96380
    // computes `this + muRequestQueueOffset + 0x14 * mucRequestDecodeIndex` itself).
    // ⚠️ Index through THIS accessor (RequestQueue()[i]) rather than reproducing that
    // console arithmetic: the record grew past 0x14 on the host once its leading pointer
    // was typed honestly -- see the DecoderRequest note above.
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
    System *mpSystemUseGetSystemAccessor; // +0x04
    Decoder *mpDecoder;                  // +0x08 -- self pointer seeded by DecoderFactory
public:
    // The codec callbacks live at +0x0C / +0x14 in the X360 image; subclasses install them.
    void (*mpFinaliser)(Decoder *self);                          // +0x0C
    void *mpAllocatedBlock;                                      // +0x10
    s32 (*mpDecodeCallback)(Decoder *self, DecoderBuffer *pDst,  // +0x14
                            s32 iCount);
protected:
    // Fixed-header state, exposed to codec subclasses (Xas1Dec, EaLayer3DecBase, ...)
    // that derive from Decoder and read/advance it from their own decode callbacks.
    u32   mGuid;                    // +0x18
    s32   miCurrentSampleOffset;    // +0x1C
    u32   muInstanceSize;           // +0x20 -- whole inline decoder allocation
    u32   muRequestQueueOffset;     // +0x24
    u32   muSourceBufferOffset;     // +0x28
    u16   muCarrySamples;           // +0x2C
    u8    mucChannelCount;          // +0x2E
    // ⭐ NAMED 2026-08-28 (phase E) by Decoder::Feed @0x82B67920: this is the ring's WRITE
    // cursor -- Feed indexes the request queue with it, then post-increments it modulo
    // mucRequestCount. It completes the three-cursor set (write / read / decode).
    u8    mucRequestWriteIndex;     // +0x2F
    u8    mucRequestReadIndex;      // +0x30
    u8    mucRequestDecodeIndex;    // +0x31
    u8    mucRequestCount;          // +0x32
    u8    mucUsesSourceBuffer;      // +0x33
};

} // namespace core
} // namespace audio
} // namespace rw
