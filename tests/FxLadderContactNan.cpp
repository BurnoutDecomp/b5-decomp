// Harness for run_fxladder_contact_nan.py (crash parity FX-LADDER): the two NaN-only arms of the
// race-car/race-car contact chain. The runner compiles the PRODUCTION text into fixup_piece.cpp
// (GetInterpolatedContactPointAndNormal, CalculateTangentPoints, the fix-up TU's helpers and the two
// DeformableObject accessors they call) and vac_piece.cpp (ValidateAndAddContact and its TU's helpers);
// this file runs them on zero-filled storage of the REAL DeformationManager / DeformableObject /
// VehiclePhysics / DeformationSensor types.
//
// ARTIST, GetInterpolatedContactPointAndNormal @0x82604948:
//   0x82604AE8  vcmpgtfp. v12, v0(0), v12(dot(nbr0C - C, other - C)) ; 0x82604AF8 beq 0x82604B44 -> neighbour 0
//   0x82604B2C  vcmpgtfp. v13, v0(0), v13(dot(nbr1C - C, other - C)) ; 0x82604B3C beq 0x82604B44 -> neighbour 1
//   `beq` is taken when the all-true bit is clear, i.e. when 0 > dot is FALSE -- a NaN dot ACCEPTS.
//   An accepted neighbour with NaN distances then fails `dThis > dNbr` (vcmpgtfp. 0x82604C04) and runs
//   the tangent arm (CalculateTangentPoints, return 1), whose result does not read the other position.
// ARTIST, ValidateAndAddContact @0x825E1788:
//   0x825E1B5C  vmaxfp128 v13, v127(0), v13(pen * recip(basis)) -- a NaN ratio stays NaN, so
//   0x825E1B78  vcmpgefp128. 1.0 >= t fails (no latch) and 0x825E1CB8 fails (the return is then
//   0.01 > pen, 0x825E1CF0, also false for a NaN pen).
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int giAsserts = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

alignas(16) static unsigned char gManager[sizeof(DeformationManager)];
alignas(16) static unsigned char gModel[sizeof(DeformableObject)];
alignas(16) static unsigned char gVehicle[sizeof(Vehicle::VehiclePhysics)];
alignas(16) static unsigned char gSensor[sizeof(DeformationSensor)];
static SensorSpec gaSpec[3];
static CgsGeometric::Sphere gaLocal[3];
static CgsGeometric::Sphere gLocalB;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}
static bool IsNan3(const Vector3& a) { return a.x != a.x || a.y != a.y || a.z != a.z; }
static bool Same3(const Vector3& a, const Vector3& b)
{
    return std::memcmp(&a.x, &b.x, 4) == 0 && std::memcmp(&a.y, &b.y, 4) == 0 && std::memcmp(&a.z, &b.z, 4) == 0;
}

static const f32 kNaN = std::numeric_limits<f32>::quiet_NaN();

// The car: a 90-degree yaw (local x -> world -z, local z -> world x) at (10, 2, 30), so every input and
// output crosses the local/world round trip. Three body sensors on the car's +x side: sensor 0 (the one
// in contact), its boundary neighbours 1 (toward +z) and 2 (toward -z). Everything below is given in
// LOCAL space and pushed through the transform.
static Matrix44Affine CarTransform()
{
    Matrix44Affine lT;
    lT.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
    lT.yAxis = Vector3{ 0.0f, 1.0f,  0.0f, 0.0f };
    lT.zAxis = Vector3{ 1.0f, 0.0f,  0.0f, 0.0f };
    lT.wAxis = Vector3{ 10.0f, 2.0f, 30.0f, 1.0f };
    return lT;
}
static Vector3 ToWorld(const Vector3& l)
{
    const Matrix44Affine lT = CarTransform();
    return Vector3{ lT.xAxis.x * l.x + lT.yAxis.x * l.y + lT.zAxis.x * l.z + lT.wAxis.x,
                    lT.xAxis.y * l.x + lT.yAxis.y * l.y + lT.zAxis.y * l.z + lT.wAxis.y,
                    lT.xAxis.z * l.x + lT.yAxis.z * l.y + lT.zAxis.z * l.z + lT.wAxis.z, 0.0f };
}

