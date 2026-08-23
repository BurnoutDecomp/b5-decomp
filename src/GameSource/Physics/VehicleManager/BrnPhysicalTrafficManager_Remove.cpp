// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Remove.cpp
//
// The PHYSICS-SIDE REMOVE / RECYCLE drain and the frame's create/remove PUBLISH. Without these
// the twenty traffic slots never recycle and the manager's three per-frame bitsets never clear,
// so the first promotion arms three asserts at the top of ProcessTrafficMaintenanceEvents on the
// NEXT frame (:3701 / :3702 / :3703).
//
//   ProcessRemoveEvents            @0x8262D0C0 (125)
//   RemoveTrafficVehicle           @0x8261CC98 (570)
//   RecycleTrafficVehicle          @0x8262DA10 (112)
//   DeallocateInternalBuffers      @0x82615A38 ( 53)  .ida-exports HOLE, dumped headless
//   ComputeTrafficVehicleInertia   @0x825CAF60 (201)
//   SendCreateRemoveTrafficEvents  @0x825F2088 (990)
//   PhysicallyUncrashTrafficCar    @0x825F00D8 (140)  -- named gate (body not reconstructed)
//
// -------------------------------------------------------------------------------------------------
// TWO ENTITY-ID SPACES CROSS HERE, AND THE CONSOLE SPLICES A DIFFERENT OWNER BYTE INTO EACH.
//   * The DEFORMATION model's handling-body id keeps the TRAFFIC owner (2):
//       `extldi r30, r11, 64,32`   == (u64)entityWord << 32, posted verbatim (0x825F229C).
//   * The rw::physics SIMULATION's rigid-body id gets owner 0x0C spliced in:
//       `clrlwi r11,r11,8 ; oris r11,r11,0xC00 ; sldi 32 ; or lo`   (0x825F2228 / 0x825F2C9C).
// Both are built from the SAME entity word in the same three instructions apart. Do not unify them.
//
// THE OUTPUT OF SendCreateRemoveTrafficEvents IS INERT ON PURPOSE. Its two rigid-body queues
// (VehicleOutputRequestInterface::mRequiredRigidBodiesQueue / mRemoveRigidBodyQueue) drain only
// through WorldModule::BridgeVehicleManagerToSimulation_PostScene, a deliberately inert link stub
// (WorldLinkStubs.cpp:2694): Burnout integrates its own vehicles (ExternalPhysicsBody +
// ReadUpdatedBodies + IntegrateTransform), so a promoted traffic car reacts through the
// VehicleManager-generated contacts, not through rw::physics. The posts still have to happen --
// they are what CLEARS mAddedTrafficVehicles / mRemovedTrafficVehicles /
// mMadeSimpleTrafficVehicles at the tail of this function.
//
// No Feb-2007 source. ARTIST pseudocode + asm, DecFIGS DWARF for declaration shape.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"          // ArticulatedJointCreateBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"     // the remove queue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"    // both request queues + AddTrafficRemovedEvent
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                             // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"            // InAddRigidBody / InRemoveRigidBody
#include "GameShared/GameClasses/Core/CgsAssert.h"                                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                           // the named gates
#include "rw/physics/inertia.h"                                                      // rw::physics::Inertia

namespace
{
    inline void TrafficRemoveLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }

    // The simulation-side owner byte the console splices into a traffic rigid-body id
    // (`oris r11, r11, 0xC00` == 0x0C000000). FLAG: the full BrnWorld::EEntityType enum is owned
    // by the World module; only the one value these two posts use is reproduced, the same way
    // BrnVehicleConstants.h reproduces KU_ENTITYTYPE_TRAFFIC_VEHICLE.
    const u32 KU_ENTITYTYPE_PHYSICS_VEHICLE = 12u;

    const f32 KF_ZERO                  = 0.0f;      // flt_82001CC0
    const f32 KF_ONE                   = 1.0f;      // flt_82001C98
    const f32 KF_TWO                   = 2.0f;      // flt_82001D9C
    const f32 KF_BOX_INERTIA_COEFF     = 1.0f / 12.0f;  // flt_82094724 (0.0833333358f)
    const f32 KF_TRAFFIC_MAX_LINEAR_VELOCITY  = 2000.0f;  // stru_8208F640 (the race car's is 120000)
    const f32 KF_TRAFFIC_MAX_ANGULAR_VELOCITY = 5.0f;     // flt_8200426C   (the race car's is 30)
    const f32 KF_TRAFFIC_DRAG          = 0.0f;      // flt_82001CC0, linear AND angular
}

