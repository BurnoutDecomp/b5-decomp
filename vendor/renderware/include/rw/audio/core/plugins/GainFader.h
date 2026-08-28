#pragma once

// =====================================================================================
// rw::audio::core::GainFader -- the "GainFader" audio plug-in: a gain stage that ramps
// between two levels over a scheduled, timed fade.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. Decode report:
// progress/scratch_dossiers/gainfader_lowpassbutterworth_decode_codex.md.
//   GetPlugInDescRunTime @0x82B97368 -> the registered "GainFader" descriptor (off_82F8CC50)
//   GetSize              @0x82B97360 -> console 0x70; host sizeof (the stage carve)
//   CreateInstance       @0x82BA2C08
//   Process              @0x82B97378
//   EventEvent           @0x82BA2C50 (vt[1]) -- EVENT_STARTFADE only
//   StartFadeHandler     @0x82B9DF18 -- the deferred command replayed off the System ring
//   `vector deleting destructor' @0x82BA1758 (vt[3]); vt[0]/vt[2] are ICF-shared no-ops
//
// ⭐ VENDOR HEADER: references/Feb-2007/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/
// 2.11.00/include/rw/audio/core/plugins/gainfader.h is the authoritative naming source and
// MATCHES the ARTIST layout member-for-member -- every enum, the StartFadeParams event
// record, the StartFadeCommand ring record, the Request block and all thirteen members
// below are its own names.
//
// This plug-in HAS an event (its descriptor's numEvents byte is 1), so like Dac it is a
// real polymorphic PlugIn subclass rather than a PlugInBaseView composition: the console
// dispatches its event through vt[1], which is exactly what PlugIn::Event tail-vcalls into.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API.
// =====================================================================================

#include "types.hpp" // f32, f64, s32, u32, u8
#include "rw/audio/core/PlugIn.h" // PlugIn (the polymorphic base) + Attribute_t

namespace rw
{
namespace audio
{
namespace core
{

class Mixer;
typedef Mixer AudioProcessContext;

// -------------------------------------------------------------------------------------
// GainFader -- console sizeof 0x70. Layout grounded in CreateInstance @0x82BA2C08,
// Process @0x82B97378 and StartFadeHandler @0x82B9DF18; names from the vendor header.
//   +0x00..+0x27  the PlugIn base (vptr, mpSystem, mpAttribute @+0x0C, mInputChannels @+0x20)
//   +0x28  mAttribute[1]   -- ATTRIBUTE_GETCURRENTGAIN; Process publishes the frame's last
//                             gain here so the graph can read the fader's current level
//   +0x30  mLastRequest    -- the pending Request the deferred handler latched
//   +0x48  mStartTime      -- the live fade's scheduled start (stream time, seconds)
//   +0x50  mFadeTime       -- the live fade's duration in seconds
//   +0x54  mFadeSamplesTotal   -- that duration in samples (>= 1)
//   +0x58  mCurrentFadeSample  -- how far into the fade this frame starts
//   +0x5C  mStartGain / +0x60 mEndGain -- the ramp endpoints
//   +0x64  mLastGain       -- the gain the previous frame ended on (the hold level)
//   +0x68  mUnservicedRequest / +0x69 mFadeState / +0x6A mFadeType
// -------------------------------------------------------------------------------------
class GainFader : public PlugIn
{
public:
    enum { KU_GUID = 0x47614630u };   // 'GaF0'

    enum Attribute
    {
        ATTRIBUTE_GETCURRENTGAIN = 0,
        ATTRIBUTE_MAX = 1
    };

    enum EventId
    {
        EVENT_STARTFADE = 0
    };

    enum FadeType
    {
        FADETYPE_LINEARAMPLITUDE = 0,
        FADETYPE_LINEARPOWER = 1,
        FADETYPE_SINAMPLITUDE = 2,
        FADETYPE_MAX = 3
    };

    enum FadeState
    {
        FADESTATE_FINISHED = 0,
        FADESTATE_PENDING = 1,
        FADESTATE_FADING = 2,
        NUM_STATES = 3
    };

    // The caller-supplied event parameter block for EVENT_STARTFADE (vendor name/shape).
    // NOTE the fadeType arrives as a FLOAT here and is truncated to the enum by EventEvent.
    struct StartFadeParams
    {
        f64 startTime;   // +0x00
        f32 fadeTime;    // +0x08
        f32 endGain;     // +0x0C
        f32 fadeType;    // +0x10
    };

    // The queued command EventEvent pushes into the System ring (console stride 0x20).
    // RECORD-STRIDE RULE (the X360-literal trap): the producer's cursor advance and
    // StartFadeHandler's return must BOTH be this record's size, and on the host that is
    // sizeof(StartFadeCommand) -- the two leading pointers widen -- never the console 0x20.
    struct StartFadeCommand
    {
        int (*mpHandler)(void *); // +0x00 -- &GainFader::StartFadeHandler
        GainFader *mpTarget;       // +0x04
        f64 startTime;             // +0x08
        f32 fadeTime;              // +0x10
        f32 endGain;               // +0x14
        s32 fadeType;              // +0x18 -- the fctiwz'd FadeType
    };

    // The latched request the next Process services.
    struct Request
    {
        f64 startTime;  // +0x00
        f32 fadeTime;   // +0x08
        f32 endGain;    // +0x0C
        s32 fadeType;   // +0x10 (FadeType)
    };

    // vt[1] -- the engine event entry (PlugIn::Event dispatches here). Only
    // EVENT_STARTFADE is accepted; every other id returns without touching the ring.
    // (The console body forms no return value; the host returns 0 like Dac's.)
    virtual int Event(int aiEventId, void *apParam);

    static char **GetPlugInDescRunTime();                    // @0x82B97368
    static int    GetSize();                                 // @0x82B97360
    static int    CreateInstance(GainFader *self);           // @0x82BA2C08
    static int    Process(GainFader *self, AudioProcessContext *ctx,
                          bool discontinuity);               // @0x82B97378
    static int    StartFadeHandler(void *apCommand);         // @0x82B9DF18

    PlugIn::Attribute_t mAttribute[ATTRIBUTE_MAX]; // +0x28
    Request mLastRequest;        // +0x30 .. +0x47
    f64 mStartTime;              // +0x48
    f32 mFadeTime;               // +0x50
    s32 mFadeSamplesTotal;       // +0x54
    s32 mCurrentFadeSample;      // +0x58
    f32 mStartGain;              // +0x5C
    f32 mEndGain;                // +0x60
    f32 mLastGain;               // +0x64
    u8  mUnservicedRequest;      // +0x68
    u8  mFadeState;              // +0x69 (FadeState)
    u8  mFadeType;               // +0x6A (FadeType, narrowed to a byte)
};

} // namespace core
} // namespace audio
} // namespace rw
