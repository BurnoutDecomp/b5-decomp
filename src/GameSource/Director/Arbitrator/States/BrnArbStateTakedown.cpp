// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTakedown.cpp
//
// See BrnArbStateTakedown.h for the class layouts and provenance. This .cpp defines:
//  - ArbStateTakedown::Construct
//  - ArbStateTakedown::Release
//  - ArbStateTakedown::GetName
//  - B3ClassicTakedownPlayer::Construct  (console-inlined into ArbStateTakedown::Construct)
//  - B3ClassicTakedownPlayer::Release
//  - DestructionPathTakedownPlayer::Construct  (console-inlined into ArbStateTakedown::Construct)
//  - DestructionPathTakedownPlayer::Release
//  - DriveByTakedownPlayer::Construct  (console-inlined into ArbStateTakedown::Construct)
//  - DriveByTakedownPlayer::Update
//  - DriveByTakedownPlayer::Release
//  - ShutdownTakedownPlayer::Construct
//  - ShutdownTakedownPlayer::Release
//  - SimpleIceTakedownPlayer::Prepare
//  - SimpleIceTakedownPlayer::Update
//  - SimpleIceTakedownPlayer::Release
//  - B3ClassicTakedownPlayer::Prepare / ::Update
//  - DestructionPathTakedownPlayer::Prepare / ::Update
//  - DriveByTakedownPlayer::Prepare
//  - ShutdownTakedownPlayer::Prepare / ::Update
//  - ArbStateTakedown::Prepare / ::Update
// PickNewTakedownType stays declaration-only: no asm body exists for it anywhere in this TU's
// function set and nothing in the recovered code calls it (ArbStateTakedown::Prepare picks the
// player straight off the game state), so it carries a one-line FLAG in the header.
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"    // RequestStartEffectHook / EnsureEffectIsPlaying
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h" // GameState::meTakedownVictimID
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/BrnDirectorResourceManager.h"                     // GetTakedown() (the ICE shot group)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                     // NewBehaviour<>
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"               // NamedParameters (the gyro blocks)
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                  // Camera::VehicleInfo (race-car record)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                       // Camera::Utils::CreateLookAt / SineLerp
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"   // DebugPrinter (impact-shake update)
#include "GameSource/Director/MomentController/BrnMoment.h"                     // Moment (the establishing shot)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"                // AllVehicleData::GetRaceCar
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"                // VehicleTracker::GetImplicitVelocity
#include "GameSource/Director/Utils/BrnVehicleRef.h"                            // VehicleRef (impact-shake anchor)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"                      // Timestep::E_WORLD_NO_SLOMO
#include "rw/math/vpu/vector3_operation.h"                                      // Normalize / IsZero / operator-
#include <cmath>                                                                // sinf (the blend easing curves)

namespace BrnDirector
{
    // Local alias for the camera "this behaviour produced the camera this frame" dirty flag,
    // matching the sibling arbitrator-state TUs (BrnArbStatePostEvent.cpp / BrnArbStateRaceIntro.cpp
    // / BrnArbStateRankUp.cpp each define this same local constant rather than sharing a global one).
    namespace
    {
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;
        // The camera flag word's takedown bit, raised by ArbStateTakedown::Update on every arm.
        const s32 KI_CAMERA_DIRTY_TAKEDOWN         = 0x800;
        const f32 KF_UNIT                          = 1.0f;   // flt_82001C98
        const f32 KF_TAKEDOWN_SIM_TIME_SCALE     = 0.2857143f;   // flt_8200177C (case DRIVEBY/ACTIVE constant)

        // Construct's moment-selector tuning. Both are rodata loads in the console body:
        // the three AddMoment records share one weighting (flt_82001DA0 == 0.5f), and the
        // recency factor is flt_8200AE70 == 0.995f.
        const f32 KF_MOMENT_WEIGHTING = 0.5f;
        const f32 KF_RECENCY_FACTOR   = 0.995f;

        // ShutdownTakedownPlayer::Construct's three loose-attachment overrides, written
        // straight after Parameters::Construct seeds the block (flt_82004FDC / flt_82004D04 /
        // flt_820049E0).
        const f32 KF_SHUTDOWN_ZOOM_HEIGHT   = 0.95f;
        const f32 KF_SHUTDOWN_ZOOM_DISTANCE = 1.5f;
        const f32 KF_SHUTDOWN_ZOOM_FOV      = 100.0f;

        // The per-frame blur / slow-mo curve every player drives off its blend's parametric
        // time. Both angles are rodata loads (PI and PI/2); the slow-mo ramp is 5/7, the
        // complement of the KF_TAKEDOWN_SIM_TIME_SCALE 2/7 the players hold at.
        const f32 KF_PI         = 3.1415927f;
        const f32 KF_HALF_PI    = 1.5707964f;
        const f32 KF_SLOMO_RAMP = 0.7142857f;

        // Blend durations, each a rodata load at its own Setup site.
        const f32 KF_BLEND_DURATION          = 0.5f;
        const f32 KF_SHUTDOWN_BLEND_DURATION = 2.0f;
        const f32 KF_SHUTDOWN_LOOKBACK_BLEND = 0.2f;
        const f32 KF_DESTRUCTION_PATH_BLEND  = 1.0f;

        // The per-state timeline thresholds, in seconds of the player's own active time.
        const f32 KF_B3CLASSIC_HANDOFF_TIME    = 1.5f;
        const f32 KF_DESTRUCTION_PATH_FLYBACK1 = 1.5f;
        const f32 KF_DESTRUCTION_PATH_FLYBACK2 = 3.0f;
        const f32 KF_SHUTDOWN_GYRO_TIME        = 0.5f;
        const f32 KF_SHUTDOWN_FLYBACK_TIME     = 1.0f;
        const f32 KF_SHUTDOWN_ZOOM1_FLASH_END  = 1.1f;
        const f32 KF_SHUTDOWN_ZOOM2_TIME       = 1.6f;
        const f32 KF_SHUTDOWN_ZOOM2_FLASH_END  = 1.7f;
        const f32 KF_SHUTDOWN_ZOOM3_TIME       = 2.2f;
        const f32 KF_SHUTDOWN_ZOOM3_FLASH_END  = 2.3f;
        const f32 KF_SHUTDOWN_FINISH_TIME      = 2.8f;
        const f32 KF_DRIVEBY_FINISH_TIME       = 3.0f;

        // The zoom beats run the game clock down to one frame's worth of sim time, and the
        // state's failsafe hold ramps back out of that same floor over its active time.
        const f32 KF_ZOOM_SIM_TIME_SCALE     = 0.0333333351f;
        const f32 KF_FAILSAFE_SIM_TIME_FLOOR = 0.0333333351f;

        // The console's own !IsZero() tripwire tolerance at the normalize sites: the splatted
        // FLT_EPSILON rodata word, not the vector helper's 1e-6f default.
        const f32 KF_IS_ZERO_TOLERANCE = 1.1920929e-7f;

        // The destruction-path look-at's two rodata scalars (its eye position is built from
        // the world-up selector, these two, and the player car's normalized linear velocity).
        const f32 KF_DESTRUCTION_PATH_EYE_SCALE  = 5.0f;
        const f32 KF_DESTRUCTION_PATH_EYE_OFFSET = 10.0f;

        // The selector must have been active for at least this many frames before a zero
        // valid-moment count is allowed to block Prepare (the same gate ArbStateCrashing has).
        const s32 KI_MIN_FRAMES_BEFORE_MOMENT_CHECK = 2;
        // The slow-mo the state holds outside road rage with no revenge/shutdown takedown running.
        const f32 KF_NON_TAKEDOWN_SIM_SCALE = 0.6666667f;

        // The NewBehaviour<> trailing debug ref-count limit. The gyro / loose-attachment sites
        // pass 2 where the interpolate sites pass 1; the owner argument before it is always null.
        const s32 KI_NEW_BEHAVIOUR_LIMIT_1 = 1;
        const s32 KI_NEW_BEHAVIOUR_LIMIT_2 = 2;

        // The one-shot camera flash hook the shutdown player's three zoom beats request.
        const char* const KPC_FLASH_HOOK = "2dFlash";

        // clamp(x, 0, 1) as the console spells it: an fsel pair for the low end and another
        // for the high end -- the same shape BehaviourInterpolate::GetParametricTime uses.
        f32 ClampUnit(f32 lfValue)
        {
            const f32 lfClampedLow = (-lfValue >= 0.0f) ? 0.0f : lfValue;
            return (1.0f - lfClampedLow >= 0.0f) ? lfClampedLow : 1.0f;
        }

