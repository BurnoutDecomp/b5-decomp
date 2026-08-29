// =====================================================================================
// rw::audio::core::EaXmaDec -- input-streaming bodies (part 4 of the TU).
//
//   Service   @0x82B95EB8   (this file)
//   StartPair @0x82B95A70   (this file)
//   Reset     @0x82B8F8D0   (this file)
//
// The pool side of the codec plus the leaf helpers live in EaXmaDec.cpp; the instance
// lifecycle lives in EaXmaDec_wG_02.cpp and the decode pump in EaXmaDec_wG_03.cpp. The
// class, its nested records and the shared pool statics are declared/defined there --
// see rw/audio/core/EaXmaDec.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width, branch and side-effect. No Feb-2007 source and no DecFIGS DWARF exist
// for this TU. The `_savegprlr_* / _restgprlr_*` calls in the pseudocode are the
// compiler's register save/restore helpers, not source-level calls, so they are dropped.
//
// THESE THREE BODIES TOGETHER ARE THE INPUT SIDE of the codec: Service tops up the two
// 2048-byte hardware input buffers of every stereo pair and, when all queued input has
// been handed over, pulls the next Decoder request and re-seeds every pair from it via
// StartPair; Reset (re)programs the XMA hardware contexts from the pool records.
//
// ENDIANNESS. Everything Service/StartPair read out of the request payload -- the leading
// slice-size word, the XMA packet headers and the seek-table entries -- is SERIALIZED
// big-endian resource data, so it is assembled byte-wise through ReadU32BE below (the
// console's plain `lwz` was already big-endian; StartPair's own asm spells the packet
// header out as four `lbz`/`stb` pairs). Nothing here reads decoded PCM.
//
// HOST-vs-CONSOLE SIZES. The pair table is indexed as mpPairs[i] and the hardware init
// record is zeroed with sizeof(): the console's 0x1C pair stride and 0x38 init-record
// size are 32-bit layout words that do not survive the x64 widening; they are kept only
// in the comments. Every constant that DOES survive is a stream/hardware format value
// (the 2048-byte XMA packet, the 512-sample XMA frame, the 0x3FE0-bit packet payload,
// the 4-byte serialized words), noted at each site.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include "rw/audio/core/Unpack0.h"  // Unpack0::UnpackUInt32 (packed seek-table varints)
#include "SDKs/XAudio/XmaHardware.h" // XmaContext / XmaContextInit + the XMA* HAL

#include <cstring> // memset (the console's stack-record zero fill)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{

// -------------------------------------------------------------------------------------
// Read one 32-bit big-endian word out of a serialized XMA byte stream (request payload,
// packet header or seek table). On the X360 these were plain word loads (and, for the
// packet header, four byte loads reassembled in address order -- see StartPair's asm at
// 0x82B95AB4..0x82B95AD8); on the little-endian host the bytes must be combined
// explicitly. Same convention as the committed EaXmaDec::LeftShiftInBits.
// -------------------------------------------------------------------------------------
inline u32 ReadU32BE(const u8 *pBytes)
{
    return (static_cast<u32>(pBytes[0]) << 24) | (static_cast<u32>(pBytes[1]) << 16) |
           (static_cast<u32>(pBytes[2]) << 8) | static_cast<u32>(pBytes[3]);
}

} // namespace

