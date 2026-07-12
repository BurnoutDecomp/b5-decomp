#ifndef SDKS_EATECH_SND_SNDAEMSSAMPLEPLAYER_H
#define SDKS_EATECH_SND_SNDAEMSSAMPLEPLAYER_H

#include "types.hpp"

#include "SDKs/EATech/include/snd/sndo.h" // Snd9::IAemsSamplePlayer (the base interface)

// ============================================================================
// SDKs/EATech/include/snd/sndaemssampleplayer.h
//   (Snd9::AemsStandardSamplePlayer home)
//
// EATech "Snd9" AEMS standard sample player: the middleware's own concrete
// implementation of Snd9::IAemsSamplePlayer (sndo.h), sibling of the game's
// CgsSound::Playback::AemsRWSamplePlayer. It wraps a main rw::audio::core::Voice
// plus its plug-in chain -- a sample-player source stage (mpSndPlayer), a resample
// (pitch) stage, low/high-pass stages, a wet-send stage and a gain stage -- and
// optionally an array of per-channel panner voices with a matching Pan2D plug-in
// array. The Snd9 sample-player control surface (Release / Unpause / SetInput /
// SetAzimuth / GetOutputs) is routed onto those plug-ins.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative
// for every offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF
// exist for this TU. `Snd9`/`Aems` are an EATech vendor boundary, so those
// identifiers are preserved verbatim per the naming convention.
//   Snd9::AemsStandardSamplePlayer::AemsStandardSamplePlayer  @ 0x82B71C70
//   Snd9::AemsStandardSamplePlayer::Release                   @ 0x82B723C8
//   Snd9::AemsStandardSamplePlayer::Unpause                   @ 0x82B720D8
//   Snd9::AemsStandardSamplePlayer::SetInput                  @ 0x82B71D10
//   Snd9::AemsStandardSamplePlayer::SetAzimuth                @ 0x82B71E98
//
// INHERITANCE (inferred, well-grounded): the class is created through the
// Snd9::IAemsSamplePlayerFactory-derived Snd9::AemsStandardSamplePlayerFactory
// (whose CreateInstance calls this ctor) and hands back an IAemsSamplePlayer, and
// its method family (Release/Pause/Unpause/SetInput/SetAzimuth/GetOutputs) matches
// the IAemsSamplePlayer control surface one-for-one -- so it derives from
// Snd9::IAemsSamplePlayer. That base carries no data members (only the vtable), so
// the derived layout places its own members immediately after the vptr, exactly as
// the ctor asm builds it (vptr store @+0x00 followed by the member init sequence).
//
// LAYOUT: the +0xNN annotations are the X360 (32-bit-pointer) offsets proven by the
// ctor/method asm and are documentary only; pointer members are declared with x64
// widths so only member ORDER is load-bearing (no offset asserts on the
// pointer-bearing tail -- same rule sndaems.h / sndhal.h use).
//
// FLAG (CROSS-TU / BLOCKED methods declared here for vtable shape, NOT defined):
//   * ~AemsStandardSamplePlayer -- the ledger's `vector deleting destructor'
//     @ 0x82B72380 stores off_820AB14C, which is
//     CgsSound::Playback::AemsRWSamplePlayer's vtable (see the done
//     CgsAemsInterfaceImplementation TU), NOT this class's own ctor-installed vtable
//     off_8214B310. It is a mis-attributed AemsRWSamplePlayer thunk, so it is BLOCKED
//     (reconstructing it here would install the wrong vtable). Declared virtual so the
//     base's virtual dtor slot is satisfied; its real definition is not in this TU.
//   * Pause -- its own (not-yet-done) TU.
//   * GetOutputs -- BLOCKED (its body calls the un-homed rw::audio::core::PlugIn-family
//     attribute-query sub_82B6A8E0 @ 0x82B6A8E0, whose class home and prototype are
//     not recovered). Declared here for the vtable slot; defined when that callee is homed.
// ============================================================================

