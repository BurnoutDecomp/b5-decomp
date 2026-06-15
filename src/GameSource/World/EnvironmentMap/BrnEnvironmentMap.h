#pragma once

#include "types.hpp"

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
}
