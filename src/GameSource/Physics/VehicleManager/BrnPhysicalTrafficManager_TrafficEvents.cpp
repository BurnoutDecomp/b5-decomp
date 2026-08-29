// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_TrafficEvents.cpp
//
// THE SET-TRAFFIC-CRASHING EVENT DRAIN. ProcessTrafficEvents was a named conductor gate
// (BrnVehicleManager_MaintenanceEvents.cpp) whose banner said it "dispatches the whole crash
// sub-tree"; it does, and the gate really was firing -- `conductor gate:
// PhysicalTrafficManager::ProcessTrafficEvents @0x82643FB0 (72) inert` is in every BrnGame.log.
// With it inert the ONLY consumer of VehicleInputInterface::mSetTrafficCrashingEventQueue did
// not exist, so every event BrnTraffic::TrafficEntityModule posts through
// VehicleInputInterface::SetTrafficCrashing / SetTrafficNotCrashing was written into a 25-slot
// queue that nothing ever read.
//
//   PhysicalTrafficManager::ProcessTrafficEvents             @0x82643FB0 ( 72)  DWARF :124
//   PhysicalTrafficManager::ProcessSetTrafficCrashingEvents  @0x82640E20 (142)  DWARF :647
//   PhysicalTrafficManager::ProcessUpdateNetworkTrafficEvents@0x8262CED0 (145)  DWARF :656
//   PhysicalTrafficManager::SetTrafficVehicleNotCrashing     @0x825CAB48 ( 50)  DWARF :179
//   PhysicalTrafficManager::UpdateNetworkTrafficVehicle      @0x8261CBD0 ( 50)  DWARF :662
//                                                            -- NAMED GATE, see its seat
//
// No Feb-2007 source for any of these. ARTIST pseudocode + asm; DecFIGS DWARF for declaration
// shape. Every signature below is read off the PROLOGUE, not the pseudocode.
//
// ---------------------------------------------------------------------------------------------
// THE PIPELINE THIS RE-CONNECTS (both halves already existed; only the middle was missing):
//
//   BrnTraffic::TrafficEntityModule
//        -> VehicleInputInterface::SetTrafficCrashing(globalTrafficEntityId)   @0x8271D138
//             -> BaseEventQueue<SetTrafficCrashingEvent>::AddEvent             @0x82719EA8
//   ......................... mSetTrafficCrashingEventQueue ..........................
//        -> PhysicalTrafficManager::ProcessTrafficEvents                       (THIS FILE)
//             -> ProcessSetTrafficCrashingEvents                               (THIS FILE)
//                  -> SetTrafficVehicleCrashing      @0x82636E38  (already bodied)
//                  -> SetTrafficVehicleNotCrashing   @0x825CAB48  (THIS FILE, was absent)
//
// ⭐ THE TWO ID SPACES. The queue carries a GLOBAL traffic entity id (owner 2, index in
// [0,600)); SetTrafficVehicleCrashing takes a PHYSICS traffic id (owner 2, index in [0,20)).
// The console converts between them INLINE at both drain sites with the same five steps -- the
// bound tripwire, mu8GlobalToPhysicalEntityIndexMap[], the KU8_INVALID_MAP(127) miss, the
// CgsEntityId.h:116 index assert, and the (idx << 10) | (TRAFFIC << 24) repack. It is the same
// shape ProcessAddAirRamEvent already carries in BrnPhysicalTrafficManager.cpp; both are
// spelled here BY NAME rather than through GetPhysicsEntityId(), because that accessor fires a
// [0,20) assert the console does NOT fire at these two sites.
//
// ⚠️ WHAT THE CONSOLE SEEDS AND NEVER READS. Both drains load the K_INVALID_ENTITY_ID global
// (dword_82F2A3A4) into the physics-id register BEFORE the map lookup and overwrite it on a
// hit; on a miss the boolean is false and the register is dead. Reproduced as an initialised
// local so the `lPhysicsId.IsValid()` tripwire the console emits on the HIT path still has
// something to test -- same treatment ProcessAddAirRamEvent documents.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG / BRN_TRAFFIC_EVENTS_CONTROL)

