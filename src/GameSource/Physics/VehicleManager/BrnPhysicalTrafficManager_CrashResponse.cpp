// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_CrashResponse.cpp
//
// Wave T3 round 2, owner B -- WHAT THE TRAFFIC CAR DOES WHEN THE PLAYER HITS IT. The four
// response arms VehicleManager::HandleRaceCarTrafficCarPotentialContact dispatches, plus the
// crash latch they share.
//
//   PhysicalTrafficManager::SetTrafficVehicleCrashing  @0x82636E38 (229)
//   PhysicalTrafficManager::SetTrafficVehicleChecked   @0x8262D748 (177)
//   PhysicalTrafficManager::SetTrafficVehicleSlammed   @0x825EFDE8 (188)  export HOLE, closed here
//   PhysicalTrafficManager::TestForNearMissFreakOut    @0x82637A30 (148)
//   PhysicalTrafficManager::PhysicallyCrashTrafficCar  @0x825CAC10 ( 42)
//
// ⚠️ SetTrafficVehicleSlammed was an .ida-exports HOLE and is absent from progress/identity.json.
// Dumped headless from a COPY of the ARTIST .i64 during this round
// (scratchpad .../traffic_wave/wave3r2/B/holes/0x825EFDE8.txt: pseudocode + full asm + xrefs).
//
// ⚠️ NO FEB-2007 SOURCE for any of these (the leak has no physics-side PhysicalTrafficManager at
// all). ARTIST pseudocode + asm; DecFIGS DWARF for declaration shape.
//
// ⭐ THE STATE MACHINE, read off the three state stores:
//     E_TRAFFIC_TYPE_POTENTIAL(0) --Slammed/Checked--> E_TRAFFIC_TYPE_PHYSICAL(2)
//                                 --Crashing--------> E_TRAFFIC_TYPE_CRASHING(1)
//   and every arm drops the car's bit in mPotentialTrafficVehicles, i.e. "it is no longer merely
//   a candidate". SetTrafficVehicleSlammed acts ONLY from POTENTIAL; SetTrafficVehicleCrashing
//   no-ops when already CRASHING; SetTrafficVehicleChecked is unconditional.
//
// ⭐ RECOVERED CONSTANTS (all three were dyn-init .data splats reading ZERO in the image; taken
//    from their static-init thunks at 0x82C5BB88..0x82C5BD30 instead -- the standing
//    "dyn-init-thunk zeros" recipe):
//     unk_82FB8B50 = 1.0f / unk_82FB9350.x = 1.0f / 1000.0f  (the slam-magnitude scale)
//     unk_83017FE0 = 0.44704f                                (MPH -> m/s, already in the tree)
//     flt_820047C4 = 15.0f, flt_82001C98 = 1.0f, flt_82001DA0 = 0.5f
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)
#include <cmath>     // sqrtf ([T3-impulse])

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // [T3-*] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }
    bool s_bCrashReported   = false;
    bool s_bImpulseReported = false;

    // [T4-hit] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One latch PER OUTCOME so the
    // first crash does not mask the first check/slam/near-miss (the [T3-impulse] pair share one).
    bool s_bT4CrashingReported = false;
    bool s_bT4CheckedReported  = false;
    bool s_bT4SlammedReported  = false;
    bool s_bT4NearMissReported = false;

    f32 SpeedOf(const SimpleVehiclePhysics& lrBody)
    {
        const Vector3 lv = lrBody.GetLinearVelocity();
        return sqrtf(lv.x * lv.x + lv.y * lv.y + lv.z * lv.z);
    }

    f32 RelativeSpeed(const SimpleVehiclePhysics& lrA, const SimpleVehiclePhysics& lrB)
    {
        const Vector3 lvA = lrA.GetLinearVelocity();
        const Vector3 lvB = lrB.GetLinearVelocity();
        const f32 lfX = lvB.x - lvA.x;
        const f32 lfY = lvB.y - lvA.y;
        const f32 lfZ = lvB.z - lvA.z;
        return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
    }

    // MPH -> m/s (unk_83017FE0, the same splat SimpleVehiclePhysics.h:422 already names).
    const f32 KF_CRASHRESP_MPH_TO_MPS = 0.447039992f;

    // unk_82FB8B50, recovered from the static-init thunk @0x82C5BCF0: splat(1.0f / unk_82FB9350),
    // and unk_82FB9350 is splat(flt_82009E10 == 1000.0f). SetTrafficVehicleSlammed scales the
    // slam magnitude by this before clamping it to 1.
    const f32 KF_SLAM_MAGNITUDE_SCALE = 1.0f / 1000.0f;

    // flt_820047C4 -- the near-miss freak-out only fires on a traffic car that is itself moving.
    const f32 KF_NEAR_MISS_MIN_TRAFFIC_SPEED_MPH = 15.0f;

    // flt_82002138's sibling in TestForNearMissFreakOut's first gate: the race car has to be
    // doing at least this to make anything freak out (the `60.0 > speed` early-out).
    const f32 KF_NEAR_MISS_MIN_RACECAR_SPEED_MPH = 60.0f;

    // EntityId in this tree is the packed word (BrnCommonTypes.h:28), not CgsSceneManager's
    // class -- the owner/index extractions are the same `srwi 24` / `extrwi 14,8` pair every
    // sibling body in this directory spells out.
    inline u32 EntityOwnerOf(EntityId lEntityId) { return lEntityId.muValue >> 24; }
    inline u16 EntityIndexOf(EntityId lEntityId)
    {
        return static_cast<u16>((lEntityId.muValue >> 10) & 0x3FFFu);
    }

    // The X360 spells its sign selects as `vcmpgtfp / vcmpgefp / vsel / vsel`, which is exactly
    // signum with a hard 0.0 at zero. Never fold this to copysign (that would return +1 at 0).
    inline f32 SignumLane(f32 lfValue)
    {
        if (lfValue > 0.0f)  return 1.0f;
        if (lfValue < 0.0f)  return -1.0f;
        return 0.0f;
    }

    inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
    }

    inline Vector3 Sub3(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lvR;
        lvR.x = lrA.x - lrB.x; lvR.y = lrA.y - lrB.y;
        lvR.z = lrA.z - lrB.z; lvR.w = lrA.w - lrB.w;
        return lvR;
    }

    inline Vector3 Add3(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lvR;
        lvR.x = lrA.x + lrB.x; lvR.y = lrA.y + lrB.y;
        lvR.z = lrA.z + lrB.z; lvR.w = lrA.w + lrB.w;
        return lvR;
    }

    // The "which side of the traffic car did the race car come from" scalar both the CHECKED and
    // the SLAMMED arm compute, instruction-identical (0x8262D8C4..0x8262D918 /
    // 0x825EFF60..0x825EFFC0): project (raceCarPos + raceCarForward - trafficPos) onto the
    // traffic car's RIGHT axis and take its signum.
    inline f32 SteeringSideSignum(const SimpleVehiclePhysics& lrRaceCarBody,
                                  const SimpleVehiclePhysics& lrTrafficBody)
    {
        const Matrix44Affine lRaceCar = lrRaceCarBody.GetTransform();
        const Matrix44Affine lTraffic = lrTrafficBody.GetTransform();
        const Vector3 lvAhead = Add3(lRaceCar.wAxis, lRaceCar.zAxis);
        return SignumLane(Dot3(lTraffic.xAxis, Sub3(lvAhead, lTraffic.wAxis)));
    }
}

