#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the helper tripwires)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"  // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"  // Camera::BehaviourHandle / BehaviourHelperIndex
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h" // THE BehaviourInterpolate home
                                                             // (RECONCILED 2026-08-01: the manager header used to carry
                                                             // a rival, member-less slice of this class and the two were
                                                             // mutually exclusive -- C2011. The slice is retired, so the
                                                             // real home is included here and mInterpolaterParams below
                                                             // is the real Parameters record.)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h
//
// BrnDirector::InterpolaterHelper -- the (offline) car-select state's little
// wrapper around a BehaviourHandle<BehaviourInterpolate>: it owns the camera
// interpolation the state runs between two behaviours, exposing prepare / ready /
// finished / camera / release. DWARF home per the X360 asserts
// (BrnArbStateCarSelect.h:62/:65/:68).
//
// BrnDirector::ArbStateCarSelect -- the director arbitrator state that runs the
// camera for the OFFLINE junkyard (car-select) screen AND for the retail
// new-profile GAME INTRO fly-by. It owns six ICE-anim camera behaviours, a
// rotate-about-vehicle "look around car" cam, three interpolators and the junkyard
// shot references, and walks a 15-state machine (EState below).
//
// ⭐ THE GAME-INTRO PATH (the retail intro camera). GameState::mbNewProfileIntroActive
// (+216) selects it out of E_STATE_PREPARING: the state allocates mGameIntro, binds it
// to shot 0 of the DirectorResourceManager's "game intro" shot group (mGameIntroGroup,
// manager +1272 -- the AttribSys collection whose vault name key is "606002"), and then
// walks
//     PREPARING -> GAME_INTRO_PART_ONE  (shot 0 playing; waits for
//                                        GameState::mbGameIntroFlybyActive (+217))
//               -> GAME_INTRO_PART_TWO  (ChangeMovie to shot 1)
//               -> GAME_INTRO_PART_THREE(ChangeMovie to shot 2 when part two finishes;
//                                        when the fly-by flag CLEARS, interpolates onto
//                                        the player's car and hands over to
//                                        ROTATE_ABOUT_CAR)
// The three-shot form is taken only when the group holds exactly three shots; otherwise
// the state jumps straight to PART_THREE. (VERIFIED against the retail data: the
// CAMERAS.BUNDLE CameraVault's "606002" shotgroup holds exactly 3 iceanim shots, guids
// 610132 / 605855 / 605858.)
//
// LAYOUT: member NAMES + declaration ORDER are the DecFIGS DWARF's
// (BrnArbStateCarSelect.h:161..:212, X360-attested). The per-member X360 offsets quoted
// in the comments are pinned from the ARTIST asm (Construct @0x8225AFB8,
// Prepare @0x8226EFA0, Update @0x8226F5D0, StartCarUnlockCam @0x8226F398,
// SetupJunkyardShotgroup @0x821F64A0). Parity is BY NAMED MEMBER (the x64 gate rule):
// the embedded Camera widens on the host so absolute offsets shift.
// ============================================================================

namespace Attrib { namespace Gen { class shotgroup; class iceanim; } }

namespace BrnDirector
{
    class DirectorResourceManager;

    namespace Camera
    {
        class BehaviourIceAnim;
        class BehaviourRotateAboutVehicle;
    }

    // ------------------------------------------------------------------------
    // InterpolaterHelper (DWARF BrnArbStateCarSelect.h:39). X360 offsets are helper-
    // relative and pinned by ArbStateCarSelect::Construct's write set for the three
    // embedded helpers (+0x00 handle, +0x14 parameters, +0x24 owner):
    //     handle 5 words @+0x00..+0x10, Parameters {8, 0, method 0, mapping 1} @+0x14,
    //     mpArbStateParent @+0x24.
    // ------------------------------------------------------------------------
    struct InterpolaterHelper
    {
        // X360: the inlined ctor Construct(lpArbStateParent) -- zero the handle, default
        // the interpolation parameters (type tag 8 / no debug name / slerp / sinusoidal),
        // and remember the owning state (the owner NewBehaviour<> books the behaviour to).
        void Construct(const ArbitratorState* lpArbStateParent)
        {
            mInterpolater = Camera::BehaviourHandle<Camera::BehaviourInterpolate>();
            mInterpolaterParams.Construct();
            mpArbStateParent = lpArbStateParent;
        }