namespace
{
    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    // ⭐ THE ONE-LINE CONTROL. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    // Restores the pre-2026-08-29 behaviour of this file EXACTLY -- ProcessTrafficEvents returns
    // without dispatching, which is what the named gate did -- so the control and the treatment
    // come out of ONE binary and differ in one branch. Without it a "traffic now crashes" claim
    // rests on comparing two different builds.
    bool TrafficEventsControl()
    {
        static const bool sbControl = (getenv("BRN_TRAFFIC_EVENTS_CONTROL") != 0);
        return sbControl;
    }

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    void LogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

namespace BrnPhysics
{
namespace Vehicle
{

// =================================================================================================
// ProcessTrafficEvents @0x82643FB0 (72)  -- DWARF :124, asserts BrnPhysicalTrafficManager.cpp
// :413..:417
//
// Five null tripwires in parameter order, then TWO unconditional calls with the SAME six
// registers re-materialised for each (0x82644090..0x826440A8 and 0x826440AC..0x826440C4). No
// branch, no local state, no return value used by the caller. That is the whole function.
//
// ⚠️ THE REGISTER MAP IS FROM THE PROLOGUE, not from Hex-Rays' `int a1..a6`:
//     r3 this | r4 lpInputInterface | r5 lpOutputRequestInterface | r6 lpManagerOutputInterface
//     r7 lpVehicleOutputInterface   | r8 lpDeformationInterface
// =================================================================================================
void PhysicalTrafficManager::ProcessTrafficEvents(
    const VehicleInputInterface* lpInputInterface,
    VehicleOutputRequestInterface* lpOutputRequestInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpInputInterface         != 0, "lpInputInterface != NULL");          // :413
    CGS_ASSERT(lpOutputRequestInterface != 0, "lpOutputInterface != NULL");         // :414
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface != NULL");  // :415
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface != NULL");  // :416
    CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");    // :417

    // CONTROL ARM. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. See TrafficEventsControl().
    if (TrafficEventsControl())
    {
        static bool s_bControlLogged = false;
        LogOnce(s_bControlLogged,
                "[T6-tevt] CONTROL: BRN_TRAFFIC_EVENTS_CONTROL=1 -- ProcessTrafficEvents "
                "dispatches NOTHING this run (the pre-2026-08-29 gate) [DELETE-WHEN-STABLE]\n");
        return;
    }

    ProcessSetTrafficCrashingEvents(lpInputInterface, lpOutputRequestInterface,
                                    lpManagerOutputInterface, lpVehicleOutputInterface,
                                    lpDeformationInterface);

    ProcessUpdateNetworkTrafficEvents(lpInputInterface, lpOutputRequestInterface,
                                      lpManagerOutputInterface, lpVehicleOutputInterface,
                                      lpDeformationInterface);
}

// =================================================================================================
// ProcessSetTrafficCrashingEvents @0x82640E20 (142)  -- DWARF :647, asserts :454..:458 and :474
//
// The drain, in asm order:
//   0x82640E2C..E48   the six parameter registers are homed; r31 (lpOutputRequestInterface) is
//                     ASSERTED AND THEN NEVER READ AGAIN. That is not an oversight to tidy up --
//                     the parameter exists so the signature matches its sibling drain.
//   0x82640F04..F18   r16 = lpInputInterface + 0x22048 (the queue's miLength) and the whole loop
//                     is skipped when it is <= 0. 0x22040 is mSetTrafficCrashingEventQueue --
//                     reached here by name through the DWARF-attested GetSetTrafficCrashingEvents.
//   0x82640F64        BaseEventQueue<SetTrafficCrashingEvent>::GetEvent(i)   (sub_825BBDE8)
//   0x82640F70..F80   global index = (mEntityId >> 10) & 0x3FFF, bound-tripwired against
//                     sizeof(mu8GlobalToPhysicalEntityIndexMap) (== 600, `cmplwi 0x258`)
//   0x82640FA4..FD8   map lookup; 0x7F == KU8_INVALID_MAP == this global id owns no physical
//                     traffic slot, so the event is DROPPED. A hit asserts the index fits
//                     KU_NUM_BITS_FOR_ENTITY_NUM (CgsEntityId.h:116) and repacks
//                     (physIdx << 10) | 0x02000000.
//   0x82641010        mbCrashing (`lbz r11, 4(r29)`) picks the arm.
//   0x82641034        SetTrafficVehicleCrashing(physicsId, K_INVALID_ENTITY_ID, mgrOut, vehOut,
//                                               deform)  -- the crasher is DELIBERATELY invalid:
//                     an event-driven crash has no contact partner, and the callee's
//                     "crashed into itself" tripwire and its check-owner inheritance arm both
//                     test for exactly that.
//   0x8264103C        SetTrafficVehicleNotCrashing(physicsId)  -- TWO arguments. r5 is loaded
//                     before the branch and is dead on this arm (Hex-Rays prints a third `-1`
//                     parameter that the callee's own prologue does not have).
//   0x82641040        the loop RE-READS miLength every iteration.
// =================================================================================================
void PhysicalTrafficManager::ProcessSetTrafficCrashingEvents(
    const VehicleInputInterface* lpInputInterface,
    VehicleOutputRequestInterface* lpOutputRequestInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpInputInterface         != 0, "lpInputInterface != NULL");          // :454
    CGS_ASSERT(lpOutputRequestInterface != 0, "lpOutputInterface != NULL");         // :455
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface != NULL");  // :456
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface != NULL");  // :457
    CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");    // :458

