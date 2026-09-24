// FX-TRAFFIC2 (crash parity 2026-09-24, G34-D3): the PRODUCTION
//   PhysicalTrafficManager::RemoveTrafficVehicle                  @0x8261CC98 (the articulated arm
//                                                                  0x8261CDF4..0x8261CFBC)
//   ArticulatedJointPool::RemoveJoint                             @0x825D8248 (when the revision has it)
//   ArticulatedJointPool::GetIndexOfOtherHalf / GetJoint / IsJointInUse and the packed-id readers
//   ArticulatedJointCreateBuffer::FlagJointToBeRemoved            @0x825C24F8
//   PhysicalTrafficVehicle::HasNonBrokenJoint / GetArticulatedVehicleType, GetTrafficVehicle
// extracted from the b5 sources by run_fxtraffic2_articulated_remove.py and hosted on a fixture that
// has the manager's real member types. PhysicallyUncrashTrafficCar and VehicleDriver::ClearControls
// are recorders here (they are not under test).
//
// Checked against the ARTIST asm: removing either half of an ATTACHED pair clears the OTHER half's
// +0x24/+0x28/+0x2C to NONE/NONE/-1 (0x8261CF74..0x8261CF84), flags the joint for removal in the
// working buffer carrying its packed id (RemoveJoint 0x825D8448 -> FlagJointToBeRemoved) and frees
// the pool slot (mUsedJoints.UnSetBit, 0x825D8474..0x825D8480); then the own-half tail runs as before.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

// The pre-fix revision's gate body names the TU's file-scope macro.
static unsigned guGateLogs = 0;
#define BRN_T3_REMOVE_GATE(TAG) do { ++guGateLogs; } while (0)

static std::vector<u16> gUncrashed;
static unsigned guClearControls = 0;

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleDriver::ClearControls() { ++guClearControls; }

    struct RemoveFixture
    {
        typedef PhysicalTrafficManager M;

        decltype(M::mpaTrafficVehicles)                mpaTrafficVehicles;
        decltype(M::mpaTrafficDrivers)                 mpaTrafficDrivers;
        decltype(M::maTrafficEntityIDs)                maTrafficEntityIDs;
        decltype(M::mUsedTrafficVehicles)              mUsedTrafficVehicles;
        decltype(M::mUsedFullTrafficPhysics)           mUsedFullTrafficPhysics;
        decltype(M::mUsedSimpleVehiclePhysics)         mUsedSimpleVehiclePhysics;
        decltype(M::mPotentialTrafficVehicles)         mPotentialTrafficVehicles;
        decltype(M::mAddedTrafficVehicles)             mAddedTrafficVehicles;
        decltype(M::mRemovedTrafficVehicles)           mRemovedTrafficVehicles;
        decltype(M::mu8GlobalToPhysicalEntityIndexMap) mu8GlobalToPhysicalEntityIndexMap;
        decltype(M::mpArticulatedJointCreateBuffer)    mpArticulatedJointCreateBuffer;
        decltype(M::mArticulatedJointPool)             mArticulatedJointPool;

        PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle);
        void PhysicallyUncrashTrafficCar(u16 lu16TrafficEntityNum,
                                         BrnPhysics::Deformation::DeformationInputInterface*)
        {
            gUncrashed.push_back(lu16TrafficEntityNum);
        }
        void RemoveTrafficVehicle(u8 lu8TrafficEntityNum, VehicleOutputRequestInterface* lpOutputRequestInterface,
                                  BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                  bool lbRemoveFromSimulation);
    };
}
}

// The production bodies under test.
#include "articulated_remove.inc"

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

