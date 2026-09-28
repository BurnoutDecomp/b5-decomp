#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (unhandled-state assert)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"        // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"             // Camera effect-hook free functions
#include "GameSource/Director/BrnDirectorICEWrapper.h"                      // ICEWrapper::PlayMovie / ICEPlayingMovie
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"   // Camera::BehaviourRoadRunner (slice)
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                    // Camera::BehaviourManager (complete)
#include "GameSource/Director/Camera/Camera.h"                              // Camera::Camera (operator= / mState_uFlags)
#include "rw/math/vpu/types.h"                                              // rw::math::vpu::Vector3
#include "rw/math/vpu/vector3_operation.h"                                  // MagnitudeSquared / operator-
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"          // Camera::VehicleInfo (mpPlayerCar, by name)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::GetSharedPlaylists (Prepare)
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"        // SharedCameraContainer::GetGameplayCameraHelperIndex
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"              // BrnDirector::Timestep::E_GAME (the fly-by's clock)
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"             // ConsoleVpu::Dot3 (the vmsum3fp128 model, rule 1)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // [diag] CgsDev::Log::gpDebugPrint
#include <cstdlib>                                                        // [diag] getenv (BRN_CRASHCAM_DIAG)
#include <cmath>                                                          // [diag] std::sqrt (the sample line's carDist)

// ============================================================================
// BrnDirector::ArbStateCrashNav -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x82266010
//   GetName    @0x821F6300
//   Release    @0x8224FAA0
//   Update     @0x8226DC98
//
// The crash-nav (picture-paradise) director state: it drives the road-runner fly-by camera after a
// crash, plays an ICE movie alongside it, fades in/out of black between the fly-by segments, can
// turn the take about to show the way back, and hands control back to gameplay once the take has
// finished or the game stops requesting crash-nav. All member access is BY NAME; the camera copy /
// fade hooks go through named Camera setters + the effect-trigger free functions, the road-runner
// behaviour is driven through its handle's named accessors, and the ICE playback goes through the
// ICEMoviePlayer / ICEWrapper. The GameState / player-car snapshot it reacts to is read by named
// members.
// ============================================================================

namespace BrnDirector
{
    namespace
    {
        // ---- shared .rdata literals the X360 loads (IEEE-754 patterns the asm references) ----
        const f32 KF_UNIT = 1.0f;   // flt_82001C98 (the EnsureEffectIsPlaying blend)
        const f32 KF_ZERO = 0.0f;   // flt_82001CC0

        // The crash-nav per-frame depth-of-field "blurriness" the fly-by camera requests
        // (SF_CRASHNAV_BLURRINESS, loaded as flt_82CDA4E0 into the camera's running-blur lane each
        // ACTIVE/TURNABOUT/WAITING frame: lfs @0x8226DDB0 / 0x8226DF54 / 0x8226E150, stfs camera
        // +0x134). VALUE READ FROM THE IMAGE (tools/re/x360rd.py 0x82CDA4E0): 3F800000 == 1.0.
        const f32 SF_CRASHNAV_BLURRINESS = 1.0f;  // flt_82CDA4E0 == 3F800000

        // The two squared-distance gates the fly-by turnabout / re-entry tests the player's distance
        // from the crash-nav camera position against (the X360 vmsum3fp128 squared length compared by
        // vcmpgtfp against these constants). DWARF names them VecFloat; here the scalar single-lane
        // value is read (the project's de-modelled-VecFloat rule).
        //   OUTER (case 3, unk_82FAAAF0 @0x8226DE5C): squaredDist > OUTER -> fade and turn about (-> 4).
        //   INNER (case 4, unk_82FAA990 @0x8226E038): INNER > squaredDist -> come back in (-> 3).
        // ⭐ BOTH ARE .bss DYNAMIC-INIT CONSTANTS -- the image reads 0 at both addresses BY
        // DEFINITION, which is why the old body carried a 1.0 "undumped" placeholder for each
        // (OWNERLIST 2026-09-27, lane L5). tools/re/findinit.py names their CRT-init thunks, which
        // splat one .rdata float into all four lanes (lfs / lvlx / vspltw / stvx128):
        //   0x82C48488..0x82C484AC: lfs flt_8200D518 (481C4000 == 160000.0 == 400 m squared)
        //                           -> stvx128 unk_82FAAAF0
        //   0x82C484B0..0x82C484D4: lfs flt_8200D51C (47EF4200 == 122500.0 == 350 m squared)
        //                           -> stvx128 unk_82FAA990
        // With the 1.0 placeholders any fly-by frame 1 m from the car turned about after 2 s and
        // could never come back (no camera is ever inside 1 m of the car's centre).
        const f32 KF_MAX_CRASH_NAV_PIC_PARADISE_OUTER_DISTANCE_SQ = 160000.0f;  // unk_82FAAAF0 = splat(flt_8200D518)
        const f32 KF_MAX_CRASH_NAV_PIC_PARADISE_INNER_DISTANCE_SQ = 122500.0f;  // unk_82FAA990 = splat(flt_8200D51C)

