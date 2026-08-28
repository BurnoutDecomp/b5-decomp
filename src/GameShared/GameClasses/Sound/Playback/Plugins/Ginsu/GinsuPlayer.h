#pragma once

// =====================================================================================
// rw::audio::core::GinsuPlayer -- the GRANULAR ENGINE-SOUND SYNTHESIZER ("Gns0"), the
// heart of Burnout's car-engine audio.
//
// It is a mono SOURCE-stage plug-in. A play event binds a "Gnsu2" blob -- a predictive
// coded stream of 19 bytes per 32 samples, plus a frequency table and a cycle table -- and
// then, every frame, maps the requested engine frequency to a position in that stream,
// periodically JUMPS to a deterministically-chosen nearby cycle (so a short recording does
// not audibly loop), crossfades over 0.5 ms, and resamples the decoded stream to the
// requested output rate.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the asm is authoritative. Decode report
// progress/scratch_dossiers/ginsuplayer_decode_codex.md.
//   GetSize        @0x826A40B8   CreateInstance @0x826C3418
//   PreProcess     @0x8268C1D8   Process        @0x8268C1E8
//   EventEvent     @0x826C3498 (vt[1])   PlayHandler @0x826A4100   StopHandler @0x8268B1B8
//   `vector deleting destructor' @0x826AA938; vt[0]/vt[2] are ICF-shared no-ops
//   descriptor off_82F2D094 'Gns0' (type 0 == SOURCE stage, 1 ctor param, 5 attributes,
//   2 events)
//
// Although this code lives in the GAME module's address range (0x826xxxxx) rather than the
// vendor rwaudio range, its symbols are `rw::audio::core::GinsuPlayer::*`, so the namespace
// below is the console's own -- the same situation as the committed SndPlayer1_CgsStreamMod.
// There is no Feb-2007 vendor header for it; the names come from the decode's offset/access
// analysis plus the DecFIGS virtual order.
// =====================================================================================

#include "types.hpp"              // f32, f64, s32, u32, u8, u16
#include "rw/audio/core/PlugIn.h" // PlugIn (the polymorphic base) + Attribute_t + System

#include <cstddef> // size_t

namespace rw
{
namespace audio
{
namespace core
{

class Mixer;

// -------------------------------------------------------------------------------------
// The serialized "Gnsu2" blob header (32 bytes). This is an explicit on-disk byte layout,
// read at fixed serialized offsets and copied into native fields -- deliberately NOT a
// packed struct the code dereferences.
//   +0x00 id[4] "Gnsu"      +0x04 version[2] (starts '2')   +0x06 endianDone (u16)
//   +0x08 minFrequency f32  +0x0C maxFrequency f32
//   +0x10 segCount s32      +0x14 cycleCount s32
//   +0x18 sampleCount s32   +0x1C sampleRate s32
// then s32 frequencySamples[segCount+1], s32 cycleSamples[cycleCount+1], then the encoded
// sample bytes (19 per 32 samples).
// -------------------------------------------------------------------------------------
enum
{
    KI_GINSU_HEADER_BYTES = 32,
    KI_GINSU_RECORD_BYTES = 19,   // one encoded record
    KI_GINSU_BLOCK_SAMPLES = 32,  // ... expands to this many samples
    KI_GINSU_OLD_BLOCKS = 8,      // the crossfade look-back cache holds this many records
    KI_GINSU_OLD_BYTES = KI_GINSU_OLD_BLOCKS * KI_GINSU_RECORD_BYTES  // 152
};

// -------------------------------------------------------------------------------------
// GinsuSynthData -- the bound-content decoder. Console extent 0x154 at player +0x58.
// mFreqOffset / mCycleOffset are RELATIVE offsets from this object (not serialized
// pointers), because the tables are copied into the instance's own allocation tail.
// -------------------------------------------------------------------------------------
class GinsuSynthData
{
public:
    static u32  GetTotalTableSize(void *apGinFile);                       // @0x8268B308
    bool BindToData(void *apGinFile, void *apTableStorage);               // @0x8268B398
    void DecodeBlock(int aiBlock, bool abUseOldData, System *apSystem);   // @0x8268B588
    int   FrequencyToSample(f32 afFrequency) const;                       // @0x8268B7B8
    int   CycleToSample(f32 afCycle) const;                               // @0x8268B8F8
    f32   CyclePeriod(f32 afCycle) const;                                 // @0x8268BA20
    f32   SampleToCycle(int aiSample) const;                              // @0x8268BBF8
    bool  GetSamples(int aiStartSample, int aiNumInputSamples,
                     int aiNumOutputSamples, f32 *apOutput,
                     f32 *apResampleBuffer, bool abUseOldData,
                     System *apSystem);                                   // @0x8268BED8

    // The two copied tables live in the instance's allocation tail; both are reached
    // through a relative offset so they stay correct at host widths.
    const s32 *FrequencyTable() const
    {
        return reinterpret_cast<const s32 *>(
            reinterpret_cast<const u8 *>(this) + mFreqOffset);
    }
    const s32 *CycleTable() const
    {
        return reinterpret_cast<const s32 *>(
            reinterpret_cast<const u8 *>(this) + mCycleOffset);
    }

