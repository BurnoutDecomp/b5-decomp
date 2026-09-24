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
//                                                            (bodied 2026-09-23, G34-D1)
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
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                   // FindModelIndexByEntityID / GetDeformableObject
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // ClearStoredContacts
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG / BRN_TRAFFIC_EVENTS_CONTROL / BRN_NETCRASH_DIAG)

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

    // FLAG PC witness (crash parity FX-NETCRASH; NOT in the X360 binary). BRN_NETCRASH_DIAG, default
    // off: the network crashing-traffic chain's [netcrash] lines (UpdateNetworkTrafficVehicle,
    // ClearSnappedNetworkTrafficContacts), each hard-capped.
    bool NetCrashDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_NETCRASH_DIAG") != 0);
        return sbEnabled;
    }
    const s32 KI_NETCRASH_DIAG_MAX_LINES = 40;
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

    // FLAG PC witness (BRN_NETCRASH_DIAG, capped; NOT console code): once per frame that carried
    // network-traffic updates, how many of them named a vehicle that is physical here.
    if (NetCrashDiagEnabled() && CgsDev::Log::gpDebugPrint != 0 && lpQueue->GetLength() > 0)
    {
        static s32 siSummaryLines = 0;
        if (siSummaryLines < KI_NETCRASH_DIAG_MAX_LINES)
        {
            ++siSummaryLines;
            s32 liMapped = 0;
            for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
            {
                const u32 luIndex = static_cast<u32>(lpQueue->GetEvent(liEvent).mVolumeInstanceID.muId
                                                     >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);
                const u32 luGlobal = (luIndex >> 10) & 0x3FFFu;
                if (luGlobal < sizeof(mu8GlobalToPhysicalEntityIndexMap)
                    && mu8GlobalToPhysicalEntityIndexMap[luGlobal] != KU8_INVALID_MAP)
                    ++liMapped;
            }
            *CgsDev::Log::gpDebugPrint
                << "[netcrash] ProcessUpdateNetworkTrafficEvents events=" << lpQueue->GetLength()
                << " physical=" << liMapped << " [FLAG PC witness]\n";
        }
    }

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

        // FLAG PC witness input (BRN_NETCRASH_DIAG; NOT console code): the FULL body's position
        // before this event's catch-up is armed.
        static s32 siWitnessLines = 0;
        PhysicalTrafficVehicle* lpWitnessVehicle = 0;
        Vector3 lvBefore = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (NetCrashDiagEnabled() && CgsDev::Log::gpDebugPrint != 0 && siWitnessLines < KI_NETCRASH_DIAG_MAX_LINES)
        {
            lpWitnessVehicle = GetTrafficVehicle(static_cast<s32>(lu8PhysicalIndex));
            if (lpWitnessVehicle->mu8PhysicalType
                    != static_cast<u8>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL))
                lpWitnessVehicle = 0;
            else
                lvBefore = lpWitnessVehicle->GetFullTrafficPhysics()->GetTransform().wAxis;
        }

        UpdateNetworkTrafficVehicle(&lrEvent, lPhysicsId);

        // FLAG PC witness (BRN_NETCRASH_DIAG, capped; NOT console code, crash parity FX-NETCRASH): the
        // network traffic car's body position before and after UpdateNetworkTrafficVehicle armed its
        // catch-up. A snap (> 10 m, KF_MAX_VEHICLE_INTERP_DIST_SQ) moves it here; otherwise the armed
        // slerp steps move it over the next frames (VehicleDriver::UpdateVehicle), which the next
        // line's `before` shows.
        if (lpWitnessVehicle != 0)
        {
            ++siWitnessLines;
            const VehicleDriver* const lpDriver = GetTrafficDriver(static_cast<s32>(lu8PhysicalIndex));
            const Vector3 lvAfter = lpWitnessVehicle->GetFullTrafficPhysics()->GetTransform().wAxis;
            *CgsDev::Log::gpDebugPrint
                << "[netcrash] UpdateNetworkTrafficVehicle slot=" << static_cast<s32>(lu8PhysicalIndex)
                << " global=" << static_cast<s32>(luGlobalIndex)
                << " before=(" << lvBefore.x << ", " << lvBefore.y << ", " << lvBefore.z << ")"
                << " target=(" << lrEvent.mTransform.wAxis.x << ", " << lrEvent.mTransform.wAxis.y
                << ", " << lrEvent.mTransform.wAxis.z << ")"
                << " after=(" << lvAfter.x << ", " << lvAfter.y << ", " << lvAfter.z << ")"
                << " snapped=" << (lpDriver->SnappedThisFrame() ? 1 : 0)
                << " steps=" << static_cast<s32>(lpDriver->GetNumInterpSteps())
                << " [FLAG PC witness]\n";
        }
    }
}