        // The time-in-state thresholds the fly-by waits before testing the distance gates / the
        // turnabout completion (lfs flt_82001D9C == 2.0 for the distance gate; flt_82001CC0/C98
        // shared above). flt_82001CC0 == 0.0 (the running-blur reset is f30 == 0.0 in the asm).
        const f32 KF_DISTANCE_GATE_MIN_TIME_IN_STATE = 2.0f;  // flt_82001D9C (asm: time > 2.0)
        const f32 KF_TURNABOUT_MIN_TIME_IN_STATE     = 1.0f;  // flt_82001C98 (asm case 4: time > 1.0)

        // X360 dirty-flag bit Update OR-pokes into the camera state's flag word every Update exit
        // (the "small near clip" bit; oris +0x150 by 1 -> bit 16).
        const s32 KU_CAMERA_DIRTY_SMALL_NEAR_CLIP = 0x10000;

        // ---- Prepare @0x822660A8's blend timings ------------------------------------------
        // ⭐ VALUES READ OUT OF THE IMAGE, not off the pseudocode (tools/re/x360rd.py):
        //   flt_82003F40 = 3E800000 = 0.25   (the lfs into f1 for InterpolateFrom @0x82266150)
        //   flt_82001DA0 = 3F000000 = 0.5    (the lfs into f0 stored to BOTH out-blend floats)
        // Both also render as 0.25 / 0.5 in Hex-Rays, so each has two independent derivations.
        const f32 KF_INTERPOLATE_IN_DURATION     = 0.25f;  // flt_82003F40
        const f32 KF_INTERPOLATE_OUT_DURATION    = 0.5f;   // flt_82001DA0
        const f32 KF_INTERPOLATE_OUT_MAX_OVERLAP = 0.5f;   // flt_82001DA0 (the same constant)

        // Both blends keep running while the simulation is paused -- which is the point of the
        // whole state. X360: r8 = 1 into InterpolateFrom, stb 1 -> player +0x6A2.
        const bool KB_INTERPOLATE_UPDATES_DURING_PAUSE = true;

        // BehaviourManager::NewBehaviour's owner / ref-limit slots (X360 r6 = 0, r7 = 1). Same
        // pair the other arbitrator states pass; named identically to BrnArbStateOnlineRaceIntro.
        const void* const KPC_NEW_BEHAVIOUR_OWNER   = 0;
        const s32         KI_NEW_BEHAVIOUR_REFLIMIT = 1;

        // Squared distance between the player car position and the crash-nav camera position. The
        // X360 loads the player position from the shared-info player-car (the lvx128 at player-car
        // +0x220, i.e. VehicleInfo::GetWorldPosition), the camera EYE position from this state's
        // camera +0x30 (mTransform's translation row, asm `lvx128 v13, this, 0x40` with mCamera
        // embedded at this+0x10 -> camera+0x30) -- NOT mSubject (+0x40, the look-at target) --
        // subtracts them (`vsubfp v0, v13, v0` == camera - player, @0x8226DE44 / 0x8226E020), and
        // squares-and-sums the xyz lanes (`vmsum3fp128 v0, v0, v0` @0x8226DE50 / 0x8226E02C).
        // Rounding (ROUNDING_RULE.md): the vsubfp lanes are rounded each (rule 4, the vpu
        // operator-); the vmsum3fp128 is ONE rounding of the f64 sum (rule 1, ConsoleVpu::Dot3).
        // FLAG (model): vmsum = one rounding of the f64 sum (xenia DOT_PRODUCT_3). Rule 6: a
        // denormal lane or sum needs the eye within ~1e-19 m of the car's origin -- not reachable.
        inline f32 PlayerToCameraDistanceSquared(const ArbStateSharedInfo& lrSharedInfo,
                                                  const Camera::Camera& lrCamera)
        {
            // ✅ THE OFFSET HACK IS GONE (2026-08-29, crash-camera wave). The lvx128 at
            // player-car +0x220 is mRaceCarState.mTransform (@496 == 0x1F0) + 0x30 -- the
            // transform's position row. The reason it WAS an offset read ("mpPlayerCar's type
            // BrnDirector::VehicleInfo is forward-declared only") was the namespace fork, now
            // retired: the member is a `const Camera::VehicleInfo*`.
            const rw::math::vpu::Vector3& lrPlayerPos =
                lrSharedInfo.mpPlayerCar->mRaceCarState.mTransform.Pos();
            const rw::math::vpu::Vector3& lrCameraPos =
                reinterpret_cast<const rw::math::vpu::Vector3&>(lrCamera.GetTransform().wAxis);

            const rw::math::vpu::Vector3 lDelta = lrCameraPos - lrPlayerPos;          // vsubfp
            return Camera::Utils::ConsoleVpu::Dot3(lDelta, lDelta);                   // vmsum3fp128
        }

