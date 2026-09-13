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
// The five Prepare / three Update functions still declaration-only in the header, plus
// ArbStateTakedown::Destruct and PickNewTakedownType, each carry a one-line FLAG there naming
// the single missing declaration or missing asm body that blocks them.
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"    // Camera::RequestStartEffectHook
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h" // GameState::meTakedownVictimID

namespace BrnDirector
{
    // Local alias for the camera "this behaviour produced the camera this frame" dirty flag,
    // matching the sibling arbitrator-state TUs (BrnArbStatePostEvent.cpp / BrnArbStateRaceIntro.cpp
    // / BrnArbStateRankUp.cpp each define this same local constant rather than sharing a global one).
    namespace
    {
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;
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
}
