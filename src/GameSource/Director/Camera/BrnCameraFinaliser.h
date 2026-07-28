#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H

#include "types.hpp"
#include "GameSource/Director/Shots/ShotControllers/BrnInertiaController.h"  // BrnDirector::InertiaController (mInertiaController @+0)

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
// FLAG: BrnDirector::KeyAnimShakeController has no reconstructed home (it is NOT the
//   committed BrnDirector::KeyAnimController in Shots/ShotControllers -- different type,
//   different Update signature), so it is modelled here as correctly-SIZED opaque storage
//   carrying its recovered name and offset -- the BrnDirectorModuleIO.h house style. Grow it
//   into its real type additively when its own TU lands.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace DirectorIO { struct InputBuffer; }
    namespace Camera     { struct Camera; }
    class DirectorResourceManager;

    struct alignas(16) CameraFinaliser
    {
        // X360 @0x82250440 (caller: MainDirector::Update, its only caller). Apply the frame's
        // camera inertia + shake. lrCameraInOut is finalised in place.
        void Update(const DirectorIO::InputBuffer* lpInputBuffer,
                    void*                          lpCameraStateBlock,
                    const DirectorResourceManager* lpResourceManager,
                    Camera::Camera*                lpCameraInOut);

        // +0x00: the camera-lag slerp. Its Update @0x8221ECD0 is REAL (BrnInertiaController.cpp).
        InertiaController mInertiaController;

        // +0x50: the key-anim shake controller. FLAG: no homed type -- sized opaque storage.
        u8 mKeyAnimShakeController[0x7F0 - 0x50];

        // +0x7F0: the shake/lag scale accumulator. Update zeroes it when the camera raises the
        // 0x40 state bit, ramps it toward 1.0 while the camera-state block raises its 0x10 bit,
        // and decays it by the resource manager's per-frame decay each tick.
        // FLAG: name inferred from its role (the trimmed DWARF does not name it).
        f32 mfShakeScale;
    };
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_FINALISER_H
