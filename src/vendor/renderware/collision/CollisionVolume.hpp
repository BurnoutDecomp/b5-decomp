#pragma once

#include "types.hpp"
#include <cstring>   // memcpy

// ===========================================================================
// rw::collision primitive volume builders -- minimal slice.
//
// The X360 BrnWorld::TriggerEntityModule::ProcessAddTriggerEvents (0x822D8F48) builds a collision
// primitive into a 128-byte stack image, then hands that image to the scene manager
// (InSceneUpdateInterface::AddDynamicVolume block-copies the 128 bytes). The two builders it calls
// (both X360-inlined) are rw::collision::SphereVolume::Initialize (sphere from a radius) and
// rw::collision::BoxVolume::Initialize (box from half-extents).
//
// FLAG (minimal slice): the full RenderWare collision-volume class hierarchy
// (rw::collision::Volume + its primitive subclasses) is NOT reconstructed here. This slice homes
// ONLY the two Initialize entry points the trigger module needs. Each volume IS the 128-byte image
// (the size the producer block-copies); Initialize() stamps the volume type + parameters into the
// image and returns non-zero on success (the X360 uses the return as the lpVolume non-null guard).
// The interior field offsets below are a faithful placeholder, not the real RW volume layout;
// promote to the full Volume hierarchy when that TU lands.
// ===========================================================================

namespace rw
{
namespace collision
{
    enum EVolumeType
    {
        E_VOLUMETYPE_NULL   = 0,
        E_VOLUMETYPE_SPHERE = 1,
        E_VOLUMETYPE_BOX    = 2,
    };

    // 128-byte collision-volume image (the block AddDynamicVolume memcpy's).
    struct Volume
    {
        u32 muVolumeType;          // +0x00  EVolumeType
        f32 mafParams[3];          // +0x04  radius / half-extents
        u8  maPad[128 - 16];       // +0x10  remainder of the 128-byte image (opaque)
    };

    struct SphereVolume : public Volume
    {
        // FLAG: returns non-zero on success (X360 uses the result as the lpVolume non-null guard).
        bool Initialize(f32 lfRadius)
        {
            muVolumeType = E_VOLUMETYPE_SPHERE;
            mafParams[0] = lfRadius;
            mafParams[1] = 0.0f;
            mafParams[2] = 0.0f;
            return lfRadius > 0.0f;
        }
    };

    struct BoxVolume : public Volume
    {
        // FLAG: returns non-zero on success.
        bool Initialize(f32 lfHalfX, f32 lfHalfY, f32 lfHalfZ)
        {
            muVolumeType = E_VOLUMETYPE_BOX;
            mafParams[0] = lfHalfX;
            mafParams[1] = lfHalfY;
            mafParams[2] = lfHalfZ;
            return (lfHalfX > 0.0f) && (lfHalfY > 0.0f) && (lfHalfZ > 0.0f);
        }
    };
}
}