// -------------------------------------------------------------------------------------
// Service @0x82B95EB8
// The codec's input pump (also the body behind the FeedEvent vtable slot).
//
// Each turn of the outer loop first makes a feed pass over every stereo pair: if BOTH of
// a pair's hardware input buffers still hold undecoded packets there is no room, so the
// context is merely (re-)enabled; otherwise the pair's remaining encoded bytes are handed
// over <=2048 bytes at a time, alternating between input buffer 0 and 1, until either the
// pair runs dry or the buffer that is next in line is still marked valid. A first-frame
// bit offset left behind by StartPair is programmed into the hardware with the feed that
// carries its packet.
//
// When the feed pass has drained everything that was queued (miPendingInputBytes == 0)
// the pump takes the next request off the Decoder ring, resets the hardware from the
// stream's mode bits on the very first request, and re-seeds every pair from the request
// payload via StartPair -- then loops back to feed the freshly queued bytes. It returns
// when input is still pending or the request ring is empty.
// -------------------------------------------------------------------------------------
void EaXmaDec::Service()
{
    for (;;)
    {
        // ---- feed pass over every stereo pair -----------------------------------------
        // (the console re-reads muPairCount each turn; the pair table is walked by the
        // console 0x1C stride, expressed here as the host mpPairs[] index.)
        for (u32 uPair = 0; uPair < muPairCount; ++uPair)
        {
            DecPair &lPair = mpPairs[uPair];

            // No null check on mpRecord anywhere in this pump -- faithful to the asm.
            if (XMAIsInputBuffer0Valid(lPair.mpRecord->mpContext) &&
                XMAIsInputBuffer1Valid(lPair.mpRecord->mpContext))
            {
                // Both packet buffers are still loaded: nothing to feed, just keep the
                // hardware context running.
                XMAEnableContext(lPair.mpRecord->mpContext);
            }
            else
            {
                while (lPair.miInputBytesRemaining)
                {
                    XmaContext *lpContext = lPair.mpRecord->mpContext;

                    if (lPair.mbNextInputIsBuffer1)
                    {
                        // The buffer next in line has not been consumed yet: stop feeding
                        // this pair (the console `bne loc_82B95F1C` leaves the while).
                        if (XMAIsInputBuffer1Valid(lpContext))
                            break;

                        // 2048 = one XMA packet, the hardware input-buffer size. The
                        // console compares UNSIGNED (`cmplwi 0x800`).
                        u32 uChunk = static_cast<u32>(lPair.miInputBytesRemaining);
                        if (uChunk >= 2048u)
                            uChunk = 2048u;

                        XMAmemcpy(lPair.mpRecord->mpInputBuffer1, lPair.mpInputCursor,
                                  static_cast<s32>(uChunk));
                        XMADisableContext(lPair.mpRecord->mpContext, 1);
                        XMASetInputBuffer1Valid(lPair.mpRecord->mpContext);

                        // A pending first-frame bit offset belongs to the packet that has
                        // just been loaded; +32 skips the packet's own 4-byte header.
                        if (lPair.miPendingBitOffset > 0)
                        {
                            XMASetInputBufferReadOffset(
                                lPair.mpRecord->mpContext,
                                static_cast<u32>(lPair.miPendingBitOffset + 32));
                            lPair.miPendingBitOffset = 0;
                        }

                        XMAEnableContext(lPair.mpRecord->mpContext);

                        lPair.miInputBytesRemaining -= static_cast<s32>(uChunk);
                        miPendingInputBytes -= static_cast<s32>(uChunk);
                        lPair.mbNextInputIsBuffer1 = false;
                        lPair.mpInputCursor += uChunk;
                    }
                    else
                    {
                        if (XMAIsInputBuffer0Valid(lpContext))
                            break;

                        u32 uChunk = static_cast<u32>(lPair.miInputBytesRemaining);
                        if (uChunk >= 2048u)
                            uChunk = 2048u;

                        XMAmemcpy(lPair.mpRecord->mpInputBuffer0, lPair.mpInputCursor,
                                  static_cast<s32>(uChunk));
                        XMADisableContext(lPair.mpRecord->mpContext, 1);
                        XMASetInputBuffer0Valid(lPair.mpRecord->mpContext);

                        if (lPair.miPendingBitOffset > 0)
                        {
                            XMASetInputBufferReadOffset(
                                lPair.mpRecord->mpContext,
                                static_cast<u32>(lPair.miPendingBitOffset + 32));
                            lPair.miPendingBitOffset = 0;
                        }

                        XMAEnableContext(lPair.mpRecord->mpContext);

                        lPair.miInputBytesRemaining -= static_cast<s32>(uChunk);
                        miPendingInputBytes -= static_cast<s32>(uChunk);
                        lPair.mbNextInputIsBuffer1 = true;
                        lPair.mpInputCursor += uChunk;
                    }
                }
            }
        }

        // ---- pull the next request once everything queued has been handed over --------
        if (miPendingInputBytes)
            break;

        DecoderRequest *lpRequest = GetCurrentRequestDesc();
        if (!lpRequest)
            break;

        // Request+0x00 holds the pointer to this request's encoded XMA payload (the ring
        // entry used to be pinned to 20 bytes, which forced the pointer through a 32-bit word;
        // that wrinkle is retired -- the field is a real host pointer now, named by Decoder::Feed).
        const u8 *lpData =
            static_cast<const u8 *>(lpRequest->mpFedData);

        if (mbFirstDecode)
        {
            // The stream's leading serialized word carries the XMA stream mode in its low
            // two bits; the hardware is programmed with it once, on the first request.
            Reset(ReadU32BE(lpData) & 3u);
            mbFirstDecode = false;
        }

        // Each pair owns a consecutive slice of the payload; StartPair returns the slice's
        // byte size, by which the cursor advances to the next pair's slice.
        for (u32 uPair = 0; uPair < muPairCount; ++uPair)
        {
            lpData += StartPair(&mpPairs[uPair], uPair, lpRequest, lpData);
            miPendingInputBytes += mpPairs[uPair].miInputBytesRemaining;
        }
    }
}

