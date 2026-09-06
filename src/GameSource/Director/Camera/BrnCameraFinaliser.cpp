// ============================================================================
// GameSource/Director/Camera/BrnCameraFinaliser.cpp
//
// BrnDirector::CameraFinaliser::Update @0x82250440 -- the per-frame camera finalise pass
// MainDirector::Update runs immediately before publishing the camera.
// ============================================================================

#include "GameSource/Director/Camera/BrnCameraFinaliser.h"

#include <cstring>   // [diag] std::memcpy (the cameradefaults record read)

#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"   // DirectorIO::InputBuffer (GetTimerStatusInterface)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatus (GetCurrentTimeStep)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"  // BrnDirector::GameState (miThisFramesActionFlags)
#include "GameSource/Director/BrnDirectorResourceManager.h"            // GetShakeAnimGroup / GetCameraDefaults
#include "GameSource/Director/Camera/Camera.h"                         // Camera::Camera / CameraEffects
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"          // Attrib::Gen::shotgroup
#include "GameSource/AttribSys/Generated/classes/cameradefaults.h"     // Attrib::Gen::cameradefaults
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // [diag] CgsDev::Log::gpDebugPrint
#include <cstdlib>                                                     // [diag] getenv

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // Update @0x82250440
    //
    // The X360 body, statement for statement:
    //
    //   1. lpTimer  = lpInputBuffer->GetTimerStatusInterface();
    //      timestep = lpTimer[+8] * lpTimer[+4];        // multiplier * base
    //      InertiaController::Update( this, lpCameraInOut, timestep );
    //
    //   2. if ( lpCameraInOut[+324] & 0x40 )  mfShakeScale = 0.0f;
    //      if ( lpCameraStateBlock[+228] & 0x10 )
    //          mfShakeScale = fsel( mfShakeScale - 1.0f, mfShakeScale, 1.0f );   // clamp UP to 1
    //
    //   3. lpBank = *(lpResourceManager + 1500);
    //      f32 lfWanted = lpBank[+44] * mfShakeScale;
    //      if ( lfWanted > lpCameraInOut[+276] )
    //      {
    //          if ( !lpCameraInOut[+284] )  lpCameraInOut[+284] = lpBank[+40];
    //          lpCameraInOut[+276] = lfWanted;
    //      }
    //      mfShakeScale -= lpBank[+52] * mfShakeScale;    // per-frame decay
    //
    //   4. KeyAnimShakeController::Update( this + 80, lpTimer, lpCameraInOut, timestep );
    //
    // STEP 1 IS RECONSTRUCTED (below). It is the one this wave needs and the one that is
    // fully resolvable: `lpTimer[+8] * lpTimer[+4]` is literally CgsSystem::TimerStatus's
    // mfTimeStepMultiplier * mfBaseTimeStep, i.e. its committed inline GetCurrentTimeStep()
    // (CgsTimerStatusInterface.h -- miFrameCount@+0, mfBaseTimeStep@+4,
    // mfTimeStepMultiplier@+8), and BrnDirector::InertiaController::Update @0x8221ECD0 is
    // REAL (BrnInertiaController.cpp).
    //
    // ⭐⭐ STEPS 2-4 ARE LIVE (2026-09-06, camera-shake lane). THE GATE THAT STOOD HERE WAS
    // THE CAMERA-SHAKE BUG. It read "those are CONSOLE byte offsets INSIDE the committed
    // Camera's mEffects sub-block ... there is no named accessor for these three yet", and
    // every clause of it had expired:
    //   * camera +276 / +284 / +324 are mEffects.mfShakeAmplitude (effects +0xAC),
    //     mEffects.mu8ShakeType (+0xB4) and CameraState::mCurrentFlags -- all three NAMED in
    //     BrnCameraEffects.h / Camera.h since the effects carve, and reached here by name;
    //   * `*(lpResourceManager + 1500)` is not "a shot/shake parameter bank inside
    //     maPaddingAfterICEWrapper": 1500 == 0x5DC == mCameraDefaults (+0x5D8) + 4, i.e.
    //     Attrib::Instance::mpAttributeData of the cameradefaults record -- the SAME instance
    //     BoostShakeController::Update already reads through GetLayoutPointer(), and the same
    //     +4-on-console / +8-on-x64 slot that makes the accessor mandatory;
    //   * BrnDirector::KeyAnimShakeController is homed now
    //     (Shots/ShotControllers/BrnKeyAnimShakeController.{h,cpp}).
    //
    // WHY IT MATTERED, and why "this degrades polish, it does not break the frame" was wrong:
    // step 4 is the ONLY consumer of Camera::mEffects' shake request in the entire image
    // (PerlinShakeController::Update is its callee and nothing else calls that either). While
    // it was gated, EVERY shake in the game -- the boost shake, ArbStateRoaming's rival-impact
    // / traffic-check / landed-stunt shakes, the crash and takedown states' requests, and this
    // step's own base shake -- was written into the effects block and never read. The camera
    // never moved.
    //
    // The four steps, with the asm that decides each:
    //   1  0x82250480  InertiaController::Update(camera, timestep)
    //   2  0x82250484  a camera CUT (state flag 6) zeroes the shake scale;
    //      0x822504B8  a SMASH action this frame (GameState::miThisFramesActionFlags & 0x10)
    //                  clamps it UP to 1 (`fsel f0, shakeScale - 1, shakeScale, 1`)
    //   3  0x822504E0  the cameradefaults BASE shake: amplitude = data[+0x2C] * shakeScale,
    //                  published only when it EXCEEDS the live request, and taking the shake
    //                  TYPE from data[+0x28] only when the camera has not already named one;
    //      0x82250514  then the scale decays by data[+0x34] * scale every frame
    //   4  0x8225054C  KeyAnimShakeController::Update(timestep, camera)
    //
    // ------------------------------------------------------------------------
    namespace
    {
        // ---- the cameradefaults ATTRIBUTE-DATA fields step 3 reads -----------------------
        // Console byte offsets ON PURPOSE: they index the SERIALISED AttribSys record the
        // vault ships (a 0x38-byte data area), not a host struct -- the same rule and the same
        // idiom as BrnBoostShakeController.cpp's four curve fields at +0x18/+0x1C/+0x20/+0x24
        // in this very record.
        const u32 KU_BASE_SHAKE_TYPE_OFFSET      = 0x28;  // lwz  0x28(data) -> stb into mu8ShakeType
        const u32 KU_BASE_SHAKE_AMPLITUDE_OFFSET = 0x2C;  // lfs  0x2C(data)
        const u32 KU_SHAKE_SCALE_DECAY_OFFSET    = 0x34;  // lfs  0x34(data)

        // The CameraState flag whose set state means "this camera CUT this frame" -- BitArray
        // index 6, the same one InertiaController::Update reads to restart its history
        // (`ld r11, 0x140(camera)` / `rlwinm r11,r11,0,25,25` == the 0x40 mask).
        const u32 KU_CAMERA_FLAG_CUT_THIS_FRAME = 6;

        // GameState::miThisFramesActionFlags bit 4 -- "a SMASH stunt element completed this
        // frame". MainDirector::ProcessInputQueue's game-action-58 arm raises it for stunt
        // element type 1, and ArbStateRoaming.cpp:684 reads the same bit for the
        // "Smash_Effect" camera effect. It is what makes a smashed gate shake the camera.
        const s32 KI_ACTION_FLAG_SMASH = 0x10;

        inline f32 ReadFloat(const u8* lpData, u32 luByteOffset)
        {
            f32 lfValue = 0.0f;
            std::memcpy(&lfValue, lpData + luByteOffset, sizeof(f32));
            return lfValue;
        }

        inline s32 ReadInt(const u8* lpData, u32 luByteOffset)
        {
            s32 liValue = 0;
            std::memcpy(&liValue, lpData + luByteOffset, sizeof(s32));
            return liValue;
        }

        // [DIAG BRN_CAMERA_TRACE] [FLAG PC witness] -- NOT IN THE X360 BINARY.
        //
        // WHY IT EXISTS. The console's shake chain is producer -> Camera::mEffects
        // (mfShakeAmplitude / mu8ShakeType) -> CameraFinaliser steps 2-4 ->
        // KeyAnimShakeController -> the authored SHAKE-ANIM SHOT the type indexes. On this
        // build steps 2-4 are gated, so nothing has ever read the shake request and nothing
        // has ever looked at the authored data behind it. These two one-shot dumps answer, in
        // ONE boot and before any behaviour change:
        //   (a) does the shake-anim shot group resolve at all, how many shots does it have,
        //       and is each shot an `iceanim` take or a `proceduralshake` record?
        //   (b) what base shake does the cameradefaults record carry (type / amplitude /
        //       per-frame decay -- the three fields step 3 reads at data +0x28/+0x2C/+0x34)?
        // Off unless BRN_CAMERA_TRACE is set. DELETE-WHEN: the camera-shake bring-up closes.
        void BrnDiag_DumpShakeData(const DirectorResourceManager* lpResourceManager)
        {
            static const bool sbOn = (getenv("BRN_CAMERA_TRACE") != 0);
            static bool sbDone  = false;
            static s32  siCalls = 0;
            if (!sbOn || sbDone || CgsDev::Log::gpDebugPrint == 0 || lpResourceManager == 0)
                return;

            const Attrib::Gen::shotgroup& lrGroup = lpResourceManager->GetShakeAnimGroup();

            // The director resource manager Prepares its 65 collections over several frames,
            // so the FIRST finalise call is too early to read them. Wait for the group to
            // resolve; report the failure anyway after ~20 s so a never-resolving group is a
            // measurement rather than a silence.
            ++siCalls;
            if (!lrGroup.IsValid() && siCalls < 1200)
                return;
            sbDone = true;
            const u32 luNumShots = lrGroup.IsValid() ? lrGroup.Num_ShotList() : 0u;
            *CgsDev::Log::gpDebugPrint
                << "[cam-shotlist] shakeAnimGroup valid=" << (lrGroup.IsValid() ? 1 : 0)
                << " numShots=" << luNumShots << "\n";

            for (u32 luShot = 0; luShot < luNumShots && luShot < 32u; ++luShot)
            {
                const Attrib::RefSpec* lpRef = lrGroup.ShotList(luShot);
                const u64 luClassKey = (lpRef != 0) ? lpRef->GetClassKey() : 0ull;
                *CgsDev::Log::gpDebugPrint
                    << "[cam-shotlist]  [" << luShot << "] classKeyHi="
                    << static_cast<u32>(luClassKey >> 32)
                    << " classKeyLo=" << static_cast<u32>(luClassKey & 0xFFFFFFFFull) << "\n";
            }

            const u8* const lpDefaults =
                static_cast<const u8*>(lpResourceManager->GetCameraDefaults().GetLayoutPointer());
            if (lpDefaults == 0)
            {
                *CgsDev::Log::gpDebugPrint << "[cam-defaults] cameradefaults layout NULL\n";
                return;
            }
            f32 lfBaseAmplitude = 0.0f;
            f32 lfDecay         = 0.0f;
            s32 liBaseType      = 0;
            std::memcpy(&liBaseType,      lpDefaults + 0x28, sizeof(s32));
            std::memcpy(&lfBaseAmplitude, lpDefaults + 0x2C, sizeof(f32));
            std::memcpy(&lfDecay,         lpDefaults + 0x34, sizeof(f32));
            *CgsDev::Log::gpDebugPrint
                << "[cam-defaults] baseShakeType=" << liBaseType
                << " baseShakeAmplitude=" << lfBaseAmplitude
                << " shakeScaleDecay=" << lfDecay << "\n";
        }

        // [DIAG BRN_CAMERA_TRACE] the SMASH edge -- GameState::miThisFramesActionFlags bit
        // 0x10, which MainDirector::ProcessInputQueue's game-action-58 arm raises when a
        // stunt element of type 1 (SMASH) completes. It is the one shake PRODUCER that is
        // demonstrably live on this build (the gate-UI wave measured the whole
        // OnPropHit -> action 58 ladder firing on the junkyard-exit route), so it is the
        // event the shake test anchors on.
        void BrnDiag_ReportSmash(const GameState* lpGameState)
        {
            static const bool sbOn = (getenv("BRN_CAMERA_TRACE") != 0);
            if (!sbOn || CgsDev::Log::gpDebugPrint == 0 || lpGameState == 0)
                return;
            if ((lpGameState->miThisFramesActionFlags & 0x10) == 0)
                return;
            static s32 siCount = 0;
            if (siCount >= 64)
                return;
            ++siCount;
            *CgsDev::Log::gpDebugPrint
                << "[cam] smash n=" << siCount
                << " actionFlags=" << lpGameState->miThisFramesActionFlags << "\n";
        }
    }

    void CameraFinaliser::Update(const DirectorIO::InputBuffer* lpInputBuffer,
                                 GameState*                     lpGameState,
                                 const DirectorResourceManager* lpResourceManager,
                                 Camera::Camera*                lpCameraInOut)
    {
        // The committed InputBuffer types this accessor `const void*` (its interface homes are
        // not reconstructed) and its own header directs consumers to reinterpret the returned
        // address at the type they know. The X360 reads +4 and +8 off it and multiplies them,
        // which is CgsSystem::TimerStatus::GetCurrentTimeStep() inlined.
        const CgsSystem::TimerStatus* lpTimerStatus =
            reinterpret_cast<const CgsSystem::TimerStatus*>(
                lpInputBuffer->GetTimerStatusInterface());

        const f32 lfTimeStep = lpTimerStatus->GetCurrentTimeStep();

        // Step 1 -- the camera-lag slerp (this+0 IS the inertia controller).
        mInertiaController.Update(lpCameraInOut, lfTimeStep);

        // [diag] BRN_CAMERA_TRACE -- see the two helpers above.
        BrnDiag_DumpShakeData(lpResourceManager);
        BrnDiag_ReportSmash(lpGameState);

        // ---- Step 2 @0x82250484 / @0x822504B8 -- the shake SCALE accumulator --------------
        if (lpCameraInOut->GetState().IsFlagSet(KU_CAMERA_FLAG_CUT_THIS_FRAME))
        {
            mfShakeScale = 0.0f;
        }
        if (lpGameState != 0 && (lpGameState->miThisFramesActionFlags & KI_ACTION_FLAG_SMASH) != 0)
        {
            // `fsel f0, shakeScale - 1.0, shakeScale, 1.0` -- clamp UP to 1, never down.
            if (mfShakeScale < 1.0f)
                mfShakeScale = 1.0f;
        }

        // ---- Step 3 @0x822504E0 -- the cameradefaults BASE shake, then the decay ----------
        // `lwz r11, 4(r10)` with r10 == manager + 0x5D8 is the cameradefaults instance's
        // mpAttributeData. BY ACCESSOR: that slot is +0x04 on console and +0x08 on x64.
        const u8* const lpCameraDefaultsData =
            (lpResourceManager != 0)
                ? static_cast<const u8*>(lpResourceManager->GetCameraDefaults().GetLayoutPointer())
                : 0;
        if (lpCameraDefaultsData != 0)
        {
            Camera::CameraEffects& lrEffects = lpCameraInOut->GetEffects();

            const f32 lfWantedAmplitude =
                ReadFloat(lpCameraDefaultsData, KU_BASE_SHAKE_AMPLITUDE_OFFSET) * mfShakeScale;

            if (lfWantedAmplitude > lrEffects.mfShakeAmplitude)
            {
                // The base shake only NAMES a shake type when the camera has not already
                // asked for one (`lbz r9, 0x11C(r30)` / `bne` over the store).
                if (lrEffects.mu8ShakeType == 0)
                {
                    lrEffects.mu8ShakeType = static_cast<u8>(
                        ReadInt(lpCameraDefaultsData, KU_BASE_SHAKE_TYPE_OFFSET));
                }
                lrEffects.mfShakeAmplitude = lfWantedAmplitude;
            }

            // `fneg f12, f0` + `fmadds f0, f13, f12, f0` == scale -= decay * scale.
            mfShakeScale -= ReadFloat(lpCameraDefaultsData, KU_SHAKE_SCALE_DECAY_OFFSET)
                            * mfShakeScale;
        }

        // ---- Step 4 @0x8225054C -- APPLY the request to the camera ------------------------
        // The console re-reads the timer status and recomputes the timestep here
        // (0x82250530..0x82250548). It is the same object and the same product as step 1's, so
        // the value is reused rather than the two calls transcribed.
        mKeyAnimShakeController.Update(lfTimeStep, lpCameraInOut);
    }

    // ------------------------------------------------------------------------
    // Construct -- INLINED on the console into MainDirector::Construct @0x8225B448
    // (0x8225B7E0 `stw r31, 0x40(r9)` / 0x8225B7E4 `stfs f0, 0x7F0(r9)` / 0x8225B7E8
    // `bl KeyAnimShakeController::Construct` with r3 == r9 + 0x50 and r4 == the resource
    // manager, r9 == this). De-inlined to the three named calls so MainDirector never forms
    // `this + 0x50` itself.
    // ------------------------------------------------------------------------
    void CameraFinaliser::Construct(const DirectorResourceManager* lpResourceManager)
    {
        mInertiaController.Construct();
        mfShakeScale = 0.0f;
        mKeyAnimShakeController.Construct(lpResourceManager);
    }
}
