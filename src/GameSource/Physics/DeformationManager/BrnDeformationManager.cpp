#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

// ============================================================================
// BrnPhysics::Deformation::DeformationManager -- manager lifecycle + per-frame event
// ingress (mgr-core group).
//
//   Construct  @0x82621510   Destruct @0x82603F78   Prepare @0x82630230   Release @0x82603F60
//   ProcessEvents @0x82644E38
//   ProcessAddDeformationModelEvents        @0x82644828
//   ProcessRemoveDeformationModelEvents     @0x82641A00
//   ProcessDeactivateDeformationModelEvents @0x82641C58
//   OutputData @0x826225D8
//
// The X360 build is VMX128 + heavily inlined (the BitArray bit math, the per-model
// resource resolution, the assert StrStream formatting are all inline). Per the project
// convention (cf. BrnDeformableObject.cpp / BrnPhysicalTrafficManager.cpp) the bodies
// below are the DE-SIMD'd, BY-NAME equivalents written against the frozen header members
// -- no __asm, no raw `*(this + N)` offset pokes. The control flow, branches, early-outs,
// loop bounds, constants and the assert MESSAGES are reproduced exactly from the X360
// pseudocode/asm; the file/line baked into the X360 asserts is intentionally dropped (the
// CGS_ASSERT macro supplies the host file/line).
//
// The packed entity-id convention (shared with BrnPhysicalTrafficManager.cpp): an id word
// packs  owner = (word >> 24) & 0xFF  and  index = (word >> 10) & 0x3FFF.
// ============================================================================

#include <new>   // placement new (Prepare constructs the 28 models in the allocated pool storage)

#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / Resource / BaseResourceDescriptors (the pool carve)

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"        // CgsDev::PerfMonCpu
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"  // DeformationInputInterface (event queues)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (homed callee)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec

namespace BrnPhysics
{
namespace Deformation
{
    // -----------------------------------------------------------------------------------
    // DWARF BrnDeformationManager.h namespace-scope rodata (:57-58, :752). The float /
    // VecFloat VALUES are not recovered (the X360 loads them from a constant pool with no
    // resolvable symbol); per the no-fabrication rule they are honest FLAGGED-0 placeholders,
    // defined here once (the frozen header declares them extern). The friction pair is the
    // detached-part dynamic/static friction; the padding is the swept-sphere project radius pad.
    // -----------------------------------------------------------------------------------
    const f32      KF_PART_DYNAMIC_FRICTION         = 0.0f;                       // FLAG: rodata value unrecovered
    const f32      KF_PART_STATIC_FRICTION          = 0.0f;                       // FLAG: rodata value unrecovered
    const VecFloat KVF_PROJECTSPHERE_RADIUS_PADDING = { 0.0f, 0.0f, 0.0f, 0.0f }; // FLAG: rodata value unrecovered

    // The packed-entity-id owner tags this TU's asserts test against (BrnWorld::E_ENTITYTYPE_*;
    // the enum is not in-tree -- the high byte values are the asm-attested constants, matching
    // BrnPhysicalBodyPartPool.cpp / BrnPhysicalTrafficManager.cpp). FLAG: enum un-homed.
    static const u32 KU_ENTITYTYPE_RACECAR               = 1;
    static const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE       = 2;

    // The deformation-pool capacities the Construct memset loops + the add/remove bounds asserts
    // pin (8 race cars, 20 traffic, 600+1 global traffic). Local names; the table extents are
    // the frozen-header array bounds (ma8RaceCarToModelIndex[8] / ma8TrafficToModelIndex[20] /
    // ma8GlobalTrafficToModelIndex[601]).
    static const s32 KI_MAX_NUM_RACE_CARS                = 8;    // Vehicle::ku8MaxNumRaceCars
    static const s32 KI_MAX_NUM_PHYSICAL_TRAFFIC         = 20;   // Vehicle::ku8TotalMaxNumPhysicalTraffic
    static const s32 KI_MAX_TOTAL_TRAFFIC                = 600;  // BrnTraffic::KU_MAX_TOTAL_TRAFFIC

