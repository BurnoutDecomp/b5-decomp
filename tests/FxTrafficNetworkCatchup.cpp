// FX-TRAFFIC (crash parity 2026-09-23, G34-D1): the PRODUCTION
// PhysicalTrafficManager::UpdateNetworkTrafficVehicle (plus GetTrafficVehicle / GetTrafficDriver
// and PhysicalTrafficVehicle::GetFullTrafficPhysics), extracted by run_fxtraffic_network_catchup.py
// and hosted on a fixture that carries the real mpaTrafficVehicles / mpaTrafficDrivers types.
// VehicleDriver::StartCatchupInterpolation is a RECORDER here (it is FX-VMNET's, tested there).
//
// ARTIST @0x8261CBD0: assert id != -1 (:560) ; idx = extrwi(id,14,8) ; GetTrafficVehicle(idx) ;
// lbz 0x32 < 2 (h:382) ; FULL (== 0) only ; GetFullTraffic (0x825C0148) ; GetTrafficDriver(idx)
// (0x825B4900) ; StartCatchupInterpolation(r4 full, r5 event+0x10, v1 [full+0x50], v2 [full+0x60],
// r6 = 0) @0x8261CC8C.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <vector>

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
}

// The pre-fix revision's gate body names the TU's file-scope LogOnce.
static void LogOnce(bool&, const char*) {}

namespace BrnPhysics
{
namespace Vehicle
{
    struct NetworkCatchupFixture
    {
        typedef PhysicalTrafficManager M;
        decltype(M::mpaTrafficDrivers)  mpaTrafficDrivers;
        decltype(M::mpaTrafficVehicles) mpaTrafficVehicles;

        PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle);
        VehicleDriver*          GetTrafficDriver(s32 liVehicle);
        void UpdateNetworkTrafficVehicle(const UpdateNetworkTrafficEvent* lpEvent, EntityId lTrafficPhysicsId);
    };

    struct CatchupCall
    {
        const VehicleDriver*  mpDriver;
        const VehiclePhysics* mpVehicle;
        const Matrix44Affine* mpTransform;
        Vector3               mvLinear;
        Vector3               mvAngular;
        bool                  mbSnap;
    };
    static std::vector<CatchupCall> gCalls;

    void VehicleDriver::StartCatchupInterpolation(VehiclePhysics* lpVehicle,
                                                  const Matrix44Affine& lCatchupTransformGraphicsSpace,
                                                  const Vector3 lCatchupLinearVelocity,
                                                  const Vector3 lCatchupAngularVelocity,
                                                  bool lbSnap)
    {
        CatchupCall lCall = { this, lpVehicle, &lCatchupTransformGraphicsSpace,
                              lCatchupLinearVelocity, lCatchupAngularVelocity, lbSnap };
        gCalls.push_back(lCall);
    }
}
}

// The production bodies under test.
#include "network_catchup.inc"

using namespace BrnPhysics::Vehicle;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Same(const Vector3& lrA, f32 lfX, f32 lfY, f32 lfZ)
{
    return lrA.x == lfX && lrA.y == lfY && lrA.z == lfZ;
}

alignas(64) static unsigned char gaVehicles[sizeof(PhysicalTrafficVehicle) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaDrivers[sizeof(VehicleDriver) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaBody[sizeof(TrafficPhysics)];

int main()
{
    std::memset(gaVehicles, 0, sizeof(gaVehicles));
    std::memset(gaDrivers, 0, sizeof(gaDrivers));
    std::memset(gaBody, 0, sizeof(gaBody));

    NetworkCatchupFixture lManager;
    lManager.mpaTrafficVehicles = reinterpret_cast<PhysicalTrafficVehicle*>(gaVehicles);
    lManager.mpaTrafficDrivers  = reinterpret_cast<VehicleDriver*>(gaDrivers);

    TrafficPhysics* const lpBody = reinterpret_cast<TrafficPhysics*>(gaBody);
    lpBody->mLinearVelocity  = { 1.0f, 2.0f, 3.0f, 0.0f };
    lpBody->mAngularVelocity = { 4.0f, 5.0f, 6.0f, 0.0f };

    alignas(16) UpdateNetworkTrafficEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.mTransform.wAxis = { 10.0f, 20.0f, 30.0f, 1.0f };

    // ---- Case A: a FULL car in physical slot 7 ----
    PhysicalTrafficVehicle& lrFull = lManager.mpaTrafficVehicles[7];
    lrFull.mu8PhysicalType = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL;
    lrFull.mpVehicleBody   = reinterpret_cast<SimpleVehiclePhysics*>(lpBody);
    EntityId lId;
    lId.muValue = (7u << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);
    lManager.UpdateNetworkTrafficVehicle(&lEvent, lId);

    Check(gCalls.size() == 1, "FULL car: exactly one StartCatchupInterpolation  @0x8261CC8C");
    const bool lbHave = !gCalls.empty();
    Check(lbHave && gCalls[0].mpDriver == &lManager.mpaTrafficDrivers[7],
          "...on the driver of the same physical slot (GetTrafficDriver(extrwi(id,14,8)) 0x825B4900)");
    Check(lbHave && gCalls[0].mpVehicle == static_cast<VehiclePhysics*>(lpBody),
          "...with the car's full-physics body (GetFullTraffic 0x825C0148 -> r4)");
    Check(lbHave && gCalls[0].mpTransform == &lEvent.mTransform,
          "...and the event's transform BY REFERENCE (r5 = event + 0x10)");
    Check(lbHave && Same(gCalls[0].mvLinear, 1.0f, 2.0f, 3.0f),
          "...v1 = the car's own linear velocity (+0x50)");
    Check(lbHave && Same(gCalls[0].mvAngular, 4.0f, 5.0f, 6.0f),
          "...v2 = the car's own angular velocity (+0x60)");
    Check(lbHave && !gCalls[0].mbSnap, "...lbSnap == false (li r6, 0 @0x8261CC78)");

    // ---- Case B: a SIMPLE car -- nothing (0x8261CC50 bne out, not an assert) ----
    gCalls.clear();
    PhysicalTrafficVehicle& lrSimple = lManager.mpaTrafficVehicles[3];
    lrSimple.mu8PhysicalType = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE;
    lrSimple.mpVehicleBody   = reinterpret_cast<SimpleVehiclePhysics*>(lpBody);
    lId.muValue = (3u << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);
    lManager.UpdateNetworkTrafficVehicle(&lEvent, lId);
    Check(gCalls.empty(), "SIMPLE car: no catch-up");

    Check(gAsserts == 0, "no assert fires on valid ids");
    std::printf("FxTrafficNetworkCatchup: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
