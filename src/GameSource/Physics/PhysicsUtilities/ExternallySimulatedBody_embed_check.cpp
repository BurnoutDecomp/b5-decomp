// ExternallySimulatedBody embed/compile check. Pulls in the (grown) home + the RigidBody
// vendor type so the gate compiles the header + the ReadFromRenderware body together.
//
// Layout asserts are intentionally LIMITED: the committed ExternallySimulatedBody home is
// polymorphic on the host (virtual dtor -> 8-byte host vptr vs the X360 4-byte vptr / no-vptr
// DWARF layout), so the absolute member offsets are NOT reproduced on x64 and are not asserted
// (project rule: pin BY NAME across a vptr). We DO assert the math-vocabulary sizes the body
// relies on, and that the RigidBody vector-field run has the expected 16-byte stride.

#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"

static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine == 4 x 16-byte rows");
static_assert(sizeof(Vector3) == 16,        "Vector3 == one 16-byte lane register");

// The RigidBody pose/velocity fields are 16-byte SIMD registers in DWARF sequence; assert the
// reconstructed type carries the eleven-register run at 16-byte stride (11 * 16 == 176).
static_assert(sizeof(rw::physics::RigidBody) == 11 * 16,
              "RigidBody == eleven 16-byte SIMD register fields (DWARF rigidbody.h:419..459)");

namespace
{
    void TouchExternallySimulatedBody(BrnPhysics::ExternallySimulatedBody& lrBody,
                                      const rw::physics::RigidBody* lpRigidBody)
    {
        lrBody.ReadFromRenderware(lpRigidBody);
    }
}
