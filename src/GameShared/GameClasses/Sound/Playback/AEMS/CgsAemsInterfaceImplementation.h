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
    class System;  // RenderWare audio system (the Playback::System typedef target)
}
}
}

namespace CgsSound
{
namespace Playback
{
    // Engine collaborators (full homes elsewhere). Forward-declared -- the dtor
    // reads none of them, so incomplete types suffice for the pointer/ref members.
    // (Playback::System is the CgsVoice.h:59 TYPEDEF of rw::audio::core::System --
    // a `struct System;` fwd decl here collides with it the moment both are in one
    // TU (C2371, cascade slice 2), so the rw class is forward-declared above and
    // the typedef spelled locally.)
    struct Environment;
    typedef rw::audio::core::System System;
    struct AemsPlayerVoice;

    // CgsAemsInterfaceImplementation.h (DWARF). A plug-in config slot the
    // AemsRWSampleFactory holds three of, by value (console 12 bytes: ptr @+0,
    // handle @+4, count byte @+8). TYPED (AEMS-cascade slice 2) from the
    // AemsRWSampleFactory ctor's store triples (@0x826C26B8 orders 17-25: word+0
    // = an initial-value pointer or 0, word+4 = a plug-in handle, byte+8 = the
    // channel count) -- the same triple shape the SpliceManager voice helpers
    // build on the stack (Voice::CreateInstance's stage-config records).
    struct PlugInConfig
    {
        void*     mpInitialValue;    // console +0 (stack-float pointer or 0)
        uintptr_t muPlugInHandle;    // console +4 (a PlugInRegistry handle -- a 4-byte
                                     // node word on console, host-width here: the host
                                     // GetPlugInHandle returns a pointer-sized handle)
        u8        mu8ChannelCount;   // console +8
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
        friend struct AemsPlayerVoice;
        friend struct AemsRWSampleFactory;
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
    // CgsAemsInterfaceImplementation.h:48 (DWARF). The AEMS sample-player factory.
    // ⭐ MI CORRECTED (AEMS-cascade slice 2): the DWARF renders ONE base
    // (`: public Factory`) -- the known dwarfdump single-base limitation (the
    // SplicerPlayerVoice wave-6 lesson: zero MI renderings corpus-wide; MI must
    // be proven from the asm). The X360 ctor @0x826C26B8 PROVES the dual base:
    // the provisional IAemsSamplePlayerFactory vptr (off_820AB168) stores at
    // overall +0x00 BEFORE the Factory base constructs at overall +0x04, both
    // final vptrs restore (+0x00 off_820B2F84 / +0x04 off_820B2F70), and the
    // creates increment the refcount at overall +0x08 == Factory-subobject +0x04.
    // Environment::AddFactory receives overallThis+4 (the Factory subobject).
    //
    // All members are trivially destructible (a PlugInConfig[3] block, an rw
    // PlugInRegistry*, and eight handle words), so the class destructor body is
    // empty; the Factory base dtor does the teardown. Members pinned BY NAME +
    // SEQUENCE (host-width FLAG); console offsets in the comments.
    // ========================================================================
    struct AemsRWSampleFactory : public Snd9::IAemsSamplePlayerFactory, public Factory
    {
        // @ 0x826C26B8 (AEMS-cascade slice 2; full store-order decode in
        // progress/scratch_dossiers/aems_factory_cascade_codex.md). Body in
        // CgsAemsInterfaceImplementation.cpp.
        AemsRWSampleFactory(Name aName, Environment& arEnvironment);

        // CgsAemsInterfaceImplementation.cpp:460 (own TUs; declared for vtable shape).
        virtual Snd9::IAemsSamplePlayer* CreateInstance(void* apParams, int aiNumOutputs,
                                                        const int* apOutputs, const char* apcName,
                                                        int aiValue,
                                                        const Snd9::AemsPlayerInputAccessor* apAccessor);
        virtual void Release();                                        // own TU

        // CgsAemsInterfaceImplementation.h:48 @ 0x826C2848 (scalar-deleting dtor).
        virtual ~AemsRWSampleFactory();

    protected:
        // Layout faithful to DWARF (CgsAemsInterfaceImplementation.h:84..95).
        // Handle words host-widened (console 4-byte PlugInRegistry::PlugInHandle
        // node words; the host GetPlugInHandle returns pointer-sized handles) --
        // by-name access only, the AEMS-cascade slice-2 convention.
        PlugInConfig                     maAemsSubMixPlugInConfig[3]; // :84  (console +0x14/+0x20/+0x2C)
        rw::audio::core::PlugInRegistry* mpPlugInRegistry;            // :87  (console +0x38)
        uintptr_t mGainHandle;      // :88  (console +0x3C)
        uintptr_t mPan2DHandle;     // :89  (console +0x40)
        uintptr_t mRouteHandle;     // :90  (console +0x44)
        uintptr_t mSendHandle;      // :91  (console +0x48, 'Sen0' lookup)
        uintptr_t mSndPlayer1Handle;// :92  (console +0x4C)
        uintptr_t mRechannelHandle; // :93  (console +0x50)
        uintptr_t mResampleHandle;  // :94  (console +0x54)
        uintptr_t mSubMixHandle;    // :95  (console +0x58, 'Sub0' lookup)
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSINTERFACEIMPLEMENTATION_H
