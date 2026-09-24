// FX-TRAFFIC2 (crash parity 2026-09-24, G35-D1): the PRODUCTION
//   PhysicalTrafficManager::CheckForTrafficHittingWater   @0x8261DDF0
//   PhysicalTrafficManager::GetTrafficVehicle             @0x825B4800
// extracted from the b5 sources by run_fxtraffic2_hitting_water.py and hosted on a fixture that has
// the manager's real member types. GetSimpleVehicleBox, AddTrafficRemovedEvent and
// RemoveTrafficVehicle are recorders/stubs here (they are not under test).
//
// Checked against the ARTIST asm @0x8261DDF0:
//   walk mUsedTrafficVehicles; body = mpVehicleBody (+0x1C); AGTR +0x570; mbValid (lbz 0x28);
//   surface = (low halfword of the tag >> 4) & 0x3F; KAB_SURFACE_IS_WATER[surface];
//   GetSimpleVehicleBox; lowest = w.y - |x.y*dx| - |y.y*dy| - |z.y*dz| -> mavfLowestPointWorldSpace[i]
//   (stored before the compare); hit iff AGTR.y >= lowest - 0.25 (vcmpgefp., KVF_RESET_ON_WATER_HEIGHT);
//   on a hit AddTrafficRemovedEvent(maTrafficEntityIDs[i], mePhysicalTrafficState) then
//   RemoveTrafficVehicle((u8)i, outReq, deform, false).
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "SharedClasses/World/BrnCollisionTag.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// The pre-fix revision's gate body names the TU's file-scope macro.
#define BRN_MAINTENANCE_GATE(TAG) do { } while (0)

namespace BrnPhysics
{
namespace Vehicle
{
    bool gbReadSurfaceProperties = true;
    bool KAB_SURFACE_IS_WATER[KI_MAX_NUM_SURFACES] = {};

    // ---- recorders ---------------------------------------------------------------------------
    struct RemovedEvent { u32 muEntity; s32 meType; };
    struct Removal { u8 mu8Slot; const void* mpOutReq; const void* mpDeform; bool mbRemoveFromSim; };
    static std::vector<RemovedEvent> gEvents;
    static std::vector<Removal>      gRemovals;
    static std::map<const SimpleVehiclePhysics*, CgsGeometric::Box> gBoxes;

    s32 VehicleManagerOutputInterface::AddTrafficRemovedEvent(EntityId lRemovedVehicleEntityId,
                                                             ETrafficType leTrafficType)
    {
        gEvents.push_back(RemovedEvent{ lRemovedVehicleEntityId.muValue, static_cast<s32>(leTrafficType) });
        return static_cast<s32>(gEvents.size()) - 1;
    }

    void SimpleVehiclePhysics::GetSimpleVehicleBox(CgsGeometric::Box& lrOutBox) const
    {
        lrOutBox = gBoxes[this];
    }

    struct WaterFixture
    {
        typedef PhysicalTrafficManager M;
        typedef PhysicalTrafficManager::TotalPhysicalTrafficBitArray TotalPhysicalTrafficBitArray;

        decltype(M::mpaTrafficVehicles)         mpaTrafficVehicles;
        decltype(M::maTrafficEntityIDs)         maTrafficEntityIDs;
        decltype(M::mUsedTrafficVehicles)       mUsedTrafficVehicles;
        decltype(M::mavfLowestPointWorldSpace)  mavfLowestPointWorldSpace;

        PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle);
        void RemoveTrafficVehicle(u8 lu8TrafficEntityNum, VehicleOutputRequestInterface* lpOutputRequestInterface,
                                  BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                  bool lbRemoveFromSimulation)
        {
            gRemovals.push_back(Removal{ lu8TrafficEntityNum, lpOutputRequestInterface, lpDeformationInterface,
                                         lbRemoveFromSimulation });
            mUsedTrafficVehicles.UnSetBit(lu8TrafficEntityNum);   // as the real removal does
        }
        void CheckForTrafficHittingWater(VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         VehicleOutputRequestInterface* lpOutputRequestInterface,
                                         BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);
    };
}
}

// The production bodies under test.
#include "hitting_water.inc"

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

