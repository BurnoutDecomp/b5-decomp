// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTakedown.cpp
//
// See BrnArbStateTakedown.h for the class layouts, provenance, and the file-wide banner
// explaining which of this TU's 18 functions are bodied here vs. left declaration-only (and
// why). This .cpp only defines the tractable subset:
//   - ArbStateTakedown::GetName            @0x821F62E0
//   - ArbStateTakedown::Release             @0x822353B8
//   - DriveByTakedownPlayer::Update          @0x8225A000
//   - SimpleIceTakedownPlayer::Prepare       @0x8226CF38
//   - SimpleIceTakedownPlayer::Update        @0x8225A1E8
//   - SimpleIceTakedownPlayer::Release       @0x82235208
// The remaining twelve functions are declaration-only in the header, each with an inline FLAG
// comment naming exactly what blocks it.
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