// =================================================================================================
// UpdateNetworkTrafficVehicle @0x8261CBD0 (50) -- DWARF :662. BODIED 2026-09-23 (G34-D1); it was a
// named gate waiting on VehicleDriver::StartCatchupInterpolation, which FX-VMNET bodied (489966db).
//
// Hands a network traffic car's authoritative transform to its driver's catch-up interpolation.
// Straight off the asm:
//   0x8261CBE8  `cmpwi r31(id), -1`            assert "lTrafficPhysicsId.IsValid()" (.cpp 0x230 == 560)
//   0x8261CC10  `extrwi r30, r31, 14,8`         idx == lTrafficPhysicsId.GetEntityIndex()
//   0x8261CC1C  GetTrafficVehicle(idx)          @0x825B4880 (*(this+0x194B4) + idx*64)
//   0x8261CC24  `lbz r29, 0x32`                 mu8PhysicalType, assert < COUNT
//                                               (BrnPhysicalTrafficVehicle.h 0x17E == 382)
//   0x8261CC50  `cmpwi r29, 0 ; bne out`        only a FULL car -- NOT an assert here
//   0x8261CC5C  GetFullTraffic                  @0x825C0148 == PhysicalTrafficVehicle::
//                                               GetFullTrafficPhysics (same address)
//   0x8261CC6C  GetTrafficDriver(idx)           @0x825B4900 (*(this+0x194B0) + idx*0xE0)
//   0x8261CC70..0x8261CC8C  r4 = the full body, r5 = event + 0x10 (mTransform, past the 16-byte
//               mVolumeInstanceID), `li r6, 0` (lbSnap -- the old banner's `li r7,0` was wrong),
//               v1 = [full+0x50] (mLinearVelocity), v2 = [full+0x60] (mAngularVelocity) --
//               vmx128.py on the raw lvx128 words: vD 1 and 2 -- then
//               bl VehicleDriver::StartCatchupInterpolation @0x825FED30.
// The car's OWN current velocities are passed: the event carries only a transform.
// Network-only: the queue's sole console producer is CrashModule::HandleNetworkCrashingTraffic
// @0x827CB788 (online; bodied 2026-09-24, b5 6c535ee9), so this runs zero times offline.
// =================================================================================================
void PhysicalTrafficManager::UpdateNetworkTrafficVehicle(const UpdateNetworkTrafficEvent* lpEvent,
                                                         EntityId lTrafficPhysicsId)
{
    CGS_ASSERT(lTrafficPhysicsId.muValue != 0xFFFFFFFFu, "lTrafficPhysicsId.IsValid()");   // .cpp:560

    const s32 liTrafficIndex = static_cast<s32>((lTrafficPhysicsId.muValue >> 10) & 0x3FFFu);
    PhysicalTrafficVehicle* const lpTrafficVehicle = GetTrafficVehicle(liTrafficIndex);

    const u8 lu8PhysicalType = lpTrafficVehicle->mu8PhysicalType;
    CGS_ASSERT(static_cast<u32>(lu8PhysicalType)
                   < static_cast<u32>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT),
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                  // BrnPhysicalTrafficVehicle.h:382

    if (lu8PhysicalType == static_cast<u8>(PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL))
    {
        TrafficPhysics* const lpFullTraffic = lpTrafficVehicle->GetFullTrafficPhysics();
        GetTrafficDriver(liTrafficIndex)->StartCatchupInterpolation(
            lpFullTraffic,
            lpEvent->mTransform,
            lpFullTraffic->GetLinearVelocity(),
            lpFullTraffic->GetAngularVelocity(),
            false);
    }
}

