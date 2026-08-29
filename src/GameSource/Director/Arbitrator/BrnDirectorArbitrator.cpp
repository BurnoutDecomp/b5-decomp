#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h"
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (the fly-world handle tripwire)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"          // ArbitratorState (cycle request / camera)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"    // BehaviourDebugFlyWorld (WarpToLookAt)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"             // DirectorIO::ControlInput (the named camera-control bytes)

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
        //    external gameplay camera starts selected, with no lookback override and the
        //    handle book-keeping cleared. Reached BY NAME; the full container init lands with
        //    the SharedCameraContainer TU (this TU only seeds the two attested selection
        //    flags -- DWARF names, BrnDirectorArbitratorSharedCameraContainer.h:90/91).
        mSharedCameraContainer.mbUseGameplayExternal = true;   // X360 stb 1, +0x14560 (container +0x00)
        mSharedCameraContainer.mbLookbackOverride    = false;  // X360 stb 0, +0x14561 (container +0x01)

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

    // ========================================================================
    // Update @0x8226ADA0 -- the OUTER arbitrator state-select spine.  RECONSTRUCTED.
    //
    // Per outer state it picks the arbitrator state that owns the frame and copies that
    // state's produced camera into lrCameraInOut. THIS is the function that makes a director
    // camera track anything, and case 5/6 is the DJ fly-by (attract mode) path.
    //
    // ⭐ WHY THIS IS A BODY NOW. The previous wave left it declaration-only on the grounds
    // that the per-camera flag pokes had no name mapping. Re-checked against the committed
    // headers, every one of them resolves (camera-relative -> Camera/CameraEffects member):
    //     +104 -> mEffects.mStartHookNameString        +137 -> mEffects.mStopHookNameString
    //     +228 -> mEffects.muRequestedPostFxId         +232 -> mEffects.mfStartHookNameBlendAmount
    //     +260 -> mEffects.mfSimTimeScale              +287 -> mEffects.mbHasStartHookNameString
    //     +288 -> mEffects.mbHasStopHookNameString     +308 -> mDepthOfField.mfBlurriness
    //     +320 -> mState_uFlags
    // and the arbitrator-relative ones close against this class's committed member map
    // (+784 mStateContainer, +14560 mSharedCameraContainer, +14608 mArbStateCrashNav,
    //  +16800 mArbStateAttractMode, +17216 mArbStateRenderMetrics, +17632 mFinalEliteCam,
    //  +17656 meState, +17660..+17664 the flag quad, +17668 mfSlomoFactor). Two further
    // cross-checks: `arb + 7072` == mStateContainer + 0x1890 == the ROAMING state (the same
    // offset BrnArbStateAttractMode.cpp already cites), and the "+2909 / +670 / +656 = FLT_MAX"
    // triple in the crash branch is exactly the committed
    // SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish (its header quotes +0xB5D /
    // +0x29E).
    //
    // ⚠️ WHAT IS STILL GATED: every remaining gate here is a byte inside the un-homed
    // MainDirector GameState block that the shared info carries as mpGameState. Each gate below
    // carries its own consequence + DELETE-when.
    //
    // ⛔ THE SECOND HALF OF THIS NOTE WAS WRONG, AND IT IS RETIRED (2026-08-29). It read:
    // "(b) BehaviourManager::NewBehaviour<> -- the behaviour ALLOCATION path. (b) is the wall:
    //  NewBehaviour @0x82267418 needs BehaviourHelper::Prepare @0x82255F48, which dispatches a
    //  virtual through the pooled object's own vtable, i.e. the un-homed Camera::Behaviour base.
    //  Until that base is homed, NO camera behaviour can be allocated by anyone."
    // BehaviourHelper::Prepare has been homed since the Pass-A re-home, and behaviours HAVE been
    // allocated since: the crash camera's deathcam and taken-down cam both come through
    // NewBehaviour<TBehaviour> (bodied out-of-line in BrnBehaviourManager.h), filmed 2026-08-29.
    // The only thing @0x82267418 was ever missing is its own runtime TYPE DISPATCH off the shot
    // RefSpec's class key -- landed in BrnBehaviourManager_NewBehaviourFromShot.cpp with the
    // drive-thru camera. ⚠️ Read the DEBUG-CAMERA gates below on their own merits; they are NOT
    // blocked by this any more.
    // ========================================================================
    void Arbitrator::Update(bool lbPaused, Camera::Camera& lrCameraInOut,
                            ArbStateSharedInfo& lrSharedInfo,
                            bool lbCycleCamera, bool lbCycleCameraHeld)
    {
        // The prologue: publish this arbitrator's two containers into the shared info (the
        // caller deliberately leaves both slots for us -- see MainDirector::UpdateArbitrator).
        lrSharedInfo.mpStateContainer        = &mStateContainer;          // *(info+20)
        lrSharedInfo.mpSharedCameraContainer = &mSharedCameraContainer;   // *info

        // X360: `if ( mpGameState[+337] ) mSharedCameraContainer.mbUseGameplayExternal = 1;`
        // ⚠️ GATE: the +337 byte lives in the un-homed MainDirector GameState block, so the
        //   test cannot be written. CONSEQUENCE: the external ("chase") gameplay camera is not
        //   force-selected by whatever game mode raises that byte; the container keeps whatever
        //   Construct left (external selected) or whatever a state last set.
        //   DELETE-WHEN: BrnDirector::GameState is homed.

        mSharedCameraContainer.mbLookbackOverride = false;                // *(arb+14561) = 0

        switch (meState)
        {
        // ---- 0: PREPARE ---------------------------------------------------------------
        case E_STATE_PREPARE:
            mSharedCameraContainer.Prepare(lrSharedInfo);
            // ⚠️ GATE: the two debug-camera allocations
            //     BehaviourManager::NewBehaviour<BehaviourDebugOrbitPlayer>(
            //         *info.mpBehaviourManager, mDebugCameraOrbitPlayer, 0, 0, 1 );
            //     BehaviourManager::NewBehaviour<BehaviourDebugFlyWorld>( ... );
            //     mDebugCameraOrbitPlayer.SetUpdatesDuringPause(true);
            //     mDebugCameraFlyWorld.SetUpdatesDuringPause(true);
            //   -- NewBehaviour<> is declaration-only behind the un-homed Camera::Behaviour
            //   base (see the banner). CONSEQUENCE: the fly-world / orbit-player debug cameras
            //   never exist; UpdateDebugCameras has nothing to drive and IsDebugCamera() stays
            //   false. Nothing in the shipping path uses them.
            //   DELETE-WHEN: Camera::Behaviour + BehaviourHelper::Prepare are homed and
            //   BehaviourManager::NewBehaviour<> is bodied.
            meState = E_STATE_PRE_NORMAL;
            break;

        // ---- 1: PRE_NORMAL ------------------------------------------------------------
        case E_STATE_PRE_NORMAL:
            // X360: `*(arb+14556) = *(arb+14516)` -- container +0x35CC = container +0x35A4,
            // i.e. mpCurrentState = mArrayOfStatePointers[1] == the ROAMING slot.
            mStateContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
            meState = E_STATE_NORMAL;
            // fall through -- the X360 jumps straight into the NORMAL body this same frame.
            [[fallthrough]];

        // ---- 2: NORMAL ----------------------------------------------------------------
        case E_STATE_NORMAL:
        {
            bool lbChangedThisFrame = false;

            if (!lbPaused)
            {
                UpdateCameraCycleControl(lrSharedInfo.mfTimestep, lbCycleCameraHeld,
                                         lbCycleCamera, lbChangedThisFrame);
            }

            // X360: `mSharedCameraContainer.mbLookbackOverride = mpControllerInfo[4]`.
            // The shared info types that slot as the (forward-declared) ControllerInfo; the
            // block it points at is the committed DirectorIO::ControlInput the input buffer
            // publishes, whose +4 byte is the named lookback query.
            mSharedCameraContainer.mbLookbackOverride =
                reinterpret_cast<const DirectorIO::ControlInput*>(
                    lrSharedInfo.mpControllerInfo)->IsLookbackHeld();

            if (lbCycleCamera)
                CycleNormalCamera();

            if (!lbPaused)
                mStateContainer.UpdateAll(lrSharedInfo);

            // The test-bed state ticks every frame regardless (vtable +8), then its
            // cycle-camera request byte is cleared (arb +0x1A1 == testbed +0x171).
            mArbStateTestbed.Update(lrSharedInfo);
            mArbStateTestbed.ClearCycleCameraThisFrame();

            // ⚠️ GATE: `if ( GetCurrentState() == <container +0x2F70> &&
            //               (GetNormalCamera().mState_uFlags & 2) ) mbStartOfGame = false;`
            //   -- the compare is against an embedded container sub-state at container
            //   +0x2F70 whose EState identity is NOT pinned (the container models its states
            //   by name, not by console offset, so the offset cannot be resolved to a slot).
            //   Guessing it would be a "0 is a valid id, not none"-class mistake.
            //   CONSEQUENCE: mbStartOfGame is never cleared here, so the epilogue keeps
            //   clearing the camera's lookback flag bit every frame. Harmless (the bit starts
            //   clear); it only means a start-of-game lookback request would be suppressed.
            //   DELETE-WHEN: the container's per-state console offsets are mapped to EStates.

            lrCameraInOut = GetNormalCamera();

            // ⚠️ GATE: the "BlackFade_Water" branch. X360:
            //     if ( mpPlayerCrashInfo[+39] ) {
            //         lrCameraInOut.mState_uFlags &= ~2;
            //         lrCameraInOut.GetEffects().mStartHookNameString.Set("BlackFade_Water");
            //         ...mfStartHookNameBlendAmount = 1.0f; ...mbHasStartHookNameString = true;
            //         ...mbHasStopHookNameString = false;   ...muRequestedPostFxId = 0;
            //         mSharedCameraContainer.ForcePrimaryGameplayBehaviourToFinish();
            //     } else {
            //         Camera::EnsureEffectIsStopped(lrCameraInOut, *info.mpEffectInterface,
            //                                       "BlackFade_Water");
            //     }
            //   The camera side is fully named (see the banner) -- what is NOT reachable is the
            //   CONDITION: `mpPlayerCrashInfo[+39]` is a byte inside BrnDirector::PlayerCrashInfo,
            //   which has no homed layout (MainDirector hands the slot on as raw input-buffer
            //   storage). Running either arm on a guessed condition would drive a visible
            //   full-screen fade at the wrong times, so BOTH arms are gated rather than one
            //   picked. Both callees are also declaration-only today
            //   (ForcePrimaryGameplayBehaviourToFinish / EnsureEffectIsStopped).
            //   CONSEQUENCE: the drown/reset black-fade is neither started nor stopped by the
            //   arbitrator. DELETE-WHEN: PlayerCrashInfo is homed.

            // ⚠️ GATE: `if ( lbPaused && GetCurrentState() == mStateContainer.GetState(
            //               E_STATE_ROAMING) ) lrCameraInOut = mSharedCameraContainer.
            //               GetSelectedGameplayCamera();`
            //   -- the pause-in-roaming hand-over to the live gameplay camera. Its callee
            //   (GetSelectedGameplayCamera) is declaration-only AND resolves a behaviour
            //   through the manager, i.e. it is behind the same allocation wall.
            //   CONSEQUENCE: while paused in roaming the camera holds the roaming state's own
            //   camera instead of re-reading the gameplay behaviour's. DELETE-WHEN: as (b).

            // ⚠️ GATE: `UpdateDebugCameras(lrCameraInOut, *info.mpBehaviourManager,
            //                              lbCycleCamera, lbChangedThisFrame);`
            //   -- declaration-only (no X360 asm in this TU's function set) and it drives the
            //   two debug-camera behaviours that the PREPARE gate above never allocates.
            //   CONSEQUENCE: none on the shipping path.

            // ⭐ THE ATTRACT-MODE / DJ-FLY-BY TRIGGER (arb +0x44FD -> +0x44F8 = 5).
            if (mbDoAttractMode)
                meState = E_STATE_CHANGING_TO_ATTRACT_MODE;

            // ⚠️ GATE: the CRASH_NAV trigger --
            //     `if ( (mpGameState[257] || mpGameState[432]) && !mpGameState[217] ) {
            //          mArbStateCrashNav.Prepare(info); mArbStateCrashNav.Update(info);
            //          meState = E_STATE_CRASH_NAV_ICE_CAMERAS; }`
            //   All three keys are bytes in the un-homed GameState block. CONSEQUENCE: crash
            //   navigation (picture paradise) is never entered from here. DELETE-WHEN: as (a).

            // ⚠️ GATE: the RENDER_METRICS trigger --
            //     `if ( mbDoRenderMetrics ) { meState = E_STATE_RENDER_METRICS;
            //          mArbStateRenderMetrics.Prepare(info); }`
            //   The FLAG itself is a named member, but the state it enters is the
            //   ArbStateRenderMetrics PLACEHOLDER (an empty ArbitratorState subclass with no
            //   real home), so entering it would park the arbitrator on a state whose Update
            //   does nothing and whose camera is never written -- a stuck camera, worse than
            //   not entering. CONSEQUENCE: the GameTalk StartRenderMetrics command sets the
            //   flag but the arbitrator ignores it. DELETE-WHEN: ArbStateRenderMetrics gets a
            //   real home TU.

            // ⚠️ GATE: the "Jump_Effect" blend (the mbWasDoingTrainingLastFrame latch +
            //   mfSlomoFactor ramp driving mEffects.mfSimTimeScale / mDepthOfField.mfBlurriness
            //   and the output interface's two pause-request bytes). Keyed off mpGameState[450]
            //   -- un-homed. Both members it ramps ARE named here, so this un-gates the moment
            //   GameState does. CONSEQUENCE: no jump slow-mo / blur. DELETE-WHEN: as (a).
            break;
        }

        // ---- 3: CRASH_NAV --------------------------------------------------------------
        case E_STATE_CRASH_NAV:
            lrCameraInOut = mStateContainer.GetCurrentState()->GetCamera();
            // ⚠️ GATE: `if ( !mpGameState[257] ) meState = E_STATE_NORMAL;` -- un-homed
            //   GameState byte. CONSEQUENCE: once entered (which the trigger gate above
            //   prevents today) this state would not exit. DELETE-WHEN: as (a).
            break;

        // ---- 4: CRASH_NAV_ICE_CAMERAS --------------------------------------------------
        case E_STATE_CRASH_NAV_ICE_CAMERAS:
            // X360 asserts mArbStateCrashNav.IsActive() (BrnDirectorArbitrator.cpp:295) --
            // ⚠️ GATE: IsActive() is DWARF-listed but has no committed declaration on
            //   ArbStateCrashNav (its header records the omission), so the tripwire is not
            //   reproduced. Non-gating on the console too.
            mArbStateCrashNav.Update(lrSharedInfo);
            lrCameraInOut = mArbStateCrashNav.GetCamera();
            // ⚠️ GATE: the `if ( !mArbStateCrashNav.<active> ) meState = E_STATE_NORMAL;` exit,
            //   same missing accessor. DELETE-WHEN: ArbStateCrashNav::IsActive is declared.
            break;

        // ---- 5: CHANGING_TO_ATTRACT_MODE  /  6: ATTRACT_MODE  ⭐ THE DJ FLY-BY ----------
        case E_STATE_CHANGING_TO_ATTRACT_MODE:
        {
            lrCameraInOut = GetNormalCamera();

            // ⚠️ GATE: the transition's own post-FX --
            //     lrCameraInOut.GetEffects().mStartHookNameString.Set("Jump_Effect");
            //     ...mbHasStartHookNameString = true;  ...mfStartHookNameBlendAmount = 1.0f;
            //   Every field is named and this WOULD be writable; it is held back only because
            //   HookNameStringWrapper::Set is declaration-only (its body lands with the
            //   effect-trigger TU) and firing a hook the EffectInterface cannot resolve is
            //   worse than not firing it. CONSEQUENCE: no screen effect on the cut into attract
            //   mode. DELETE-WHEN: HookNameStringWrapper::Set is bodied.

            if (mArbStateAttractMode.Prepare(lrSharedInfo))
            {
                meState = E_STATE_ATTRACT_MODE;
                // The X360 falls straight into the ATTRACT_MODE body this same frame.
                UpdateAttractMode(lrCameraInOut, lrSharedInfo, lbCycleCamera, lbCycleCameraHeld);
            }
            else
            {
                // Still preparing: tick the state so it can keep trying.
                mArbStateAttractMode.Update(lrSharedInfo);
            }
            break;
        }

        case E_STATE_ATTRACT_MODE:
            UpdateAttractMode(lrCameraInOut, lrSharedInfo, lbCycleCamera, lbCycleCameraHeld);
            break;

        // ---- 7: FINAL_ELITE_SEQUENCE ---------------------------------------------------
        case E_STATE_FINAL_ELITE_SEQUENCE:
            // ⚠️ GATE (whole case). The X360 allocates a BehaviourIceAnim into mFinalEliteCam
            //   via BehaviourManager::NewBehaviour<>, binds it to the AttribSys instance at
            //   `info.mpDirectorResourceManager + 1288` (id 354708297, falling back to
            //   Attrib::DefaultDataArea(24)), pokes six fields of the behaviour's interior
            //   (+3612/+3600/+3608/+3604/+3624/+40/+4/+3556) and then drives it, exiting on
            //   mpGameState[432]. It is behind BOTH walls at once: the allocation path (b) and
            //   the un-homed Behaviour interior those six pokes land in.
            //   CONSEQUENCE: the final-elite ("burnout licence complete") camera sequence does
            //   not run; the arbitrator holds whatever camera the previous state left.
            //   DELETE-WHEN: Camera::Behaviour + BehaviourIceAnim are homed.
            break;

        // ---- 8: RENDER_METRICS ---------------------------------------------------------
        case E_STATE_RENDER_METRICS:
            mArbStateRenderMetrics.Update(lrSharedInfo);
            lrCameraInOut = mArbStateRenderMetrics.GetCamera();
            if (!mbDoRenderMetrics)
            {
                mArbStateRenderMetrics.Release(lrSharedInfo);
                meState = E_STATE_NORMAL;
            }
            break;

        // ---- 9: RELEASE ----------------------------------------------------------------
        case E_STATE_RELEASE:
            mStateContainer.ReleaseAll(lrSharedInfo);
            mArbStateCrashNav.Release(lrSharedInfo);     // vtable +0xC
            mArbStateTestbed.Release(lrSharedInfo);      // vtable +0xC
            mDebugCameraOrbitPlayer.Release();
            mDebugCameraFlyWorld.Release();
            // ⚠️ GATE: `mSharedCameraContainer.Release(info)` (X360 @0x822348A8) -- the
            //   committed SharedCameraContainer has no Release declaration, and adding one
            //   would be a declaration-only call on a teardown path. CONSEQUENCE: the two
            //   shared gameplay behaviours are not handed back to the manager on arbitrator
            //   release; the manager's own ReleaseBehaviours sweep is the backstop.
            //   DELETE-WHEN: SharedCameraContainer::Release is homed.
            break;

        default:
            break;
        }

        // ---- epilogue (X360 LABEL_64) ---------------------------------------------------
        // While the arbitrator is still in its start-of-game window, force the camera's
        // "lookback" state bit off. (`camera + 320` is Camera::mState_uFlags; the asm clears
        // bit 1 through a 64-bit load/store pair -- reproduced as the named 32-bit field it is.)
        if (mbStartOfGame)
        {
            lrCameraInOut.mState_uFlags &= ~2;
        }
    }

    // ------------------------------------------------------------------------
    // UpdateAttractMode -- the shared ATTRACT_MODE body (the X360's LABEL_45, reached from
    // both case 5's success arm and case 6). De-inlined to one named helper so the two entry
    // points do not duplicate it.
    //
    //     mArbStateAttractMode.Update(info);                       // vtable +8
    //     lrCameraInOut = mArbStateAttractMode.GetCamera();        // arb +16816 == state +0x10
    //     UpdateCameraCycleControl(info.mfTimestep, held, cycleOut, changedOut);
    //     if ( !mbDoAttractMode ) {
    //         mArbStateAttractMode.Release(info);                  // vtable +0xC
    //         meState = E_STATE_NORMAL;
    //         <the "Jump_Effect" stop-hook poke -- gated, see case 5>
    //     }
    // ------------------------------------------------------------------------
    void Arbitrator::UpdateAttractMode(Camera::Camera& lrCameraInOut,
                                       ArbStateSharedInfo& lrSharedInfo,
                                       bool lbCycleCamera, bool lbCycleCameraHeld)
    {
        mArbStateAttractMode.Update(lrSharedInfo);
        lrCameraInOut = mArbStateAttractMode.GetCamera();

        bool lbChangedThisFrame = false;
        UpdateCameraCycleControl(lrSharedInfo.mfTimestep, lbCycleCameraHeld,
                                 lbCycleCamera, lbChangedThisFrame);

        if (!mbDoAttractMode)
        {
            mArbStateAttractMode.Release(lrSharedInfo);
            meState = E_STATE_NORMAL;
            // ⚠️ GATE: the stop-hook poke
            //     lrCameraInOut.GetEffects().mStopHookNameString.Set("Jump_Effect");
            //     lrCameraInOut.GetEffects().mbHasStopHookNameString = true;
            //   -- held back for the same reason as case 5's start-hook (see there).
        }
    }


    // ------------------------------------------------------------------------
    // Destruct / UpdateDebugCameras -- DECLARATION-ONLY (no X360 asm in this TU's function set;
    // the dossier carries DWARF variable hints only). Their bodies land when their asm is
    // attested (or with the debug-camera / behaviour-tweaker TUs).
    // IsDebugCamera / GetDebugFlyWorldTransform remain declaration-only.
    // CycleNormalCamera / DebugCameraFlyWorldLookAt / GetNormalCamera are bodied below
    // (class:BrnDirector::Arbitrator, batch 12).
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // CycleNormalCamera @0x822087F0 -- request a camera cycle on whichever state is
    // driving the "normal" camera: the testbed state directly when the debug-camera
    // selector reads 2 (the X360 stb at arb +0x1A1 == mArbStateTestbed's +0x171
    // byte), else the container's current state (the inlined GetCurrentState carries
    // the StateContainer.h:131 "mpCurrentState != NULL" tripwire -- the committed
    // out-of-line GetCurrentState fires the same assert).
    // ------------------------------------------------------------------------
    void Arbitrator::CycleNormalCamera()
    {
        if (miDebugCameraMode == 2)
        {
            mArbStateTestbed.RequestCycleCameraThisFrame();
        }
        else
        {
            mStateContainer.GetCurrentState()->RequestCycleCameraThisFrame();
        }
    }

    // ------------------------------------------------------------------------
    // GetNormalCamera @0x821F5BD8 -- the camera currently driving the "normal" view:
    // the testbed state's camera (arb +0x40 == mArbStateTestbed's +0x10 Camera) when
    // the debug-camera selector reads 2, else the current state's camera (state +0x10;
    // the inlined GetCurrentState carries the StateContainer.h:140 tripwire).
    // ------------------------------------------------------------------------
    const Camera::Camera& Arbitrator::GetNormalCamera() const
    {
        if (miDebugCameraMode == 2)
        {
            return mArbStateTestbed.GetCamera();
        }
        return mStateContainer.GetCurrentState()->GetCamera();
    }

    // ------------------------------------------------------------------------
    // DebugCameraFlyWorldLookAt @0x82234738 -- point the debug fly-world camera at
    // lLookAt from lEye (the GameTalk "CameraPos" landing spot). The X360 asserts the
    // fly-world handle is allocated (the BrnBehaviourManager.h:589 "IsAllocated()"
    // tripwire), re-resolves the behaviour slot through GetBehaviourSlotFromHandle
    // (== the handle's own cached mpBehaviour -- Prepare defines the cache as exactly
    // that slot's pointee; the committed GetBehaviour() accessor is the named
    // equivalent), and forwards both vectors (VMX registers v1/v2 saved across the
    // resolve) to BehaviourDebugFlyWorld::WarpToLookAt.
    // ------------------------------------------------------------------------
    void Arbitrator::DebugCameraFlyWorldLookAt(rw::math::vpu::Vector3 lEye,
                                               rw::math::vpu::Vector3 lLookAt)
    {
        CGS_ASSERT(mDebugCameraFlyWorld.IsAllocated(), "IsAllocated()");   // BrnBehaviourManager.h:589 (non-gating)
        mDebugCameraFlyWorld.GetBehaviour()->WarpToLookAt(lEye, lLookAt);
    }
}
