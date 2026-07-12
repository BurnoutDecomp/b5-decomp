// ============================================================================
// SDKs/EATech/include/snd/sndaemssampleplayer.cpp
//
// Snd9::AemsStandardSamplePlayer bodies. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every offset, width and side-effect. No
// Feb-2007 source and no DecFIGS DWARF exist for this TU.
//   Snd9::AemsStandardSamplePlayer::AemsStandardSamplePlayer  @ 0x82B71C70
//   Snd9::AemsStandardSamplePlayer::Release                   @ 0x82B723C8
//   Snd9::AemsStandardSamplePlayer::Unpause                   @ 0x82B720D8
//   Snd9::AemsStandardSamplePlayer::SetInput                  @ 0x82B71D10
//   Snd9::AemsStandardSamplePlayer::SetAzimuth                @ 0x82B71E98
//
// The `_savegprlr_NN` / `_restgprlr_NN` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so
// they are dropped. off_83271928 is the shared rwaudio System singleton (same global
// reached by sndaems.cpp / PlugIn.cpp / Decoder.cpp), the allocator Release frees the
// player block back through.
//
// The Snd9::IAemsSamplePlayer control-surface methods are declared `void` (their base
// pure-virtuals return void); the Hex-Rays "int __fastcall ... return result" is the
// PPC r3 pass-through artefact and is dropped.
//
// NOT in this TU (see the header): ~AemsStandardSamplePlayer (BLOCKED -- mis-attributed
// AemsRWSamplePlayer thunk), Pause (own TU), GetOutputs (BLOCKED -- un-homed callee
// sub_82B6A8E0).
// ============================================================================

#include "SDKs/EATech/include/snd/sndaemssampleplayer.h"

#include "rw/audio/core/Voice.h"  // rw::audio::core::Voice::Release
#include "rw/audio/core/PlugIn.h" // rw::audio::core::PlugIn::SetAttribute, System::Free

// The shared rwaudio System singleton (off_83271928). Defined/owned by the System
// sub-system TU; declared here as the target of System::Free. Same declaration form
// sndaems.cpp / Decoder.cpp use.
extern "C" rw::audio::core::System* off_83271928;

namespace Snd9
{

// Input-value fixed-point scales (X360 rodata constants).
//   flt_820AA8F4: pitch-multiplier input scale (0.00024414062 == 1/4096).
static const f32 KF_PITCH_SCALE = 0.00024414062f;
//   flt_820AA8F8: level (vol/dry/wet) input scale (0.000030518509 == 1/32767, s16 norm).
static const f32 KF_LEVEL_SCALE = 0.000030518509f;
//   flt_820AD9B0: azimuth-unit scale applied to each (legacyAzimuth + delta) & 0xFFFF
//   before it is handed to the Pan2D stage (0.0054931641 == 180/32768).
static const f32 KF_AZIMUTH_SCALE = 0.0054931641f;

// ----------------------------------------------------------------------------
// Snd9::AemsStandardSamplePlayer::AemsStandardSamplePlayer @ 0x82B71C70
// ----------------------------------------------------------------------------
AemsStandardSamplePlayer::AemsStandardSamplePlayer()
{
    // The vtable store (X360: *this = off_8214B310) is emitted by the compiler as part
    // of constructing this polymorphic class; only the member init sequence is written.
    mpVoice     = 0;
    mpSndPlayer = 0;
    mpResample  = 0;
    mpHighPass  = 0;
    mpLowPass   = 0;
    mpSendWet   = 0;
    mpGain      = 0;
    for (u32 luVoice = 0; luVoice < KU_MAX_PANNER_VOICES; ++luVoice)
    {
        mpPannerVoice[luVoice] = 0;
        mpPan2D[luVoice]       = 0;
    }
    mfPitch          = 1.0f;
    mfVol            = 1.0f;
    mfDryLevel       = 1.0f;
    mfWetLevel       = 0.0f;
    mfStartThreshold = -1.0f;
    mdSampleLength   = 0.0;
    mPauseState      = 0;
    muVoiceBaseIndex = 0;
    muNumVoices      = 0;
}

// ----------------------------------------------------------------------------
// Snd9::AemsStandardSamplePlayer::Release @ 0x82B723C8
// ----------------------------------------------------------------------------
void AemsStandardSamplePlayer::Release()
{
    if (mpVoice)
    {
        rw::audio::core::Voice::Release(mpVoice);
    }

    // Release the per-channel panner voices only when more than one is present.
    if (muNumVoices > 1u)
    {
        for (u32 luVoice = 0; luVoice < muNumVoices; ++luVoice)
        {
            if (mpPannerVoice[luVoice])
            {
                rw::audio::core::Voice::Release(mpPannerVoice[luVoice]);
            }
        }
    }

    rw::audio::core::System::Free(off_83271928, this, 0);
}

// ----------------------------------------------------------------------------
// Snd9::AemsStandardSamplePlayer::Unpause @ 0x82B720D8
// ----------------------------------------------------------------------------
void AemsStandardSamplePlayer::Unpause()
{
    // Restore the dry (gain) and wet (send) levels scaled by the master volume, and the
    // pitch on the resample stage; then clear the paused flag. mpGain / mpResample are
    // assumed present (the asm writes them unconditionally); only mpSendWet is guarded.
    rw::audio::core::PlugIn::SetAttribute(mpGain, 0, mfDryLevel * mfVol);
    if (mpSendWet)
    {
        rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, mfWetLevel * mfVol);
    }
    rw::audio::core::PlugIn::SetAttribute(mpResample, 0, mfPitch);
    mPauseState = 0;
}