// -------------------------------------------------------------------------------------------
// PhysicallyCrashTrafficCar  @0x825CAC10 (42)  -- DWARF :451
//
// ⚠️ THE CONSOLE BODY REALLY DOES END HERE. It mints the crashing car's VolumeInstanceId
// (owner = TRAFFIC_VEHICLE, entity index = the traffic slot) and then RETURNS -- the tail is
// `bl SetEntityIDEntityIndex ; addi r1 ; b __restgprlr_29` with nothing in between. Whatever
// consumed that id (the deformation post the parameter name implies) is gone from this build;
// lpDeformationInterface is asserted non-null and never dereferenced. Reproduced as-is rather
// than "completed" -- the drop is the binary's, not this reconstruction's.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::PhysicallyCrashTrafficCar(
    u16 lu16TrafficCarIndex,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpDeformationInterface != 0, "NULL != lpDeformationInterface");
    CGS_ASSERT(lu16TrafficCarIndex < 20u, "lu16TrafficCarIndex < ku8TotalMaxNumPhysicalTraffic");
    (void)lpDeformationInterface;

    if (GetTrafficVehicle(static_cast<s32>(lu16TrafficCarIndex))->mePhysicalTrafficState
            == static_cast<u32>(E_TRAFFIC_TYPE_CRASHING))
    {
        CgsSceneManager::VolumeInstanceId lVolumeId;
        lVolumeId.muId = 0;
        lVolumeId.SetEntityIDOwner(static_cast<u8>(2));   // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        lVolumeId.SetEntityIDEntityIndex(lu16TrafficCarIndex);
        (void)lVolumeId;
    }
}

