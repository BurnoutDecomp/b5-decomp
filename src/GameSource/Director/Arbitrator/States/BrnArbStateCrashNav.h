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
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x8226DC98
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x8224FAA0
        const char* GetName() const override;                        // @0x821F6300

        // Prepare() / Destruct() are NOT in this TU's X360 function set (Prepare is reached as a
        // virtual call -- (*(*this+4))(this, info) -- so its body lives in another driver TU; the
        // base declaration is kept, no override added here). The DWARF also lists IsActive(); it is
        // NOT in this TU's X360 function set either and is omitted.

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Released in Release() via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Release/Update asm:
        // mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10). The X360 re-resolves the live behaviour from the
        // manager pool through (helper word, allocation key); GetBehaviour() returns the cached
        // pointer to that same behaviour after asserting IsAllocated(). GetProducedCamera() returns
        // the camera the live behaviour produced this frame (the manager keeps it alongside the
        // behaviour pointer in the same pool slot -- the X360 reads it via sub_821FDC58).
        // FLAG: the +0x08 word's role is not fully recovered in this TU (modelled as an opaque
        //   behaviour-lookup helper index, as in BrnArbStateRoaming.h / BrnArbStateRaceIntro.h).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            // The live behaviour this handle owns (only valid while IsAllocated()). The X360
            // asserts IsAllocated() inside the manager-pool lookup before returning it.
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            // The camera the live behaviour produced this frame. The X360 reads it from the manager
            // pool slot the handle resolves to (sub_821FDC58), which is the same camera the
            // road-runner behaviour writes each frame -- modelled BY NAME as the behaviour's
            // produced camera. Asserts IsAllocated(). Defined out-of-line in the .cpp where
            // BehaviourRoadRunner is complete.
            const Camera::Camera& GetProducedCamera() const;

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // The interpolation curve/easing parameters block the in/out interpolators read. Opaque
        // slice (Construct seeds the leading words: [+0x00]=8, then 1/1 selectors). 0x10 bytes.
        // FLAG: field roles not recovered (mirrors BrnArbStateRoaming::InterpolateParameters).
        struct InterpolateParameters
        {
            u32 mauParams[4];   // +0x00..+0x0C  (Construct: [+0x00]=8; [+0x08]=1; [+0x0C]=1)
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        ICEPlayingMovie                               mICEPlayingMovie;   // X360 +0x180
        ICEMoviePlayer                                mICEMoviePlayer;    // X360 +0x190
        BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;      // X360 +0x840
        InterpolateParameters                         mInterpolateParams; // X360 +0x854
        BehaviourHandle<Camera::BehaviourRoadRunner>  mRoadRunnerCam;     // X360 +0x864
        EState                                        meState;            // X360 +0x878
        u32                                           muCurrentIceMovie;  // X360 +0x87C
        f32                                           mfTimeInState;      // X360 +0x880
        bool                                          mbHasReversed;      // X360 +0x884
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_NAV_H