static DeformableObject& FreshModel(f32 lfNeighbour1CentreX)
{
    std::memset(gModel, 0, sizeof(gModel));
    std::memset(gVehicle, 0, sizeof(gVehicle));
    DeformableObject&        lrModel   = *reinterpret_cast<DeformableObject*>(gModel);
    Vehicle::VehiclePhysics& lrVehicle = *reinterpret_cast<Vehicle::VehiclePhysics*>(gVehicle);
    lrVehicle.mTransform = CarTransform();
    lrModel.mVehicleBody.mpAttachedVehicle = &lrVehicle;
    gaLocal[0].mPositionRadius = Vector4{ 0.9f, 0.2f,  0.0f, 0.45f };
    gaLocal[1].mPositionRadius = Vector4{ 0.9f, 0.2f,  0.8f, 0.45f };
    gaLocal[2].mPositionRadius = Vector4{ lfNeighbour1CentreX, 0.2f, -0.8f, 0.45f };
    for (int i = 0; i < 3; ++i)
    {
        std::memset(&gaSpec[i], 0, sizeof(gaSpec[i]));
        gaSpec[i].mau8NextBoundarySensor[0] = static_cast<u8>(i);
        gaSpec[i].mau8NextBoundarySensor[1] = static_cast<u8>(i);
        lrModel.maDeformationSensors[i].mpSpec = &gaSpec[i];
        lrModel.maDeformationSensors[i].mpLocalSpaceSphere = &gaLocal[i];
    }
    gaSpec[0].mau8NextBoundarySensor[0] = 1;
    gaSpec[0].mau8NextBoundarySensor[1] = 2;
    return lrModel;
}

struct Interp { bool mbResult; Vector3 mPoint; Vector3 mNormal; int miAsserts; };
static Interp RunInterp(f32 lfNeighbour1CentreX, const Vector3& lrOtherLocal)
{
    DeformationManager& lrManager = *reinterpret_cast<DeformationManager*>(gManager);
    DeformableObject&   lrModel   = FreshModel(lfNeighbour1CentreX);
    const Vector3 lPosInLocal = { 1.35f, 0.2f, 0.1f, 0.0f };   // on sensor 0's surface, facing +x
    Interp r{};
    r.mNormal = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };            // the caller's seed (world +x in local)
    const int liBefore = giAsserts;
    r.mbResult = lrManager.GetInterpolatedContactPointAndNormal(&lrModel, 0, ToWorld(lPosInLocal),
                                                                 ToWorld(lrOtherLocal), &r.mPoint, &r.mNormal);
    r.miAsserts = giAsserts - liBefore;
    return r;
}

struct Latch { bool mbResult; f32 mfStoredTime; };
static Latch RunValidate(const Vector3& lrPointOnA)
{
    std::memset(gSensor, 0, sizeof(gSensor));
    DeformationSensor& lrSensor = *reinterpret_cast<DeformationSensor*>(gSensor);
    gLocalB.mPositionRadius = Vector4{ 0.0f, 0.0f, 0.0f, 0.45f };
    lrSensor.mpLocalSpaceSphere = &gLocalB;
    lrSensor.mImpulseContact.mfImpactTimeInFrame = 100.0f;           // the per-frame disarm value
    lrSensor.mPointDisplacement_BiggestImpulseThisFrame.x = 0.0f;
    lrSensor.mPointDisplacement_BiggestImpulseThisFrame.y = 0.0f;
    lrSensor.mPointDisplacement_BiggestImpulseThisFrame.z = -0.1f;   // basis = -dot(disp, n) = 0.1 > 0
    lrSensor.mPointDisplacement_BiggestImpulseThisFrame.w = 0.0f;

    alignas(16) CgsSceneManager::SceneManagerIO::PotentialContact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mPointOnA = lrPointOnA;
    lContact.mPointOnB = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    lContact.mNormal   = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };

    Matrix44Affine lIdentity;
    lIdentity.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lIdentity.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lIdentity.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lIdentity.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 1.0f };

    Latch r{};
    r.mbResult = lrSensor.ValidateAndAddContact(lIdentity,
        reinterpret_cast<const CgsSceneManager::PotentialContact&>(lContact),
        BrnPhysics::ContactId(0x07000001u), nullptr, nullptr);
    r.mfStoredTime = lrSensor.mImpulseContact.mfImpactTimeInFrame;
    return r;
}