    const VehicleInputInterface::SetTrafficCrashingEventQueue* const lpQueue =
        lpInputInterface->GetSetTrafficCrashingEvents();

    for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
    {
        const SetTrafficCrashingEvent& lrEvent = lpQueue->GetEvent(liEvent);

        // The console seeds the physics id with K_INVALID_ENTITY_ID and only overwrites it on a
        // map hit (0x82640F6C `lwz r30, dword_82F2A3A4`).
        // 0xFFFFFFFF == CgsEntityId.h's KU_INVALID_ENTITY_ID (private there; the physics
        // traffic code carries the plain `struct EntityId { u32 muValue; }`, so the sentinel is
        // spelled the same way the sibling crash latch spells it).
        EntityId lPhysicsId;
        lPhysicsId.muValue = 0xFFFFFFFFu;

        const u32 luGlobalIndex = (lrEvent.mEntityId.muValue >> 10) & 0x3FFFu;
        CGS_ASSERT(luGlobalIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap),
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // h:944

        // HOST DIVERGENCE, flagged: for an index >= 600 the console runs the map read anyway (an
        // OOB read feeding the sentinel test); the host guards it. Same accepted divergence as
        // ProcessAddAirRamEvent @0x8261DC08 in BrnPhysicalTrafficManager.cpp.
        if (luGlobalIndex >= sizeof(mu8GlobalToPhysicalEntityIndexMap))
            continue;

        const u8 lu8PhysicalIndex = mu8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
        if (lu8PhysicalIndex == KU8_INVALID_MAP)
            continue;   // this global traffic entity owns no physical slot -- drop the event

        CGS_ASSERT(static_cast<u32>(lu8PhysicalIndex) < (1u << 14),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");   // CgsEntityId.h:116
        lPhysicsId.muValue =
            (static_cast<u32>(lu8PhysicalIndex) << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);

        CGS_ASSERT(lPhysicsId.muValue != 0xFFFFFFFFu, "lPhysicsId.IsValid()");   // :474

        if (lrEvent.mbCrashing)
        {
            EntityId lCrasherId;
            lCrasherId.muValue = 0xFFFFFFFFu;   // `li r18, -1` at 0x82640F44
            SetTrafficVehicleCrashing(lPhysicsId, lCrasherId, lpManagerOutputInterface,
                                      lpVehicleOutputInterface, lpDeformationInterface);
        }
        else
        {
            SetTrafficVehicleNotCrashing(lPhysicsId);
        }

        // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One line per drained event, so a
        // zero here is distinguishable from "the drain never ran" (the [T6-tevt] armed line).
        if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T6-tevt] drained crashing=" << (lrEvent.mbCrashing ? 1 : 0)
                << " globalIdx=" << static_cast<s32>(luGlobalIndex)
                << " physSlot=" << static_cast<s32>(lu8PhysicalIndex)
                << " [DELETE-WHEN-STABLE]\n";
        }
    }
}

