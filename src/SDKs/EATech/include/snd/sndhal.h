#ifndef SDKS_EATECH_SND_SNDHAL_H
#define SDKS_EATECH_SND_SNDHAL_H

#include "types.hpp"
#include <cstddef>

// ============================================================================
// SDKs/EATech/include/snd/sndhal.h  (Snd9::Hal channel volume/pan home)
//
// EATech "Snd9" hardware-abstraction-layer channel mixer. Sibling of the sndo.h
// sample-player surface and the sndaems.h module-bank registry. This header is the
// canonical OWNING home for the Snd9::Hal channel-gain path reconstructed from
// BURNOUT_X360_ARTIST.XEX (the PowerPC asm is authoritative for every offset,
// width and side-effect; no Feb-2007 source and no DecFIGS DWARF exist for this
// TU):
//     Snd9::Hal::SetPan          @ 0x82B7F910
//     Snd9::Hal::SetVol          @ 0x82B7E5F0
//     Snd9::Hal::SetVolInternal  @ 0x82B7D228
//
// The HAL keeps three parallel per-channel tables, each indexed by channel number:
//   * gpChannels    -- the source channel state (volume, sample level, pan info),
//   * gpChannelPan  -- the per-speaker pan gains a channel maps onto,
//   * gpMixChannels -- the mixer's per-speaker output gains + dirty flag.
// The tables themselves are allocated/initialised by the (not-yet-reconstructed)
// Snd9 HAL setup path; the functions here only read/write into them. `Snd9`/`Hal`
// are an EATech vendor boundary, so those identifiers are preserved verbatim per
// the naming convention.
//
// Layout follows the Xbox 360 channel and mixer array strides.
// ============================================================================

