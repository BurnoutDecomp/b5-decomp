// ============================================================================
// GameSource/Director/Camera/BrnCameraFinaliser.cpp
//
// BrnDirector::CameraFinaliser::Update @0x82250440 -- the per-frame camera finalise pass
// MainDirector::Update runs immediately before publishing the camera.
// ============================================================================

#include "GameSource/Director/Camera/BrnCameraFinaliser.h"

#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"   // DirectorIO::InputBuffer (GetTimerStatusInterface)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatus (GetCurrentTimeStep)

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
    // ⚠️ STEPS 2-4 ARE A DOCUMENTED QUIET GATE, for three separate reasons:
    //   * steps 2 and 3 read Camera::Camera at +324 / +276 / +284 -- those are CONSOLE byte
    //     offsets INSIDE the committed Camera's mEffects sub-block, whose own header states
    //     its field layout "is nominal past +0x44" and that the named setters are the
    //     offset-authoritative API. There is no named accessor for these three yet, and
    //     poking a sibling class's effects block by console offset is exactly the pattern
    //     Camera.h's setter surface exists to prevent (and it would be WRONG anyway: the
    //     camera's pointer members widen on the x64 host, so +276/+284/+324 do not name the
    //     same fields here that they name on console -- the classic "field read at the wrong
    //     offset" bug class);
    //   * step 3's `*(lpResourceManager + 1500)` is a shot/shake parameter bank inside
    //     DirectorResourceManager's `maPaddingAfterICEWrapper` -- no named member;
    //   * step 4's BrnDirector::KeyAnimShakeController has no reconstructed home at all.
    //
    // CONSEQUENCE WHILE GATED: the camera still gets its INERTIA (the lag/smoothing that
    // stops the finalised camera snapping frame to frame), which is the finaliser's main
    // job; it does not get the key-anim shake overlay or the shake-amplitude ramp. A camera
    // is produced either way -- this degrades polish, it does not break the frame.
    //
    // DELETE-WHEN: delete this gate and transcribe steps 2-4 once (a) Camera/CameraEffects
    // exposes named accessors for the three fields above (this is the same "grow the mEffects
    // API" follow-up Camera.h already asks for), (b) DirectorResourceManager's +1500 shake
    // parameter bank is named, and (c) BrnDirector::KeyAnimShakeController is homed.
    // ------------------------------------------------------------------------
    void CameraFinaliser::Update(const DirectorIO::InputBuffer* lpInputBuffer,
                                 void*                          lpCameraStateBlock,
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

        // Steps 2-4: gated, see the banner above.
        (void)lpCameraStateBlock;
        (void)lpResourceManager;
    }
}
