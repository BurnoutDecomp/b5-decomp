#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_NAV_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_NAV_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/BrnDirectorICEWrapper.h"                    // ICEPlayingMovie (mICEPlayingMovie by value)
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                  // ICEMoviePlayer (by value) / Camera::Behaviour* layer

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h
//
// BrnDirector::ArbStateCrashNav -- the director arbitrator state that runs the "crash nav"
// (picture-paradise) fly-by camera: after a crash the camera flies the road-runner take from the
// crash site, optionally turns about (ACTIVE_TURNABOUT) to show the way back, and fades in/out of
// black between segments. It plays an ICE movie alongside the fly-by and hands control back to the
// gameplay camera once the take has finished (or the game stops requesting crash-nav). Derives from
// ArbitratorState (vtable order pinned by the base); the heavy work lives in Update().
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateCrashNav.h, X360-attested for this build). The per-member X360 offsets are pinned
// from the ARTIST asm (Construct @0x82266010, Update @0x8226DC98, Release @0x8224FAA0):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by name
//     through the base GetNonConstCamera() / the effect-trigger free functions; Construct calls
//     Camera::Construct(this+0x10)).
//   * mICEPlayingMovie @+0x180 (ICEPlayingMovie, 0x10) -- the take id / playback pos / valid flag
//     the fly-by re-plays (the X360 reads its mID @+0x180, mfPlaybackPositionParameter @+0x188,
//     mbIsValid @+0x18C in Update's PlayMovie path).
//   * mICEMoviePlayer  @+0x190 (ICEMoviePlayer, by value) -- the ICE movie player driven each frame
//     (Construct calls ICEMoviePlayer::Construct(this+0x190); its mbIsPlaying byte lands at +0x830,
//     mbHasReachedEnd at +0x82E).
//   * mInterpolater    @+0x840 (BehaviourHandle, 0x14) -- the in/out interpolation cam handle
//   * mInterpolateParams @+0x854 (BehaviourInterpolate::Parameters, 0x10) -- the interp params block
//   * mRoadRunnerCam   @+0x864 (BehaviourHandle, 0x14) -- the road-runner fly-by cam handle (the
//     one Update actively drives: resolves the behaviour for HasFinished()/the fade selector/the
//     turnabout, and copies its produced camera)
//   * meState          @+0x878 (EState, the crash-nav state machine value; the switch dispatch key)
//   * muCurrentIceMovie@+0x87C (uint32; DWARF -- the current movie index; not read in these 4 fns)
//   * mfTimeInState    @+0x880 (f32, accumulated every Update tail by mfTimestep)
//   * mbHasReversed    @+0x884 (bool, latched once the ACTIVE_TURNABOUT has flipped the take)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets quoted
// above are provenance; on the x64 host the embedded Camera / ICEMoviePlayer / handles widen, so
// absolute offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourInterpolate; class BehaviourRoadRunner; }

    class ArbStateCrashNav : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateCrashNav.h:70). The crash-nav state machine. Construct seeds 0
        // (INACTIVE); the switch dispatches on this value. The X360 only walks cases 0/1/3/4/6 in
        // Update (cases 2/5/7 fall through to the default "unhandled state" assert -- they are
        // entered by the other crash-nav driver TUs). Values are the X360 jump-table case indices /
        // the immediates the asm stores into meState.
        enum EState
        {
            E_STATE_INACTIVE                 = 0,
            E_STATE_PREPARING                = 1,
            E_STATE_INTERPOLATE_FROM_GAMEPLAY= 2,
            E_STATE_ACTIVE                   = 3,
            E_STATE_ACTIVE_TURNABOUT         = 4,
            E_STATE_INTERPOLATE_TO_GAMEPLAY  = 5,
            E_STATE_WAITING_TO_STOP          = 6,
            E_STATE_RELEASING                = 7,

            E_NUM_STATES                     = 8
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                            // @0x82266010
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x822660A8
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x8226DC98
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x8224FAA0
        const char* GetName() const override;                        // @0x821F6300

        // ⛔ CORRECTED 2026-08-29 (pause-greyscale wave). The note here used to read "Prepare()
        // is NOT in this TU's X360 function set ... its body lives in another driver TU". That
        // was wrong: BrnDirector::ArbStateCrashNav::Prepare IS a named ARTIST symbol, at
        // 0x822660A8, and it is the ONLY caller of ICEMoviePlayer::Prepare / Loop /
        // InterpolateFrom in the entire image -- i.e. it is the single entry point of the
        // moving pause camera. It is now overridden and bodied in the .cpp.
        //
        // Destruct() is genuinely absent from this TU's X360 function set (base declaration
        // kept, no override). The DWARF also lists IsActive(); likewise absent, and omitted.

    private:
        // ⭐⭐ THE NESTED HANDLE / PARAMS FORKS ARE RETIRED (2026-08-29, pause-greyscale wave).
        //
        // This state used to declare its OWN five-word `BehaviourHandle<TBehaviour>` and its own
        // opaque `InterpolateParameters { u32 mauParams[4] }`. Both had real homes already:
        //   * Camera::BehaviourHandle<TBehaviour>       -- GameSource/Director/Camera/BrnBehaviourManager.h
        //   * Camera::BehaviourInterpolate::Parameters  -- .../Camera/Behaviours/BrnBehaviourInterpolate.h
        //
        // ⛔ The fork was not merely untidy, it was LOAD-BEARING AGAINST US, and only a LINK would
        //    have found it. BehaviourManager declares TWO NewBehaviour<> overloads: a generic
        //    `template<TBehaviour, THandle> NewBehaviour(THandle&, ...)` that is DECLARATION-ONLY,
        //    and the BODIED `NewBehaviour(BehaviourHandle<TBehaviour>&, ...)`. A nested fork binds
        //    the generic one -- so it COMPILES, and then leaves an unresolved external, with the
        //    behaviour never allocated. Prepare @0x822660A8 allocates the road-runner fly-by
        //    through exactly that call, so with the fork in place this state could never have
        //    driven a camera at all.
        //
        // The two homes carry the same asm-attested five-word layout
        // (mbAllocated +0x00 / muAllocationKey +0x04 / mpHelperPool +0x08 / mpManager +0x0C /
        // mpBehaviour +0x10) and additionally IDENTIFY the +0x08 word this file used to FLAG as
        // "role not recovered": it is the owning BehaviourHelper POOL pointer, pinned in
        // BrnBehaviourManager.h from NewBehaviour<BehaviourRoadRunner> @0x822580F8.

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        ICEPlayingMovie                                       mICEPlayingMovie;   // X360 +0x180
        ICEMoviePlayer                                        mICEMoviePlayer;    // X360 +0x190
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>  mInterpolater;      // X360 +0x840
        Camera::BehaviourInterpolate::Parameters               mInterpolateParams; // X360 +0x854
        Camera::BehaviourHandle<Camera::BehaviourRoadRunner>   mRoadRunnerCam;     // X360 +0x864
        EState                                                meState;            // X360 +0x878
        u32                                                   muCurrentIceMovie;  // X360 +0x87C
        f32                                                   mfTimeInState;      // X360 +0x880
        bool                                                  mbHasReversed;      // X360 +0x884
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_NAV_H