namespace Snd9
{
namespace Mix { struct FilterState; struct ResampleFilter; }
namespace Hal
{

// ARTIST82B80098: each envelope segment contains a frame duration and target level.
struct EnvelopeSegment
{
    s32 miDuration;
    s32 miTarget;
};
CGS_STATIC_ASSERT((sizeof(EnvelopeSegment) == 8), "ARTIST Snd9 envelope segment");

// A source channel's HAL state (X360 stride 0x84).
struct Channel
{
    s32  miVoiceHandle;       // +0x00
    s16  maiSampleVoices[6];   // +0x04
    u8   mPad10[0x1C - 0x10];
    u16  muPanPosition;       // +0x1C  azimuth handed to SNDI_aztospkrvol
    u8   mPad1E;              // +0x1E
    u8   mucPanActive;        // +0x1F  nonzero => channel carries an explicit per-speaker pan
    u16  muSampleRate;         // +0x20
    u8   mPad22;
    u8   mucSampleVoiceCount;  // +0x23
    u8   mucVoiceGroup;       // +0x24
    u8   mucGroupPrimary;     // +0x25
    s16  miPitchPatternDepth;  // +0x26
    s16  miParentVoice;       // +0x28, -1 for the primary voice
    u8   mPad2A;
    u8   mucPacketFlags;      // +0x2B
    u32  muLastVoiceTick;     // +0x2C
    f32  mfFadeStep;          // +0x30
    f32  mfFadeTarget;        // +0x34
    f32  mfFadeCurrent;       // +0x38
    s32  miEnvelopeStep;      // +0x3C
    s32  miEnvelopeCurrent;   // +0x40
    s32  miEnvelopeRemaining; // +0x44
    f32  mfVolume;            // +0x48  linear channel volume
    u8   mPad4C[2];
    u8   mucVolumePatternLength; // +0x4E
    u8   mucPitchPatternLength;  // +0x4F
    u8   mucVolumePatternIndex;  // +0x50
    u8   mPad51[0x60 - 0x51];
    s8   mcEnvelopeCount;     // +0x60
    s8   mcEnvelopeIndex;     // +0x61
    u8   mPad62;
    s8   mcLevel;             // +0x63  signed sample level (0..127)
    f32* mpSourceGains;       // +0x64  per-output source gains
    u8   mucPitchPatternIndex; // +0x68
    u8   mucVoiceState;       // +0x69
    u8   mPad6A[2];
    const EnvelopeSegment* mpEnvelope; // +0x6C
    const s8* mpcVolumeRemap;  // +0x70
    const s8* mpcVolumePattern; // +0x74
    const s8* mpcPitchPattern;  // +0x78
    s16  miBasePitchCents;    // +0x7C
    u16  muCachedPitch;        // +0x7E
    u16  muPitchMultiplier;   // +0x80
    u16  muComputedPitch;     // +0x82
};
CGS_STATIC_ASSERT((sizeof(Channel) == 0x84), "ARTIST HAL channel stride");
CGS_STATIC_ASSERT((offsetof(Channel, maiSampleVoices) == 4), "ARTIST sample voice group");
CGS_STATIC_ASSERT((offsetof(Channel, mucVoiceGroup) == 0x24), "ARTIST voice group");
CGS_STATIC_ASSERT((offsetof(Channel, mucGroupPrimary) == 0x25), "ARTIST voice group primary");
CGS_STATIC_ASSERT((offsetof(Channel, miParentVoice) == 0x28), "ARTIST parent voice");
CGS_STATIC_ASSERT((offsetof(Channel, mucPacketFlags) == 0x2B), "ARTIST packet flags");
CGS_STATIC_ASSERT((offsetof(Channel, muLastVoiceTick) == 0x2C), "ARTIST voice tick");
CGS_STATIC_ASSERT((offsetof(Channel, mucVoiceState) == 0x69), "ARTIST voice state");
CGS_STATIC_ASSERT((offsetof(Channel, mfFadeStep) == 0x30), "ARTIST voice fade step");
CGS_STATIC_ASSERT((offsetof(Channel, miEnvelopeStep) == 0x3C), "ARTIST envelope delta");
CGS_STATIC_ASSERT((offsetof(Channel, mucVolumePatternLength) == 0x4E), "ARTIST modulation lengths");
CGS_STATIC_ASSERT((offsetof(Channel, mcEnvelopeCount) == 0x60), "ARTIST envelope count");
CGS_STATIC_ASSERT((offsetof(Channel, mucPitchPatternIndex) == 0x68), "ARTIST pitch pattern index");
CGS_STATIC_ASSERT((offsetof(Channel, mpEnvelope) == 0x6C), "ARTIST envelope pointer");
CGS_STATIC_ASSERT((offsetof(Channel, mpcVolumePattern) == 0x74), "ARTIST volume pattern");
CGS_STATIC_ASSERT((offsetof(Channel, mpcPitchPattern) == 0x78), "ARTIST pitch pattern");
CGS_STATIC_ASSERT((offsetof(Channel, muSampleRate) == 0x20), "ARTIST sample rate");
CGS_STATIC_ASSERT((offsetof(Channel, mucSampleVoiceCount) == 0x23), "ARTIST sample voice count");
CGS_STATIC_ASSERT((offsetof(Channel, miPitchPatternDepth) == 0x26), "ARTIST pitch modulation depth");
CGS_STATIC_ASSERT((offsetof(Channel, mpcVolumeRemap) == 0x70), "ARTIST volume remap");
CGS_STATIC_ASSERT((offsetof(Channel, miBasePitchCents) == 0x7C), "ARTIST base pitch");
CGS_STATIC_ASSERT((offsetof(Channel, muCachedPitch) == 0x7E), "ARTIST cached pitch");
CGS_STATIC_ASSERT((offsetof(Channel, muPitchMultiplier) == 0x80), "ARTIST pitch multiplier");
CGS_STATIC_ASSERT((offsetof(Channel, muComputedPitch) == 0x82), "ARTIST computed pitch");
CGS_STATIC_ASSERT((offsetof(Channel, mpSourceGains) == 0x64), "ARTIST HAL channel gains");

// A channel's per-speaker pan gains (0x18 = 24 bytes, all f32 -> byte-exact). The
// reset path zeroes mafGain[0 .. numSpeakers-1] and forces mafGain[5] (+0x14) to
// unity, so index 5 is the reference/last-speaker slot.
struct SpeakerVol
{
    f32 mafGain[6]; // +0x00 .. +0x14  per-speaker gains
};
CGS_STATIC_ASSERT((sizeof(SpeakerVol) == 24), "Snd9::Hal::SpeakerVol must be 24 bytes");

// One mixer channel's computed output (X360 stride 0x5C). mafSpeakerGain holds the
// per-speaker gains SetVolInternal writes; mpOutputBuffer is the destination gain
// buffer SetVol writes through.
struct MixChannel
{
    u8   mucState;            // +0x00  native mixer voice state
    u8   mucDirty;            // +0x01  set to 1 whenever the gains are recomputed
    u8   mPad02[2];           // +0x02
    f32  mafPreviousSpeakerGain[6]; // +0x04  previous ramp gains
    f32  mafSpeakerGain[6];   // +0x1C  computed per-speaker gains
    f32* mpPreviousOutputGains; // +0x34  first per-output buffer from MIX_create
    f32* mpOutputBuffer;      // +0x38  destination gain buffer
    f32  mfLastSample;        // +0x3C
    Mix::FilterState* mpFilter; // +0x40
    u8   mPad44[0x50 - 0x44];
    Mix::ResampleFilter* mpResample; // +0x50
    u8   mPad54[0x5C - 0x54];
};
CGS_STATIC_ASSERT((sizeof(MixChannel) == 0x5C), "ARTIST mixer channel stride");
CGS_STATIC_ASSERT((offsetof(MixChannel, mafPreviousSpeakerGain) == 0x04), "ARTIST mixer previous gains");
CGS_STATIC_ASSERT((offsetof(MixChannel, mfLastSample) == 0x3C), "ARTIST mixer last sample");
CGS_STATIC_ASSERT((offsetof(MixChannel, mpFilter) == 0x40), "ARTIST mixer filter");
CGS_STATIC_ASSERT((offsetof(MixChannel, mpResample) == 0x50), "ARTIST mixer resampler");
CGS_STATIC_ASSERT((offsetof(MixChannel, mafSpeakerGain) == 0x1C), "ARTIST mixer speaker gains");
CGS_STATIC_ASSERT((offsetof(MixChannel, mpPreviousOutputGains) == 0x34), "ARTIST mixer previous output gains");
CGS_STATIC_ASSERT((offsetof(MixChannel, mpOutputBuffer) == 0x38), "ARTIST mixer output gains");

// The shared per-channel HAL tables + speaker/output counts. Owned here; the Snd9
// HAL setup path (its own not-yet-reconstructed TU) allocates/populates them, the
// mixer functions here only index into them.
extern Channel*    gpChannels;     // off_832778C0  -> Channel[]
extern SpeakerVol* gpChannelPan;   // dword_83277620 -> SpeakerVol[]
extern u8          gucNumSpeakers;  // byte_83277660   speaker count (pan/level loops)
extern MixChannel* gpMixChannels;   // off_8327781C   -> MixChannel[]
extern s32         giNumOutputs;    // dword_82F883D0  output count (mix loop)

// @ 0x82B7F910. Update channel aiChannel's pan: if the channel carries an explicit
// pan, clear its per-speaker gains and force the reference slot to unity; otherwise
// derive the per-speaker gains from the channel's azimuth via SNDI_aztospkrvol.
// Then re-run the volume path (SetVol) and forward its result.
int SetPan(int aiChannel);

// @ 0x82B7E5F0. Recompute channel aiChannel's per-output gains: run SetVolInternal,
// then, for each output, write output = source-gain * channel-volume and flag the
// mixer channel dirty.
int SetVol(int aiChannel);

// @ 0x82B7D228. Recompute channel aiChannel's per-speaker mixer gains from its pan
// gains scaled by (sample level 0..127) * volume / 127, flagging the mixer channel
// dirty. Returns aiChannel unchanged.
int SetVolInternal(int aiChannel);

} // namespace Hal
} // namespace Snd9

#endif // SDKS_EATECH_SND_SNDHAL_H