// -------------------------------------------------------------------------------------------
// SetTrafficVehicleCrashing  @0x82636E38 (229)  -- DWARF :174
//
// Spine, in asm order:
//   0x82636E9C..EF0  three non-null asserts (mgr / veh / deform)
//   0x82636F0C       GetTrafficVehicle(idx); EARLY OUT when mePhysicalTrafficState == CRASHING
//   0x82636F30..F60  both ids lifted to GLOBAL entity ids through maTrafficEntityIDs when their
//                    owner byte is TRAFFIC_VEHICLE (the bare `lwzx` the friend declaration exists
//                    for), with the crasher's K_INVALID_ENTITY_ID passed straight through
//   0x82636F74       "Traffic entity <id> crashed into itself" tripwire
//   0x82636FFC       state = CRASHING, then mpVehicleBody->SetCrashing() (vtable +8)
//   0x8263705C       mPotentialTrafficVehicles.UnSetBit(idx)     (this + 104576)
//   0x82637060..90   inherit the CRASHER's check owner when this car has none
//   0x826370A0       PhysicallyCrashTrafficCar
//   0x826370B0       AddCrashedTrafficEvent(VolumeInstanceId((u64)globalTrafficId << 32), crasher)
//   0x8263715C       the 32-byte game event (type 63) onto VehicleOutputInterface's queue
//   0x826371C0       the articulated other-half recursion -- GATED, see below
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::SetTrafficVehicleCrashing(
    EntityId lTrafficEntityID, EntityId lCrasherEntityID,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface");
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface");
    CGS_ASSERT(lpDeformationInterface != 0, "lpDeformationInterface");

    const u16 lu16TrafficIndex = EntityIndexOf(lTrafficEntityID);

    PhysicalTrafficVehicle* lpVehicle = GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    if (lpVehicle->mePhysicalTrafficState == static_cast<u32>(E_TRAFFIC_TYPE_CRASHING))
        return;

    const EntityId lGlobalTrafficID =
        (EntityOwnerOf(lTrafficEntityID) == 2u) ? maTrafficEntityIDs[lu16TrafficIndex] : lTrafficEntityID;

    EntityId lGlobalCrasherID = lCrasherEntityID;
    if (lCrasherEntityID.muValue != 0xFFFFFFFFu && EntityOwnerOf(lCrasherEntityID) == 2u)
        lGlobalCrasherID = maTrafficEntityIDs[EntityIndexOf(lCrasherEntityID)];

    // BrnPhysicalTrafficManager.cpp:2101 -- the console streams the id into the message.
    CGS_ASSERT(lGlobalTrafficID.muValue != lGlobalCrasherID.muValue,
               "Traffic entity  crashed into itself");

    lpVehicle = GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    SimpleVehiclePhysics* const lpBody = lpVehicle->mpVehicleBody;
    lpVehicle->mePhysicalTrafficState = static_cast<u32>(E_TRAFFIC_TYPE_CRASHING);
    lpBody->SetCrashing();                                     // vtable +8

    CGS_ASSERT(lu16TrafficIndex < 20u, "luIndex < NUMBITS");   // CgsBitArray.h:241
    mPotentialTrafficVehicles.UnSetBit(lu16TrafficIndex);

    if (EntityOwnerOf(lCrasherEntityID) == 2u)
    {
        PhysicalTrafficVehicle* const lpCrasher =
            GetTrafficVehicle(static_cast<s32>(EntityIndexOf(lCrasherEntityID)));
        if (lpCrasher->miCheckOwner != -1 && lpVehicle->miCheckOwner == -1)
        {
            // asm: the CRASHER's own check owner is what gets latched (r4 is loaded from
            // lpCrasher+0x33 and never rewritten before the call).
            lpVehicle->SetCheckOwner(static_cast<EActiveRaceCarIndex>(lpCrasher->miCheckOwner));
        }
    }

    PhysicallyCrashTrafficCar(lu16TrafficIndex, lpDeformationInterface);

    CgsSceneManager::VolumeInstanceId lTrafficVolumeID;
    lTrafficVolumeID.muId = static_cast<u64>(lGlobalTrafficID.muValue) << 32;
    lpManagerOutputInterface->AddCrashedTrafficEvent(lTrafficVolumeID, lGlobalCrasherID);

    lpVehicle = GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    CGS_ASSERT(lpVehicle != 0, "lpCrashedTrafficVehicle != NULL");
    CGS_ASSERT(lpVehicle->mCgsID != 0, "mCgsID != kCGSID_NULL");   // BrnPhysicalTrafficVehicle.h:438

    // The 32-byte game event the console pushes at VehicleOutputInterface + 0x65F0 with type 63.
    // Field seats are the six stores at 0x826370F4..0x82637158, verbatim.
    struct TrafficCrashedGameEvent : public CgsModule::Event
    {
        u32 muGlobalTrafficID;   // +0x00
        u32 muGlobalCrasherID;   // +0x04
        u64 mCgsID;              // +0x08
        u8  mu8Crashing;         // +0x10
        f32 mfMass;              // +0x14
        s32 miReserved;          // +0x18  (`li r11,-1`)
    };
    TrafficCrashedGameEvent lEvent;
    lEvent.muGlobalTrafficID = lGlobalTrafficID.muValue;
    lEvent.muGlobalCrasherID = lGlobalCrasherID.muValue;
    lEvent.mCgsID            = lpVehicle->mCgsID;
    lEvent.mu8Crashing       = 1;
    lEvent.mfMass            = lpVehicle->mpVehicleBody->GetMass().x;
    lEvent.miReserved        = -1;
    lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lEvent), 63, 32);

    // [T3-crash] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    if (!s_bCrashReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bCrashReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[T3-crash] SetTrafficVehicleCrashing traffic=" << static_cast<s32>(lu16TrafficIndex)
            << " globalTraffic=0x" << static_cast<s32>(lGlobalTrafficID.muValue)
            << " crasher=0x" << static_cast<s32>(lGlobalCrasherID.muValue)
            << " mass=" << lEvent.mfMass
            << "\n";
    }

    // [T4-hit] DIAG. NOT IN THE X360 BINARY. Per-outcome latch. DELETE-WHEN-STABLE.
    if (!s_bT4CrashingReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bT4CrashingReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[T4-hit] outcome=CRASHING trafficSlot=" << static_cast<s32>(lu16TrafficIndex)
            << " crasherOwner=" << static_cast<s32>(EntityOwnerOf(lCrasherEntityID))
            << " crasherIdx=" << static_cast<s32>(EntityIndexOf(lCrasherEntityID))
            << " trafficSpeed=" << SpeedOf(*lpVehicle->mpVehicleBody)
            << "\n";
    }

    // GATE: PhysicalTrafficManager::SetTrafficVehicleCrashing @0x826371C0 -- the articulated
    // other-half recursion. Blocker: ArticulatedJointPool::GetIndexOfOtherHalf @0x825D8490 has no
    // body (BrnArticulatedJointPool.h:57). DELETE-WHEN the trailer wave lands it.
    (void)lpVehicle->HasNonBrokenJoint();
}

