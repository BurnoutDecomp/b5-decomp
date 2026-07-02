#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu)

// BrnDirector::InertiaController - the camera-finaliser inertia smoother: it slerps
// the finalised camera back toward last frame's actual transform by the camera's
// requested lag amount (CameraEffects::mfCameraLag). DWARF home
// BrnInertiaController.h:41. Update is bodied in BrnInertiaController.cpp (this TU);
// Construct is its own ledger function (declaration-only here).
namespace BrnDirector
{
    namespace Camera { struct Camera; }

    struct InertiaController
    {
        // DWARF h:45 / cpp:37 -- declaration-only (its own ledger function).
        void Construct();

        // @0x8221ECD0 (this TU, DWARF h:50 / cpp:54) -- apply the per-frame camera
        // inertia. Called by BrnDirector::CameraFinaliser::Update.
        void Update(Camera::Camera* lpCamera, f32 lfTimeStep);

    private:
        rw::math::vpu::Matrix44Affine mPreviousActualXform;   // +0x00 (h:54)
        s32                           miFrame;                 // +0x40 (h:55)
    };
}