        // The player-car "keep crash-nav running" gate byte.
        // ⭐ THE NAME IS RECOVERED (2026-08-29, crash-camera wave) and the offset hack with it:
        // +0x44D == 1101 == RaceCarState::mbStartedDeforming (BrnVehicleEvents.h:88 maps the
        // serialised @1098/@1099/@1101 triple back to physics +0x710/+0x711/+0x712 ->
        // mbCrashing / mbIsFatalyCrashing / mbStartedDeforming). The old FLAG here said the
        // byte's DWARF name was "not recovered"; it was only unREACHABLE, because mpPlayerCar
        // was typed as the BrnDirector::VehicleInfo namespace fork. That fork is retired.
        // ⚠️ The ROLE reading is unchanged and still asm-only: this byte is OR'd with
        // mbDoing100PercentSequence to keep the fly-by driving.
        inline bool PlayerCarWantsCrashNav(const ArbStateSharedInfo& lrSharedInfo)
        {
            return lrSharedInfo.mpPlayerCar->mRaceCarState.mbStartedDeforming;
        }

        // Whether the road-runner fly-by should keep driving the state camera this frame: the take
        // has NOT finished AND the game still wants crash-nav (the player car wants it, or the 100%
        // sequence is running). The X360 condition (case 3/6): !roadRunner.HasFinished() &&
        // (mpPlayerCar[+0x44D] || mpGameState->mbDoing100PercentSequence).
        bool ShouldFlyByCameraDrive(const ArbStateSharedInfo& lrSharedInfo,
                                    Camera::BehaviourRoadRunner& lrFlyBy)
        {
            return !lrFlyBy.HasFinished() &&
                   (PlayerCarWantsCrashNav(lrSharedInfo) ||
                    lrSharedInfo.mpGameState->mbDoing100PercentSequence);
        }
    }

    // ------------------------------------------------------------------------
    // Construct @0x82266010 -- build the camera + ICE movie player, clear the base camera flags, and
    // zero the behaviour handles / interpolation params / state machine.
    // ------------------------------------------------------------------------
    void ArbStateCrashNav::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x171 / +0x170

        meState = E_STATE_INACTIVE;        // +0x878 = 0

        mICEMoviePlayer.Construct();       // X360 ICEMoviePlayer::Construct(this+0x190)

        // The behaviour handles start unallocated (+0x840 / +0x864 blocks zeroed: a byte store
        // for mbAllocated then four word stores for the rest of the five-word handle).
        mInterpolater.Clear();
        mRoadRunnerCam.Clear();

        // ⭐ THE INTERPOLATION PARAMS ARE NO LONGER FOUR OPAQUE WORDS (2026-08-29).
        // The X360 store sequence at 0x82266048..0x82266078 writes +0x854..+0x860 TWICE over,
        // and the doubled write is the tell -- it is the inlined default
        // BehaviourInterpolate::Parameters::Construct() followed by two explicit assignments:
        //     stw 8,   0x854   ; mType = 8 (the interpolate behaviour-type tag)
        //     stw 0,   0x858   ; SetDebugName(0)
        //     stw 0,   0x85C   ; meInterpolationMethod  = E_METHOD_SLERP        <- Construct()
        //     stw 1,   0x860   ; meInterpolationMapping = E_MAPPING_SINUSOIDAL  <- Construct()
        //     stw 1,   0x860   ; meInterpolationMapping = E_MAPPING_SINUSOIDAL  <- explicit
        //     stw 1,   0x85C   ; meInterpolationMethod  = ...ROTATE_ABOUT_PLAYER_CAR <- explicit
        // i.e. the final pair is {method = 1, mapping = 1}. Recovering the type turns the old
        // "[+0x08]=1" note into a MEANING: the crash-nav / pause blend ORBITS THE PLAYER CAR
        // rather than slerping between two framings, which is what the retail pause camera does.
        mInterpolateParams.Construct();
        mInterpolateParams.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;   // +0x85C
        mInterpolateParams.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_SINUSOIDAL;               // +0x860