// -------------------------------------------------------------------------------------------
// SetTrafficVehicleChecked  @0x8262D748 (177)  -- DWARF :190
//
// The response to E_RCTIR_CHECK_TRAFFIC. Unlike the SLAMMED arm this one is unconditional and it
// runs OnChecked first (which is what latches the checking race car and arms the full body's
// crash state). The posted event is a TrafficSlammedEvent with meCrashTrafficType == Slammed(3)
// -- that is what the asm stores (`li r9,3`), NOT Checked(1); the type word distinguishes the
// EVENT SHAPE, not the arm, and both arms use the same one.
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::SetTrafficVehicleChecked(
    EntityId lTrafficEntityID, EntityId lCrasherEntityID,
    const RaceCarPhysics* lpRaceCarPhysics,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    Vector3 lContactPointOnTraffic)
{
    (void)lpVehicleOutputInterface;
    (void)lpDeformationInterface;

    CGS_ASSERT(EntityOwnerOf(lTrafficEntityID) == 2u,
               "EntityOwnerOf(lTrafficEntityID) == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
    const u16 lu16TrafficIndex = EntityIndexOf(lTrafficEntityID);
    CGS_ASSERT(lu16TrafficIndex < 20u, "lu16TrafficIndex < ku8TotalMaxNumPhysicalTraffic");

    PhysicalTrafficVehicle* const lpVehicle = GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));

    lpVehicle->OnChecked(static_cast<EActiveRaceCarIndex>(EntityIndexOf(lCrasherEntityID)),
                         lpRaceCarPhysics, lContactPointOnTraffic);

    lpVehicle->mePhysicalTrafficState = static_cast<u32>(E_TRAFFIC_TYPE_PHYSICAL);

    CGS_ASSERT(lu16TrafficIndex < 20u, "luIndex < NUMBITS");   // CgsBitArray.h:241
    mPotentialTrafficVehicles.UnSetBit(lu16TrafficIndex);

    // GATE: PhysicalTrafficManager::SetTrafficVehicleChecked @0x8262D8B8 -- the articulated
    // other-half recursion. Blocker: ArticulatedJointPool::GetIndexOfOtherHalf @0x825D8490 unbodied.
    // DELETE-WHEN the trailer wave lands it.
    (void)lpVehicle->HasNonBrokenJoint();

    const TrafficPhysics* const lpFull = lpVehicle->GetFullTrafficPhysics();
    const f32 lfSteeringSide = SteeringSideSignum(*lpRaceCarPhysics, *lpFull);

    const EntityId lGlobalTrafficID =
        (EntityOwnerOf(lTrafficEntityID) == 2u) ? maTrafficEntityIDs[lu16TrafficIndex] : lTrafficEntityID;
    const EntityId lGlobalCrasherID =
        (EntityOwnerOf(lCrasherEntityID) == 2u) ? maTrafficEntityIDs[EntityIndexOf(lCrasherEntityID)]
                                            : lCrasherEntityID;

    // flt_82001DA0 == 0.5 on the steering lane; flt_82001C98 == 1.0 on the drive lane.
    const f32 lfDriveDirection = SignumLane(lpRaceCarPhysics->GetSpeedMPH().x * KF_CRASHRESP_MPH_TO_MPS) * 1.0f;

    lpManagerOutputInterface->AddSlammedTrafficEvent(lGlobalTrafficID, lGlobalCrasherID,
                                                     eCrashTrafficType_Slammed,
                                                     lfSteeringSide * 0.5f,
                                                     lfDriveDirection);

    // [T3-impulse] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. OnChecked's own crash-impulse
    // fold is a FLAGged omission in BrnPhysicalTrafficManager.cpp:695, so what this reports is the
    // observable that matters: the traffic body's linear velocity at the first handled response.
    if (!s_bImpulseReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bImpulseReported = true;
        const Vector3 lvVelocity = lpVehicle->mpVehicleBody->GetLinearVelocity();
        *CgsDev::Log::gpDebugPrint
            << "[T3-impulse] checked traffic=" << static_cast<s32>(lu16TrafficIndex)
            << " |v|=" << sqrtf(lvVelocity.x * lvVelocity.x + lvVelocity.y * lvVelocity.y
                                + lvVelocity.z * lvVelocity.z)
            << " side=" << lfSteeringSide << " drive=" << lfDriveDirection << "\n";
    }

    // [T4-hit] DIAG. NOT IN THE X360 BINARY. Per-outcome latch. DELETE-WHEN-STABLE.
    if (!s_bT4CheckedReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bT4CheckedReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[T4-hit] outcome=CHECKED trafficSlot=" << static_cast<s32>(lu16TrafficIndex)
            << " raceCarIdx=" << static_cast<s32>(EntityIndexOf(lCrasherEntityID))
            << " relSpeed=" << RelativeSpeed(*lpRaceCarPhysics, *lpVehicle->mpVehicleBody)
            << "\n";
    }
}

