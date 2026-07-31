#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"

#include "types.hpp"
#include <cmath>                                                                  // fabsf (the car-stillness test)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT / the FireAssert tripwires
#include "GameShared/GameClasses/Development/CgsStrStream.h"                      // CgsDev::StrStream (the formatted asserts)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"              // BrnDirector::GameState
#include "GameSource/Director/Camera/Camera.h"                                    // Camera::Camera (mEffects / mState_uFlags)
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                  // SharedCameraContainer
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"                 // NamedParameters
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                   // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"                  // VehicleTracker::GetImplicitAcceleration
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                    // Camera::VehicleInfo
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"            // Camera::BehaviourIceAnim + the DirectorResourceManager slice
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // Camera::BehaviourRotateAboutVehicle
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                         // Camera::Utils::Cycle
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                     // Attrib::Gen::shotgroup
#include "GameSource/AttribSys/Generated/classes/iceanim.h"                       // Attrib::Gen::iceanim
#include "rw/math/vpu/types.h"                                                    // rw::math::vpu::Matrix44Affine / Vector3

// ============================================================================
// BrnDirector::ArbStateCarSelect / BrnDirector::InterpolaterHelper
//   -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//
//   ArbStateCarSelect::Construct              @0x8225AFB8
//   ArbStateCarSelect::GetName                @0x821F6490
//   ArbStateCarSelect::Prepare                @0x8226EFA0
//   ArbStateCarSelect::SetupJunkyardShotgroup @0x821F64A0
//   ArbStateCarSelect::StartCarUnlockCam      @0x8226F398
//   ArbStateCarSelect::Update                 @0x8226F5D0
//   InterpolaterHelper::Prepare               @0x82266200
//
// The director's OFFLINE junkyard (car-select) arbitrator state -- and, through the
// GameState::mbNewProfileIntroActive branch of its PREPARING state, the retail
// NEW-PROFILE GAME-INTRO fly-by camera. See the header banner for the intro path.
//
// SCOPE NOTE. The X360 ledger's function set for this TU is the six functions listed
// above plus InterpolaterHelper::Prepare; every one is reconstructed here, and every arm
// of Update's 14-way state machine is reproduced. `Release` / `Destruct` (DWARF
// BrnArbStateCarSelect.cpp:1017 / :1038) are NOT in this build's export set, so no
// override is declared for them -- the base declarations stand. Three sub-behaviours the
// X360 inlines are documented ⚠️ GATEs rather than fabricated (each carries its own
// CONSEQUENCE + DELETE-WHEN): the CHANGING_TO_ROAMING hand-off (needs Release), the
// per-frame impact shake (needs CameraImpactEffect::Update + CameraShake::Parameters),
// and the car-asset's own unlock movie (needs the burnoutcarasset generated field set).
//
// All member access is BY NAME (the x64 gate rule); the console byte offsets quoted in
// the header are provenance only.
//
// ⚠️ COMPILE STATUS (inherited, pre-existing). This .cpp does NOT pass `cl /c` yet, and
// neither do its already-committed siblings BrnArbStateOnlineCarSelect.cpp /
// BrnArbStateRaceIntro.cpp (verified 2026-07-31: identical failure). The cause is not in
// this TU: any TU that includes BOTH the named-parameter bank (-> BehaviourPassengerCam.h
// -> BehaviourRig.h) AND Behaviours/BrnBehaviourIceAnim.h hits SIX C2011 redefinitions,
// because the ICE-anim header still carries its own forks of CollisionPolicy /
// VisibilityCollisionPolicy / CollisionPolicyAttachedToVehicle / Utils::CameraShake /
// Utils::Looker / Utils::Tweaker whose canonical homes (BehaviourRig.h /
// BrnCollisionPolicy.h) define the same names. Both calls are unavoidable here (the
// look-around-car cam is configured from the bank; the ICE-anim behaviour IS the state's
// camera). That de-fork is its own wave -- the same one that already retired the ICE-anim
// header's Behaviour + Timestep forks -- and it is what unblocks this whole state family
// (and lets ArbitratorStateContainer de-fork its ArbStateCarSelect placeholder). The GAME
// BUILD is green with this file present: it is not mounted, exactly like its siblings.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // ---- the TU's file-scope tunables (DWARF BrnArbStateCarSelect.cpp:21..:25). Each is
        // read from exactly one site in Update and written by nothing in the image, so the
        // .data words ARE the const initialisers. Values dumped from the ARTIST image.
        const f32 KF_MAX_EARLY_OUT_OF_TRANSITION_CAM_SECS = 3.0f;   // flt_82CDA4E4
        const f32 KF_TIME_BEFORE_ROTATE_ABOUT_CAR_SECS    = 10.0f;  // flt_82CDADA4
        const f32 KF_CAR_STILLNESS_THRESHOLD              = 0.1f;   // flt_82CDA4EC
        const f32 KF_IMPACT_AMOUNT                        = 0.5f;   // flt_82CDADA8

        // FLAG: the fifth constant DWARF-declares for this .cpp is KF_IMPACT_THRESHOLD, and
        //   flt_82CDADA0 (2.0) is the only remaining one of the five addresses -- but its single
        //   use site is a TIME bound (mfTimeWaitingToLookAtOriginalSelection < it), not an impact
        //   magnitude. The name/address pairing for this ONE constant is therefore BY
        //   ELIMINATION and its role reads wrong; the VALUE (2.0) is dumped, not guessed.
        const f32 KF_IMPACT_THRESHOLD                     = 2.0f;   // flt_82CDADA0

        // The two trailing selectors the NewBehaviour<TBehaviour> allocation request carries.
        // The state-machine behaviours are allocated with (0, 2), the car-unlock cam with
        // (0, 1) (X360 li r6,0 and li r7,2 / li r7,1).
        void* const KP_NEW_BEHAVIOUR_OWNER          = 0;
        const s32   KI_NEW_BEHAVIOUR_REF_LIMIT_STATE  = 2;
        const s32   KI_NEW_BEHAVIOUR_REF_LIMIT_UNLOCK = 1;

        // The junkyard shot group must carry at least this many ICE movies
        // (BrnArbStateCarSelect.cpp:111 / :140 assert "Not enough ice movies in junk yard
        // shot group"): shots 0..4 == the two intro movies, the two browse movies and the outro.
        const u32 KU_MIN_JUNKYARD_SHOTS = 5u;

        // The junkyard ShotList slots (the indices Prepare resolves).
        const u32 KU_JUNKYARD_SHOT_INTRO_NO_NEW_CARS = 0u;
        const u32 KU_JUNKYARD_SHOT_LEFT_TO_RIGHT     = 1u;
        const u32 KU_JUNKYARD_SHOT_RIGHT_TO_LEFT     = 2u;
        const u32 KU_JUNKYARD_SHOT_INTRO_NEW_CARS    = 3u;
        const u32 KU_JUNKYARD_SHOT_OUTRO             = 4u;

        // ⭐ The GAME-INTRO shot group's ShotList slots (DirectorResourceManager::
        // mGameIntroGroup). The three-part form runs shot 0 -> 1 -> 2; any other count jumps
        // straight to part three.
        const u32 KU_GAME_INTRO_SHOT_PART_ONE        = 0u;
        const u32 KU_GAME_INTRO_SHOT_PART_TWO        = 1u;
        const u32 KU_GAME_INTRO_SHOT_PART_THREE      = 2u;
        const u32 KU_GAME_INTRO_THREE_PART_SHOT_COUNT = 3u;
        const u32 KU_MIN_GAME_INTRO_CHANGE_SHOTS     = 2u;

        // The junkyard forces the world clock while its camera owns the frame: 16:30 for the
        // browse / intro states, 12:30 for the outro (mEffects.mfTimeOfDay + mbSetTimeOfDay).
        const f32 KF_JUNKYARD_TIME_OF_DAY = 16.5f;
        const f32 KF_OUTRO_TIME_OF_DAY    = 12.5f;

        // The camera-PFX hook blend every junkyard request plays at.
        const f32 KF_HOOK_BLEND = 1.0f;

        // The delay between a livery change and the fade back in.
        const f32 KF_LIVERY_FADE_DELAY_SECS = 0.5f;

        // The parametric time the transition take is parked at while the state prepares
        // (1.0 == the end of the take) and rewound to when the car-unlock cam hands back.
        const f32 KF_TAKE_PARAMETRIC_TIME_END   = 1.0f;
        const f32 KF_TAKE_PARAMETRIC_TIME_START = 0.0f;

        // The take-reset byte (+0xDE4) the state raises on a take that must HOLD on its last
        // frame (the game-intro part one and the idle cam) and clears when the take is allowed
        // to run to completion.
        const u8 KU8_TAKE_HOLDS_ON_LAST_FRAME = 1u;
        const u8 KU8_TAKE_RUNS_TO_END         = 0u;

        // The interpolation the state runs onto / off the look-around-car camera
        // (InterpolaterHelper::Prepare's method / mapping selectors and its two durations).
        const s32 KI_INTERPOLATION_METHOD  = 1;
        const s32 KI_INTERPOLATION_MAPPING = 3;
        const f32 KF_INTERPOLATE_ONTO_CAR_SECS        = 1.0f;
        const f32 KF_INTERPOLATE_FROM_BROWSE_CAM_SECS = 2.0f;

        // The camera-state flag bits the junkyard states raise/clear on their produced camera
        // (X360 `mCamera.mState_uFlags |= <bit>` / `&= ~<bit>`; the bit VALUES are asm, the
        // ROLE names are inferred from which state raises each).
        const s32 KI_CAMERA_STATE_FOLLOW            = 0x00000002;
        const s32 KI_CAMERA_STATE_JUNKYARD_ENTERING = 0x00200000;
        const s32 KI_CAMERA_STATE_JUNKYARD          = 0x00400000;
        const s32 KI_CAMERA_STATE_JUNKYARD_NEW_CARS = 0x00800000;

        // The screen-effect hook names the junkyard requests.
        const char* const KPC_HOOK_LIVERY_IN           = "Livery_In";
        const char* const KPC_HOOK_LIVERY_OUT          = "Livery_Out";
        const char* const KPC_HOOK_JUNK_CAR_SELECT     = "Junk_Car_Select";
        const char* const KPC_HOOK_FADE_OUT_BLACK_JUNK = "FadeOutBlack_Junk";

        const char* const KPC_SOURCE_FILE =
            "..\\..\\..\\GameSource\\Director/Arbitrator/States/BrnArbStateCarSelect.cpp";

        // The five junkyards' world positions, in the order their shot-group pairs are staged
        // in the DirectorResourceManager (+1064.. -- MotorCity, WestAcres, SouthBay,
        // Heartbreak, LowerPeaks). Values dumped from the ARTIST .rdata
        // (flt_82003F70 .. flt_82003FA8).
        const s32 KI_NUM_JUNKYARDS = 5;

        const f32 KAAF_JUNKYARD_POSITIONS[KI_NUM_JUNKYARDS][3] =
        {
            {  2978.0f,  -2.7f, -2014.4f },   // Motor City
            {  1316.8f,  14.5f,  -464.5f },   // West Acres
            {  -356.9f,  10.1f,   903.2f },   // South Bay
            { -1046.2f, 113.0f, -1785.5f },   // Heartbreak Hills
            { -2239.6f, 102.1f,   438.9f }    // Lower Peaks
        };

        // One junkyard's staged record (the X360 builds these five on the stack).
        struct JunkyardShotGroups
        {
            f32                           mafPosition[3];
            const Attrib::Gen::shotgroup* mpCarSelectShots;
            const Attrib::Gen::shotgroup* mpRivalUnlockShots;
        };

        void FireAssert(const char* lpcText, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcText, KPC_SOURCE_FILE, liLine);
            CgsDev::Assert::EndAssert();
        }

        // "<text><value>" into the shared assert buffer, the X360's StrStream idiom.
        void FireValueAssert(const char* lpcText, s32 liValue, s32 liLine)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << lpcText << liValue;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_SOURCE_FILE, liLine);
            CgsDev::Assert::EndAssert();
        }

        // The ShotList element accessor every junkyard / intro shot resolve goes through
        // (Attrib::Instance::GetAttributePointer(group, 0x15246B49, index), falling back to
        // Attrib::DefaultDataArea(0x18) when the element is absent -- the generated shotgroup
        // accessor already carries exactly that fallback).
        Camera::BehaviourIceAnim::ShotReference* GetShot(const Attrib::Gen::shotgroup& lrGroup,
                                                        u32 luIndex)
        {
            return static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                const_cast<void*>(lrGroup.GetShotListElement(luIndex)));
        }
    }

    // ------------------------------------------------------------------------
    // InterpolaterHelper::Prepare @0x82266200
    //
    // Store the method/mapping into the helper's own parameter block, allocate the
    // interpolator behaviour through the manager (owned by the parent arbitrator state),
    // hand the behaviour those parameters, then set it running between the two helper slots
    // over the requested duration.
    // ------------------------------------------------------------------------
    void InterpolaterHelper::Prepare(f32 lfDuration,
                                     Camera::BehaviourHelperIndex lFromBehaviourHelperIndex,
                                     Camera::BehaviourHelperIndex lToBehaviourHelperIndex,
                                     Camera::BehaviourManager* lpBehaviourController,
                                     s32 leInterpolationMethod,
                                     s32 leInterpolationMapping)
    {
        mInterpolaterParams.meInterpolationMethod  = leInterpolationMethod;   // helper +0x1C
        mInterpolaterParams.meInterpolationMapping = leInterpolationMapping;  // helper +0x20

        lpBehaviourController->NewBehaviour<Camera::BehaviourInterpolate>(
            mInterpolater,
            const_cast<ArbitratorState*>(mpArbStateParent),
            0,
            1);

        CGS_ASSERT(mInterpolater.IsAllocated(), "IsAllocated()");   // BrnBehaviourManager.h:589

        // The X360 inlines BehaviourInterpolate::SetParameters here (the params pointer + the
        // parameters' debug name into the behaviour's own base slot).
        mInterpolater.GetBehaviour()->SetParameters(&mInterpolaterParams);

        CGS_ASSERT(mInterpolater.IsAllocated(), "IsAllocated()");   // BrnBehaviourManager.h:589

        // Setup(from, to, manager, duration): blend the FROM helper's camera into the TO
        // helper's over lfDuration.
        mInterpolater.GetBehaviour()->SetupCameraAFromHelper(lFromBehaviourHelperIndex,
                                                            *lpBehaviourController);
        mInterpolater.GetBehaviour()->SetupCameraBFromHelper(lToBehaviourHelperIndex,
                                                            *lpBehaviourController);
        mInterpolater.GetBehaviour()->SetupDuration(lfDuration);
        mInterpolater.GetBehaviour()->Setup();
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225AFB8
    // ------------------------------------------------------------------------
    void ArbStateCarSelect::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        mTransitionCam    = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mCarUnlockCam     = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mIntroNoNewCars   = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mIntroNewCars     = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mGameIntro        = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mIdleCam          = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
        mLookAroundCarCam = Camera::BehaviourHandle<Camera::BehaviourRotateAboutVehicle>();

        mToCarSelectInterpolater.Construct(this);
        mToGameplayInterpolater.Construct(this);
        mFromGameplayInterpolater.Construct(this);

        // ⚠️ GATE: X360 CameraImpactEffect::Construct(mImpactEffect) -- the member is
        //   named opaque storage here (see the header FLAG), so the block is zeroed as
        //   storage. Same observable state; re-point at the real call when the type
        //   becomes includable.
        for (u32 luByte = 0; luByte < sizeof(maImpactEffect); ++luByte)
            maImpactEffect[luByte] = 0;

        mfTimeWaitingToLookAtOriginalSelection = 0.0f;
        mbWaitingToLookAtOriginalSelection     = false;
        mbFadingOutCarUnlockMovie              = false;
        muRivalUnlockMovie                     = 0u;
        muCarUnlockMovie                       = 0u;

        meState = E_STATE_INACTIVE;
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6490
    // ------------------------------------------------------------------------
    const char* ArbStateCarSelect::GetName() const
    {
        return "ArbStateCarSelect";
    }

    // ------------------------------------------------------------------------
    // SetupJunkyardShotgroup @0x821F64A0
    //
    // Pick the junkyard nearest the player car out of the five staged pairs, latch that
    // junkyard's RIVAL-UNLOCK group into mpRivalUnlockShotGroup, and return its plain
    // car-select group. The X360 builds the five {position, carSelectShots, rivalUnlockShots}
    // records on the stack, then walks them keeping the smallest squared distance (the
    // vmsum3fp128 dot of the position delta).
    // ------------------------------------------------------------------------
    const Attrib::Gen::shotgroup*
    ArbStateCarSelect::SetupJunkyardShotgroup(ArbStateSharedInfo& lrSharedInfo)
    {
        const DirectorResourceManager& lrResources = *lrSharedInfo.mpDirectorResourceManager;

        JunkyardShotGroups laJunkyards[KI_NUM_JUNKYARDS];

        laJunkyards[0].mpCarSelectShots   = &lrResources.GetCarSelectMotorCityShots();
        laJunkyards[0].mpRivalUnlockShots = &lrResources.GetCarSelectMotorCityRivalUnlockShots();
        laJunkyards[1].mpCarSelectShots   = &lrResources.GetCarSelectWestAcresShots();
        laJunkyards[1].mpRivalUnlockShots = &lrResources.GetCarSelectWestAcresRivalUnlockShots();
        laJunkyards[2].mpCarSelectShots   = &lrResources.GetCarSelectSouthBayShots();
        laJunkyards[2].mpRivalUnlockShots = &lrResources.GetCarSelectSouthBayRivalUnlockShots();
        laJunkyards[3].mpCarSelectShots   = &lrResources.GetCarSelectHeartbreakShots();
        laJunkyards[3].mpRivalUnlockShots = &lrResources.GetCarSelectHeartbreakRivalUnlockShots();
        laJunkyards[4].mpCarSelectShots   = &lrResources.GetCarSelectLowerPeaksShots();
        laJunkyards[4].mpRivalUnlockShots = &lrResources.GetCarSelectLowerPeaksRivalUnlockShots();

        for (s32 liJunkyard = 0; liJunkyard < KI_NUM_JUNKYARDS; ++liJunkyard)
        {
            laJunkyards[liJunkyard].mafPosition[0] = KAAF_JUNKYARD_POSITIONS[liJunkyard][0];
            laJunkyards[liJunkyard].mafPosition[1] = KAAF_JUNKYARD_POSITIONS[liJunkyard][1];
            laJunkyards[liJunkyard].mafPosition[2] = KAAF_JUNKYARD_POSITIONS[liJunkyard][2];
        }

        // The player car's world position -- the transform's translation row (X360
        // `lvx128 v13, playerCarTransform, 0x30`).
        const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
            *static_cast<const rw::math::vpu::Matrix44Affine*>(lrSharedInfo.mpPlayerCarTransform);
        const rw::math::vpu::Vector3& lrPlayerPosition = lrPlayerTransform.Pos();

        s32 liNearestJunkyard   = 0;
        f32 lfNearestSqDistance = 0.0f;
        for (s32 liJunkyard = 0; liJunkyard < KI_NUM_JUNKYARDS; ++liJunkyard)
        {
            const f32 lfDx = laJunkyards[liJunkyard].mafPosition[0] - lrPlayerPosition.x;
            const f32 lfDy = laJunkyards[liJunkyard].mafPosition[1] - lrPlayerPosition.y;
            const f32 lfDz = laJunkyards[liJunkyard].mafPosition[2] - lrPlayerPosition.z;
            const f32 lfSqDistance = (lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz);

            // The X360 seeds the running minimum from the first record's own slot, so the
            // first iteration always wins.
            if (liJunkyard == 0 || lfSqDistance < lfNearestSqDistance)
            {
                lfNearestSqDistance = lfSqDistance;
                liNearestJunkyard   = liJunkyard;
            }
        }

        mpRivalUnlockShotGroup = laJunkyards[liNearestJunkyard].mpRivalUnlockShots;
        return laJunkyards[liNearestJunkyard].mpCarSelectShots;
    }

    // ------------------------------------------------------------------------
    // Prepare @0x8226EFA0
    //
    // Idempotent (returns immediately once the state machine has left INACTIVE). On the
    // first pass it resolves the nearest junkyard's shot group, binds the seven shot
    // references, and allocates the transition / intro / look-around-car behaviours. The
    // tail seeds the state's camera: from the shared gameplay camera while the transition
    // behaviour is still queued for its first Prepare, else from the behaviour's own
    // produced camera. Always reports "prepared".
    // ------------------------------------------------------------------------
    bool ArbStateCarSelect::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState != E_STATE_INACTIVE)
            return true;

        const bool lbTransitionCamAllocated = mTransitionCam.IsAllocated();
        meState = E_STATE_PREPARING;

        const DirectorResourceManager& lrResources = *lrSharedInfo.mpDirectorResourceManager;

        if (!lbTransitionCamAllocated)
        {
            const Attrib::Gen::shotgroup& lrJunkyardShots = *SetupJunkyardShotgroup(lrSharedInfo);
            if (lrJunkyardShots.Num_ShotList() < KU_MIN_JUNKYARD_SHOTS)
                FireAssert("Not enough ice movies in junk yard shot group", 111);

            mpShotIntroNoNewCars = GetShot(lrJunkyardShots, KU_JUNKYARD_SHOT_INTRO_NO_NEW_CARS);
            mpLeftToRight        = GetShot(lrJunkyardShots, KU_JUNKYARD_SHOT_LEFT_TO_RIGHT);
            mpRightToLeft        = GetShot(lrJunkyardShots, KU_JUNKYARD_SHOT_RIGHT_TO_LEFT);

            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mTransitionCam, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
            mTransitionCam.GetBehaviour()->ClearBaseFirstFrameGate();

            if (static_cast<u32>(lrSharedInfo.mpGameState->miJunkyardPosIndex) > 2u)
            {
                FireValueAssert("Invalid junkyard pos: ",
                                lrSharedInfo.mpGameState->miJunkyardPosIndex, 119);
            }

            // The junkyard opens on the right-to-left browse movie.
            mTransitionCam.GetBehaviour()->SetParameters(mpRightToLeft);
            mbIsLeft = true;
        }

        mpIdle = GetShot(lrResources.GetCarSelectIdleShots(), 0u);

        if (!mIntroNoNewCars.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mIntroNoNewCars, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
            mIntroNoNewCars.GetBehaviour()->ClearBaseFirstFrameGate();
            mIntroNoNewCars.GetBehaviour()->SetParameters(mpShotIntroNoNewCars);
        }

        if (!mIntroNewCars.IsAllocated())
        {
            const Attrib::Gen::shotgroup& lrJunkyardShots = *SetupJunkyardShotgroup(lrSharedInfo);
            if (lrJunkyardShots.Num_ShotList() < KU_MIN_JUNKYARD_SHOTS)
                FireAssert("Not enough ice movies in junk yard shot group", 140);

            mpIntroNewCarsShot = GetShot(lrJunkyardShots, KU_JUNKYARD_SHOT_INTRO_NEW_CARS);
            mpOutroShot        = GetShot(lrJunkyardShots, KU_JUNKYARD_SHOT_OUTRO);
            mpWaitForAudioShot = GetShot(lrResources.GetCarSelectOutroShots(), 0u);

            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mIntroNewCars, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
            mIntroNewCars.GetBehaviour()->ClearBaseFirstFrameGate();
            mIntroNewCars.GetBehaviour()->SetParameters(mpIntroNewCarsShot);
        }

        if (!mLookAroundCarCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourRotateAboutVehicle>(
                mLookAroundCarCam, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
            mLookAroundCarCam.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->GetLookAroundCarCamParameters());
        }

        if (mTransitionCam.IsWaitingToPrepare())
        {
            // Still queued: hold the live gameplay camera and drop its "follow" bit.
            GetNonConstCamera() = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();
            GetNonConstCamera().mState_uFlags &= ~KI_CAMERA_STATE_FOLLOW;
            return true;
        }

        GetNonConstCamera() = mTransitionCam.GetProducedCamera();
        return true;
    }

    // ------------------------------------------------------------------------
    // StartCarUnlockCam @0x8226F398
    //
    // Start the car-unlock ICE movie on mCarUnlockCam. The X360 has three sources:
    //   * a RIVAL unlock -> the nearest junkyard's rival-unlock shot group, cycling
    //     muRivalUnlockMovie through its ShotList;
    //   * else the unlocked car's OWN burnoutcarasset movie, when the asset declares one;
    //   * else the shared mCarUnlock shot group, cycling muCarUnlockMovie.
    // ------------------------------------------------------------------------
    void ArbStateCarSelect::StartCarUnlockCam(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState& lrGameState = *lrSharedInfo.mpGameState;

        if (lrGameState.mbIsRivalUnlock)
        {
            Camera::BehaviourIceAnim::ShotReference* lpUnlockShot =
                GetShot(*mpRivalUnlockShotGroup, muRivalUnlockMovie);

            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mCarUnlockCam, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_UNLOCK);
            mCarUnlockCam.GetBehaviour()->ClearBaseFirstFrameGate();
            mCarUnlockCam.GetBehaviour()->SetParameters(lpUnlockShot);

            muRivalUnlockMovie = Camera::Utils::Cycle(muRivalUnlockMovie, 1u,
                                                     mpRivalUnlockShotGroup->Num_ShotList(), 1u);
            mbSafeToBeAttachedToCar      = false;
            mbFadingOutCarUnlockMovie    = false;
            mbWaitingForCarToSpawn       = true;
            mbWaitingForCarToTouchGround = true;
            return;
        }

        // ⚠️ GATE: the X360's MIDDLE arm resolves the unlocked car's own unlock movie --
        //     DirectorResourceManager::GetVehicleInfoRef(lRef, rm, mUnlockedVehicleType);
        //     lpUnlockShot = Attrib::Gen::burnoutcarasset(lRef).<field @ +392>;
        //     if ( lpUnlockShot ) assert lpUnlockShot->GetClassKey()==iceanim::ClassKey();
        //     ... Attrib::RefSpec::Clean(lRef);
        //   The burnoutcarasset generated accessor set is not recovered for this build (its
        //   only ledger function is the ctor), so the +392 field has no NAME -- inventing one
        //   would be fabrication, and reaching it by offset is an audit failure. The X360's
        //   own fallback (the shared car-unlock shot group) is therefore taken unconditionally.
        //   CONSEQUENCE: a car whose asset declares a bespoke unlock movie gets the shared one.
        //   DELETE-WHEN: the burnoutcarasset generated field set is recovered.
        const Attrib::Gen::shotgroup& lrCarUnlockShots =
            lrSharedInfo.mpDirectorResourceManager->GetCarUnlockShots();
        Camera::BehaviourIceAnim::ShotReference* lpUnlockShot =
            GetShot(lrCarUnlockShots, muCarUnlockMovie);
        muCarUnlockMovie = Camera::Utils::Cycle(muCarUnlockMovie, 0u,
                                                lrCarUnlockShots.Num_ShotList(), 1u);

        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
            mCarUnlockCam, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_UNLOCK);
        mCarUnlockCam.GetBehaviour()->ClearBaseFirstFrameGate();
        mCarUnlockCam.GetBehaviour()->SetParameters(lpUnlockShot);
    }

    // ------------------------------------------------------------------------
    // The three shared blocks Update's arms branch to (X360 LABEL_136 / LABEL_138 /
    // LABEL_117). De-inlined into named helpers rather than reproduced as gotos; each is
    // reached from several state arms and then falls into the shared per-frame tail.
    // ------------------------------------------------------------------------
    void ArbStateCarSelect::StartOutroMovie(ArbStateSharedInfo& lrSharedInfo)
    {
        lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();

        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
            mIntroNewCars, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
        mIntroNewCars.GetBehaviour()->ClearBaseFirstFrameGate();
        mIntroNewCars.GetBehaviour()->SetParameters(mpOutroShot);

        meState = E_STATE_OUTRO;
    }

    void ArbStateCarSelect::StartWaitForAudioMovie(ArbStateSharedInfo& lrSharedInfo)
    {
        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
            mIntroNewCars, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
        mIntroNewCars.GetBehaviour()->ClearBaseFirstFrameGate();
        mIntroNewCars.GetBehaviour()->SetParameters(mpWaitForAudioShot);
        mIntroNewCars.GetBehaviour()->SetTakeResetByte0(KU8_TAKE_RUNS_TO_END);

        meState = E_STATE_WAIT_FOR_AUDIO;
    }

    void ArbStateCarSelect::ReturnToActive()
    {
        mToGameplayInterpolater.Release();
        mfTimeInState = 0.0f;
        meState       = E_STATE_ACTIVE;
    }

    // ------------------------------------------------------------------------
    // The INTRO state's body (X360 LABEL_37). It is its own case AND the arm the PREPARING
    // state falls into while it is still waiting for its behaviours, so it is de-inlined
    // here rather than duplicated.
    // ------------------------------------------------------------------------
    void ArbStateCarSelect::UpdateIntroState(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState& lrGameState = *lrSharedInfo.mpGameState;

        if (mbHasCarsToUnlock)
        {
            GetNonConstCamera() = mIntroNewCars.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_NEW_CARS;
        }
        else
        {
            GetNonConstCamera() = mIntroNoNewCars.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;
        }
        mLookAroundCarCam.GetBehaviour()->BecomeSimilarTo(GetNonConstCamera(),
                                                         *lrSharedInfo.mpAllVehicleData);

        if (lrGameState.mbGameIntroFlybyActive)
            FireAssert("!lSharedInfo.mpGameState->mbGameIntroFlybyActive", 381);

        if (lrGameState.meJunkyardState == GameState::E_JY_INTRO_NO_CARS ||
            lrGameState.meJunkyardState == GameState::E_JY_INTRO_UNLOCKING_CARS)
        {
            return;   // still playing the intro movie
        }

        mfTimeInState = 0.0f;

        if (!mbHasCarsToUnlock && !mToGameplayInterpolater.IsPrepared())
        {
            mToGameplayInterpolater.Prepare(KF_INTERPOLATE_ONTO_CAR_SECS,
                                            mIntroNoNewCars.GetBehaviourHelperIndex(),
                                            mLookAroundCarCam.GetBehaviourHelperIndex(),
                                            lrSharedInfo.mpBehaviourManager,
                                            KI_INTERPOLATION_METHOD,
                                            KI_INTERPOLATION_MAPPING);
        }
        if (!mbHasCarsToUnlock && mToGameplayInterpolater.IsReady())
        {
            GetNonConstCamera() = mToGameplayInterpolater.GetCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;
        }

        if (lrGameState.meJunkyardState == GameState::E_JY_CAR_UNLOCK)
        {
            if (!mbHasCarsToUnlock)
                FireAssert("mbHasCarsToUnlock", 407);
            mToCarSelectInterpolater.Release();
            mToGameplayInterpolater.Release();
            mIntroNewCars.Release();
            meState = E_STATE_CAR_UNLOCK;
            StartCarUnlockCam(lrSharedInfo);
        }
        else if (lrGameState.meJunkyardState != GameState::E_JY_INACTIVE)
        {
            if (lrGameState.meJunkyardState == GameState::E_JY_CAR_SELECT &&
                !mbHasCarsToUnlock && mToGameplayInterpolater.HasFinished())
            {
                mToCarSelectInterpolater.Release();
                mIntroNewCars.Release();
                mfTimeInState = 0.0f;
                meState       = E_STATE_ROTATE_ABOUT_CAR;
            }
            else if (lrGameState.meJunkyardState != GameState::E_JY_CAR_SELECT)
            {
                FireValueAssert("Unhandled state: ",
                                static_cast<s32>(lrGameState.meJunkyardState), 435);
            }
        }
        else
        {
            mToGameplayInterpolater.Release();
            mIntroNewCars.Release();
            mfTimeInState = 0.0f;
            meState       = E_STATE_ACTIVE;
        }
    }

    // ------------------------------------------------------------------------
    // Update @0x8226F5D0 -- the junkyard / game-intro state machine.
    // ------------------------------------------------------------------------
    void ArbStateCarSelect::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState&                     lrGameState = *lrSharedInfo.mpGameState;
        // ArbStateSharedInfo forward-declares mpPlayerCar's pointee as
        // BrnDirector::VehicleInfo, but the real home is BrnDirector::Camera::VehicleInfo
        // (SharedIO/BrnPlayerInfo.h) -- the shared header's forward declaration is in the
        // wrong namespace. Bound to the REAL named type here so every read below is by
        // member NAME (the sibling states read the same pointer by raw byte offset).
        // DELETE-WHEN: ArbStateSharedInfo::mpPlayerCar / mpRaceCars are re-typed.
        const Camera::VehicleInfo&     lrPlayerCar =
            *reinterpret_cast<const Camera::VehicleInfo*>(lrSharedInfo.mpPlayerCar);
        Camera::BehaviourManager&      lrManager   = *lrSharedInfo.mpBehaviourManager;
        const DirectorResourceManager& lrResources = *lrSharedInfo.mpDirectorResourceManager;

        // The junkyard camera is not the "entering junkyard" camera by default; each arm that
        // wants that bit raises it again.
        GetNonConstCamera().mState_uFlags &= ~KI_CAMERA_STATE_JUNKYARD_ENTERING;

        // Has the car settled? (rising implicit acceleration + a near-zero vertical velocity)
        const bool lbCarIsStill =
            (lrSharedInfo.mpPlayerTracker->GetImplicitAcceleration().y > 0.0f) &&
            (fabsf(lrPlayerCar.mRaceCarState.mLinearVelocity.y) < KF_CAR_STILLNESS_THRESHOLD);

        if (mbWaitingForCarToTouchGround)
        {
            const bool lbAnyWheelOnGround =
                lrPlayerCar.mRaceCarState.maWheels[0].mRoadContact.mbIsOnGround ||
                lrPlayerCar.mRaceCarState.maWheels[1].mRoadContact.mbIsOnGround ||
                lrPlayerCar.mRaceCarState.maWheels[2].mRoadContact.mbIsOnGround ||
                lrPlayerCar.mRaceCarState.maWheels[3].mRoadContact.mbIsOnGround;
            if (lbAnyWheelOnGround)
            {
                // ⚠️ GATE: X360 `CameraImpactEffect::RegisterImpact(mImpactEffect,
                //   KF_IMPACT_AMOUNT)`. The accumulator lives inside the opaque member (header
                //   FLAG); writing it here would be an offset poke. The latch below is real.
                //   CONSEQUENCE: no car-drop shake (cosmetic; off the intro path).
                //   DELETE-WHEN: as the header FLAG.
                mbWaitingForCarToTouchGround = false;
            }
        }

        if (lbCarIsStill && !mbWaitingForCarToSpawn)
            mbSafeToBeAttachedToCar = true;

        if (lrGameState.mbJunkyardPlayerRespawnedThisFrame)
        {
            mbSafeToBeAttachedToCar = false;
            mbWaitingForCarToSpawn  = false;
            if (!lrGameState.mbJunkyardCarModActive)
                mbWaitingForCarToTouchGround = true;
        }

        switch (meState)
        {
        // ---- 0: INACTIVE -- nothing runs (not even the shared tail) --------------------
        case E_STATE_INACTIVE:
            return;

        // ---- 1: PREPARING -------------------------------------------------------------
        case E_STATE_PREPARING:
        {
            const bool lbReady = Prepare(lrSharedInfo)
                              && !mTransitionCam.IsWaitingToPrepare()
                              && !mIntroNewCars.IsWaitingToPrepare()
                              && !mLookAroundCarCam.IsWaitingToPrepare();
            if (!lbReady)
            {
                // Still preparing: the X360 falls into the INTRO body this frame.
                UpdateIntroState(lrSharedInfo);
                break;
            }

            // Park the transition take at its end and reset the per-entry state.
            mTransitionCam.GetBehaviour()->SetControllerParametricTime0To1(
                KF_TAKE_PARAMETRIC_TIME_END);
            mfTimeBeforeFadeInAfterLiveryChange = 0.0f;
            mbIsFirstCarSelect                  = false;
            mbHasCarsToUnlock                   = false;
            mbSafeToBeAttachedToCar             = true;
            mbWaitingForCarToTouchGround        = false;
            mbWaitingForCarToSpawn              = false;
            // ⚠️ GATE: X360 CameraImpactEffect::Construct(mImpactEffect) -- the member is
            //   named opaque storage here (see the header FLAG), so the block is zeroed as
            //   storage. Same observable state; re-point at the real call when the type
            //   becomes includable.
            for (u32 luByte = 0; luByte < sizeof(maImpactEffect); ++luByte)
                maImpactEffect[luByte] = 0;

            if (lrGameState.mbNewProfileIntroActive)
            {
                // ⭐ THE RETAIL GAME-INTRO FLY-BY. Bind mGameIntro to shot 0 of the "game
                // intro" shot group (holding on its last frame) and start the sequence.
                const Attrib::Gen::shotgroup& lrGameIntroShots = lrResources.GetGameIntroShots();
                if (lrGameIntroShots.Num_ShotList() == 0u)
                    FireAssert("Not enough ice movies in game intro group", 299);

                Camera::BehaviourIceAnim::ShotReference* lpShot =
                    GetShot(lrGameIntroShots, KU_GAME_INTRO_SHOT_PART_ONE);

                lrManager.NewBehaviour<Camera::BehaviourIceAnim>(
                    mGameIntro, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
                mGameIntro.GetBehaviour()->ClearBaseFirstFrameGate();
                mGameIntro.GetBehaviour()->SetParameters(lpShot);
                mGameIntro.GetBehaviour()->SetTakeResetByte0(KU8_TAKE_HOLDS_ON_LAST_FRAME);

                mToCarSelectInterpolater.Release();
                mToGameplayInterpolater.Release();
                mIntroNoNewCars.Release();
                mIntroNewCars.Release();

                mbIsFirstCarSelect = true;

                meState = (lrGameIntroShots.Num_ShotList() == KU_GAME_INTRO_THREE_PART_SHOT_COUNT)
                              ? E_STATE_GAME_INTRO_PART_ONE
                              : E_STATE_GAME_INTRO_PART_THREE;
                break;
            }

            // The ordinary junkyard entry, keyed off the game's junkyard sub-state.
            switch (lrGameState.meJunkyardState)
            {
            case GameState::E_JY_INTRO_UNLOCKING_CARS:
                mbHasCarsToUnlock = true;
                meState           = E_STATE_INTRO;
                break;
            case GameState::E_JY_INTRO_NO_CARS:
                meState = E_STATE_INTRO;
                break;
            case GameState::E_JY_CAR_SELECT:
                meState = E_STATE_ACTIVE;
                break;
            default:
                FireValueAssert("Unhandled state: ",
                                static_cast<s32>(lrGameState.meJunkyardState), 348);
                meState = E_STATE_ACTIVE;
                break;
            }
            mfTimeInState = 0.0f;
            break;
        }

        // ---- 2: GAME_INTRO_PART_ONE  ⭐ ------------------------------------------------
        // Shot 0 is playing. Force the junkyard time of day; when the GUI raises the fly-by
        // flag (BrnGui::Intro's START_FLYBY -> command 477), change to shot 1.
        case E_STATE_GAME_INTRO_PART_ONE:
            GetNonConstCamera() = mGameIntro.GetProducedCamera();
            GetNonConstCamera().GetEffects().mbSetTimeOfDay = true;
            GetNonConstCamera().GetEffects().mfTimeOfDay    = KF_JUNKYARD_TIME_OF_DAY;

            if (lrGameState.mbGameIntroFlybyActive)
            {
                const Attrib::Gen::shotgroup& lrGameIntroShots = lrResources.GetGameIntroShots();
                if (lrGameIntroShots.Num_ShotList() < KU_MIN_GAME_INTRO_CHANGE_SHOTS)
                    FireAssert("Not enough ice movies in game intro group", 451);

                mGameIntro.GetBehaviour()->ChangeMovie(
                    GetShot(lrGameIntroShots, KU_GAME_INTRO_SHOT_PART_TWO), lrResources);
                mGameIntro.GetBehaviour()->SetTakeResetByte0(KU8_TAKE_RUNS_TO_END);
                meState = E_STATE_GAME_INTRO_PART_TWO;
            }
            break;

        // ---- 3: GAME_INTRO_PART_TWO  ⭐ ------------------------------------------------
        // Shot 1 is playing; when it finishes, change to shot 2.
        case E_STATE_GAME_INTRO_PART_TWO:
            GetNonConstCamera() = mGameIntro.GetProducedCamera();

            if (mGameIntro.GetBehaviour()->HasFinishedOrFailed())
            {
                const Attrib::Gen::shotgroup& lrGameIntroShots = lrResources.GetGameIntroShots();
                if (lrGameIntroShots.Num_ShotList() < KU_MIN_GAME_INTRO_CHANGE_SHOTS)
                    FireAssert("Not enough ice movies in game intro group", 469);

                mGameIntro.GetBehaviour()->ChangeMovie(
                    GetShot(lrGameIntroShots, KU_GAME_INTRO_SHOT_PART_THREE), lrResources);
                mGameIntro.GetBehaviour()->SetTakeResetByte0(KU8_TAKE_RUNS_TO_END);
                meState = E_STATE_GAME_INTRO_PART_THREE;
            }
            break;

        // ---- 4: GAME_INTRO_PART_THREE  ⭐ ----------------------------------------------
        // Shot 2 is playing and the look-around-car cam is kept aligned with it. When the GUI
        // CLEARS the fly-by flag (command 478), interpolate off the movie onto the player's
        // car and hand over to ROTATE_ABOUT_CAR.
        case E_STATE_GAME_INTRO_PART_THREE:
            GetNonConstCamera() = mGameIntro.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;
            mLookAroundCarCam.GetBehaviour()->BecomeSimilarTo(GetNonConstCamera(),
                                                             *lrSharedInfo.mpAllVehicleData);

            if (!lrGameState.mbGameIntroFlybyActive)
            {
                if (!mToGameplayInterpolater.IsPrepared())
                {
                    mToGameplayInterpolater.Prepare(KF_INTERPOLATE_ONTO_CAR_SECS,
                                                    mGameIntro.GetBehaviourHelperIndex(),
                                                    mLookAroundCarCam.GetBehaviourHelperIndex(),
                                                    &lrManager,
                                                    KI_INTERPOLATION_METHOD,
                                                    KI_INTERPOLATION_MAPPING);
                }
                if (mToGameplayInterpolater.IsReady())
                {
                    GetNonConstCamera() = mToGameplayInterpolater.GetCamera();
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;
                }
                if (mToGameplayInterpolater.HasFinished())
                {
                    mGameIntro.Release();
                    mfTimeInState = 0.0f;
                    meState       = E_STATE_ROTATE_ABOUT_CAR;
                }
            }
            break;

        // ---- 5: INTRO -----------------------------------------------------------------
        case E_STATE_INTRO:
            UpdateIntroState(lrSharedInfo);
            break;

        // ---- 6: OUTRO -----------------------------------------------------------------
        case E_STATE_OUTRO:
            if (mbIsFirstCarSelect)
            {
                GetNonConstCamera().GetEffects().mbSetTimeOfDay = true;
                GetNonConstCamera().GetEffects().mfTimeOfDay    = KF_OUTRO_TIME_OF_DAY;
            }
            lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();

            GetNonConstCamera() = mIntroNewCars.GetProducedCamera();
            if (mIntroNewCars.GetBehaviour()->HasFinishedOrFailed())
            {
                mIntroNewCars.Release();
                mfTimeInState = 0.0f;
                meState       = E_STATE_CHANGING_TO_ROAMING;
            }
            break;

        // ---- 7: CAR_UNLOCK ------------------------------------------------------------
        case E_STATE_CAR_UNLOCK:
            GetNonConstCamera() = mCarUnlockCam.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.mbJunkyardCarUnlockTickedClosedThisFrame)
                mbFadingOutCarUnlockMovie = true;
            if (mbFadingOutCarUnlockMovie)
            {
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_FADE_OUT_BLACK_JUNK, KF_HOOK_BLEND);
            }

            if (lrGameState.meJunkyardState == GameState::E_JY_CAR_UNLOCK)
            {
                if (lrGameState.mbNewCarUnlockedThisFrame)
                    StartCarUnlockCam(lrSharedInfo);
            }
            else
            {
                // ⚠️ GATE: X360 CameraImpactEffect::Construct(mImpactEffect) -- the member is
                //   named opaque storage here (see the header FLAG), so the block is zeroed as
                //   storage. Same observable state; re-point at the real call when the type
                //   becomes includable.
                for (u32 luByte = 0; luByte < sizeof(maImpactEffect); ++luByte)
                    maImpactEffect[luByte] = 0;
                mbHasCarsToUnlock = false;
                mToGameplayInterpolater.Release();
                mCarUnlockCam.Release();
                mTransitionCam.GetBehaviour()->SetControllerParametricTime0To1(
                    KF_TAKE_PARAMETRIC_TIME_START);
                mfTimeInState = 0.0f;
                meState       = E_STATE_INTRO;
            }
            break;

        // ---- 8: ACTIVE (the browse camera) --------------------------------------------
        case E_STATE_ACTIVE:
            GetNonConstCamera() = mTransitionCam.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.mbJunkyardPosJustChanged)
            {
                if (mbIsLeft != lrGameState.mbJunkyardPosIsLeft)
                {
                    mTransitionCam.GetBehaviour()->ChangeMovie(
                        lrGameState.mbJunkyardPosIsLeft ? mpRightToLeft : mpLeftToRight,
                        lrResources);
                }
                mfTimeInState = 0.0f;
                mbIsLeft      = lrGameState.mbJunkyardPosIsLeft;
                GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                mbWaitingForCarToSpawn       = true;
                mbWaitingForCarToTouchGround = false;
            }

            if (lrGameState.meJunkyardState == GameState::E_JY_WAITING_FOR_AUDIO)
            {
                StartWaitForAudioMovie(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_CAR_UNLOCK)
            {
                meState = E_STATE_CAR_UNLOCK;
                StartCarUnlockCam(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState != GameState::E_JY_INACTIVE)
            {
                if (lrGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame &&
                    !lrGameState.mbJunkyardCarModActive &&
                    mTransitionCam.GetBehaviour()->HasFinishedOrFailed())
                {
                    mbIsLeft = !mbIsLeft;
                    mTransitionCam.GetBehaviour()->ChangeMovie(
                        mbIsLeft ? mpLeftToRight : mpRightToLeft, lrResources);
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                    Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface,
                                                  KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                    mToGameplayInterpolater.Release();
                    mfTimeInState                      = 0.0f;
                    mbWaitingToLookAtOriginalSelection = false;
                    mbWaitingForCarToSpawn             = true;
                    meState                            = E_STATE_WAIT_FOR_CAR_DROP;
                }
                else if (mTransitionCam.GetBehaviour()->GetTimeRemaining()
                             < KF_MAX_EARLY_OUT_OF_TRANSITION_CAM_SECS &&
                         meState == E_STATE_ACTIVE &&
                         (mbSafeToBeAttachedToCar || lrGameState.mbJunkyardCarModActive))
                {
                    mToGameplayInterpolater.Prepare(KF_INTERPOLATE_FROM_BROWSE_CAM_SECS,
                                                    mTransitionCam.GetBehaviourHelperIndex(),
                                                    mLookAroundCarCam.GetBehaviourHelperIndex(),
                                                    &lrManager,
                                                    KI_INTERPOLATION_METHOD,
                                                    KI_INTERPOLATION_MAPPING);
                    mLookAroundCarCam.GetBehaviour()->BecomeSimilarTo(
                        mTransitionCam.GetProducedCamera(), *lrSharedInfo.mpAllVehicleData);
                    mfTimeInState = 0.0f;
                    meState       = E_STATE_ROTATE_ABOUT_CAR;
                }
            }
            else
            {
                StartOutroMovie(lrSharedInfo);
            }
            break;

        // ---- 9: ROTATE_ABOUT_CAR ------------------------------------------------------
        case E_STATE_ROTATE_ABOUT_CAR:
            GetNonConstCamera() = mToGameplayInterpolater.GetCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.meJunkyardState == GameState::E_JY_INACTIVE)
            {
                StartOutroMovie(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_WAITING_FOR_AUDIO)
            {
                StartWaitForAudioMovie(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState != GameState::E_JY_CAR_SELECT)
            {
                ReturnToActive();
            }
            else if (lrGameState.mbJunkyardPosJustChanged &&
                     !lrGameState.mbJunkyardCarModActive)
            {
                if (mbIsLeft != lrGameState.mbJunkyardPosIsLeft)
                {
                    mTransitionCam.GetBehaviour()->ChangeMovie(
                        lrGameState.mbJunkyardPosIsLeft ? mpRightToLeft : mpLeftToRight,
                        lrResources);
                }
                mbIsLeft = lrGameState.mbJunkyardPosIsLeft;
                GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                mbSafeToBeAttachedToCar = false;
                mbWaitingForCarToSpawn  = true;
                mToGameplayInterpolater.Release();
                mfTimeInState = 0.0f;
                meState       = E_STATE_ACTIVE;
            }
            else if (lrGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame &&
                     !lrGameState.mbJunkyardCarModActive)
            {
                mbIsLeft = !mbIsLeft;
                mTransitionCam.GetBehaviour()->ChangeMovie(
                    mbIsLeft ? mpLeftToRight : mpRightToLeft, lrResources);
                GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                mToGameplayInterpolater.Release();
                mfTimeInState                      = 0.0f;
                mbWaitingToLookAtOriginalSelection = false;
                mbWaitingForCarToSpawn             = true;
                mbWaitingForCarToTouchGround       = false;
                meState                            = E_STATE_WAIT_FOR_CAR_DROP;
            }
            else if (lrGameState.mfPadInactiveTime > KF_TIME_BEFORE_ROTATE_ABOUT_CAR_SECS &&
                     mfTimeInState > KF_TIME_BEFORE_ROTATE_ABOUT_CAR_SECS)
            {
                lrManager.NewBehaviour<Camera::BehaviourIceAnim>(
                    mIdleCam, this, KP_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REF_LIMIT_STATE);
                mIdleCam.GetBehaviour()->ClearBaseFirstFrameGate();
                mIdleCam.GetBehaviour()->SetParameters(mpIdle);
                mIdleCam.GetBehaviour()->SetTakeResetByte0(KU8_TAKE_HOLDS_ON_LAST_FRAME);
                mfTimeInState = 0.0f;
                meState       = E_STATE_IDLE;
            }
            break;

        // ---- 10: WAIT_FOR_CAR_DROP ----------------------------------------------------
        case E_STATE_WAIT_FOR_CAR_DROP:
            GetNonConstCamera() = mTransitionCam.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.meJunkyardState == GameState::E_JY_INACTIVE)
            {
                StartOutroMovie(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_WAITING_FOR_AUDIO)
            {
                StartWaitForAudioMovie(lrSharedInfo);
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_CAR_SELECT)
            {
                if (lrGameState.mbJunkyardPlayerRespawnedThisFrame)
                {
                    if (mbIsLeft == lrGameState.mbJunkyardPosIsLeft)
                    {
                        mfTimeInState                = 0.0f;
                        mbSafeToBeAttachedToCar      = false;
                        mbWaitingForCarToTouchGround = true;
                        mbWaitingForCarToSpawn       = false;
                        meState                      = E_STATE_ACTIVE;
                    }
                    else if (!mbWaitingToLookAtOriginalSelection)
                    {
                        mbWaitingForCarToTouchGround           = false;
                        mfTimeWaitingToLookAtOriginalSelection = 0.0f;
                        mbWaitingToLookAtOriginalSelection     = true;
                    }
                    else
                    {
                        mbWaitingForCarToTouchGround = false;
                    }
                }
                else if (lrGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame)
                {
                    mbWaitingToLookAtOriginalSelection = false;
                }
                else if (mbWaitingToLookAtOriginalSelection)
                {
                    if (mfTimeWaitingToLookAtOriginalSelection < KF_IMPACT_THRESHOLD)
                    {
                        mfTimeWaitingToLookAtOriginalSelection += lrSharedInfo.mfTimestep;
                    }
                    else
                    {
                        mbIsLeft = !mbIsLeft;
                        mTransitionCam.GetBehaviour()->ChangeMovie(
                            mbIsLeft ? mpLeftToRight : mpRightToLeft, lrResources);
                        mfTimeInState                = 0.0f;
                        mbWaitingForCarToSpawn       = true;
                        mbWaitingForCarToTouchGround = false;
                        meState                      = E_STATE_ACTIVE;
                    }
                }
            }
            else
            {
                ReturnToActive();
            }
            break;

        // ---- 11: IDLE -----------------------------------------------------------------
        case E_STATE_IDLE:
            GetNonConstCamera() = mIdleCam.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.meJunkyardState == GameState::E_JY_INACTIVE)
            {
                StartOutroMovie(lrSharedInfo);
                mIdleCam.Release();
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_WAITING_FOR_AUDIO)
            {
                StartWaitForAudioMovie(lrSharedInfo);
                mIdleCam.Release();
            }
            else if (lrGameState.meJunkyardState == GameState::E_JY_CAR_SELECT)
            {
                if (lrGameState.mbJunkyardPosJustChanged && !lrGameState.mbJunkyardCarModActive)
                {
                    if (mbIsLeft != lrGameState.mbJunkyardPosIsLeft)
                    {
                        mTransitionCam.GetBehaviour()->ChangeMovie(
                            lrGameState.mbJunkyardPosIsLeft ? mpRightToLeft : mpLeftToRight,
                            lrResources);
                    }
                    mbIsLeft = lrGameState.mbJunkyardPosIsLeft;
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                    Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface,
                                                  KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                    mbSafeToBeAttachedToCar = false;
                    mbWaitingForCarToSpawn  = true;
                    mToGameplayInterpolater.Release();
                    mfTimeInState = 0.0f;
                    mIdleCam.Release();
                    meState = E_STATE_ACTIVE;
                }
                else if (lrGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame &&
                         !lrGameState.mbJunkyardCarModActive)
                {
                    mbIsLeft = !mbIsLeft;
                    mTransitionCam.GetBehaviour()->ChangeMovie(
                        mbIsLeft ? mpLeftToRight : mpRightToLeft, lrResources);
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                    Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface,
                                                  KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                    mToGameplayInterpolater.Release();
                    mfTimeInState                      = 0.0f;
                    mbWaitingToLookAtOriginalSelection = false;
                    mbWaitingForCarToSpawn             = true;
                    mbWaitingForCarToTouchGround       = false;
                    mIdleCam.Release();
                    meState = E_STATE_WAIT_FOR_CAR_DROP;
                }
                else if (lrGameState.mfPadInactiveTime == 0.0f)
                {
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD_ENTERING;
                    Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface,
                                                  KPC_HOOK_JUNK_CAR_SELECT, KF_HOOK_BLEND);
                    mToGameplayInterpolater.Prepare(KF_INTERPOLATE_FROM_BROWSE_CAM_SECS,
                                                    mTransitionCam.GetBehaviourHelperIndex(),
                                                    mLookAroundCarCam.GetBehaviourHelperIndex(),
                                                    &lrManager,
                                                    KI_INTERPOLATION_METHOD,
                                                    KI_INTERPOLATION_MAPPING);
                    mLookAroundCarCam.GetBehaviour()->BecomeSimilarTo(
                        mTransitionCam.GetProducedCamera(), *lrSharedInfo.mpAllVehicleData);
                    mIdleCam.Release();
                    mfTimeInState = 0.0f;
                    meState       = E_STATE_ROTATE_ABOUT_CAR;
                }
            }
            else
            {
                mToGameplayInterpolater.Release();
                mfTimeInState = 0.0f;
                mIdleCam.Release();
                meState = E_STATE_ACTIVE;
            }
            break;

        // ---- 12: WAIT_FOR_AUDIO -------------------------------------------------------
        case E_STATE_WAIT_FOR_AUDIO:
            GetNonConstCamera() = mIntroNewCars.GetProducedCamera();
            GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_JUNKYARD;

            if (lrGameState.meJunkyardState == GameState::E_JY_INACTIVE)
            {
                StartOutroMovie(lrSharedInfo);
                GetNonConstCamera().mState_uFlags &= ~KI_CAMERA_STATE_FOLLOW;
                if (mbIsFirstCarSelect)
                {
                    GetNonConstCamera().GetEffects().mbSetTimeOfDay = true;
                    GetNonConstCamera().GetEffects().mfTimeOfDay    = KF_OUTRO_TIME_OF_DAY;
                }
            }
            break;

        // ---- 13: CHANGING_TO_ROAMING --------------------------------------------------
        case E_STATE_CHANGING_TO_ROAMING:
            GetNonConstCamera() = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();
            lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
            // ⚠️ GATE: the X360 hands the frame to the ROAMING sibling here --
            //     `if ( mpStateContainer->GetState(E_STATE_ROAMING)->Prepare(info) ) {
            //          mpStateContainer->SetCurrentState(E_STATE_ROAMING); Release(info); }`
            //   Release() is NOT in this TU's X360 export set (see the banner), so the
            //   hand-off's second half cannot be written faithfully, and running only the first
            //   half would leave this state current with its behaviours still held.
            //   CONSEQUENCE: the junkyard outro does not hand back to roaming.
            //   DELETE-WHEN: ArbStateCarSelect::Release is recovered.
            break;

        default:
            FireAssert("unhandled state", 965);
            break;
        }

        // ---- the shared per-frame tail (X360 LABEL_194) --------------------------------
        // The livery-change fade pair, the impact shake, and the time-in-state accumulate.
        if (lrGameState.mbJunkyardCarModActive && lrGameState.mbJunkyardPosJustChanged)
        {
            if (mfTimeBeforeFadeInAfterLiveryChange <= 0.0f)
            {
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_LIVERY_IN, KF_HOOK_BLEND);
            }
            else
            {
                mfTimeBeforeFadeInAfterLiveryChange = 0.0f;
            }
        }
        if (lrGameState.mbJunkyardCarModActive && lrGameState.mbJunkyardPlayerRespawnedThisFrame)
            mfTimeBeforeFadeInAfterLiveryChange = KF_LIVERY_FADE_DELAY_SECS;

        if (mfTimeBeforeFadeInAfterLiveryChange > 0.0f)
        {
            mfTimeBeforeFadeInAfterLiveryChange -= lrSharedInfo.mfTimestep;
            if (mfTimeBeforeFadeInAfterLiveryChange <= 0.0f)
            {
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_LIVERY_OUT, KF_HOOK_BLEND);
                mfTimeBeforeFadeInAfterLiveryChange = 0.0f;
            }
        }

        // ⚠️ GATE: the per-frame impact shake. X360 (with meState != INACTIVE):
        //     CameraImpactEffect::Update( mImpactEffect, mCamera,
        //         <stack Parameters {shake 0.06, 0.0, 1.15, 0.11; decay 0.05;
        //                            magnitude 15.0; freqScale 5.0}>,
        //         *info.mpRandom, info.mfTimestep * 5.0f, mImpactFactor * 15.0f );
        //     mImpactFactor += -mImpactFactor * 0.05f;   // the decay-factor fold
        //   The committed CameraImpactEffect declares RegisterImpact only -- Update has no
        //   declaration, its embedded CameraShake is a 16-byte OPAQUE blob, and
        //   CameraShake::Parameters' four field names are not recovered, so the seven staged
        //   tunings cannot be assigned to named members. Writing the shake through the opaque
        //   blob would be an offset hack. The IMPACT REGISTRATION half above is real.
        //   CONSEQUENCE: the junkyard car-drop shake is registered but never played out.
        //   DELETE-WHEN: CameraImpactEffect::Update + CameraShake::Parameters are homed.

        mfTimeInState += lrSharedInfo.mfTimestep;
    }
}
