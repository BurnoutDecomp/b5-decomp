// FX-TRAFFIC2 (crash parity 2026-09-24, G39-D2 + G39-D3): the PRODUCTION
//   PhysicalTrafficVehicle::SetArticulated                       @0x825F3B68
//   PhysicalTrafficVehicle::GetFullTrafficPhysics                @0x825C0148
//   Deformation::LocatorPointSpecList::GetLocatorXf              @0x825B31E0
// extracted from the b5 sources by run_fxtraffic2_set_articulated.py and run against a real
// StreamedDeformationSpec whose generic locators live below 4 GB (Ptr32 slots). Every expected
// value below is derived from the ARTIST asm (the decode is in the body):
//   the hitch point is the FIRST generic locator of type 29 (REAR) for a CAB, else of type 28
//   (FRONT), moved by the inverse of spec +0x610 (mCarModelSpaceToHandlingBodySpaceTransform):
//   for an orthonormal R with translation t that is (dot(X, p - t), dot(Y, p - t), dot(Z, p - t), 0)
//   and it lands in mArticulationPointLocal (+0); a spec without the tag fires
//   "Failed to find articulation tag point" (:463) and the index (== the count) is still used.
//   G39-D3: a FULL (type 0) car's body +0x1050 gets lane w = 0.5 (unk_8208FACC, `vrlimi128 ... 1, 0`)
//   and nothing else; a SIMPLE car's body is never touched.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static unsigned gTagMisses = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        if (std::strcmp(lpcMessage, "Failed to find articulation tag point") == 0)
        {
            ++gTagMisses;
        }
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// The production bodies under test.
#include "set_articulated.inc"

using namespace BrnPhysics::Vehicle;
using namespace BrnPhysics::Deformation;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-6f; }

alignas(64) static unsigned char gaSpec[sizeof(StreamedDeformationSpec)];
alignas(64) static unsigned char gaVehicle[sizeof(PhysicalTrafficVehicle)];
alignas(64) static unsigned char gaBody[sizeof(TrafficPhysics)];
static StreamedDeformationSpec* gpSpec = nullptr;
static LocatorPointSpec* gpLocators = nullptr;   // below 4 GB: the list holds a Ptr32

static StreamedDeformationSpec& Spec() { return *reinterpret_cast<StreamedDeformationSpec*>(gaSpec); }
static PhysicalTrafficVehicle& Car()  { return *reinterpret_cast<PhysicalTrafficVehicle*>(gaVehicle); }
static Vector4& WeightRow()
{
    return reinterpret_cast<TrafficPhysics*>(gaBody)
        ->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor;
}

static void Locator(u32 luIndex, ETagPointType leType, f32 lfX, f32 lfY, f32 lfZ)
{
    new (&gpLocators[luIndex]) LocatorPointSpec{};
    gpLocators[luIndex].mLocatorMatrix.SetIdentity();
    gpLocators[luIndex].mLocatorMatrix.wAxis = { lfX, lfY, lfZ, 1.0f };
    gpLocators[luIndex].meTagPointType = leType;
}

// Spec: X = (0,0,-1), Y = (0,1,0), Z = (1,0,0), t = (0.1, 0.4, 0.2); generic tags
// {5 at (7,7,7); 28 FRONT at (0, 0.9, -2.5); 29 REAR at (0, 1.1, 3.0); 29 REAR again at (9,9,9)}.
static void Fresh(u32 luNumTags)
{
    std::memset(gaSpec, 0, sizeof(gaSpec));
    std::memset(gaVehicle, 0, sizeof(gaVehicle));
    Matrix44Affine& lrM = Spec().mCarModelSpaceToHandlingBodySpaceTransform;
    lrM.SetIdentity();
    lrM.xAxis = { 0.0f, 0.0f, -1.0f, 0.0f };
    lrM.yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
    lrM.zAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
    lrM.wAxis = { 0.1f, 0.4f, 0.2f, 1.0f };
    Locator(0, static_cast<ETagPointType>(5), 7.0f, 7.0f, 7.0f);
    Locator(1, E_TAGPOINT_ARTICULATIONPOINT_FRONT, 0.0f, 0.9f, -2.5f);
    Locator(2, E_TAGPOINT_ARTICULATIONPOINT_REAR, 0.0f, 1.1f, 3.0f);
    Locator(3, E_TAGPOINT_ARTICULATIONPOINT_REAR, 9.0f, 9.0f, 9.0f);
    Spec().mGenericTags.muNumLocators = luNumTags;
    Spec().mGenericTags.mpaLocatorPoints.muSlot = static_cast<u32>(reinterpret_cast<uintptr_t>(gpLocators));
    Car().miJointIndex = -1;
    Car().mu8PhysicalType = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE;
    Car().mArticulationPointLocal = { 999.0f, 999.0f, 999.0f, 999.0f };
    std::memset(gaBody, 0, sizeof(gaBody));
    Car().mpVehicleBody = reinterpret_cast<SimpleVehiclePhysics*>(gaBody);
    WeightRow() = { -0.1f, 1.0f, 100.0f, 1.0f };   // as VehiclePhysics::Prepare seeds it
    gAsserts = 0;
    gTagMisses = 0;
}

