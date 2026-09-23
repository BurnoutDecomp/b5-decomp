// Harness for run_fxdeformlat_output_state.py (crash parity G17-D1, FX-DEFORM-LAT).
//
// The shipped DeformableObject::OutputState (BrnDeformableObject_GlassState.cpp) and the file's
// anonymous-namespace constants are pasted in through methods.inc and run on zero-filled storage
// of the REAL DeformableObject / VehiclePhysics / StreamedDeformationSpec / CarState types.
//
// ARTIST OutputState @0x825C1EA8:
//   * wheel loop: 0x825C20F8 `lvx128 v127` = maTagPoints[idx].mPos; 0x825C2210 `lvx128 v0` =
//     Wheel::mPosition (vehicle + 0x130 + 0xE0*i + 0x80); 0x825C2214 `vrlimi128 v127,v0,4,0`
//     (IMM 4 == the Y lane); 0x825C2240 `stvx128 v127` -> CarState + 0x660 + 16*i.
//     => maWheelTagPoints[i] == (tag.x, wheel.y, tag.z, tag.w). PS3 0x6F4190 vperm<0,5,2,3>.
//   * bbox copy: 0x825C1FB4..0x825C1FD0 four 8-byte ld/std == 32 bytes from vehicle+0x6D0
//     (mDeformableAABB) to CarState+0x640 (mDeformedBBoxMin/Max).
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include <cstdio>
#include <cstring>
#include <new>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

#include "methods.inc"

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

alignas(16) static unsigned char gObjectStorage[sizeof(DeformableObject)];
alignas(16) static unsigned char gVehicleStorage[sizeof(Vehicle::VehiclePhysics)];
alignas(16) static unsigned char gSpecStorage[sizeof(StreamedDeformationSpec)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liIndex = -1)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (%d)\n", lpcName, liIndex); }
}
static bool Same(const Vector3& a, float x, float y, float z, float w)
{
    return a.x == x && a.y == y && a.z == z && a.w == w;
}

int main()
{
    DeformableObject&        lrObject  = *reinterpret_cast<DeformableObject*>(gObjectStorage);
    Vehicle::VehiclePhysics& lrVehicle = *reinterpret_cast<Vehicle::VehiclePhysics*>(gVehicleStorage);
    StreamedDeformationSpec& lrSpec    = *reinterpret_cast<StreamedDeformationSpec*>(gSpecStorage);

    lrObject.mpDeformationSpec = &lrSpec;
    lrObject.mVehicleBody.mpAttachedVehicle = &lrVehicle;
    lrSpec.mu8NumDeformationSensors = 0;             // the sensor walk is not under test

    // Wheel i uses tag point 3+2i (a non-identity map, so a wheel/tag index mix-up fails too).
    for (int i = 0; i < 4; ++i)
    {
        lrSpec.maWheelSpecs[i].liTagPointIndex = 3 + 2 * i;
        lrObject.maTagPoints[3 + 2 * i].SetPosition(Vector3{ 1.0f + i, 2.0f + i, 3.0f + i, 4.0f + i });
        lrVehicle.maWheels[i].mPosition = Vector3{ 10.0f + i, 20.0f + i, 30.0f + i, 40.0f + i };
    }
    lrVehicle.mDeformableAABB.mMin = Vector4{ -1.5f, -2.5f, -3.5f, 0.25f };
    lrVehicle.mDeformableAABB.mMax = Vector4{  1.5f,  2.5f,  3.5f, 0.75f };
    lrVehicle.mOriginalAABB.mMin   = Vector4{ -9.0f, -9.0f, -9.0f, -9.0f };
    lrVehicle.mOriginalAABB.mMax   = Vector4{  9.0f,  9.0f,  9.0f,  9.0f };

    CarState lState;
    std::memset(&lState, 0xCD, sizeof(lState));
    lrObject.OutputState(&lState);

    for (int i = 0; i < 4; ++i)
    {
        const Vector3& lrRow = lState.maWheelTagPoints[i];
        Check(lrRow.y == 20.0f + i, "axle row Y is the WHEEL's position y (vrlimi128 IMM 4 @0x825C2214)", i);
        Check(lrRow.w == 4.0f + i, "axle row W stays the TAG point's w", i);
        Check(lrRow.x == 1.0f + i && lrRow.z == 3.0f + i, "axle row X/Z are the local tag point", i);
    }
    Check(Same(lState.mDeformedBBoxMin, -1.5f, -2.5f, -3.5f, 0.25f), "mDeformedBBoxMin = vehicle mDeformableAABB.mMin");
    Check(Same(lState.mDeformedBBoxMax,  1.5f,  2.5f,  3.5f, 0.75f), "mDeformedBBoxMax = vehicle mDeformableAABB.mMax");
    Check(lState.mu8NumSensors == 0 && lState.mfSummedDisplacementSquared == 0.0f, "no sensors: count 0, sum 0");
    Check(giAsserts == 0, "valid fixture fires no tripwire");

    std::printf("FxDeformLatOutputState: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
