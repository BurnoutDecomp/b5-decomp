// ===========================================================================
// EATech Apt -- AptAnimationTarget class-static data layer.
//
// A small set of FILE-STATIC tables the Apt input/update layer shares across all
// animation targets (X360 globals dword_8324E534..dword_8324E550), plus the
// static accessors over them. Reconstructed from the X360 ARTIST pseudocode/asm
// (SetupStaticData @0x82AE41F0 builds them, CleanupStaticData @0x82AE42A8 frees
// them) via the decompile->verify workflow.
//
// (The per-player analog-stick tables saAStickLeft/Right @unk_8324D750/unk_8324E2D8
// are a follow-on -- their fixed element bound is not yet determined.)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"

#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager::Allocate/Deallocate

#include <cstring>   // memset

// The shared Apt fixed-size pool (X360 off_8324D808); defined by the Apt pseudo-data
// layer (same handle AptActionQueue / AptCharacterSpriteInstBase allocate from).
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG (deferred input-recorder subsystem): the Apt input recorder the X360 feeds
// every accepted input/analog event so a session can be replayed. dword_8324E518 is
// the "recording enabled" flag, dword_8324E830 the record-sink callback invoked as
// fn(pBigEndianRecord, nBytes), and dword_8324D820 the record-tag word stamped into
// the leading dword. The byte/halfword reversal AddInput performs is the X360's
// big-endian record serialization (reproduced verbatim); the sink itself is a
// not-yet-homed debug subsystem, declared here as opaque externs.
// ---------------------------------------------------------------------------
extern int  gAptInputRecorderEnabled;                    // dword_8324E518
extern int  (*gpAptInputRecorderSink)(void*, int);       // dword_8324E830
extern u32  gAptInputRecorderTag;                         // dword_8324D820

namespace
{
    // X360 globals dword_8324E534..dword_8324E550 (the shared static-data layer).
    int   ssMousePosX             = 0;        // dword_8324E534 -- cursor X fed to AS
    int   ssMousePosY             = 0;        // dword_8324E538 -- cursor Y fed to AS
    void* spStaticBlock           = nullptr;  // dword_8324E53C -- 80-byte scratch block
    int   snMaxNewMovieClips      = 0;        // dword_8324E540 -- table element count
    void* spNewInsts              = nullptr;  // off_8324E544   -- new-instance table (4*count)
    int   snNewInstSize           = 0;        // dword_8324E548 -- new-instance fill counter
    void* spDelayedReleaseList    = nullptr;  // off_8324E54C   -- delayed-release table (4*count)
    int   snDelayedReleaseListSize = 0;       // dword_8324E550 -- delayed-release fill counter
}

// ---- class-static accessors (X360 one-liners over the globals above) -----------
int   AptAnimationTarget::GetXMousePos()             { return ssMousePosX; }              // @0x82AD5F98
int   AptAnimationTarget::GetYMousePos()             { return ssMousePosY; }              // @0x82AD5FA8
int   AptAnimationTarget::GetMaxNewMovieClips()      { return snMaxNewMovieClips; }       // @0x82AD5E90
void* AptAnimationTarget::GetNewInsts()              { return spNewInsts; }               // @0x82AD5EA0
int   AptAnimationTarget::GetNewInstSize()           { return snNewInstSize; }            // @0x82AD5EB0
void* AptAnimationTarget::GetDelayedReleaseList()    { return spDelayedReleaseList; }     // @0x82AD5ED8
int   AptAnimationTarget::GetDelayedReleaseListSize(){ return snDelayedReleaseListSize; } // @0x82AD5EE8

// DecNewInstSize @0x82AD5EC0 -- despite the name, hands out the next new-instance
// index and POST-INCREMENTS the fill counter (returns the pre-increment value).
int   AptAnimationTarget::DecNewInstSize()           { return snNewInstSize++; }

