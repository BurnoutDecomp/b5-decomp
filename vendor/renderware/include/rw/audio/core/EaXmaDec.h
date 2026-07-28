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
// ARCHITECTURE. The codec drives the X360's XMA hardware decoder (the XMA* HAL declared in
// SDKs/XAudio/XmaHardware.h) through a SHARED, file-scope pool of hardware-context records
// built once by AllocateResources: per context one XmaContext, two 2048-byte encoded-input
// packet buffers, a 6144-byte decoded-PCM output ring and a 512-byte overlap buffer, all
// carved from two physical allocations, threaded on an intrusive free/in-use double list
// under a pool mutex. Instances claim ceil(channels/2) records (one per stereo pair) in
// CreateInstanceEvent and return them in ReleaseEvent. DecodeEvent drains the hardware
// output rings (s16 -> f32 via CopySamplesOut) while Service tops up the input double
// buffers; StartPair walks a request's seek table + XMA packet headers to the start frame,
// and Reset / EnterRecoveryMode re-initialise the hardware contexts.
//
// DESC vs VTABLE (recovered from the image, scratchpad/waveG/eaxma_dump.txt):
//   * static DecoderDesc off_82F89394 (.data, registered by GetDecoderDesc) carries the
//     factory callbacks { +0x00 GetSize, +0x04 CreateInstanceEvent, +0x08 ReleaseEvent,
//     +0x0C DecodeEvent, +0x10 mpNext = 0, +0x14 muId = 'EXm0' }.
//   * the vtable off_8215A928 is two slots { [0] FeedEvent, [1] deleting dtor }, matching
//     the Decoder base slot order (Decoder.h).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). Fixed base header is Decoder's (see
// Decoder.h); EaXmaDec appends the members below. The compile target is x64 PC, so the
// pointer members widen and the console offsets describe the X360 image only -- every
// access is by named member. The per-pair table is NOT a member array: the descriptor
// framework allocates GetSize() bytes and CreateInstanceEvent lays the pair records at
// the first 8-aligned address past the fixed header ((this + 0x5F) & ~7 on the console,
// align8(this + sizeof(EaXmaDec)) on the host), so GetSize/CreateInstanceEvent must use
// the HOST sizes -- the console literals 0x58/0x1C/28 would corrupt memory on x64.
// =====================================================================================

#include "types.hpp" // u8, s16, s32, u32, f32

#include "rw/audio/core/Decoder.h" // rw::audio::core::Decoder (base), DecoderBuffer/Request

struct XmaContext; // opaque XMA hardware context (SDKs/XAudio/XmaHardware.h)

namespace EA
{
namespace Thread
{
class Mutex; // pool mutex; pointer-only here -- full type in SDKs/EATech/eathread
} // namespace Thread
} // namespace EA

namespace rw
{
namespace audio
{
namespace core
{

// rwaudio codec registration descriptor (factory callbacks / GUID). Its full home is
// DecoderRegistry.h; forward-declared here (GetDecoderDesc only returns its address).
struct DecoderDesc;

// -------------------------------------------------------------------------------------
// EaXmaDec -- XMA decoder plug-in. See the file header for the architecture. The base
// (Decoder) carries the request ring and streaming state; EaXmaDec appends the per-pair
// decode state and owns the shared hardware-context pool statics.
// -------------------------------------------------------------------------------------
class EaXmaDec : public Decoder
{
public:
    // ----- shared hardware-context pool types ------------------------------------------

    // Intrusive free/in-use list node, embedded in each ContextRecord at X360 +0x14. The
    // pool lists link NODES (the asm stores the +0x14 address); the owning record is
    // recovered by subtracting the node's host offset (see the .cpp).
    struct PoolNode
    {
        PoolNode *mpNext; // +0x00 (X360 record +0x14)
        PoolNode *mpPrev; // +0x04 (X360 record +0x18)
    };

    // One shared hardware-context record (X360 28 bytes / 0x1C stride; the host stride is
    // sizeof(ContextRecord) -- pointers widen). Built by AllocateResources.
    struct ContextRecord
    {
        XmaContext *mpContext;  // +0x00  from XMACreateContext
        u8 *mpInputBuffer0;     // +0x04  2048-byte encoded-input packet buffer 0
        u8 *mpInputBuffer1;     // +0x08  2048-byte encoded-input packet buffer 1
        u8 *mpOutputBuffer;     // +0x0C  6144-byte decoded s16 PCM output ring
        u8 *mpOverlapBuffer;    // +0x10  512-byte decoder overlap/scratch buffer
        PoolNode mNode;         // +0x14  free/in-use list linkage
    };