        // The world-space normalized "victim car -> player car" vector the gyro rigs are
        // seeded from. Three players build it identically (the same subtract, the same
        // !IsZero tripwire, the same normalize), so it is outlined here per the project's
        // inlining-reversal rule rather than written out three times.
        rw::math::vpu::Vector3 NormalizedVectorToPlayer(const ArbStateSharedInfo& lrSharedInfo,
                                                        s32 liRaceCarIndex)
        {
            const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
                *lrSharedInfo.mpPlayerCarTransform;

            const rw::math::vpu::Vector3 lToPlayer =
                lrPlayerTransform.wAxis -
                lrSharedInfo.mpRaceCars[liRaceCarIndex].mRaceCarState.mTransform.wAxis;

            CGS_ASSERT(!rw::math::vpu::IsZero(lToPlayer, KF_IS_ZERO_TOLERANCE), "!IsZero(lToPlayer)");

            return rw::math::vpu::Normalize(lToPlayer);
        }
    }

    // ------------------------------------------------------------------------
    // BrnDirector::B3ClassicTakedownPlayer::Construct -- seed the player. No standalone console
    // symbol: the compiler inlined it into ArbStateTakedown::Construct, where the
    // whole store block is addressed off `this + 0x180`. De-inlined
    // here per the project's inlining-reversal rule.
    //
    // The interpolate block's stores are {tag 8, name 0, method 0, mapping 1} from
    // Parameters::Construct, then an immediate re-store of mapping = 3 and method = 0 -- so the
    // net seed is the default block with an exponential-out-x-cubed mapping.
    // ------------------------------------------------------------------------
    void B3ClassicTakedownPlayer::Construct()
    {
        meState = E_STATE_INACTIVE;   // stw 0, 0x54(this)

        mInterpolaterA.Clear();       // +0x18 block
        mInterpolaterB.Clear();       // +0x2C block
        mGyroCam.Clear();             // +0x04 block

        mInterpolateParams.Construct();                                            // +0x40 {8,0,0,1}
        mInterpolateParams.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;       // stw 3, 0x4C
        mInterpolateParams.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_SLERP;                          // stw 0, 0x48
    }

    // ------------------------------------------------------------------------
    // BrnDirector::B3ClassicTakedownPlayer::Release -- reset the state machine, then
    // hand each of the three behaviour holds back to the manager. The console inlines
    // BehaviourHandle<T>::Release() at each site (the `if (mbAllocated) { UnSetBehaviourUsedByHandle
    // (key); clear the four remaining words; }` shape); expressed here as the named call.
    // ------------------------------------------------------------------------
    void B3ClassicTakedownPlayer::Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;
        (void)lrSharedInfo;

        meState = E_STATE_INACTIVE;   // stw 0, 0x54(this)