// -------------------------------------------------------------------------------------
// StartPair @0x82B95A70
// Position one stereo pair at a request's start sample inside that pair's slice of the
// encoded payload, and report the slice's byte size.
//
// The slice begins with a serialized word whose top 30 bits are its byte size; the packed
// XMA data follows. The start sample is converted to an XMA frame index (512 samples per
// frame; requests that do not carry the "continuation" flag are padded by 384 samples of
// decoder priming). If the request supplies a seek table, this pair's entry is a packed
// run of per-packet frame counts (Unpack0::UnpackUInt32 varints) which is walked whole
// 2048-byte packets at a time until the target frame lies inside the current packet; the
// remainder is then walked frame by frame with ParseToNextFrame.
//
// The result is written back as the pair's input cursor, its first-frame bit offset (to
// be programmed by Service with the feed that carries this packet) and the number of
// encoded bytes still to feed -- the slice size less whatever the seek/parse walk skipped.
// -------------------------------------------------------------------------------------
s32 EaXmaDec::StartPair(DecPair *pPair, u32 uPairIndex, const DecoderRequest *pRequest,
                        const u8 *pPairData)
{
    // Leading serialized word (big-endian): this pair's slice size in bytes, in the top
    // 30 bits.
    const u32 uSliceBytes = ReadU32BE(pPairData) >> 2;

    // Target XMA frame within the stream. Signed division by the 512-sample frame (the
    // asm's `srawi 9` + `addze` is the compiler's round-toward-zero idiom, written here
    // as the plain division it came from). The flag word at Request+0x10 is read as a
    // BYTE by the console (`lbz 0x10`), i.e. the field is byte-sized in the source and
    // the modelled u32 only ever holds a 0/1 flag -- so the test is a plain truthiness
    // test (same reading as the committed Pcm16BigDec::DecodeEvent).
    s32 iTargetFrame = pRequest->miStartSample / 512;
    if (!pRequest->mucContinue)
        iTargetFrame = (pRequest->miStartSample + 384) / 512;

    const u8 *const lpSliceBase = pPairData + 4; // past the 4-byte serialized size word
    const u8 *lpCursor = lpSliceBase;

    // First-frame bit offset out of the XMA packet header: bits 11..25 of the packet's
    // leading big-endian word. 0x3FE0 is the packet's payload size in bits, so a header
    // pointing at or past it means "no frame starts in this packet" -> -1.
    s32 iBitOffset = static_cast<s32>((ReadU32BE(lpCursor) >> 11) & 0x7FFFu);
    if (iBitOffset >= 0x3FE0)
        iBitOffset = -1;

    if (pRequest->mpSeekData)
    {
        // Request+0x04 holds the seek table's base pointer, whose first words are per-pair
        // byte offsets into the packed frame-count runs.
        // ⭐ 2026-08-29: this used to read `muReserved04`, a u32, and launder it back with
        // `static_cast<usize>`. The comment here already knew it was a pointer ("again in a
        // 32-bit ring word") -- but on x64 the store side truncated it, so this base was
        // only ever going to be correct by accident. The field is a real `const u8 *` now
        // and the cast is gone; see the mpSeekData note in Decoder.h for the full chain.
        const u8 *const lpSeekBase = pRequest->mpSeekData;
        const u8 *lpPacked = lpSeekBase + ReadU32BE(lpSeekBase + 4 * uPairIndex);

        // Skip whole 2048-byte packets while the target frame lies beyond the running
        // frame total. The comparisons are SIGNED (`cmpw`).
        s32 iFramesSkipped = 0;
        u32 uFramesInPacket = 0;
        lpPacked += Unpack0::UnpackUInt32(lpPacked, &uFramesInPacket);
        if (iTargetFrame >= static_cast<s32>(uFramesInPacket))
        {
            do
            {
                iFramesSkipped += static_cast<s32>(uFramesInPacket);
                lpCursor += 2048;
                lpPacked += Unpack0::UnpackUInt32(lpPacked, &uFramesInPacket);
            }
            while (iTargetFrame >= static_cast<s32>(uFramesInPacket) + iFramesSkipped);
        }
        iTargetFrame -= iFramesSkipped;

        // Re-read the packet header at the (possibly advanced) cursor. The console does
        // this unconditionally inside the seek-table branch -- even when no packet was
        // skipped, in which case it recomputes the same value -- so it is kept as-is.
        iBitOffset = static_cast<s32>((ReadU32BE(lpCursor) >> 11) & 0x7FFFu);
        if (iBitOffset >= 0x3FE0)
            iBitOffset = -1;
    }

    // Walk the rest of the way frame by frame.
    while (iTargetFrame > 0)
    {
        ParseToNextFrame(&lpCursor, &iBitOffset);
        --iTargetFrame;
    }

    pPair->mpInputCursor = lpCursor;
    pPair->miPendingBitOffset = iBitOffset;
    // Bytes still to feed: the slice, less the 4-byte size word, less whatever the walk
    // skipped (lpSliceBase - lpCursor is <= 0).
    pPair->miInputBytesRemaining = static_cast<s32>(lpSliceBase - lpCursor) +
                                   static_cast<s32>(uSliceBytes) - 4;
    return static_cast<s32>(uSliceBytes);
}