    // ===================================================================================
    // DECLARE-ONLY cross-TU surface (FLAG). The home TUs for these callees are not yet in-tree,
    // so they are declared here with their asm-attested operation and called BY NAME (compiles
    // under `cl /c`; resolved at link when the owning TU lands). NONE of these are fabricated
    // behaviour -- each stands in for a single inlined X360 operation:
    //
    //   * DeformationDebugComponent_{Construct,Register,Destruct} -- the manager touches the
    //     FILE-SCOPE STATIC DeformationDebugComponent (frozen-header `static mDebugComponent`):
    //     Construct constructs it with `this`; Prepare registers it with the debug menu; Destruct
    //     asserts its manager back-pointer is wired, clears it, and destructs the base collision
    //     generator it derives from. The component's full home (BrnDeformationDebugComponent.h) is
    //     forward-declared in the frozen header.
    //   * AllocateDeformableModelPool -- Prepare's typed pool allocation through the rw allocator
    //     (X360 builds a 28*sizeof(DeformableObject) descriptor + calls the allocator; the call
    //     returns the pool base). The descriptor build + allocator vtable call are cross-TU.
    //   * ResolveDeformationSpec -- ProcessAdd's resource-handle -> StreamedDeformationSpec*
    //     resolution (X360 CreateFromHandle + the resource-resolve thunk sub_825E5210). The
    //     resource-ptr machinery (CgsResource::BaseResourcePtr) is cross-TU.
    // ===================================================================================
    void DeformationDebugComponent_Construct(DeformationDebugComponent* lpComponent,
                                             DeformationManager* lpManager);   // X360: DeformationDebugComponent::Construct(&static, this)
    void DeformationDebugComponent_Register(DeformationDebugComponent* lpComponent);  // X360: DebugComponent::Register(&static)
    void DeformationDebugComponent_Destruct(DeformationDebugComponent* lpComponent);  // X360: clear mpDeformationManager + BaseCollisionGenerator::Destruct(&static)
    bool DeformationDebugComponent_HasManager(const DeformationDebugComponent* lpComponent); // X360: the (mpDeformationManager != NULL) assert read