        // NOTE: Construct does NOT seed muCurrentIceMovie (+0x87C), mfTimeInState (+0x880) or
        // mbHasReversed (+0x884) -- the X360 store range ends at +0x874. They are set on entry to
        // the active states (mfTimeInState on the PREPARING success edge; mbHasReversed in the
        // turnabout).
    }

    // ------------------------------------------------------------------------
    // ⭐⭐⭐ Prepare @0x822660A8 -- THE ENTRY POINT OF THE MOVING PAUSE CAMERA.
    //
    // Enter crash-nav: snapshot whatever ICE take was playing, load the SHARED PAUSE PLAYLIST
    // into this state's own movie player, blend into it from the live gameplay camera, latch the
    // blend back out for when it ends, and LOOP it; then allocate the road-runner fly-by
    // behaviour and mark it "keeps updating while the sim is paused". Returns whether the fly-by
    // behaviour has finished waiting to be prepared (i.e. whether the state may go ACTIVE).
    //
    // ⭐ WHY THIS IS THE WHOLE "camera still moving" HALF OF THE PAUSE LOOK. Verified against the
    //    ARTIST xref table: this function is the ONLY caller of ICEMoviePlayer::Prepare
    //    (@0x821F7CC8), ICEMoviePlayer::Loop (@0x82251340) and ICEMoviePlayer::InterpolateFrom
    //    (@0x82263A48) in the entire image. Nothing else in the game drives an ICEMoviePlayer.
    //    So "the pause world's camera keeps moving" is not a property of the pause SCREEN at all
    //    -- it is this one function looping SharedPlaylists::GetPausePlaylist through a movie
    //    player whose behaviours are flagged to tick through the pause.
    //
    // ⭐ THE THREE INDEPENDENT "KEEP GOING WHILE PAUSED" SWITCHES, all asm-attested here:
    //      1. InterpolateFrom(..., lbUpdatesDuringPause = true)      (r8 = 1 @0x82266140)
    //      2. WhenFinishedInterpolateTo(..., lbUpdatesDuringPause = true)
    //                                                    (stb 1, player+0x6A2 @0x8226618C)
    //      3. mRoadRunnerCam.SetUpdatesDuringPause(true) (sub_82212B28(handle, 1) @0x822661C4)
    //    plus the fly-by behaviour's timestep flavour being switched off world time (below).
    //
    // The X360 pseudocode renders InterpolateFrom with an INVENTED extra parameter (`v8`): the
    // f32 duration takes f1 and EATS the r6 integer slot, so Hex-Rays sees an unset r6 and
    // renumbers everything after it. The argument list below is taken from the ASM
    // (r3=this, r4=manager, r5=fromHelper, [r6 skipped], r7=params, r8=flag, f1=0.25).
    // ------------------------------------------------------------------------
    bool ArbStateCrashNav::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already entered -- Prepare is idempotent (the arbitrator calls it on the trigger edge
        // and Update's PREPARING case calls it again on the next tick). X360: `lwz r11, 0x878;
        // if (r11) return true;`
        if (meState != E_STATE_INACTIVE)
        {
            return true;
        }

        meState = E_STATE_PREPARING;   // +0x878 = 1

        // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. One line per entry edge.
        if (getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[pause-cam] ArbStateCrashNav::Prepare INACTIVE -> PREPARING (movie player playing "
                << (mICEMoviePlayer.IsPlaying() ? 1 : 0) << ", pause playlist "
                << lrSharedInfo.mpStateContainer->GetSharedPlaylists().GetPausePlaylist().GetMovieCount()
                << " takes, fly-by allocated " << (mRoadRunnerCam.IsAllocated() ? 1 : 0) << ")\n";
        }

        // The movie player is only set up once; a re-entry while it is still playing keeps the
        // take that is already running (X360 `lbz r11, 0x830` -> mICEMoviePlayer.IsPlaying()).
        if (!mICEMoviePlayer.IsPlaying())
        {
            // Remember the take the ICE wrapper was playing, so the exit paths in Update() can
            // put it back (mICEPlayingMovie is read there by mID / mfPlaybackPositionParameter /
            // mbIsValid). X360 copies the 16-byte return value with two ld/std pairs.
            mICEPlayingMovie = lrSharedInfo.mpICEWrapper->GetCurrentMovie();

            // ⭐ THE PAUSE PLAYLIST. X360 emits `memcpy(this+0x2F0, GetPausePlaylist(), 0x4E8)`,
            // which is the by-value ICEMoviePlaylist assignment inside SetPlaylist. The source
            // pointer is shared-info +0x14 -- the ArbitratorStateContainer -- because
            // SharedPlaylists is that container's FIRST member (DWARF
            // BrnDirectorArbitratorStateContainer.h:50), so the console's inlined
            // GetSharedPlaylists() is a no-op cast. Reached BY NAME here.
            mICEMoviePlayer.SetPlaylist(
                lrSharedInfo.mpStateContainer->GetSharedPlaylists().GetPausePlaylist());

            mICEMoviePlayer.Prepare(lrSharedInfo.mpICEWrapper);

            // Blend INTO the playlist from whatever gameplay camera is live, over 0.25 s, using
            // this state's orbit-the-car curve, ticking through the pause.
            mICEMoviePlayer.InterpolateFrom(
                *lrSharedInfo.mpBehaviourManager,
                lrSharedInfo.mpSharedCameraContainer->GetGameplayCameraHelperIndex(),
                KF_INTERPOLATE_IN_DURATION,
                &mInterpolateParams,
                KB_INTERPOLATE_UPDATES_DURING_PAUSE);

            // ...and latch the blend BACK OUT to that same gameplay camera for when the playlist
            // ends (0.5 s, may overlap the last take by 0.5 s). The X360 inlines this as six
            // stores into the player; the second GetGameplayCameraHelperIndex() call is the
            // console's, not a duplication introduced here.
            mICEMoviePlayer.WhenFinishedInterpolateTo(
                lrSharedInfo.mpSharedCameraContainer->GetGameplayCameraHelperIndex(),
                KF_INTERPOLATE_OUT_DURATION,
                KF_INTERPOLATE_OUT_MAX_OVERLAP,
                &mInterpolateParams,
                KB_INTERPOLATE_UPDATES_DURING_PAUSE);

            // ⭐ AND LOOP IT. Loop() asserts the wrapper is set and that nothing is playing,
            // calls Play(), and raises mbIsLooping -- so the pause take list runs for as long as
            // the screen is up instead of ending on one take.
            mICEMoviePlayer.Loop();
        }

        // The road-runner fly-by behaviour is likewise allocated once.
        if (!mRoadRunnerCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourRoadRunner>(
                mRoadRunnerCam, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

            // ⭐ Keep the fly-by ticking while the simulation is frozen. X360 sub_82212B28 is
            // BehaviourHandle<T>::SetUpdatesDuringPause (identified by its own assert:
            // "IsAllocated()", BrnBehaviourManager.h:676, then
            // BehaviourManager::SetBehaviourUpdatesDuringPause(mpManager, muAllocationKey, arg)).
            mRoadRunnerCam.SetUpdatesDuringPause(true);

            // ...and drive it off the GAME timestep rather than the world one. X360:
            //     bl BehaviourManager::BehaviourHandle(handle)   ; == handle.GetBehaviour(),
            //                                                    ;    assert @ BrnBehaviourManager.h:589
            //     li  r11, 2 ; stw r11, 4(r3)                    ; behaviour +0x04
            // and Camera::Behaviour +0x04 is meTimestepType (Timestep::EType), whose value 2 is
            // E_GAME. That is the fourth "ignore the paused world clock" switch.
            mRoadRunnerCam.GetBehaviour()->SetTimestepType(BrnDirector::Timestep::E_GAME);
        }

        // Ready once the fly-by behaviour is no longer queued for its first Prepare. X360:
        // `result = BehaviourRoadRun(handle) == 0`, where the truncated symbol @0x82212AC8 is
        // BehaviourHandle<BehaviourRoadRunner>::IsWaitingToPrepare (assert "mbIsAllocated",
        // BrnBehaviourManager.h:517, then manager IsBehaviourWaitingToPrepare).
        return !mRoadRunnerCam.IsWaitingToPrepare();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6300
    // ------------------------------------------------------------------------
    const char* ArbStateCrashNav::GetName() const
    {
        return "ArbStateCrashNav";
    }

    // ------------------------------------------------------------------------
    // Release @0x8224FAA0 -- leave crash nav: drop to INACTIVE, stop the ICE movie if one is
    // playing, release both behaviour handles back to the manager, and assert no behaviours remain
    // allocated by this state.
    // ------------------------------------------------------------------------
    bool ArbStateCrashNav::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        // X360 reads mICEMoviePlayer.mbIsPlaying (the byte at +0x830) BEFORE clearing meState.
        const bool lbWasPlaying = mICEMoviePlayer.IsPlaying();

        meState = E_STATE_INACTIVE;        // +0x878 = 0

        if (lbWasPlaying)
        {
            mICEMoviePlayer.Stop();        // X360 ICEMoviePlayer::Stop(this+0x190)
        }

        // Both handles go back to the manager (road-runner first, matching the X360 store
        // order). BehaviourHandle::Release @0x8222DD00 is exactly the guarded
        // UnSetBehaviourUsedByHandle + five-word clear this used to spell out inline.
        mRoadRunnerCam.Release();          // +0x864 block (released first)
        mInterpolater.Release();           // +0x840 block

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }

    // ------------------------------------------------------------------------
    // Update @0x8226DC98 -- the crash-nav per-frame tick. Dispatches on meState. Only the X360
    // jump-table cases 0/1/3/4/6 are walked here (cases 2/5/7 hit the default "unhandled state"
    // assert -- they are driven by the other crash-nav TUs). After the switch, the time-in-state is
    // accumulated and the camera's "small near clip" dirty bit is raised.
    // ------------------------------------------------------------------------
    void ArbStateCrashNav::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera = GetNonConstCamera();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;   // fall through to the shared exit

        case E_STATE_PREPARING:
            // Run Prepare (X360 virtual call (*(*this+4))(this, info)); on success seed the
            // time-in-state, enter ACTIVE, and run the ACTIVE body this same frame.
            if (Prepare(lrSharedInfo))
            {
                mfTimeInState = 0.0f;          // +0x880 = 0.0
                meState       = E_STATE_ACTIVE; // +0x878 = 3
                goto active_body;
            }
            break;

        case E_STATE_ACTIVE:
        active_body:
        {
            // Whether the road-runner fly-by should keep driving the state camera this frame.
            const bool lbDriveFlyByCamera =
                ShouldFlyByCameraDrive(lrSharedInfo, *mRoadRunnerCam.GetBehaviour());

            mICEMoviePlayer.Update(*lrSharedInfo.mpBehaviourManager);

            if (lbDriveFlyByCamera)
            {
                // Drive the state camera from the road-runner behaviour and request the crash-nav
                // depth-of-field blurriness (DOF blurriness lane + border post-FX cleared).
                lrCamera = mRoadRunnerCam.GetProducedCamera();
                lrCamera.GetDepthOfField().SetBlurriness(SF_CRASHNAV_BLURRINESS);  // camera +0x134
                lrCamera.SetRequestedBorderPostFX(KF_ZERO);                        // camera +0x110
            }
            else
            {
                // The fly-by is not driving: take the ICE movie player's camera instead.
                lrCamera = mICEMoviePlayer.GetCamera();
            }

            // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. Every 30th ACTIVE frame: which source
            // drives the pause camera, the ICE take's playback position, and where the ICE camera is.
            {
                static s32 siPauseCamDiagFrame = 0;
                if (getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0 &&
                    (siPauseCamDiagFrame++ % 30) == 0)
                {
                    const ICEPlayingMovie lMovie = lrSharedInfo.mpICEWrapper->GetCurrentMovie();
                    const Camera::Camera* lpIceCamera = lrSharedInfo.mpICEWrapper->GetCamera();
                    const rw::math::vpu::Vector3 lIceEye = lpIceCamera->GetTransform().wAxis;
                    const rw::math::vpu::Vector3 lStateEye = lrCamera.GetTransform().wAxis;
                    *CgsDev::Log::gpDebugPrint
                        << "[pause-cam] sample t " << mfTimeInState << " flyby " << (lbDriveFlyByCamera ? 1 : 0)
                        << " inInterp " << (mICEMoviePlayer.mInInterpolator.IsAllocated() ? 1 : 0)
                        << " outInterp " << (mICEMoviePlayer.mOutInterpolator.IsAllocated() ? 1 : 0)
                        << " wrapperPlaying " << (lrSharedInfo.mpICEWrapper->IsPlayingMovie() ? 1 : 0)
                        << " movieValid " << (lMovie.mbIsValid ? 1 : 0) << " pos " << lMovie.mfPlaybackPositionParameter
                        << " iceEye (" << lIceEye.x << ", " << lIceEye.y << ", " << lIceEye.z << ")"
                        << " stateEye (" << lStateEye.x << ", " << lStateEye.y << ", " << lStateEye.z << ")"
                        << " carDist " << std::sqrt(PlayerToCameraDistanceSquared(lrSharedInfo, lrCamera)) << "\n";
                }
            }

            // Fade IN of black while the take wants it, OUT otherwise.
            const char* lpcFadeHook = mRoadRunnerCam.GetBehaviour()->ShouldFadeBlackIn()
                                          ? "Black_In_BW" : "Black_Out_BW";
            Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                          lpcFadeHook, KF_UNIT);

            // Once the fly-by has spent long enough AND the player has flown past the OUTER
            // distance gate, fade to black and turn the take about (-> ACTIVE_TURNABOUT).
            if (lbDriveFlyByCamera &&
                mfTimeInState > KF_DISTANCE_GATE_MIN_TIME_IN_STATE &&
                PlayerToCameraDistanceSquared(lrSharedInfo, lrCamera) >
                    KF_MAX_CRASH_NAV_PIC_PARADISE_OUTER_DISTANCE_SQ)
            {
                Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                              "Black_In_BW", KF_UNIT);
                mfTimeInState = 0.0f;          // +0x880 = 0.0
                mbHasReversed = false;         // +0x884 = 0
                meState       = E_STATE_ACTIVE_TURNABOUT;  // +0x878 = 4
            }

            // If the game has stopped requesting crash-nav, hand off to the gameplay camera.
            if (!lrSharedInfo.mpGameState->mbCrashNavShown &&
                !lrSharedInfo.mpGameState->mbDoing100PercentSequence)
            {
                meState = E_STATE_WAITING_TO_STOP;   // +0x878 = 6
                mICEMoviePlayer.CutToInterpolateOut();
                lrCamera.ClearCrashNavEffectGate();  // stb 0, camera +0x11F (effects gate byte)
                Camera::StopCurrentEffect(lrCamera, *lrSharedInfo.mpEffectInterface);

                if (lbDriveFlyByCamera)
                {
                    if (mICEPlayingMovie.mbIsValid)
                    {
                        lrSharedInfo.mpICEWrapper->PlayMovie(
                            mICEPlayingMovie.mID, mICEPlayingMovie.mfPlaybackPositionParameter,
                            static_cast<VehicleRef::EType>(0),
                            static_cast<EActiveRaceCarIndex>(0));
                    }
                    Release(lrSharedInfo);   // X360 LABEL_50: tail virtual Release call (vtable +0xC)
                }
            }
            break;
        }

        case E_STATE_ACTIVE_TURNABOUT:
        {
            mICEMoviePlayer.Update(*lrSharedInfo.mpBehaviourManager);

            // While turning about, the road-runner fly-by always drives the camera (with the
            // crash-nav blurriness).
            lrCamera = mRoadRunnerCam.GetProducedCamera();
            lrCamera.GetDepthOfField().SetBlurriness(SF_CRASHNAV_BLURRINESS);  // camera +0x134
            lrCamera.SetRequestedBorderPostFX(KF_ZERO);                        // camera +0x110

            // Once past the turnabout time threshold: flip the take's travel direction (one shot,
            // latched by mbHasReversed) and, unless the take wants the IN fade, start the OUT fade.
            if (mfTimeInState > KF_TURNABOUT_MIN_TIME_IN_STATE)
            {
                if (!mbHasReversed)
                {
                    Camera::BehaviourRoadRunner* lpFlyBy = mRoadRunnerCam.GetBehaviour();
                    lpFlyBy->ClearSmoothingForNextSample();   // stb 0, behaviour +0x08
                    lpFlyBy->ReverseTravelDirection();        // negate behaviour +0x29C/+0x294/+0xA0
                }
                mbHasReversed = true;   // +0x884 = 1

                if (!mRoadRunnerCam.GetBehaviour()->ShouldFadeBlackIn())
                {
                    Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                                  "Black_Out_BW", KF_UNIT);
                }
            }

            // Once the player has come back inside the INNER distance gate (and enough time has
            // passed), drop back into the forward fly-by (-> ACTIVE).
            if (mfTimeInState > KF_DISTANCE_GATE_MIN_TIME_IN_STATE &&
                KF_MAX_CRASH_NAV_PIC_PARADISE_INNER_DISTANCE_SQ >
                    PlayerToCameraDistanceSquared(lrSharedInfo, lrCamera))
            {
                mfTimeInState = 0.0f;          // +0x880 = 0.0
                meState       = E_STATE_ACTIVE; // +0x878 = 3
            }

            // If the game has stopped requesting crash-nav, hand off to the gameplay camera.
            if (!lrSharedInfo.mpGameState->mbCrashNavShown &&
                !lrSharedInfo.mpGameState->mbDoing100PercentSequence)
            {
                meState = E_STATE_WAITING_TO_STOP;   // +0x878 = 6
                mICEMoviePlayer.CutToInterpolateOut();
                Camera::StopCurrentEffect(lrCamera, *lrSharedInfo.mpEffectInterface);
                if (mICEPlayingMovie.mbIsValid)
                {
                    lrSharedInfo.mpICEWrapper->PlayMovie(
                        mICEPlayingMovie.mID, mICEPlayingMovie.mfPlaybackPositionParameter,
                        static_cast<VehicleRef::EType>(0),
                        static_cast<EActiveRaceCarIndex>(0));
                }
                Release(lrSharedInfo);   // X360 LABEL_50: tail virtual Release call
            }
            break;
        }

        case E_STATE_WAITING_TO_STOP:
        {
            // Keep driving the camera while the ICE movie interpolates out, then -- once the movie
            // has reached its end or the fly-by has finished -- replay the snapshot movie and hand
            // off to the gameplay camera.
            const bool lbDriveFlyByCamera =
                ShouldFlyByCameraDrive(lrSharedInfo, *mRoadRunnerCam.GetBehaviour());

            mICEMoviePlayer.Update(*lrSharedInfo.mpBehaviourManager);

            if (lbDriveFlyByCamera)
            {
                lrCamera = mRoadRunnerCam.GetProducedCamera();
                lrCamera.GetDepthOfField().SetBlurriness(SF_CRASHNAV_BLURRINESS);  // camera +0x134
                lrCamera.SetRequestedBorderPostFX(KF_ZERO);                        // camera +0x110
            }
            else
            {
                lrCamera = mICEMoviePlayer.GetCamera();
            }

            // X360 case-6 exit condition: the ICE movie has reached its end, OR the fly-by has
            // finished.
            if (mICEMoviePlayer.HasReachedEnd() ||
                !mRoadRunnerCam.GetBehaviour()->HasFinished())
            {
                if (mICEPlayingMovie.mbIsValid)
                {
                    lrSharedInfo.mpICEWrapper->PlayMovie(
                        mICEPlayingMovie.mID, mICEPlayingMovie.mfPlaybackPositionParameter,
                        static_cast<VehicleRef::EType>(0),
                        static_cast<EActiveRaceCarIndex>(0));
                }
                Release(lrSharedInfo);   // X360 LABEL_50: tail virtual Release call
            }
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        // Shared exit (X360 LABEL after the switch): accumulate the time-in-state by the frame
        // timestep, then raise the camera's "small near clip" dirty bit.
        mfTimeInState += lrSharedInfo.mfTimestep;                       // +0x880 += *(a2+0x5C)
        GetNonConstCamera().mState_uFlags |= KU_CAMERA_DIRTY_SMALL_NEAR_CLIP;  // oris +0x150, 1

        // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. One line per meState edge, with the
        // fly-by / movie-player camera choice and the fade hook the camera now carries.
        {
            static EState seLastState = E_STATE_INACTIVE;
            if (meState != seLastState && getenv("BRN_CRASHCAM_DIAG") != 0 &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                const Camera::CameraEffects& lrEffects = GetNonConstCamera().GetEffects();
                *CgsDev::Log::gpDebugPrint
                    << "[pause-cam] ArbStateCrashNav state " << static_cast<s32>(seLastState) << " -> "
                    << static_cast<s32>(meState) << " (time " << mfTimeInState << ", fly-by finished "
                    << ((mRoadRunnerCam.IsAllocated() && mRoadRunnerCam.GetBehaviour()->HasFinished()) ? 1 : 0)
                    << ", movie playing " << (mICEMoviePlayer.IsPlaying() ? 1 : 0) << ", start hook '"
                    << (lrEffects.mbHasStartHookNameString ? lrEffects.mStartHookNameString.mHookNameString : "")
                    << "')\n";
            }
            seLastState = meState;
        }
    }
}