        mGyroCam.Release();           // +0x04 block
        mInterpolaterA.Release();     // +0x18 block
        mInterpolaterB.Release();     // +0x2C block
    }

    // ------------------------------------------------------------------------
    // BrnDirector::DestructionPathTakedownPlayer::Construct -- inlined by the console into
    // ArbStateTakedown::Construct off `this + 0x1D8`.
    // Same shape as the B3-classic seed plus the second gyro cam.
    // ------------------------------------------------------------------------
    void DestructionPathTakedownPlayer::Construct()
    {
        meState = E_STATE_INACTIVE;   // stw 0, 0x68(this)

        mInterpolaterA.Clear();       // +0x2C block
        mInterpolaterB.Clear();       // +0x40 block
        mGyroCamA.Clear();            // +0x04 block
        mGyroCamB.Clear();            // +0x18 block

        mInterpolateParams.Construct();                                            // +0x54 {8,0,0,1}
        mInterpolateParams.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_SLERP;                          // stw 0, 0x5C
        mInterpolateParams.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;       // stw 3, 0x60
    }

    // ------------------------------------------------------------------------
    // BrnDirector::DestructionPathTakedownPlayer::Release -- as B3Classic's, over
    // four holds.
    // ------------------------------------------------------------------------
    void DestructionPathTakedownPlayer::Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;
        (void)lrSharedInfo;

        meState = E_STATE_INACTIVE;   // stw 0, 0x68(this)

        mGyroCamA.Release();          // +0x04 block
        mGyroCamB.Release();          // +0x18 block
        mInterpolaterA.Release();     // +0x2C block
        mInterpolaterB.Release();     // +0x40 block
    }

    // ------------------------------------------------------------------------
    // BrnDirector::DriveByTakedownPlayer::Construct -- inlined by the console into
    // ArbStateTakedown::Construct off `this + 0x398`.
    // No parameter block: this player owns only the two gyro-cam holds.
    // ------------------------------------------------------------------------
    void DriveByTakedownPlayer::Construct()
    {
        meState = E_STATE_INACTIVE;   // stw 0, 0x30(this)

        mGyroCamDriveByL.Clear();     // +0x04 block
        mGyroCamDriveByR.Clear();     // +0x18 block
    }

    // ------------------------------------------------------------------------
    // BrnDirector::DriveByTakedownPlayer::Release -- reset the state machine, then
    // drop the two gyro-cam holds.
    // ------------------------------------------------------------------------
    void DriveByTakedownPlayer::Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;
        (void)lrSharedInfo;

        meState = E_STATE_INACTIVE;   // stw 0, 0x30(this)

        mGyroCamDriveByL.Release();   // +0x04 block
        mGyroCamDriveByR.Release();   // +0x18 block
    }

    // ------------------------------------------------------------------------
    // BrnDirector::ShutdownTakedownPlayer::Construct -- seed the nine-state player:
    // clear the seven behaviour holds, seed the three interpolate parameter blocks, then seed
    // the loose-attachment parameter block and override three of its tunables.
    //
    // The three interpolate blocks' NET seeds, read store-for-store off the asm (each is
    // Parameters::Construct's {8, 0, SLERP, SINUSOIDAL} followed by an explicit mapping store
    // and an explicit method store):
    //     A  method ROTATE_ABOUT_PLAYER_CAR, mapping SINUSOIDAL            (0x90 block)
    //     B  method SLERP,                   mapping EXPONENTIAL_OUT_X_CUBED (0xA0 block)
    //     C  method SLERP,                   mapping LINEAR               (0xB0 block)
    // mfActiveTime (+0x124) and mbUsedFinalShotImpact (+0x12C) are deliberately NOT written --
    // the console leaves both to the first Prepare / the LOOKBACK case.
    // ------------------------------------------------------------------------
    void ShutdownTakedownPlayer::Construct()
    {
        meState = E_STATE_INACTIVE;   // stw 0, 0x128(this)

        mInterpolaterA.Clear();       // +0x18 block
        mInterpolateParamsA.Construct();                                           // +0x90 {8,0,0,1}
        mInterpolateParamsA.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_SINUSOIDAL;                    // stw 1, 0x9C
        mInterpolateParamsA.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;        // stw 1, 0x98

        mInterpolaterB.Clear();       // +0x2C block
        mInterpolateParamsB.Construct();                                           // +0xA0 {8,0,0,1}
        mInterpolateParamsB.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;       // stw 3, 0xAC
        mInterpolateParamsB.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_SLERP;                          // stw 0, 0xA8

        mInterpolateParamsC.Construct();                                           // +0xB0 {8,0,0,1}
        mInterpolateParamsC.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_LINEAR;                        // stw 0, 0xBC
        mInterpolateParamsC.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_SLERP;                          // stw 0, 0xB8

        mLooseAttachment.Clear();     // +0x40 block

        mLooseAttachmentParameters.Construct();                        // bl Parameters::Construct(this+0xC0)
        mLooseAttachmentParameters.mfHeight   = KF_SHUTDOWN_ZOOM_HEIGHT;    // stfs 0x10C (params +0x4C)
        mLooseAttachmentParameters.mfField54  = KF_SHUTDOWN_ZOOM_FOV;       // stfs 0x114 (params +0x54, declaration reference mfFOV)
        mLooseAttachmentParameters.mfDistance = KF_SHUTDOWN_ZOOM_DISTANCE;  // stfs 0x110 (params +0x50)

        mGyroCam.Clear();             // +0x04 block
        mZoom1.Clear();               // +0x54 block
        mZoom2.Clear();               // +0x68 block
        mZoom3.Clear();               // +0x7C block
    }

    // ------------------------------------------------------------------------
    // BrnDirector::ShutdownTakedownPlayer::Release -- reset the state machine, then
    // drop all seven behaviour holds. The console's release ORDER is not the declaration order:
    // it runs mInterpolaterA, mInterpolaterB, mGyroCam, then the loose-attachment hold and the
    // three zoom beats; kept verbatim.
    // ------------------------------------------------------------------------
    void ShutdownTakedownPlayer::Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;
        (void)lrSharedInfo;

        meState = E_STATE_INACTIVE;   // stw 0, 0x128(this)

        mInterpolaterA.Release();     // +0x18 block
        mInterpolaterB.Release();     // +0x2C block
        mGyroCam.Release();           // +0x04 block
        mLooseAttachment.Release();   // +0x40 block
        mZoom1.Release();             // +0x54 block
        mZoom2.Release();             // +0x68 block
        mZoom3.Release();             // +0x7C block
    }

    // ------------------------------------------------------------------------
    // ArbStateTakedown::Construct -- bring the state up: construct the embedded
    // camera, clear the base flags and the state machine, seed all five sub-players, clear the
    // three behaviour handles this state owns directly, seed its own interpolate parameters,
    // then register the three candidate takedown "moments" with the selector and seed the
    // remaining scalars.
    //
    // ⭐ The three moment records are passed in the r4:r5 GPR pair as one 16-byte
    // MomentDescription by value -- {meMomentType, meMomentParamID} in r4 and
    // {mfWeighting, mbCanBeInhibited} in r5 -- exactly as ArbStateCrashing::Construct does. All
    // three parameter ids that fall out are `*_TAKEDOWN_ONLY` enumerators, which is the
    // independent confirmation that the right two words are being read out of each record:
    //     BYSTANDER_SEES_ACTION + BYSTANDER_CLOSE_TAKEDOWN_ONLY       weight 0.5, NOT inhibitable
    //     TUMBLING              + TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY weight 0.5, NOT inhibitable
    //     TUMBLING              + TUMBLING_LEAD_TAKEDOWN_ONLY          weight 0.5, NOT inhibitable
    //
    // mfFailsafeTimer, mfActiveTime and mbHasTriggeredFlash are deliberately NOT seeded here --
    // the console's store set does not touch +0x620 / +0x624 / +0x62E.
    // ------------------------------------------------------------------------
    void ArbStateTakedown::Construct()
    {
        GetNonConstCamera().Construct();   // bl Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // stw 0, +0x628

        // The console inlines every sub-player's Construct except the shutdown player's, which
        // it calls out of line; de-inlined back to the five calls, in the console's own order.
        mDestructionPathTakedown.Construct();   // this+0x1D8 store block
        mSimpleIceTakedown.Construct();         // this+0x244 store block
        mShutdownTakedown.Construct();          // bl ShutdownTakedownPlayer::Construct(this+0x268)
        mDriveByTakedown.Construct();           // this+0x398 store block
        mClassicTakedown.Construct();           // this+0x180 store block

        mTakedownDebugCam.Clear();         // +0x3E0 block
        mGyroCam.Clear();                  // +0x3F4 block
        mInterpolator.Clear();             // +0x408 block

        mInterpolatorParams.Construct();                                           // +0x41C {8,0,0,1}
        mInterpolatorParams.meInterpolationMapping =
            Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;       // stw 3, 0x428
        mInterpolatorParams.meInterpolationMethod  =
            Camera::BehaviourInterpolate::E_METHOD_SLERP;                          // stw 0, 0x424

        // Inlined MomentSelector::Construct over the embedded selector at +0x42C (the three
        // Array count words plus the scalar block), exactly as the crashing/roaming states do.
        mMomentSelector.Construct();

        mMomentSelector.AddMoment(Moment::E_MOMENT_BYSTANDER_SEES_ACTION,
                                  MomentParameterBank::E_PARAM_BYSTANDER_CLOSE_TAKEDOWN_ONLY,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);
        mMomentSelector.AddMoment(Moment::E_MOMENT_TUMBLING,
                                  MomentParameterBank::E_PARAM_TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);
        mMomentSelector.AddMoment(Moment::E_MOMENT_TUMBLING,
                                  MomentParameterBank::E_PARAM_TUMBLING_LEAD_TAKEDOWN_ONLY,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);

        mMomentSelector.SetRecencyFactor(KF_RECENCY_FACTOR);

        // A second, redundant store of meSelectionMode after SetRecencyFactor
        // (`stw 0, 0x608(this)` == selector +0x1DC, no call) -- an inlined SetSelectionMode
        // right after Construct()'s own seed, the same shape ArbStateCrashing::Construct has.
        mMomentSelector.SetSelectionMode(MomentSelector::E_MODE_LRU_BEST);

        mbUseTakedownDebugCam  = false;    // stb 0, +0x62D
        mbAlwaysUseShutdownCam = false;    // stb 0, +0x62C
        miIceMovieIndex        = -1;       // stw -1, +0x618

        // Inlined ImpactShakeController::Construct over +0x3CC (the five 0.0f stores: the
        // impact factor plus the embedded shake's four wobble words).
        mImpactShakeController.Construct();

        mpCurrentTakedown = 0;             // stw 0, +0x610
        meTakedownType    = E_NUM_TYPES;   // stw 1, +0x614 (the past-the-end "unset" sentinel)
    }

    // ------------------------------------------------------------------------
    // ArbStateTakedown::GetName @0x821F62E0 -- the state's literal debug name.
    // ------------------------------------------------------------------------
    const char* ArbStateTakedown::GetName() const
    {
        return "ArbStateTakedown";
    }

    // ------------------------------------------------------------------------
    // ArbStateTakedown::Release @0x822353B8 -- leave the takedown state: hand off to the
    // currently-active player's own Release (X360: `(*(**mpCurrentTakedown+8))(mpCurrentTakedown,
    // this, &lrSharedInfo)`, i.e. the player's vtable slot 2, TakedownPlayer::Release), reset the
    // state machine, drop the three base-level behaviour handles this state owns directly (debug
    // cam / gyro cam / interpolator) back to the manager, release the moment selector, and finally
    // assert no behaviours remain allocated by this state.
    // ------------------------------------------------------------------------
    bool ArbStateTakedown::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        mpCurrentTakedown->Release(this, lrSharedInfo);

        meState = E_STATE_INACTIVE;   // X360: *(a1+1576) = 0

        if (mTakedownDebugCam.IsAllocated())
        {
            mTakedownDebugCam.Release();
        }

        mMomentSelector.Release();

        if (mInterpolator.IsAllocated())
        {
            mInterpolator.Release();
        }

        if (mGyroCam.IsAllocated())
        {
            mGyroCam.Release();
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);

        return true;
    }

    // ------------------------------------------------------------------------
    // DriveByTakedownPlayer::Update @0x8225A000 -- per-frame drive-by state machine: hold on
    // whichever of the two gyro cams (left/right shooter seat) is currently the "behaviour
    // driven" one -- i.e. whichever produced camera has KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN set --
    // request the one-shot "Takedown" start hook the first frame, then hold at the fixed
    // KF_TAKEDOWN_SIM_TIME_SCALE blend amount until mfActiveTime passes 3s, at which point the
    // state advances to FINISHED (the terminal hold just keeps redrawing whichever gyro cam is
    // still selected).
    // ------------------------------------------------------------------------
    Camera::Camera DriveByTakedownPlayer::Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;

        Camera::Camera lOutCamera;
        lOutCamera.Construct();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            // X360: (**a2)(a2, a3, a4) -- the player's own Prepare, vtable slot 0.
            if (Prepare(lpCallingState, lrSharedInfo))
            {
                mfActiveTime = 0.0f;
                meState = E_STATE_DRIVEBY;
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_DRIVEBY:
        {
            // Pick whichever gyro cam is currently "behaviour driven" (its produced camera's
            // dirty-flags word has KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN set); default to the right
            // seat when neither/the left one isn't.
            const Camera::Camera& lrLeftProduced = mGyroCamDriveByL.GetProducedCamera();
            const bool lbLeftIsDriving = (lrLeftProduced.mState_uFlags & KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN) != 0;

            lOutCamera = lbLeftIsDriving ? lrLeftProduced : mGyroCamDriveByR.GetProducedCamera();

            if (mfActiveTime == 0.0f)
            {
                Camera::RequestStartEffectHook(lOutCamera, "Takedown", KF_UNIT);
            }

            lOutCamera.mEffects.mfSimTimeScale = KF_TAKEDOWN_SIM_TIME_SCALE;

            if (mfActiveTime > 3.0f)
            {
                meState = E_STATE_FINISHED;
            }
            break;
        }

        case E_STATE_FINISHED:
        {
            const Camera::Camera& lrLeftProduced = mGyroCamDriveByL.GetProducedCamera();
            const bool lbLeftIsDriving = (lrLeftProduced.mState_uFlags & KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN) != 0;
            lOutCamera = lbLeftIsDriving ? lrLeftProduced : mGyroCamDriveByR.GetProducedCamera();
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        lOutCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;
        return lOutCamera;
    }

    // ------------------------------------------------------------------------
    // SimpleIceTakedownPlayer::Prepare @0x8226CF38 -- enter the ICE-anim takedown: allocate and
    // configure the ICE-anim behaviour once (adopt the bound shot's parameters, anchor both the
    // secondary and bystander vehicle refs to the takedown's victim race car, latch the
    // collision-policy + first-frame-reset flags), then report whether the freshly-allocated
    // behaviour is ready.
    // ------------------------------------------------------------------------
    bool SimpleIceTakedownPlayer::Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;

        if (meState < E_STATE_ACTIVE)
        {
            meState = E_STATE_PREPARING;

            if (!mIceCam.IsAllocated())
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mIceCam, const_cast<ArbitratorState*>(lpCallingState), 0, 1);

                Camera::BehaviourIceAnim* lpIceAnim = mIceCam.GetBehaviour();
                lpIceAnim->SetParameters(mpIceAnim);

                // X360: both refs read the SAME GameState::meTakedownVictimID
                // (mpGameState+0xE0, BrnDirectorGameState.h:27) and each asserts it is a valid
                // race-car index before storing (BrnVehicleRef.h:222).
                const s32 liVictimRaceCarIndex = static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);
                CGS_ASSERT(liVictimRaceCarIndex < 8, "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");

                lpIceAnim->SetSecondaryVehicleRefToRaceCarIndex(liVictimRaceCarIndex);
                lpIceAnim->SetBystanderRefToRaceCarIndex(liVictimRaceCarIndex);
                lpIceAnim->SetUseCollisionPolicy(true);
                lpIceAnim->ClearBaseFirstFrameGate();
            }

            CGS_ASSERT(mIceCam.IsAllocated(), "mbIsAllocated");
            return mIceCam.IsReadyToPrepare();
        }

        return true;
    }

    // ------------------------------------------------------------------------
    // SimpleIceTakedownPlayer::Update @0x8225A1E8 -- drive the output camera from the ICE-anim
    // behaviour's produced camera; once PREPARING succeeds advance to ACTIVE, and once the anim
    // reports finished or failed advance to FINISHED.
    // ------------------------------------------------------------------------
    Camera::Camera SimpleIceTakedownPlayer::Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;

        Camera::Camera lOutCamera;
        lOutCamera.Construct();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            if (Prepare(lpCallingState, lrSharedInfo))
            {
                mfActiveTime = 0.0f;
                meState = E_STATE_ACTIVE;
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_ACTIVE:
            lOutCamera = mIceCam.GetProducedCamera();
            if (mIceCam.GetBehaviour()->HasFinishedOrFailed())
            {
                meState = E_STATE_FINISHED;
            }
            break;

        case E_STATE_FINISHED:
            lOutCamera = mIceCam.GetProducedCamera();
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        lOutCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;
        return lOutCamera;
    }

    // ------------------------------------------------------------------------
    // SimpleIceTakedownPlayer::Release @0x82235208 -- reset the state machine and drop the
    // ICE-anim behaviour hold (if allocated) back to the manager.
    // ------------------------------------------------------------------------
    void SimpleIceTakedownPlayer::Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lpCallingState;
        (void)lrSharedInfo;

        meState = E_STATE_INACTIVE;

        if (mIceCam.IsAllocated())
        {
            mIceCam.Release();
        }
    }

    // ------------------------------------------------------------------------
    // The state's three named constants (declaration reference: all three live in this .cpp).
    // KI_MAX_ROADRAGE_CUTS is the console's literal 2 in Update's cut-budget compare; the two
    // scalars are both the 1.0f rodata word Update loads at its transition and failsafe gates.
    // ------------------------------------------------------------------------
    const s32 ArbStateTakedown::KI_MAX_ROADRAGE_CUTS = 2;
    const f32 ArbStateTakedown::KF_TRANSITION_TIME   = 1.0f;
    const f32 ArbStateTakedown::KF_MIN_FAILSAFE_TIME = 1.0f;
    // ------------------------------------------------------------------------
    // B3ClassicTakedownPlayer::Prepare -- bring the classic flyback up: allocate the gyro cam
    // on the victim's car and seed its from-car vector from the live player, then allocate the
    // blend that carries the gameplay camera into it. Re-entrant: each block is skipped once its
    // handle is allocated, and the whole function short-circuits to "ready" once the player has
    // left PREPARING.
    // ------------------------------------------------------------------------
    bool B3ClassicTakedownPlayer::Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState >= E_STATE_FLYBACK)
        {
            return true;
        }

        bool lbReady = true;
        meState = E_STATE_PREPARING;

        if (!mGyroCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCam, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            // The takedown gyro block, record +2112 on the bank's 204-byte gyro grid.
            mGyroCam.GetBehaviour()->SetParameters(&lrSharedInfo.mpNamedParameters->mGyroCamTakedownParams);

            const s32 liVictimRaceCarIndex =
                static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);
            mGyroCam.GetBehaviour()->AttachToRaceCar(liVictimRaceCarIndex);

            mGyroCam.GetBehaviour()->SetUseVehicleAttachmentCollision(true);

            mGyroCam.GetBehaviour()->SetWorldSpaceNormalizedVectorFromCar(
                NormalizedVectorToPlayer(lrSharedInfo, liVictimRaceCarIndex));
        }

        if (!mInterpolaterA.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                mInterpolaterA, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_1);

            mInterpolaterA.GetBehaviour()->SetParameters(&mInterpolateParams);
            mInterpolaterA.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

            // The blend IN: FROM the live gameplay camera's helper TO the gyro cam's.
            mInterpolaterA.GetBehaviour()->Setup(
                KF_BLEND_DURATION,
                lrSharedInfo.mpSharedCameraContainer->GetGameplayCameraHelperIndex(),
                mGyroCam.GetBehaviourHelperIndex(),
                lrSharedInfo.mpBehaviourManager);
        }

        // Ready only once neither behaviour is still queued for its own first Prepare (each
        // handle's IsWaitingToPrepare carries the console's own "mbIsAllocated" tripwire).
        if (mGyroCam.IsWaitingToPrepare() || mInterpolaterA.IsWaitingToPrepare())
        {
            lbReady = false;
        }

        return lbReady;
    }

    // ------------------------------------------------------------------------
    // B3ClassicTakedownPlayer::Update -- the four-beat classic takedown. FLYBACK rides the
    // blend into the gyro cam, requesting the one-shot "Takedown" hook on its first frame and
    // driving motion blur + slow-mo off the blend's parametric time; once that blend finishes
    // and the beat has run 1.5s it allocates the second blend (gyro -> gameplay) and hands over
    // to INTERPOLATING_TO_GAMEPLAY, which runs the mirrored curve and then holds at FINISHED.
    // ------------------------------------------------------------------------
    Camera::Camera B3ClassicTakedownPlayer::Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera lOutCamera;
        lOutCamera.Construct();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            if (Prepare(lpCallingState, lrSharedInfo))
            {
                mfActiveTime = 0.0f;
                meState = E_STATE_FLYBACK;
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_FLYBACK:
        {
            lOutCamera = mInterpolaterA.GetProducedCamera();

            if (mfActiveTime == 0.0f)
            {
                Camera::RequestStartEffectHook(lOutCamera, "Takedown", KF_UNIT);
            }

            const f32 lfBlurAmount =
                ClampUnit(sinf(mInterpolaterA.GetBehaviour()->GetParametricTime() * KF_PI));
            const f32 lfEaseAmount =
                sinf(mInterpolaterA.GetBehaviour()->GetParametricTime() * KF_HALF_PI);

            lOutCamera.RequestMotionBlur(lfBlurAmount, ClampUnit(lfEaseAmount));
            lOutCamera.mEffects.mfSimTimeScale = KF_UNIT - lfEaseAmount * KF_SLOMO_RAMP;

            if (mInterpolaterA.GetBehaviour()->HasFinished() && mfActiveTime > KF_B3CLASSIC_HANDOFF_TIME)
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                    mInterpolaterB, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                mInterpolaterB.GetBehaviour()->SetParameters(&mInterpolateParams);
                mInterpolaterB.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                // The blend OUT, the mirror of Prepare's: FROM the gyro cam TO the gameplay camera.
                mInterpolaterB.GetBehaviour()->Setup(
                    KF_BLEND_DURATION,
                    mGyroCam.GetBehaviourHelperIndex(),
                    lrSharedInfo.mpSharedCameraContainer->GetGameplayCameraHelperIndex(),
                    lrSharedInfo.mpBehaviourManager);

                meState = E_STATE_INTERPOLATING_TO_GAMEPLAY;
            }
            break;
        }

        case E_STATE_INTERPOLATING_TO_GAMEPLAY:
        {
            lOutCamera = mInterpolaterB.GetProducedCamera();

            // The hold value the eased term immediately overwrites; the console stores it first
            // all the same, so it is kept.
            lOutCamera.mEffects.mfSimTimeScale = KF_TAKEDOWN_SIM_TIME_SCALE;

            const f32 lfBlurAmount =
                ClampUnit(sinf(mInterpolaterB.GetBehaviour()->GetParametricTime() * KF_PI));
            // The blend-out runs the ease in REVERSE (1 - sin), so the slow-mo returns to normal.
            const f32 lfEaseAmount =
                KF_UNIT - sinf(mInterpolaterB.GetBehaviour()->GetParametricTime() * KF_HALF_PI);

            lOutCamera.RequestMotionBlur(lfBlurAmount, ClampUnit(lfEaseAmount));
            lOutCamera.mEffects.mfSimTimeScale = KF_UNIT - lfEaseAmount * KF_SLOMO_RAMP;

            if (mInterpolaterB.GetBehaviour()->HasFinished())
            {
                meState = E_STATE_FINISHED;
            }
            break;
        }

        case E_STATE_FINISHED:
            lOutCamera = mInterpolaterB.GetProducedCamera();
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        lOutCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;
        return lOutCamera;
    }

    // ------------------------------------------------------------------------
    // DestructionPathTakedownPlayer::Prepare -- two gyro rigs plus the blend between them. Both
    // rigs are seeded from the PLAYER TRACKER's implicit velocity (not from a car-to-car vector
    // like the other players): the first adopts the default gyro block and rides the player's own
    // race car, the second adopts the takedown block and rides the victim. The blend then runs
    // from a synthesised look-at camera into the first rig.
    // ------------------------------------------------------------------------
    bool DestructionPathTakedownPlayer::Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState >= E_STATE_FLYBACK1)
        {
            return true;
        }

        // NOTE the inverted shape: this player starts NOT ready and is raised at the tail once
        // all three behaviours have stopped waiting, where its siblings start ready and clear.
        bool lbReady = false;
        meState = E_STATE_PREPARING;

        if (!mGyroCamA.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCamA, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            // The default gyro block, record +480 (gyro slot 0).
            mGyroCamA.GetBehaviour()->SetParameters(&lrSharedInfo.mpNamedParameters->mGyroCamDefaultParams);
            mGyroCamA.GetBehaviour()->AttachToRaceCar(
                static_cast<s32>(lrSharedInfo.mpAllVehicleData->GetPlayerRCIndex()));

            // NOTE: unlike its siblings this player leaves the vehicle-attachment collision
            // select alone on BOTH its rigs -- neither of the console's two blocks here stores
            // the byte, so the destruction-path cameras keep the visibility collision policy.

            mGyroCamA.GetBehaviour()->SetWorldSpaceNormalizedVectorFromCar(
                rw::math::vpu::Normalize(lrSharedInfo.mpPlayerTracker->GetImplicitVelocity()));
        }

        if (!mGyroCamB.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCamB, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            mGyroCamB.GetBehaviour()->SetParameters(&lrSharedInfo.mpNamedParameters->mGyroCamTakedownParams);
            mGyroCamB.GetBehaviour()->AttachToRaceCar(
                static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID));

            mGyroCamB.GetBehaviour()->SetWorldSpaceNormalizedVectorFromCar(
                rw::math::vpu::Normalize(lrSharedInfo.mpPlayerTracker->GetImplicitVelocity()));
        }

        if (!mInterpolaterA.IsAllocated())
        {
            // The blend's "from" camera is BUILT here rather than taken from a helper: a look-at
            // whose target is the player car's world position and whose eye is the player's
            // normalized linear velocity scaled by the world-up-selected pair {5, y + 5, 5} and
            // offset by 10. Reproduced lane-for-lane from the console's VMX block -- the
            // per-lane asymmetry (the up selector only reaches the y lane) is the binary's, and
            // is why it is written out rather than folded into a tidier vector expression.
            const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
                *lrSharedInfo.mpPlayerCarTransform;
            const rw::math::vpu::Vector3 lPlayerPosition = lrPlayerTransform.wAxis;

            const rw::math::vpu::Vector3 lPlayerDirection =
                rw::math::vpu::Normalize(lrSharedInfo.mpPlayerCar->mRaceCarState.mLinearVelocity);

            const rw::math::vpu::Vector3 lEyeScale =
            {
                KF_DESTRUCTION_PATH_EYE_SCALE,
                lPlayerPosition.y + KF_DESTRUCTION_PATH_EYE_SCALE,
                KF_DESTRUCTION_PATH_EYE_SCALE,
                0.0f
            };

            const rw::math::vpu::Vector3 lEyePosition =
            {
                lPlayerDirection.x * lEyeScale.x + KF_DESTRUCTION_PATH_EYE_OFFSET,
                lPlayerDirection.y * lEyeScale.y + KF_DESTRUCTION_PATH_EYE_OFFSET,
                lPlayerDirection.z * lEyeScale.z + KF_DESTRUCTION_PATH_EYE_OFFSET,
                0.0f
            };

            Camera::Camera lFromCamera;
            lFromCamera.Construct();
            lFromCamera.mTransform = Camera::Utils::CreateLookAt(lEyePosition, lPlayerPosition);
            lFromCamera.ValidateTransformWithDebugInfo();

            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                mInterpolaterA, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_1);

            mInterpolaterA.GetBehaviour()->SetParameters(&mInterpolateParams);
            mInterpolaterA.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

            // The camera-taking Setup overload: FROM the synthesised look-at TO the first rig.
            mInterpolaterA.GetBehaviour()->SetupDuration(KF_BLEND_DURATION);
            mInterpolaterA.GetBehaviour()->SetupCameraAFromCamera(lFromCamera);
            mInterpolaterA.GetBehaviour()->SetupCameraBFromHelper(
                mGyroCamA.GetBehaviourHelperIndex(), *lrSharedInfo.mpBehaviourManager);
            mInterpolaterA.GetBehaviour()->Setup();
        }

        if (!mGyroCamA.IsWaitingToPrepare() &&
            !mGyroCamB.IsWaitingToPrepare() &&
            !mInterpolaterA.IsWaitingToPrepare())
        {
            lbReady = true;
        }

        return lbReady;
    }

    // ------------------------------------------------------------------------
    // DestructionPathTakedownPlayer::Update -- two flyback beats. FLYBACK1 rides the first blend
    // (look-at -> first rig) at the fixed takedown slow-mo, driving depth-of-field blurriness off
    // the blend's parametric time; once that blend finishes and the beat has run 1.5s it
    // allocates the second blend (first rig -> second rig) and advances. FLYBACK2 runs the same
    // curve and finishes once the second blend is done AND the player has run 3s.
    // ------------------------------------------------------------------------
    Camera::Camera DestructionPathTakedownPlayer::Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera lOutCamera;
        lOutCamera.Construct();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            if (Prepare(lpCallingState, lrSharedInfo))
            {
                mfActiveTime = 0.0f;
                meState = E_STATE_FLYBACK1;
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_FLYBACK1:
        {
            lOutCamera = mInterpolaterA.GetProducedCamera();

            if (mfActiveTime == 0.0f)
            {
                Camera::RequestStartEffectHook(lOutCamera, "Takedown", KF_UNIT);
            }

            lOutCamera.mEffects.mfSimTimeScale = KF_TAKEDOWN_SIM_TIME_SCALE;
            lOutCamera.mDepthOfField.SetBlurriness(
                sinf(mInterpolaterA.GetBehaviour()->GetParametricTime() * KF_PI));

            if (mInterpolaterA.GetBehaviour()->HasFinished() &&
                mfActiveTime > KF_DESTRUCTION_PATH_FLYBACK1)
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                    mInterpolaterB, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                mInterpolaterB.GetBehaviour()->SetParameters(&mInterpolateParams);
                mInterpolaterB.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                // FROM the second rig TO the first (the console's own argument order).
                mInterpolaterB.GetBehaviour()->Setup(
                    KF_DESTRUCTION_PATH_BLEND,
                    mGyroCamB.GetBehaviourHelperIndex(),
                    mGyroCamA.GetBehaviourHelperIndex(),
                    lrSharedInfo.mpBehaviourManager);

                meState = E_STATE_FLYBACK2;
            }
            break;
        }

        case E_STATE_FLYBACK2:
            lOutCamera = mInterpolaterB.GetProducedCamera();
            lOutCamera.mEffects.mfSimTimeScale = KF_TAKEDOWN_SIM_TIME_SCALE;
            lOutCamera.mDepthOfField.SetBlurriness(
                sinf(mInterpolaterB.GetBehaviour()->GetParametricTime() * KF_PI));

            if (mInterpolaterB.GetBehaviour()->HasFinished() &&
                mfActiveTime > KF_DESTRUCTION_PATH_FLYBACK2)
            {
                meState = E_STATE_FINISHED;
            }
            break;

        case E_STATE_FINISHED:
            lOutCamera = mInterpolaterB.GetProducedCamera();
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        lOutCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;
        return lOutCamera;
    }

    // ------------------------------------------------------------------------
    // DriveByTakedownPlayer::Prepare -- allocate the two drive-by gyro rigs. BOTH adopt the same
    // parameter block (record +2928, the drive-by-left slot) and BOTH ride the victim's car; the
    // left/right distinction is made downstream in Update, by which rig produced the frame's
    // camera. Neither rig gets a from-car vector seed -- this is the one player that does not.
    // ------------------------------------------------------------------------
    bool DriveByTakedownPlayer::Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState >= E_STATE_DRIVEBY)
        {
            return true;
        }

        bool lbReady = true;
        meState = E_STATE_PREPARING;

        const s32 liVictimRaceCarIndex = static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);

        if (!mGyroCamDriveByL.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCamDriveByL, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            mGyroCamDriveByL.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->mGyroCamDriveByLParams);
            mGyroCamDriveByL.GetBehaviour()->AttachToRaceCar(liVictimRaceCarIndex);
        }

        if (!mGyroCamDriveByR.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCamDriveByR, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            mGyroCamDriveByR.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->mGyroCamDriveByLParams);
            mGyroCamDriveByR.GetBehaviour()->AttachToRaceCar(liVictimRaceCarIndex);
        }

        if (mGyroCamDriveByL.IsWaitingToPrepare() || mGyroCamDriveByR.IsWaitingToPrepare())
        {
            lbReady = false;
        }

        return lbReady;
    }

    // ------------------------------------------------------------------------
    // ShutdownTakedownPlayer::Prepare -- bring up the loose-attachment rig hung off the PLAYER's
    // car and aimed at the VICTIM's, plus the blend that carries the gameplay camera into it.
    // The rig adopts this player's own embedded parameter block (the one Construct re-tunes), not
    // a bank block.
    // ------------------------------------------------------------------------
    bool ShutdownTakedownPlayer::Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState >= E_STATE_LOOKBACK)
        {
            return true;
        }

        bool lbReady = true;
        meState = E_STATE_PREPARING;

        if (!mLooseAttachment.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(
                mLooseAttachment, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_2);

            mLooseAttachment.GetBehaviour()->SetParameters(&mLooseAttachmentParameters);

            CGS_ASSERT(lrSharedInfo.mpGameState->mbTakedownActive,
                       "lSharedInfo.mpGameState->mbTakedownActive");

            // The rig hangs off the PLAYER's car and looks at the VICTIM's.
            mLooseAttachment.GetBehaviour()->AttachTo(
                static_cast<s32>(lrSharedInfo.mePlayerActiveRaceCarIndex));
            mLooseAttachment.GetBehaviour()->SetTarget(
                static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID));
        }

        if (!mInterpolaterA.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                mInterpolaterA, const_cast<ArbitratorState*>(lpCallingState),
                0, KI_NEW_BEHAVIOUR_LIMIT_1);

            // The LOOKBACK blend adopts the THIRD parameter block (the linear one), not the
            // first -- the console addresses this+0xB0, i.e. mInterpolateParamsC.
            mInterpolaterA.GetBehaviour()->SetParameters(&mInterpolateParamsC);
            mInterpolaterA.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

            mInterpolaterA.GetBehaviour()->Setup(
                KF_SHUTDOWN_LOOKBACK_BLEND,
                lrSharedInfo.mpSharedCameraContainer->GetGameplayCameraHelperIndex(),
                mLooseAttachment.GetBehaviourHelperIndex(),
                lrSharedInfo.mpBehaviourManager);
        }

        if (mInterpolaterA.IsWaitingToPrepare() || mLooseAttachment.IsWaitingToPrepare())
        {
            lbReady = false;
        }

        return lbReady;
    }

    // ------------------------------------------------------------------------
    // ShutdownTakedownPlayer::Update -- the nine-state impact takedown.
    //
    // LOOKBACK rides the lookback blend; once the gyro rig exists it sets up the second blend
    // (loose attachment -> gyro), drops the loose attachment and advances to FLYBACK, and after
    // half a second it allocates that gyro rig on the victim's car. FLYBACK rides the second
    // blend, then hands over to the three sequential "zoom" beats, each of which is its own
    // loose-attachment behaviour hung off the victim's car, aimed at the player's, registering a
    // camera impact and requesting a one-shot flash hook. The last beat terminates at FINISHED.
    //
    // ⚠️ State 7 (RELEASING) has no case in the console's dispatch -- its jump-table slot falls
    // into the default assert arm -- so it is deliberately absent here too.
    // ------------------------------------------------------------------------
    Camera::Camera ShutdownTakedownPlayer::Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera lOutCamera;
        lOutCamera.Construct();

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            if (Prepare(lpCallingState, lrSharedInfo))
            {
                mfActiveTime          = 0.0f;
                mbUsedFinalShotImpact = false;
                meState               = E_STATE_LOOKBACK;
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_LOOKBACK:
        {
            if (mGyroCam.IsAllocated() && !mInterpolaterB.IsAllocated())
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                    mInterpolaterB, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                mInterpolaterB.GetBehaviour()->SetParameters(&mInterpolateParamsB);

                // FROM the loose-attachment rig TO the gyro rig. The loose attachment is dropped
                // immediately afterwards: the blend has already snapshotted its reference.
                mInterpolaterB.GetBehaviour()->Setup(
                    KF_SHUTDOWN_BLEND_DURATION,
                    mLooseAttachment.GetBehaviourHelperIndex(),
                    mGyroCam.GetBehaviourHelperIndex(),
                    lrSharedInfo.mpBehaviourManager);

                mLooseAttachment.Release();
                meState = E_STATE_FLYBACK;
            }

            if (mfActiveTime > KF_SHUTDOWN_GYRO_TIME && !mGyroCam.IsAllocated())
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                    mGyroCam, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                // ATTESTATION: unlike the sibling sites, the console forms this block's address
                // off the behaviour manager (shared-info +0x18) rather than off the named
                // parameter pointer (shared-info +0x1C) -- manager +0x12540, then the same
                // +0x840 the by-name sites use. The two spellings are the SAME storage: the
                // manager embeds the parameter bank at +0x12530 and the bank's NamedParameters
                // payload starts at bank +0x10, so manager +0x12540 is exactly the record the
                // director hands over as mpNamedParameters. Spelled by name here.
                mGyroCam.GetBehaviour()->SetParameters(
                    &lrSharedInfo.mpNamedParameters->mGyroCamTakedownParams);

                const s32 liVictimRaceCarIndex =
                    static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);
                mGyroCam.GetBehaviour()->AttachToRaceCar(liVictimRaceCarIndex);

                mGyroCam.GetBehaviour()->SetUseVehicleAttachmentCollision(true);

                mGyroCam.GetBehaviour()->SetWorldSpaceNormalizedVectorFromCar(
                    NormalizedVectorToPlayer(lrSharedInfo, liVictimRaceCarIndex));
            }

            // The frame's camera is the LOOKBACK blend's, whichever of the two arms above ran.
            lOutCamera = mInterpolaterA.GetProducedCamera();
            lOutCamera.RequestMotionBlur(0.0f, KF_UNIT);
            lOutCamera.mEffects.mfSimTimeScale =
                (mfActiveTime < KF_SHUTDOWN_GYRO_TIME) ? KF_TAKEDOWN_SIM_TIME_SCALE : KF_UNIT;
            break;
        }

        case E_STATE_FLYBACK:
            lOutCamera = mInterpolaterB.GetProducedCamera();
            lOutCamera.RequestMotionBlur(KF_UNIT, KF_UNIT);

            if (mfActiveTime > KF_SHUTDOWN_FLYBACK_TIME)
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(
                    mZoom1, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                // The first of the three shutdown zoom beats. Console order per beat: adopt the
                // beat's own loose-attachment block, bind attachment then target, register a
                // UNIT impact on the embedded impact effect, and finally pin the rig to world
                // rate so the beat keeps running through the takedown's slow-motion.
                mZoom1.GetBehaviour()->SetParameters(
                    &lrSharedInfo.mpNamedParameters->GetLooseAttachmentTakedown1Parameters());

                mZoom1.GetBehaviour()->AttachTo(
                    static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID));
                mZoom1.GetBehaviour()->SetTarget(
                    static_cast<s32>(lrSharedInfo.mePlayerActiveRaceCarIndex));

                mZoom1.GetBehaviour()->GetImpactEffect().RegisterImpact(KF_UNIT);
                mZoom1.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                mInterpolaterA.Release();
                mInterpolaterB.Release();
                mGyroCam.Release();

                meState = E_STATE_ZOOM1;
            }
            break;

        case E_STATE_ZOOM1:
            lOutCamera = mZoom1.GetProducedCamera();
            lOutCamera.mEffects.mfSimTimeScale = KF_ZOOM_SIM_TIME_SCALE;
            lOutCamera.RequestMotionBlur(0.0f, KF_UNIT);

            if (mfActiveTime < KF_SHUTDOWN_ZOOM1_FLASH_END)
            {
                Camera::RequestStartEffectHook(lOutCamera, KPC_FLASH_HOOK, KF_UNIT);
            }

            if (mfActiveTime > KF_SHUTDOWN_ZOOM2_TIME)
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(
                    mZoom2, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                mZoom2.GetBehaviour()->SetParameters(
                    &lrSharedInfo.mpNamedParameters->GetLooseAttachmentTakedown2Parameters());

                mZoom2.GetBehaviour()->AttachTo(
                    static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID));
                mZoom2.GetBehaviour()->SetTarget(
                    static_cast<s32>(lrSharedInfo.mePlayerActiveRaceCarIndex));

                mZoom2.GetBehaviour()->GetImpactEffect().RegisterImpact(KF_UNIT);
                mZoom2.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                mZoom1.Release();
                meState = E_STATE_ZOOM2;
            }
            break;

        case E_STATE_ZOOM2:
            lOutCamera = mZoom2.GetProducedCamera();
            lOutCamera.mEffects.mfSimTimeScale = KF_ZOOM_SIM_TIME_SCALE;
            lOutCamera.RequestMotionBlur(0.0f, KF_UNIT);

            if (mfActiveTime < KF_SHUTDOWN_ZOOM2_FLASH_END)
            {
                Camera::RequestStartEffectHook(lOutCamera, KPC_FLASH_HOOK, KF_UNIT);
            }

            if (mfActiveTime > KF_SHUTDOWN_ZOOM3_TIME)
            {
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourLooseAttachment>(
                    mZoom3, const_cast<ArbitratorState*>(lpCallingState),
                    0, KI_NEW_BEHAVIOUR_LIMIT_1);

                mZoom3.GetBehaviour()->SetParameters(
                    &lrSharedInfo.mpNamedParameters->GetLooseAttachmentTakedown3Parameters());

                mZoom3.GetBehaviour()->AttachTo(
                    static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID));
                mZoom3.GetBehaviour()->SetTarget(
                    static_cast<s32>(lrSharedInfo.mePlayerActiveRaceCarIndex));

                mZoom3.GetBehaviour()->GetImpactEffect().RegisterImpact(KF_UNIT);
                mZoom3.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

                mZoom2.Release();
                meState = E_STATE_ZOOM3;
            }
            break;

        case E_STATE_ZOOM3:
            lOutCamera = mZoom3.GetProducedCamera();
            lOutCamera.mEffects.mfSimTimeScale = KF_ZOOM_SIM_TIME_SCALE;
            lOutCamera.RequestMotionBlur(0.0f, KF_UNIT);

            if (mfActiveTime < KF_SHUTDOWN_ZOOM3_FLASH_END)
            {
                Camera::RequestStartEffectHook(lOutCamera, KPC_FLASH_HOOK, KF_UNIT);
            }

            if (mfActiveTime > KF_SHUTDOWN_FINISH_TIME)
            {
                meState = E_STATE_FINISHED;
            }
            break;

        case E_STATE_FINISHED:
            lOutCamera = mZoom3.GetProducedCamera();
            lOutCamera.mEffects.mfSimTimeScale = KF_UNIT;
            lOutCamera.RequestMotionBlur(0.0f, KF_UNIT);
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        lOutCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;
        return lOutCamera;
    }

    // ------------------------------------------------------------------------
    // ArbStateTakedown::Prepare -- enter the takedown state.
    //
    // On the FIRST call (while still INACTIVE) it picks which player owns the takedown, straight
    // off the game state -- no random draw, no PickNewTakedownType:
    //   * a shutdown takedown (or the debug always-on flag) -> the simple ICE player, bound to
    //     the next shot in the resource manager's takedown shot group (the index is a rolling
    //     counter taken modulo the group's shot count);
    //   * a revenge takedown                                -> the shutdown/impact player;
    //   * anything else                                     -> the B3-classic player, and the
    //     takedown type is latched to E_TYPE_B3CLASSIC.
    // Then, every call, it brings up the three behaviours this state owns directly (the
    // aftertouch-crash debug cam, its own gyro rig on the victim, and the blend into that rig),
    // prepares the moment selector, and finally chains into the chosen player's own Prepare.
    // ------------------------------------------------------------------------
    bool ArbStateTakedown::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState >= E_STATE_TAKEDOWN_PLAYING)
        {
            return true;
        }

        bool lbReady = true;

        if (meState == E_STATE_INACTIVE)
        {
            const GameState& lrGameState = *lrSharedInfo.mpGameState;

            if (lrGameState.mbIsShutdown || mbAlwaysUseShutdownCam)
            {
                CGS_ASSERT(lrSharedInfo.mpDirectorResourceManager != 0,
                           "lSharedInfo.mpDirectorResourceManager");

                const Attrib::Gen::shotgroup& lrTakedown =
                    lrSharedInfo.mpDirectorResourceManager->GetTakedown();

                CGS_ASSERT(lrTakedown.Num_ShotList() > 0,
                           "lSharedInfo.mpDirectorResourceManager->GetTakedown().Num_ShotList() > 0");

                // A rolling shot index: advance, then wrap on the group's shot count (the
                // console's own twllei-guarded modulo, so a zero count traps rather than divides).
                ++miIceMovieIndex;
                const u32 luShotCount = lrTakedown.Num_ShotList();
                const u32 luShotIndex = static_cast<u32>(miIceMovieIndex) % luShotCount;
                miIceMovieIndex = static_cast<s32>(luShotIndex);

                mSimpleIceTakedown.SetIceAnim(
                    static_cast<Camera::Camera::ShotReference*>(
                        const_cast<void*>(lrTakedown.GetShotListElement(luShotIndex))));

                mpCurrentTakedown = &mSimpleIceTakedown;
            }
            else if (lrGameState.mbIsRevengeTD)
            {
                mpCurrentTakedown = &mShutdownTakedown;
            }
            else
            {
                meTakedownType    = E_TYPE_B3CLASSIC;
                mpCurrentTakedown = &mClassicTakedown;
            }
        }

        meState = E_STATE_PREPARING;

        if (!mTakedownDebugCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourAftertouchCrash>(
                mTakedownDebugCam, this, 0, KI_NEW_BEHAVIOUR_LIMIT_1);

            // The bank's SECOND aftertouch-crash block, record +220 (the debug/crash-cam one).
            mTakedownDebugCam.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->mCrashDebugParams);

            mTakedownDebugCam.GetBehaviour()->DisableCollision();
            mTakedownDebugCam.GetBehaviour()->SetIsTempDebugCrashCamera();
        }

        if (!mGyroCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                mGyroCam, this, 0, KI_NEW_BEHAVIOUR_LIMIT_1);

            // This state's own rig takes the ZOOMED-OUT takedown block, record +2316 (gyro slot
            // 9) -- NOT the +2112 block the players adopt.
            mGyroCam.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->mGyroCamTakedownZoomedOutParams);

            const s32 liVictimRaceCarIndex =
                static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);
            mGyroCam.GetBehaviour()->AttachToRaceCar(liVictimRaceCarIndex);

            mGyroCam.GetBehaviour()->SetUseVehicleAttachmentCollision(true);

            mGyroCam.GetBehaviour()->SetWorldSpaceNormalizedVectorFromCar(
                NormalizedVectorToPlayer(lrSharedInfo, liVictimRaceCarIndex));
        }

        if (!mInterpolator.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
                mInterpolator, this, 0, KI_NEW_BEHAVIOUR_LIMIT_1);

            mInterpolator.GetBehaviour()->SetParameters(&mInterpolatorParams);
            mInterpolator.GetBehaviour()->SetTimestepType(Timestep::E_WORLD_NO_SLOMO);

            // ⚠️ This blend's "from" helper is the EXTERNAL gameplay behaviour's, taken
            // directly off the shared container -- not GetGameplayCameraHelperIndex()'s
            // bumper/external selection, which every sibling site uses. The console addresses
            // the container's first handle here, so the lookback override cannot redirect it.
            mInterpolator.GetBehaviour()->Setup(
                KF_BLEND_DURATION,
                lrSharedInfo.mpSharedCameraContainer->mGameplayExternal.GetBehaviourHelperIndex(),
                mGyroCam.GetBehaviourHelperIndex(),
                lrSharedInfo.mpBehaviourManager);
        }

        // The selector must be prepared, and (until the road-rage cut budget is spent) it must
        // already be able to see at least one valid moment.
        if (!mMomentSelector.Prepare(*lrSharedInfo.mpMomentController, *lrSharedInfo.mpBehaviourManager) ||
            (mMomentSelector.GetFramesActive() < KI_MIN_FRAMES_BEFORE_MOMENT_CHECK &&
             mMomentSelector.SnoopNumValidMoments() == 0))
        {
            lbReady = false;
        }

        if (!mpCurrentTakedown->Prepare(this, lrSharedInfo))
        {
            lbReady = false;
        }

        return lbReady;
    }

    // ------------------------------------------------------------------------
    // ArbStateTakedown::Update -- the state's own per-frame machine.
    //
    //   INACTIVE                  reset the selector's active clock and re-prepare it.
    //   PREPARING                 on a successful Prepare, clear the impact shake and the active
    //                             clock and enter TAKEDOWN_PLAYING -- or ROAD_RAGE_TAKEDOWN_PLAYING
    //                             when the event is road rage (event type 3), which also opens
    //                             the road-rage cut budget.
    //   TAKEDOWN_PLAYING          publish the chosen player's camera.
    //   ROAD_RAGE_TAKEDOWN_PLAYING  for the first second, ride this state's own blend with the
    //                             sine blur/slow-mo curve; after that, cut to the moment
    //                             selector's best establishing shot when it has a valid one,
    //                             else stay on the blend and let the failsafe timer run.
    //   CHANGING_TO_ROAMING       hand back to the roaming state.
    //
    // Every arm then shares the tail: advance the active clock, optionally run the takedown debug
    // camera, raise the camera's takedown flag bit, and clear / re-hold the effect requests
    // according to the event type.
    // ------------------------------------------------------------------------
    void ArbStateTakedown::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        if (mMomentSelector.IsPrepared())
        {
            mMomentSelector.Update(lrSharedInfo.mfTimestep);
        }

        // Set once an arm has produced this frame's camera; the effect / shake tail below only
        // runs on those arms (INACTIVE and a failed PREPARING fall straight through to the
        // debug-camera tail, exactly as the console's jump table does).
        bool lbCameraProduced = false;
        // ...and whether that arm is the one with the "Car_Reset" one-shot. TAKEDOWN_PLAYING
        // does NOT have it: when the takedown ends there it leaves for roaming immediately,
        // where ROAD_RAGE_TAKEDOWN_PLAYING first asks for the reset effect once.
        bool lbHasResetEffectArm = false;

        switch (meState)
        {
        case E_STATE_INACTIVE:
            mMomentSelector.ResetTimeActive();
            mMomentSelector.Prepare(*lrSharedInfo.mpMomentController, *lrSharedInfo.mpBehaviourManager);
            break;

        case E_STATE_PREPARING:
            if (!Prepare(lrSharedInfo))
            {
                break;
            }

            if (lrSharedInfo.mpGameState->meEventType == 3)
            {
                miRoadRageRDCutCount = 1;
                mbHasTriggeredFlash  = false;
                meState              = E_STATE_ROAD_RAGE_TAKEDOWN_PLAYING;
            }
            else
            {
                meState = E_STATE_TAKEDOWN_PLAYING;
            }

            mImpactShakeController.Construct();
            mfActiveTime = 0.0f;
            // FALLTHROUGH -- the console's case 1 tail branches straight into case 2.

        case E_STATE_TAKEDOWN_PLAYING:
            GetNonConstCamera() = mpCurrentTakedown->Update(this, lrSharedInfo);
            lbCameraProduced = true;
            break;

        case E_STATE_ROAD_RAGE_TAKEDOWN_PLAYING:
            if (mInterpolator.IsAllocated() && mfActiveTime < KF_TRANSITION_TIME)
            {
                GetNonConstCamera() = mInterpolator.GetProducedCamera();

                const f32 lfBlurAmount =
                    ClampUnit(sinf(mInterpolator.GetBehaviour()->GetParametricTime() * KF_PI));
                const f32 lfEaseAmount =
                    ClampUnit(sinf(mInterpolator.GetBehaviour()->GetParametricTime() * KF_HALF_PI));

                GetNonConstCamera().RequestMotionBlur(lfBlurAmount, lfEaseAmount);
                GetNonConstCamera().mEffects.mfSimTimeScale =
                    Camera::Utils::SineLerp(KF_FAILSAFE_SIM_TIME_FLOOR, KF_UNIT, mfActiveTime);
                GetNonConstCamera().mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

                mfFailsafeTimer = KF_MIN_FAILSAFE_TIME;
            }
            else
            {
                // Pick a fresh establishing moment once the failsafe has had its second and the
                // cut budget still has room.
                // The failsafe gate is a taken-when-unordered branch on the console (the skip
                // is the "less than" arm), so an unordered timer RUNS the pick; spell it as the
                // negated less-than rather than >=.
                if (!mMomentSelector.HasSelectedMoment() &&
                    !(mfFailsafeTimer < KF_MIN_FAILSAFE_TIME) &&
                    miRoadRageRDCutCount < KI_MAX_ROADRAGE_CUTS)
                {
                    mMomentSelector.SelectBestMoment(*reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));
                }

                // A selected-but-not-yet-valid moment is dropped and re-picked, and that spends
                // one of the cuts.
                if (mMomentSelector.HasSelectedMoment() && !mMomentSelector.GetSelectedMoment()->IsValid())
                {
                    mMomentSelector.CancelSelection();
                    mMomentSelector.SelectBestMoment(*reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));
                    ++miRoadRageRDCutCount;
                }

                if (mMomentSelector.HasSelectedMoment() && mMomentSelector.GetSelectedMoment()->IsValid())
                {
                    GetNonConstCamera() = mMomentSelector.GetSelectedMoment()->GetCamera();
                    mfFailsafeTimer = 0.0f;
                }
                else
                {
                    if (mMomentSelector.HasSelectedMoment())
                    {
                        mMomentSelector.CancelSelection();
                    }

                    mfFailsafeTimer = lrSharedInfo.mfTimestep + mfFailsafeTimer;

                    GetNonConstCamera() = mInterpolator.GetProducedCamera();
                    GetNonConstCamera().RequestMotionBlur(0.0f, KF_UNIT);
                    GetNonConstCamera().mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
                }
            }
            lbCameraProduced     = true;
            lbHasResetEffectArm  = true;
            break;

        case E_STATE_CHANGING_TO_ROAMING:
            // The exit edge: hand the frame back to the roaming state. On a successful switch
            // ChangeToState runs THIS state's Release (which drops every behaviour hold and
            // resets meState to INACTIVE); on a blocked one it parks meState here so the edge is
            // retried next frame.
            ArbUtils::ChangeToState<EState>(this, lrSharedInfo,
                                            ArbitratorStateContainer::E_STATE_ROAMING,
                                            meState, E_STATE_CHANGING_TO_ROAMING);
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        if (lbCameraProduced)
        {
            if (lrSharedInfo.mpGameState->mbTakedownActive)
            {
                // Anchor the impact shake on the victim's car.
                VehicleRef lVictimRef;
                lVictimRef.mbSet = false;
                lVictimRef.SetToRaceCar(lrSharedInfo.mpGameState->meTakedownVictimID);

                // The shake only runs while that reference resolves to a LIVE race car.
                if (lVictimRef.IsValid(*lrSharedInfo.mpAllVehicleData))
                {
                    mImpactShakeController.Update(GetNonConstCamera(),
                                                  lrSharedInfo.mfSimTimestep,
                                                  *lrSharedInfo.mpAllVehicleData,
                                                  *lrSharedInfo.mpPlayerTracker,
                                                  *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom),
                                                  *lrSharedInfo.mpDebugPrinter,
                                                  lVictimRef);
                }
            }
            else if (!lbHasResetEffectArm || mbHasTriggeredFlash)
            {
                // The takedown is over -- and either this arm has no reset-effect step at all,
                // or it has already run once. Leave for roaming.
                ArbUtils::ChangeToState<EState>(this, lrSharedInfo,
                                                ArbitratorStateContainer::E_STATE_ROAMING,
                                                meState, E_STATE_CHANGING_TO_ROAMING);
            }
            else
            {
                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              "Car_Reset", KF_UNIT);
                mbHasTriggeredFlash = true;
            }
        }

        mfActiveTime = lrSharedInfo.mfTimestep + mfActiveTime;

        // The debug takedown camera: when armed AND allocated AND the victim's race-car slot is
        // live, publish the aftertouch-crash camera and shift it by the player-to-victim offset.
        if (mbUseTakedownDebugCam && mTakedownDebugCam.IsAllocated())
        {
            const s32 liVictimRaceCarIndex =
                static_cast<s32>(lrSharedInfo.mpGameState->meTakedownVictimID);

            if (lrSharedInfo.mpAllVehicleData->GetUsedRaceCarsBitArray().IsBitSet(
                    static_cast<u32>(liVictimRaceCarIndex)))
            {
                GetNonConstCamera() = mTakedownDebugCam.GetProducedCamera();

                const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
                    *lrSharedInfo.mpPlayerCarTransform;

                const rw::math::vpu::Vector3 lPlayerToVictim =
                    lrSharedInfo.mpAllVehicleData->GetRaceCar(
                        static_cast<EActiveRaceCarIndex>(liVictimRaceCarIndex))
                            .mRaceCarState.mTransform.wAxis -
                    lrPlayerTransform.wAxis;

                GetNonConstCamera().mTransform.wAxis =
                    GetNonConstCamera().mTransform.wAxis + lPlayerToVictim;
            }
        }

        // The camera's takedown bit, raised unconditionally on every arm.
        GetNonConstCamera().mState_uFlags |= KI_CAMERA_DIRTY_TAKEDOWN;

        // Event-type housekeeping: road rage (3) and one other mode (8) clear the two hook
        // latches and the requested post-FX id outright...
        const s32 liEventType = lrSharedInfo.mpGameState->meEventType;
        if (liEventType == 3 || liEventType == 8)
        {
            GetNonConstCamera().mEffects.mbHasStartHookNameString = false;
            GetNonConstCamera().mEffects.mbHasStopHookNameString  = false;
            GetNonConstCamera().mEffects.muRequestedPostFxId      = 0;
        }

        // ...and outside road rage, with neither a revenge nor a shutdown takedown running, the
        // state holds the game clock at 2/3 instead of whatever the player asked for.
        if (lrSharedInfo.mpGameState->meEventType != 3 &&
            !lrSharedInfo.mpGameState->mbIsRevengeTD &&
            !lrSharedInfo.mpGameState->mbIsShutdown)
        {
            GetNonConstCamera().mEffects.mfSimTimeScale = KF_NON_TAKEDOWN_SIM_SCALE;
        }
    }
}