    // ⭐ BOTH BODIED 2026-08-14 (walls wave) -- they were declare-only "cross-TU surface" and no TU
    // anywhere defined them (trial-link-measured). Their mechanisms are now byte-attested:
    //
    // AllocateDeformableModelPool -- Prepare @0x826302E0..0x8263033C builds the standard rw
    // five-entry resource descriptor ({size,align} in entry[0], {0,1} in the rest), size 0xB5200
    // (console 28 * 26496 -- the host uses its own 28 * sizeof) with ALIGN 16 (`li r11, 0x10`),
    // names the allocation "DeformationModels", and calls the allocator's DoAllocate slot
    // (vtable +0x10). Same CarveResource idiom as CgsPhysicsSimulationModule.cpp:1008 and
    // TriangleCacheManager::Prepare. The pool base is m_baseResources[0].
    DeformableObject* AllocateDeformableModelPool(rw::IResourceAllocator* lpAllocator, u32 luCount)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luCount * static_cast<u32>(sizeof(DeformableObject));
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;   // 0x8263031C `li r11, 0x10`

        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), "DeformationModels");
        return static_cast<DeformableObject*>(lResource.m_baseResources[0]);
    }

    // ResolveDeformationSpec -- ONE dereference of the widened handle's leading pointer:
    // DeformableObject::Prepare @0x826421C0 `lwz r27,0(evt) ; lwz r27,0(r27)` and the validate
    // drain @0x825DB14C..164 both read *(mModelHandle.mpResourceMemory) as the fixed-up spec.
    // The same *reinterpret_cast<T* const*>(mpResourceMemory) shape every CgsResourcePtr_* TU
    // uses (e.g. CgsResourcePtr_BrnVehicle_GraphicsSpec.cpp).
    StreamedDeformationSpec* ResolveDeformationSpec(ResourceHandle lModelHandle)
    {
        return *reinterpret_cast<StreamedDeformationSpec* const*>(lModelHandle.mpResourceMemory);
    }

    // The X360 reaches three DeformableObject members the FROZEN BrnDeformableObject.h models in a
    // way this TU cannot call directly: ConstructUpdate/IK/PostPhysics PerformanceMonitors are
    // declared as INSTANCE methods there but the X360 Construct calls them with NO object (they are
    // static perfmon-registration routines), and ClearVariables is declared PRIVATE there but the
    // X360 Prepare calls it on each model. Rather than edit a frozen header outside this group, they
    // are reached via declare-only stand-ins. FLAG: reconcile to `static` / `public` (or a friend
    // grant) on DeformableObject when that header is next revised.
    void DeformableObject_ConstructUpdatePerformanceMonitors();
    void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
    void DeformableObject_ConstructPostPhysicsPerformanceMonitors();
    void DeformableObject_ClearVariables(DeformableObject* lpModel);


    // -----------------------------------------------------------------------------------
    // Destruct  @0x82603F78
    //
    // Tear the manager + its collision generator down. Asserts the static debug component's
    // manager back-pointer is still wired, clears it, destructs the base collision generator,
    // then clears the manager's live-slot set.
    // -----------------------------------------------------------------------------------
    void DeformationManager::Destruct()
    {
        // The X360 asserts the static debug component still references a manager
        // ("mpDeformationManager != NULL", baked BrnDeformationDebugComponent.cpp:227), then
        // clears that back-pointer and destructs the base collision generator the static derives
        // from. Both reach the file-scope-static debug component; modelled by name through it.
        CGS_ASSERT(DeformationDebugComponent_HasManager(&mDebugComponent), "mpDeformationManager != NULL");
        DeformationDebugComponent_Destruct(&mDebugComponent);

        // No models live after teardown.
        mModelsAdded.UnSetAll();
    }

    // -----------------------------------------------------------------------------------
    // Prepare  @0x82630230
    //
    // Per-level prepare: register the debug component, allocate the pool of 28 DeformableObjects
    // through the resource allocator, placement-construct each, clear their per-object scratch +
    // ClearVariables, then reset the live-slot set + the IK round-robin cursor.
    // -----------------------------------------------------------------------------------
    bool DeformationManager::Prepare(rw::IResourceAllocator* lpAllocator)
    {
        // The static debug component joins the debug menu tree (DebugComponent::Register).
        DeformationDebugComponent_Register(&mDebugComponent);

        CGS_ASSERT(lpAllocator != nullptr, "lpAllocator != NULL");

        // (X360 logs "DeformationManager: array of deformation models required <bytes> bytes" when
        // the message filter bit is set -- a developer-only trace, no observable state.)

        // Allocate the 28-model pool through the resource allocator. The X360 builds a
        // resource-allocation descriptor (28 * sizeof(DeformableObject)) and asks the allocator
        // for it, then placement-constructs each of the 28 models (27..0). FLAG: the allocator
        // descriptor build + the typed allocation call are declare-only cross-TU surface; the pool
        // pointer is bound BY NAME below and each model is constructed via DeformableObject's ctor.
        mpaModels = AllocateDeformableModelPool(lpAllocator, KU_MAX_DEFORMATION_MODELS);

        if (mpaModels != nullptr)
        {
            for (s32 li = static_cast<s32>(KU_MAX_DEFORMATION_MODELS) - 1; li >= 0; --li)
            {
                // Placement-construct the model in its allocated slot (X360 stride 26496).
                new (&mpaModels[li]) DeformableObject();
            }
        }

        CGS_ASSERT(mpaModels != nullptr, "mpaModels != NULL");

        // Reset every model's per-object scratch to a quiescent state, then ClearVariables. The
        // X360 zeroes a small per-model header (the active flag / culling group / a few scratch
        // bytes) before calling ClearVariables. FLAG: the per-model header bytes the X360 pokes
        // (+26384/+26402/+26408/+26409/+26416/+26480) are interior DeformableObject fields not
        // reachable by name from here; the by-name ClearVariables call performs the model's own
        // reset (the pokes are folded into it).
        for (u32 lu = 0; lu < KU_MAX_DEFORMATION_MODELS; ++lu)
        {
            DeformableObject_ClearVariables(&mpaModels[lu]);
        }

        // No models live + the IK round-robin reset. (X360 re-zeroes the live-slot set word
        // *(this+75904) and the per-frame latch *(this+76672) at the tail of Prepare. It also
        // zeroes two INTERIOR sub-object fields -- *(this+75888) in mDetachedWheelManager's region
        // and *(this+48096) in mStateOutput's region -- which are not reachable by name from here;
        // FLAG: those two interior resets are folded into the sub-objects' own prepare and are
        // deferred until those interiors are homed. Prepare does NOT touch miPlayerModelIndex (it
        // is seeded to -1 once, in Construct).)
        mModelsAdded.UnSetAll();          // X360 *(this+75904) = 0
        miLastBodyToHaveIKUpdate = 0;     // X360 *(this+76672) = 0

        return true;
    }

    // -----------------------------------------------------------------------------------
    // Release  @0x82603F60
    //
    // Per-level release. The X360 simply clears the live-slot set and returns true.
    // -----------------------------------------------------------------------------------
    bool DeformationManager::Release()
    {
        mModelsAdded.UnSetAll();
        return true;
    }

    // -----------------------------------------------------------------------------------
    // ProcessEvents  @0x82644E38  (private)
    //
    // Per-scene-update dispatch of the deformation event queues, in order: deactivate, remove,
    // add, validate. Then, if the input interface flagged a player-scratch reset, zero the
    // player model's per-driven-point scratch and re-run its IK + skinning. Finally clear every
    // input queue's length (they are drained).
    // -----------------------------------------------------------------------------------
    void DeformationManager::ProcessEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                           DeformationInputInterface* lpInputInterface,
                                           CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        ProcessDeactivateDeformationModelEvents(lpSimInput, lpInputInterface, lpSceneInterface);
        ProcessRemoveDeformationModelEvents(lpSimInput, lpInputInterface, lpSceneInterface);
        ProcessAddDeformationModelEvents(lpSimInput, lpInputInterface, lpSceneInterface);
        ProcessValidateDeformationModelEvents(lpInputInterface);

        if (lpInputInterface->ShouldResetPlayerScratches())
        {
            DeformableObject* lpPlayerModel = GetPlayerCarModel();
            CGS_ASSERT(lpPlayerModel != nullptr, "lpPlayerModel");

            if (lpPlayerModel != nullptr)
            {
                // Zero the player's per-driven-point skinning scratch (the X360 walks the model's
                // mpDeformationSpec driven-point count, clearing one f32 per driven point at
                // model+432*i+6900). FLAG: the per-driven-point scratch field is interior to the
                // DeformableObject and is reset by the model's own UpdateSkinningOffsets below; the
                // explicit per-point clear is folded into that path.
                lpPlayerModel->UpdateIK(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });   // X360 passes vfTime = 0 here
                lpPlayerModel->UpdateSkinningOffsets();
            }
        }

        // Drain every input queue (the X360 zeroes each queue's miLength directly -- the queues
        // are consumed). ClearAllQueues is the by-name equivalent (additive interface accessor).
        lpInputInterface->ClearAllQueues();
    }

    // -----------------------------------------------------------------------------------
    // ProcessAddDeformationModelEvents  @0x82644828  (private)
    //
    // Drain the add-model queue: for each event whose entity is not already mapped to a model,
    // claim a free model slot, resolve + COM-transform its streamed spec, mark the slot live,
    // record its global id + the race-car / traffic index-table mapping, and Prepare the model.
    // -----------------------------------------------------------------------------------
    void DeformationManager::ProcessAddDeformationModelEvents(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            const DeformationInputInterface* lpInputInterface,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const CgsModule::EventQueue<AddDeformationModelEvent, 20>& lrQueue =
            lpInputInterface->GetAddDeformationModelQueue();

        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liI = 0; liI < liNumEvents; ++liI)
        {
            const AddDeformationModelEvent& lrEvent = lrQueue.GetEvent(liI);

            // Already simulating this entity? Skip (re-add is a no-op).
            const s32 liExisting = FindModelIndexByEntityID(lrEvent.mGlobalEntityId);
            if (liExisting != -1)
                continue;

            // Claim the first free model slot.
            const s32 liModelIndex = mModelsAdded.GetFirstClearBit();
            CGS_ASSERT(liModelIndex != static_cast<s32>(CgsContainers::BitArray<28u>::KI_INVALID_BITINDEX),
                       "Ran out of slots to add deformation model");

            // Resolve the streamed deformation spec from the event's resource handle and re-express
            // it in the event's centre-of-mass frame (the X360 resolves + transforms unconditionally
            // -- the handle always resolves for a queued add). FLAG: the resource-ptr resolution
            // (CreateFromHandle -> StreamedDeformationSpec*) is declare-only cross-TU surface; the
            // COM transform is the homed StreamedDeformationSpec call.
            StreamedDeformationSpec* lpSpec = ResolveDeformationSpec(lrEvent.mModelHandle);
            lpSpec->TransformToNewCOMSpace(lrEvent.mCOMOffset);

            // CgsBitArray.h:222 bounds tripwire (the X360 SetBit path streams
            // "Index: " << liModelIndex << ", Number of bits: " << 28 into the message buffer
            // before FireAssert). Reduced to the static streamed wording.
            CGS_ASSERT(static_cast<u32>(liModelIndex) < KU_MAX_DEFORMATION_MODELS,
                       "Index: liModelIndex, Number of bits: 28");

            // Mark the slot live, bump the live count, record the owning global entity id.
            mModelsAdded.SetBit(static_cast<u32>(liModelIndex));
            ++miNumUsedModels;
            maGlobalEntityIDs[liModelIndex] = lrEvent.mGlobalEntityId;

            // The handling body must be a race car or a traffic vehicle.
            const u32 luHandlingOwner = lrEvent.mHandlingBodyID.GetEntityIDOwner();
            CGS_ASSERT(luHandlingOwner == KU_ENTITYTYPE_RACECAR || luHandlingOwner == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                       "lHandlingBodyEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                       "lHandlingBodyEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

            CGS_ASSERT(liModelIndex >= 0,
                       "lModelIndex >= 0 && lModelIndex < static_cast<int32_t>( KU_MAX_DEFORMATION_MODELS )");

            // Map the per-class index -> this model slot. Race cars use the handling-body index
            // (the global id is asserted to be a race car); traffic vehicles use BOTH the physics
            // index (handling id) and the global-traffic index (global id).
            if (luHandlingOwner == KU_ENTITYTYPE_RACECAR)
            {
                const u32 lu16RaceCarIndex = lrEvent.mHandlingBodyID.GetEntityId().GetEntityIndex();
                CGS_ASSERT(lu16RaceCarIndex < static_cast<u32>(KI_MAX_NUM_RACE_CARS),
                           "lu16RaceCarIndex < Vehicle::ku8MaxNumRaceCars");
                ma8RaceCarToModelIndex[lu16RaceCarIndex] = static_cast<s8>(liModelIndex);
            }
            else
            {
                const u32 luGlobalOwner = lrEvent.mGlobalEntityId.muValue >> 24;
                CGS_ASSERT(luGlobalOwner == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
                           "lEvent.mGlobalEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

                const u32 lu16TrafficPhysicsIndex = lrEvent.mHandlingBodyID.GetEntityId().GetEntityIndex();
                const u32 lu16TrafficGlobalIndex  = (lrEvent.mGlobalEntityId.muValue >> 10) & 0x3FFFu;
                CGS_ASSERT(lu16TrafficPhysicsIndex < static_cast<u32>(KI_MAX_NUM_PHYSICAL_TRAFFIC),
                           "lu16TrafficPhysicsIndex < Vehicle::ku8TotalMaxNumPhysicalTraffic");
                CGS_ASSERT(lu16TrafficGlobalIndex < static_cast<u32>(KI_MAX_TOTAL_TRAFFIC),
                           "lu16TrafficGlobalIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
                ma8TrafficToModelIndex[lu16TrafficPhysicsIndex]      = static_cast<s8>(liModelIndex);
                ma8GlobalTrafficToModelIndex[lu16TrafficGlobalIndex] = static_cast<s8>(liModelIndex);
            }

            // Bring the model up: bind its spec, ids, transforms + seed velocities, register it
            // with the sim (mRandom passes as the last arg -- it lives at the manager's head).
            mpaModels[liModelIndex].Prepare(lpSimInput, static_cast<u16>(liModelIndex), lrEvent,
                                            lpSceneInterface, &mDetachedPartManager,
                                            &mDetachedWheelManager, mRandom);
        }
    }

    // -----------------------------------------------------------------------------------
    // ProcessRemoveDeformationModelEvents  @0x82641A00  (private)
    //
    // Drain the remove-model queue: for each event whose entity maps to a live model, Release the
    // model, clear its live-slot bit, decrement the live count, unmap its id table and clear the
    // per-class index-table entry.
    // -----------------------------------------------------------------------------------
    void DeformationManager::ProcessRemoveDeformationModelEvents(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            const DeformationInputInterface* lpInputInterface,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const CgsModule::EventQueue<RemoveDeformationModelEvent, 20>& lrQueue =
            lpInputInterface->GetRemoveDeformationModelQueue();

        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liI = 0; liI < liNumEvents; ++liI)
        {
            const RemoveDeformationModelEvent& lrEvent = lrQueue.GetEvent(liI);

            // The handling-body id identifies the model to remove.
            const EntityId lHandlingId = { static_cast<u32>(lrEvent.mHandlingBodyID.GetEntityId()) };
            const s32 liModelIndex = FindModelIndexByEntityID(lHandlingId);
            if (liModelIndex == -1)
                continue;

            CGS_ASSERT(liModelIndex >= 0 && liModelIndex < static_cast<s32>(KU_MAX_DEFORMATION_MODELS),
                       "liModelIndex >= 0 && liModelIndex < static_cast<int32_t>( KU_MAX_DEFORMATION_MODELS )");

            // Tear the model's sim + scene presence down.
            mpaModels[liModelIndex].Release(lpSimInput, lpSceneInterface,
                                            &mDetachedPartManager, &mDetachedWheelManager);

            // Clear the live-slot bit (CgsBitArray.h:241 bounds tripwire).
            CGS_ASSERT(static_cast<u32>(liModelIndex) < KU_MAX_DEFORMATION_MODELS, "luIndex < NUMBITS");
            mModelsAdded.UnSetBit(static_cast<u32>(liModelIndex));
            --miNumUsedModels;

            // Unmap the model's global-id slot (the X360 stores -1 into maGlobalEntityIDs[index]
            // and reads the prior id's index out of it for the index-table clears below).
            const u32 luPriorId = maGlobalEntityIDs[liModelIndex].muValue;
            maGlobalEntityIDs[liModelIndex].muValue = 0xFFFFFFFFu;
            const u32 luPriorGlobalIndex = (luPriorId >> 10) & 0x3FFFu;

            // Clear the per-class index-table entry. Race cars: the race-car table by the handling
            // id index. Traffic: both the physics-index table and the global-traffic table.
            const u32 luHandlingOwner = lrEvent.mHandlingBodyID.GetEntityIDOwner();
            if (luHandlingOwner == KU_ENTITYTYPE_RACECAR)
            {
                const u32 lu16RaceCarIndex = lrEvent.mHandlingBodyID.GetEntityId().GetEntityIndex();
                CGS_ASSERT(lu16RaceCarIndex < static_cast<u32>(KI_MAX_NUM_RACE_CARS),
                           "lu16RaceCarIndex < Vehicle::ku8MaxNumRaceCars");
                ma8RaceCarToModelIndex[lu16RaceCarIndex] = -1;
            }
            else
            {
                const u32 lu16TrafficPhysicsIndex = lrEvent.mHandlingBodyID.GetEntityId().GetEntityIndex();
                CGS_ASSERT(lu16TrafficPhysicsIndex < static_cast<u32>(KI_MAX_NUM_PHYSICAL_TRAFFIC),
                           "lu16TrafficPhysicsIndex < Vehicle::ku8TotalMaxNumPhysicalTraffic");
                CGS_ASSERT(luPriorGlobalIndex < static_cast<u32>(KI_MAX_TOTAL_TRAFFIC),
                           "lu16VehicleGlobalIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
                ma8TrafficToModelIndex[lu16TrafficPhysicsIndex] = -1;
                ma8GlobalTrafficToModelIndex[luPriorGlobalIndex] = -1;
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // ProcessDeactivateDeformationModelEvents  @0x82641C58  (private)
    //
    // Drain the deactivate-model queue: for each event whose entity maps to a live model, reset
    // that model's deformation (without removing it). The reset's "flag" is true only for the
    // player race car (owner == RACECAR && index == 0).
    // -----------------------------------------------------------------------------------
    void DeformationManager::ProcessDeactivateDeformationModelEvents(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            const DeformationInputInterface* lpInputInterface,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const CgsModule::EventQueue<DeactivateDeformationModelEvent, 28>& lrQueue =
            lpInputInterface->GetDeactivateDeformationModelQueue();

        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liI = 0; liI < liNumEvents; ++liI)
        {
            const DeactivateDeformationModelEvent& lrEvent = lrQueue.GetEvent(liI);

            const EntityId lHandlingId = { static_cast<u32>(lrEvent.mHandlingBodyID.GetEntityId()) };
            const s32 liModelIndex = FindModelIndexByEntityID(lHandlingId);
            if (liModelIndex == -1)
                continue;

            // The reset flag is set only for the player race car: owner byte == RACECAR (1) and the
            // packed index == 0 (the player slot). Any other case clears it.
            const u32 luOwner = lrEvent.mHandlingBodyID.GetEntityIDOwner();
            const u32 luIndex = lrEvent.mHandlingBodyID.GetEntityId().GetEntityIndex();
            const bool lbIsPlayerRaceCar = (luOwner == KU_ENTITYTYPE_RACECAR) && (luIndex == 0u);

            // Reset the model's deformation in place (keep the slot live). The damage/time + reset
            // type come from the event; mRandom passes as the last arg.
            mpaModels[liModelIndex].ResetDeformation(
                lpSimInput, lpSceneInterface, &mDetachedPartManager, &mDetachedWheelManager,
                VecFloat{ lrEvent.mfInitialDamageAmount, lrEvent.mfInitialDamageAmount,
                          lrEvent.mfInitialDamageAmount, lrEvent.mfInitialDamageAmount },
                lrEvent.meDeformationResetType, lbIsPlayerRaceCar, mRandom);
        }
    }

    // -----------------------------------------------------------------------------------
    // PostSceneUpdate  @0x82644F40  (26 insns -- ⭐ ADDED 2026-08-14, walls wave)
    //
    // The manager's per-scene-update tick: the two perfmon monitors (miTotalDeformationPerfMon
    // @this+76676, miPostSceneUpdatePerfMon @this+76680 -- `addis r30,r31,1 ; addi r30,r30,0x2B84`
    // / `...0x2B88`) bracket the ProcessEvents call, outer first, inner stopped first. That is the
    // whole function. Its conductor gate in BrnPhysicsConductorGates.cpp carries the runtime seam
    // until this TU mounts; the mount deletes that gate (LNK2005 says so loudly).
    // -----------------------------------------------------------------------------------
    void DeformationManager::PostSceneUpdate(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                             DeformationInputInterface* lpInputInterface,
                                             CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        CgsDev::PerfMonCpu::StartMonitor(miTotalDeformationPerfMon);      // 0x82644F68
        CgsDev::PerfMonCpu::StartMonitor(miPostSceneUpdatePerfMon);       // 0x82644F78

        ProcessEvents(lpSimInput, lpInputInterface, lpSceneInterface);    // 0x82644F8C

        CgsDev::PerfMonCpu::StopMonitor(miPostSceneUpdatePerfMon);        // 0x82644F94
        CgsDev::PerfMonCpu::StopMonitor(miTotalDeformationPerfMon);       // 0x82644F9C
    }

    // -----------------------------------------------------------------------------------
    // ProcessValidateDeformationModelEvents  @0x825DB0E0  (44 insns -- ⭐ ADDED 2026-08-14,
    // walls wave; the last of ProcessEvents' four drains to be bodied)
    //
    // Drain the validate queue (EventQueue<Vehicle::ValidateRaceCarEvent, 8> @interface+3856 --
    // `addi r27, r4, 0xF10`): for each event whose HIGH dword of mVolumeInstanceID maps to a live
    // model (`ld r11,8(r31) ; srdi r11,r11,32` -> FindModelIndexByEntityID), REFRESH that model's
    // cached spec pointer:
    //   * mbValidate != 0 -> model.mpDeformationSpec = *(mModelHandle.mpResourceMemory)
    //     (0x825DB14C `ld r10,0x10(r31)` the handle, then `lwz r9,0(r10)` ONE deref -- the same
    //      handle->spec resolve DeformableObject::Prepare @0x826421C0 performs, and the same
    //      *reinterpret_cast<T* const*>(mpResourceMemory) shape every CgsResourcePtr_* TU uses)
    //   * mbValidate == 0 -> model.mpDeformationSpec = NULL   (0x825DB174)
    // (the store target is model+0x18E0 == mpDeformationSpec, stride 0x6780 off mpaModels --
    // exactly the seat ProcessAdd's Prepare fills; reached via the additive SetDeformationSpec.)
    // -----------------------------------------------------------------------------------
    void DeformationManager::ProcessValidateDeformationModelEvents(const DeformationInputInterface* lpInputInterface)
    {
        const CgsModule::EventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent, 8>& lrQueue =
            lpInputInterface->GetValidateDeformationModelEvents();

        const s32 liNumEvents = lrQueue.GetLength();
        for (s32 liI = 0; liI < liNumEvents; ++liI)                       // 0x825DB110..0x825DB184
        {
            const BrnPhysics::Vehicle::ValidateRaceCarEvent& lrEvent = lrQueue.GetEvent(liI);

            // The entity word is the HIGH dword of the volume-instance id (0x825DB128 srdi 32).
            const s32 liModelIndex = FindModelIndexByEntityID(
                EntityId{ static_cast<u32>(lrEvent.mVolumeInstanceID.muId >> 32) });
            if (liModelIndex < 0)
                continue;                                                  // 0x825DB138

            if (lrEvent.mbValidate)                                        // 0x825DB140
            {
                // The one-deref handle->spec resolve (see the banner).
                mpaModels[liModelIndex].SetDeformationSpec(
                    *reinterpret_cast<StreamedDeformationSpec* const*>(lrEvent.mModelHandle.mpResourceMemory));
            }
            else
            {
                mpaModels[liModelIndex].SetDeformationSpec(nullptr);       // 0x825DB174
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // OutputData  @0x826225D8
    //
    // Output every live model's deformation/skinned/locator state into the two output interfaces,
    // emit the detached-part events, output every live model's wheel + joint state, then the
    // sensor state.
    // -----------------------------------------------------------------------------------
    void DeformationManager::OutputData(DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
                                        DeformationOutputInterface* lpOutput)
    {
        // First pass: for every live model slot, the X360 pushes its skinned-model base id +
        // skin/locator outputs into the entity-module + output interfaces, asserting the
        // destination count stays < 28 before each of three pushes.
        //
        // FLAG (GROW DEFERRAL -- NOT fabricated, NOT yet reproduced): the per-model skinned/locator
        // records the X360 copies are interior DeformableObject fields, and the destination tables
        // on the output interfaces are an opaque slice in the current reconstruction
        // (DeformationOutputInterfaceForEntityModules is a reserved blob; DeformationOutputInterface
        // does not yet model the skin/locator tables). Because those tables are not yet homed, the
        // three count-bound asserts AND the table writes are deliberately NOT emitted here -- only
        // the bit-walk is present. The three asserts to reinstate, in X360 order, are:
        //   1. "miNumSkinnedModels < (int32_t)KU_MAX_DEFORMATION_MODELS"  (BrnDeformationOutputInterface.h:622)
        //   2. "miNumLocatorOutputs < (int32_t)KU_MAX_DEFORMATION_MODELS" (BrnDeformationOutputInterface.h:634)
        //   3. "miNumLocatorOutputs < (int32_t)KU_MAX_DEFORMATION_MODELS" (BrnDeformationOutputInterface.h:500)
        // GROW: reinstate the asserts + the three table writes when the output-interface tables are homed.
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            (void)liModelIndex;
        }

        // Emit the detached-PART render + current-position events for every live part.
        mDetachedPartManager.OutputEvents(lpOutputForEntityModules, lpOutput);

        // Second pass: for every live model slot, output its wheel data + update/output its joint
        // states. (The X360 inlines a per-model wheel-direction normalisation immediately before
        // OutputWheelData; that precompute reads interior DeformableObject wheel-record fields and is
        // folded into the homed OutputWheelData call.)
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            mpaModels[liModelIndex].OutputWheelData(liModelIndex, lpOutputForEntityModules,
                                                    &mDetachedWheelManager);
            mpaModels[liModelIndex].UpdateAndOutputJointStates(lpOutput, &mDetachedPartManager);
        }

        // Finally, output every live model's sensor state into the output interface.
        OutputSensorState(lpOutput);
    }

    // ⚠️ DeformationManager::_AssertLayout IS NOT HOMED HERE. This TU is UNMOUNTED (see
    // build_game_exe.bat's `rem BrnDeformationManager.cpp  25 unresolved`), so a gate placed
    // here would never be compiled -- it was, briefly, and a tamper test caught it passing with
    // the very defect it was written to catch. It lives in the MOUNTED
    // BrnDeformationManager_Construct.cpp instead. Do not re-home it here until this TU mounts.
}
}