// ----------------------------------------------------------------------------
// Snd9::AemsStandardSamplePlayer::SetInput @ 0x82B71D10
// ----------------------------------------------------------------------------
void AemsStandardSamplePlayer::SetInput(InputSelector aeSelector, int aiValue)
{
    switch (aeSelector)
    {
    case PLAYER_INPUT_PITCHMULT: // 0 -> resample stage, 1/4096 scale (unconditional)
        rw::audio::core::PlugIn::SetAttribute(
            mpResample, 0, static_cast<f32>(aiValue) * KF_PITCH_SCALE);
        break;

    case PLAYER_INPUT_VOL: // 2 -> master volume; re-drive gain (dry) and send (wet)
        mfVol = static_cast<f32>(aiValue) * KF_LEVEL_SCALE;
        if (mpGain)
        {
            rw::audio::core::PlugIn::SetAttribute(mpGain, 0, mfDryLevel * mfVol);
        }
        if (mpSendWet)
        {
            rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, mfWetLevel * mfVol);
        }
        break;

    case PLAYER_INPUT_FXWET0: // 5 -> wet level; re-drive the send stage
        mfWetLevel = static_cast<f32>(aiValue) * KF_LEVEL_SCALE;
        if (mpSendWet)
        {
            rw::audio::core::PlugIn::SetAttribute(mpSendWet, 0, mfVol * mfWetLevel);
        }
        break;

    case PLAYER_INPUT_LOWPASS: // 6 -> low-pass stage, raw (unscaled) value
        if (mpLowPass)
        {
            rw::audio::core::PlugIn::SetAttribute(mpLowPass, 0, static_cast<f32>(aiValue));
        }
        break;

    case PLAYER_INPUT_HIGHPASS: // 7 -> high-pass stage, raw (unscaled) value
        if (mpHighPass)
        {
            rw::audio::core::PlugIn::SetAttribute(mpHighPass, 0, static_cast<f32>(aiValue));
        }
        break;

    case PLAYER_INPUT_DRYLEVEL: // 8 -> dry level; re-drive the gain stage
        mfDryLevel = static_cast<f32>(aiValue) * KF_LEVEL_SCALE;
        if (mpGain)
        {
            rw::audio::core::PlugIn::SetAttribute(mpGain, 0, mfVol * mfDryLevel);
        }
        break;

    default:
        // TIMEMULT (1), AZIMUTH (3), ELEVATION (4) and any user selector: no-op here.
        break;
    }
}

// ----------------------------------------------------------------------------
// Snd9::AemsStandardSamplePlayer::SetAzimuth @ 0x82B71E98
// ----------------------------------------------------------------------------
void AemsStandardSamplePlayer::SetAzimuth(int aiAzimuth, int* apLegacyAzimuths)
{
    // apLegacyAzimuths is an external (serialised) azimuth table walked with a
    // data-driven base: the first sample for this player sits at a 24-byte-strided
    // block offset (muVoiceBaseIndex), after which the per-voice azimuths are packed
    // consecutively (4-byte ints). Raw pointer walk over external data is intended.
    const int* lpAzimuth = reinterpret_cast<const int*>(
        reinterpret_cast<char*>(apLegacyAzimuths) + 24 * muVoiceBaseIndex - 24);

    for (u32 luVoice = 0; luVoice < muNumVoices; ++luVoice)
    {
        // (legacyAzimuth + delta) masked to 16 bits, scaled to the Pan2D azimuth unit.
        const u32 luAzimuth =
            static_cast<u32>(*lpAzimuth + aiAzimuth) & 0xFFFFu;
        rw::audio::core::PlugIn::SetAttribute(
            mpPan2D[luVoice], 0, static_cast<f32>(luAzimuth) * KF_AZIMUTH_SCALE);
        ++lpAzimuth;
    }
}

} // namespace Snd9