    // Per-instance stereo-pair decode state (X360 28 bytes / 0x1C stride), laid out in the
    // aligned tail area past the fixed header (see the LAYOUT NOTE above).
    struct DecPair
    {
        ContextRecord *mpRecord;    // +0x00  the claimed pool record (null = claim failed)
        const u8 *mpInputCursor;    // +0x04  next encoded byte to feed (StartPair/Service)
        s32 miInputBytesRemaining;  // +0x08  encoded bytes still to feed this request
        u32 muRingReadOffset;       // +0x0C  software read cursor into the output ring, bytes
        u32 muSamplesQueued;        // +0x10  frames queued to skip/consume before output
        s32 miPendingBitOffset;     // +0x14  first-frame bit offset to program on next feed
        u8 mucChannels;             // +0x18  channels decoded by this pair (1 or 2)
        bool mbNextInputIsBuffer1;  // +0x19  which input buffer the next feed fills
        u8 mPad1A[2];               // +0x1A  (untouched)
    };

    // ----- DecoderDesc factory callbacks (off_82F89394 slots) --------------------------

    // @0x82B93B88 -- desc registration accessor: one-shot AllocateResources(), then return
    // the address of the static registration descriptor off_82F89394.
    static DecoderDesc *GetDecoderDesc();

    // @0x82B93C78 -- report the codec's per-instance allocation footprint: writes the
    // required alignment (16) through *puAlignment and returns the instance size -- the
    // fixed header plus one pair record per stereo pair, ceil(channels/2), laid at the
    // first 8-aligned address past the header. HOST sizes (see the LAYOUT NOTE; the
    // console constants were 0x58 + 0x1C per pair). The descriptor framework passes the
    // channel count in the first register (not an instance pointer).
    static u32 GetSize(u32 uNumChannels, u32 *puAlignment);

    // @0x82B93C98 -- desc create-instance callback over the raw GetSize() allocation (the
    // framework has already seeded the Decoder base fields, e.g. mucChannelCount).
    // Installs the EaXmaDec identity (vptr; the asm's `stw off_8215A928, 0(r3)`), lays the
    // pair table into the aligned tail, zeroes it, then claims one shared pool record per
    // pair from the free list (under the pool mutex), assigning 2 channels to each full
    // pair and 1 to a trailing mono pair. On any claim failure every claimed record is
    // returned to the free list and false is returned; true on success.
    static bool CreateInstanceEvent(EaXmaDec *pSelf);

    // @0x82B8F7C0 -- desc release-instance callback: under the pool mutex, unlink every
    // pair's record from the in-use list and append it to the free-list tail.
    void ReleaseEvent();

    // @0x82B96380 -- desc decode callback: produce up to iNumSamples planar f32 samples
    // per channel into pOutput, draining the hardware output rings via CopySamplesOut,
    // pumping Service, re-arming the hardware read offsets, and entering recovery mode
    // (zero-filling the output) when the hardware stalls. Returns samples produced.
    s32 DecodeEvent(DecoderBuffer *pOutput, s32 iNumSamples);

    // ----- virtuals, in X360 vtable-slot order (off_8215A928) --------------------------

    // @0x82B96140 -- decoder "feed more input" callback (slot 0, overriding the Decoder
    // base no-op); a bare tail-call onto the internal Service pump (`b Service`).
    virtual void FeedEvent();

    // @0x82B93ED0 (`vector deleting destructor', slot 1) -- the thunk reinstalls the
    // Decoder base vtable (off_8214B1CC, via the inlined trivial ~Decoder) and
    // conditionally frees. Per the Decoder-family convention (Decoder.cpp,
    // HELPER_CMpegBase.cpp) the thunk is compiler-generated from this (empty) virtual
    // destructor; the pool records are NOT returned here -- that is ReleaseEvent's job.
    virtual ~EaXmaDec();

private:
    // ----- internal pump / hardware management -----------------------------------------

    // @0x82B8F580 -- one-shot (guarded by sbResourcesAllocated) construction of the shared
    // pool: physically allocate the input/overlap and output-ring memory, allocate +
    // placement-construct the pool mutex and the record table through the System
    // allocator, XMACreateContext each record, carve its buffers, and thread every record
    // onto the free list.
    static void AllocateResources();

public:
    // @0x82B93BB0 -- tear the shared pool down again (guarded by the same flag):
    // XMAReleaseContext every record, destroy + free the mutex/record allocation, free
    // both physical ranges, and null the statics. Declared public because
    // rw::audio::core::System::Release calls it during system teardown. NOTE: this
    // function has no ledger identity (exporter gap -- see
    // scratchpad/waveE/AudioSystem.spec.md); its asm lives in that spec and in
    // scratchpad/waveG/eaxma_dump.txt.
    static void DeallocateResources();

private:
    // @0x82B95EB8 -- input pump: for every pair, while encoded bytes remain and a hardware
    // input buffer is free, XMAmemcpy the next <=2048-byte chunk into the idle buffer,
    // mark it valid, program a pending first-frame bit offset, and re-enable the context;
    // when all pending input is consumed, pull the next request (Decoder ring), Reset the
    // hardware on the very first request, and StartPair every pair from the request data.
    void Service();

