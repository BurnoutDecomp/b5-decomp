#ifndef GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_PERLIN_SHAKE_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_PERLIN_SHAKE_CONTROLLER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Matrix44Affine (rw::math::vpu) aliases

// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.h
//
// BrnDirector::PerlinShakeController -- the Perlin-noise camera shake (DWARF home
// BrnPerlinShakeController.h:33). A stateless math controller: it samples three 1-D
// Perlin-noise channels (roll/pitch/yaw, phase-offset by 0/50/100 so they decorrelate),
// scales them by the camera's live shake amplitude and the per-axis scales, and rotates
// the target by the resulting ZXY Euler angles.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PerlinShakeController::Update @0x8221E798  (this TU; called by
//     BrnDirector::KeyAnimShakeController::Update)
// GetMatrix (DWARF h:66) is a separate ledger function/TU and is intentionally not
// declared here yet (X360-gated growth).
//
// The DWARF declares Update as a member (h:44), but the ASM proves it takes NO `this`:
// r3 is the Camera* itself (the shake amplitude/frequency loads and the transform
// stores are all r3-relative), f1..f7 are the seven float args -- exactly the declared
// parameter list with no instance register left over, and the DWARF struct carries no
// data members for a `this` to reach. Per AGENTS.md the asm arbitrates the calling
// convention, so Update is reconstructed as `static` (same precedent as
// CgsGui::ParticleSystem2d::GetRGBA).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { struct Camera; }

    struct PerlinShakeController
    {
        // @0x8221E798 (DWARF h:44). When the camera's requested shake amplitude is
        // positive, rotate the camera transform by three decorrelated Perlin-noise Euler
        // angles scaled by (amplitude x per-axis scale), with the noise phase driven by
        // (shake frequency x lfTime x per-axis frequency scale).
        static void Update(Camera::Camera* lpCamera, f32 lfTime,
                           f32 lfRollScale, f32 lfPitchScale, f32 lfYawScale,
                           f32 lfRollFreqScale, f32 lfPitchFreqScale, f32 lfYawFreqScale);
    };
}

#endif // GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_PERLIN_SHAKE_CONTROLLER_H