// -------------------------------------------------------------------------------------
// Reset @0x82B8F8D0
// (Re)initialise every pair's XMA hardware context from its pool record: build the init
// record on the stack, stop the context, initialise it, arm the output ring and both
// encoded-input packet buffers, and start it again. The stream mode is remembered in
// muStreamMode so EnterRecoveryMode can replay this without the stream header.
//
// The console zeroes 0x38 stack bytes (seven doubleword stores) before filling the record
// in; on the host the record's pointer fields widen, so the fill is sizeof()-driven.
// -------------------------------------------------------------------------------------
void EaXmaDec::Reset(u32 uStreamMode)
{
    muStreamMode = uStreamMode;

    for (u32 uPair = 0; uPair < muPairCount; ++uPair)
    {
        DecPair &lPair = mpPairs[uPair];
        ContextRecord *lpRecord = lPair.mpRecord;

        XmaContextInit lInit;
        std::memset(&lInit, 0, sizeof(lInit)); // console: 0x38 bytes

        lInit.mpInputBuffer0 = lpRecord->mpInputBuffer0;
        lInit.muInputBuffer0Packets = 1;
        lInit.mpInputBuffer1 = lpRecord->mpInputBuffer1;
        lInit.muInputBuffer1Packets = 1;
        lInit.muUnknown10 = 0x20;
        lInit.mpOutputBuffer = lpRecord->mpOutputBuffer;
        lInit.muOutputBufferBlocks = 0x18; // the 6144-byte ring in 256-byte blocks
        lInit.mpOverlapBuffer = lpRecord->mpOverlapBuffer;
        lInit.muUnknown20 = 4;
        lInit.muChannelsMinusOne = static_cast<u32>(lPair.mucChannels) - 1;
        lInit.muStreamMode = uStreamMode;

        XMADisableContext(lpRecord->mpContext, 1);
        XMAInitializeContext(lpRecord->mpContext, &lInit);
        XMASetOutputBufferValid(lpRecord->mpContext);
        XMASetInputBuffer0(lpRecord->mpContext, lpRecord->mpInputBuffer0, 1);
        XMASetInputBuffer1(lpRecord->mpContext, lpRecord->mpInputBuffer1, 1);
        XMAEnableContext(lpRecord->mpContext);
    }
}

} // namespace core
} // namespace audio
} // namespace rw