static const u32 KU_WATER_SURFACE = 7;
static const u32 KU_ROAD_SURFACE  = 3;

alignas(64) static unsigned char gaVehicles[sizeof(PhysicalTrafficVehicle) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaBodies[KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC][sizeof(SimpleVehiclePhysics)];
alignas(64) static unsigned char gaManagerOut[sizeof(VehicleManagerOutputInterface)];
alignas(64) static unsigned char gaOutReq[64];
alignas(64) static unsigned char gaDeform[64];

static WaterFixture gManager;

static SimpleVehiclePhysics& BodyOf(u32 luSlot) { return *reinterpret_cast<SimpleVehiclePhysics*>(gaBodies[luSlot]); }

// A used physical slot whose down-ray hit (0, lfHitY, 0) on lu32Surface (valid or not). Its box:
// centre y 1.0, half-dims (1, 0.5, 2), rows as given (identity by default) -> lowest y 0.5.
static void Slot(u32 luSlot, bool lbValid, u32 luSurface, f32 lfHitY,
                 Vector3 lvX = { 1.0f, 0.0f, 0.0f, 0.0f }, Vector3 lvY = { 0.0f, 1.0f, 0.0f, 0.0f })
{
    SimpleVehiclePhysics& lrBody = BodyOf(luSlot);
    gManager.mpaTrafficVehicles[luSlot].mpVehicleBody = &lrBody;
    gManager.mpaTrafficVehicles[luSlot].mePhysicalTrafficState = E_TRAFFIC_TYPE_CRASHING;
    gManager.maTrafficEntityIDs[luSlot].muValue = 0x02000000u | ((100u + luSlot) << 10);
    gManager.mUsedTrafficVehicles.SetBit(luSlot);
    lrBody.mAboveGroundTestResult.mbValid = lbValid;
    lrBody.mAboveGroundTestResult.mIntersectionPosition = { 0.0f, lfHitY, 0.0f, 0.0f };
    lrBody.mAboveGroundTestResult.mCollisionTag.muValue = (luSurface << 4) & 0xFFFFu;   // the low halfword

    CgsGeometric::Box lBox;
    std::memset(&lBox, 0, sizeof(lBox));
    lBox.mTransform.SetIdentity();
    lBox.mTransform.xAxis = lvX;
    lBox.mTransform.yAxis = lvY;
    lBox.mTransform.wAxis = { 0.0f, 1.0f, 0.0f, 1.0f };
    lBox.mDimensionsAndFatness.x = 1.0f;
    lBox.mDimensionsAndFatness.y = 0.5f;
    lBox.mDimensionsAndFatness.z = 2.0f;
    gBoxes[&lrBody] = lBox;
}

static bool Lowest(u32 luSlot, f32 lfY)
{
    const VecFloat& lr = gManager.mavfLowestPointWorldSpace[luSlot];
    return lr.x == lfY && lr.y == lfY && lr.z == lfY && lr.w == lfY;
}

int main()
{
    std::memset(gaVehicles, 0, sizeof(gaVehicles));
    std::memset(gaBodies, 0, sizeof(gaBodies));
    std::memset(&gManager, 0, sizeof(gManager));
    gManager.mpaTrafficVehicles = reinterpret_cast<PhysicalTrafficVehicle*>(gaVehicles);
    for (u32 luSlot = 0; luSlot < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC; ++luSlot)
    {
        gManager.mavfLowestPointWorldSpace[luSlot] = { -99.0f, -99.0f, -99.0f, -99.0f };
    }
    KAB_SURFACE_IS_WATER[KU_WATER_SURFACE] = true;

    Slot(0, true,  KU_WATER_SURFACE, 0.30f);           // lowest 0.5 ; 0.30 >= 0.25 -> hit
    Slot(1, true,  KU_ROAD_SURFACE,  0.30f);           // not water -> nothing
    Slot(2, false, KU_WATER_SURFACE, 0.30f);           // no valid down-ray -> nothing
    Slot(3, true,  KU_WATER_SURFACE, 0.25f);           // exactly lowest - 0.25 -> hit (>=)
    Slot(4, true,  KU_WATER_SURFACE, 0.2499f);         // just under -> no hit, but the lowest point IS stored
    Slot(5, true,  KU_WATER_SURFACE, -0.2f,            // rolled: x row points down -> lowest 1 - |-1*1| = 0.0
         Vector3{ 0.0f, -1.0f, 0.0f, 0.0f }, Vector3{ 1.0f, 0.0f, 0.0f, 0.0f });   // -0.2 >= -0.25 -> hit
    Slot(9, true,  KU_WATER_SURFACE, 0.30f);
    gManager.mUsedTrafficVehicles.UnSetBit(9);         // a free slot is never visited

    VehicleManagerOutputInterface* const lpManagerOut = reinterpret_cast<VehicleManagerOutputInterface*>(gaManagerOut);
    VehicleOutputRequestInterface* const lpOutReq = reinterpret_cast<VehicleOutputRequestInterface*>(gaOutReq);
    BrnPhysics::Deformation::DeformationInputInterface* const lpDeform =
        reinterpret_cast<BrnPhysics::Deformation::DeformationInputInterface*>(gaDeform);

    gManager.CheckForTrafficHittingWater(lpManagerOut, lpOutReq, lpDeform);

    Check(gEvents.size() == 3 && gRemovals.size() == 3, "three cars reach the water: slots 0, 3 and 5");
    const bool lbHave = gEvents.size() == 3 && gRemovals.size() == 3;
    Check(lbHave && gEvents[0].muEntity == (0x02000000u | (100u << 10)) && gEvents[1].muEntity == (0x02000000u | (103u << 10))
          && gEvents[2].muEntity == (0x02000000u | (105u << 10)),
          "each removed event carries maTrafficEntityIDs[i] (0x8261E124 lwzx this+4*(i+0x64F0))");
    Check(lbHave && gEvents[0].meType == E_TRAFFIC_TYPE_CRASHING && gEvents[2].meType == E_TRAFFIC_TYPE_CRASHING,
          "... and the car's mePhysicalTrafficState (lwz 0x20)");
    Check(lbHave && gRemovals[0].mu8Slot == 0 && gRemovals[1].mu8Slot == 3 && gRemovals[2].mu8Slot == 5,
          "RemoveTrafficVehicle((u8)i) in bit order (0x8261E174)");
    Check(lbHave && gRemovals[0].mpOutReq == lpOutReq && gRemovals[0].mpDeform == lpDeform && !gRemovals[0].mbRemoveFromSim,
          "... with outReq, deform and false (byte_82FB7DF1 == 0)");
    Check(Lowest(0, 0.5f) && Lowest(3, 0.5f) && Lowest(4, 0.5f),
          "lowest point = centre.y - |x.y*dx| - |y.y*dy| - |z.y*dz| = 0.5, stored BEFORE the compare (slot 4 stored, not removed)");
    Check(Lowest(5, 0.0f), "the rolled box uses |x.y*dx| (the vandc sign clear): lowest 0.0");
    Check(Lowest(1, -99.0f) && Lowest(2, -99.0f) && Lowest(9, -99.0f),
          "no store for a non-water, an invalid or a free slot");
    Check(!gManager.mUsedTrafficVehicles.IsBitSet(0) && gManager.mUsedTrafficVehicles.IsBitSet(1)
          && gManager.mUsedTrafficVehicles.IsBitSet(4),
          "only the water cars leave the used set");

    // gbReadSurfaceProperties is asserted, not a gate.
    gEvents.clear();
    gRemovals.clear();
    gbReadSurfaceProperties = false;
    Slot(6, true, KU_WATER_SURFACE, 0.30f);
    const unsigned luAssertsBefore = gAsserts;
    gManager.CheckForTrafficHittingWater(lpManagerOut, lpOutReq, lpDeform);
    Check(gAsserts > luAssertsBefore && gRemovals.size() == 1 && gRemovals[0].mu8Slot == 6,
          "surface properties not read: the assert fires and the check still runs (fire-and-continue, :4230)");
    gAsserts = luAssertsBefore;

    Check(gAsserts == 0, "no other assert fires");
    std::printf("FxTraffic2HittingWater: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