// -------------------------------------------------------------------------------------------
// SetTrafficVehicleSlammed  @0x825EFDE8 (188)  -- DWARF :202
//
// The response to E_RCTIR_SLAM_TRAFFIC. ⭐ THE WHOLE BODY IS GUARDED ON
// `mePhysicalTrafficState == E_TRAFFIC_TYPE_POTENTIAL` (`if (!*(result+32))` @0x825EFE94) -- a
// car that is already PHYSICAL or CRASHING is not re-slammed. No OnChecked here: a slam does not
// latch a check owner.
//
// The steering lane differs from the CHECKED arm: it is scaled by the SLAM MAGNITUDE, clamped to
// one -- `vmulfp v8, v127(magnitude), splat(unk_82FB8B50) ; vminfp v8, v8, 1.0` -- and the drive
// lane is halved (flt_82001DA0 == 0.5 into the drive slot rather than the steering slot).
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::SetTrafficVehicleSlammed(
    EntityId lTrafficEntityID, EntityId lCrasherEntityID,
    const SimpleVehiclePhysics* lpRaceCarPhysics,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    Vector3 lContactPointOnTraffic, VecFloat lvfSlamMagnitude)
{
    (void)lpVehicleOutputInterface;
    (void)lpDeformationInterface;
    (void)lContactPointOnTraffic;   // asm: v126 is saved on entry and only replayed into the
                                    // gated articulated recursion below.

    CGS_ASSERT(EntityOwnerOf(lTrafficEntityID) == 2u,
               "EntityOwnerOf(lTrafficEntityID) == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
    const u16 lu16TrafficIndex = EntityIndexOf(lTrafficEntityID);
    CGS_ASSERT(lu16TrafficIndex < 20u, "lu16TrafficIndex < ku8TotalMaxNumPhysicalTraffic");

    PhysicalTrafficVehicle* const lpVehicle = GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    if (lpVehicle->mePhysicalTrafficState != static_cast<u32>(E_TRAFFIC_TYPE_POTENTIAL))
        return;

    lpVehicle->mePhysicalTrafficState = static_cast<u32>(E_TRAFFIC_TYPE_PHYSICAL);

    CGS_ASSERT(lu16TrafficIndex < 20u, "luIndex < NUMBITS");   // CgsBitArray.h:241
    mPotentialTrafficVehicles.UnSetBit(lu16TrafficIndex);

    // GATE: PhysicalTrafficManager::SetTrafficVehicleSlammed @0x825EFF3C -- the articulated
    // other-half recursion. Blocker: ArticulatedJointPool::GetIndexOfOtherHalf @0x825D8490 unbodied.
    // DELETE-WHEN the trailer wave lands it.
    (void)lpVehicle->HasNonBrokenJoint();

    const TrafficPhysics* const lpFull = lpVehicle->GetFullTrafficPhysics();
    const f32 lfSteeringSide = SteeringSideSignum(*lpRaceCarPhysics, *lpFull);

    f32 lfMagnitude = lvfSlamMagnitude.x * KF_SLAM_MAGNITUDE_SCALE;
    if (lfMagnitude > 1.0f)
        lfMagnitude = 1.0f;                                   // vminfp against 1.0

    const EntityId lGlobalTrafficID =
        (EntityOwnerOf(lTrafficEntityID) == 2u) ? maTrafficEntityIDs[lu16TrafficIndex] : lTrafficEntityID;
    const EntityId lGlobalCrasherID =
        (EntityOwnerOf(lCrasherEntityID) == 2u) ? maTrafficEntityIDs[EntityIndexOf(lCrasherEntityID)]
                                            : lCrasherEntityID;

    const f32 lfDriveDirection = SignumLane(lpRaceCarPhysics->GetSpeedMPH().x * KF_CRASHRESP_MPH_TO_MPS) * 0.5f;

    lpManagerOutputInterface->AddSlammedTrafficEvent(lGlobalTrafficID, lGlobalCrasherID,
                                                     eCrashTrafficType_Slammed,
                                                     lfSteeringSide * lfMagnitude,
                                                     lfDriveDirection);

    // [T3-impulse] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    if (!s_bImpulseReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bImpulseReported = true;
        const Vector3 lvVelocity = lpVehicle->mpVehicleBody->GetLinearVelocity();
        *CgsDev::Log::gpDebugPrint
            << "[T3-impulse] slammed traffic=" << static_cast<s32>(lu16TrafficIndex)
            << " |v|=" << sqrtf(lvVelocity.x * lvVelocity.x + lvVelocity.y * lvVelocity.y
                                + lvVelocity.z * lvVelocity.z)
            << " mag=" << lfMagnitude << " side=" << lfSteeringSide << "\n";
    }

    // [T4-hit] DIAG. NOT IN THE X360 BINARY. Per-outcome latch. DELETE-WHEN-STABLE.
    if (!s_bT4SlammedReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bT4SlammedReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[T4-hit] outcome=SLAMMED trafficSlot=" << static_cast<s32>(lu16TrafficIndex)
            << " raceCarIdx=" << static_cast<s32>(EntityIndexOf(lCrasherEntityID))
            << " relSpeed=" << RelativeSpeed(*lpRaceCarPhysics, *lpVehicle->mpVehicleBody)
            << " slamMag=" << lfMagnitude
            << "\n";
    }
}

