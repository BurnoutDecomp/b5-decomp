#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H

#include "types.hpp"
#include "GameSource/Director/Shots/ShotControllers/BrnInertiaController.h"  // BrnDirector::InertiaController (mInertiaController @+0)
#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.h" // BrnDirector::KeyAnimShakeController (mKeyAnimShakeController @+0x50)

// ============================================================================
// GameSource/Director/Camera/BrnCameraFinaliser.h
//
// BrnDirector::CameraFinaliser -- the LAST thing that touches the director camera before it
// is published. MainDirector::Update @0x82274070 calls it once per frame, after the
// arbitrator has chosen the frame's camera and before ValidateTransformWithDebugInfo /
// CopyToCgsCamera / SetCameraOutput:
//
//     CameraFinaliser::Update( this + 74880,      // the finaliser        (MainDirector +0x12480)
//                              lpIO->mpInputBuffer,
//                              this + 210912,     // a camera-state block (MainDirector +0x337E0)
//                              lpIO->mpResourceManager,
//                              &lCamera );        // the frame camera, IN-OUT
//
// It does three things: apply the per-frame camera INERTIA (slerp the finalised transform
// back toward last frame's actual one by the camera's requested lag), drive the shake-scale
// accumulator, and apply the key-anim shake.
//
// LAYOUT. Recovered from CameraFinaliser::Update @0x82250440, which addresses exactly three
// regions of `this`:
//     this + 0      -> InertiaController::Update(this, lpCamera, timestep)
//     this + 0x50   -> KeyAnimShakeController::Update(this + 80, timer, lpCamera, timestep)
//     this + 0x7F0  -> a f32 accumulator (zeroed / clamped / decayed each frame)
// The first two pin themselves: BrnDirector::InertiaController is a Matrix44Affine (0x40) +
// s32 (0x44), which rounds to exactly 0x50 under its 16-byte alignment -- i.e. the inertia
// controller ends EXACTLY where the key-anim shake controller begins. Parity is BY NAMED
// MEMBER (the x64 gate); the offsets above are provenance.
//
// -- RETIRED 2026-09-06 (camera-shake lane). The FLAG that stood here said
//   "BrnDirector::KeyAnimShakeController has no reconstructed home ... modelled as
//   correctly-SIZED opaque storage". It HAS a home now
//   (Shots/ShotControllers/BrnKeyAnimShakeController.h, bodied from Construct @0x8223D240 and
//   Update @0x8223D488), and the opaque span is replaced by the real member at the same
//   offset and the same 0x7A0 size. That mattered: the opaque span meant step 4 could never
//   run, and step 4 is the ONLY reader of Camera::mEffects' shake request in the game.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace DirectorIO { struct InputBuffer; }
    namespace Camera     { struct Camera; }
    class DirectorResourceManager;
    struct GameState;

    struct alignas(16) CameraFinaliser
    {
        // X360 @0x82250440 (caller: MainDirector::Update, its only caller). Apply the frame's
        // camera inertia + shake. lrCameraInOut is finalised in place.
        // ⭐ THE SECOND ARGUMENT IS TYPED NOW (2026-09-06, camera-shake lane). It used to be
        // `void* lpCameraStateBlock` and the banner called it "a camera-state block"; the
        // X360 reads `lwz r11, 0xE4(r27)` off it and MainDirector's only call site passes
        // `&maGameState`, and GameState::miThisFramesActionFlags is at console +0xE4 (the
        // member run mpEventJLBox/0x00, mTrafficLightSpace/0x10, mBaseDriveThruTransform/0x50,
        // mDriveThruTransform/0x90, the six bools 0xD0..0xDD, meTakedownVictimID/0xE0 lands it
        // exactly there, and ArbStateRoaming.cpp:684 independently reads bit 0x10 of that same
        // member as "smash action this frame"). Named, so step 2 below never pokes an offset.
        void Update(const DirectorIO::InputBuffer* lpInputBuffer,
                    GameState*                     lpGameState,
                    const DirectorResourceManager* lpResourceManager,
                    Camera::Camera*                lpCameraInOut);

        // Seed the finaliser. INLINED on the console into MainDirector::Construct
        // @0x8225B448, where the three statements are visible as
        //     stw   r31(=0), 0x40(r9)                 -> mInertiaController.Construct()
        //     stfs  0.0,     0x7F0(r9)                -> mfShakeScale = 0
        //     addi  r3, r9, 0x50 ; bl KeyAnimShakeController::Construct   (r4 == the manager)
        // with r9 == this. De-inlined to the one named call so MainDirector never forms
        // `this + 0x50` itself.
        void Construct(const DirectorResourceManager* lpResourceManager);

        // +0x00: the camera-lag slerp. Its Update @0x8221ECD0 is REAL (BrnInertiaController.cpp).
        InertiaController mInertiaController;

        // +0x50: the key-anim shake controller -- the consumer of the camera's shake request.
        // Its own sizeof is 0x7A0 (see its header), i.e. exactly 0x7F0 - 0x50, so the member
        // that follows still lands where the asm puts it.
        KeyAnimShakeController mKeyAnimShakeController;

        // +0x7F0: the shake/lag scale accumulator. Update zeroes it when the camera raises the
        // 0x40 state bit, ramps it toward 1.0 while the camera-state block raises its 0x10 bit,
        // and decays it by the resource manager's per-frame decay each tick.
        // FLAG: name inferred from its role (the trimmed DWARF does not name it).
        f32 mfShakeScale;
    };
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H
