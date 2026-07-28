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
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // ---- shared .rdata literals the X360 loads (IEEE-754 patterns the asm references) ----
        const f32 KF_UNIT = 1.0f;   // flt_82001C98 (the EnsureEffectIsPlaying blend)
        const f32 KF_ZERO = 0.0f;   // flt_82001CC0

        // The crash-nav per-frame depth-of-field "blurriness" the fly-by camera requests
        // (SF_CRASHNAV_BLURRINESS, loaded as flt_82CDA4E0 into the camera's running-blur lane each
        // ACTIVE/TURNABOUT/WAITING frame). FLAG (value): flt_82CDA4E0 is not in any available rodata
        // dump, so the VALUE below is a flagged placeholder -- only the symbol->member mapping
        // (read off the load offset) is asm-attested. Pin the real value when the 0x82CDA4E0 rodata
        // is dumped.
        const f32 SF_CRASHNAV_BLURRINESS = 1.0f;  // flt_82CDA4E0  FLAG: value undumped (placeholder)

        // The two squared-distance gates the fly-by turnabout / re-entry tests the player's distance
        // from the crash-nav camera position against (the X360 vmsum3fp128 squared length compared by
        // vcmpgtfp against these rodata constants). DWARF names them VecFloat; here the scalar
        // single-lane value is read (the project's de-modelled-VecFloat rule).
        //   OUTER (case 3, unk_82FAAAF0): squaredDist > OUTER -> blend OUT and turn about (-> case 4).
        //   INNER (case 4, unk_82FAA990): squaredDist < INNER -> come back in (-> case 3).
        // FLAG (values): unk_82FAAAF0 / unk_82FAA990 are not in any available rodata dump, so the
        // VALUES below are flagged placeholders -- only the symbol->member mapping (read off the
        // load offsets) is asm-attested. The comparison DIRECTIONS are X360-attested (the OUTER test
        // is `dist > OUTER`; the INNER test is `INNER > dist`). Pin the real values when the
        // 0x82FAAAF0 / 0x82FAA990 rodata is dumped. Do NOT treat these magnitudes as ground truth.
        const f32 KF_MAX_CRASH_NAV_PIC_PARADISE_OUTER_DISTANCE_SQ = 1.0f;  // unk_82FAAAF0  FLAG: undumped
        const f32 KF_MAX_CRASH_NAV_PIC_PARADISE_INNER_DISTANCE_SQ = 1.0f;  // unk_82FAA990  FLAG: undumped

        // The time-in-state thresholds the fly-by waits before testing the distance gates / the
        // turnabout completion (lfs flt_82001D9C == 2.0 for the distance gate; flt_82001CC0/C98
        // shared above). flt_82001CC0 == 0.0 (the running-blur reset is f30 == 0.0 in the asm).
        const f32 KF_DISTANCE_GATE_MIN_TIME_IN_STATE = 2.0f;  // flt_82001D9C (asm: time > 2.0)
        const f32 KF_TURNABOUT_MIN_TIME_IN_STATE     = 1.0f;  // flt_82001C98 (asm case 4: time > 1.0)

        // X360 dirty-flag bit Update OR-pokes into the camera state's flag word every Update exit
        // (the "small near clip" bit; oris +0x150 by 1 -> bit 16).
        const s32 KU_CAMERA_DIRTY_SMALL_NEAR_CLIP = 0x10000;

        // Squared distance between the player car position and the crash-nav camera position. The
        // X360 loads the player position from the shared-info player-car (the lvx128 at player-car
        // +0x220, i.e. VehicleInfo::GetWorldPosition), the camera EYE position from this state's
        // camera +0x30 (mTransform's translation row, asm `lvx128 v13, this, 0x40` with mCamera
        // embedded at this+0x10 -> camera+0x30) -- NOT mSubject (+0x40, the look-at target) --
        // subtracts them (vsubfp), and squares-and-sums the xyz lanes (vmsum3fp128). Expressed here
        // through the committed vpu vocabulary BY NAME (NOT paraphrased to a per-axis scalar formula).
        inline f32 PlayerToCameraDistanceSquared(const ArbStateSharedInfo& lrSharedInfo,
                                                  const Camera::Camera& lrCamera)
        {
            // The player car's world position is the lvx128 at shared-info player-car +0x220 (a
            // Vector3 lane). mpPlayerCar's type (BrnDirector::VehicleInfo) is forward-declared only,
            // so it is read by attested byte offset -- the same opaque-vehicle read pattern
            // BrnArbStateRaceIntro uses for the race-car position.
            const rw::math::vpu::Vector3& lrPlayerPos =
                *reinterpret_cast<const rw::math::vpu::Vector3*>(
                    reinterpret_cast<const u8*>(lrSharedInfo.mpPlayerCar) + 0x220);
            const rw::math::vpu::Vector3& lrCameraPos =
                reinterpret_cast<const rw::math::vpu::Vector3&>(lrCamera.GetTransform().wAxis);

            return rw::math::vpu::MagnitudeSquared(lrPlayerPos - lrCameraPos);  // vsubfp + vmsum3fp128
        }

        // The player-car "keep crash-nav running" gate byte (X360 byte at shared-info player-car
        // +0x44D). mpPlayerCar is forward-declared only, so it is read by attested byte offset (the
        // opaque-vehicle read pattern). FLAG: the +0x44D byte's DWARF name is not recovered; only
        // the byte READ + its role (OR'd with mbDoing100PercentSequence to keep the fly-by driving)
        // are asm-attested.
        inline bool PlayerCarWantsCrashNav(const ArbStateSharedInfo& lrSharedInfo)
        {
            return *(reinterpret_cast<const u8*>(lrSharedInfo.mpPlayerCar) + 0x44D) != 0;
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
    // BehaviourHandle::GetProducedCamera -- the camera the live road-runner behaviour produced this
    // frame (the X360 reaches it through the handle helper sub_821FDC58). Defined out-of-line here,
    // where BehaviourRoadRunner is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    const Camera::Camera& ArbStateCrashNav::BehaviourHandle<TBehaviour>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        // RE-POINTED (2026-07-29): a behaviour does NOT own a camera -- its owning
        // BehaviourManager::BehaviourHelper does (helper +0x10). The manager-side accessor
        // resolves the helper by index, which is the same slot the console's sub_821FDC58
        // helper reaches. (This state still carries its own nested five-word handle copy, so it
        // goes through the manager rather than the shared handle's GetHelper().)
        return mpManager->GetCameraFromBehaviour(
            Camera::BehaviourHelperIndex(static_cast<s32>(muAllocationKey)));
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

        // The behaviour handles start unallocated (+0x840 / +0x864 blocks zeroed).
        mInterpolater  = BehaviourHandle<Camera::BehaviourInterpolate>();
        mRoadRunnerCam = BehaviourHandle<Camera::BehaviourRoadRunner>();

        // The interpolation params block the X360 seeds: [+0x00]=8, [+0x04]=0, [+0x08]=1, [+0x0C]=1.
        mInterpolateParams.mauParams[0] = 8;   // +0x854
        mInterpolateParams.mauParams[1] = 0;   // +0x858
        mInterpolateParams.mauParams[2] = 1;   // +0x85C
        mInterpolateParams.mauParams[3] = 1;   // +0x860

        // NOTE: Construct does NOT seed muCurrentIceMovie (+0x87C), mfTimeInState (+0x880) or
        // mbHasReversed (+0x884) -- the X360 store range ends at +0x874. They are set on entry to
        // the active states (mfTimeInState on the PREPARING success edge; mbHasReversed in the
        // turnabout).
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

        if (mRoadRunnerCam.mbAllocated)    // +0x864 block (released first)
        {
            mRoadRunnerCam.mpManager->UnSetBehaviourUsedByHandle(mRoadRunnerCam.muAllocationKey);
            mRoadRunnerCam.muHelperIndex = 0;
            mRoadRunnerCam.mpManager     = 0;
            mRoadRunnerCam.mpBehaviour   = 0;
            mRoadRunnerCam.mbAllocated   = false;
        }

        if (mInterpolater.mbAllocated)     // +0x840 block
        {
            mInterpolater.mpManager->UnSetBehaviourUsedByHandle(mInterpolater.muAllocationKey);
            mInterpolater.muHelperIndex = 0;
            mInterpolater.mpManager     = 0;
            mInterpolater.mpBehaviour   = 0;
            mInterpolater.mbAllocated   = false;
        }

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
    }
}