static void Run(PhysicalTrafficVehicle::EArticulatedVehicleType leType)
{
    CreatePhysicalTrafficEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.mModelHandle.mpResourceMemory = &gpSpec;   // *(mpResourceMemory) == the spec
    Car().SetArticulated(lEvent, leType);
}

// (dot(X, p - t), dot(Y, p - t), dot(Z, p - t)) for the fixture's rows.
static bool Hitch(f32 lfX, f32 lfY, f32 lfZ)
{
    const f32 lfDX = lfX - 0.1f, lfDY = lfY - 0.4f, lfDZ = lfZ - 0.2f;
    const Vector3& lr = Car().mArticulationPointLocal;
    return Near(lr.x, -lfDZ) && Near(lr.y, lfDY) && Near(lr.z, lfDX) && lr.w == 0.0f;
}

int main()
{
    for (uintptr_t luAddress = 0x10000000; !gpLocators && luAddress < 0xF0000000; luAddress += 0x10000)
    {
        gpLocators = static_cast<LocatorPointSpec*>(VirtualAlloc(reinterpret_cast<void*>(luAddress), 65536,
                                                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    }
    if (!gpLocators)
    {
        return 2;
    }
    gpSpec = &Spec();

    // ---- a CAB hitches at the FIRST REAR (29) locator -----------------------------------------
    Fresh(4);
    Run(PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB);
    Check(Hitch(0.0f, 1.1f, 3.0f),
          "CAB: mArticulationPointLocal = inverse(spec +0x610) * the first type-29 locator = (-2.8, 0.7, -0.1, 0)");
    Check(Car().meArticulatedVehicleType == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB
          && Car().meArticulatedJointState == PhysicalTrafficVehicle::E_ARTICULATE_JOINT_ATTACHED,
          "CAB: type +0x24 and state ATTACHED +0x28 are still set");
    Check(gAsserts == 0, "CAB: no assert");

    // ---- a TRAILER hitches at the FRONT (28) locator -----------------------------------------
    Fresh(4);
    Run(PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER);
    Check(Hitch(0.0f, 0.9f, -2.5f),
          "TRAILER: mArticulationPointLocal = inverse(spec +0x610) * the type-28 locator = (2.7, 0.5, -0.1, 0)");
    Check(gAsserts == 0, "TRAILER: no assert");

    // ---- no REAR tag in the list: the assert fires once and the index (== count) is used --------
    Fresh(2);   // tags {5, 28}: locator [2] exists in memory but is past the count
    Run(PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB);
    Check(gTagMisses == 1, "CAB with no type-29 locator: \"Failed to find articulation tag point\" fires exactly once");
    Check(Hitch(0.0f, 1.1f, 3.0f), "... and the point is still read at index == count (non-gating assert)");

    // ---- G39-D3: a FULL car's body gets the articulated solve-penetration weight, lane w only ----
    Fresh(4);
    Car().mu8PhysicalType = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL;
    Run(PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB);
    Check(WeightRow().w == 0.5f, "FULL: body +0x1050 .w = KF_ARTICULATED_SOLVE_PENETRATION_WEIGHT_FACTOR (0.5, unk_8208FACC)");
    Check(WeightRow().x == -0.1f && WeightRow().y == 1.0f && WeightRow().z == 100.0f,
          "FULL: lanes x/y/z untouched (vrlimi128 mask 1)");

    Fresh(4);   // SIMPLE (type 1)
    Run(PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER);
    Check(WeightRow().x == -0.1f && WeightRow().y == 1.0f && WeightRow().z == 100.0f && WeightRow().w == 1.0f
          && gAsserts == 0,
          "SIMPLE: the body is never touched and GetFullTrafficPhysics is never asked");

    std::printf("FxTraffic2SetArticulated: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