        // @0x82266200 -- allocate the interpolator behaviour and set it running between
        // two behaviour helper slots. The X360 stores the method/mapping into the
        // parameters block FIRST (helper +0x1C / +0x20), allocates through the manager,
        // hands the behaviour its parameters (the inlined SetParameters: the params
        // pointer + the debug name), then runs the interpolate Setup with the duration
        // and the two helper indices.
        void Prepare(f32 lfDuration,
                     Camera::BehaviourHelperIndex lFromBehaviourHelperIndex,
                     Camera::BehaviourHelperIndex lToBehaviourHelperIndex,
                     Camera::BehaviourManager* lpBehaviourController,
                     s32 leInterpolationMethod,
                     s32 leInterpolationMapping);

        // The helper is "prepared" once its handle owns a behaviour (the X360's
        // helper +0x00 read IS the handle's mbAllocated -- the handle is the
        // helper's leading member).
        bool IsPrepared() const { return mInterpolater.IsAllocated(); }

        // @0x82219900 -- h:62 tripwire, then the handle's IsReadyToPrepare
        // (whose committed body carries the X360's inlined :517 "mbIsAllocated"
        // tripwire + the manager IsBehaviourWaitingToPrepare()==0 tail).
        bool IsReady() const
        {
            CGS_ASSERT(IsPrepared(), "IsPrepared()");   // :62 (non-gating)
            return mInterpolater.IsReadyToPrepare();
        }

        // @0x82208680 -- h:65 tripwire + the handle-level :600 "IsAllocated()"
        // tripwire (the X360 inlines the handle's behaviour resolve: slot ->
        // *slot -> the interpolator's +0x596 finished byte; the committed
        // GetBehaviour() cache is that same pointee by Prepare's definition).
        bool HasFinished() const
        {
            CGS_ASSERT(IsPrepared(), "IsPrepared()");            // :65 (non-gating)
            CGS_ASSERT(mInterpolater.IsAllocated(), "IsAllocated()");   // BrnBehaviourManager.h:600 (non-gating)
            return mInterpolater.GetBehaviour()->HasFinished();
        }

        // @0x82219998 -- h:68 tripwire (the X360 CALLS IsReady out-of-line, so
        // its own tripwires fire too), then a BY-VALUE copy of the camera the
        // owned behaviour produced (the X360 copy-constructs from the manager
        // pool slot's +0x10 camera -- the handle's GetProducedCamera surface,
        // which carries the :610 handle tripwire).
        Camera::Camera GetCamera() const
        {
            CGS_ASSERT(IsReady(), "IsReady()");   // :68 (non-gating)
            return mInterpolater.GetProducedCamera();
        }

        // @0x822346E0 -- exactly the handle's Release body (drop the manager-side
        // hold and clear; a no-op when nothing is held).
        void Release()
        {
            mInterpolater.Release();
        }

        // DWARF h:71 -- the helper's own behaviour helper index (the handle's).
        Camera::BehaviourHelperIndex GetBehaviourHelperIndex() const
        {
            return mInterpolater.GetBehaviourHelperIndex();
        }

    private:
        // DWARF h:75/:76/:77. The X360 helper is layout-identical to its leading handle
        // (+0x00 allocated, +0x04 key, +0x08 pool, +0x0C manager, +0x10 behaviour), then
        // the by-value interpolation parameters and the owning state.
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;        // +0x00
        Camera::BehaviourInterpolate::Parameters              mInterpolaterParams;  // +0x14
        const ArbitratorState*                                mpArbStateParent;     // +0x24
    };

