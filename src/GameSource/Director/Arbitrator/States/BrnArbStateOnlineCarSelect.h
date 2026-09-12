#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h
//
// BrnDirector::ArbStateOnlineCarSelect -- the director arbitrator state that runs the camera
// for the ONLINE car-select / livery screen (the pre-race lobby where each player picks their
// car/livery before an online race intro). It owns two camera behaviours -- an ICE-anim
// "reveal" cam (the black-and-white car reveal) and a "rotate about vehicle" cam (the look-
// around-car / car-mod screen orbit) -- and walks a state machine
// INACTIVE -> PREPARING -> SELECTING_CAR -> SELECTING_LIVERY -> WAIT_TO_CHANGE_TO_INTRO ->
// CHANGING_TO_INTRO / CHANGING_TO_ROAMING, fading the screen in/out (the "Black_Out_BW" /
// "Black_In_BW" / "Black_*(No_B_W)" / "BlackFadeIn_Quick" camera-PFX hooks) and handing
// control to the online race-intro state once the player is ready (or back to roaming if the
// car-select is aborted). Derives from ArbitratorState (vtable order pinned by the base).
//
// LAYOUT: the member names and declaration order are the shipped build's; the per-member
// console offsets quoted below are pinned from Construct / Prepare / Update / Release:
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by
//     name through the base GetNonConstCamera() / the effect-trigger free functions;
//     Construct calls Camera::Construct(this+0x10)).
//   * mIceCam          @+0x180 (BehaviourHandle, 0x14) -- the ICE-anim reveal cam handle
//                       (Construct zeroes its five words: mbAllocated@+0x180,
//                       muAllocationKey@+0x184, muHelperIndex@+0x188, mpManager@+0x18C,
//                       mpBehaviour@+0x190).
//   * mLookAroundCarCam @+0x194 (BehaviourHandle, 0x14) -- the rotate-about-vehicle (look-
//                       around-car) cam handle (zeroed +0x194..+0x1A4 like mIceCam).
//   * mbWasCarModScreen @+0x1A8 (bool) -- whether the SELECTING_LIVERY (car-mod) screen has
//                       been entered; selects which produced camera the later states copy
//                       (the look-around cam vs the ICE reveal cam) and which "No_B_W" vs
//                       "_BW" fade hook plays. NOT seeded by Construct (the console's
//                       Construct does not write +0x1A8).
//   * meState          @+0x1AC (EState, the car-select state machine value; the dispatch
//                       table is indexed by this value 0..6, default -> assert).
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the console's 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera
    {
        class BehaviourManager;
        class BehaviourIceAnim;
        class BehaviourRotateAboutVehicle;
    }

    class ArbStateOnlineCarSelect : public ArbitratorState
    {
    public:
        // EState -- the online car-select state machine.
        // Construct seeds 0 (INACTIVE); Prepare forces 1 (PREPARING); Update's case-1 success
        // edge stores 2 (SELECTING_CAR) and falls straight into the case-2 body that frame. The
        // SELECTING_CAR edges store 3 (SELECTING_LIVERY) / 4 (WAIT_TO_CHANGE_TO_INTRO); the
        // hand-off edges run ChangeToState with blocked values 5 (CHANGING_TO_INTRO) and 6
        // (CHANGING_TO_ROAMING). The dispatch table is indexed by this value (0..6; >6 hits the
        // default assert). Values are the console's dispatch-table case indices / the values it
        // stores into meState (+0x1AC).
        enum EState
        {
            E_STATE_INACTIVE                = 0,
            E_STATE_PREPARING               = 1,
            E_STATE_SELECTING_CAR           = 2,
            E_STATE_SELECTING_LIVERY        = 3,
            E_STATE_WAIT_TO_CHANGE_TO_INTRO = 4,
            E_STATE_CHANGING_TO_INTRO       = 5,
            E_STATE_CHANGING_TO_ROAMING     = 6,
            E_STATE_RELEASING               = 7,

            E_NUM_STATES                    = 8
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) ------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;

        // Destruct() is NOT in this TU's recovered function set (it keeps the base declaration;
        // no override added here -- the declaration record lists one, but the shipped build does
        // not attest a body for this TU, so it is omitted).

    private:
        // RETIRED: this state used to carry its OWN nested five-word BehaviourHandle<> copy.
        // It now uses the SHARED BrnDirector::Camera::BehaviourHandle<TBehaviour>, which is what
        // the console has -- ONE template instantiated per behaviour type, not a per-state
        // duplicate. Using the shared handle is therefore more faithful, and it is what lets the
        // bodied BehaviourManager::NewBehaviour<> overload bind here (the generic THandle overload
        // is declaration-only, so a nested fork compiles and then leaves the behaviour unallocated
        // at link time). The shared handle also identifies the +0x08 word this file used to flag
        // as "role not recovered": it is the owning BehaviourHelper pool pointer, which a u32
        // would have truncated on this host. It brings IsAllocated / GetBehaviour /
        // GetProducedCamera / IsWaitingToPrepare / Release with it, all bodied, so this header no
        // longer declares any of them.

        // ---- members, declaration order; offsets in comments -----------------------------
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mIceCam;           // +0x180
        Camera::BehaviourHandle<Camera::BehaviourRotateAboutVehicle> mLookAroundCarCam; // +0x194
        bool                                                  mbWasCarModScreen; // +0x1A8
        EState                                                meState;           // +0x1AC
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H
