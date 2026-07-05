#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H

// BrnDirector::Camera::Utils::CameraSphericalRotationController -- the free-look
// spherical (yaw + pitch) rotation controller driven by a look stick.
//
// Provenance: class shape / member names / method set verbatim from the DecFIGS DWARF
// (GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.{h,cpp}).
// This TU bodies Update @0x8223F808; Construct and the Get*/Is* query family are their
// own ledger functions (declaration-only here -- the per-TU `cl /c` gate does not link,
// so a declaration is all a not-yet-done callee needs).
//
// Member byte offsets pinned by the Update asm (base ptr r31 = this):
//   +0x00 mStickVector (Vector2, 16-byte SIMD slot; `stvx128 v1,r0,r31`)
//   +0x10 mfYawDegs         (0x10)     +0x14 mfYawVelocity   (0x14)
//   +0x18 mfYawReturnRate   (0x18)     +0x1C mfUnRotatedTime (0x1C)
//   +0x20 mbIsLookback      (0x20)     +0x21 mbWasLookbackLastFrame (0x21)
//   +0x22 mbIsRotated       (0x22)     +0x24 mPitchMover (SmoothMover, `addi r3,r31,0x24`)

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector2 alias
#include "GameSource/Director/Camera/Utils/BrnCameraSmoothMover.h"  // SmoothMover mPitchMover

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // DWARF BrnCameraSphericalRotationController.h:57.
    struct CameraSphericalRotationController
    {
        // DWARF h:61/:73-:103 -- declaration-only (their own ledger functions).
        void Construct();
        bool     IsRotated() const;
        Vector2  GetAdjustedStickVector() const;
        Vector2  GetRawStickVector() const;
        f32      GetYawRotationAngleDegs() const;
        f32      GetYawRotationAngleRads() const;
        f32      GetPitchRotationAngleDegs() const;
        f32      GetPitchRotationAngleRads() const;
        f32      GetUnRotatedTime() const;
        bool     IsLookback() const;
        bool     IsStartingLookbackThisFrame() const;
        bool     IsEndingLookbackThisFrame() const;

        // @0x8223F808 (this TU, DWARF h:70) -- advance one frame: stash the look stick,
        // update lookback edge flags, drive yaw (free-spring when live, integrated when
        // paused) and pitch (via mPitchMover), then refresh the rotated / un-rotated-time
        // tracking.
        void Update(f32 lfTimestep, Vector2 lStick, bool lbLookback, bool lbPaused,
                    f32 lfMinPitch, f32 lfMaxPitch);

    private:
        Vector2     mStickVector;             // h:111  +0x00
        f32         mfYawDegs;                 // h:112  +0x10
        f32         mfYawVelocity;             // h:113  +0x14
        f32         mfYawReturnRate;           // h:114  +0x18
        f32         mfUnRotatedTime;           // h:115  +0x1C
        bool        mbIsLookback;             // h:116  +0x20
        bool        mbWasLookbackLastFrame;   // h:117  +0x21
        bool        mbIsRotated;              // h:118  +0x22
        SmoothMover mPitchMover;              // h:120  +0x24
    };
}
}
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H