    // ------------------------------------------------------------------------
    // ArbStateCarSelect (DWARF BrnArbStateCarSelect.h:83).
    // ------------------------------------------------------------------------
    class ArbStateCarSelect : public ArbitratorState
    {
    public:
        // DWARF BrnArbStateCarSelect.h:116 -- the junkyard / game-intro state machine.
        // The Update jump table is indexed by this value (0..13; the default arm asserts
        // "unhandled state", BrnArbStateCarSelect.cpp:965).
        enum EState
        {
            E_STATE_INACTIVE             = 0,
            E_STATE_PREPARING            = 1,
            E_STATE_GAME_INTRO_PART_ONE  = 2,
            E_STATE_GAME_INTRO_PART_TWO  = 3,
            E_STATE_GAME_INTRO_PART_THREE= 4,
            E_STATE_INTRO                = 5,
            E_STATE_OUTRO                = 6,
            E_STATE_CAR_UNLOCK           = 7,
            E_STATE_ACTIVE               = 8,
            E_STATE_ROTATE_ABOUT_CAR     = 9,
            E_STATE_WAIT_FOR_CAR_DROP    = 10,
            E_STATE_IDLE                 = 11,
            E_STATE_WAIT_FOR_AUDIO       = 12,
            E_STATE_CHANGING_TO_ROAMING  = 13,
            E_STATE_RELEASING            = 14,
            E_NUM_STATES                 = 15
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) ----------
        void        Construct() override;                                // @0x8225AFB8
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;   // @0x8226EFA0
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;    // @0x8226F5D0
        const char* GetName() const override;                             // @0x821F6490

        // Release @0x?????? / Destruct are NOT in this TU's exported X360 function set
        // (see the .cpp banner) -- the base declarations stand.

    private:
        // @0x8226F398 -- start the car-unlock ICE movie (rival-unlock variant when
        // GameState::mbIsRivalUnlock, else the unlocked car's own asset movie, else the
        // shared car-unlock shot group).
        void StartCarUnlockCam(ArbStateSharedInfo& lrSharedInfo);

        // @0x821F64A0 -- pick the junkyard nearest the player car and return its
        // car-select shot group (also latching that junkyard's rival-unlock group into
        // mpRivalUnlockShotGroup).
        const Attrib::Gen::shotgroup* SetupJunkyardShotgroup(ArbStateSharedInfo& lrSharedInfo);

        // ---- de-inlined shared blocks (NOT X360 functions) -----------------------------
        // The X360 Update reaches each of these from several state arms by branch
        // (LABEL_136 / LABEL_138 / LABEL_117) and its INTRO arm is also the fall-through
        // target of the PREPARING arm (LABEL_37). Extracted into named helpers so the
        // reconstruction carries no gotos and no duplicated blocks.
        void StartOutroMovie(ArbStateSharedInfo& lrSharedInfo);         // X360 LABEL_136
        void StartWaitForAudioMovie(ArbStateSharedInfo& lrSharedInfo);  // X360 LABEL_138
        void ReturnToActive();                                          // X360 LABEL_117
        void UpdateIntroState(ArbStateSharedInfo& lrSharedInfo);        // X360 LABEL_37 (case 5)

        // ---- members, DWARF order; X360 offsets in comments ---------------------------
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mTransitionCam;    // +0x180 (384)
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mCarUnlockCam;     // +0x194 (404)
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mIntroNoNewCars;   // +0x1A8 (424)
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mIntroNewCars;     // +0x1BC (444)
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mGameIntro;        // +0x1D0 (464)
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>            mIdleCam;          // +0x1E4 (484)
        Camera::BehaviourHandle<Camera::BehaviourRotateAboutVehicle> mLookAroundCarCam; // +0x1F8 (504)

        InterpolaterHelper  mToCarSelectInterpolater;   // +0x20C (524)
        InterpolaterHelper  mToGameplayInterpolater;    // +0x234 (564)
        InterpolaterHelper  mFromGameplayInterpolater;  // +0x25C (604)  (allocated but unused by Update)