namespace rw
{
namespace audio
{
namespace core
{
    class Voice;   // rw::audio::core mixing voice (pointer members only; full home Voice.h)
    class PlugIn;  // rw::audio::core processing-graph node (pointer members only; full home PlugIn.h)
}
}
}

namespace Snd9
{

// The EATech AEMS standard sample player. Concrete IAemsSamplePlayer backend.
struct AemsStandardSamplePlayer : public Snd9::IAemsSamplePlayer
{
    // Fixed capacity of the panner-voice / Pan2D plug-in arrays (X360 stride between
    // the +0x20 voice array and the +0x34 plug-in array is 5 words each).
    static const u32 KU_MAX_PANNER_VOICES = 5;

    // @ 0x82B71C70. Default-construct: install the vtable (compiler-emitted), null every
    // plug-in / voice pointer, and seed the gain/level defaults (pitch/vol/dry = 1.0,
    // wet = 0.0, start-threshold = -1.0, length = 0.0, unpaused, 0 panner voices).
    AemsStandardSamplePlayer();

    // @ 0x82B72380 -- BLOCKED (mis-attributed AemsRWSamplePlayer thunk; see file header).
    // Declared for the base virtual-dtor slot; NOT defined in this TU.
    virtual ~AemsStandardSamplePlayer();

    // ---- Snd9::IAemsSamplePlayer control surface ----
    virtual void Release();                                       // @ 0x82B723C8
    virtual void Pause();                                         // own TU (declared for slot)
    virtual void Unpause();                                       // @ 0x82B720D8
    virtual void SetInput(InputSelector aeSelector, int aiValue); // @ 0x82B71D10
    virtual void SetAzimuth(int aiAzimuth, int* apLegacyAzimuths); // @ 0x82B71E98
    virtual void GetOutputs(int aiNumOutputs, int* apValues);     // @ 0x82B71F20 -- BLOCKED

    // ---- layout (X360 offsets documentary; x64 widths, by-name access) ----
    rw::audio::core::Voice*  mpVoice;                        // +0x04  main mixing voice
    rw::audio::core::PlugIn* mpSndPlayer;                    // +0x08  sample-player source stage
    rw::audio::core::PlugIn* mpResample;                     // +0x0C  resample (pitch) stage
    rw::audio::core::PlugIn* mpHighPass;                     // +0x10  high-pass stage
    rw::audio::core::PlugIn* mpLowPass;                      // +0x14  low-pass stage
    rw::audio::core::PlugIn* mpSendWet;                      // +0x18  wet-send stage
    rw::audio::core::PlugIn* mpGain;                         // +0x1C  gain (dry) stage
    rw::audio::core::Voice*  mpPannerVoice[KU_MAX_PANNER_VOICES]; // +0x20  per-channel panner voices
    rw::audio::core::PlugIn* mpPan2D[KU_MAX_PANNER_VOICES];  // +0x34  per-channel Pan2D stages
    f32    mfPitch;                                          // +0x48  init 1.0
    f32    mfVol;                                            // +0x4C  master volume, init 1.0
    f32    mfDryLevel;                                       // +0x50  init 1.0
    f32    mfWetLevel;                                       // +0x54  init 0.0
    f32    mfStartThreshold;                                 // +0x58  play-start compare level, init -1.0
    // +0x5C  (4 bytes) padding to 8-byte-align the following double
    double mdSampleLength;                                   // +0x60  total sample length (s), init 0.0
    u32    mPauseState;                                      // +0x68  paused flag, init 0 (Unpause clears)
    u8     muVoiceBaseIndex;                                 // +0x6C  base index into the legacy-azimuth table
    u8     muNumVoices;                                      // +0x6D  live panner-voice count
};

} // namespace Snd9

#endif // SDKS_EATECH_SND_SNDAEMSSAMPLEPLAYER_H
