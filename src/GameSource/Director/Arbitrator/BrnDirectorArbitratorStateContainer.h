#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_CONTAINER_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_CONTAINER_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"  // BrnDirector::ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"   // BrnDirector::ArbStateRoaming (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h" // BrnDirector::ArbStateCrashMode (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h" // BrnDirector::ArbStateRaceIntro (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"    // BrnDirector::ArbStateRankUp (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h" // BrnDirector::ArbStateOnlineCarSelect (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h" // BrnDirector::ArbStateOnlineRaceIntro (real layout)
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h" // BrnDirector::ArbStatePostEvent (real layout)
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                // BrnDirector::SharedPlaylists

// ----------------------------------------------------------------------------
// BrnDirector::ArbitratorStateContainer -- the fixed set of 11 director arbitrator
// states, each embedded BY VALUE, plus an 11-entry pointer table (one per EState)
// and the current-state pointer. ConstructAll seeds the table with the address of
// each embedded state, runs each state's Construct, and asserts the table holds no
// nulls / no duplicates. UpdateAll/ReleaseAll dispatch the corresponding virtual
// through the table.
//
// Layout/member order recovered from the DecFIGS DWARF
// (BrnDirectorArbitratorStateContainer.h) and the X360 pseudocode/asm
// (ConstructAll @0x8224F020, UpdateAll @0x821F5DC0, ReleaseAll @0x821F5EA0).
//
// LAYOUT NOTE: the source build embeds each state at a fixed absolute offset (the
// states are full-sized objects there). Our rebuilt state/SharedPlaylists sizes
// differ, so members are declared in their recorded ORDER and parity is BY NAME, not
// by reproducing absolute offsets (same convention as BrnICEMoviePlayer.h).
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // The 11 concrete arbitrator states. Each is a distinct ArbitratorState subtype;
    // their Construct/Update/Release overrides and per-state members live in their own
    // TUs. MINIMAL PLACEHOLDER HOMES: declared here as empty ArbitratorState subclasses
    // purely so the container can embed them by value and dispatch their virtuals --
    // GROW each into its real layout (additively) when its own TU lands. (Parity is by
    // name; the container does not touch any per-state member.)
    //
    // ArbStateRoaming, ArbStateCrashMode, ArbStateRaceIntro, ArbStateRankUp,
    // ArbStateOnlineCarSelect, ArbStateOnlineRaceIntro and ArbStatePostEvent now have their
    // real layouts (BrnArbStateRoaming.h / BrnArbStateCrashMode.h / BrnArbStateRaceIntro.h /
    // BrnArbStateRankUp.h / BrnArbStateOnlineCarSelect.h / BrnArbStateOnlineRaceIntro.h /
    // BrnArbStatePostEvent.h, #included above) -- de-forked from the placeholders so each TU and
    // the container share one definition.
    class ArbStateCrashing        : public ArbitratorState {};
    class ArbStateTakedown        : public ArbitratorState {};
    class ArbStateDriveThru       : public ArbitratorState {};
    class ArbStateCarSelect       : public ArbitratorState {};

    class ArbitratorStateContainer
    {
    public:
        // The per-EState index into mArrayOfStatePointers (DWARF
        // BrnDirectorArbitratorStateContainer.h:65).
        enum EState
        {
            E_STATE_DRIVETHRU          = 0,
            E_STATE_ROAMING            = 1,
            E_STATE_CRASHING           = 2,
            E_STATE_TAKEDOWN           = 3,
            E_STATE_CRASH_MODE         = 4,
            E_STATE_POST_EVENT         = 5,
            E_STATE_RACE_INTRO         = 6,
            E_STATE_ONLINE_RACE_INTRO  = 7,
            E_STATE_CAR_SELECT         = 8,
            E_STATE_RANK_UP            = 9,
            E_STATE_ONLINE_CAR_SELECT  = 10,
            E_NUM_STATES               = 11
        };

        // X360 0x827E2D18. The constructor default-constructs each embedded state and
        // the shared playlists (the source build's synthesized ctor writes each embedded
        // ArbitratorState subobject's vtable pointer + initialises its members to the -1
        // sentinel at the subobject's interior offsets, and runs SharedPlaylists' ctor).
        // By-name parity: each ArbStateXxx member runs ArbitratorState() and mSharedPlaylists
        // default-constructs; the pointer table / mpCurrentState are seeded later by
        // ConstructAll (the source ctor leaves them untouched too).
        ArbitratorStateContainer();

        void ConstructAll();
        void UpdateAll(ArbStateSharedInfo& lrSharedInfo);
        void ReleaseAll(ArbStateSharedInfo& lrSharedInfo);

        // X360 0x821F5A60. Returns the active state, asserting it is non-null
        // ("mpCurrentState != NULL"). mpCurrentState is the source build's +0x35CC word.
        ArbitratorState* GetCurrentState() const;

        // The state registered for a given EState index. The X360 reaches the embedded
        // sub-states by their fixed interior offsets (e.g. container+0x1C60 == the crash-mode
        // state) when a transition needs to query/hand-off a sibling; here that is the pointer
        // table indexed by EState (seeded by ConstructAll). Declaration-only -- body forwards
        // to mArrayOfStatePointers[leState].
        ArbitratorState* GetState(EState leState) const;

        // Make the given EState the active state (mpCurrentState = mArrayOfStatePointers[leState]
        // == the source build's +0x35CC word store from the +0x35A0 pointer table). A state's
        // Update inlines this after successfully Prepare()-ing the destination sibling, to hand
        // the frame over (BrnArbStateCrashMode::Update's ACTIVE->post-event and
        // CHANGING_TO_ROAMING->roaming edges). Declaration-only -- body forwards to the table.
        void SetCurrentState(EState leState);

    private:
        // Embedded BY VALUE, in the DWARF member order (which is the source build's
        // memory order). The pointer table indexes these by EState, NOT by declaration
        // order (e.g. DriveThru is EState 0 but is declared near the end).
        SharedPlaylists       mSharedPlaylists;

        ArbStateRoaming       mArbStateRoaming;
        ArbStateCrashing      mArbStateCrashing;
        ArbStateTakedown      mArbStateTakedown;
        ArbStateCrashMode     mArbStateCrashMode;
        ArbStatePostEvent     mArbStatePostEvent;
        ArbStateRaceIntro     mArbStateRaceIntro;
        ArbStateOnlineRaceIntro mArbStateOnlineRaceIntro;
        ArbStateDriveThru     mArbStateDriveThru;
        ArbStateCarSelect     mArbStateCarSelect;
        ArbStateRankUp        mArbStateRankUp;
        ArbStateOnlineCarSelect mArbStateOnlineCarSelect;

        ArbitratorState*      mArrayOfStatePointers[E_NUM_STATES];
        ArbitratorState*      mpCurrentState;
    };
}

#endif