    u8   mOldDataBlock[KI_GINSU_OLD_BYTES]; // +0x00 -- the crossfade look-back record cache
    s32  mOldDataBlockIndex;                // +0x98 -- first block the cache covers; -1 == invalid
    s32  mTempStoreBlockIndex;              // +0x9C
    f32 *mpTempStore;                       // +0xA0
    f32  mLastInputSample;                  // +0xA4 -- continuity for the resampler's prefix
    s32  mCycleCount;                       // +0xA8
    f32  mMinFrequency;                     // +0xAC
    f32  mMaxFrequency;                     // +0xB0
    s32  mSegCount;                         // +0xB4
    s32  mSampleCount;                      // +0xB8
    s32  mSampleRate;                       // +0xBC
    size_t mFreqOffset;                     // +0xC0 -- RELATIVE to this
    size_t mCycleOffset;                    // +0xC4 -- RELATIVE to this
    const u8 *mSampleData;                  // +0xC8 -- into the bound blob
    f32  mMinPeriod;                        // +0xCC -- smallest adjacent cycle-table delta
    s32  mCurrentBlock;                     // +0xD0 -- which block mSample holds; -1 == none
    f32  mSample[KI_GINSU_BLOCK_SAMPLES];   // +0xD4 -- the decoded block
};

// -------------------------------------------------------------------------------------
// GinsuPlayer -- console sizeof 0x1D0, followed by the copied frequency/cycle tables in an
// over-allocated tail (which is why GetSize is content-dependent).
// -------------------------------------------------------------------------------------
class GinsuPlayer : public PlugIn
{
public:
    enum { KU_GUID = 0x476E7330u };   // 'Gns0'

    enum Attribute
    {
        ATTRIBUTE_SETFREQUENCY = 0,   // the requested engine frequency (init 1000)
        ATTRIBUTE_SETJUMPSPAN  = 1,   // how many cycles wide the random jump window is
        ATTRIBUTE_GETSAMPLERATE = 2,  // readback: the bound content's sample rate
        ATTRIBUTE_GETMINFREQUENCY = 3,// readback: the bound content's minimum frequency
        ATTRIBUTE_GETMAXFREQUENCY = 4,// readback: the bound content's maximum frequency
        ATTRIBUTE_MAX = 5
    };

    enum EventId
    {
        EVENT_PLAY = 0
        // NOTE: there is deliberately no EVENT_STOP constant -- the console does not
        // compare against 1. EVERY nonzero id dispatches stop.
    };

    // The constructor parameter AND the event-0 parameter block are the same one-pointer
    // record: the "Gnsu2" blob to bind.
    struct PlayParams
    {
        void *pGinFile;   // +0x00
    };

    // The two deferred command records. RECORD-STRIDE RULE (the X360-literal trap): the
    // console sizes are 12 and 8, but both records lead with widening pointers, so the
    // producer's advance and each handler's return must be the HOST sizeof.
    struct PlayCommand
    {
        int (*mpHandler)(void *); // +0x00
        GinsuPlayer *mpTarget;     // +0x04
        void *mpGinFile;           // +0x08
    };
    struct StopCommand
    {
        int (*mpHandler)(void *); // +0x00
        GinsuPlayer *mpTarget;     // +0x04
    };

    // vt[1] -- EVENT_PLAY queues a bind+start; anything else queues a stop.
    virtual int Event(int aiEventId, void *apParam);

    static char **GetPlugInDescRunTime();
    static int    GetSize(const VoiceStageConfig *config);                 // @0x826A40B8
    static int    CreateInstance(GinsuPlayer *self,
                                 const PlayParams *params);                // @0x826C3418
    static int    PreProcess(GinsuPlayer *self, Mixer *ctx, bool discontinuity,
                             int outputSamplesRequested);                  // @0x8268C1D8
    static int    Process(GinsuPlayer *self, Mixer *ctx, bool isLastInput); // @0x8268C1E8
    static int    PlayHandler(void *apCommand);                            // @0x826A4100
    static int    StopHandler(void *apCommand);                            // @0x8268B1B8

    // The host layout helper BOTH GetSize and PlayHandler must agree on: where the copied
    // tables start relative to the instance base. The console's 0x1D0/0x11D0/(self+0x1D7)&~7
    // are console extents containing 32-bit pointers and do NOT survive on x64.
    static size_t TableStorageOffset();

    PlugIn::Attribute_t mAttribute[ATTRIBUTE_MAX]; // +0x28 .. +0x4F
    u8   mPlaying;                                 // +0x50
    s32  mOutputSamplesRequested;                  // +0x54 -- a full WORD (SndPlayer1's is a halfword)
    GinsuSynthData mSynthData;                     // +0x58 .. +0x1AB
    void *mpGinFile;                               // +0x1AC
    f32  mSampleRate;                              // +0x1B0
    f32  mPrevSampleRate;                          // +0x1B4
    s32  mNoJumpSize;                              // +0x1B8 -- initialised, never read by Process
    s32  mOverlapSize;                             // +0x1BC
    s32  mPlaybackPos;                             // +0x1C0 -- decoded-input sample index
    u32  mRandomSeed;                              // +0x1C4
    f64  mNextJumpTime;                            // +0x1C8
};

} // namespace core
} // namespace audio
} // namespace rw
