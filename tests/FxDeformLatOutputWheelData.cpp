// Harness for run_fxdeformlat_output_wheel_data.py (crash parity G18-D1 + G18-D2, FX-DEFORM-LAT).
//
// The shipped DeformableObject::OutputWheelData (BrnDeformableObject_GlassState.cpp) and the
// file's anonymous-namespace constants are pasted in through methods.inc and run on zero-filled
// storage of the REAL DeformableObject / VehiclePhysics / StreamedDeformationSpec /
// DeformationOutputInterfaceForEntityModules types; the real WheelPhysicalStates::operator= and
// CgsStrStream are linked. Two fixtures stand in for bodies outside the function under test:
// SimpleVehiclePhysics::GetWheelsWorldTransfrom returns a known per-wheel transform, and
// DetachedWheelManager::GetWheel returns "no record".
//
// ARTIST OutputWheelData @0x82608E28, live arm 0x82608F04..0x82608F60 (r28 = &maWheels[w],
// entry = block + 0x60*w):
//   0x82608F38 lvx128 v13 = wheel+0xA0 (mBodyPointVelocity)       -> 0x82608F50 entry+0x40
//   0x82608F30/34 lvx128 wheel+0x30 ; vspltw 0 ; 0x82608F48 vmulfp128 v0 = row0 * splat
//                                                                 -> 0x82608F54 entry+0x50
//   0x82608F58/5C stb 1 exists, stb 1 attached ; 0x82608F60 b loc_82609160 (the loop step):
//   the live arm never names v127, the running entity-sphere size (G18-D2).
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

// Fixture: wheel w's world transform. Row 0 is distinct per wheel so the spin product is
// observable lane by lane (w lane included -- the console's vmulfp128 multiplies all four); the
// translation puts wheel w (w+1)*1.6 m from the car so a |wheel - car| fold would be visible.
static Matrix44Affine FixtureWheelTransform(int w)
{
    Matrix44Affine m;
    m.xAxis = Vector3{ 0.5f + w, -0.25f - w, 2.0f + w, 0.125f * (w + 1) };
    m.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    m.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    m.wAxis = Vector3{ 100.0f + 1.6f * (w + 1), 7.0f, -3.0f, 1.0f };
    return m;
}
namespace BrnPhysics { namespace Vehicle {
Matrix44Affine SimpleVehiclePhysics::GetWheelsWorldTransfrom(EVehicleDrivenWheel leWheel, bool) const
{
    return FixtureWheelTransform(static_cast<int>(leWheel));
}
} }
namespace BrnPhysics { namespace Deformation {
const PhysicalWheel* DetachedWheelManager::GetWheel(EntityId, s32) const { return nullptr; }
} }

#include "methods.inc"

alignas(16) static unsigned char gObjectStorage[sizeof(DeformableObject)];
alignas(16) static unsigned char gVehicleStorage[sizeof(Vehicle::VehiclePhysics)];
alignas(16) static unsigned char gSpecStorage[sizeof(StreamedDeformationSpec)];
alignas(16) static unsigned char gOutStorage[sizeof(DeformationOutputInterfaceForEntityModules)];
alignas(16) static unsigned char gWheelMgrStorage[sizeof(DetachedWheelManager)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liIndex = -1)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (%d)\n", lpcName, liIndex); }
}
static bool Same(const Vector3& a, const Vector3& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

int main()
{
    DeformableObject&        lrObject  = *reinterpret_cast<DeformableObject*>(gObjectStorage);
    Vehicle::VehiclePhysics& lrVehicle = *reinterpret_cast<Vehicle::VehiclePhysics*>(gVehicleStorage);
    StreamedDeformationSpec& lrSpec    = *reinterpret_cast<StreamedDeformationSpec*>(gSpecStorage);
    DeformationOutputInterfaceForEntityModules& lrOut =
        *reinterpret_cast<DeformationOutputInterfaceForEntityModules*>(gOutStorage);
    DetachedWheelManager& lrWheelMgr = *reinterpret_cast<DetachedWheelManager*>(gWheelMgrStorage);

    lrObject.mpDeformationSpec = &lrSpec;
    lrObject.mVehicleBody.mpAttachedVehicle = &lrVehicle;
    lrSpec.maWheelSpecs[0].liTagPointIndex = 5;                  // spec+0x70 != -1: steer flag off
    lrObject.mLastLinearVelocityPlusEntityRadius.w = 0.5f;       // the |halfExtent| seed

    Matrix44Affine lCar;
    lCar.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lCar.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lCar.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lCar.wAxis = Vector3{ 100.0f, 7.0f, -3.0f, 1.0f };
    lrVehicle.mTransform = lCar;
    for (int w = 0; w < 4; ++w)
    {
        lrVehicle.maWheels[w].mu8State = (w == 3) ? 2 : 0;       // wheel 3 detached, no record
        lrVehicle.maWheels[w].mBodyPointVelocity    = Vector3{ 1.0f + w, 2.0f + w, 3.0f + w, 4.0f + w };
        lrVehicle.maWheels[w].mIntegrationVariables = Vector4{ 5.0f + w, 99.0f, 98.0f, 97.0f };
    }

    lrObject.OutputWheelData(0, &lrOut, &lrWheelMgr);

    Check(lrOut.GetNumEntries() == 1u, "one entry published");
    const WheelPhysicalStates& lrStates =
        *reinterpret_cast<const WheelPhysicalStates*>(lrOut.GetWheelStateSlot(0));
    for (int w = 0; w < 3; ++w)
    {
        const WheelPhysicalStates::WheelPhysicalState& lrE = lrStates.maStates[w];
        const Matrix44Affine lT = FixtureWheelTransform(w);
        const f32 lfSpin = 5.0f + w;
        Check(Same(lrE.mWorldSpaceTransform.xAxis, lT.xAxis) && Same(lrE.mWorldSpaceTransform.wAxis, lT.wAxis),
              "live transform = GetWheelsWorldTransfrom", w);
        Check(Same(lrE.mWorldSpaceVelocity, Vector3{ 1.0f + w, 2.0f + w, 3.0f + w, 4.0f + w }),
              "entry+0x40 = wheel mBodyPointVelocity (0x82608F38/0x82608F50)", w);
        Check(Same(lrE.mWorldSpaceAngularVelocity,
                   Vector3{ lT.xAxis.x * lfSpin, lT.xAxis.y * lfSpin, lT.xAxis.z * lfSpin, lT.xAxis.w * lfSpin }),
              "entry+0x50 = row0 * splat(mIntegrationVariables.x) (0x82608F48/0x82608F54)", w);
        Check(lrStates.mabWheelExists[w] && lrStates.mabWheelAttached[w], "live: exists = attached = 1", w);
    }
    Check(!lrStates.mabWheelExists[3] && !lrStates.mabWheelAttached[3], "detached, no record: flags 0", 3);
    Check(lrObject.mLastLinearVelocityPlusEntityRadius.w == 0.5f,
          "live wheels fold nothing into the entity sphere size (live arm never names v127)");
    Check(giAsserts == 0, "valid fixture fires no tripwire");

    std::printf("FxDeformLatOutputWheelData: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