int main()
{
    // ---- GetInterpolatedContactPointAndNormal, neighbour 0 (0x82604AE8) ---------------------------------
    // Control: the other car at local (2.0, 0.2, 0.3). dot((0,0,0.8), (1.1,0,0.3)) = 0.24 >= 0 -> neighbour 0;
    // dThis = |(1.1, 0.3)| - 0.45 = 0.690, dNbr = |(1.1, -0.5)| - 0.45 = 0.758, dThis > dNbr false -> tangent.
    const Interp lControl0 = RunInterp(0.9f, Vector3{ 2.0f, 0.2f, 0.3f, 0.0f });
    Check(lControl0.mbResult, "control: neighbour 0 faces the other car -> the tangent arm returns 1");
    Check(!IsNan3(lControl0.mPoint) && !IsNan3(lControl0.mNormal), "control: finite tangent point and normal");
    // NaN other position: both neighbour dots are NaN. The console accepts neighbour 0 (0x82604AE8 beq), fails
    // dThis > dNbr on the NaN distances and runs the SAME tangent arm (it never reads the other position).
    const Interp lNan0 = RunInterp(0.9f, Vector3{ kNaN, 0.2f, 0.3f, 0.0f });
    Check(lNan0.mbResult, "0x82604AE8: a NaN dot accepts neighbour 0 -> the tangent arm returns 1");
    Check(Same3(lNan0.mPoint, lControl0.mPoint), "0x82604AE8: the point is the neighbour-0 tangent point, bit for bit");
    Check(Same3(lNan0.mNormal, lControl0.mNormal), "0x82604AE8: the normal is the neighbour-0 tangent direction, bit for bit");
    Check(lNan0.miAsserts == 0, "0x82604AE8: no tripwire on the tangent arm");

    // ---- GetInterpolatedContactPointAndNormal, neighbour 1 (0x82604B2C) ---------------------------------
    // Control: the other car at local (2.0, 0.2, -0.3): dot((0,0,0.8), (1.1,0,-0.3)) = -0.24 < 0 -> neighbour 0
    // refused on both builds; dot((0,0,-0.8), (1.1,0,-0.3)) = 0.24 >= 0 -> neighbour 1 -> tangent.
    const Interp lControl1 = RunInterp(0.9f, Vector3{ 2.0f, 0.2f, -0.3f, 0.0f });
    Check(lControl1.mbResult, "control: neighbour 0 refused, neighbour 1 faces the other car -> tangent, 1");
    // Neighbour 1's centre is NaN: its dot is NaN, the console accepts it (0x82604B2C beq), the NaN distance
    // fails dThis > dNbr, and the tangent arm runs on the NaN sphere -> returns 1 with a NaN normal (the
    // :1337 IsValid tripwire fires, non-gating). The tree refused the neighbour and returned 0.
    const Interp lNan1 = RunInterp(kNaN, Vector3{ 2.0f, 0.2f, -0.3f, 0.0f });
    Check(lNan1.mbResult, "0x82604B2C: a NaN dot accepts neighbour 1 -> the tangent arm returns 1");
    Check(IsNan3(lNan1.mNormal), "0x82604B2C: the tangent arm on the NaN neighbour yields a NaN normal");

    // ---- ValidateAndAddContact (0x825E1B5C) ---------------------------------------------------------------
    // n = +z, basis 0.1. Control: pen 0.05 -> t = 0.5 -> latched, returns true.
    const Latch lLatch = RunValidate(Vector3{ 0.0f, 0.0f, 0.05f, 0.0f });
    Check(lLatch.mbResult && lLatch.mfStoredTime == 0.5f, "control: pen 0.05 / basis 0.1 latches t = 0.5 and returns true");
    // Control: pen -0.05 -> ratio -0.5 -> vmaxfp(0, -0.5) = +0 -> latched at 0, returns true.
    const Latch lZero = RunValidate(Vector3{ 0.0f, 0.0f, -0.05f, 0.0f });
    Check(lZero.mbResult && lZero.mfStoredTime == 0.0f && !std::signbit(lZero.mfStoredTime),
          "control: a negative ratio clamps to +0, latches and returns true");
    // A NaN lane in the point makes pen NaN: vmaxfp keeps it, 1.0 >= NaN fails at 0x825E1B78 and 0x825E1CB8,
    // and 0.01 > NaN fails at 0x825E1CF0 -- no latch, returns false. The tree clamped the NaN to 0 and latched.
    const Latch lNan = RunValidate(Vector3{ kNaN, 0.0f, 0.05f, 0.0f });
    Check(!lNan.mbResult, "0x825E1B5C: a NaN ratio stays NaN -> the contact is not accepted");
    Check(lNan.mfStoredTime == 100.0f, "0x825E1B5C: a NaN ratio does not arm the impulse latch (stays at 100.0)");

    std::printf("FxLadderContactNan: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