// =================================================================================================
// ProcessUpdateNetworkTrafficEvents @0x8262CED0 (145)  -- DWARF :656, asserts :517..:521
//
// Byte-for-byte the same drain as its sibling above -- five null tripwires, the queue at
// lpInputInterface + 0x22278 (mUpdateNetworkTrafficEventQueue, count at +0x22280), the same
// global->physical map conversion, the same KU8_INVALID_MAP drop -- with two differences:
//   * it does NOT emit the `lPhysicsId.IsValid()` tripwire the crashing drain has;
//   * the hit calls UpdateNetworkTrafficVehicle(&event, physicsId).
// Offline the queue is empty by construction: its only producer is
// VehicleInputInterface::UpdateNetworkTraffic, driven from the network traffic bridge.
// =================================================================================================
void PhysicalTrafficManager::ProcessUpdateNetworkTrafficEvents(
    const VehicleInputInterface* lpInputInterface,
    VehicleOutputRequestInterface* lpOutputRequestInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpInputInterface         != 0, "lpInputInterface != NULL");          // :517
    CGS_ASSERT(lpOutputRequestInterface != 0, "lpOutputInterface != NULL");         // :518
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpManagerOutputInterface != NULL");  // :519
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface != NULL");  // :520
    CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");    // :521

    const VehicleInputInterface::UpdateNetworkTrafficEventQueue* const lpQueue =
        lpInputInterface->GetUpdateNetworkTrafficEvents();

    for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
    {
        const UpdateNetworkTrafficEvent& lrEvent = lpQueue->GetEvent(liEvent);

        // `ld r11,0(r3) ; srdi` -- the VolumeInstanceId's embedded entity word.
        const u32 luEntityWord = static_cast<u32>(
            lrEvent.mVolumeInstanceID.muId
            >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);

        const u32 luGlobalIndex = (luEntityWord >> 10) & 0x3FFFu;
        CGS_ASSERT(luGlobalIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap),
                   "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // h:944

        // HOST DIVERGENCE, flagged -- see the sibling drain above.
        if (luGlobalIndex >= sizeof(mu8GlobalToPhysicalEntityIndexMap))
            continue;

        const u8 lu8PhysicalIndex = mu8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
        if (lu8PhysicalIndex == KU8_INVALID_MAP)
            continue;

        CGS_ASSERT(static_cast<u32>(lu8PhysicalIndex) < (1u << 14),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");   // CgsEntityId.h:116

        EntityId lPhysicsId;
        lPhysicsId.muValue =
            (static_cast<u32>(lu8PhysicalIndex) << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);

        UpdateNetworkTrafficVehicle(&lrEvent, lPhysicsId);
    }
}

// =================================================================================================
// GATE PhysicalTrafficManager::UpdateNetworkTrafficVehicle @0x8261CBD0 (50) -- DWARF :662
//    blocker: its two working callees have no bodies anywhere in the tree --
//      BrnPhysics::Vehicle::PhysicalTrafficVehicle::GetFullTraffic  (the +0x1C body as a
//        TrafficPhysics; distinct from the bodied GetFullTrafficPhysics @0x825C0148) and
//      BrnPhysics::Vehicle::VehicleDriver::StartCatchupInterpolation (DECLARED in
//        BrnVehicleDriver.h:64, no definition -- the whole catch-up interpolation family is
//        unreconstructed).
//    ⭐ IT IS UNREACHABLE OFFLINE AND THAT IS STRUCTURAL, NOT AN ASSUMPTION: the only producer
//    of the queue that feeds it is VehicleInputInterface::UpdateNetworkTraffic, so
//    ProcessUpdateNetworkTrafficEvents' loop above executes zero iterations in single player.
//    Gating it here is therefore strictly better than trap-stubbing the two callees, which
//    would put a trap on the ONLY path a future network wave has to walk.
//    DELETE-WHEN VehicleDriver::StartCatchupInterpolation lands.
//
//    The body it will get, off the asm (0x8261CBD0..0x8261CC94), for whoever lands it:
//      assert lTrafficPhysicsId.IsValid()                                (:560)
//      idx = lTrafficPhysicsId.GetEntityIndex()
//      lpVehicle = GetTrafficVehicle(idx)
//      assert lpVehicle->mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT (BrnPhysicalTrafficVehicle.h:382)
//      if (lpVehicle->mu8PhysicalType == FULL)                           -- NOT an assert here
//          GetTrafficDriver(idx)->StartCatchupInterpolation(
//              lpVehicle->GetFullTraffic(),
//              lpEvent->mTransform,                 // the event + 16, i.e. past mVolumeInstanceID
//              <v1 = full+0x50>, <v2 = full+0x60>,  // `lvx128 v2,r31,96 / lvx128 v1,r31,80`
//              false)                               // `li r7,0`
// =================================================================================================
void PhysicalTrafficManager::UpdateNetworkTrafficVehicle(const UpdateNetworkTrafficEvent*,
                                                         EntityId)
{
    static bool s_bLogged = false;
    LogOnce(s_bLogged,
            "conductor gate: PhysicalTrafficManager::UpdateNetworkTrafficVehicle @0x8261CBD0 "
            "(50) inert -- VehicleDriver::StartCatchupInterpolation has no body; NETWORK-ONLY, "
            "the queue that reaches it has no offline producer [FLAG PC boot gate]\n");
}

