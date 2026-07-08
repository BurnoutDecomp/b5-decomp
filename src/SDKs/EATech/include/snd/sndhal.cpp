// ============================================================================
// SDKs/EATech/include/snd/sndhal.cpp
//
// Snd9::Hal channel volume/pan mixer bodies. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every offset,
// width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU.
//   Snd9::Hal::SetPan          @ 0x82B7F910
//   Snd9::Hal::SetVol          @ 0x82B7E5F0
//   Snd9::Hal::SetVolInternal  @ 0x82B7D228
//
// The `_savegprlr_29` / `_restgprlr_29` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so
// they are dropped. The per-channel tables (gpChannels / gpChannelPan /
// gpMixChannels) and the speaker/output counts are defined here as their owning home
// but are allocated/initialised by the Snd9 HAL setup path in its own TU.
// ============================================================================

#include "SDKs/EATech/include/snd/sndhal.h"

// The Snd9 HAL's azimuth->per-speaker-volume distributor (SNDI_ = Snd internal, a
// plain C free function). Its body lives in its own TU; declared here so SetPan's
// non-explicit-pan path can hand it the channel azimuth and the destination pan
// gains. FLAGGED vendor extern -- declared, not defined, here.
extern "C" void SNDI_aztospkrvol(u16 auPanPosition, Snd9::Hal::SpeakerVol* apDest);

namespace Snd9
{
namespace Hal
{

// Sample-level scale: level (0..127) maps to 0..1 (X360 flt_82156410, 1/127).
static const f32 KF_LEVEL_SCALE = 0.0078740157f;

// Owning definitions of the shared HAL tables/counts (see header). Null/zero until
// the HAL setup path installs them.
Channel*    gpChannels     = 0;
SpeakerVol* gpChannelPan    = 0;
u8          gucNumSpeakers  = 0;
MixChannel* gpMixChannels   = 0;
s32         giNumOutputs     = 0;

// ----------------------------------------------------------------------------
// Snd9::Hal::SetVolInternal @ 0x82B7D228
// ----------------------------------------------------------------------------
int SetVolInternal(int aiChannel)
{
    Channel& lrChannel = gpChannels[aiChannel];

    // (signed sample level 0..127) * channel volume, scaled by 1/127. The X360
    // routes the level through fcfid (int64->double) purely to convert the integer;
    // it is a plain float conversion.
    const f32 lfScaledVol =
        static_cast<f32>(lrChannel.mcLevel) * lrChannel.mfVolume * KF_LEVEL_SCALE;

    if (gucNumSpeakers)
    {
        SpeakerVol& lrPan = gpChannelPan[aiChannel];
        MixChannel& lrMix = gpMixChannels[aiChannel];
        for (int liSpeaker = 0; liSpeaker < gucNumSpeakers; ++liSpeaker)
        {
            lrMix.mafSpeakerGain[liSpeaker] = lrPan.mafGain[liSpeaker] * lfScaledVol;
            lrMix.mucDirty = 1;
        }
    }

    return aiChannel;
}

// ----------------------------------------------------------------------------
// Snd9::Hal::SetVol @ 0x82B7E5F0
// ----------------------------------------------------------------------------
int SetVol(int aiChannel)
{
    const int liChannel = SetVolInternal(aiChannel);

    if (giNumOutputs <= 0)
    {
        return liChannel;
    }

    Channel&    lrChannel = gpChannels[liChannel];
    MixChannel& lrMix     = gpMixChannels[liChannel];
    for (int liOutput = 0; liOutput < giNumOutputs; ++liOutput)
    {
        lrMix.mpOutputBuffer[liOutput] = lrChannel.mpSourceGains[liOutput] * lrChannel.mfVolume;
        lrMix.mucDirty = 1;
    }

    // X360: r3 is never reloaded after the mix-channel address setup, so the function
    // returns the dirty-byte offset (0x5C*channel + 1) left in it -- reproduced verbatim.
    return 0x5C * liChannel + 1;
}

// ----------------------------------------------------------------------------
// Snd9::Hal::SetPan @ 0x82B7F910
// ----------------------------------------------------------------------------
int SetPan(int aiChannel)
{
    Channel&    lrChannel = gpChannels[aiChannel];
    SpeakerVol& lrPan     = gpChannelPan[aiChannel];

    if (lrChannel.mucPanActive)
    {
        // Explicit pan: clear the active speaker gains and force the reference slot
        // (+0x14) to unity.
        for (int liSpeaker = 0; liSpeaker < gucNumSpeakers; ++liSpeaker)
        {
            lrPan.mafGain[liSpeaker] = 0.0f;
        }
        lrPan.mafGain[5] = 1.0f;
    }
    else
    {
        // Derive the per-speaker pan gains from the channel azimuth.
        SNDI_aztospkrvol(lrChannel.muPanPosition, &lrPan);
    }

    return SetVol(aiChannel);
}

} // namespace Hal
} // namespace Snd9