// =================================================================================================
// ClearSnappedNetworkTrafficContacts @0x825F37F0 (220) -- DWARF h:391. BODIED 2026-09-24 (G45-D1,
// crash parity FX-NETCRASH); it had no declaration and no body, and its one caller's tail call
// (VehicleManager::ClearSnappedNetworkCarContacts 0x8261AC28) was a [FLAG PC bring-up] comment.
//
// The traffic twin of the race-car loop in ClearSnappedNetworkCarContacts. A car snapped by the
// network catch-up (VehicleDriver::StartCatchupInterpolation's snap arm is the image's only store
// of 1 to mbSnappedThisFrame, +0xD5) had its transform replaced wholesale, so the contacts its
// deformable object stored for the old pose are dropped. Straight off the asm:
//   0x825F3804..0x825F386C  the inlined BitArray<20> first-set-bit walk over mUsedTrafficVehicles
//                           (this + 0x19868: `addis r18,r31,2 ; addi r18,r18,-0x6798`), then
//                           0x825F3988..0x825F3B58 the next-set-bit step (CgsBitArray.h:203 tripwire)
//   0x825F38F0  `cmpwi r30, 0x14 ; blt`  else assert "liVehicle < ku8TotalMaxNumPhysicalTraffic"
//               (BrnPhysicalTrafficManager.h 0x2F0 == 752)
//   0x825F3910  lwz (this + 0x194B0) mpaTrafficDrivers ; mulli 0xE0 ; lbz 0xD5 == mbSnappedThisFrame;
//               clear -> next vehicle
//   0x825F3938  bl GetPhysicsEntityId(liVehicle)          @0x825B4980
//   0x825F3948  bl DeformationManager::FindModelIndexByEntityID
//   0x825F3950  == -1 -> assert "liModelIndex != -1" (BrnDeformationManager.h 0x36E == 878)
//   0x825F3970..0x825F3984  *(mgr + 0x12900) + idx * 0x6780 -> DeformableObject::ClearStoredContacts
// Nothing is written to the driver: UpdateTrafficPhysicsPostSimulation clears mbSnappedThisFrame.
// =================================================================================================
void PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts(
    Deformation::DeformationManager* lpDeformationManager)
{
    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(liVehicle < static_cast<s32>(KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC),
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // h:752

        if (!mpaTrafficDrivers[liVehicle].SnappedThisFrame())                         // 0x825F3920 lbz +0xD5
            continue;

        const s32 liModelIndex =
            lpDeformationManager->FindModelIndexByEntityID(GetPhysicsEntityId(liVehicle));   // 0x825F3948
        CGS_ASSERT(liModelIndex != -1, "liModelIndex != -1");                         // BrnDeformationManager.h:878

        // HOST DIVERGENCE, flagged (the race-car twin in ClearSnappedNetworkCarContacts carries the
        // same one): on the asserted miss the console indexes the model array at -1; on the host that
        // is a wild pointer, so only the asserted-impossible miss is skipped.
        if (liModelIndex != -1)
            lpDeformationManager->GetDeformableObject(liModelIndex)->ClearStoredContacts();   // 0x825F3984

        // FLAG PC witness (BRN_NETCRASH_DIAG, capped; NOT console code).
        if (NetCrashDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siLines = 0;
            if (siLines < KI_NETCRASH_DIAG_MAX_LINES)
            {
                ++siLines;
                *CgsDev::Log::gpDebugPrint
                    << "[netcrash] ClearSnappedNetworkTrafficContacts slot=" << liVehicle
                    << " model=" << liModelIndex << " [FLAG PC witness]\n";
            }
        }
    }
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