#define BRN_T3_REMOVE_GATE(TAG)                                                              \
    do { static bool s_bLogged = false;                                                      \
         TrafficRemoveLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{
namespace Vehicle
{
    // =============================================================================================
    // DeallocateInternalBuffers @0x82615A38 (53)
    //   asserts BrnPhysicalTrafficManager.h:865 / :866 / :867 / :872 (this one is in the HEADER,
    //   not the .cpp -- the console string is d:\...\vehiclemanager\BrnPhysicalTrafficManager.h).
    //   DWARF :269. The AllocateInternalBuffers twin: the buffer is popped off the INPUT stack.
    // lpOutputBufferStack is NULL-checked and then unused, exactly as in AllocateInternalBuffers.
    // =============================================================================================
    void PhysicalTrafficManager::DeallocateInternalBuffers(CgsModule::IOBufferStack* lpInputBufferStack,
                                                           CgsModule::IOBufferStack* lpOutputBufferStack)
    {
        CGS_ASSERT(lpInputBufferStack  != 0, "lpInputBufferStack != NULL");                  // h:865
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");                 // h:866
        CGS_ASSERT(mpArticulatedJointCreateBuffer != 0,
                   "mpArticulatedJointCreateBuffer != NULL");                                // h:867

        lpInputBufferStack->DestroyIOBuffer<ArticulatedJointCreateBuffer>(&mpArticulatedJointCreateBuffer);

        CGS_ASSERT(mpArticulatedJointCreateBuffer == 0,
                   "mpArticulatedJointCreateBuffer == NULL");                                // h:872
    }

    // =============================================================================================
    // GATE PhysicalTrafficManager::PhysicallyUncrashTrafficCar @0x825F00D8 (140)
    //   blocker: body not reconstructed (it posts the DeactivateDeformationModelEvent that tears
    //   the crashed car's deformation model down). DELETE-WHEN that body lands.
    // =============================================================================================
    void PhysicalTrafficManager::PhysicallyUncrashTrafficCar(u16, Deformation::DeformationInputInterface*)
    {
        BRN_T3_REMOVE_GATE("PhysicalTrafficManager::PhysicallyUncrashTrafficCar @0x825F00D8 (140)");
    }

    // =============================================================================================
    // DisposeOfNonCrashingTraffic @0x825EFB40 (72)
    //   DWARF :165 / BrnPhysicalTrafficManager.cpp:2041. No asserts of its own.
    //
    // THE WALK, verbatim from 0x825EFB4C..0x825EFDE4:
    //   r19 = this + 104576 == mPotentialTrafficVehicles
    //   r15 = this + 104624 == mUnusedPotentialTrafficQueue
    //   `for (i = GetFirstNonZeroBit(); i >= 0; i = GetNextNonZeroBit(i)) AddEvent((s8)i)`
    //   (`stb r31, var_B0(r1)` + `addi r4,r1,var_B0` is the s8 passed by address.)
    //
    // IT DOES NOT CLEAR THE BIT. RemoveTrafficVehicle does that (:320) when ProcessRemoveEvents
    // drains the queue on the NEXT frame's PostSceneUpdate, BEFORE ProcessCreateEvents -- which is
    // why an overlap pair may legitimately re-promote that frame without tripping :1110.
    //
    // CONSOLE CALL POSITION: the TAIL of VehicleManager::ProcessContactSpies @0x82646E5C, inside
    // PhysicsModule::Update's non-catchup contact-spy leg, AFTER WriteOutVehicleStats.
    // ProcessContactSpies is still a conductor gate, so the call is seated at that same frame
    // position in BrnPhysicsModuleUpdateFunctions.cpp. DELETE-WHEN ProcessContactSpies is bodied.
    // =============================================================================================
    void PhysicalTrafficManager::DisposeOfNonCrashingTraffic()
    {
        for (s32 liVehicle = mPotentialTrafficVehicles.GetFirstNonZeroBit();
             liVehicle >= 0;
             liVehicle = mPotentialTrafficVehicles.GetNextNonZeroBit(liVehicle))
        {
            mUnusedPotentialTrafficQueue.AddEvent(static_cast<s8>(liVehicle));
        }
    }

    // =============================================================================================
    // ProcessRemoveEvents @0x8262D0C0 (125)
    //   asserts BrnPhysicalTrafficManager.cpp:600 / :601 / :602 (+ the queue's own :292/:294/:295).
    //   DWARF :321.
    //
    // Two drains. The first walks the WORLD's remove queue and linear-searches all twenty slots for
    // the entity id (`for i in 0..19` over maTrafficEntityIDs, NOT a bitset walk -- 0x8262D18C) and
    // stops at the first hit. The second drains the manager's own mUnusedPotentialTrafficQueue
    // (EventQueue<s8,50>, one BYTE per entry -- `lbzx r4, r11, r31`) and then clears it.
    // =============================================================================================
    void PhysicalTrafficManager::ProcessRemoveEvents(
            const CgsModule::EventQueue<RemoveTrafficEvent, 25>* lpRemoveTrafficQueue,
            VehicleOutputRequestInterface* lpOutputInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(lpRemoveTrafficQueue   != 0, "lpRemoveTrafficQueue != NULL");             // :600
        CGS_ASSERT(lpOutputInterface      != 0, "lpOutputInterface != NULL");                // :601
        CGS_ASSERT(lpDeformationInterface != 0, "lpDeformationInterface != NULL");           // :602

        for (s32 liEvent = 0; liEvent < lpRemoveTrafficQueue->GetLength(); ++liEvent)
        {
            // `ld r9,0(r3) ; srdi r9,r9,32` -- the embedded entity word.
            const u32 luGlobalEntityId =
                static_cast<u32>(lpRemoveTrafficQueue->GetEvent(liEvent).mVolumeInstanceID.muId
                                 >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX);

            for (s32 liVehicle = 0; liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC; ++liVehicle)
            {
                if (maTrafficEntityIDs[liVehicle].muValue == luGlobalEntityId)
                {
                    RemoveTrafficVehicle(static_cast<u8>(liVehicle), lpOutputInterface,
                                         lpDeformationInterface, true);
                    break;
                }
            }
        }

        for (s32 liEvent = 0; liEvent < mUnusedPotentialTrafficQueue.GetLength(); ++liEvent)
        {
            RemoveTrafficVehicle(static_cast<u8>(mUnusedPotentialTrafficQueue.GetEvent(liEvent)),
                                 lpOutputInterface, lpDeformationInterface, true);
        }

        // `stw r21(0), 0(r28)` where r28 == this+104632 == the queue's own miLength.
        mUnusedPotentialTrafficQueue.Clear();
    }

    // =============================================================================================
    // RecycleTrafficVehicle @0x8262DA10 (112)
    //   asserts BrnPhysicalTrafficManager.cpp:3426, :3455, :3466. DWARF :312.
    //
    // CONSOLE DEFECT, REPRODUCED VERBATIM AND FLAGGED (the final three instructions):
    //       0x8262DBB8  add   r11, r27, r31       ; r27 == liIndexToRecycle, r31 == this
    //       0x8262DBBC  lbzx  r11, r11, r30       ; == mu8GlobalToPhysicalEntityIndexMap[SLOT]
    //       0x8262DBC0  add   r11, r11, r31
    //       0x8262DBC4  stbx  r20(0x7F), r11, r30 ; == map[ map[SLOT] ] = KU8_INVALID_MAP
    // The ARTICULATED half three lines above does it correctly (`lwzx maTrafficEntityIDs[other] ;
    // extrwi 14,8 ; stbx`). The recycled car's OWN entry is not left stale -- RemoveTrafficVehicle
    // already cleared it (0x8261D3EC..0x8261D3F8, reproduced at :299) -- so the damage is the
    // opposite: this writes KU8_INVALID_MAP over map[ map[slot] ], i.e. over the entry of whichever
    // GLOBAL entity index happens to equal 0..19, unmapping an unrelated live car. Reproduced, not
    // silently corrected; the store is in bounds (the byte read is <= 127, the map is 600 wide) so
    // it cannot fault.
    //
    // AND THE :3466 ASSERT READS PAST THE ARRAY. The console re-loads maTrafficEntityIDs[slot]
    // AFTER RemoveTrafficVehicle stored the 0xFFFFFFFF sentinel there (0x8262DB84 `lwzx r11,r29,r31`
    // follows the `bl` at 0x8262DB80; sentinel written at 0x8261D3FC..0x8261D404 from
    // dword_82F2A3A4 == 0xFFFFFFFF), so `extrwi 14,8` yields 16383 every time. On console that is
    // still inside the enclosing VehicleManager; on the host it is ~15.4 KB past a u8[600]. The
    // index expression is kept verbatim and the READ is bounds-gated the same way the mounted
    // BrnPhysicalTrafficManager.cpp:474 gates its own map read, so a stray host byte equal to 127
    // cannot masquerade as a create-drain failure.
    // =============================================================================================
    void PhysicalTrafficManager::RecycleTrafficVehicle(
            s32 liIndexToRecycle,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(liIndexToRecycle >= 0 && liIndexToRecycle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liIndexToRecycle >= 0 && liIndexToRecycle < ku8TotalMaxNumPhysicalTraffic"); // :3426

        PhysicalTrafficVehicle* lpVehicle = GetTrafficVehicle(liIndexToRecycle);
        if (lpVehicle->HasNonBrokenJoint())
        {
            // GATE ArticulatedJointPool::GetIndexOfOtherHalf @0x825D8490 (304)
            //    blocker: declare-only on the pool's surface, and its DWARF parameter is
            //    PhysicalTrafficVehicle::EArticulatedVehicleType -- a nested enum of a class whose
            //    header INCLUDES the pool's, so it cannot be named there without an include cycle.
            //    DELETE-WHEN the trailer wave lands. Unreachable today: nothing creates a joint.
            BRN_T3_REMOVE_GATE("PhysicalTrafficManager::RecycleTrafficVehicle articulated half "
                               "(ArticulatedJointPool::GetIndexOfOtherHalf @0x825D8490)");
        }

        // `lwzx r28, r29, r31` then `lwz r5, 0x20(vehicle)` -- the removed car's own global id and
        // its lifecycle state, into the world-facing "a physical traffic car went away" event.
        lpManagerOutputInterface->AddTrafficRemovedEvent(
                maTrafficEntityIDs[liIndexToRecycle],
                static_cast<ETrafficType>(GetTrafficVehicle(liIndexToRecycle)->mePhysicalTrafficState));

        // `lbz r7, byte_82FB7DF1` -- a .data bool that is 0 in the shipped image. The parameter it
        // feeds is never read inside RemoveTrafficVehicle (r7 is dead after the prologue), so the
        // value is not load-bearing; passed as the image's own false.
        RemoveTrafficVehicle(static_cast<u8>(liIndexToRecycle), lpOutputRequestInterface,
                             lpDeformationInterface, false);

        // Re-read AFTER RemoveTrafficVehicle, so this is 0xFFFFFFFF >> 10 & 0x3FFF == 16383 every
        // time (see banner). Host bounds guard on the READ only; the index is verbatim.
        const u32 luEntityIndex =
            (maTrafficEntityIDs[liIndexToRecycle].muValue >> 10) & 0x3FFFu;   // `extrwi r11,r11,14,8`
        if (luEntityIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap))
        {
            CGS_ASSERT(mu8GlobalToPhysicalEntityIndexMap[luEntityIndex] != KU8_INVALID_MAP,
                       "mu8GlobalToPhysicalEntityIndexMap[maTrafficEntityIDs[liIndexToRecycle]."
                       "GetEntityIndex()] != KU8_INVALID_MAP");                              // :3466
        }

        // THE DEFECT ABOVE, VERBATIM: the console indexes the map WITH THE MAP, not with the
        // entity index the assert on the line above just read.
        mu8GlobalToPhysicalEntityIndexMap[mu8GlobalToPhysicalEntityIndexMap[liIndexToRecycle]] =
            KU8_INVALID_MAP;
    }

    // =============================================================================================
    // RemoveTrafficVehicle @0x8261CC98 (570)
    //   asserts BrnPhysicalTrafficManager.cpp:660, :682, :689, :694, :702, :736, :744, :749;
    //   BrnPhysicalTrafficVehicle.h:382; CgsBitArray.h:203/:241. DWARF :291.
    //
    // TWO OF THE FOUR PARAMETERS ARE DEAD IN THIS BUILD. lpOutputRequestInterface (r5) and
    // lbRemoveFromSimulation (r7) are never read after the prologue -- the removal is published a
    // frame later out of mRemovedTrafficVehicles by SendCreateRemoveTrafficEvents, not here. Kept
    // in the signature because the DWARF and every call site have them.
    // =============================================================================================
    void PhysicalTrafficManager::RemoveTrafficVehicle(
            u8 lu8TrafficEntityNum,
            VehicleOutputRequestInterface* /*lpOutputRequestInterface -- unread, see banner*/,
            Deformation::DeformationInputInterface* lpDeformationInterface,
            bool /*lbRemoveFromSimulation -- unread, see banner*/)
    {
        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "invalid index : luIndex < 20");                                          // CgsBitArray.h:203
        CGS_ASSERT(mUsedTrafficVehicles.IsBitSet(lu8TrafficEntityNum),
                   "mUsedTrafficVehicles.IsBitSet( lu8TrafficEntityNum )");                  // :660

        PhysicalTrafficVehicle* lpVehicle = GetTrafficVehicle(lu8TrafficEntityNum);
        if (lpVehicle->HasNonBrokenJoint())
        {
            // GATE the articulated teardown -- ArticulatedJointPool::GetIndexOfOtherHalf
            //    @0x825D8490 (304) + ::RemoveJoint @0x825D8248 (580), both declare-only on the
            //    pool's surface, both parked with the trailer sub-tree.
            //    DELETE-WHEN the trailer wave lands. Unreachable today: nothing creates a joint.
            BRN_T3_REMOVE_GATE("PhysicalTrafficManager::RemoveTrafficVehicle articulated teardown "
                               "(ArticulatedJointPool::GetIndexOfOtherHalf/RemoveJoint)");
        }

        // Unconditional, and NOT part of the branch above (loc_8261CFC0 is the not-taken target).
        lpVehicle->meArticulatedVehicleType = PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE;
        lpVehicle->meArticulatedJointState  = PhysicalTrafficVehicle::E_ARTICULATE_JOINT_NONE;
        lpVehicle->miJointIndex             = -1;

        PhysicallyUncrashTrafficCar(lu8TrafficEntityNum, lpDeformationInterface);

        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "lu8TrafficEntityNum < ku8TotalMaxNumPhysicalTraffic");                   // :736

        const u8 lu8Type      = lpVehicle->mu8PhysicalType;
        const u8 lu8PoolIndex = lpVehicle->mu8PhysicsPoolIndex;
        CGS_ASSERT(lu8Type < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                                // h:382

        // The console streams the slot/pool indices into these two messages ("Trying to remove
        // traffic <id> using TrafficPhysics object <pool>, which isn't in use"); collapsed to the
        // static text, the committed precedent for streamed asserts.
        if (lu8Type == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
        {
            CGS_ASSERT(lu8PoolIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "invalid index : luIndex < 20");
            CGS_ASSERT(mUsedFullTrafficPhysics.IsBitSet(lu8PoolIndex),
                       "Trying to remove traffic using TrafficPhysics object which isn't in use"); // :744
            CGS_ASSERT(lu8PoolIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "luIndex < NUMBITS");
            mUsedFullTrafficPhysics.UnSetBit(lu8PoolIndex);
        }
        else
        {
            CGS_ASSERT(lu8PoolIndex < 1, "invalid index : luIndex < 1");
            CGS_ASSERT(mUsedSimpleVehiclePhysics.IsBitSet(lu8PoolIndex),
                       "Trying to remove traffic using SimpleVehiclePhysics object which isn't in use"); // :749
            CGS_ASSERT(lu8PoolIndex < 1, "luIndex < NUMBITS");
            mUsedSimpleVehiclePhysics.UnSetBit(lu8PoolIndex);
        }

        // The inlined VehicleDriver::ClearControls over mpaTrafficDrivers[num] -- byte for byte the
        // same store set PreparePhysicsForNewTrafficVehicle emits.
        mpaTrafficDrivers[lu8TrafficEntityNum].ClearControls();

        // Order matters: the map is invalidated through the OLD entity id, then the id itself is
        // invalidated (`lwz dword_82F2A3A4` == 0xFFFFFFFF, the EntityId invalid sentinel).
        const u32 luEntityIndex =
            (maTrafficEntityIDs[lu8TrafficEntityNum].muValue >> 10) & 0x3FFFu;
        mu8GlobalToPhysicalEntityIndexMap[luEntityIndex] = KU8_INVALID_MAP;
        maTrafficEntityIDs[lu8TrafficEntityNum].muValue  = 0xFFFFFFFFu;

        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "luIndex < NUMBITS");
        mUsedTrafficVehicles.UnSetBit(lu8TrafficEntityNum);
        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "luIndex < NUMBITS");
        mPotentialTrafficVehicles.UnSetBit(lu8TrafficEntityNum);
        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "invalid index : luIndex < 20");
        mRemovedTrafficVehicles.SetBit(lu8TrafficEntityNum);
        CGS_ASSERT(lu8TrafficEntityNum < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "luIndex < NUMBITS");
        mAddedTrafficVehicles.UnSetBit(lu8TrafficEntityNum);
    }

    // =============================================================================================
    // ComputeTrafficVehicleInertia @0x825CAF60 (201)
    //   asserts BrnPhysicalTrafficManager.cpp:3766 / :3767 (+ the CgsBitArray bound). DWARF :345.
    //
    // The solid-box inertia tensor, the same shape VehicleManager::ProcessCreateEvents builds for a
    // race car -- with THREE measured differences, all read out of the image:
    //   * the box is mHalfExtent * 2.0f with NO |COMOffset| term (the race car adds one);
    //   * max linear velocity is 2000 (stru_8208F640) against the race car's 120000;
    //   * max angular velocity is 5 (flt_8200426C) against the race car's 30, and both drags are 0
    //     (flt_82001CC0) against the race car's 1e-5.
    // The vrefp/vnmsubfp/vmaddfp triples are the VMX reciprocal expansion of the three divides.
    // Lane .w of the inverse-inertia vector is never written by the console (three `vrlimi128`
    // inserts touch lanes 0/1/2 only); zeroed here rather than left as stack residue.
    // =============================================================================================
    void PhysicalTrafficManager::ComputeTrafficVehicleInertia(s32 liTrafficVehicleIndex,
                                                              rw::physics::Inertia* lpOutInertia)
    {
        CGS_ASSERT(lpOutInertia != 0, "lpOutInertia != NULL");                               // :3766
        CGS_ASSERT(liTrafficVehicleIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "invalid index : luIndex < 20");                                          // CgsBitArray.h
        CGS_ASSERT(mUsedTrafficVehicles.IsBitSet(static_cast<u32>(liTrafficVehicleIndex)),
                   "mUsedTrafficVehicles.IsBitSet( liTrafficVehicleIndex )");                // :3767

        const SimpleVehiclePhysics* lpBody = GetTrafficVehicle(liTrafficVehicleIndex)->mpVehicleBody;

        const Vector3 lvHalfExtent    = lpBody->GetHalfExtent();
        const f32     lfMass          = lpBody->GetMass().x;
        const f32     lfCoeff         = lfMass * KF_BOX_INERTIA_COEFF;

        const f32 lfX = lvHalfExtent.x * KF_TWO;
        const f32 lfY = lvHalfExtent.y * KF_TWO;
        const f32 lfZ = lvHalfExtent.z * KF_TWO;

        const f32 lfXX = lfX * lfX;
        const f32 lfYY = lfY * lfY;
        const f32 lfZZ = lfZ * lfZ;

        // `vmulfp128 v,broadcast(coeff),(sum)` then `vrefp` + two Newton steps == 1/(coeff*sum).
        Vector3 lvInverseInertia;
        lvInverseInertia.x = KF_ONE / ((lfYY + lfZZ) * lfCoeff);
        lvInverseInertia.y = KF_ONE / ((lfXX + lfZZ) * lfCoeff);
        lvInverseInertia.z = KF_ONE / ((lfXX + lfYY) * lfCoeff);
        lvInverseInertia.w = KF_ZERO;

        rw::physics::Inertia lInertia;
        lInertia.SetLinearDrag(KF_TRAFFIC_DRAG);                          // block +0x20
        lInertia.SetAngularDrag(KF_TRAFFIC_DRAG);                         // block +0x24
        lInertia.SetMaxLinearVelocity(KF_TRAFFIC_MAX_LINEAR_VELOCITY);    // block +0x18
        lInertia.SetMaxAngularVelocity(KF_TRAFFIC_MAX_ANGULAR_VELOCITY);  // block +0x1C
        lInertia.SetInverseMass(KF_ONE / lfMass);                         // block +0x10
        lInertia.SetInverseInertia(lvInverseInertia);                     // +0x00 and the +0x14 derive

        *lpOutInertia = lInertia;   // the six ld/std doublewords at 0x825CB268
    }

    // =============================================================================================
    // SendCreateRemoveTrafficEvents @0x825F2088 (990)
    //   asserts BrnPhysicalTrafficManager.cpp:3826 / :3827 / :3920; BrnPhysicalTrafficVehicle.h:382
    //   and :391; CgsEntityId.h:116/:833; the CgsBitArray bounds. DWARF :342.
    //
    // Three drains, then the three bitsets are cleared (`li r11,0 ; std x3` @0x825F2FD8..0x825F2FEC).
    // =============================================================================================
    void PhysicalTrafficManager::SendCreateRemoveTrafficEvents(
            VehicleOutputRequestInterface* lpOutputRequestInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(lpOutputRequestInterface != 0, "lpOutputRequestInterface != NULL");        // :3826
        CGS_ASSERT(lpDeformationInterface   != 0, "lpDeformationInterface != NULL");          // :3827

        // ---- 1. removed --------------------------------------------------------------------------
        for (s32 liVehicle = mRemovedTrafficVehicles.GetFirstNonZeroBit();
             liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liVehicle = mRemovedTrafficVehicles.GetNextNonZeroBit(liVehicle))
        {
            // `extldi r30, r11, 64,32` -- the physics entity word in the HIGH dword.
            const u64 lu64DeformationBodyId =
                static_cast<u64>(GetPhysicsEntityId(liVehicle).muValue) << 32;

            // ...and the same word with the simulation owner byte spliced in (see the banner).
            const u32 luSimEntityWord =
                (static_cast<u32>(lu64DeformationBodyId >> 32) & 0x00FFFFFFu)
                | (KU_ENTITYTYPE_PHYSICS_VEHICLE << 24);

            CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lRemoveBody;
            lRemoveBody.mID = (static_cast<u64>(luSimEntityWord) << 32)
                              | (lu64DeformationBodyId & 0xFFFFFFFFull);
            lRemoveBody.mbFailIfRigidBodyNotFound = false;                 // `stb r16(0), var_388`
            lpOutputRequestInterface->GetRemoveRigidBodyQueue()->AddEvent(lRemoveBody);

            CGS_ASSERT(liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                       "liVehicle < ku8TotalMaxNumPhysicalTraffic");
            const u8 lu8Type = mpaTrafficVehicles[liVehicle].mu8PhysicalType;
            CGS_ASSERT(lu8Type < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                       "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
            if (lu8Type != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE)
            {
                // THE DEFORMATION ID KEEPS THE TRAFFIC OWNER -- no 0x0C splice here.
                lpDeformationInterface->RemoveDeformationModel(
                        CgsPhysics::RigidBodyId(lu64DeformationBodyId));
            }
        }

        // ---- 2. made simple ---------------------------------------------------------------------
        for (s32 liTrafficIndex = mMadeSimpleTrafficVehicles.GetFirstNonZeroBit();
             liTrafficIndex != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liTrafficIndex = mMadeSimpleTrafficVehicles.GetNextNonZeroBit(liTrafficIndex))
        {
            // A car demoted to a bare collision box keeps no deformation model.
            lpDeformationInterface->RemoveDeformationModel(CgsPhysics::RigidBodyId(
                    static_cast<u64>(GetPhysicsEntityId(liTrafficIndex).muValue) << 32));
        }

        // ---- 3. added ---------------------------------------------------------------------------
        for (s32 liTrafficIndex = mAddedTrafficVehicles.GetFirstNonZeroBit();
             liTrafficIndex != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
             liTrafficIndex = mAddedTrafficVehicles.GetNextNonZeroBit(liTrafficIndex))
        {
            CGS_ASSERT(liTrafficIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                       "liVehicle < ku8TotalMaxNumPhysicalTraffic");
            PhysicalTrafficVehicle* lpVehicle = &mpaTrafficVehicles[liTrafficIndex];
            const SimpleVehiclePhysics* lpBody = lpVehicle->mpVehicleBody;

            const u64 lu64PhysicsBodyId =
                static_cast<u64>(GetPhysicsEntityId(liTrafficIndex).muValue) << 32;

            CGS_ASSERT(mUsedTrafficVehicles.IsBitSet(static_cast<u32>(liTrafficIndex)),
                       "Vehicle was removed before it had finished being added");            // :3920

            // A DEFAULT-CONSTRUCTED Inertia is what the console builds on the stack first
            // ({1,1,1,0} / 1 / 1 / FLT_MAX / FLT_MAX / 0 / 0), then overwrites.
            rw::physics::Inertia lInertia;
            ComputeTrafficVehicleInertia(liTrafficIndex, &lInertia);

            CgsPhysics::PhysicsSimulationIO::InAddRigidBody lAddBody;
            lAddBody.mRigidBody.mTransform       = lpBody->GetTransform();
            lAddBody.mRigidBody.mVelocity        = lpBody->GetLinearVelocity();
            lAddBody.mRigidBody.mAngularVelocity = lpBody->GetAngularVelocity();
            lAddBody.mRigidBody.mInertia         = lInertia;
            lAddBody.mRigidBody.mbSpy            = true;                    // `stb r9(1), var_2E0`
            lAddBody.meState                     = rw::physics::ACTIVE_BODY;// `stw r11(4), var_180`

            const u32 luSimEntityWord =
                (static_cast<u32>(lu64PhysicsBodyId >> 32) & 0x00FFFFFFu)
                | (KU_ENTITYTYPE_PHYSICS_VEHICLE << 24);
            lAddBody.mID = (static_cast<u64>(luSimEntityWord) << 32)
                           | (lu64PhysicsBodyId & 0xFFFFFFFFull);
            lpOutputRequestInterface->GetRequiredRigidBodiesQueue()->AddEvent(lAddBody);

            const u8 lu8Type = lpVehicle->mu8PhysicalType;
            CGS_ASSERT(lu8Type < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                       "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
            if (lu8Type == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)
            {
                // The second `lbz +0x32` + "IsFullyPhysical()" assert pair IS the inlined
                // PhysicalTrafficVehicle::GetFullTrafficPhysics @0x825C0148.
                TrafficPhysics* lpFullTrafficPhysics = lpVehicle->GetFullTrafficPhysics();

                // `srwi r10,r31,24 ; cmplwi 2 ; bne` then the maTrafficEntityIDs lookup -- the
                // physics id is mapped back to the GLOBAL id the deformation module knows.
                EntityId lGlobalEntityId;
                lGlobalEntityId.muValue = static_cast<u32>(lu64PhysicsBodyId >> 32);
                if ((lGlobalEntityId.muValue >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE)
                {
                    lGlobalEntityId = maTrafficEntityIDs[(lGlobalEntityId.muValue >> 10) & 0x3FFFu];
                }

                // [FLAG PC bring-up] NULL-mpAttribs TRIPWIRE -- the SAME one
                // BrnVehicleManager_MaintenanceEvents.cpp's AddRaceCarDeformationModel carries, for
                // the same reason. The console reads mpAttribs unconditionally here
                // (`lwz r9, 0x720(r11)` @0x825F2D44), but VehiclePhysics::Construct seeds it NULL
                // and only PhysicalTrafficVehicle::PreparePhysical's TrafficPhysics delegation
                // fills it -- and that delegation is `(void)`-INERT until cluster C3 wires it. On
                // the console the read survives; here it is a frame-one AV at +0x20 that would be
                // misread as a create-drain failure. DELETE-WHEN C3 lands the delegation.
                if (lpFullTrafficPhysics->GetAttribs() == 0)
                {
                    CGS_ASSERT(lpFullTrafficPhysics->GetAttribs() != 0,
                               "SendCreateRemoveTrafficEvents: mpAttribs != NULL "
                               "(TrafficPhysics::PreparePhysical delegation still inert?)");
                    continue;
                }

                // NOT the race car's argument set: the traffic post carries the body's REAL
                // velocities, zero damage, reset type -1 and NO swept-sphere tests.
                lpDeformationInterface->AddDeformationModel(
                        maTrafficCarModelHandles[liTrafficIndex],
                        CgsPhysics::RigidBodyId(lu64PhysicsBodyId),
                        lGlobalEntityId,
                        lpFullTrafficPhysics,
                        lpFullTrafficPhysics->GetAttribs()->mBaseAttribs.mCOMOffset,
                        lpBody->GetTransform(),
                        lpBody->GetLinearVelocity(),
                        lpBody->GetAngularVelocity(),
                        KF_ZERO,
                        static_cast<Deformation::DeformationResetType>(-1),
                        false);
            }
        }

        mAddedTrafficVehicles.UnSetAll();
        mRemovedTrafficVehicles.UnSetAll();
        mMadeSimpleTrafficVehicles.UnSetAll();
    }
}
}
