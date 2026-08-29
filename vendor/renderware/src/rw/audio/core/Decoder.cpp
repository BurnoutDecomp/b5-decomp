// =====================================================================================
// rw::audio::core::Decoder bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, width and side-effect. No Feb-2007 source and no
// DecFIGS DWARF exist for this TU. See Decoder.h for the byte layout of Decoder and its
// inline Request-ring / source-Buffer descriptors.
//   AdvanceDecodeState    @0x82B679D8
//   Decode                @0x82B67A50
//   GetCurrentRequestDesc @0x82B92050
//   GetSamplesRemaining   @0x826914D0
//   Release               @0x82691528
//   ~Decoder / 'vector deleting destructor' @0x82B678D8 (compiler thunk; see below)
//
// The `_savegprlr_24` / `_restgprlr_24` calls in Decode's pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped. XMemCpy is the Xbox platform block copy (declared here, defined in the
// platform TU). off_83271928 is the shared rwaudio System singleton, freed through
// System::Free (the same singleton Collection/PlugIn reach).
// =====================================================================================

#include "rw/audio/core/Decoder.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (Free)

#include <cstddef> // offsetof

namespace rw
{
namespace audio
{
namespace core
{

// Xbox platform interleaved block copy (declaration only for the per-TU compile gate).
extern "C" void *XMemCpy(void *pDest, const void *pSrc, unsigned int uiCount);

// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System allocator TU; here it is only the target for System::Free. Same global reached
// by Collection.cpp / PlugIn.cpp.
extern "C" System *off_83271928;

// --- layout pins for the inline request ring ---
//
// ⚠️ REVISED 2026-08-28 (phase E). These used to pin the CONSOLE stride: sizeof == 20 and
// miStartSample/miEndSample at the absolute console offsets +0x08/+0x0C. That held only
// while the record was believed pointer-free. Raw-decoding Decoder::Feed @0x82B67920 (an
// exporter gap) showed the console stores a POINTER at +0x00 -- the chunk it was handed --
// so on x64 the record legitimately grows and those absolute offsets legitimately move.
// The old asserts fired, which is exactly what they were for: they caught the widening
// instead of letting it silently shift the two sample fields.
//
// What is actually invariant is the ORDER and the self-consistency of the ring, so that is
// what is pinned now. Every committed consumer reaches the ring through RequestQueue()[i],
// i.e. host array indexing, so they all stay in step automatically.
//
// ⚠️ THE ONE THING THAT MUST MATCH THIS: whoever ALLOCATES the ring must size it with
// sizeof(DecoderRequest), never the console's 0x14. That is DecoderRegistry::DecoderFactory
// @0x82B6C778 (`mulli r21, r26, 0x14`), which is itself an exporter gap and is not bodied
// on the host yet -- when it lands, it must use the host sizeof or the ring and its
// consumers will disagree.
static_assert(offsetof(DecoderRequest, miStartSample) < offsetof(DecoderRequest, miEndSample),
              "DecoderRequest: the sample range must stay in start-then-end order");
static_assert(offsetof(DecoderRequest, mpFedData) == 0,
              "DecoderRequest: the fed-chunk pointer is the leading word (Feed: stw r4, 0)");
static_assert(offsetof(DecoderRequest, mpSeekData) == sizeof(void *),
              "DecoderRequest: the seek-table base is the SECOND word (Feed: stw r8, 4)");
static_assert(sizeof(DecoderRequest) >= 20,
              "DecoderRequest must still cover every console field it carries");
// ⭐ 2026-08-29: the record carries TWO pointers, not one. `mpSeekData` was `u32
// muReserved04`; on x64 that silently truncated the seek-table base every consumer needs.
// The pin above exists so a future "tidy-up" that reorders or re-narrows either pointer
// fails the build instead of corrupting the ring.
static_assert(sizeof(DecoderRequest) >= 2 * sizeof(void *) + 3 * sizeof(u32),
              "DecoderRequest: two pointers plus the sample range and flags must all fit");

// -------------------------------------------------------------------------------------
// ~Decoder @0x82B678D8
// The X360 'vector deleting destructor' thunk simply reinstalls the Decoder vtable and,
// when the deleting flag is set, calls operator delete. That whole thunk is compiler-
// generated from this virtual destructor (per the project convention of dropping
// deleting-destructor thunks), so only the (empty) source destructor is written here.
// -------------------------------------------------------------------------------------
Decoder::~Decoder()
{
}

// -------------------------------------------------------------------------------------
// AdvanceDecodeState @0x82B679D8
// Advance the decode cursor by `iCount`. When it reaches the end of the current request
// slot, clear that slot, step the ring cursor (wrapping at mucRequestCount), and reseed
// the cursor from the next slot's start sample.
// -------------------------------------------------------------------------------------
void Decoder::AdvanceDecodeState(s32 iCount)
{
    DecoderRequest *pQueue = RequestQueue();

    miCurrentSampleOffset += iCount;

    DecoderRequest *pCurrent = &pQueue[mucRequestDecodeIndex];
    if (miCurrentSampleOffset == pCurrent->miEndSample)
    {
        pCurrent->miEndSample = 0;

        u8 ucNext = static_cast<u8>(mucRequestDecodeIndex + 1);
        mucRequestDecodeIndex = ucNext;
        if (ucNext >= mucRequestCount)
            mucRequestDecodeIndex = 0;

        miCurrentSampleOffset = pQueue[mucRequestDecodeIndex].miStartSample;
    }
}

// -------------------------------------------------------------------------------------
// Decode @0x82B67A50
// Produce up to `iNumSamples` interleaved samples into `pOutput`, returning the count
// actually produced.
//
// Two paths, selected by mucUsesSourceBuffer:
//  * source-buffer path -- the codec decodes into the decoder's inline scratch Buffer,
//    then this method scatters/interleaves the staged samples into the caller's output,
//    first draining any samples carried from the previous call, then decoding fresh
//    chunks a request at a time; and
//  * direct path -- the codec decodes straight into the caller's output buffer, one
//    request chunk at a time, until the request is exhausted or the output is full.
// -------------------------------------------------------------------------------------
s32 Decoder::Decode(DecoderBuffer *pOutput, s32 iNumSamples)
{
    s32 iProduced = 0;

    if (mucUsesSourceBuffer)
    {
        DecoderBuffer *pSource = SourceBuffer();

        // First, drain whatever the previous decode left staged in the source buffer.
        bool bRunTail = (muCarrySamples != 0);
        s32 iLastCount = 0;
        if (bRunTail)
        {
            iProduced = muCarrySamples;
            if (iProduced >= iNumSamples)
                iProduced = iNumSamples;

            for (u32 uc = 0; uc < mucChannelCount; ++uc)
            {
                s32 iSrcIndex = static_cast<s32>(pSource->muStride * uc)
                                - static_cast<s32>(muCarrySamples)
                                + static_cast<s32>(pSource->muSampleCursor);
                XMemCpy(pOutput->mpData + pOutput->muStride * uc,
                        pSource->mpData + iSrcIndex,
                        4 * iProduced);
            }
            iLastCount = iProduced;
        }

        for (;;)
        {
            if (bRunTail)
            {
                muCarrySamples = static_cast<u16>(muCarrySamples - iLastCount);
                AdvanceDecodeState(iLastCount);
            }
            bRunTail = true;

            if (iProduced >= iNumSamples)
                break;

            DecoderRequest *pCurrent = &RequestQueue()[mucRequestDecodeIndex];
            if (pCurrent->miEndSample == 0)
                break;

            s32 iDecoded = mpDecodeCallback(this, pSource, iNumSamples - iProduced);

            s32 iStaged = pCurrent->miEndSample - miCurrentSampleOffset;
            if (iDecoded < iStaged)
                iStaged = static_cast<s16>(iDecoded); // LOWORD(v13) = v12

            muCarrySamples = static_cast<u16>(iStaged);
            pSource->muSampleCursor = static_cast<u16>(iStaged);

            s32 iCopy = muCarrySamples;
            if (iCopy >= iNumSamples - iProduced)
                iCopy = iNumSamples - iProduced;

            for (u32 uc = 0; uc < mucChannelCount; ++uc)
            {
                XMemCpy(pOutput->mpData + (pOutput->muStride * uc + iProduced),
                        pSource->mpData + pSource->muStride * uc,
                        4 * iCopy);
            }

            iLastCount = iCopy;
            iProduced += iCopy;
        }
    }
    else if (iNumSamples > 0)
    {
        do
        {
            DecoderRequest *pCurrent = &RequestQueue()[mucRequestDecodeIndex];
            s32 iEnd = pCurrent->miEndSample;
            if (iEnd == 0)
                break;

            s32 iChunk = iEnd - miCurrentSampleOffset;
            if (iNumSamples - iProduced < iChunk)
                iChunk = iNumSamples - iProduced;

            mpDecodeCallback(this, pOutput, iChunk);

            iProduced += iChunk;
            AdvanceDecodeState(iChunk);
        } while (iProduced < iNumSamples);
    }

    return iProduced;
}

// -------------------------------------------------------------------------------------
// GetCurrentRequestDesc @0x82B92050
// Return the request at the read cursor. If that slot is empty, return null (leaving the
// cursor put); otherwise advance the read cursor (wrapping at mucRequestCount) and return
// the (original) request.
// -------------------------------------------------------------------------------------
DecoderRequest *Decoder::GetCurrentRequestDesc()
{
    DecoderRequest *pRequest = &RequestQueue()[mucRequestReadIndex];
    if (pRequest->miEndSample == 0)
        return 0;

    u8 ucNext = static_cast<u8>(mucRequestReadIndex + 1);
    mucRequestReadIndex = ucNext;
    if (ucNext >= mucRequestCount)
        mucRequestReadIndex = 0;

    return pRequest;
}

// -------------------------------------------------------------------------------------
// GetSamplesRemaining @0x826914D0
// Samples still owed by request `ucIndex`. Empty slot -> 0. For the slot currently being
// decoded the live decode cursor is used as the start; otherwise the slot's own start.
// -------------------------------------------------------------------------------------
s32 Decoder::GetSamplesRemaining(u8 ucIndex)
{
    DecoderRequest *pRequest = &RequestQueue()[ucIndex];

    s32 iEnd = pRequest->miEndSample;
    if (iEnd == 0)
        return 0;

    s32 iStart;
    if (ucIndex == mucRequestDecodeIndex)
        iStart = miCurrentSampleOffset;
    else
        iStart = pRequest->miStartSample;

    return iEnd - iStart;
}

// -------------------------------------------------------------------------------------
// Release @0x82691528
// Run the codec teardown callback (if any), free the codec's owned block (if any), then
// free the Decoder instance itself -- all through the shared System allocator.
// -------------------------------------------------------------------------------------
void Decoder::Release()
{
    if (mpFinaliser)
        mpFinaliser(this);

    if (mpAllocatedBlock)
        System::Free(off_83271928, mpAllocatedBlock, 0);

    System::Free(off_83271928, this, 0);
}

// -------------------------------------------------------------------------------------
// Feed @0x82B67920 -- hand the decoder one more chunk.
//
// EXPORTER GAP: this address has no dossier, so the body below was raw-decoded from the
// decrypted XEX (file_off 0x00B6A920) and hand-disassembled instruction by instruction.
// It is the PRODUCER half of the request ring whose consumer side (Decode /
// AdvanceDecodeState / GetCurrentRequestDesc / GetSamplesRemaining) was already
// reconstructed, and both SndPlayer1 families call it from their SubmitChunk.
//
// Fill the slot at the write cursor, tell the codec about it through FeedEvent, advance
// the cursor, and return the index of the slot just filled -- that index IS the "decoder
// request handle" the players store in their feed descriptors and later pass to
// GetSamplesRemaining.
// -------------------------------------------------------------------------------------
u8 Decoder::Feed(const void *pData, s32 iNumSamples, u8 ucContinue, s32 iStartSample,
                 const u8 *apSeekData, u8 ucFlag11)
{
    const u8 lucSlot = mucRequestWriteIndex;                  // lbz 0x2F
    DecoderRequest *lpRequest = reinterpret_cast<DecoderRequest *>(
        reinterpret_cast<u8 *>(this) + muRequestQueueOffset
        + sizeof(DecoderRequest) * lucSlot);                  // mulli 0x14 + add

    // A non-zero end sample means the slot still owes the consumer samples.
    // ⚠️ The refusal returns 0, which is indistinguishable from having filled slot 0 --
    // the console has no separate status here. Both callers only reach this after their
    // own GetFeedSlot has confirmed a free slot, so the ambiguity never bites in practice.
    if (lpRequest->miEndSample != 0)
        return 0;

    lpRequest->mpFedData = pData;            // stw r4 -> +0x00
    lpRequest->mpSeekData = apSeekData;      // stw r8 -> +0x04 (the seek-table base)
    lpRequest->miStartSample = iStartSample; // stw r7 -> +0x08
    lpRequest->miEndSample = iNumSamples;    // stw r5 -> +0x0C (also arms the busy flag)
    lpRequest->mucFlag11 = ucFlag11;         // stb r9 -> +0x11
    lpRequest->mucContinue = ucContinue;     // stb r6 -> +0x10

    // vt[0] -- the codec's own notification that a chunk arrived (EaXmaDec tail-calls its
    // Service from here). The console passes the slot index; the committed base override
    // takes none and the subclasses read the cursor themselves.
    FeedEvent();

    // If the decode cursor is sitting on the slot just filled, it has nothing staged yet,
    // so seed the in-chunk offset from this request's start sample.
    if (mucRequestWriteIndex == mucRequestDecodeIndex)
        miCurrentSampleOffset = lpRequest->miStartSample;

    // Post-increment the write cursor, wrapping at the ring modulus.
    u8 lucNext = static_cast<u8>(mucRequestWriteIndex + 1);
    mucRequestWriteIndex = lucNext;
    if (lucNext >= mucRequestCount)
        mucRequestWriteIndex = 0;

    return lucSlot;
}

} // namespace core
} // namespace audio
} // namespace rw
