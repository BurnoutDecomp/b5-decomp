#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::Camera
#include "rw/math/vpu/types.h"                         // rw::math::vpu::Vector3

// Environment-map (reflection cube) support. Reconstructed from the DecFIGS DWARF
// (BrnEnvironmentMap.h). Only the cube-face enum is reconstructed so far - it is shared
// by the renderer's per-frame shader constants (env-map view/projection per face); the
// BrnGraphics::EnvironmentMap manager class is reconstructed when reached.
namespace BrnGraphics
{
    enum EEnvironmentMapFace
    {
        E_FACE_POSITIVE_X = 0,
        E_FACE_NEGATIVE_X = 1,
        E_FACE_POSITIVE_Y = 2,
        E_FACE_NEGATIVE_Y = 3,
        E_FACE_POSITIVE_Z = 4,
        E_FACE_NEGATIVE_Z = 5,
        E_FACE_NUM        = 6,
    };

    // ------------------------------------------------------------------------
    // BrnGraphics::EnvironmentMap -- the reflection-cube camera manager
    // (PROMOTED 2026-07-24 from the BrnEnvironmentMap.cpp-local declaration: the
    // WorldModule fleet embed needs the complete type; bodies stay in the .cpp).
    // X360: Construct @0x827B40D0; embedded in WorldModule @+6168096, in the
    // renderer module, and destructed by WorldModule::Destruct @0x827BD0F0.
    // ------------------------------------------------------------------------
    struct EnvironmentMap
    {
        void Construct();
        // ADDITIVE (WorldModule::Prepare @0x827D53B0 stage 13). Declaration-only.
        bool Prepare();
        bool Release();
        void Update(rw::math::vpu::Vector3 lCameraPosition);
        // Destruct: the X360 tears the cameras down; declared for the WorldModule
        // consumer, bodied in the .cpp.
        void Destruct();

        CgsGraphics::Camera maEnvMapCameras[E_FACE_NUM];
        rw::math::vpu::Vector3 mCameraPosition;
        f32 mfFOV;
        f32 mfAspectRatio;
    };
}