// -------------------------------------------------------------------------------------------
// TestForNearMissFreakOut  @0x82637A30 (148)  -- DWARF :386
//
// The arm taken when PredictCarCarIntersection says the two cars will NOT actually meet: a race
// car doing >= 60 mph brushing past a traffic car doing >= 15 mph makes that car crash and freak
// out. Gate ladder, in asm order:
//   0x82637AA0/AC0  both contact owners asserted (A == RACECAR, B == TRAFFIC_VEHICLE)
//   0x82637AF0      `60.0 > raceCar.mfSpeedMPH` -> RETURN
//   0x82637B30      the traffic car must be FULLY physical (SIMPLE -> RETURN)
//   0x82637B70      already crashing -> RETURN
//   0x82637BAC      `15.0 > traffic.mfSpeedMPH` -> RETURN
//   0x82637BCC      when the race car is NOT drifting (mu8DriftState == 0), the contact point
//                   must be in FRONT of the traffic car (dot with its forward axis >= 0)
//   0x82637C28      SetTrafficVehicleCrashing, then TrafficPhysics::SetFreakedOut(side, relSpeedSq)
// -------------------------------------------------------------------------------------------
void PhysicalTrafficManager::TestForNearMissFreakOut(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    EntityId lTrafficPhysicsEntityID, EntityId lRaceCarPhysicsEntityID,
    const RaceCarPhysics* lpRaceCarPhysics,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lContact.muVolumeInstanceIdA.GetEntityIDOwner() == 1u,
               "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
    CGS_ASSERT(lContact.muVolumeInstanceIdB.GetEntityIDOwner() == 2u,
               "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

    if (KF_NEAR_MISS_MIN_RACECAR_SPEED_MPH > lpRaceCarPhysics->GetSpeedMPH().x)
        return;

    PhysicalTrafficVehicle* const lpVehicle =
        GetTrafficVehicle(static_cast<s32>(EntityIndexOf(lTrafficPhysicsEntityID)));

    CGS_ASSERT(lpVehicle->mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
    if (lpVehicle->mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE)
        return;

    TrafficPhysics* const lpFull = lpVehicle->GetFullTrafficPhysics();
    if (lpFull->IsCrashing())
        return;
    if (KF_NEAR_MISS_MIN_TRAFFIC_SPEED_MPH > lpFull->GetSpeedMPH().x)
        return;

    const Matrix44Affine lTraffic = lpFull->GetTransform();
    const Vector3 lvContactFromTraffic = Sub3(lContact.mPointOnB, lTraffic.wAxis);

    // `lbz r8, 0x1352(r27)` == RaceCarPhysics::mu8DriftState. A drifting car skips the
    // in-front test entirely (a drift brushes past sideways).
    if (lpRaceCarPhysics->mu8DriftState == 0)
    {
        if (Dot3(lvContactFromTraffic, lTraffic.zAxis) < 0.0f)
            return;
    }

    SetTrafficVehicleCrashing(lTrafficPhysicsEntityID, lRaceCarPhysicsEntityID,
                              lpManagerOutputInterface, lpVehicleOutputInterface,
                              lpDeformationInterface);

    // rw::math::fpu::Sgn on the lateral offset, then the SQUARED relative speed. Both land in
    // the same stack slot on the console (`stvx128 v1 ; lfs f1` then `vmsum3fp ; stvx128 ; lfs f2`).
    const f32 lfSide = SignumLane(Dot3(lvContactFromTraffic, lTraffic.xAxis));
    const Vector3 lvRelative = Sub3(lpFull->GetLinearVelocity(), lpRaceCarPhysics->GetLinearVelocity());
    const f32 lfSeverity = Dot3(lvRelative, lvRelative);

    lpFull->SetFreakedOut(lfSide, lfSeverity);

    // [T4-hit] DIAG. NOT IN THE X360 BINARY. Per-outcome latch. DELETE-WHEN-STABLE. lfSeverity is
    // the SQUARED relative speed the console feeds SetFreakedOut, reported as-is.
    if (!s_bT4NearMissReported && TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bT4NearMissReported = true;
        *CgsDev::Log::gpDebugPrint
            << "[T4-hit] outcome=NEARMISS trafficSlot="
            << static_cast<s32>(EntityIndexOf(lTrafficPhysicsEntityID))
            << " raceCarIdx=" << static_cast<s32>(EntityIndexOf(lRaceCarPhysicsEntityID))
            << " relSpeedSq=" << lfSeverity
            << " side=" << lfSide
            << "\n";
    }
}

}   // namespace Vehicle
}   // namespace BrnPhysics
