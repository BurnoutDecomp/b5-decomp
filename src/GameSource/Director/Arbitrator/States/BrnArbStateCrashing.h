#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASHING_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASHING_H

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                  // EActiveRaceCarIndex
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"    // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"               // Camera::BehaviourHandle<>
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.h" // the two impact controllers (by value)
#include "GameSource/Director/MomentController/BrnMomentSelector.h"       // MomentSelector (by value)
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"          // VehicleTracker::ECrashType

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCrashing.h
//
// ⭐⭐⭐ BrnDirector::ArbStateCrashing -- THE CRASH CAMERA. This is the arbitrator state the
// director enters the instant the player's car starts crashing, and it owns the whole
// presentation: the moment-selected crash camera, the "Crash"/"Wrecked" screen effect, the
// spiralling deathcam when the player has been totalled, the takedown ICE cam when the player
// has been taken down -- and the IMPACT SLOW MOTION.
//
// ⛔ IT IS NOT ArbStateCrashMode. Crash mode (container EState 4) is only ever reached FROM
// here, by this state's own ProcessPossibleStateChanges ("Switching to crash mode"), and only
// once the intro has asked for it. ArbStateRoaming's crash edge TESTS crash mode's CanRun and
// then enters E_STATE_CRASHING -- this state (BrnArbStateRoaming.cpp:912-919).
//
// ⛔⛔ WHY THIS FILE DID NOT EXIST UNTIL NOW, AND WHY IT COULD NOT LAND ALONE.
// E_STATE_CRASHING's container slot was `class ArbStateCrashing : public ArbitratorState {};` --
// an empty placeholder whose inherited Update does nothing and never writes meState. Nothing
// wrote GameState::mbCrashActive either, so the arbitrator could not even attempt the edge and
// the shell was never exercised. Landing the mbCrashActive writer WITHOUT this class would have
// handed the first crash of the session to that shell: no Update, no state change, no exit edge
// -- a camera frozen for the rest of the session, out of a green build. The two land together.
//
// ----------------------------------------------------------------------------
// SOURCE OF TRUTH. Member NAMES, TYPES and DECLARATION ORDER are the DecFIGS DWARF for
// BrnArbStateCrashing.h (:105..:129), X360-attested -- every one of the nine functions below is
// in the ARTIST ledger. The per-member CONSOLE offsets in the right column are pinned from the
// ARTIST asm (Construct @0x82259EA0, Release @0x82234F70, ApplySlomoAndShake @0x8224F8D8,
// Update @0x8226BFB0) and are PROVENANCE ONLY: on the x64 host the embedded Camera / handles /
// MomentSelector widen, so absolute offsets shift. Parity is BY NAMED MEMBER, as everywhere.
//
// ⚠️ Hex-Rays' `field_3A8 / field_3A9 / field_3AA / field_390 / field_398 / field_38C` are NOT
// members of this class -- they are mMomentSelector's own +0x1E0 / +0x1E1 / +0x1E2 / +0x1C8 /
// +0x1D0 / +0x1C4, i.e. mbHasSelectedMoment / mbPrepared / mbHasMaxLimit / miFramesActive /
// muValidMoments / mfTimeActive. Reading them as state members is how a previous transcription
// grew phantom fields.
// ============================================================================

namespace BrnDirector
{
    // The two camera behaviours this state allocates. Referenced only as BehaviourHandle<>
    // template arguments here (pointer members), so the forward declarations suffice; the
    // .cpp includes their real homes to call through them.
    namespace Camera { class BehaviourSpirallingDeathcam; class BehaviourIceAnim; }

    // Reference-only in the two de-inlined helpers below; the .cpp includes their real homes.
    struct GameState;
    class  ArbitratorStateContainer;