// =================================================================================================
// SetTrafficVehicleNotCrashing @0x825CAB48 (50)  -- DWARF :179, assert BrnPhysicalTrafficManager
// .cpp:2196
//
// The exact inverse of the crash latch, and MUCH smaller than it: no event is posted, no bit is
// touched, no deformation is involved.
//   0x825CAB5C  extrwi r30, r4, 14,8            == lTrafficPhysicsId.GetEntityIndex()
//   0x825CAB68  GetTrafficVehicle(idx)
//   0x825CAB6C  lwz r11, 0x20(r3) ; cmpwi 1     == EARLY OUT unless state == CRASHING. Note the
//               console re-fetches the vehicle pointer after the compare rather than reusing
//               r3 (0x825CAB80) -- reproduced, it is the same object either way.
//   0x825CAB88  lbz r31, 0x32(r30)              == mu8PhysicalType, two tripwires on it
//   0x825CABDC  li r11,2 ; lwz r3,0x1C(r30) ; stw r11,0x20(r30)
//                                               == state = E_TRAFFIC_TYPE_PHYSICAL, THEN the
//                                                  body pointer is loaded (order irrelevant)
//   0x825CABE8  lwz r11,0(r3) ; lwz r11,4(r11) ; bctrl
//                                               == vtable slot 1. The declared virtual order is
//                                                  GetSteeringAngle(0) / ClearCrashing(1) /
//                                                  SetCrashing(2), and the crash latch's own
//                                                  banner pins SetCrashing at "+8" -- so slot 1
//                                                  is ClearCrashing, the exact inverse.
// =================================================================================================
void PhysicalTrafficManager::SetTrafficVehicleNotCrashing(EntityId lTrafficPhysicsId)
{
    const u16 lu16TrafficIndex =
        static_cast<u16>((lTrafficPhysicsId.muValue >> 10) & 0x3FFFu);

    if (GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex))->mePhysicalTrafficState
            != static_cast<u32>(E_TRAFFIC_TYPE_CRASHING))
    {
        return;
    }

    PhysicalTrafficVehicle* const lpVehicle =
        GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));

    CGS_ASSERT(static_cast<u32>(lpVehicle->mu8PhysicalType)
                   < static_cast<u32>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT),
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");            // BrnPhysicalTrafficVehicle.h:382
    CGS_ASSERT(static_cast<u32>(lpVehicle->mu8PhysicalType)
                   == static_cast<u32>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL),
               "lpTrafficVehicle->IsFullyPhysical()");               // :2196

    SimpleVehiclePhysics* const lpBody = lpVehicle->mpVehicleBody;
    lpVehicle->mePhysicalTrafficState = static_cast<u32>(E_TRAFFIC_TYPE_PHYSICAL);
    lpBody->ClearCrashing();                                          // vtable +4

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[T6-tevt] UNCRASH slot=" << static_cast<s32>(lu16TrafficIndex)
            << " [DELETE-WHEN-STABLE]\n";
    }
}

}   // namespace Vehicle
}   // namespace BrnPhysics
