#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (unhandled-state assert)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"        // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"             // Camera effect-hook free functions
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h" // BehaviourAftertouchCrash

// ============================================================================
// BrnDirector::ArbStateCrashMode -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x8225AC98
//   GetName    @0x821F62F0
//   DoCloseup  @0x82208EF8
//   Release    @0x82235A48
//   Update     @0x82235488
//
// The crash-mode director state: it drives the aftertouch-crash camera behaviour during the
// post-crash slow-motion, the intro flash / borders / motion-blur ramp, a slow camera roll
// oscillation, and the periodic slow-mo "close-up" punctuations. All member access is BY NAME;
// the camera-effect pokes go through named Camera setters, the produced-camera copy and the
// roll/close-up writes go through BehaviourAftertouchCrash accessors, and the sibling-state
// hand-offs go through the ArbitratorStateContainer. The GameState snapshot it reacts to
// (lrSharedInfo.mpGameState) is read by named members; a handful of trailing-sub-object reads
// land in opaque DirectorProfileData / ShowTimeInfo blobs (FLAGged inline).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // ---- shared .rdata literals the X360 loads (IEEE-754 patterns the asm references) ----
        const f32 KF_UNIT          = 1.0f;          // flt_82001C98
        const f32 KF_ZERO          = 0.0f;          // flt_82001CC0
        const f32 KF_POSTFX_SCALE  = 1.0f;          // flt_82CDA5E0 (borders post-FX scale)
        const f32 KF_HIT_COUNT_CAP = 5.0f;          // flt_8200426C (vehicle-hit-count blur cap)

        // The motion-blur "dead-zone" the ACTIVE path tests mfCurrentBlur against before pushing
        // a blur request. The asm bounds are ASYMMETRIC (read off the instructions, not the
        // Hex-Rays +/-eps simplification): the blur is treated as "settled" only while
        // KF_BLUR_DEADZONE_LO <= mfCurrentBlur <= KF_BLUR_DEADZONE_HI.
        const f32 KF_BLUR_DEADZONE_HI = 3.3333334e-5f;  // flt_82001770 (asm: blur > this -> not settled)
        const f32 KF_BLUR_DEADZONE_LO = 1.1920929e-7f;  // flt_82002514 (asm: blur >= this -> maybe settled)

        // The two time-dilation factors a close-up requests (DoCloseup).
        const f32 KF_CLOSEUP_SUPER_SLOMO_TIME_SCALE = 0.033333335f; // flt_82001778 (1/30, super slow-mo)
        const f32 KF_CLOSEUP_SLOMO_TIME_SCALE       = 0.2857143f;   // flt_8200177C (2/7, normal close-up)

        // ---- crash-mode tuning constants (DWARF BrnArbStateCrashMode.cpp:22..43) -------------
        // FLAG (values): these are the named tuning floats the X360 loads from the .rdata pool at
        // 0x82CDA4A4..0x82CDA4DC (+ 0x82FAA5E0/E4 for the intro flash/borders). That .rdata is not
        // in the exports, so the VALUES below are inferred placeholders -- only the
        // symbol->member->KF mapping (read off the load offsets) is asm-attested. Pin the real
        // values when the 0x82CDA4xx / 0x82FAA5Ex .rdata is dumped. The mapping:
        //   flt_82FAA5E0 -> KF_INTRO_FLASH_DURATION     flt_82FAA5E4 -> KF_INTRO_BORDERS_DURATION
        //   flt_82CDA4A4 -> KF_INTRO_BLUR_IN_DURATION   flt_82CDA4A8 -> KF_INTRO_BLUR_OUT_DURATION
        //   flt_82CDA4AC -> KF_INTRO_BLUR_RAMP_SPEED    flt_82CDA4B0 -> KF_INTRO_BLUR_MAXIMUM
        //   flt_82CDA4BC -> KF_EXTRA_SPIN_BLUR_DURATION (KF_EXTRA_SPIN_BLUR_MAXIMUM == 1.0, shared)
        //   flt_82CDA4B4 -> KF_DEFAULT_BLUR_DURATION    flt_82CDA4C0 -> KF_DEFAULT_BLUR_RAMP_SPEED
        //   flt_82CDA4B8 -> KF_BACKGROUND_BLUR_MAXIMUM
        //   flt_82CDA4C8 -> KF_CAMERA_MAX_TILT_ANGLE    flt_82CDA4CC -> KF_CAMERA_TILT_LERP_SPEED
        //   flt_82CDA4D0 -> KF_CAMERA_TILT_CHANGE_TIME
        //   flt_82CDA4D4 -> KF_MIN_TIME_BETWEEN_CLOSEUPS
        //   flt_82CDA4D8 -> KF_CLOSEUP_DURATION         flt_82CDA4DC -> KF_CLOSEUP_BLEND_IN_DURATION
        const f32 KF_INTRO_FLASH_DURATION     = 0.5f;
        const f32 KF_INTRO_BORDERS_DURATION   = 0.5f;
        const f32 KF_INTRO_BLUR_IN_DURATION   = 0.25f;
        const f32 KF_INTRO_BLUR_OUT_DURATION  = 0.25f;
        const f32 KF_INTRO_BLUR_RAMP_SPEED    = 4.0f;
        const f32 KF_INTRO_BLUR_MAXIMUM       = 1.0f;
        const f32 KF_EXTRA_SPIN_BLUR_DURATION = 0.5f;
        const f32 KF_EXTRA_SPIN_BLUR_MAXIMUM  = 1.0f;     // flt_82001C98 (shared 1.0)
        const f32 KF_DEFAULT_BLUR_DURATION    = 0.5f;
        const f32 KF_DEFAULT_BLUR_RAMP_SPEED  = 2.0f;
        const f32 KF_BACKGROUND_BLUR_MAXIMUM  = 0.5f;
        const f32 KF_CAMERA_MAX_TILT_ANGLE    = 0.1f;
        const f32 KF_CAMERA_TILT_LERP_SPEED   = 0.05f;
        const f32 KF_CAMERA_TILT_CHANGE_TIME  = 1.0f;
        const f32 KF_MIN_TIME_BETWEEN_CLOSEUPS = 1.0f;
        const f32 KF_CLOSEUP_DURATION          = 1.0f;
        const f32 KF_CLOSEUP_BLEND_IN_DURATION = 0.25f;

        // The motion-blur baseline mfCurrentBlur resets to when both ramp timers expire (the
        // separate .rdata word flt_82CDA4C4; ~0). FLAG: value undumped (placeholder 0.0).
        const f32 KF_BLUR_BASELINE = 0.0f;                // flt_82CDA4C4

        // X360 dirty-flag bits the ACTIVE path OR-pokes into the camera state's flag word.
        const s32 KU_CAMERA_DIRTY_TRANSFORM = 0x2;    // ACTIVE: every frame
        const s32 KU_CAMERA_DIRTY_EXIT      = 0x400;  // every Update exit

        // The motion-blur "shake/strobe" type id the blur-shake request carries (stb 8).
        const u8 KU8_BLUR_SHAKE_TYPE = 8;

        // Clamp a value to >= 0.0f, returning 0.0f for negatives (the X360 fsel idiom
        // `fsel(value, value, 0.0)`).
        inline f32 ClampToNonNegative(f32 lfValue)
        {
            return (lfValue >= 0.0f) ? lfValue : 0.0f;
        }
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225AC98 -- build the camera and seed the per-state flags / timers / tuning
    // defaults. (The aftertouch handle starts unallocated; meState starts INACTIVE.)
    // ------------------------------------------------------------------------
    void ArbStateCrashMode::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        mfFlashTime   = 0.0f;              // +0x198
        mfBordersTime = 0.0f;              // +0x19C
        mfBlurInTime  = 0.0f;              // +0x1A0
        mfBlurOutTime = 0.0f;              // +0x1A4
        mfCurrentBlur = 0.0f;              // +0x1A8

        meState = E_STATE_INACTIVE;        // +0x194 = 0

        mAftertouch = BehaviourHandle<Camera::BehaviourAftertouchCrash>();  // +0x180 block zeroed

        mbLinearBlurRamp = true;           // +0x1B4 = 1
        mbBlurShake      = false;          // +0x1B5 = 0

        mfBlurRampSpeed  = KF_DEFAULT_BLUR_RAMP_SPEED;  // +0x1B0
        mfMaximumBlur    = KF_UNIT;        // +0x1AC = 1.0
        mfTiltAngle      = 0.0f;           // +0x1B8

        mfTiltChangeTime  = KF_CAMERA_TILT_CHANGE_TIME; // +0x1C0
        mfTargetTiltAngle = KF_CAMERA_MAX_TILT_ANGLE;   // +0x1BC
        mfCloseupTime     = 0.0f;          // +0x1C4

        mfTimeSinceCloseup = KF_MIN_TIME_BETWEEN_CLOSEUPS; // +0x1C8 (so the first close-up may fire)
        mbDoingCloseup     = false;        // +0x1CC = 0
        mbAllowCloseup     = true;         // +0x1CD = 1
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F62F0
    // ------------------------------------------------------------------------
    const char* ArbStateCrashMode::GetName() const
    {
        return "ArbStateCrashMode";
    }

    // ------------------------------------------------------------------------
    // DoCloseup @0x82208EF8 -- drive the active slow-mo close-up: ramp the behaviour's close-up
    // blend toward 1.0 over KF_CLOSEUP_BLEND_IN_DURATION, then request the close-up's time
    // dilation (deeper for the super-slow-mo variant).
    // ------------------------------------------------------------------------
    void ArbStateCrashMode::DoCloseup(ArbStateSharedInfo& lrSharedInfo)
    {
        // closeupAmount = clamp(mfCloseupTime / KF_CLOSEUP_BLEND_IN_DURATION, 0, 1)
        const f32 lfRawBlend     = mfCloseupTime / KF_CLOSEUP_BLEND_IN_DURATION;
        const f32 lfClampedLow   = ClampToNonNegative(lfRawBlend);
        const f32 lfCloseupBlend = (lfClampedLow <= KF_UNIT) ? lfClampedLow : KF_UNIT;

        // X360 re-resolves + asserts IsAllocated() inside the behaviour-pool lookup.
        Camera::BehaviourAftertouchCrash& lrBehaviour = *mAftertouch.GetBehaviour();
        lrBehaviour.SetCloseupAmount0To1(lfCloseupBlend);

        // Request the close-up time dilation (the camera's slow-mo factor).
        const f32 lfTimeScale = mbSuperSloMoCloseUp ? KF_CLOSEUP_SUPER_SLOMO_TIME_SCALE
                                                    : KF_CLOSEUP_SLOMO_TIME_SCALE;
        GetNonConstCamera().SetRequestedTimeDilation(lfTimeScale);

        // Mark the camera transform dirty for this frame.
        GetNonConstCamera().mState_uFlags |= KU_CAMERA_DIRTY_TRANSFORM;

        (void)lrSharedInfo;   // the X360 takes lrSharedInfo but DoCloseup reads nothing from it
    }

    // ------------------------------------------------------------------------
    // Release @0x82235A48 -- leave crash mode: drop to INACTIVE, release the aftertouch behaviour
    // back to the manager, and assert no behaviours remain allocated by this state.
    // ------------------------------------------------------------------------
    bool ArbStateCrashMode::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;        // +0x194 = 0

        if (mAftertouch.mbAllocated)       // +0x180 block
        {
            mAftertouch.mpManager->UnSetBehaviourUsedByHandle(mAftertouch.muAllocationKey);
            mAftertouch.muHelperIndex = 0;
            mAftertouch.mpManager     = 0;
            mAftertouch.mpBehaviour   = 0;
            mAftertouch.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }

    // ------------------------------------------------------------------------
    // Update @0x82235488 -- the crash-mode per-frame tick. Dispatches on meState:
    //   INACTIVE            : nothing (just mark the camera exit-dirty)
    //   PREPARING           : try to enter ACTIVE (Prepare); on success seed the intro blur/flash
    //                         block and fall into the ACTIVE body
    //   ACTIVE              : drive the aftertouch camera, tilt oscillation, flash/borders/blur
    //                         ramp, close-ups, then hand off to post-event when the event ends
    //   CHANGING_TO_ROAMING : copy the produced camera and hand off to the roaming state
    // ------------------------------------------------------------------------
    void ArbStateCrashMode::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;   // fall through to the shared exit (mark camera exit-dirty)

        case E_STATE_PREPARING:
            if (!Prepare(lrSharedInfo))   // X360: vtable slot 1
            {
                break;   // still preparing -> shared exit
            }
            // Entered ACTIVE: seed the intro flash / borders / blur block, then run the ACTIVE
            // body this same frame (the X360 falls case-1 through into case-2).
            mfBlurInTime  = KF_INTRO_BLUR_IN_DURATION;   // +0x1A0
            mfMaximumBlur = KF_INTRO_BLUR_MAXIMUM;       // +0x1AC
            mbLinearBlurRamp = false;                    // +0x1B4 = 0
            mbBlurShake      = false;                    // +0x1B5 = 0
            meState = E_STATE_ACTIVE;                    // +0x194 = 2
            mfFlashTime    = KF_INTRO_FLASH_DURATION;    // +0x198
            mfBordersTime  = KF_INTRO_BORDERS_DURATION;  // +0x19C
            mfBlurOutTime  = KF_INTRO_BLUR_OUT_DURATION; // +0x1A4
            mfBlurRampSpeed = KF_INTRO_BLUR_RAMP_SPEED;  // +0x1B0
            TickActive(lrSharedInfo);
            break;

        case E_STATE_ACTIVE:
            TickActive(lrSharedInfo);
            break;

        case E_STATE_CHANGING_TO_ROAMING:
        {
            // Copy the produced camera, then hand the frame to the roaming state.
            GetNonConstCamera() = mAftertouch.GetBehaviour()->GetCamera();
            ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;
            if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_ROAMING)->Prepare(lrSharedInfo))
            {
                lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
                Release(lrSharedInfo);   // X360: vtable slot 3
            }
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        // Shared exit (X360 LABEL_49): mark the camera exit-dirty for every path.
        GetNonConstCamera().mState_uFlags |= KU_CAMERA_DIRTY_EXIT;
    }

    // ------------------------------------------------------------------------
    // TickActive -- the ACTIVE-state per-frame body (X360 inlined into Update's case 2).
    // ------------------------------------------------------------------------
    void ArbStateCrashMode::TickActive(ArbStateSharedInfo& lrSharedInfo)
    {
            GameState&             lrGameState = *lrSharedInfo.mpGameState;
            const EffectInterface& lrEffects   = *lrSharedInfo.mpEffectInterface;
            Camera::Camera&        lrCamera    = GetNonConstCamera();
            Camera::BehaviourAftertouchCrash& lrBehaviour = *mAftertouch.GetBehaviour();

            // Copy the aftertouch behaviour's produced camera into ours, then mark dirty.
            lrCamera = lrBehaviour.GetCamera();
            lrCamera.mState_uFlags |= KU_CAMERA_DIRTY_TRANSFORM;

            // ---- camera roll/tilt oscillation -------------------------------------------
            // Count down the tilt-change timer; when it crosses zero, flip the target sign and
            // re-arm it.
            mfTiltChangeTime -= lrSharedInfo.mfSimTimestep;
            if (mfTiltChangeTime < 0.0f)
            {
                mfTargetTiltAngle = -mfTargetTiltAngle;
                mfTiltChangeTime  = KF_CAMERA_TILT_CHANGE_TIME;
            }
            // Lerp the current roll toward the target and drive it into the behaviour.
            mfTiltAngle += (mfTargetTiltAngle - mfTiltAngle) * KF_CAMERA_TILT_LERP_SPEED;
            lrBehaviour.SetRollAngleRads(mfTiltAngle);

            // ---- intro flash / showtime hook --------------------------------------------
            if (mfFlashTime <= 0.0f)
            {
                // No flash left: keep the "Showtime" post-FX playing, blended by how little boost
                // the player has.
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Showtime",
                                              KF_UNIT - lrGameState.mfPlayerBoostPercentage);
            }
            else
            {
                mfFlashTime -= lrSharedInfo.mfSimTimestep;
                Camera::RequestStartEffectHook(lrCamera, "2dFlash", KF_UNIT);
            }

            // ---- periodic slow-mo close-up ----------------------------------------------
            // Trigger a new close-up once enough time has elapsed, close-ups are allowed, and the
            // game-state requests one this frame.
            // FLAG: gameState +0x1EA / +0x1EB land in the opaque DirectorProfileData blob (the
            // X360 reads two adjacent "request close-up" bytes there); the first doubles as the
            // super-slow-mo selector cached into mbSuperSloMoCloseUp.
            if (mfTimeSinceCloseup > KF_MIN_TIME_BETWEEN_CLOSEUPS && mbAllowCloseup)
            {
                const bool lbRequestSuperSloMo = lrGameState.mDirectorProfileData.maOpaque[0x02] != 0; // +0x1EA
                const bool lbRequestNormal     = lrGameState.mDirectorProfileData.maOpaque[0x03] != 0; // +0x1EB
                if (lbRequestSuperSloMo || lbRequestNormal)
                {
                    mfCloseupTime       = 0.0f;
                    mbSuperSloMoCloseUp = lbRequestSuperSloMo;
                    mbDoingCloseup      = true;
                }
            }

            if (mbDoingCloseup)
            {
                if (mfCloseupTime < KF_CLOSEUP_DURATION)
                {
                    mfCloseupTime += lrSharedInfo.mfTimestep;
                    DoCloseup(lrSharedInfo);
                }
                else
                {
                    mbDoingCloseup = false;
                }
            }
            else
            {
                // No close-up: clear the behaviour's close-up blend and accumulate the gap timer.
                lrBehaviour.SetCloseupAmount0To1(0.0f);
                mfTimeSinceCloseup += lrSharedInfo.mfTimestep;
            }

            // When NOT in a close-up, request the game-state's impact-time slow-mo factor.
            if (!mbDoingCloseup)
            {
                lrCamera.SetRequestedTimeDilation(lrGameState.mfImpactTimeSloMoFactor);
            }

            // ---- choose the blur "mode" for this frame ----------------------------------
            // FLAG: gameState +0x1E9 / +0x1EC land in the opaque DirectorProfileData blob (the
            // "extra spin blur" and "background/default blur" request bytes).
            const bool lbUseSpinBlur    = lrGameState.mDirectorProfileData.maOpaque[0x01] != 0; // +0x1E9
            const bool lbUseDefaultBlur = lrGameState.mDirectorProfileData.maOpaque[0x04] != 0; // +0x1EC

            if (lbUseSpinBlur)
            {
                mbBlurShake     = true;                       // +0x1B5 = 1
                mfBlurInTime    = KF_EXTRA_SPIN_BLUR_DURATION; // +0x1A0
                mfBlurOutTime   = KF_EXTRA_SPIN_BLUR_DURATION; // +0x1A4
                mfBlurRampSpeed = KF_DEFAULT_BLUR_RAMP_SPEED;  // +0x1B0
                mfMaximumBlur   = KF_EXTRA_SPIN_BLUR_MAXIMUM;  // +0x1AC = 1.0
                mbLinearBlurRamp = true;                      // +0x1B4 = 1
            }
            else if (lbUseDefaultBlur)
            {
                mbBlurShake     = false;                      // +0x1B5 = 0
                mfBlurInTime    = KF_DEFAULT_BLUR_DURATION;   // +0x1A0
                mfBlurOutTime   = KF_DEFAULT_BLUR_DURATION;   // +0x1A4
                mfBlurRampSpeed = KF_DEFAULT_BLUR_RAMP_SPEED; // +0x1B0
                mfMaximumBlur   = KF_BACKGROUND_BLUR_MAXIMUM; // +0x1AC
                mbLinearBlurRamp = true;                      // +0x1B4 = 1
            }

            // ---- ramp the motion blur in / out ------------------------------------------
            if (mfBlurInTime + mfBlurOutTime > 0.0f)
            {
                // The vehicle-hit count scales the blur fraction, clamped to [1, 5].
                // FLAG: gameState +0x1E0 lands in the opaque ShowTimeInfo blob (a vehicle-hit
                // count the X360 reads as a 4-byte int and converts to float).
                const s32 liVehicleHitCount =
                    *reinterpret_cast<const s32*>(&lrGameState.mShowTimeInfo.maOpaque[0x0C]); // +0x1E0
                const f32 lfHitCount = static_cast<f32>(liVehicleHitCount);
                const f32 lfHitFloor = (KF_UNIT - lfHitCount >= 0.0f) ? KF_UNIT : lfHitCount; // max(1, cnt)
                const f32 lfHitScale = (KF_HIT_COUNT_CAP - lfHitFloor >= 0.0f) ? lfHitFloor
                                                                              : KF_HIT_COUNT_CAP; // min(.,5)

                f32 lfBlurFraction;
                if (mfBlurInTime <= 0.0f)
                {
                    // asm Update@0x82235488: `ble cr6, loc_82235808` (mfBlurInTime <= 0) -> Ramp OUT:
                    // SHRINK mfCurrentBlur toward KF_BLUR_BASELINE (fnmsubs: current - rampSpeed*step),
                    // decrement mfBlurOutTime (stfs +0x1A4), clamp max(next, baseline).
                    const f32 lfRampRatio = mfBlurOutTime / KF_EXTRA_SPIN_BLUR_DURATION;
                    mfBlurOutTime -= lrSharedInfo.mfSimTimestep;
                    const f32 lfNextBlur = mfCurrentBlur - mfBlurRampSpeed * lrSharedInfo.mfSimTimestep;
                    const f32 lfRatioClamped = (-lfRampRatio >= 0.0f) ? 0.0f : lfRampRatio; // ratio<=0 -> 0
                    mfCurrentBlur = (lfNextBlur - KF_BLUR_BASELINE >= 0.0f) ? lfNextBlur : KF_BLUR_BASELINE; // max(next,baseline)
                    lfBlurFraction = (KF_UNIT - lfRatioClamped >= 0.0f) ? lfRatioClamped : KF_UNIT;          // min(.,1)
                }
                else
                {
                    // asm fall-through @0x822357C4 (mfBlurInTime > 0) -> Ramp IN: GROW mfCurrentBlur
                    // toward mfMaximumBlur (fmadds: current + rampSpeed*step), decrement mfBlurInTime
                    // (stfs +0x1A0), clamp min(next, mfMaximumBlur).
                    const f32 lfRampRatio = mfBlurInTime / KF_EXTRA_SPIN_BLUR_DURATION;
                    mfBlurInTime -= lrSharedInfo.mfSimTimestep;
                    const f32 lfNextBlur = mfCurrentBlur + mfBlurRampSpeed * lrSharedInfo.mfSimTimestep;
                    const f32 lfRatioClamped = (-lfRampRatio >= 0.0f) ? 0.0f : lfRampRatio; // ratio<=0 -> 0
                    mfCurrentBlur = (lfNextBlur - mfMaximumBlur >= 0.0f) ? mfMaximumBlur : lfNextBlur; // min(next,max)
                    lfBlurFraction = (KF_UNIT - lfRatioClamped >= 0.0f) ? lfRatioClamped : KF_UNIT;    // min(.,1)
                }

                const f32 lfShakeAmount = lfBlurFraction * lfHitScale;
                if (mbBlurShake)
                {
                    lrCamera.RequestMotionBlurShake(lfShakeAmount, KF_UNIT, KU8_BLUR_SHAKE_TYPE);
                }
            }
            else
            {
                // Both ramp timers expired: reset to the baseline and a linear ramp.
                mbLinearBlurRamp = true;             // +0x1B4 = 1
                mfCurrentBlur    = KF_BLUR_BASELINE;  // +0x1A8
            }

            // ---- push the current blur into the camera ----------------------------------
            // Only when mfCurrentBlur is OUTSIDE the (asymmetric) settled dead-zone.
            const bool lbBlurSettled =
                !(mfCurrentBlur > KF_BLUR_DEADZONE_HI) && (mfCurrentBlur >= KF_BLUR_DEADZONE_LO);
            if (!lbBlurSettled)
            {
                f32 lfBlur = mfCurrentBlur;
                if (!mbLinearBlurRamp)
                {
                    lfBlur = mfCurrentBlur * mfCurrentBlur;   // squared ramp
                }
                const f32 lfBlurNonNeg = ClampToNonNegative(lfBlur);
                const f32 lfBlurClamped = (KF_UNIT - lfBlurNonNeg >= 0.0f) ? lfBlurNonNeg : KF_UNIT; // min(.,1)
                lrCamera.RequestMotionBlur(lfBlurClamped, lfBlurClamped);
            }

            // ---- borders post-FX --------------------------------------------------------
            lrCamera.SetRequestedBorderPostFX((mfBordersTime > 0.0f) ? KF_POSTFX_SCALE : KF_ZERO);
            mfBordersTime = ClampToNonNegative(mfBordersTime - lrSharedInfo.mfSimTimestep);

            // ---- hand off to post-event when the event ends -----------------------------
            // Stay in crash mode while the event is ACTIVE and the game wants the post-intro
            // crash mode; otherwise stop the current effect and try to hand the frame to the
            // post-event state.
            const bool lbStayInCrashMode =
                lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_ACTIVE &&
                lrGameState.mbGoToCrashModeAfterIntro;   // +0x11C

            if (!lbStayInCrashMode)
            {
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;
                if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_POST_EVENT)->Prepare(lrSharedInfo))
                {
                    lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_POST_EVENT);
                    Release(lrSharedInfo);   // X360: vtable slot 3
                }
            }
    }
}