        // The seven per-shot iceanim parameter blocks Prepare resolves out of the junkyard
        // / idle / outro shot groups. Offsets from Prepare's store set:
        //   +0x284 (644) junkyard ShotList[0]   +0x294 (660) junkyard ShotList[1]
        //   +0x288 (648) junkyard ShotList[3]   +0x298 (664) junkyard ShotList[2]
        //   +0x28C (652) junkyard ShotList[4]   +0x29C (668) mCarSelectIdle  ShotList[0]
        //   +0x290 (656) mCarSelectOutro ShotList[0]
        // (Camera::BehaviourIceAnim::ShotReference IS Attrib::Gen::iceanim -- named through
        // the generated class here so the header needs no behaviour include.)
        Attrib::Gen::iceanim* mpShotIntroNoNewCars;  // +0x284 (644)
        Attrib::Gen::iceanim* mpIntroNewCarsShot;    // +0x288 (648)
        Attrib::Gen::iceanim* mpOutroShot;           // +0x28C (652)
        Attrib::Gen::iceanim* mpWaitForAudioShot;    // +0x290 (656)
        Attrib::Gen::iceanim* mpLeftToRight;         // +0x294 (660)
        Attrib::Gen::iceanim* mpRightToLeft;         // +0x298 (664)
        Attrib::Gen::iceanim* mpIdle;                // +0x29C (668)

        // ⚠️ FLAG (opaque, un-includable embedded aggregate): the DWARF member here is
        //   `Camera::Utils::CameraImpactEffect mImpactEffect` (h:182) -- a 20-byte
        //   {f32 mfImpactFactor; CameraShake mCameraShake;} block, exactly the extent
        //   Construct zeroes (+672..+688). Its real home
        //   (Camera/Utils/BrnCameraImpactEffect.h) transitively pulls the canonical
        //   CameraShake / Looker / Tweaker / CollisionPolicy homes, which are STILL FORKED
        //   inside Behaviours/BrnBehaviourIceAnim.h (six C2011 redefinitions) -- and this TU
        //   must include the ICE-anim behaviour. Modelling the member as its named, correctly
        //   sized opaque storage is the project's convention for exactly this case; the two
        //   operations the X360 runs on it (Construct + RegisterImpact, and the per-frame
        //   Update) are documented ⚠️ GATEs in the .cpp rather than reached by offset.
        //   DELETE-WHEN: BrnBehaviourIceAnim.h's Utils/collision-policy forks are retired in
        //   favour of the canonical homes (the same de-forking wave that already retired its
        //   Behaviour + Timestep forks).
        u8 maImpactEffect[20];                             // +0x2A0 (672)

        const Attrib::Gen::shotgroup* mpRivalUnlockShotGroup;  // +0x2B4 (692)

        f32  mfTimeInState;                            // +0x2B8 (696)
        f32  mfTimeWaitingToLookAtOriginalSelection;   // +0x2BC (700)
        f32  mfTimeBeforeFadeInAfterLiveryChange;      // +0x2C0 (704)

        bool mbIsLeft;                                 // +0x2C4 (708)
        bool mbHasCarsToUnlock;                        // +0x2C5 (709)
        bool mbIsFirstCarSelect;                       // +0x2C6 (710)
        bool mbWaitingForCarToTouchGround;             // +0x2C7 (711)
        bool mbWaitingForCarToSpawn;                   // +0x2C8 (712)
        bool mbSafeToBeAttachedToCar;                  // +0x2C9 (713)
        bool mbWaitingToLookAtOriginalSelection;       // +0x2CA (714)
        bool mbFadingOutCarUnlockMovie;                // +0x2CB (715)

        u32  muRivalUnlockMovie;                       // +0x2CC (716)
        u32  muCarUnlockMovie;                         // +0x2D0 (720)

        EState meState;                                // +0x2D4 (724)
    };
}