    class ArbStateCrashing : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateCrashing.h:91). Values are the X360 immediates: Construct
        // seeds 0, Prepare writes 1, Update's ACTIVE arm writes 2, ProcessPossibleStateChanges
        // writes 3 / 4 / 6, and the AFTERCRASH arm writes 5.
        //
        // ⭐ THE LADDER OUT, so nobody has to re-derive it from the switch: a crash runs
        //   PREPARING -> ACTIVE, and when GameState::mbCrashActive drops,
        //   ProcessPossibleStateChanges picks the exit --
        //     wrecked                        -> CHANGING_TO_ROAMING (with "BlackFadeIn_Quick")
        //     low-energy crash on a hard-stop moment -> AFTERCRASH_SLOW (hold the moment camera)
        //     otherwise                      -> AFTERCRASH (0.5 s on the gameplay camera)
        //   and AFTERCRASH -> INTERPOLATING_TO_ROAMING -> CHANGING_TO_ROAMING -> Release().
        //   INTERPOLATING_TO_ROAMING and CHANGING_TO_ROAMING both try ArbStateRoaming::Prepare
        //   and hand the container over when it succeeds.
        enum EState
        {
            E_STATE_INACTIVE                 = 0,
            E_STATE_PREPARING                = 1,
            E_STATE_ACTIVE                   = 2,
            E_STATE_AFTERCRASH               = 3,
            E_STATE_AFTERCRASH_SLOW          = 4,
            E_STATE_INTERPOLATING_TO_ROAMING = 5,
            E_STATE_CHANGING_TO_ROAMING      = 6,
            E_STATE_RELEASING                = 7,

            E_NUM_STATES                     = 8
        };

        // ---- ArbitratorState virtual overrides (base vtable order; DO NOT REORDER) --------
        void        Construct() override;                                 // @0x82259EA0
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;   // @0x822655E8
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;    // @0x8226BFB0
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;   // @0x82234F70
        bool        CanRun(ArbStateSharedInfo& lrSharedInfo) const override; // @0x821F6258
        const char* GetName() const override;                             // @0x821F6248

        // Destruct is in the DWARF (:735) but has NO X360 export in this build's ledger, so the
        // base declaration stands and no override is added here (the same rule the sibling
        // states follow for their un-exported slots).

    private:
        // @0x82254FB0. Pick / re-pick the moment camera for this frame and copy it into
        // mCamera, or fall back to the gameplay camera when no moment is selected. The bool is
        // "the crash is nearly over, do not cut cameras now" -- Update passes
        // `lrGameState.mfCrashTimeRemaining <= 1.0f`.
        void SelectNormalCrashCamera(ArbStateSharedInfo& lrSharedInfo, bool lbTooLateToSwitchCameras);

        // @0x8224F5B0. Decide whether this frame leaves the crashing state, and say so through
        // the out-param so Update knows not to keep driving a state it has just left.
        void ProcessPossibleStateChanges(ArbStateSharedInfo& lrSharedInfo, bool* lpbHasChangedState);

        // ⭐ @0x8224F8D8. THE SLOW MOTION AND THE SHAKE. Drives the two impact controllers, or
        // resets them when the selected moment / crash type says this crash gets neither.
        void ApplySlomoAndShake(ArbStateSharedInfo& lrSharedInfo);

        // ---- de-inlined blocks of Update, NOT DWARF members ------------------------------
        // Neither of these is a function in the source build; both are blocks of Update's own
        // body that the console reaches from more than one place, and naming them is the
        // de-`goto`-ing the project's conventions ask for. They are private, so nothing outside
        // this class can mistake them for part of the state's interface.
        //
        //   UpdateActive     -- the ACTIVE arm. Update reaches it twice: as switch case 2, and
        //                       by falling out of case 1 on the frame Prepare succeeds (the
        //                       console's `goto LABEL_9`, @0x8226C0B8 -> loc_8226C0D8).
        //   ChangeToRoaming  -- the "ask ArbStateRoaming to take over, and Release if it does"
        //                       edge, emitted identically at three sites (cases 4 / 5 / 6).
        //                       ⚠️ Deliberately NOT routed through ArbUtils::ChangeToState even
        //                       though the shape matches: that template also fires a CanRun
        //                       tripwire, and these three sites do not.
        void UpdateActive(ArbStateSharedInfo& lrSharedInfo, GameState& lrGameState);
        bool ChangeToRoaming(ArbStateSharedInfo& lrSharedInfo,
                             ArbitratorStateContainer& lrContainer);

        // ---- DWARF member list, in declaration order (BrnArbStateCrashing.h:105..:129) ----
        Camera::BehaviourHandle<Camera::BehaviourSpirallingDeathcam> mDeathcam;      // :105  +0x180
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mTakenDownCam;  // :107  +0x194

        Camera::ImpactSlomoController mImpactSlomoController;   // :109  +0x1A8 (12 bytes)
        Camera::ImpactShakeController mImpactShakeController;   // :110  +0x1B4 (20 bytes)

        MomentSelector mMomentSelector;                         // :112  +0x1C8 (0x1E4)

        VehicleTracker::ECrashType meCrashType;                 // :114  +0x3AC (Prepare latches the tracker's)

        f32  mfTimeActive;                                      // :116  +0x3B0
        f32  mfMomentTimer;                                     // :117  +0x3B4
        f32  mfFailsafeTimer;                                   // :118  +0x3B8

        bool mbAftertouchingSinceStart;                         // :120  +0x3BC
        bool mbAftertouchActivated;                             // :121  +0x3BD

        bool mbShouldUseDeathcam;                               // :123  +0x3BE
        bool mbPlayerWasWreckedThisCrash;                       // :124  +0x3BF

        bool mbPlayerWasTakenDown;                              // :126  +0x3C0
        EActiveRaceCarIndex mePlayerKillerIndex;                // :127  +0x3C4

        EState meState;                                         // :129  +0x3C8
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASHING_H
