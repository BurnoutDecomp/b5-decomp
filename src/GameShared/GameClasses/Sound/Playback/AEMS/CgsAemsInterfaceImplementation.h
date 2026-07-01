#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H

#include "types.hpp"

#include "SDKs/EATech/include/snd/sndo.h" // Snd9::IAemsSamplePlayer (the base)
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h" // Factory (AemsRWSampleFactory base)

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
    class PlugInRegistry;  // RenderWare plug-in registry (AemsRWSampleFactory member)
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
    struct AemsPlayerInputAccessor;   // AemsRWSampleFactory::CreateInstance arg (ptr).

    // CgsAemsInterfaceImplementation.h (DWARF). A 12-byte plug-in config slot the
    // AemsRWSampleFactory holds three of, by value. Modelled as an opaque 12-byte
    // block (its fields are not read by the destructor TU). FLAG: minimal by-value
    // member -- full layout DEFERRED to the PlugInConfig home.
    struct PlugInConfig
    {
        u8 mau8Opaque[12];
    };

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

    // ========================================================================
    // CgsAemsInterfaceImplementation.h:48 (DWARF). AemsRWSampleFactory : public
    // Factory -- the AEMS sample-player factory. Added for the Wave-6 scalar-deleting
    // destructor TU (@0x826C2848). All members are trivially destructible (a
    // PlugInConfig[3] block, an rw PlugInRegistry*, and nine PlugInHandle words), so
    // the class destructor body is empty; the Factory base dtor does the teardown.
    // PlugInConfig / rw::audio::core::PlugInRegistry are engine collaborators with
    // their own homes -- forward-declared here (the dtor touches none of them).
    // Members pinned BY NAME + SEQUENCE (host-width FLAG).
    // ========================================================================
    struct AemsRWSampleFactory : public Factory
    {
        AemsRWSampleFactory(Name aName, Environment& arEnvironment);   // own TU

        // CgsAemsInterfaceImplementation.cpp:460 (own TUs; declared for vtable shape).
        virtual Snd9::IAemsSamplePlayer* CreateInstance(void* apParams, int aiNumOutputs,
                                                        const int* apOutputs, const char* apcName,
                                                        int aiValue,
                                                        const AemsPlayerInputAccessor* apAccessor);
        virtual void Release();                                        // own TU

        // CgsAemsInterfaceImplementation.h:48 @ 0x826C2848 (scalar-deleting dtor).
        virtual ~AemsRWSampleFactory();

    private:
        // Layout faithful to DWARF (CgsAemsInterfaceImplementation.h:84..95).
        PlugInConfig                     maAemsSubMixPlugInConfig[3]; // :84
        rw::audio::core::PlugInRegistry* mpPlugInRegistry;            // :87
        u32 mGainHandle;      // :88  PlugInRegistry::PlugInHandle
        u32 mPan2DHandle;     // :89
        u32 mRouteHandle;     // :90
        u32 mSendHandle;      // :91
        u32 mSndPlayer1Handle;// :92
        u32 mRechannelHandle; // :93
        u32 mResampleHandle;  // :94
        u32 mSubMixHandle;    // :95
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H