    // @0x82B8F8D0 -- (re)initialise every pair's hardware context: build an XmaContextInit
    // from the record's buffers + the pair's channel count + uStreamMode (stored to
    // muStreamMode), then XMAInitializeContext / re-arm buffers / XMAEnableContext.
    void Reset(u32 uStreamMode);

    // @0x82B96148 -- hardware-stall recovery: set mbRecoveryMode, Reset(muStreamMode),
    // clear every pair's streaming state, advance the request read cursor past the decode
    // cursor's slot, zero the pending-input count, then Service.
    void EnterRecoveryMode();

    // @0x82B95A70 -- position one pair at a request's start sample: walk the request's
    // packed seek table (Unpack0::UnpackUInt32) whole packets at a time, then
    // ParseToNextFrame frame-by-frame to the start frame; writes the pair's input cursor,
    // remaining-byte count and first-frame bit offset. Returns the byte size of this
    // pair's slice of the request data (its leading word >> 2), by which the caller
    // advances to the next pair's slice.
    static s32 StartPair(DecPair *pPair, u32 uPairIndex, const DecoderRequest *pRequest,
                         const u8 *pPairData);

    // @0x82B8FC18 -- convert decoded s16 PCM out of a hardware output ring into planar
    // f32 (scale 1/32768), de-interleaving stereo pairs into pOutLeft/pOutRight (mono
    // ignores pOutRight); two segments handle the ring wrap: iFramesFirst frames from
    // pRing + (iRingByteOffset & ~1), then iFramesSecond frames from the ring base. The
    // X360 body is hand-written VMX (lvlx/lvrx + vperm de-interleave + vupk[hl]sh +
    // vcfsx 15); its own scalar tail loops attest the identical per-sample math. A
    // member, but the asm never reads `this` (r3 is overwritten with a constant).
    void CopySamplesOut(f32 *pOutLeft, f32 *pOutRight, const s16 *pRing,
                        s32 iRingByteOffset, s32 iNumChannels, s32 iFramesFirst,
                        s32 iFramesSecond);

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

    // ----- appended per-instance state (X360 offsets in comments) ----------------------

    DecPair *mpPairs;                 // +0x34  pair table in the aligned instance tail
    DecoderRequest *mpCurrentRequest; // +0x38  request currently being decoded
    s32 miPendingInputBytes;          // +0x3C  encoded bytes queued across all pairs
    u8 mPad40[4];                     // +0x40  (untouched by every reconstructed body)
    u32 muPairCount;                  // +0x44  ceil(mucChannelCount / 2)
    s32 miSamplesRemaining;           // +0x48  samples left in the current request
    s32 miOutputSampleBias;           // +0x4C  frame-alignment sample bias carried into
                                      //        the next request (512-sample frame pad)
    u32 muStreamMode;                 // +0x50  low 2 bits of the stream's leading word,
                                      //        stored by Reset, replayed by recovery
    bool mbRecoveryMode;              // +0x54  zero-fill output instead of decoding
    bool mbFirstDecode;               // +0x55  set by CreateInstanceEvent, cleared by the
                                      //        first Service request (gates Reset)

    // ----- the shared hardware-context pool (file-scope statics on the X360; static
    // members here so every part file of this TU reaches the same storage). X360
    // addresses in comments; values recovered in scratchpad/waveG/eaxma_dump.txt. -------

    static bool sbResourcesAllocated;      // byte_8327A2FE  one-shot alloc/dealloc guard
    static u32 suNumContexts;              // dword_82F89388 (.data) = 256 pool contexts
    static u32 suRecoveryTimeoutMs;        // dword_82F89390 (.data) = 4 ms stall timeout
    static ContextRecord *spRecords;       // off_8327A304   the pool record table
    static u8 *spInputMemory;              // dword_8327A308 physical input+overlap range
    static u8 *spOutputMemory;             // dword_8327A30C physical output-ring range
    static EA::Thread::Mutex *spPoolMutex; // dword_8327A310 guards the pool lists
    static PoolNode *spInUseHead;          // off_8327A314   in-use list head
    static PoolNode *spFreeHead;           // off_8327A318   free list head
    static PoolNode *spFreeTail;           // off_8327A31C   free list tail
    static u32 suFreeCount;                // dword_8327A320 free-list length
};

} // namespace core
} // namespace audio
} // namespace rw
