// ============================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ConductorLeaves.cpp
//
// ⭐ 2026-08-09 (conductor wave). Two small per-frame leaves of the real
// PhysicsModule::Update @0x825B0640, bodied off the X360 image (home
// BrnVehicleManager.cpp is still unmounted -- the established slice pattern;
// fold back when the home mounts).
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // DWARF :264. X360-attested INLINE in PhysicsModule::Update @0x825B08A8..0x825B08E4:
    // four lvx/stvx pairs, the input buffer's camera block (+0/+16/+32/+48) into
    // mCameraMatrix (VehicleManager console +172240). A whole-object Matrix44Affine
    // assignment is those exact four stores.
    void VehicleManager::UpdateCameraMatrix(const Matrix44Affine* lpCameraMatrix)
    {
        mCameraMatrix = *lpCameraMatrix;
    }

    // DWARF :375, X360 @0x8284CB38. ⭐ EMPTY AS SHIPPED -- the retail body is a single
    // `blr` (image word 0x4E800020 at 0x8284CB38, read 2026-08-09), ICF-folded with
    // BaseCollisionGenerator::Destruct; the Update call site's `bl` therefore appears to
    // target that symbol with r3 == &mVehicleManager and f1 == the timestep. The PS3
    // DecFIGS build keeps the (equally empty) function out of line under its own name.
    // Reconstructed as the empty member it is -- NOT a gate, NOT a deferral: there is no
    // console behaviour to defer.
    void VehicleManager::ProcessWheelContacts(
        f32 /*lfTimeStep*/,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* /*lpContactInterface*/)
    {
        // Empty on purpose (as shipped).
    }
}
}
