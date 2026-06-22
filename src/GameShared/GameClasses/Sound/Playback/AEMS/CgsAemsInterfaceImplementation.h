#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H

#include "types.hpp"

#include "SDKs/EATech/include/snd/sndo.h" // Snd9::IAemsSamplePlayer (the base)

// ============================================================================
// GameShared/GameClasses/Sound/Playback/aems/CgsAemsInterfaceImplementation.h
//   (DWARF home)
//
// CgsSound::Playback::AemsRWSamplePlayer (CgsAemsInterfaceImplementation.h:102):
// the game's RenderWare-audio-backed implementation of Snd9::IAemsSamplePlayer. It
// wraps an rw::audio::core::Voice + its plug-in chain (snd-player / resample / gain /
// pan2D / send) and routes the Snd9 sample-player control surface (Pause/Unpause/
// SetInput/SetAzimuth/GetOutputs) onto them.
//
// The bodied function this TU owns is the compiler-synthesized scalar-deleting
// destructor @ 0x826A2E80:
//   *this = vtable;                  // off_820AB14C  (AemsRWSamplePlayer vtable)
//   if (a2 & 1) operator delete(this);
// The dtor touches NO data members -- every member is a trivially-destructible
// pointer / float / double / enum / byte, and the base ~IAemsSamplePlayer is empty.
// So the only work is the vtable store + the conditional deallocation. Defining the
// class destructor out-of-line (CgsAemsInterfaceImplementation.cpp) emits exactly
// that; its body is empty.
//
// FLAG (member-type collaborators are forward-declared / un-homed): Environment,
// System and AemsPlayerVoice are large engine types with their own DWARF homes
// (CgsEnvironment.h, CgsSystem.h, CgsAemsPlayerVoice.h); rw::audio::core::Voice and
// rw::audio::core::PlugIn are RenderWare types. The destructor reads none of them, so
// all are forward-declared here (incomplete types suffice for the pointer/reference
// members) and the full surfaces live in their own TUs.
//
// FLAG (method family is CROSS-TU): the ctor and the Release/Pause/Unpause/SetInput/
// SetAzimuth/GetOutputs overrides (CgsAemsInterfaceImplementation.cpp) live in their
// own (not-yet-done) TUs; they are DECLARED here (where they fix the vtable shape)
// and resolved at consolidation -- NOT defined.
//
// FLAG (DWARF lists members as `float32_t`): float32_t is NOT a project type; the
// project scalar f32 is used for those members (mafPreviousAzimuths/mfPrevious*).
// ============================================================================

namespace rw
{
namespace audio
{
namespace core
{
    class Voice;   // RenderWare audio voice (forward-declared; dtor reads nothing)
    class PlugIn;  // RenderWare audio plug-in (forward-declared)
}
}
}

namespace CgsSound
{
namespace Playback
{
    // Engine collaborators (full homes elsewhere). Forward-declared -- the dtor
    // reads none of them, so incomplete types suffice for the pointer/ref members.
    struct Environment;
    struct System;
    struct AemsPlayerVoice;

    // CgsAemsInterfaceImplementation.h:102 (DWARF):
    //   AemsRWSamplePlayer : public Snd9::IAemsSamplePlayer
    struct AemsRWSamplePlayer : public Snd9::IAemsSamplePlayer
    {
        // CgsAemsInterfaceImplementation.h:135. Pause latch state.
        enum PauseState
        {
            PAUSESTATE_UNPAUSED = 0,
            PAUSESTATE_PAUSED   = 1,
        };

        // CgsAemsInterfaceImplementation.h:141. Max sample channels.
        static const u8 KU_MAX_SAMPLE_CHANNELS = 6;

        // that TU (own TU).
        AemsRWSamplePlayer(Environment& arEnvironment, AemsPlayerVoice* apPlayerVoice);

        // CgsAemsInterfaceImplementation.h:102 @ 0x826A2E80 (scalar-deleting dtor).
        virtual ~AemsRWSamplePlayer();

        // Snd9::IAemsSamplePlayer control surface (own TUs; declared here).
        virtual void Release();                                       // that TU
        virtual void Pause();                                         // that TU
        virtual void Unpause();                                       // that TU
        virtual void SetInput(Snd9::IAemsSamplePlayer::InputSelector aeSelector, int aiValue); // that TU
        virtual void SetAzimuth(int aiAzimuth, int* apLegacyAzimuths); // that TU
        virtual void GetOutputs(int aiNumOutputs, int* apValues);      // that TU

    private:
        // Layout faithful to DWARF (CgsAemsInterfaceImplementation.h:143..174).
        const Environment&        mEnvironment;        // :143
        AemsPlayerVoice*          mpPlayerVoice;        // :144
        AemsRWSamplePlayer*       mpNext;               // :145
        System*                   mpRwacSystem;         // :147
        rw::audio::core::Voice*   mpVoice;              // :150
        rw::audio::core::PlugIn*  mpSndPlayer;          // :151
        rw::audio::core::PlugIn*  mpResample;           // :152
        rw::audio::core::PlugIn*  mpSendWet;            // :153
        rw::audio::core::PlugIn*  mpGain;               // :154
        rw::audio::core::Voice*   mpPannerVoice[5];     // :158
        rw::audio::core::PlugIn*  mpPan2D[5];           // :159
        f32                       mafPreviousAzimuths[5]; // :160
        f32                       mfPreviousPitch;      // :162
        f32                       mfPreviousDry;        // :163
        f32                       mfPreviousWet;        // :164
        f32                       mPitch;               // :166
        f32                       mVol;                 // :167
        f32                       mDryLevel;            // :168
        f32                       mWetLevel;            // :169
        f32                       mRequestHandle;       // :170
        double                    mSampleLength;        // :171
        PauseState                mPauseState;          // :172
        u8                        mNumChannels;         // :173
        u8                        mNumPannerVoices;     // :174
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H
