// FX-TRAFFIC2 (crash parity 2026-09-24, G32-D1): the PRODUCTION
//   PhysicalTrafficManager::ValidateTrafficContact     @0x825CACB8
//   PhysicalTrafficManager::GetTrafficVehicle          @0x825B4800
//   SimpleVehiclePhysics::IsContactBelowWheelPlane     @0x825BF870
// extracted from the b5 sources by run_fxtraffic2_wheels_detached.py and hosted on a fixture that
// has the manager's real member types; the body is a real SimpleVehiclePhysics seated in zeroed
// storage.
//
// Checked against the ARTIST asm @0x825CACB8:
//   0x825CADAC  mu8GlobalToPhysicalEntityIndexMap[idx] == 0x7F -> return 0
//   0x825CAE28  B's owner byte != 0 (not the world) -> return 1
//   0x825CAE98  lbValid = !IsContactBelowWheelPlane(mPointOnB, 0.4)
//   0x825CAEAC  `lbz r11, 0x715(body)` (mbAnyWheelsDetatched) ; bne -> 0x825CAF4C `li r3, 1`
//   0x825CAEC8  dot3(yAxis, ground normal) > 0.8 (flt_8208F9C8) AND
//   0x825CAF04  0.4 > dot3(mPointOnB - ground position, normal)          -> return 0
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    struct ContactFixture
    {
        typedef PhysicalTrafficManager M;
        decltype(M::mpaTrafficVehicles)                 mpaTrafficVehicles;
        decltype(M::mu8GlobalToPhysicalEntityIndexMap)  mu8GlobalToPhysicalEntityIndexMap;

        PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle);
        bool ValidateTrafficContact(CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
                                    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
                                    f32 lfTimeStep);
    };
}
}

// The production bodies under test.
#include "wheels_detached.inc"

using namespace BrnPhysics::Vehicle;
typedef CgsSceneManager::SceneManagerIO::PotentialContact PotentialContact;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaVehicles[sizeof(PhysicalTrafficVehicle) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaBody[sizeof(SimpleVehiclePhysics)];
alignas(64) static unsigned char gaTriCache[64];

static ContactFixture gManager;
static SimpleVehiclePhysics& Body() { return *reinterpret_cast<SimpleVehiclePhysics*>(gaBody); }

static const u32 KU_GLOBAL_INDEX = 123;   // the traffic car's global entity index
static const u8  KU_PHYSICAL_SLOT = 4;

// A traffic car on flat ground: identity transform (yAxis (0,1,0)), its down-ray hit (0,0,0) with
// normal (0,1,0); no valid wheel plane; the contact B point 0.1 m above the ground, B = the world.
static PotentialContact Scene(bool lbWheelsDetached)
{
    std::memset(gaVehicles, 0, sizeof(gaVehicles));
    std::memset(gaBody, 0, sizeof(gaBody));
    std::memset(&gManager, 0, sizeof(gManager));
    gManager.mpaTrafficVehicles = reinterpret_cast<PhysicalTrafficVehicle*>(gaVehicles);
    std::memset(gManager.mu8GlobalToPhysicalEntityIndexMap, KU8_INVALID_MAP,
                sizeof(gManager.mu8GlobalToPhysicalEntityIndexMap));
    gManager.mu8GlobalToPhysicalEntityIndexMap[KU_GLOBAL_INDEX] = KU_PHYSICAL_SLOT;
    gManager.mpaTrafficVehicles[KU_PHYSICAL_SLOT].mpVehicleBody = &Body();

    Body().mTransform.SetIdentity();
    Body().mAboveGroundTestResult.mIntersectionPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
    Body().mAboveGroundTestResult.mIntersectionNormal   = { 0.0f, 1.0f, 0.0f, 0.0f };
    Body().mAboveGroundTestResult.mbValid               = true;
    Body().mbMinWheelDistValid  = false;
    Body().mbAnyWheelsDetatched = lbWheelsDetached;

    PotentialContact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mPointOnB = { 0.0f, 0.1f, 0.0f, 1.0f };
    lContact.muVolumeInstanceIdA.muId =
        (static_cast<u64>((KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24) | (KU_GLOBAL_INDEX << 10)) << 32);
    lContact.muVolumeInstanceIdB.muId = 0;   // owner byte 0: the world
    return lContact;
}

static bool Validate(PotentialContact& lrContact)
{
    return gManager.ValidateTrafficContact(
        &lrContact, reinterpret_cast<const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*>(gaTriCache),
        1.0f / 30.0f);
}

int main()
{
    {
        PotentialContact lContact = Scene(true);
        Check(Validate(lContact),
              "wheels detached: an upright car's ground contact 0.1 m above the plane is ACCEPTED "
              "(0x825CAEAC lbz 0x715 ; bne -> li r3, 1)");
    }
    {
        PotentialContact lContact = Scene(false);
        Check(!Validate(lContact),
              "wheels on: the same contact is rejected (upright dot 1.0 > 0.8 and height 0.1 < 0.4)");
    }
    {
        PotentialContact lContact = Scene(true);
        Body().mbMinWheelDistValid = true;               // a valid wheel plane the contact is BELOW
        Body().mWheelPlanePosAndHeight = { 0.0f, 5.0f, 0.0f, 0.0f };
        Check(Validate(lContact),
              "wheels detached: accepted even when IsContactBelowWheelPlane says below "
              "(the flag test comes after it and returns 1 outright)");
    }
    {
        PotentialContact lContact = Scene(false);
        lContact.muVolumeInstanceIdB.muId = static_cast<u64>(1u << 24) << 32;   // B is a race car
        Check(Validate(lContact), "B not the world: accepted (0x825CAE28 bne -> li r3, 1)");
    }
    {
        PotentialContact lContact = Scene(false);
        const f32 lfC = std::cos(0.785398163f), lfS = std::sin(0.785398163f);   // tilted 45 degrees
        Body().mTransform.yAxis = { 0.0f, lfC, lfS, 0.0f };
        Check(Validate(lContact), "tilted 45 degrees (dot 0.707 <= 0.8): the ground contact is kept");
    }
    {
        PotentialContact lContact = Scene(true);
        gManager.mu8GlobalToPhysicalEntityIndexMap[KU_GLOBAL_INDEX] = KU8_INVALID_MAP;
        Check(!Validate(lContact), "no physical slot (0x7F): rejected before the flag is read (0x825CADB0)");
    }

    Check(gAsserts == 0, "no assert fires");
    std::printf("FxTraffic2WheelsDetached: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