// SetupStaticData @0x82AE41F0 -- allocate the shared static tables, sized to the
// max-new-movie-clips count. (The X360 reads the count from its params struct at
// +0x18; taken here as the explicit count so no opaque param layout is needed.)
void AptAnimationTarget::SetupStaticData(int nMaxNewMovieClips)
{
    spStaticBlock = gpAptPseudoDataPool->Allocate(80);
    memset(spStaticBlock, 0, 80);

    snMaxNewMovieClips   = nMaxNewMovieClips;
    spNewInsts           = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    spDelayedReleaseList = gpAptPseudoDataPool->Allocate(4 * nMaxNewMovieClips);
    memset(spDelayedReleaseList, 0, 4 * nMaxNewMovieClips);

    snNewInstSize            = 0;
    snDelayedReleaseListSize = 0;
}

// CleanupStaticData @0x82AE42A8 -- free the three tables SetupStaticData built.
void AptAnimationTarget::CleanupStaticData()
{
    gpAptPseudoDataPool->Deallocate(spNewInsts, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spDelayedReleaseList, 4 * snMaxNewMovieClips);
    gpAptPseudoDataPool->Deallocate(spStaticBlock, 80);
}

// ===========================================================================
// Per-frame input pump (the queued-input ring at mnQueuedInputsCount/mpQueuedInputs).
// ===========================================================================

// AddInput @0x82AD93B0 -- append one packed input event to the queued-input ring.
// No-op (returns 0) when the ring is at capacity; otherwise stores the event, bumps
// the fill counter, optionally streams it to the input recorder, and returns 1.
int AptAnimationTarget::AddInput(int nPackedInput)
{
    if (mnQueuedInputsCount >= mnQueuedInputsCap)   // ring full (count >= cap) -> reject
        return 0;

    mpQueuedInputs[mnQueuedInputsCount] = (u32)nPackedInput;   // *(4*count + buf) = event
    ++mnQueuedInputsCount;

    if (gAptInputRecorderEnabled)
    {
        // FLAG (deferred recorder): serialise the event big-endian and hand it to the
        // record sink. The X360 builds an 8-byte record { tag, byteReversed(event) }
        // via the inline XOR byte-swap + halfword-swap (net: a full 32-bit byte
        // reversal); reproduced here as that reversal.
        const u32 ev = (u32)nPackedInput;
        const u32 evBE = ((ev & 0x000000FFu) << 24)
                       | ((ev & 0x0000FF00u) << 8)
                       | ((ev & 0x00FF0000u) >> 8)
                       | ((ev & 0xFF000000u) >> 24);
        u32 record[2];
        record[0] = gAptInputRecorderTag;
        record[1] = evBE;
        gpAptInputRecorderSink(record, 8);
    }
    return 1;
}

// ProcessAptInput @0x82B01F78 -- decode one packed input word and run it through the
// input set then the listener set. The word packs: event-id in bits [31..17], a 7-bit
// code in bits [16..10] and an 8-bit sub field in bits [9..2].
int AptAnimationTarget::ProcessAptInput(unsigned int nPackedInput, int bFirstThisFrame)
{
    const int nEventId = (int)(nPackedInput >> 17);          // srwi 17
    const int nCode    = (int)((nPackedInput >> 10) & 0x7F); // extrwi 7,15
    const int nSub     = (int)((nPackedInput >> 2) & 0xFF);  // extrwi 8,22

    ProcessInputSet(nEventId, nCode, nPackedInput, nSub, bFirstThisFrame);
    return ProcessListenerEvents(nEventId, (unsigned int)nCode, (int)nPackedInput, nSub);
}

// ProcessInputs @0x82B01FD0 -- drain the queued-input ring this frame: dispatch each
// packed event (the first event of the frame flagged), then reset the fill counter.
int AptAnimationTarget::ProcessInputs()
{
    int result = 0;
    int nIndex = 0;   // v2/r30: 0-based event ordinal (drives the "first this frame" flag)
    if ((int)mnQueuedInputsCount > 0)
    {
        do
        {
            // bFirstThisFrame == (v2++ == 0); the X360 forms it via cntlzw/extrwi.
            result = ProcessAptInput(mpQueuedInputs[nIndex], nIndex == 0 ? 1 : 0);
            ++nIndex;
        }
        while (nIndex < (int)mnQueuedInputsCount);
    }
    mnQueuedInputsCount = 0;
    return result;
}
