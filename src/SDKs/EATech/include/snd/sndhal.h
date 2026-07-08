#ifndef SDKS_EATECH_SND_SNDHAL_H
#define SDKS_EATECH_SND_SNDHAL_H

#include "types.hpp"

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
// LAYOUT: the +0xNN annotations are the X360 (32-bit-pointer) offsets proven by the
// asm and are documentary only; pointer members are declared with x64 widths so only
// member ORDER is load-bearing (no offset asserts on pointer-bearing structs, same
// rule sndaems.h uses). SpeakerVol is all-f32 so it is byte-exact on both platforms
// and does carry a size assert.
// ============================================================================

namespace Snd9
{
namespace Hal
{

// A single source channel's HAL state (X360 stride 0x84). Only the fields the
// gain/pan path touches are modelled; the rest is unmodelled HAL state.
struct Channel
{
    u8   mPad00[0x1C];        // +0x00
    u16  muPanPosition;       // +0x1C  azimuth handed to SNDI_aztospkrvol
    u8   mPad1E;              // +0x1E
    u8   mucPanActive;        // +0x1F  nonzero => channel carries an explicit per-speaker pan
    u8   mPad20[0x48 - 0x20]; // +0x20
    f32  mfVolume;            // +0x48  linear channel volume
    u8   mPad4C[0x63 - 0x4C]; // +0x4C
    s8   mcLevel;             // +0x63  signed sample level (0..127)
    f32* mpSourceGains;       // +0x64  per-output source gains (x64-widened pointer)
    // remaining bytes to +0x84 are unmodelled HAL state
};

// A channel's per-speaker pan gains (0x18 = 24 bytes, all f32 -> byte-exact). The
// reset path zeroes mafGain[0 .. numSpeakers-1] and forces mafGain[5] (+0x14) to
// unity, so index 5 is the reference/last-speaker slot.
struct SpeakerVol
{
    f32 mafGain[6]; // +0x00 .. +0x14  per-speaker gains
};
static_assert(sizeof(SpeakerVol) == 24, "Snd9::Hal::SpeakerVol must be 24 bytes");

// One mixer channel's computed output (X360 stride 0x5C). mafSpeakerGain holds the
// per-speaker gains SetVolInternal writes; mpOutputBuffer is the destination gain
// buffer SetVol writes through.
struct MixChannel
{
    u8   mPad00;              // +0x00
    u8   mucDirty;            // +0x01  set to 1 whenever the gains are recomputed
    u8   mPad02[0x1C - 0x02]; // +0x02
    f32  mafSpeakerGain[7];   // +0x1C  computed per-speaker gains (+0x1C..+0x37)
    f32* mpOutputBuffer;      // +0x38  destination gain buffer (x64-widened pointer)
    // remaining bytes to +0x5C are unmodelled HAL state
};

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
