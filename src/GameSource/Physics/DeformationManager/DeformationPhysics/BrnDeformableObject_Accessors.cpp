// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Accessors.cpp
//
// BrnPhysics::Deformation::DeformableObject -- the five tiny vehicle-body
// accessors the contact fix-up family resolves BY NAME. All five are declared
// in the frozen BrnDeformableObject.h (DWARF :361/:365/:370/:387/:394) with no
// body anywhere in the tree; every reconstruction that needed the values so
// far read the console pointer chain (`*(model + 6476)` == the attached
// VehiclePhysics) inline. SLICE TU (home BrnDeformableObject.cpp, still
// unmounted); fold back when the home TU mounts.
//
// Byte grounding (X360): rig+6476 == mVehicleBody.GetVehiclePhysics() (the
// frozen header's own member note); the attached body's transform is at
// vehPhys+0x10 (VehiclePhysics.h pins mTransform there), the linear velocity
// at vehPhys+0x50 (base mLinearVelocity), the half extents at vehPhys+0x6A0
// (SimpleVehiclePhysics::mHalfExtent) -- exactly the rows the FixUp* asm
// reads (`lvx [r11+0x10..0x40]`, `lvx [r11+0x50]`, `lvx [r11+0x6A0]`).
// ============================================================================

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // VehiclePhysics (GetTransform/GetLinearVelocity/mHalfExtent)

namespace BrnPhysics
{
namespace Deformation
{
    // :361 / :365 -- the attached vehicle physics body, through the embedded VehicleRigidBody.
    Vehicle::VehiclePhysics* DeformableObject::GetVehiclePhysics()
    {
        return mVehicleBody.GetVehiclePhysics();
    }

    const Vehicle::VehiclePhysics* DeformableObject::GetVehiclePhysics() const
    {
        return mVehicleBody.GetVehiclePhysics();
    }

    // :370 -- the attached body's world transform (vehPhys+0x10), copied out.
    void DeformableObject::GetTransform(Matrix44Affine& lrOut) const
    {
        lrOut = mVehicleBody.GetVehiclePhysics()->GetTransform();
    }

    // :387 -- the attached body's world-space linear velocity (vehPhys+0x50), copied out.
    void DeformableObject::GetLinearVelocity(Vector3& lrOut) const
    {
        lrOut = mVehicleBody.GetVehiclePhysics()->GetLinearVelocity();
    }

    // :394 -- the attached body's body-space box half extents (vehPhys+0x6A0,
    // SimpleVehiclePhysics::mHalfExtent). Named in the shipped WithBoxes assert stream
    // ("lpTrafficModel->GetHalfExtents(): ", BrnDeformationManager.cpp:1580).
    Vector3 DeformableObject::GetHalfExtents()
    {
        return mVehicleBody.GetVehiclePhysics()->GetHalfExtent();
    }
}
}