alignas(64) static unsigned char gaVehicles[sizeof(PhysicalTrafficVehicle) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaDrivers[sizeof(VehicleDriver) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC];
alignas(64) static unsigned char gaBuffer[sizeof(ArticulatedJointCreateBuffer)];
static RemoveFixture gManager;

static const s32 KI_JOINT = 2;
static const u32 KU_CAB = 4, KU_TRAILER = 9, KU_PLAIN = 7;
static const u64 KU64_JOINT_ID = (static_cast<u64>((KU_CAB << 10) | (2u << 24)) << 32)   // cab EntityId
                              | (static_cast<u64>(KU_TRAILER) << 16)                      // trailer index
                              | static_cast<u64>(KI_JOINT);                               // pool index

static PhysicalTrafficVehicle& Car(u32 luSlot) { return gManager.mpaTrafficVehicles[luSlot]; }
static ArticulatedJointCreateBuffer& Buffer() { return *reinterpret_cast<ArticulatedJointCreateBuffer*>(gaBuffer); }

static void Slot(u32 luSlot, PhysicalTrafficVehicle::EArticulatedVehicleType leType, s32 liJoint)
{
    PhysicalTrafficVehicle& lr = Car(luSlot);
    lr.meArticulatedVehicleType = leType;
    lr.meArticulatedJointState  = liJoint >= 0 ? PhysicalTrafficVehicle::E_ARTICULATE_JOINT_ATTACHED
                                               : PhysicalTrafficVehicle::E_ARTICULATE_JOINT_NONE;
    lr.miJointIndex             = liJoint;
    lr.mu8PhysicalType          = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL;
    lr.mu8PhysicsPoolIndex      = static_cast<u8>(luSlot);
    gManager.mUsedFullTrafficPhysics.SetBit(luSlot);
    gManager.mUsedTrafficVehicles.SetBit(luSlot);
    gManager.maTrafficEntityIDs[luSlot].muValue = 0x02000000u | ((200u + luSlot) << 10);
    gManager.mu8GlobalToPhysicalEntityIndexMap[200u + luSlot] = static_cast<u8>(luSlot);
}

static void Fresh()
{
    std::memset(gaVehicles, 0, sizeof(gaVehicles));
    std::memset(gaDrivers, 0, sizeof(gaDrivers));
    std::memset(gaBuffer, 0, sizeof(gaBuffer));
    std::memset(&gManager, 0, sizeof(gManager));
    gManager.mpaTrafficVehicles = reinterpret_cast<PhysicalTrafficVehicle*>(gaVehicles);
    gManager.mpaTrafficDrivers  = reinterpret_cast<VehicleDriver*>(gaDrivers);
    gManager.mpArticulatedJointCreateBuffer = &Buffer();
    for (u32 luSlot = 0; luSlot < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC; ++luSlot)
    {
        Car(luSlot).miJointIndex = -1;
    }
    Slot(KU_CAB, PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB, KI_JOINT);
    Slot(KU_TRAILER, PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER, KI_JOINT);
    Slot(KU_PLAIN, PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE, -1);
    gManager.mArticulatedJointPool.maJoints[KI_JOINT].mJointId.mu64RawId = KU64_JOINT_ID;
    gManager.mArticulatedJointPool.mUsedJoints.SetBit(KI_JOINT);
    Buffer().mCreatedJointBitArray.UnSetAll();
    Buffer().mRemovedJointBitArray.UnSetAll();
    gAsserts = 0;
    guGateLogs = 0;
    gUncrashed.clear();
}

static bool Unhitched(u32 luSlot)
{
    const PhysicalTrafficVehicle& lr = Car(luSlot);
    return lr.meArticulatedVehicleType == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE
        && lr.meArticulatedJointState == PhysicalTrafficVehicle::E_ARTICULATE_JOINT_NONE
        && lr.miJointIndex == -1;
}

static bool JointFlaggedForRemoval()
{
    return Buffer().mRemovedJointBitArray.IsBitSet(KI_JOINT)
        && Buffer().maRemovedJointEvents[KI_JOINT].mu64Id == KU64_JOINT_ID;
}

static void Remove(u32 luSlot)
{
    gManager.RemoveTrafficVehicle(static_cast<u8>(luSlot), nullptr, nullptr, false);
}

int main()
{
    // ---- the CAB goes: the trailer is unhitched and the joint handed back --------------------
    Fresh();
    Remove(KU_CAB);
    Check(Unhitched(KU_TRAILER), "removing the cab clears the TRAILER's type/state/joint to NONE/NONE/-1");
    Check(JointFlaggedForRemoval(),
          "joint 2 is flagged for removal in the working buffer, carrying its packed id (ld 0x40)");
    Check(!gManager.mArticulatedJointPool.mUsedJoints.IsBitSet(KI_JOINT), "the pool frees joint 2 (mUsedJoints.UnSetBit)");
    Check(Unhitched(KU_CAB) && !gManager.mUsedTrafficVehicles.IsBitSet(KU_CAB),
          "the cab itself is cleared and released (the unchanged own-half tail)");
    Check(gManager.mUsedTrafficVehicles.IsBitSet(KU_TRAILER)
          && gManager.maTrafficEntityIDs[KU_TRAILER].muValue == (0x02000000u | ((200u + KU_TRAILER) << 10)),
          "the trailer stays a live physical car (only its hitch is undone)");
    Check(gAsserts == 0 && guGateLogs == 0, "no assert, no gate");

    // ---- the TRAILER goes: the mirror ------------------------------------------------------
    Fresh();
    Remove(KU_TRAILER);
    Check(Unhitched(KU_CAB), "removing the trailer clears the CAB's type/state/joint to NONE/NONE/-1");
    Check(JointFlaggedForRemoval() && !gManager.mArticulatedJointPool.mUsedJoints.IsBitSet(KI_JOINT),
          "... and hands joint 2 back the same way");
    Check(gAsserts == 0 && guGateLogs == 0, "no assert, no gate (trailer side)");

    // ---- a plain car: the pool and the buffer are untouched ----------------------------------
    Fresh();
    Remove(KU_PLAIN);
    Check(!Buffer().mRemovedJointBitArray.IsBitSet(KI_JOINT) && gManager.mArticulatedJointPool.mUsedJoints.IsBitSet(KI_JOINT)
          && Car(KU_CAB).meArticulatedJointState == PhysicalTrafficVehicle::E_ARTICULATE_JOINT_ATTACHED
          && gAsserts == 0,
          "an un-hitched car's removal never reaches the joint pool");

    std::printf("FxTraffic2ArticulatedRemove: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
