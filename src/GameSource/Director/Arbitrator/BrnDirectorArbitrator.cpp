#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h"
#include "types.hpp"

// ============================================================================
// BrnDirector::Arbitrator -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct               @0x82254E90   [reconstructed]
//   Update                  @0x8226ADA0   [DECLARATION-ONLY + FLAG -- see header / below]
//   UpdateCameraCycleControl@0x821F5D28   [reconstructed]
//   Destruct                (no asm in this TU's function set)   [DECLARATION-ONLY]
//   UpdateDebugCameras      (no asm in this TU's function set)   [DECLARATION-ONLY]
//
// The Director camera ARBITRATOR. Construct stands the whole arbitrator up (the embedded
// state container, the shared-camera container, the three special-cam states, the
// final-elite + debug camera handles) and seeds the outer state machine to PREPARE.
// UpdateCameraCycleControl runs the "hold to slow-mo / tap to cycle" normal-camera control.
// Update -- the outer state-select spine -- is documented but left declaration-only (it
// dispatches across a not-yet-homed camera/effect/behaviour-manager API surface; writing a
// body would require fabricating those signatures + the trimmed-DWARF flag offsets, which the
// reconstruction rules forbid).
//
// All member access is BY NAME (the project's x64-gate rule); the X360 offsets quoted in the
// header are provenance only.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // Construct @0x82254E90 -- stand the arbitrator up.
    //
    // X360 order (the asm runs the embedded subobjects' init inline):
    //   1. mStateContainer.ConstructAll()                         (ConstructAll(this+0x310))
    //   2. seed the shared-camera container (its primary-active flag = 1, the rest cleared)
    //   3. mArbStateCrashNav.Construct()                          ((**(this+0x3910))(...))
    //   4. mArbStateAttractMode.Construct()                       ((**(this+0x41A0))(...))
    //   5. mArbStateRenderMetrics.Construct()                     ((**(this+0x4340))(...))
    //   6. clear mFinalEliteCam                                   (this+0x44E0 words zeroed)
    //   7. mArbStateTestbed.Construct()                           ((**(this+0x30))(...))
    //   8. clear the two debug-camera handles                     (this+0x00..+0x24 zeroed)
    //   9. seed the scalars + flags:
    //        mfSlomoFactor          = 1.0   (+0x4504)
    //        mfTimeCycleCameraHeld  = 0.0   (+0x44F4)
    //        mbCycleNormalCamSelector = 0   (+0x301)
    //        mbCameraCycleInitialised = 1   (+0x300)
    //        mbDoAttractMode        = 0     (+0x44FD)
    //        mbDoRenderMetrics      = 0     (+0x44FE)
    //        mbStartOfGame          = 1     (+0x44FC)
    //        mbWasDoingTrainingLastFrame = 0 (+0x4500)
    //        meState                = E_STATE_PREPARE (0)   (+0x44F8)
    // ------------------------------------------------------------------------
    void Arbitrator::Construct()
    {
        // 1. the embedded 11-state container.
        mStateContainer.ConstructAll();

        // 2. seed the shared-camera container. The X360 inlines its field init here: the
        //    primary gameplay camera starts active and not suspended, with its handle
        //    book-keeping cleared. Reached BY NAME; the full container init lands with the
        //    SharedCameraContainer TU (this TU only seeds the two attested selection flags).
        mSharedCameraContainer.mbPrimaryActive    = true;   // X360 stb 1, +0x14560 (container +0x00)
        mSharedCameraContainer.mbPrimarySuspended = false;  // X360 stb 0, +0x14561 (container +0x01)

        // 3-5. the three special-cam states (vtable slot 0 == Construct).
        mArbStateCrashNav.Construct();
        mArbStateAttractMode.Construct();
        mArbStateRenderMetrics.Construct();

        // 6. clear the final-elite ICE-cam handle (all five words zeroed).
        mFinalEliteCam = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();

        // 7. the test-bed state (vtable slot 0 == Construct).
        mArbStateTestbed.Construct();

        // 8. clear the two debug-camera handles.
        mDebugCameraOrbitPlayer = Camera::BehaviourHandle<Camera::BehaviourDebugOrbitPlayer>();
        mDebugCameraFlyWorld    = Camera::BehaviourHandle<Camera::BehaviourDebugFlyWorld>();

        // 9. scalars + flags.
        mfSlomoFactor               = 1.0f;
        mfTimeCycleCameraHeld       = 0.0f;
        mbCycleNormalCamSelector    = false;
        mbCameraCycleInitialised    = true;
        mbDoAttractMode             = false;
        mbDoRenderMetrics           = false;
        mbStartOfGame               = true;
        mbWasDoingTrainingLastFrame = false;
        meState                     = E_STATE_PREPARE;
    }

    // ------------------------------------------------------------------------
    // UpdateCameraCycleControl @0x821F5D28 -- the "hold to slow-mo / tap to cycle" control.
    //
    // Clears both out flags, then:
    //   * While the cycle button is HELD: accumulate the timestep into mfTimeCycleCameraHeld.
    //     The first frame the running total lands in the (1.0, 2.0) window, snap it to 2.0,
    //     FLIP the normal-camera selector (mbCycleNormalCamSelector = !mbCycleNormalCamSelector,
    //     the X360 (cntlzw(sel) & 0x20)!=0 idiom == "sel == 0"), and report "changed this
    //     frame".
    //   * On RELEASE: if the accumulated time was a short tap -- in (0.0, 1.0) -- report
    //     "cycle camera". Either way reset mfTimeCycleCameraHeld to 0.
    // ------------------------------------------------------------------------
    void Arbitrator::UpdateCameraCycleControl(f32 lfTimestep, bool lbCycleCameraHeld,
                                              bool& lrbCycleCameraOut, bool& lrbChangedThisFrameOut)
    {
        lrbCycleCameraOut      = false;   // *a5 = 0
        lrbChangedThisFrameOut = false;   // *a6 = 0

        const f32 lfTimeBeforeUpdate = mfTimeCycleCameraHeld;   // v6

        if (lbCycleCameraHeld)
        {
            const f32 lfTimeNow = lfTimestep + mfTimeCycleCameraHeld;   // v7
            mfTimeCycleCameraHeld = lfTimeNow;

            if (lfTimeNow > 1.0f && lfTimeNow < 2.0f)
            {
                mfTimeCycleCameraHeld    = 2.0f;
                // X360: *(this+0x301) = (cntlzw(sel) & 0x20) != 0, i.e. the selector toggles
                // (count-leading-zeros of a u32 is 0x20 only when the value is zero).
                mbCycleNormalCamSelector = (!mbCycleNormalCamSelector);
                lrbChangedThisFrameOut   = true;
            }
        }
        else
        {
            if (lfTimeBeforeUpdate > 0.0f && lfTimeBeforeUpdate < 1.0f)
            {
                lrbCycleCameraOut = true;
            }
            mfTimeCycleCameraHeld = 0.0f;
        }
    }

    // ------------------------------------------------------------------------
    // Update @0x8226ADA0 -- the OUTER arbitrator state-select spine.  DECLARATION-ONLY + FLAG.
    //
    // The body (X360 @0x8226ADA0) is a switch on meState that, per outer state, picks the
    // arbitrator state to run and copies its produced camera into lrCameraInOut:
    //   PREPARE(0)                 -> Prepare the shared-camera container, allocate the two
    //                                 debug-camera behaviours, advance to PRE_NORMAL.
    //   PRE_NORMAL(1)              -> snapshot, advance to NORMAL, fall into NORMAL.
    //   NORMAL(2)                  -> drive the camera-cycle control + CycleNormalCamera, tick
    //                                 mStateContainer.UpdateAll, copy the container's selected
    //                                 state camera, run the black-fade-water / jump-effect /
    //                                 debug-camera post-FX, and watch for the CRASH_NAV /
    //                                 attract / render-metrics triggers.
    //   CRASH_NAV(3) / CRASH_NAV_ICE_CAMERAS(4) -> run mArbStateCrashNav, copy its camera.
    //   CHANGING_TO_ATTRACT_MODE(5)/ATTRACT_MODE(6) -> run mArbStateAttractMode + the cycle
    //                                 control, copy its camera.
    //   FINAL_ELITE_SEQUENCE(7)    -> allocate/drive the final-elite BehaviourIceAnim, copy it.
    //   RENDER_METRICS(8)          -> run mArbStateRenderMetrics, copy its camera.
    //   RELEASE(9)                 -> ReleaseAll, release the crash-nav / test-bed states + the
    //                                 shared cameras.
    // The epilogue clears the camera "lookback" flag bit when mbStartOfGame is set.
    //
    // FLAG: NOT reconstructed as a body. The dispatch reaches a deep, not-yet-homed API --
    //   SharedCameraContainer::{SetUsingGameplayExternal,SetLookbackOverride,GetGameplayExternal,
    //   GetGameplayCamera,Prepare,Release}, Camera::Camera::operator= (the take copy),
    //   Camera::EnsureEffectIsStopped, CameraEffects::{SetStart/StopHookNameString,SetSimTimeScale,
    //   SetRequestedPostFX,ClearStart/StopHookNameString}, DepthOfField::SetBlurriness,
    //   DirectorOutputInterface::SetPauseRequest, BehaviourManager::NewBehaviour<>,
    //   HookNameStringWrapper::Set, BehaviourGameplayExternal::SnapToCar, the gameplay-external
    //   collision-policy resets, and the BehaviourIceAnim final-elite seed -- none of which have
    //   reconstructed homes / attested signatures yet, and the trimmed DWARF omits several of the
    //   per-camera flag offset->name mappings the body needs. Writing it would require
    //   FABRICATING that surface, which the rules forbid. Left declaration-only until those
    //   camera / effect / behaviour-manager TUs land (then re-home this body against them BY NAME).
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Destruct / UpdateDebugCameras -- DECLARATION-ONLY (no X360 asm in this TU's function set;
    // the dossier carries DWARF variable hints only). Their bodies land when their asm is
    // attested (or with the debug-camera / behaviour-tweaker TUs).
    // IsDebugCamera / DebugCameraFlyWorldLookAt / GetDebugFlyWorldTransform / GetNormalCamera /
    // CycleNormalCamera are likewise declaration-only here (no asm in this TU's function set).
    // ------------------------------------------------------------------------
}
