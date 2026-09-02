#pragma once

#include "types.hpp"

// Deformation-system tunables / fixed array bounds. Reconstructed at the mirrored
// DWARF home (GameSource/Physics/DeformationManager/BrnDeformationConstants.h). Only
// the bounds proven needed by the current TU set are populated here; GROW this header
// (do not fork it) as further deformation TUs are reconstructed. Values are taken from
// the DWARF (BrnDeformationConstants.h:41-69) which records the compile-time literals.

namespace BrnPhysics
{
namespace Deformation
{
    // BrnDeformationConstants.h:41
    const s32 KI_MAX_NUM_RIGID_BODIES_PER_DEFORMABLE_OBJECT  = 1;
    // BrnDeformationConstants.h:42 — array bound the TagPoint sensor-index asserts test against.
    const s32 KI_MAX_NUM_DEFORMABLE_SENSORS_PER_DEFORMABLE_OBJECT = 20;
    // BrnDeformationConstants.h:43
    const s32 KI_MAX_NUM_WHEEL_POINTS_PER_DEFORMABLE_OBJECT  = 4;
    // BrnDeformationConstants.h:44
    const s32 KI_MAX_NUM_OF_COLLISION_SPHERES                = 24;
    // BrnDeformationConstants.h:45
    const s32 KI_MAX_NUM_TAG_POINTS_PER_DEFORMABLE_OBJECT    = 128;
    // BrnDeformationConstants.h:46
    const s32 KI_MAX_NUM_DRIVEN_POINTS_PER_DEFORMABLE_OBJECT = 128;
    // BrnDeformationConstants.h:47
    const s32 KI_MAX_NUM_IK_PARTS_PER_DEFORMABLE_OBJECT      = 50;
    // BrnDeformationConstants.h:48
    const s32 KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT = 25;
    // BrnDeformationConstants.h:57 -- the per-vehicle detached-part record bound. X360:
    // TrafficEntityModule::ProcessDeformationData @0x8271EA98 `cmplwi cr6, r29, 0x14` under
    // the assert "luSlot < BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE".
    const u32 KU_MAX_DETACHED_PARTS_PER_VEHICLE              = 20;
}
}
