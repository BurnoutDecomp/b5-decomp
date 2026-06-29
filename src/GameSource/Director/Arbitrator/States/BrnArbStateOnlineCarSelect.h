#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

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
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateOnlineCarSelect.h, X360-attested for this build). The per-member X360 offsets
// are pinned from the ARTIST asm (Construct @0x8225B3E0, Prepare @0x82271020,
// Update @0x82236840, Release @0x82236E28):
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
//                       "_BW" fade hook plays. NOT seeded by Construct (the X360 Construct
//                       does not write +0x1A8).
//   * meState          @+0x1AC (EState, the car-select state machine value; the dispatch
//                       table is indexed by this value 0..6, default -> assert).
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
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
        // DWARF EState (BrnArbStateOnlineCarSelect.h:66). The online car-select state machine.
        // Construct seeds 0 (INACTIVE); Prepare forces 1 (PREPARING); Update's case-1 success
        // edge stores 2 (SELECTING_CAR) and falls straight into the case-2 body that frame. The
        // SELECTING_CAR edges store 3 (SELECTING_LIVERY) / 4 (WAIT_TO_CHANGE_TO_INTRO); the
        // hand-off edges run ChangeToState with blocked values 5 (CHANGING_TO_INTRO) and 6
        // (CHANGING_TO_ROAMING). The dispatch table is indexed by this value (0..6; >6 hits the
        // default assert). Values are the X360 jump-table case indices / the immediates the asm
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

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                              // @0x8225B3E0
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82271020
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;   // @0x82236840
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82236E28
        const char* GetName() const override;                          // @0x821F6730

        // Destruct() is NOT in this TU's X360 function set (it keeps the base declaration; no
        // override added here -- the DWARF lists a Destruct() but the X360 ledger does not
        // attest one for this TU, so it is omitted).

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare via BehaviourManager::NewBehaviour<TBehaviour> and released in
        // Release via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager, muAllocationKey).
        // 0x14-byte block (5 words) pinned from the Construct/Prepare/Update/Release asm:
        // mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10). The X360 re-resolves the live behaviour from the
        // manager pool through (helper word, allocation key); GetBehaviour() returns the cached
        // pointer to that same behaviour after asserting IsAllocated() (sub_821FD3E8 /
        // sub_821FDED0 == BrnBehaviourManager.h:589). GetProducedCamera() returns the camera the
        // live behaviour produced this frame, which the manager keeps alongside the behaviour in
        // the same pool slot (sub_821FD450 / sub_821FDF38 read slot+0x10 ==
        // BrnBehaviourManager.h:610). FLAG: the +0x08 word's role is not fully recovered in this
        // TU (modelled as an opaque behaviour-lookup helper index, as in BrnArbStateRaceIntro.h /
        // BrnArbStateCrashMode.h / BrnArbStateRankUp.h).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            // The live behaviour this handle owns (only valid while IsAllocated()). The X360
            // asserts IsAllocated() inside the manager-pool lookup before returning it
            // (sub_821FD3E8 / sub_821FDED0; BrnBehaviourManager.h:589).
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            // The camera the live behaviour produced this frame. The X360 reads it from the
            // manager pool slot the handle resolves to (sub_821FD450 / sub_821FDF38 return
            // slot+0x10; BrnBehaviourManager.h:610), which is the same camera the behaviour
            // writes each frame -- modelled by NAME as the behaviour's produced camera. Asserts
            // IsAllocated() (the X360 asserts it inside the pool lookup). Defined out-of-line in
            // the .cpp where the behaviour type is complete.
            const Camera::Camera& GetProducedCamera() const;

            // True once the owned behaviour has finished its initial Prepare and is ready to use.
            // The X360 (sub @0x822128A0, BrnBehaviourManager.h:517) asserts mbAllocated then
            // returns mpManager->IsBehaviourWaitingToPrepare(muAllocationKey). NOTE: unlike the
            // rank-up state (which returns the NEGATION), this TU's Prepare returns this value
            // directly (the asm tail-calls the helper with no negation), so the method mirrors
            // that raw value. Defined out-of-line in the .cpp where BehaviourManager is complete.
            bool IsBehaviourWaitingToPrepare() const;

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourIceAnim>             mIceCam;           // X360 +0x180
        BehaviourHandle<Camera::BehaviourRotateAboutVehicle>  mLookAroundCarCam; // X360 +0x194
        bool                                                  mbWasCarModScreen; // +0x1A8
        EState                                                meState;           // +0x1AC
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_CAR_SELECT_H
