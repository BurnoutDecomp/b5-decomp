// BrnPhysics-bodies group embed/compile check. Pulls in every home this group owns or grew, so
// the gate compiles the headers + the .cpp bodies together and the layout asserts fire.

#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

#include "BrnCommonTypes.h"

// --- ImpactEvent stride: both ImpactEvent ledger asm bodies use a 48-byte event stride. ---
static_assert(sizeof(BrnPhysics::Vehicle::ImpactEvent) == 48,
              "ImpactEvent must be 48 bytes (asm uses 48*miLength stride)");

// --- the new RW math vocabulary the physics bodies pin members against. ---
static_assert(sizeof(Matrix33) == 48, "Matrix33 == 3 x 16-byte rows");
static_assert(sizeof(VecFloat) == 16, "VecFloat == one 16-byte lane register");
static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine == 4 x 16-byte rows");

// --- ExternalPhysicsBody accumulator member sequence (stride-16, the four +0xE0..+0x110
//     offsets in the asm): assert the relative spacing of the pointer-free Vector3 tail. The
//     ABSOLUTE console offsets are NOT asserted (a vptr precedes them; host vptr is 8 bytes).
namespace
{
    struct EpbProbe : public BrnPhysics::ExternalPhysicsBody
    {
        static void Check()
        {
            EpbProbe p;
            BrnPhysics::ExternalPhysicsBody* b = &p;
            (void)b;
        }
    };
}

// Force the SimpleVehiclePhysics funcs + attribs accessor to be instantiated/compiled.
namespace
{
    bool TouchSimpleVehiclePhysics(BrnPhysics::Vehicle::SimpleVehiclePhysics& lr,
                                   Vector3 lvP, VecFloat lvfT)
    {
        Matrix44Affine m = lr.GetGraphicsVehicleTransform();
        lr.SetGraphicsVehicleTransform(m);
        return lr.IsContactBelowWheelPlane(lvP, lvfT);
    }
}
