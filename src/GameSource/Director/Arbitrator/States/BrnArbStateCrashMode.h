#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_MODE_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_MODE_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<> (the SHARED handle)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h
//
// BrnDirector::ArbStateCrashMode -- the director arbitrator state that runs while the player is
// in "crash mode" (the slow-motion aftertouch crash camera after a crash, with the intro
// flash/borders/motion-blur ramp, the slow camera roll/tilt oscillation, and the periodic
// slow-mo "close-up" punctuations). Derives from ArbitratorState (vtable order pinned by the
// base). The state machine walks INACTIVE -> PREPARING -> ACTIVE -> CHANGING_TO_ROAMING ->
// RELEASING; the heavy work lives in Update().
//
// LAYOUT: the member names and their declaration order are the ones this build declares; the
// per-member console offsets are pinned from Construct / Update / DoCloseup / Release:
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by
//     name through the base GetNonConstCamera()/the effect-trigger free functions).
//   * mAftertouch       @+0x180 (BehaviourHandle, 0x14) -- the aftertouch-crash camera handle
//   * meState           @+0x194 (EState, the crash-mode state machine value)
//   * the scalar/flag block @+0x198..+0x1CE (the intro flash/borders/blur timers, the camera
//     tilt block, and the close-up timers/flags).
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute offsets
// shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourAftertouchCrash; }

    class ArbStateCrashMode : public ArbitratorState
    {
    public:
        // EState -- the crash-mode state machine. Construct
        // seeds 0 (INACTIVE); Update's case-1 (PREPARING) success edge stores 2 (ACTIVE) and
        // the dispatch table is indexed by this value. The CHANGING_TO_ROAMING / RELEASING
        // edges store 3 / 4. Values are the console dispatch-table case indices / the immediates
        // stored into meState.
        enum EState
        {
            E_STATE_INACTIVE            = 0,
            E_STATE_PREPARING           = 1,
            E_STATE_ACTIVE              = 2,
            E_STATE_CHANGING_TO_ROAMING = 3,
            E_STATE_RELEASING           = 4,

            E_NUM_STATES                = 5
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) -------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;   // vtable slot 1
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;   // vtable slot 3
        const char* GetName() const override;

        // Destruct() is not in this TU's function set (it keeps the base declaration; no
        // override added here).

    private:
        // ---- private per-state helper ----------------------------------------------------
        // Drive the slow-mo "close-up" while one is active: ramp the behaviour's close-up
        // blend and pick the time-dilation the camera requests.
        void DoCloseup(ArbStateSharedInfo& lrSharedInfo);

        // The ACTIVE-state per-frame body. The console inlines this whole block into Update's case 2
        // (it is reached both from case 2 directly and via the case-1 PREPARING success
        // fall-through); de-inlined here to a named helper so Update's PREPARING edge can call it
        // without the switch falling through into a declaration scope. Not a separate console
        // function (it is inlined into Update).
        void TickActive(ArbStateSharedInfo& lrSharedInfo);

        // ---- members, in declaration order; console offsets in comments -------------------
        // The handle is the SHARED BrnDirector::Camera::BehaviourHandle<TBehaviour> (the same
        // five-word block the console has: mbAllocated +0x00, muAllocationKey +0x04, the owning
        // helper pool +0x08, mpManager +0x0C, mpBehaviour +0x10). This state used to carry a
        // private copy of that template with the +0x08 word FLAGged as an unrecovered "lookup
        // helper"; the shared one has it pinned as the pool pointer and brings the three members
        // this TU needs and the copy could not provide -- IsReadyToPrepare(), GetProducedCamera()
        // and Release().
        Camera::BehaviourHandle<Camera::BehaviourAftertouchCrash> mAftertouch; // +0x180
        EState meState;             // +0x194  the crash-mode state-machine state

        // intro flash / borders / motion-blur ramp block (Update case-1->2 seeds these).
        f32  mfFlashTime;           // +0x198  time left on the entry "2dFlash" hook
        f32  mfBordersTime;         // +0x19C  time left on the entry black-bars/borders
        f32  mfBlurInTime;          // +0x1A0  blur ramp-in duration remaining
        f32  mfBlurOutTime;         // +0x1A4  blur ramp-out duration remaining
        f32  mfCurrentBlur;         // +0x1A8  the current motion-blur amount [0..mfMaximumBlur]
        f32  mfMaximumBlur;         // +0x1AC  the blur amount ceiling
        f32  mfBlurRampSpeed;       // +0x1B0  blur change rate (per sim step)
        bool mbLinearBlurRamp;      // +0x1B4  linear vs squared blur ramp (Construct: true)
        bool mbBlurShake;           // +0x1B5  drive the motion-blur "shake" request this frame

        // camera tilt/roll oscillation block.
        f32  mfTiltAngle;           // +0x1B8  current camera roll angle (rads)
        f32  mfTargetTiltAngle;     // +0x1BC  the roll angle being lerped toward (sign flips)
        f32  mfTiltChangeTime;      // +0x1C0  time left before the target tilt sign flips

        // periodic slow-mo close-up block.
        f32  mfCloseupTime;         // +0x1C4  time elapsed in the current close-up
        f32  mfTimeSinceCloseup;    // +0x1C8  time since the last close-up ended
        bool mbDoingCloseup;        // +0x1CC  a close-up is currently running
        bool mbAllowCloseup;        // +0x1CD  close-ups may start (Construct: true)
        bool mbSuperSloMoCloseUp;   // +0x1CE  the current close-up is the deeper slow-mo variant
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_CRASH_MODE_H
