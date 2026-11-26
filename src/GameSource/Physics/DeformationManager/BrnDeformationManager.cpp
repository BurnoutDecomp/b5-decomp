// ARTIST compiled this unit through the nested CgsBitArray.h include (its BitArray asserts name
// containers\./CgsBitArray.h, e.g. DeformationManager::BridgeBodyPartCarContactsToSimulation 0x825DD7D0): see CgsBitArray.h.
#define CGS_ARTIST_CGSBITARRAY_NESTED_INCLUDE
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"

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
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"        // PenetrationSolver (AddArticulatedJointContacts, walls leg 4)
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                               // PhysicsModuleIO::OutputBuffer (walls leg 4)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"                     // PotentialContactInterface + queue [12] (walls leg 4)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"                    // PotentialContact (queue [12] events, walls leg 4)
#include "rw/math/vpu/vector3_operation.h"                                                       // Negate (the `vxor` sign mask, walls leg 12)

namespace BrnPhysics
{
namespace Deformation
{
    // -----------------------------------------------------------------------------------
    // DWARF BrnDeformationManager.h namespace-scope rodata (:57-58, :752), defined here once
    // (the frozen header declares them extern). The friction pair is the detached-part
    // dynamic/static friction; the padding is the swept-sphere project radius pad.
    //
    // ⭐⭐ THE FRICTION PAIR IS RECOVERED (2026-09-07, part-rest wave). The old banner said the
    // X360 "loads them from a constant pool with no resolvable symbol" -- it does not. They are
    // ordinary initialised .data, not the dyn-init VMX splats that defeated five earlier sweeps
    // on the neighbouring 0x82FBxxxx statics, and they read straight out of the image:
    //     0x82F2A140  kfPartDynamicFriction  raw 3E99999A  = 0.30000001192092896f
    //     0x82F2A144  kfPartStaticFriction   raw 3E99999A  = 0.30000001192092896f
    // The addresses come from DeformationDebugComponent::OnActivate @0x82623198, which is the
    // only function in the whole image that names them:
    //     0x82623BA0  addi r27, r11, kfPartStaticFriction@l      -> 0x82F2A144
    //     0x82623BA4  raw 389BFFFC == addi r4, r27, -4           -> 0x82F2A140
    // SECOND, INDEPENDENT ATTESTATION (required before trusting an image read): the DecFIGS PS3
    // DWARF static-init dump carries the same bytes for the same two symbols --
    // references/DecFIGS/dwarfdump/_compile/BrnMain.cpp:7742,7744 give [62,153,153,154] ==
    // 0x3E99999A for both. Two builds, two mechanisms, identical bits.
    // CALIBRATION CONTROL that the reader is on-address: the adjacent block reads
    // kfNormalImpulseScale 0.05, kfFrictionImpulseScale 0.02, kfPartLinear/AngularDrag 0.005,
    // kfPartMax{Linear,Angular}Velocity 30.0, kfPartMass 100.0, kfPartInertiaMultiplier 1.2 --
    // the exact set PhysicalBodyPart::AddToSim @0x8260AD38 consumes by name -- while
    // 0x82FB9E00 (the rotation gate, a known dyn-init splat) still reads 00000000, so the reader
    // separates the two classes correctly.
    //
    // ⚠️ CONSOLE-DEAD, AND THAT IS FAITHFUL -- NOT A MISSING WIRE. An exhaustive search of all
    // 30,084 X360 ARTIST function exports and all 35,902 DecFIGS PS3 exports for these two
    // symbols returns exactly ONE function on each build, and it is the same one: the debug
    // component's RegisterVariable/SetStep call. No shipped code path on either console reads
    // them -- AddToSim sets a detached part's mass, drags, velocity caps and inertia multiplier
    // and never a friction coefficient. So the fact that nothing in this tree consumes them is
    // the console's own state; do not "connect" them to anything.
    // -----------------------------------------------------------------------------------
    const f32      KF_PART_DYNAMIC_FRICTION         = 0.30000001f;                // RECOVERED 0x82F2A140 (3E99999A)
    const f32      KF_PART_STATIC_FRICTION          = 0.30000001f;                // RECOVERED 0x82F2A144 (3E99999A)
    const VecFloat KVF_PROJECTSPHERE_RADIUS_PADDING(0.05f); // ARTIST initializer 0x82C5B7E0, flt_820047C8

    // The packed-entity-id owner tags this TU's asserts test against (BrnWorld::E_ENTITYTYPE_*;
    // the enum is not in-tree -- the high byte values are the asm-attested constants, matching
    // BrnPhysicalBodyPartPool.cpp / BrnPhysicalTrafficManager.cpp). FLAG: enum un-homed.
    static const u32 KU_ENTITYTYPE_RACECAR               = 1;
    static const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE       = 2;

    // The deformation-pool capacities the Construct memset loops + the add/remove bounds asserts
    // pin (8 race cars, 20 traffic, 600+1 global traffic). Local names; the table extents are
    // the frozen-header array bounds (ma8RaceCarToModelIndex[8] / ma8TrafficToModelIndex[20] /
    // ma8GlobalTrafficToModelIndex[600]; ARTIST Construct clears 0x258 bytes).
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
    // static perfmon-registration routines). Rather than edit a frozen header outside this group, they
    // are reached via declare-only stand-ins. FLAG: reconcile to `static` on DeformableObject when
    // that header is next revised. (The fourth stand-in, DeformableObject_ClearVariables, is retired:
    // Prepare now calls the public DWARF DeformableObject::Construct, which runs ClearVariables --
    // crash parity G23-D4.)
    void DeformableObject_ConstructUpdatePerformanceMonitors();
    void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
    void DeformableObject_ConstructPostPhysicsPerformanceMonitors();

    // -----------------------------------------------------------------------------------
    // Destruct  @0x82603F78
    //
    // Tear the manager + its collision generator down. Asserts the static debug component's
    // manager back-pointer is still wired, clears it, destructs the base collision generator,
    // then clears the manager's live-slot set.
    // -----------------------------------------------------------------------------------
    void DeformationManager::Destruct()
    {
        mDebugComponent.Destruct();
        mModelsAdded.Destruct();
    }

    // -----------------------------------------------------------------------------------
    // SetWorldBodyId  (DWARF BrnDeformationManager.h:156)
    //
    // ⭐ BODIED 2026-08-19 (wave Q6 round 4). It was DECLARED-ONLY -- an LNK2019 waiting for its
    // first caller, which is exactly what BrnPhysics::PhysicsModule::Prepare stage 8 now is.
    //
    // NO STANDALONE X360 SYMBOL: the console inlines it, and the one place it is inlined is also
    // its whole attestation -- PhysicsModule::Prepare @0x825ADE38..0x825ADE54, immediately after
    // PrepareWorldRigidBody returns:
    //     lis  r11, 6 ; ori r11, r11, 0x9BB8   // 0x69BB8 == 433080 == PhysicsModule::mWorldRigidBodyId
    //     lis  r10, 5 ; ori r10, r10, 0xF498   // 0x5F498 == 390296
    //     ldx  r11, r30, r11                   // an EIGHT-byte load...
    //     stdx r11, r30, r10                   // ...and an EIGHT-byte store
    // 390296 - 314272 (PhysicsModule's mDeformationManager anchor) == 76024, which is this class's
    // own `mWorldRigidBodyId` seat (see this header's +76024 note and its adjacency assert). One
    // 8-byte store of the argument, nothing else -- so the body is that store and no more. The
    // 8-byte width is corroboration, not assumption: it is the SAME `stdx` pairing that settled
    // both handles' widths on the PhysicsModule side.
    // -----------------------------------------------------------------------------------

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

        CGS_ASSERT(lpAllocator != 0, "lpAllocator != NULL");

        // (X360 logs "DeformationManager: array of deformation models required <bytes> bytes" when
        // the message filter bit is set -- a developer-only trace, no observable state.)

        // Allocate the 28-model pool through the resource allocator. The X360 builds a
        // resource-allocation descriptor (28 * sizeof(DeformableObject)) and asks the allocator
        // for it, then placement-constructs each of the 28 models (27..0). FLAG: the allocator
        // descriptor build + the typed allocation call are declare-only cross-TU surface; the pool
        // pointer is bound BY NAME below and each model is constructed via DeformableObject's ctor.
        mpaModels = AllocateDeformableModelPool(lpAllocator, KU_MAX_DEFORMATION_MODELS);

        if (mpaModels != 0)
        {
            for (s32 li = static_cast<s32>(KU_MAX_DEFORMATION_MODELS) - 1; li >= 0; --li)
            {
                // Placement-construct the model in its allocated slot (X360 stride 26496).
                new (&mpaModels[li]) DeformableObject();
            }
        }

        CGS_ASSERT(mpaModels != 0, "mpaModels != NULL");
        // [marked deviation, 2026-08-14] the console has no null path here (its carve cannot
        // fail); on the host a failed carve would fall through into the ClearVariables loop and
        // AV (boot-measured before the rw-linear host headroom landed in BrnGameDataModule.cpp).
        // Guard the host: report not-ready so the prepare FSM holds this stage.
        if (mpaModels == 0)
            return false;

        // Construct every model (crash parity G23-D4, 2026-09-23): 0x826303B4..0x826303CC is the
        // inlined DeformableObject::Construct (PS3 out of line @0x6BEFC4) -- six stores
        // (mHandlingBodyID +0x6710, mbActive +0x6722, mbHasDeformedThisFrame +0x6728,
        // mbIKUpdateRequired +0x6729, miNumBrokenWheels +0x6770, mbResetDeformationNextUpdate
        // +0x6730) and then `bl ClearVariables`. The old comment said those pokes were "folded into"
        // ClearVariables; neither console's ClearVariables makes them, so the tree left six fields
        // holding the allocator's bytes.
        for (u32 lu = 0; lu < KU_MAX_DEFORMATION_MODELS; ++lu)
        {
            mpaModels[lu].Construct();
        }

        // The tail, store for store (0x826303DC..0x82630410; PS3 Prepare 0x739BC8 the same):
        //   0x826303F8/FC  std 0 -> this+0x12880 (twice)   mModelsAdded
        //   0x82630404     std 0 -> this+0x12870           the inlined DetachedWheelManager::Prepare
        //                                                  (its used-set; PS3 calls it @0x6B4C60)
        //   0x8263040C     std 0 -> this+0xBBE0            mStateOutput.mxLiveSlots (this+0x30 + 48048)
        //   0x82630410     stw 0 -> this+0x12B80           miLastBodyToHaveIKUpdate
        // (crash parity G23-D3: the two middle stores used to sit behind a FLAG calling them "not
        // reachable by name" -- both members are named; the wheel manager's Prepare result is ignored
        // by the console.) Prepare does NOT touch miPlayerModelIndex (seeded to -1 once, in Construct).
        mModelsAdded.UnSetAll();
        mDetachedWheelManager.Prepare();
        mStateOutput.mxLiveSlots.UnSetAll();
        miLastBodyToHaveIKUpdate = 0;

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
    // player model's per-sensor scratch (ResetScratching) and re-run its IK + skinning. Finally clear every
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
            CGS_ASSERT(lpPlayerModel != 0, "lpPlayerModel");

            // Zero every deformation sensor's scratch, then re-blend (crash parity G24-D1,
            // 2026-09-23): 0x82644ED0..0x82644F04 is the inlined DeformableObject::ResetScratching
            // (PS3 calls it out of line @0x6B9D7C; ProcessEvents is its only caller) -- spec+0x652
            // sensors, `stfs 0.0, 0x1AF4(model + 0x1B0*i)` == maDeformationSensors[i].mfScratchAmount.
            // The old FLAG here called it a per-driven-point clear "folded into
            // UpdateSkinningOffsets"; the stride and base make it per-SENSOR, and
            // UpdateSkinningOffsets never clears it -- so the respray re-blended the old scratch.
            lpPlayerModel->ResetScratching();
            const VecFloat lvfZero(__vspltisw(0));
            lpPlayerModel->UpdateIK(lvfZero);   // 0x82644F0C vspltisw v1,0
            lpPlayerModel->UpdateSkinningOffsets();
        }

        // Drain every input queue (the X360 zeroes each queue's miLength directly -- the queues
        // are consumed). ClearAllQueues is the by-name equivalent (additive interface accessor).
        lpInputInterface->Clear();
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
            // ⭐ THE ID IS THE HANDLING BODY'S (LOCAL PHYSICS) ENTITY WORD, NOT mGlobalEntityId
            // (traffic wave 3 round 3). FindModelIndexByEntityID indexes ma8TrafficToModelIndex,
            // which is keyed by the PHYSICAL slot (0..19) -- feeding it the global traffic index
            // (64 on the first promoted car) tripped its `lu16TrafficIndex <
            // ku8TotalMaxNumPhysicalTraffic` bound 24x in one boot. X360 @0x82644934..0x826449D8:
            //     ld    r9, 8(r11)     ; event+0x08 == mHandlingBodyID (the 8-byte RigidBodyId)
            //     srdi  r9, r9, 32     ; RigidBodyId::GetEntityId() -- the HIGH dword
            //     clrlwi r26, r9, 0
            //     mr    r4, r26
            //     bl    DeformationManager::FindModelIndexByEntityID
            // mGlobalEntityId (event+0x10) is read later, for the owner assert and the
            // ma8GlobalTrafficToModelIndex mapping -- the two ids are NOT interchangeable.
            const EntityId lHandlingEntityId =
                { static_cast<u32>(lrEvent.mHandlingBodyID.GetEntityId()) };
            const s32 liExisting = FindModelIndexByEntityID(lHandlingEntityId);
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
            //
            // ⭐⭐⭐ THE ARGUMENT IS NEGATED, AND THAT IS THE WHOLE POINT OF THIS CALL SITE
            // (walls leg 12, 2026-08-16 -- this line was missing its `vxor` for eleven legs).
            // X360 @0x82644AF0..0x82644B04, immediately before the `bl`:
            //     vspltisw v0, -1  ;  vslw v0, v0, v0      -> 0x80000000 in every lane
            //     lvx128   v13, r0, r11                    -> r11 == &(event copy + 0x20) == mCOMOffset
            //     vxor     v1, v13, v0                     -> v1 = -mCOMOffset      <-- THE SIGN
            //     bl       StreamedDeformationSpec::TransformToNewCOMSpace
            // PS3 @0x76ADE4..0x76AE00 emits the identical five instructions
            // (`vspltisw v2,-1 / lvx v0,r1,0xC0 / vslw v2,v2,v2 / vxor v2,v2,v0 / bl`), so the
            // negation is double-witnessed and is not a one-build peculiarity.
            //
            // WHY IT MUST BE NEGATED -- TransformToNewCOMSpace itself proves it. That function moves
            // every geometry point by `+= (new - old)` but pulls `mMeshOffset` by `-= (new - old)`
            // (X360 0x825E31C0 vs 0x825E31E0; PS3 0x6C9920 vs 0x6C9A14). A frame change can only
            // move one of those two ways, so the two are consistent ONLY if the incoming value is
            // already the negated centre of mass. Two independent consumers agree:
            //   * DeformableObject::GetBoundingBox @0x825E8620 places the handling-body box (which is
            //     centred on the MODEL origin) at `transform.pos + *(spec+1632)`, and transform.pos
            //     IS the COM -- so the stored mCurrentCOMOffset has to be -COM to name the model
            //     origin;
            //   * SimpleVehiclePhysics::SetAttributes @0x82602828 rebases the WHEELS by
            //     `position - mCOMOffset`, and mCOMOffset is literally built as the mean of those
            //     same wheel positions (ProcessCreateEvents @0x82616D2C..), so the wheels land in a
            //     zero-mean COM frame. The sensors must land in the SAME frame or the two halves of
            //     the car end up 2*COM apart -- which is exactly the 0.807 m split walls leg 11
            //     measured, and the 0.764 m hover it produced.
            // ⛔ The two OTHER callers (VehicleManager::ProcessCreateEvents @0x82616D00,
            // PhysicalTrafficManager::PreparePhysicsForNewTrafficVehicle @0x82644204) pass their
            // value UNNEGATED -- checked, no vxor at either -- but they call it with the *authored*
            // COM tweak BEFORE the four-wheel mean is folded in, which is a different (and, for the
            // shipped starter car, zero) quantity. Both are reproduced as the consoles have them.
            StreamedDeformationSpec* lpSpec = ResolveDeformationSpec(lrEvent.mModelHandle);
            lpSpec->TransformToNewCOMSpace(rw::math::vpu::Negate(lrEvent.mCOMOffset));

            // CgsBitArray.h:222 bounds tripwire (the X360 SetBit path streams
            // "Index: " << liModelIndex << ", Number of bits: " << 28 into the message buffer
            // before FireAssert). Reduced to the static streamed wording.

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

            // ⭐ THE LOOP-INVARIANT ROW ZERO (crash parity G24-D2, 2026-09-23). Unconditionally after
            // Prepare (its result is ignored), both consoles zero ONE 16-byte row:
            //     0x82644DB8 mulli r11,r31(model),0x700 ; 0x82644DBC vspltisw v0,0 ; add this ;
            //     addi 0x60 ; 0x82644DC4..D4 an EMPTY `li r10,0x14` countdown ; 0x82644DDC stvx128 v0
            //     == this + 0x60 + 0x700*model == mStateOutput(+0x30) + 0x6B0*model + 0x50*model + 0x30
            // i.e. maCarStates[model].maSensors[model].mDisplacementDelta. PS3 0x76AF48..0x76AF74 is the
            // same address, empty `bdnz 20` loop and stvx; the DWARF scopes an `int32_t liSensor` here
            // (BrnDeformationManager.cpp:315) -- a 20-sensor clear loop whose store indexes the MODEL,
            // so both compilers hoisted it. The empty count loop is not reproduced. Past sensor 19 the
            // row runs off the record, and the host spells each landing BY NAME (DeformationState is
            // pointer-free, so the console arithmetic is the host's too; the detached-part pool is not,
            // so slot 27 is reached through accessors, never an offset):
            //   model 0..19  -> maCarStates[m].maSensors[m] + 0x30         (mDisplacementDelta)
            //   model 20     -> maCarStates[20] + 0x670                     (maWheelTagPoints[1])
            //   model 21..26 -> maCarStates[m+1].maSensors[m-21] + 0x10     (mWorldScalarVector)
            //   model 27     -> mStateOutput end + ... == DetachedPartManager+0x170 (this+0xBD60 -
            //                   0xBBF0) == pool slot 0's mLocalGraphicsPositionPlusJointVelocity,
            //                   whether or not slot 0 is in use (no GetPart tripwires on console).
            // Only the model-27 arm is observable: OutputState rewrites the CarState rows before any
            // reader, while the part row carries a live part's render offset and hinge velocity.
            const Vector3 lZeroRow(__vspltisw(0));
            const s32 liSensorsPerCar = static_cast<s32>(CarState::KU_MAX_SENSORS);   // 20
            if (liModelIndex < liSensorsPerCar)
            {
                mStateOutput.maCarStates[liModelIndex].maSensors[liModelIndex].mDisplacementDelta = lZeroRow;
            }
            else if (liModelIndex == liSensorsPerCar)
            {
                mStateOutput.maCarStates[liModelIndex].maWheelTagPoints[1] = lZeroRow;
            }
            else if (liModelIndex < static_cast<s32>(KU_MAX_DEFORMATION_MODELS) - 1)
            {
                mStateOutput.maCarStates[liModelIndex + 1].maSensors[liModelIndex - (liSensorsPerCar + 1)].mWorldScalarVector = lZeroRow;
            }
            else
            {
                const Vector3Plus lZeroPosition(__vspltisw(0));
                mDetachedPartManager.GetPartPool().GetPartSlot(0).SetLocalGraphicsPositionPlusJointVelocity(lZeroPosition);
            }
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
            const VecFloat lvfDamage(lrEvent.mfInitialDamageAmount);
            mpaModels[liModelIndex].ResetDeformation(
                lpSimInput, lpSceneInterface, &mDetachedPartManager, &mDetachedWheelManager,
                lvfDamage,
                lrEvent.meDeformationResetType, lbIsPlayerRaceCar, mRandom);
        }
    }

    // -----------------------------------------------------------------------------------
    // PostSceneUpdate  @0x82644F40  (26 insns -- ⭐ ADDED 2026-08-14, walls wave)
    //
    // The manager's per-scene-update tick: the two perfmon monitors (miTotalDeformationPerfMon
    // @this+76676, miPostSceneUpdatePerfMon @this+76680 -- `addis r30,r31,1 ; addi r30,r30,0x2B84`
    // / `...0x2B88`) bracket the ProcessEvents call, outer first, inner stopped first. That is the
    // whole function.
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
            const EntityId lEntity = { static_cast<u32>(lrEvent.mVolumeInstanceID.muId >> 32) };
            const s32 liModelIndex = FindModelIndexByEntityID(lEntity);
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
                mpaModels[liModelIndex].SetDeformationSpec(0);       // 0x825DB174
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // OutputData -- ⭐ SPLIT OUT 2026-08-14 (deformation-mount wave) to the slice
    // BrnDeformationManager_Output.cpp, exactly as the walls-wave census prescribed. That slice
    // is mounted and carries the real body; see its banner for OutputData's own closure.
    // -----------------------------------------------------------------------------------

    // ==========================================================================================
    // GetPlayerCarModel @ 0x825B44F0  (DWARF spelling truncates to "GetPlaye")
    // ⭐ MOVED HERE 2026-08-14 (deformation-mount wave) from the still-unmounted
    // BrnDeformationManager_Contacts.cpp -- this TU mounts this wave and the mount needs it.
    //
    // The player car's deformable model. Asserts the player's deformation model is active
    // (miPlayerModelIndex >= 0), then returns &mpaModels[miPlayerModelIndex].
    // ==========================================================================================
    DeformableObject* DeformationManager::GetPlayerCarModel()
    {
        CGS_ASSERT(miPlayerModelIndex >= 0,
                   "Trying to access deformable object for player when it isn't active");
        return &mpaModels[miPlayerModelIndex];
    }

    // ⚠️ DeformationManager::_AssertLayout IS NOT HOMED HERE. This TU is UNMOUNTED (see
    // build_game_exe.bat's `rem BrnDeformationManager.cpp  25 unresolved`), so a gate placed
    // here would never be compiled -- it was, briefly, and a tamper test caught it passing with
    // the very defect it was written to catch. It lives in the MOUNTED
    // BrnDeformationManager_Construct.cpp instead. Do not re-home it here until this TU mounts.

    // =============================================================================================
    // WALLS LEG 4 (2026-08-14): the per-frame conductor pair + the articulated-joint feeder land.
    // =============================================================================================

    // The dev-menu deformation-debug master toggle (home: BrnDeformationConstructShims.cpp,
    // default false == the X360 .bss byte). Update's player-IK arm reads it, as on console.
    extern bool kbAllowDeformationDebug;

    namespace
    {
        // BrnPhysics::KI_MAX_NUM_OF_IK_AND_LOCATOR_UPDATES -- the per-frame IK budget for
        // non-player models. IMAGE-READ ground truth: X360 .data dword_82F2A238 == 2 (the manager
        // Update's `lwz r11, dword_82F2A238` compare operand); PS3 names the global verbatim.
        const s32 KI_MAX_NUM_OF_IK_AND_LOCATOR_UPDATES = 2;
    }

    // =============================================================================================
    // Update @0x82649B40 (1021; PS3 0x765DB0, 925 -- the PS3 names every member) -- THE PER-STEP
    // DEFORMATION CONDUCTOR. Flow (both consoles agree; monitors are the +76676.. member ids):
    //   StartMonitor(miTotalDeformationPerfMon); StartMonitor(miUpdatePerfMon);
    //   (1) per live model: clear mbHasDeformedThisFrame (model+26408 = 0);
    //   (2) StartMonitor(miUpdateModelsPerfMon); per live model:
    //         if (model.Update(simIn, simOut, physIn, physOut, timeStep, &partMgr, &wheelMgr,
    //                          contacts, mRandom, gameMode))
    //             lUpdatedModels.SetBit(i), ++liNumUpdated;
    //       StopMonitor;
    //   (3) StartMonitor(miUpdateIkAndDetachingPerfMon);
    //       the PLAYER model IK's every updated frame (or under the debug toggle) and its bit is
    //       cleared from the budget set;
    //   (4) round-robin min(KI_MAX_NUM_OF_IK_AND_LOCATOR_UPDATES, liNumUpdated) other models
    //       through miLastBodyToHaveIKUpdate (GetNextNonZeroBit, wrap via GetFirstNonZeroBit;
    //       asserts :788 "miLastBodyToHaveIKUpdate != -1" and :789 "... != miPlayerModelIndex");
    //       StopMonitor;
    //   (5) StartMonitor(miUpdateDetachedPartsPerfMon); the detached-part pool RW step
    //       (X360 inlines DetachedPartManager::Update == mPartPool.UpdateRWBodies); StopMonitor;
    //   (6) the miNumUsedModels == live-bit-count tripwire (:817);
    //   StopMonitor(miUpdatePerfMon); StopMonitor(miTotalDeformationPerfMon).
    // =============================================================================================
    // =============================================================================================
    // UpdateSensorDisplacements (189 insns) -- ⭐⭐⭐ BODIED 2026-08-16 (walls leg 10); it had
    // no body before that, only an inert log-once gate.
    //
    // The whole function is two perf-mon monitors around a walk of the live-model set, calling
    // DeformableObject::UpdateSensorDisplacements on each with the SAME time step:
    //   0x82604028  StartMonitor(*(this+76676))            ; miTotalDeformationPerfMon
    //   0x8260403C  StartMonitor(*(this+76684))            ; miUpdateSensorDisplPerfMon
    //   0x8260404C..0x826042CC  the inlined BitArray<28> GetFirstNonZeroBit / GetNextNonZeroBit
    //                           walk over mModelsAdded (this+75904), with the CgsBitArray.h:203
    //                           "invalid index : N < 28" tripwire inside the walk
    //   0x826040F4  vmr128 v1, v127                        ; re-splat the saved VecFloat argument
    //   0x826040FC  bl DeformableObject::UpdateSensorDisplacements(26496 * index + *(this+76032))
    //                                                      ; 26496 == sizeof(DeformableObject),
    //                                                      ; this+76032 == mpaModels
    //   0x826042D8/E0  StopMonitor in the reverse order
    //
    // ⛔⛔ WHY THE STUB WAS EXPENSIVE. PhysicsModule::Update calls this EVERY FRAME
    // (BrnPhysicsModuleUpdateFunctions.cpp:541), so the stub was a textbook silent drop: it
    // compiled, linked, ran, copied nothing, and left every sensor's
    // mPointDisplacement_BiggestImpulseThisFrame at the zero ClearVariables set it. That vector is
    // the denominator of ValidateAndAddContact's impact-time latch, so with it zero the latch's
    // `basis > 0` gate could never pass and NO contact -- wall or floor -- could ever become a
    // deformation impulse. Witnessed live before the fix as `disp 0.000000 0.000000 0.000000` on
    // every one of 24 wall-face contacts taken at 30.4 m/s.
    // =============================================================================================
    void DeformationManager::UpdateSensorDisplacements(VecFloat lvfTimeStep)
    {
        CgsDev::PerfMonCpu::StartMonitor(miTotalDeformationPerfMon);
        CgsDev::PerfMonCpu::StartMonitor(miUpdateSensorDisplPerfMon);

        for ( s32 liModel = mModelsAdded.GetFirstNonZeroBit(); liModel != -1;
              liModel = mModelsAdded.GetNextNonZeroBit(liModel) )
        {
            mpaModels[liModel].UpdateSensorDisplacements(lvfTimeStep);
        }

        CgsDev::PerfMonCpu::StopMonitor(miUpdateSensorDisplPerfMon);
        CgsDev::PerfMonCpu::StopMonitor(miTotalDeformationPerfMon);
    }

    void DeformationManager::Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
                                    const PhysicsModuleIO::InputBuffer* lpInputBuffer,
                                    PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                                    PhysicsModuleIO::PotentialContactInterface* lpContacts,
                                    VecFloat lvfTimeStep, s32 leGameMode)
    {
        CgsDev::PerfMonCpu::StartMonitor(miTotalDeformationPerfMon);
        CgsDev::PerfMonCpu::StartMonitor(miUpdatePerfMon);

        // ---- (1) clear the per-frame deformed latch on every live model -------------------------
        for ( s32 liModel = mModelsAdded.GetFirstNonZeroBit(); liModel != -1;
              liModel = mModelsAdded.GetNextNonZeroBit(liModel) )
        {
            mpaModels[liModel].ClearHasDeformedThisFrame();   // model+26408 = 0
        }

        // ---- (2) per-model Update; collect who deformed -----------------------------------------
        s32 liNumUpdated = 0;
        CgsContainers::BitArray<28u> lUpdatedModels;
        lUpdatedModels.UnSetAll();

        CgsDev::PerfMonCpu::StartMonitor(miUpdateModelsPerfMon);
        for ( s32 liModel = mModelsAdded.GetFirstNonZeroBit(); liModel != -1;
              liModel = mModelsAdded.GetNextNonZeroBit(liModel) )
        {
            if ( mpaModels[liModel].Update(lpSimInput, lpSimOutput, lpInputBuffer, lpOutputBuffer,
                                           lvfTimeStep, &mDetachedPartManager, &mDetachedWheelManager,
                                           lpContacts, mRandom, leGameMode) )
            {
                lUpdatedModels.SetBit(static_cast<u32>(liModel));   // ("Index: %d, Number of bits: 28" bound assert inside)
                ++liNumUpdated;
            }
        }
        CgsDev::PerfMonCpu::StopMonitor(miUpdateModelsPerfMon);

        // ---- (3) the player model IK's every updated frame --------------------------------------
        CgsDev::PerfMonCpu::StartMonitor(miUpdateIkAndDetachingPerfMon);
        if ( miPlayerModelIndex != -1 )
        {
            if ( lUpdatedModels.IsBitSet(static_cast<u32>(miPlayerModelIndex))
                 || kbAllowDeformationDebug )
            {
                mpaModels[miPlayerModelIndex].UpdateIKAndLocators(
                    lpSimInput, lpOutputBuffer, lvfTimeStep,
                    &mDetachedPartManager, &mDetachedWheelManager, &mRandom);
                lUpdatedModels.UnSetBit(static_cast<u32>(miPlayerModelIndex));
                --liNumUpdated;
            }
        }

        // ---- (4) round-robin the IK budget over the remaining updated models --------------------
        s32 liNumToUpdate = liNumUpdated;
        if ( liNumToUpdate > KI_MAX_NUM_OF_IK_AND_LOCATOR_UPDATES )
        {
            liNumToUpdate = KI_MAX_NUM_OF_IK_AND_LOCATOR_UPDATES;
        }
        for ( s32 li = 0; li < liNumToUpdate; ++li )
        {
            miLastBodyToHaveIKUpdate = lUpdatedModels.GetNextNonZeroBit(miLastBodyToHaveIKUpdate);
            if ( miLastBodyToHaveIKUpdate == -1 )
            {
                miLastBodyToHaveIKUpdate = lUpdatedModels.GetFirstNonZeroBit();   // wrap
            }
            CGS_ASSERT(miLastBodyToHaveIKUpdate != -1,
                       "miLastBodyToHaveIKUpdate != -1");                         // :788
            CGS_ASSERT(miLastBodyToHaveIKUpdate != miPlayerModelIndex,
                       "miLastBodyToHaveIKUpdate != miPlayerModelIndex");         // :789
            if ( miLastBodyToHaveIKUpdate == -1 )
            {
                break;   // host guard behind the fire-and-continue tripwire (empty set)
            }
            mpaModels[miLastBodyToHaveIKUpdate].UpdateIKAndLocators(
                lpSimInput, lpOutputBuffer, lvfTimeStep,
                &mDetachedPartManager, &mDetachedWheelManager, &mRandom);
        }
        CgsDev::PerfMonCpu::StopMonitor(miUpdateIkAndDetachingPerfMon);

        // ---- (5) the detached-part RW step (X360 inlines DetachedPartManager::Update) -----------
        CgsDev::PerfMonCpu::StartMonitor(miUpdateDetachedPartsPerfMon);
        mDetachedPartManager.Update(lpSimInput, lvfTimeStep);
        CgsDev::PerfMonCpu::StopMonitor(miUpdateDetachedPartsPerfMon);

        // ---- (6) the used-model count tripwire (:817) -------------------------------------------
        s32 liNumModels = 0;
        for ( u32 lu = 0; lu < 28u; ++lu )
        {
            if ( mModelsAdded.IsBitSet(lu) )
            {
                ++liNumModels;
            }
        }
        CGS_ASSERT(miNumUsedModels == liNumModels, "miNumUsedModels == liNumModels");   // :817

        CgsDev::PerfMonCpu::StopMonitor(miUpdatePerfMon);
        CgsDev::PerfMonCpu::StopMonitor(miTotalDeformationPerfMon);
    }

    // =============================================================================================
    // UpdatePostPhysics @0x82630420 (236; PS3 0x767800, 827) -- the post-physics conductor:
    //   StartMonitor(miTotalDeformationPerfMon); StartMonitor(miUpdatePostPhysicsPerfMon);
    //   sceneIface = physModOut->GetSceneInputInterface();
    //   SolvePenetration(ioStack, contacts);          (the two-pass positional solve + read-back)
    //   StartMonitor(miPostPhysicsUpdateModelsPerfMon);
    //     per live model: model.UpdatePostPhysics(sceneIface);
    //   StopMonitor;
    //   StartMonitor(miPostPhysicsUpdateDetachedPartsManPerfMon);
    //     mDetachedPartManager pool AddPartsToScene(sceneIface);
    //     mDetachedPartManager.UpdatePostPhysics(simOut, sceneIface, spyData, contacts);
    //     mDetachedWheelManager.UpdatePostPhysics(simOut, sceneIface);
    //   StopMonitor; StopMonitor; StopMonitor.
    // =============================================================================================
    void DeformationManager::UpdatePostPhysics(const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
                                               PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                                               ContactSpy::ContactSpyData* lpContactSpyData,
                                               IOBufferStack* lpIOBufferStack,
                                               const PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        CgsDev::PerfMonCpu::StartMonitor(miTotalDeformationPerfMon);
        CgsDev::PerfMonCpu::StartMonitor(miUpdatePostPhysicsPerfMon);

        // The module-output scene interface, through the same reinterpret seam the mounted
        // PhysicsModule::Update read-backs use (the storage member is opaque on the host).
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface =
            reinterpret_cast<CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*>(
                lpOutputBuffer->GetSceneInputInterface());

        SolvePenetration(lpIOBufferStack, lpContacts);

        CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateModelsPerfMon);
        for ( s32 liModel = mModelsAdded.GetFirstNonZeroBit(); liModel != -1;
              liModel = mModelsAdded.GetNextNonZeroBit(liModel) )
        {
            mpaModels[liModel].UpdatePostPhysics(lpSceneInterface);
        }
        CgsDev::PerfMonCpu::StopMonitor(miPostPhysicsUpdateModelsPerfMon);

        CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateDetachedPartsManPerfMon);
        mDetachedPartManager.AddPartsToScene(lpSceneInterface);
        // ⭐ THE CAST IS GONE 2026-09-06 (contact-spy wave). This read
        // `reinterpret_cast<ContactSpyData*>(lpContactSpyData)` and was flagged "fork to
        // reconcile": the whole chain below it was declared over a PHANTOM
        // Deformation::ContactSpyData. Bodying PhysicalBodyPart::AddContactSpy made the pointer
        // load-bearing (it has to reach mHingedPartContactQueue by name), so the phantom was
        // retired and the chain now carries BrnPhysics::ContactSpy::ContactSpyData end to end.
        mDetachedPartManager.UpdatePostPhysics(lpSimOutput, lpSceneInterface,
                                               lpContactSpyData,
                                               lpContacts);
        mDetachedWheelManager.UpdatePostPhysics(lpSimOutput, lpSceneInterface);
        CgsDev::PerfMonCpu::StopMonitor(miPostPhysicsUpdateDetachedPartsManPerfMon);

        CgsDev::PerfMonCpu::StopMonitor(miUpdatePostPhysicsPerfMon);
        CgsDev::PerfMonCpu::StopMonitor(miTotalDeformationPerfMon);
    }

    // =============================================================================================
    // AddArticulatedJointContacts @0x825DB190 (162; PS3 0x739FAC, 566 -- names + asserts) -- drain
    // the articulated-joint (traffic cab/trailer) contact queue [12] into the penetration solver.
    // Per event: assert BOTH volume-instance owners are TRAFFIC_VEHICLE (:1033/:1034); map both
    // entity words to model indices (the same table lookups FindModelIndexByEntityID inlines,
    // with its own :701/:710 bounds asserts); stream "Failed to find deformation model for cab"
    // on a -1 (:1039/:1040, fire-and-continue -- the console adds the contact regardless);
    // AddVehicleContact(pointOnA, pointOnB, normal, cabModel, trailerModel).
    // Queue [12] is EMPTY offline (no articulated traffic on the junkyard path) -- a live,
    // empty walk.
    // =============================================================================================
    void DeformationManager::AddArticulatedJointContacts(PenetrationSolver* lpSolver,
                                                         const PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        const PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue& lrQueue =
            lpContacts->GetArticulatedJointQueue();   // maCustomEventQueues[12]

        const s32 liNumEvents = lrQueue.GetLength();
        for ( s32 li = 0; li < liNumEvents; ++li )
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact& lrEvent = lrQueue.GetEvent(li);

            const u32 luCabWord     = static_cast<u32>(lrEvent.muVolumeInstanceIdA.muId >> 32);
            const u32 luTrailerWord = static_cast<u32>(lrEvent.muVolumeInstanceIdB.muId >> 32);

            CGS_ASSERT((luCabWord >> 24) == 2u,
                       "lCabEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");      // :1033
            CGS_ASSERT((luTrailerWord >> 24) == 2u,
                       "lTrailerEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");  // :1034

            const EntityId lCabEntity = { luCabWord };
            const EntityId lTrailerEntity = { luTrailerWord };
            const s32 liCabModel     = FindModelIndexByEntityID(lCabEntity);
            const s32 liTrailerModel = FindModelIndexByEntityID(lTrailerEntity);

            // :1039/:1040 -- the console STREAMS the failure and adds the contact anyway
            // (fire-and-continue asserts). Reproduced: tripwire, then add.
            CGS_ASSERT(liCabModel != -1,     "Failed to find deformation model for cab ");   // :1039
            CGS_ASSERT(liTrailerModel != -1, "Failed to find deformation model for cab ");   // :1040

            const Vector3 lvPointOnA(lrEvent.mPointOnA.mV);
            const Vector3 lvPointOnB(lrEvent.mPointOnB.mV);
            const Vector3 lvNormal(lrEvent.mNormal.mV);

            lpSolver->AddVehicleContact(lvPointOnA, lvPointOnB, lvNormal,
                                        liCabModel, liTrailerModel);
        }
    }

}
}

// ============================================================================
// FOLDED FROM BrnDeformationManager_wG12_VerifyPartIndices.cpp (wave G12) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_wG12_VerifyPartIndices.cpp
//
// DeformationManager::VerifyPartIndices -- the set-bit walk over mModelsAdded (the live-model
// slot mask), called every frame from PhysicsModule::Update, ::PostSceneUpdate and
// ::HandleGameActionsPostScene. THE LOOP BODY IS EMPTY ON PURPOSE: the console emits no call,
// no load and no store inside the walk, so whatever per-slot checking the debug build once did
// left no instruction behind. Do not invent one. In effect this is a no-op sweep.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    void DeformationManager::VerifyPartIndices()
    {
        // GetFirstNonZeroBit/GetNextNonZeroBit return KI_INVALID_BITINDEX (-1) when there is
        // nothing left.
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != CgsContainers::BitArray<28u>::KI_INVALID_BITINDEX;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            // Empty on purpose -- see the banner.
            (void)liModelIndex;
        }
    }
}
}

// ============================================================================
// FOLDED FROM BrnDeformationManager_wG_ResetModelEvent.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// BrnPhysics::Deformation::DeformationManager -- the single-model RESET slice: the body-shop
// arm of PhysicsModule::HandleGameActionsPostScene calls it to reset one model's deformation.

namespace BrnPhysics
{
namespace Deformation
{
    // FLAG: the console has no branch after the `index == -1` assert and indexes the model pool
    // at -1, one model stride below the pool base. The assert is reproduced as-is; the wild read
    // that follows it is replaced by the early-out below, because a player reaches this path by
    // driving into a body shop with an entity that owns no deformation model.
    void DeformationManager::ProcessResetDeformationModelEvent(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
            EntityId lEntityId)
    {
        const s32 liModelIndex = FindModelIndexByEntityID(lEntityId);

        CGS_ASSERT(liModelIndex != -1, "Failed to find deformation model to deactivate");
        if (liModelIndex == -1)
            return;                 // FLAG: see the banner -- the console has no branch here.

        // Zero time vector, the raw -1 reset type the console passes (the enum names only
        // E_DEFORMATION_RESET_NONE == 0), flag clear, the manager's own generator.
        const VecFloat lvfZero(__vspltisw(0));
        mpaModels[liModelIndex].ResetDeformation(
            lpSimInput, lpSceneInterface, &mDetachedPartManager, &mDetachedWheelManager,
            lvfZero,
            static_cast<DeformationResetType>(-1), false, mRandom);
    }

    // ARTIST 0x82641E50 (export hole; named by ResetDeformation's caller xref).
    // The post-scene caller runs this sweep even when its game-action queue is empty.
    void DeformationManager::ProcessDebugResetDeformationModels(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            if (mpaModels[liModelIndex].ShouldResetDeformationNextUpdate())
            {
                const VecFloat lvfZero(__vspltisw(0));
                mpaModels[liModelIndex].ResetDeformation(
                    lpSimInput, lpSceneInterface, &mDetachedPartManager, &mDetachedWheelManager,
                    lvfZero,
                    static_cast<DeformationResetType>(0), false, mRandom);
            }

            // 0x82641F90..FA0 reloads the pool and clears the request after the call.
            mpaModels[liModelIndex].ResetDeformationNextUpdate(false);
        }
    }
}
}

// FOLDED FROM BrnDeformationManager_Construct.cpp
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::AddMonitor
#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"  // complete type for the static mDebugComponent

// ==================================================================================================
// BrnPhysics::Deformation::DeformationManager::Construct -- SPLIT OUT of BrnDeformationManager.cpp.
// BUILD-MECHANICS SPLIT ONLY: the body and its banner comment were MOVED verbatim, not retyped or
// re-derived. Same precedent as RaceCarPhysics_Construct.cpp / TrafficPhysics_Construct.cpp /
// BrnVehicleManager_Construct.cpp. PhysicsModule::Construct calls it every boot.
//
// Six of the symbols Construct needs are defined in BrnDeformationConstructShims.cpp:
//     DetachedPartManager::Construct
//     DeformationDebugComponent_Construct
//     DeformableObject_ConstructUpdatePerformanceMonitors
//     DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors
//     DeformableObject_ConstructPostPhysicsPerformanceMonitors
//     DeformationManager::mDebugComponent
//
// BrnDeformationManager.cpp is mounted as well, so re-merging is a plain move of this body and text.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // The Construct memset loop bounds (duplicated from BrnDeformationManager.cpp; `static const`
    // so each TU keeps its own copy -- no ODR surface). Values are the frozen-header array extents.




    // DECLARE-ONLY cross-TU surface this body calls (same stand-ins as BrnDeformationManager.cpp
    // declares; see the banner there). Bodied by BrnDeformationConstructShims.cpp.
    void DeformationDebugComponent_Construct(DeformationDebugComponent* lpComponent,
                                             DeformationManager* lpManager);
    void DeformableObject_ConstructUpdatePerformanceMonitors();
    void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
    void DeformableObject_ConstructPostPhysicsPerformanceMonitors();

    // -----------------------------------------------------------------------------------
    // Construct  @0x82621510   [EXECUTED in goal trace]
    //
    // One-time manager construction: build the static debug component, construct the embedded
    // detached-part pool and detached-wheel manager, seed the private RNG, mark no models live + no
    // pool + no player, unmap every slot's global entity id, clear the race-car / traffic /
    // global-traffic index tables to -1, and register the ~24 CPU performance monitors in their
    // exact order.
    // -----------------------------------------------------------------------------------
    void DeformationManager::Construct()
    {
        // The static debug component (constructed at the fixed address off_82F2A440, passed
        // `this`). Modelled via the file-scope-static mDebugComponent member.
        DeformationDebugComponent_Construct(&mDebugComponent, this);

        // The embedded detached-PART pool: 50 PhysicalBodyPart slots (the X360 inlines the pool
        // construct loop -- 50 iterations, 496-byte stride, then the pool count/ptr init).
        mDetachedPartManager.Construct();

        // The detached-WHEEL manager IS constructed here (crash parity G23-D2, 2026-09-23; the old
        // banner said "the asm constructs only the part pool", which is false): 0x82621584/0x82621590
        // `addis r10,this,1 ; addi r10,r10,0x2870` then 0x826215A8 `std r30(0),0(r10)` ==
        // mDetachedWheelManager.mUsedWheels = 0 -- the inlined DetachedWheelManager::Construct. The
        // PS3 twin (DeformationManager::Construct @0x744130) calls it out of line right after
        // DetachedPartManager::Construct: `bl DetachedWheelManager::Construct` @0x744184.
        mDetachedWheelManager.Construct();

        // Seed the private RNG (the X360 inlines Random::Construct -- the 0x..1AD0891B default seed
        // + the 8-slot float ring fill + the index advance).
        mRandom.Construct();

        // Clear the per-model live-slot set (the X360 zeroes the BitArray field; done first, with
        // the RNG seed, in the asm).
        mModelsAdded.UnSetAll();

        // No model pool yet, and every model slot's global entity id is unmapped (crash parity
        // G23-D2): 0x8262177C `stwx r30(0),r31,r23(0x12900)` == mpaModels = NULL, then
        // 0x8262178C..0x82621798 `mtctr 28 ; stw r5(-1),0(r7) ; addi r7,r7,4 ; bdnz` from
        // this+0x12888 == maGlobalEntityIDs[0..27] = 0xFFFFFFFF. PS3 0x744208 `stw 0,0x2900` and the
        // 0x74420C..0x744228 28-word -1 loop are the same two stores.
        mpaModels = 0;
        for (s32 li = 0; li < static_cast<s32>(KU_MAX_DEFORMATION_MODELS); ++li)
            maGlobalEntityIDs[li].muValue = 0xFFFFFFFFu;

        // Index tables start fully unmapped (-1 == no model). The X360 memsets the three byte
        // tables (8 / 20 / 600 bytes -- the global-traffic table is sized 601 but the loop writes
        // 600; the trailing byte is initialised by the table's own clear elsewhere). Modelled as
        // the by-name table clears across each array's extent.
        for (s32 li = 0; li < KI_MAX_NUM_RACE_CARS; ++li)
            ma8RaceCarToModelIndex[li] = -1;
        for (s32 li = 0; li < KI_MAX_NUM_PHYSICAL_TRAFFIC; ++li)
            ma8TrafficToModelIndex[li] = -1;
        for (s32 li = 0; li < KI_MAX_TOTAL_TRAFFIC; ++li)
            ma8GlobalTrafficToModelIndex[li] = -1;

        // No live models, no player model yet, no part-IK round-robin progress.
        miNumUsedModels          = 0;   // X360 *(this+76036) = 0
        miPlayerModelIndex       = -1;  // X360 *(this+76040) = -1
        miLastBodyToHaveIKUpdate = 0;   // X360 *(this+76672) = 0

        // Register the CPU performance monitors in console order (page 15, minimum 0, budget
        // 10.0 ms, scaled).
        miTotalDeformationPerfMon                                = CgsDev::PerfMonCpu::AddMonitor("Total deformation",            CgsDev::E_PMP_15, false, 10.0f, true);
        miPostSceneUpdatePerfMon                                 = CgsDev::PerfMonCpu::AddMonitor("  Post scene update",          CgsDev::E_PMP_15, false, 10.0f, true);
        miUpdateSensorDisplPerfMon                               = CgsDev::PerfMonCpu::AddMonitor("  Update sensor displ.",       CgsDev::E_PMP_15, false, 10.0f, true);
        miUpdatePerfMon                                          = CgsDev::PerfMonCpu::AddMonitor("  Update",                     CgsDev::E_PMP_15, false, 10.0f, true);
        miUpdateModelsPerfMon                                    = CgsDev::PerfMonCpu::AddMonitor("     Update models",           CgsDev::E_PMP_15, false, 10.0f, true);
        DeformableObject_ConstructUpdatePerformanceMonitors();
        miUpdateIkAndDetachingPerfMon                            = CgsDev::PerfMonCpu::AddMonitor("     Upd.IK & detaching",      CgsDev::E_PMP_15, false, 10.0f, true);
        DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors();
        miUpdateDetachedPartsPerfMon                             = CgsDev::PerfMonCpu::AddMonitor("    Update detached parts",    CgsDev::E_PMP_15, false, 10.0f, true);
        miUpdateSkinnedJointsPerfMon                             = CgsDev::PerfMonCpu::AddMonitor("    Update skinned joints",    CgsDev::E_PMP_15, false, 10.0f, true);
        miUpdatePostPhysicsPerfMon                               = CgsDev::PerfMonCpu::AddMonitor("  Post physics update",        CgsDev::E_PMP_15, false, 10.0f, true);
        miPostPhysicsUpdateModelsPerfMon                         = CgsDev::PerfMonCpu::AddMonitor("     Update Models",           CgsDev::E_PMP_15, false, 10.0f, true);
        DeformableObject_ConstructPostPhysicsPerformanceMonitors();
        miPostPhysicsUpdateDetachedPartsManPerfMon               = CgsDev::PerfMonCpu::AddMonitor("     Update Detached Parts Man", CgsDev::E_PMP_15, false, 10.0f, true);
        miPostPhysicsProcessJointSpiesPerfMon                    = CgsDev::PerfMonCpu::AddMonitor("     Process Joint Spies",     CgsDev::E_PMP_15, false, 10.0f, true);
        miPostPhysicsUpdateAddContactsToPenSolverPerfMon         = CgsDev::PerfMonCpu::AddMonitor("       Add conts to PenSlvr",  CgsDev::E_PMP_15, false, 10.0f, true);
        miPostPhysicsUpdateSolvePenetrationPerfMon               = CgsDev::PerfMonCpu::AddMonitor("       Solve Penetrations",    CgsDev::E_PMP_15, false, 10.0f, true);
        miPostPhysicsUpdateReadTransformsFromPenSolverPerfMon    = CgsDev::PerfMonCpu::AddMonitor("       Read from Pen Solver",  CgsDev::E_PMP_15, false, 10.0f, true);
        miFixUpRaceCarTrafficContact                             = CgsDev::PerfMonCpu::AddMonitor("Fixup RaceCar Traf contact",   CgsDev::E_PMP_15, false, 10.0f, true);

        // (X360: a one-shot file-static init guard `if (dword_82F2A338 == -1) dword_82F2A338 = 0;`
        // -- the debug component's registration latch. Modelled inside the debug component's own
        // home; no observable state on the manager.)
    }

    // ==================================================================================================
    // DeformationManager::_AssertLayout -- NOT A GAME FUNCTION. Never called; nothing but
    // static_asserts. Added 2026-08-04 (task #141). DeformationManager had NO layout gate of any
    // kind, which is exactly how a FOUR-byte `::RigidBodyId` stand-in (BrnCommonTypes.h:28) sat in
    // the EIGHT-byte mWorldRigidBodyId seat without a single check firing.
    //
    // ⛔⛔ HOMED HERE, DELIBERATELY, AND THE REASON IS THE WHOLE POINT OF THE GATE.
    // It was first written into BrnDeformationManager.cpp -- which is UNMOUNTED (this very file's
    // banner above records the measured 25-unresolved trial link, and build_game_exe.bat carries it
    // only as a `rem`). A gate in an unmounted TU is not compiled, so it cannot fail. The tamper
    // test below caught it passing WITH the defect in place. This TU is mounted, so this gate runs.
    // ⚠️ If BrnDeformationManager.cpp is ever mounted, this stays here -- two definitions would be
    // a duplicate symbol.
    //
    // ⚠️ ADJACENCY (prev + sizeof(prev)) AGAINST THE CONSOLE OFFSETS, NEVER A TOTAL sizeof.
    // A total-size assert on this pointer-bearing, over-aligned class is absorbed by padding and by
    // the members that widen on x64, and would be green with the defect present.
    //
    // ⭐⭐ EVERY TERM IS `sizeof(DeformationManager::<member>)`, NEVER `sizeof(<Type>)`.
    // The first draft spelled the types (`sizeof(CgsPhysics::RigidBodyId)`) and was therefore
    // INVARIANT under the defect -- re-typing the member could not move an assert that names the
    // type directly. Such a gate only restates the header back to itself.
    //
    // Console offsets, all attested in committed asm notes:
    //   mModelsAdded       @ +75904   BrnDeformationDebugComponent.cpp:736
    //   mpaModels          @ +76032   BrnDeformationManager_Contacts.cpp:13, DebugComponent.cpp:737
    //   miNumUsedModels    @ +76036   this file, Construct
    //   miPlayerModelIndex @ +76040   this file, Construct
    // ⚠️ BrnDeformationManager_Contacts.cpp:16 also claims "the mModelsAdded words @ +18976".
    // That value is IMPOSSIBLE and is deliberately NOT used: it sits below mDetachedPartManager
    // (+48112), which PRECEDES mModelsAdded in DWARF member order (:360 vs :362). +75904 is the
    // only value consistent with both the member order and the attested mpaModels seat.
    //
    // Every member below is host-stable (no pointer participates), so the console arithmetic is
    // valid arithmetic on the x64 host too. mpaModels itself is only ever the RESULT of the chain,
    // never a term -- it is a pointer and widens.
    //
    // ⭐ TAMPER-TESTED 2026-08-04, and the FIRST version PASSED that test -- i.e. it was worthless
    // -- for both reasons above. Re-pointing mWorldRigidBodyId at the 4-byte `::RigidBodyId`
    // stand-in now fails the build on BOTH of the last two asserts (the size one and the
    // adjacency one, 76024 + 4 == 76028 != 76032): a real gate fails in a cascade.
    // ⛔ If you edit this function, re-run that tamper test. A gate that has never been made to
    // fail is not a gate.
    // ==================================================================================================
    void DeformationManager::_AssertLayout()
    {
        // mModelsAdded is one 64-bit BitArray field (CgsBitArray.h:22, 28 bits -> 1 field).
        CGS_STATIC_ASSERT((75904u + sizeof(((DeformationManager*)0)->mModelsAdded) == 75912u),
                      "DeformationManager: maGlobalEntityIDs must abut mModelsAdded at +75912");

        // 28 scene-entity ids, 4 bytes each.
        CGS_STATIC_ASSERT((sizeof(((DeformationManager*)0)->maGlobalEntityIDs) == 112u),
                      "DeformationManager: maGlobalEntityIDs is 28 * 4 bytes");
        CGS_STATIC_ASSERT((75912u + sizeof(((DeformationManager*)0)->maGlobalEntityIDs) == 76024u),
                      "DeformationManager: mWorldRigidBodyId must abut maGlobalEntityIDs at +76024");

        // ⭐ THE LOAD-BEARING PAIR. The rigid-body handle packs the owning EntityId in the HIGH
        // dword and the body index in the LOW dword (CgsRigidBody.h:3-5), so a 4-byte seat keeps
        // the index and discards the owning entity with no diagnostic at all. These are the two
        // asserts the 4-byte stand-in must fail.
        CGS_STATIC_ASSERT((sizeof(((DeformationManager*)0)->mWorldRigidBodyId) == 8u),
                      "DeformationManager: mWorldRigidBodyId is EIGHT bytes -- it is "
                      "CgsPhysics::RigidBodyId, not the 4-byte BrnCommonTypes.h stand-in");
        CGS_STATIC_ASSERT((76024u + sizeof(((DeformationManager*)0)->mWorldRigidBodyId) == 76032u),
                      "DeformationManager: mpaModels sits at its attested +76032 ONLY if "
                      "mWorldRigidBodyId is 8 bytes; at 4 the chain lands on +76028");
    }
}
}

// FOLDED FROM BrnDeformationManager_ContactBridges.cpp
// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_ContactBridges.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the contact-BRIDGE slice consumed by
// PhysicsModule::BridgeContactsToSimulation @0x825A99E8.
// Slice TU: home BrnDeformationManager.cpp / the _Contacts group is still unmounted (its
// SolvePenetration / UpdateTriangleCache tail carries ~19 unresolved of its own). Fold back
// when the home mounts.
//
// CONTENTS:
//   * ReadPotentialVehicleWorldContact @0x82604590 -- MOVED VERBATIM from
//     BrnDeformationManager_Contacts.cpp (this TU is mounted; that one is not). One change,
//     flagged inline: the "Un-normalised" tripwire's old always-pass placeholder was upgraded
//     to a real predicate at the move (2026-08-06), and that predicate was corrected to the
//     console's |Magnitude(n) - 1| > 0.01 form on 2026-08-27 -- see IsNormalisedWithinEpsilon.
//   * FindModelIndexByGlobalEntityID @0x825B45B0 -- MOVED VERBATIM (same reason; callee of
//     the above).
//   * ReadPotentialContact @0x826053F8 -- reconstructed from the
//     BURNOUT_X360_ARTIST.XEX asm with the PS3 DecFIGS out-of-line body @0x6F048C as the
//     structural oracle (debug names lPotentialContact/lContactId/lpSimInput,
//     liModelIndexA/B from its baked asserts).
//   * BridgeBodyPartCarContactsToSimulation @0x825DD7D0 + BridgeDetachedWheelCar-
//     ContactsToSimulation -- REAL (landed 2026-09-20; 348/392 console lines each). They
//     drain the two DETACHED part/wheel-vs-car potential-contact queues into the
//     simulation's add-contact queue.
//   * AddRaceCarBodyPartPair @0x82605928 + AddHingedBodyPartPairs @0x82605A98 -- REAL.
//     AddRaceCarWheelPair @0x82605BE8 is a NAMED GATE (one missing cylinder-vs-box appender).
//
// This TU is LIVE: PhysicsModule::Update @0x825B0640 is a real body and the three pair feeders
// run every frame from VehicleManager::StartVehicleContactGeneration.
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the boot gates)

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // PotentialContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (+ DeformationSensor)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (GetTransform + the timer bank)
#include "rw/math/vpu/vector3_operation.h"                                                // Normalize / IsValid
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder::AddPrimitivePair (+ CgsGeometric::Box)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"     // IKBodyPart::GetPartPoolIndex (the hinged-panel walk)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"  // PhysicalWheel::GetVolumeInstanceId (the detached-wheel bridge)
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"                     // CgsGeometric::Cylinder (AddRaceCarWheelPair's wheel primitive)
#include <cstdlib>                                                                        // getenv (the opt-in [wheel-car] witness)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"            // PotentialContactInterface::GetDetached*Queue (the two drained queues)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"                // InputBuffer::GetAddContactQueue
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"               // InAddPotentialContact (the queued record)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                            // EntityId::Set (the proxy-body id pack)

namespace BrnPhysics
{
namespace Deformation
{
    namespace
    {
        // Owner byte of a packed VolumeInstanceId (bits [56..63]; see the _Contacts TU banner).
        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }

        // Race-car / total-traffic index bounds (FindModelIndexByGlobalEntityID asserts).
        const u32 KU_OWNER_RACECAR         = 1;      // BrnWorld::E_ENTITYTYPE_RACECAR
        const u32 KU_OWNER_TRAFFIC_VEHICLE = 2;      // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        const u32 KU_MAX_NUM_RACE_CARS     = 8;      // Vehicle::ku8MaxNumRaceCars
        const u32 KU_MAX_TOTAL_TRAFFIC     = 0x258;  // BrnTraffic::KU_MAX_TOTAL_TRAFFIC (600)

        // The two contact-BRIDGE owner sets and pool bounds (the bodies' own assert strings).
        const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;   // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
        const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;   // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART
        const u32 KU_OWNER_DETACHED_RACECAR_WHEEL  = 9;   // BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL
        const u32 KU_OWNER_DETACHED_TRAFFIC_WHEEL  = 10;  // BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL

        // The PROXY ("dummy car") owners the two bridges retarget the car's handling body onto
        // before handing the contact to the simulation. Same retarget PropManager::
        // RoutePropVsRaceCarContactToDummyCar performs for a prop-vs-car contact.
        const u32 KU_OWNER_PROP_COLLISION_RACECAR  = 11;  // BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR
        const u32 KU_OWNER_PROP_COLLISION_TRAFFIC  = 12;  // BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC

        const u32 KU_MAX_DETACHED_PARTS      = 0x32;   // 50   (`cmplwi r31, 0x32`)
        const u32 KU_MAX_DETACHED_WHEELS     = 0x70;   // 112  (`cmplwi r31, 0x70`)
        // The model-pool bound (0x1C == 28) is the namespace-scope
        // BrnPhysics::Deformation::KU_MAX_DEFORMATION_MODELS homed by BrnDeformationState.h --
        // not re-declared here (a file-local copy makes every use ambiguous).

        // Material constants the two bridges stamp into every record they post.
        //   part-vs-car : 0.5 / 0.5 / 0.5        (one float, flt_82001DA0, stored three times)
        //   wheel-vs-car: 0.9 / 0.5 / 0.8        (flt_82005450 / flt_82001DA0 / flt_8208F9C8)
        const f32 KF_PART_CAR_FRICTION            = 0.5f;
        const f32 KF_PART_CAR_RESTITUTION         = 0.5f;
        const f32 KF_WHEEL_CAR_STATIC_FRICTION    = 0.89999998f;
        const f32 KF_WHEEL_CAR_DYNAMIC_FRICTION   = 0.5f;
        const f32 KF_WHEEL_CAR_RESTITUTION        = 0.80000001f;

        // The detached-wheel bridge's maximum tolerated penetration before it rebuilds the
        // contact off the car's own body axis. A SILENT-ZERO splat in the image (a broadcast
        // vector filled by a start-up thunk from the 0.0099999998f literal, not a zero), so it
        // is spelled here as the value the thunk writes.
        const f32 KF_MAX_WHEEL_PENETRATION        = 0.0099999998f;

        // Pair-builder feeder constants, read off the three bodies' asm.
        const f32 KF_PART_VS_CAR_CONTACT_PADDING  = 0.5f;   // flt_82001DA0 @0x82605A88
        const f32 KF_HINGED_PART_CONTACT_PADDING  = 1.0f;   // flt_82001C98 @0x82605B20
        const f32 KF_WHEEL_VS_CAR_CONTACT_PADDING = 0.5f;   // flt_82001DA0 @0x82605DFC
        const s32 KI_MAX_PARTS_PER_MODEL          = 50;     // `cmpwi r26, 0x32` (maPartStates[50])

        // Per-lane NaN self-compare (the asm's vspltw + vcmpeqfp. over lanes x/y/z) -- the
        // vendor vpu tree's own IsValid(Vector3).
        inline bool IsValidVec3Lanes(const Vector3& lrV) { return rw::math::vpu::IsValid(lrV); }

        // Both contact bridges resolve the car side of the event the same way: take the
        // contacted model's handling-body word (the 8-byte handle at model +0x6710), keep its
        // 14-bit entity index, and re-stamp the owner as the car's PROP-COLLISION PROXY body --
        // racecar -> 11, traffic -> 12 -- with a zero part index. The finished 32-bit word is
        // then widened into the event's 8-byte B-side id as the HIGH dword, leaving the low
        // dword zero. The console inlines this in both bodies; written once here so the two
        // cannot drift.
        //
        // ⚠️ THE RETARGET IS THE POINT, not a detail: the record the simulation receives does
        // NOT name the car's handling body, it names the car's prop-collision proxy ("dummy
        // car") body -- the same proxy a prop-vs-car contact is routed onto.
        inline u64 BuildProxyCarBodyId(u64 lu64HandlingBodyId)
        {
            const u32 luCarEntityWord  = static_cast<u32>(lu64HandlingBodyId >> 32);
            const u32 luCarOwner       = (luCarEntityWord >> 24) & 0xFFu;
            const u32 luCarEntityIndex = (luCarEntityWord >> 10) & 0x3FFFu;

            u32 luProxyOwner = KU_OWNER_PROP_COLLISION_RACECAR;
            if (luCarOwner != KU_OWNER_RACECAR)
            {
                // Fire-and-continue, exactly as the console: the traffic arm is taken either
                // way (the tripwire does not gate the branch it sits in).
                CGS_ASSERT(luCarOwner == KU_OWNER_TRAFFIC_VEHICLE,
                           "lAddContactEvent.mIDB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
                luProxyOwner = KU_OWNER_PROP_COLLISION_TRAFFIC;
            }

            // EntityId::Set carries the console's own entity-index tripwire
            // ("luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"), which is why the pack is
            // spelled through it rather than as loose shifts.
            CgsSceneManager::EntityId lProxyEntityId;
            lProxyEntityId.Set(luProxyOwner, luCarEntityIndex, 0);
            return static_cast<u64>(static_cast<u32>(lProxyEntityId)) << 32;
        }

        // The consoles' "Un-normalised ..." tripwire predicate.
        // ⭐⭐ CORRECTED 2026-08-27 (traffic-bus ram wave): this used to RENORMALISE the vector and
        // compare the per-lane |renormalised - original| against the epsilon. That is not the
        // console's predicate -- it is looser by up to sqrt(3) (the lane delta of a vector of
        // length 1+e is e*n_lane, never more than e) and it is a different computation. The X360
        // body spells the SCALAR magnitude and subtracts ONE, exactly as the assert text says:
        //   0x826047E4  vmsum3fp128 v13, v12(NORMAL), v12    ; dot = |n|^2
        //   0x8260480C  vrsqrtefp   v12, v13                 ; 1/sqrt(dot) + two Newton refines
        //   0x8260483C  vmulfp128   v0,  v13, v0             ; dot * 1/sqrt(dot) == |n|
        //   0x82604840  vsel        v0,  v0,  v7(zero), v3   ; dot == 0 -> magnitude 0
        //   0x82604844  vsubfp      v0,  v0,  v11(1.0)       ; |n| - 1   (v11 = vcfsx(1,0))
        //   0x82604848  vandc       v0,  v0,  v9             ; abs (0x80000000 splat)
        //   0x8260484C  vcmpgtfp    v0,  v0,  v8(0.0099999998)
        // which is the SAME predicate the bridge's own tripwires (BrnPhysicsModuleBridgeFunctions
        // .cpp IsUnitLength) and DeformationSensor::ValidateAndAddContact @0x825E1864 run. One
        // console idiom, one spelling. rw::math::vpu::Magnitude carries the zero-dot guard that
        // reproduces the vsel, so a zero normal still fails.
        inline bool IsNormalisedWithinEpsilon(const Vector3& lrV)
        {
            const f32 lfDelta = rw::math::vpu::Magnitude(lrV) - 1.0f;
            return (lfDelta < 0.0f ? -lfDelta : lfDelta) <= 0.0099999998f;
        }

        // The orthonormal-affine inverse both consoles inline in ReadPotentialContact
        // (@0x826053F8: the vmrglw/vmrghw 3x3 word-interleave transpose of the model's rigid-body
        // transform rows + the `vsubfp 0,T` negated translation folded through the transposed
        // columns via three vspltw/vmaddfp steps): rotation transposed, translation -(T . R).
        inline void BuildWorldToModel(Matrix44Affine& lrOut, const Matrix44Affine& lrTransform)
        {
            // ARTIST 0x82605528..0x8260558C, including the fourth lane and fused order.
            const __vector4 zero = __vspltisw(0);
            const __vector4 negT = __vsubfp(zero, lrTransform.wAxis.mV);
            const __vector4 xzlo = __vmrglw(lrTransform.xAxis.mV, lrTransform.zAxis.mV);
            const __vector4 yzlo = __vmrglw(lrTransform.yAxis.mV, zero);
            const __vector4 xzhi = __vmrghw(lrTransform.xAxis.mV, lrTransform.zAxis.mV);
            const __vector4 yzhi = __vmrghw(lrTransform.yAxis.mV, zero);
            lrOut.zAxis.mV = __vmrghw(xzlo, yzlo);
            lrOut.yAxis.mV = __vmrglw(xzhi, yzhi);
            lrOut.xAxis.mV = __vmrghw(xzhi, yzhi);
            __vector4 translation = __vmulfp(__vspltw(negT, 2), lrOut.zAxis.mV);
            translation = __vmaddfp(__vspltw(negT, 1), lrOut.yAxis.mV, translation);
            lrOut.wAxis.mV = __vmaddfp(__vspltw(negT, 0), lrOut.xAxis.mV, translation);
        }
    }

    // ==========================================================================================
    // ReadPotentialVehicleWorldContact @ 0x82604590   (MOVED from BrnDeformationManager_Contacts
    // .cpp -- see the TU banner. Body verbatim except the flagged tripwire upgrade.)
    //
    // Route one potential VEHICLE-vs-WORLD contact into the owning car's deformation sensor. The
    // asm: validity-gate the contact (assert + ignore if any lane is NaN); look up the car's model
    // slot from muVolumeInstanceIdA's entity; assert the owner tags (A == racecar/traffic, B ==
    // world); fetch the model's deformation sensor for that volume instance and the model's world
    // transform; assert the normal is normalised; then DeformationSensor::ValidateAndAddContact
    // with no other vehicle / other sensor.
    // ==========================================================================================
    void DeformationManager::ReadPotentialVehicleWorldContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
        BrnPhysics::ContactId lContactId,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/)
    {
        // Initial validity gate on the contact NORMAL (+0x20): 0x826045AC addi r25,r23,0x20 ;
        // 0x826045BC lvx128 ; lanes 0/1/2 self-compared (vcmpeqfp.) -- any NaN lane asserts and
        // the contact is ignored. The same r25 feeds the |n|^2 tripwire (0x8260473C) and the
        // streamed normal (0x82604888); PS3 0x73A884 `li r0,0x20 ; lvx v1,lPotentialContact,r0`.
        // (Crash parity G22-D2, 2026-09-23: the tree tested mPointOnA, so a NaN normal reached
        // the sensor and a NaN point with a valid normal was dropped.)
        if (!IsValidVec3Lanes(lrPotentialContact.mNormal))
        {
            CGS_FAIL("Invalid contact added to deformation. Ignoring...\n");
            return;
        }

        // The model lookup keys off muVolumeInstanceIdA's embedded entity word (the high dword of the
        // packed 64-bit id) -- the asm passes that word to FindModelIndexByGlobalEntityID.
        const EntityId lEntityA = { static_cast<u32>(lrPotentialContact.muVolumeInstanceIdA.muId >> 32) };
        const s32 liModelIndexA = FindModelIndexByGlobalEntityID(lEntityA);
        if (liModelIndexA == -1)
        {
            CGS_FAIL("liModelIndexA != -1");
            return;
        }

        // Owner-tag tripwires (non-gating): A must be a racecar / traffic vehicle, B must be world.
        const u32 luOwnerA = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdA);
        CGS_ASSERT(luOwnerA == KU_OWNER_RACECAR || luOwnerA == KU_OWNER_TRAFFIC_VEHICLE,
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        CGS_ASSERT(GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdB) == 0u,
                   "lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");

        DeformableObject& lrModel = mpaModels[liModelIndexA];

        // The car's deformation sensor for the contacted volume instance, and the car's world
        // transform (the asm reads the rigid-body transform off the model @ +6476 and packs it into
        // the Matrix44Affine handed to ValidateAndAddContact).
        DeformationSensor& lrSensor =
            lrModel.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdA);

        // ⭐⭐ FIXED 2026-08-14 (walls leg 4, at-rest probe): the console hands ValidateAndAddContact
        // the INVERSE (world -> model) transform -- the PS3 body's own argument is NAMED
        // lInverseVehicleTransform, and the car-car route below already builds exactly this
        // inverse. The old forward-transform pass made the stored local contact points garbage
        // (latent while nothing consumed them; the solver's at-rest pop measured it).
        Matrix44Affine lTransform;
        lrModel.GetTransform(lTransform);
        Matrix44Affine lWorldTransform;
        BuildWorldToModel(lWorldTransform, lTransform);

        // Normalised-normal tripwire (non-gating). ⭐ UPGRADED AT MOVE 2026-08-06 (the old body
        // carried an honest always-pass placeholder), ⭐⭐ CORRECTED 2026-08-27 to the console's
        // own scalar |Magnitude(n) - 1| > 0.01 predicate @0x826047E4..0x8260484C -- see
        // IsNormalisedWithinEpsilon. This site is the SAFE one either way: queues [6]/[9]
        // renormalise their copy before calling here (BrnPhysicsModuleBridgeFunctions.cpp :821 /
        // :846), so the normal is unit by construction. The streamed "Car transform: " message
        // tail is still lowered to the static prefix per the standing project rule.
        CGS_ASSERT(IsNormalisedWithinEpsilon(lrPotentialContact.mNormal),
                   "Un-normalised vehicle-world contact: ");   // BrnDeformationManager.cpp:1198

        // Validate + store the contact in the sensor (no other vehicle / other sensor for a
        // vehicle-vs-world contact).
        // FLAG (cross-TU type-namespace mismatch to reconcile at consolidation):
        // DeformationSensor::ValidateAndAddContact declares its contact arg as the stale forward-decl
        // CgsSceneManager::PotentialContact, whereas the canonical (DWARF) type is
        // CgsSceneManager::SceneManagerIO::PotentialContact (homed by CgsPotentialContact.h).
        // They are the SAME on-disk record (only the namespace differs), so the canonical record is
        // passed through a layout-safe reference cast here; the sensor header should retype its arg to
        // the SceneManagerIO:: home at consolidation, after which this cast can be dropped.
        lrSensor.ValidateAndAddContact(
            lWorldTransform,
            lrPotentialContact,
            lContactId, 0, 0);
    }

    // ==========================================================================================
    // FindModelIndexByGlobalEntityID @ 0x825B45B0   (MOVED VERBATIM from
    // BrnDeformationManager_Contacts.cpp -- see the TU banner.)
    // ==========================================================================================
    s32 DeformationManager::FindModelIndexByGlobalEntityID(EntityId lGlobalEntityId)
    {
        // Owner = high byte of the entity word; index = the 14-bit field at bit 10 (the
        // VolumeInstanceId entity-word geometry the asm decodes: `(id >> 10) & 0x3FFF`).
        const u32 luOwner = (lGlobalEntityId.muValue >> 24) & 0xFFu;

        CGS_ASSERT(luOwner == KU_OWNER_RACECAR || luOwner == KU_OWNER_TRAFFIC_VEHICLE,
                   "lID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

        if (luOwner == KU_OWNER_RACECAR)
        {
            const u32 lu16RaceCarIndex = (lGlobalEntityId.muValue >> 10) & 0x3FFFu;
            CGS_ASSERT(lu16RaceCarIndex < KU_MAX_NUM_RACE_CARS, "lu16RaceCarIndex < Vehicle::ku8MaxNumRaceCars");
            return ma8RaceCarToModelIndex[lu16RaceCarIndex];
        }

        const u32 lu16TrafficGlobalIndex = (lGlobalEntityId.muValue >> 10) & 0x3FFFu;
        CGS_ASSERT(lu16TrafficGlobalIndex < KU_MAX_TOTAL_TRAFFIC,
                   "lu16TrafficGlobalIndex < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");
        return ma8GlobalTrafficToModelIndex[lu16TrafficGlobalIndex];
    }

    // ==========================================================================================
    // ReadPotentialContact @ 0x826053F8   (NEW this wave; PS3 DecFIGS 0x6F048C)
    //
    // Route one potential VEHICLE-vs-VEHICLE contact into car A's deformation sensor (with car B
    // supplied as the "other" vehicle/sensor pair), then -- if the sensor accepted it -- clear
    // BOTH cars' contact cool-down lane. The asm:
    //   * owner tripwire :1872 (both owners racecar / traffic; fire-and-continue);
    //   * model lookups through FindModelIndexByEntityID (the PHYSICAL-id tables -- the PS3
    //     inlines its racecar/traffic table reads with bounds asserts h:701/h:710);
    //   * :1885/:1886 -1 tripwires (fire-and-continue);
    //   * the inlined world->model-A affine inverse (see BuildWorldToModel above) handed to
    //     DeformationSensor::ValidateAndAddContact as its transform argument;
    //   * sensors resolved through GetDeformationSensorFromVolumeInstance (out-of-line on the
    //     X360 @0x825B2xxx; the PS3 inlines its spec-count/wheel-map remap, h:1041/:1050/:1051);
    //   * on acceptance: `vrlimi128 v0, v127(zero), 4, 0` at BOTH cars' vehicle-physics
    //     +4192 (X360) / the vperm<0,5,2,3> zero-merge at +4176 (PS3) == zero the Y lane of
    //     mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction -- the
    //     "TimeSinceLastRaceCarContact cool-down" reset the declaration's own banner names.
    // ==========================================================================================
    // ⛔ THE UNUSED lpSimInput IS FAITHFUL -- DO NOT "FIX" IT (verified 2026-08-29).
    // A 2026-08-29 lead read the commented-out parameter as a dropped side effect and named it
    // the cause of the empty traffic contact queue. It is not. On the X360 this is a THREE-
    // argument function: the prologue saves r3/r4/r5 only (`mr r30,r4` @0x8260540C, `mr r28,r3`
    // @0x82605414, `mr r25,r5` @0x82605418), and across all 135 instructions r6 is never read --
    // the single later use of the saved r5 is `mr r6,r25` @0x826055B0, the ContactId going into
    // ValidateAndAddContact. The declaration keeps the parameter because the DWARF and every
    // sibling Read*/Bridge* in this family carry it; the body has nothing to do with it.
    // ⇒ The race-car-vs-traffic loss is NOT here. See the retraction in
    // BrnPhysicsModuleBridgeFunctions.cpp's ProcessContactSpy banner for what it actually is.
    void DeformationManager::ReadPotentialContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
        BrnPhysics::ContactId lContactId,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/)
    {
        const u32 luOwnerA = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdA);
        const u32 luOwnerB = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdB);

        CGS_ASSERT((luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE)
                       && (luOwnerA == KU_OWNER_RACECAR || luOwnerA == KU_OWNER_TRAFFIC_VEHICLE),
                   "( lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE) && "
                   "( lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");  // :1872

        // PHYSICAL-id model lookups (both entity words are already physics-local here -- the
        // bridge rewrote them upstream).
        const EntityId lEntityA = { static_cast<u32>(lrPotentialContact.muVolumeInstanceIdA.muId >> 32) };
        const EntityId lEntityB = { static_cast<u32>(lrPotentialContact.muVolumeInstanceIdB.muId >> 32) };
        const s32 liModelIndexA = FindModelIndexByEntityID(lEntityA);
        const s32 liModelIndexB = FindModelIndexByEntityID(lEntityB);

        CGS_ASSERT(liModelIndexA != -1, "liModelIndexA != -1");   // :1885
        CGS_ASSERT(liModelIndexB != -1, "liModelIndexB != -1");   // :1886
        DeformableObject& lrModelA = mpaModels[liModelIndexA];
        DeformableObject& lrModelB = mpaModels[liModelIndexB];

        // world -> model-A space (the inlined orthonormal-affine inverse of A's rigid-body
        // transform; see BuildWorldToModel's asm citation).
        Matrix44Affine lWorldToModelA;
        BuildWorldToModel(lWorldToModelA, lrModelA.GetVehiclePhysics()->GetTransform());

        // Sensor resolution: B first, then A (the console's call order).
        DeformationSensor& lrSensorB =
            lrModelB.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdB);
        DeformationSensor& lrSensorA =
            lrModelA.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdA);

        // Same cross-TU namespace cast FLAG as ReadPotentialVehicleWorldContact above.
        if (lrSensorA.ValidateAndAddContact(
                lWorldToModelA,
                lrPotentialContact,
                lContactId, &lrModelB, &lrSensorB))
        {
            // Accepted: zero BOTH cars' contact cool-down (the Y lane of the packed timer bank).
            lrModelA.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.GetY() = 0.0f;
            lrModelB.GetVehiclePhysics()
                ->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.GetY() = 0.0f;
        }
    }

    // =================================================================================================
    // DeformationManager::BridgeBodyPartCarContactsToSimulation
    //
    // Drain custom potential-contact queue [3] -- DETACHED body part vs car -- into the simulation's
    // add-contact queue. Live every frame from PhysicsModule::BridgeContactsToSimulation, which is
    // itself reached every frame from PhysicsModule::Update.
    //
    // Per queued contact, in the console's order:
    //   * an 80-byte stack copy of the record (the console's ten-doubleword copy loop);
    //   * the three material constants (0.5 / 0.5 / 0.5, all from one float);
    //   * owner tripwires -- A is a racecar/traffic DEFORMABLE PART (:2366), B is a racecar/
    //     traffic vehicle (:2369) -- and the part-slot bound muPolyTagA < 50 (:2373);
    //   * the pool gate: an unused part slot SKIPS the contact entirely (the whole tail sits
    //     inside `if (IsPartIndexUsed)`);
    //   * the event's A-side id <- the part's own packed 64-bit handle, read whole from +0x1D0;
    //   * mIDB <- the contacted car model's PROXY body id (see BuildProxyCarBodyId), reached
    //     through muPolyTagB with the model-slot bound (:2381), the bit-array index tripwire
    //     and the mModelsAdded live-slot tripwire (:2382);
    //   * the geometry: normal NEGATED into the event (the console's sign-bit splat over all
    //     four lanes), and a CROSSED (penetrating) pair collapsed -- if dot3(mPointOnB - mPointOnA,
    //     mNormal) is negative the event's mPointOnB is replaced by mPointOnA (threshold 0.0f);
    //   * muTag <- the event index tagged with this queue's owner byte, 0x03000000, behind the
    //     ContactId range tripwire (BrnContactId.h:122);
    //   * the queue-headroom tripwire (:2412) then the bounds-gated AddEventSafe.
    // =================================================================================================
    void DeformationManager::BridgeBodyPartCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        // ⛔ THE UNUSED lpInputBuffer IS FAITHFUL -- DO NOT "FIX" IT. The prologue spills only
        // `this`, the sim input and the contact interface; the module input buffer's register is
        // never read again across the whole body. The declaration keeps the parameter because the
        // recovered signature and every sibling Bridge* in this family carry it.
        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        const Queue& lrQueue = lpContacts->GetDetachedBodyPartCarQueue();

        // The loop bound is a SNAPSHOT: the console reads the length once into a stack slot
        // before the loop and compares against that copy at the bottom.
        const s32 liQueueLength = lrQueue.GetLength();

        for (s32 liEventIndex = 0; liEventIndex < liQueueLength; ++liEventIndex)
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact lContact =
                lrQueue.GetEvent(liEventIndex);   // 80-byte stack copy

            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lAddContactEvent;
            lAddContactEvent.mStaticFriction  = KF_PART_CAR_FRICTION;
            lAddContactEvent.mDynamicFriction = KF_PART_CAR_FRICTION;
            lAddContactEvent.mRestitution     = KF_PART_CAR_RESTITUTION;

            const u32 luOwnerA = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdA);
            CGS_ASSERT(luOwnerA == KU_OWNER_RACECAR_DEFORMABLE_PART
                           || luOwnerA == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");   // :2366

            const u32 luOwnerB = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdB);
            CGS_ASSERT(luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");           // :2369

            const u32 luPartSlot = lContact.muPolyTagA;
            CGS_ASSERT(luPartSlot < KU_MAX_DETACHED_PARTS,
                       "lContact.muPolyTagA < KU_MAX_DETACHED_PARTS");                                                         // :2373

            // A dead pool slot drops the contact -- the console's `beq` jumps straight to the
            // loop increment.
            if (!mDetachedPartManager.IsPartIndexUsed(static_cast<s32>(luPartSlot)))
            {
                continue;
            }

            // The part's own packed handle, read whole (the console's single `ld 0x1D0(part)`).
            const PhysicalBodyPart* lpPart =
                mDetachedPartManager.GetPartFromIndex(static_cast<u16>(luPartSlot));
            lAddContactEvent.mIDA = lpPart->GetRigidBodyId().GetBaseRigidBodyID();

            // The contacted car's deformable-model slot.
            const u32 luModelIndex = lContact.muPolyTagB;
            CGS_ASSERT(luModelIndex < KU_MAX_DEFORMATION_MODELS,
                       "lContact.muPolyTagB < BrnPhysics::Deformation::KU_MAX_DEFORMATION_MODELS");                            // :2381
            CGS_ASSERT(mModelsAdded.IsBitSet(luModelIndex),
                       "mModelsAdded.IsBitSet(lContact.muPolyTagB)");                                                          // :2382

            lAddContactEvent.mIDB =
                BuildProxyCarBodyId(mpaModels[luModelIndex].GetHandlingBodyVolumeInstanceId().muId);

            // Geometry. dot3(B - A, N) is the SIGNED separation along the contact normal; a
            // negative one means the pair is penetrating, and the console then collapses the
            // event's second point onto the first rather than shipping the crossed pair.
            const f32 lfSeparation =
                rw::math::vpu::Dot(lContact.mPointOnB - lContact.mPointOnA, lContact.mNormal);

            lAddContactEvent.mNormal   = rw::math::vpu::Negate(lContact.mNormal);
            lAddContactEvent.mPointOnA = lContact.mPointOnA;
            lAddContactEvent.mPointOnB =
                (lfSeparation < 0.0f) ? lContact.mPointOnA : lContact.mPointOnB;

            CGS_ASSERT(liEventIndex >= 0 && liEventIndex < 65535,
                       "liEventIndex >= 0 && liEventIndex < 65535");                          // BrnContactId.h:122
            lAddContactEvent.muTag = static_cast<u32>(
                BrnPhysics::ContactId((static_cast<u32>(liEventIndex) & 0xFFFFu) | 0x03000000u));

            CGS_ASSERT(lpSimInput->GetAddContactQueue()->GetLength() + 1
                           < lpSimInput->GetAddContactQueue()->GetMaxLength(),
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetLength() + 1 < "
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetMaxLength()");                                        // :2412
            lpSimInput->GetAddContactQueue()->AddEventSafe(lAddContactEvent);
        }
    }

    // =================================================================================================
    // DeformationManager::BridgeDetachedWheelCarContactsToSimulation
    //
    // The detached-WHEEL sibling of the body above: drain custom potential-contact queue [4] into
    // the simulation's add-contact queue. Live every frame from the same caller. Same shape as the
    // part bridge with four differences, all console-attested:
    //   * the owner set on side A is DETACHED_RACECAR_WHEEL / DETACHED_TRAFFIC_WHEEL (:2458) and
    //     the slot bound is muPolyTagA < 112 (:2465);
    //   * the slot lookup is DetachedWheelManager::IsSlotUsed / GetWheel; the console calls
    //     IsSlotUsed TWICE -- once as the gate, once as GetWheel's own inlined tripwire
    //     (BrnDetachedWheelManager.h:114) -- and the A-side id is the wheel's packed handle,
    //     read whole from +0x70;
    //   * the materials are 0.9 / 0.5 / 0.8, not the part bridge's 0.5 / 0.5 / 0.5;
    //   * instead of collapsing a separating pair, a DEEPLY penetrating one is REBUILT: when the
    //     separation is worse than -0.01, the contact normal is replaced by the car's own body
    //     RIGHT axis (mTransform.xAxis of the model's vehicle physics) signed to agree with the
    //     original normal, the depth is clamped to 0.01, and mPointOnB is re-derived as
    //     mPointOnA - normal*depth. This is what stops a wheel that has ended up inside a car
    //     from being pushed out along a garbage mesh normal.
    //   ⚠️ Note the model-slot leg carries NO KU_MAX_DEFORMATION_MODELS assert of its own here
    //     (the part bridge's :2381) -- only the bit-array index tripwire. Reproduced as issued.
    // =================================================================================================
    void DeformationManager::BridgeDetachedWheelCarContactsToSimulation(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        const BrnPhysics::PhysicsModuleIO::InputBuffer* /*lpInputBuffer*/,
        PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        // Same faithful-unused-parameter note as the body above: the module input buffer's
        // register is never read across the whole body.
        typedef PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        const Queue& lrQueue = lpContacts->GetDetachedWheelCarQueue();
        const s32 liQueueLength = lrQueue.GetLength();   // snapshot, as above

        for (s32 liEventIndex = 0; liEventIndex < liQueueLength; ++liEventIndex)
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact lContact =
                lrQueue.GetEvent(liEventIndex);   // 80-byte stack copy

            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact lAddContactEvent;
            lAddContactEvent.mStaticFriction  = KF_WHEEL_CAR_STATIC_FRICTION;
            lAddContactEvent.mDynamicFriction = KF_WHEEL_CAR_DYNAMIC_FRICTION;
            lAddContactEvent.mRestitution     = KF_WHEEL_CAR_RESTITUTION;

            const u32 luOwnerA = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdA);
            CGS_ASSERT(luOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL
                           || luOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");    // :2458

            const u32 luOwnerB = GetVolumeInstanceOwner(lContact.muVolumeInstanceIdB);
            CGS_ASSERT(luOwnerB == KU_OWNER_RACECAR || luOwnerB == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                       "lContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");           // :2461

            CGS_ASSERT(lContact.muPolyTagA < KU_MAX_DETACHED_WHEELS,
                       "lContact.muPolyTagA < KU_MAX_DETACHED_WHEELS");                                                        // :2465

            const u16 lu16WheelSlot = static_cast<u16>(lContact.muPolyTagA);
            if (!mDetachedWheelManager.IsSlotUsed(lu16WheelSlot))
            {
                continue;
            }

            // GetWheel re-runs IsSlotUsed as its own tripwire -- that is the console's second
            // call, not a duplicated gate.
            const PhysicalWheel* lpWheel = mDetachedWheelManager.GetWheel(lu16WheelSlot);
            lAddContactEvent.mIDA = lpWheel->GetVolumeInstanceId().muId;

            const u32 luModelIndex = lContact.muPolyTagB;
            CGS_ASSERT(mModelsAdded.IsBitSet(luModelIndex),
                       "mModelsAdded.IsBitSet(lContact.muPolyTagB)");                                                          // :2473

            DeformableObject& lrCarModel = mpaModels[luModelIndex];
            lAddContactEvent.mIDB = BuildProxyCarBodyId(lrCarModel.GetHandlingBodyVolumeInstanceId().muId);

            const f32 lfSeparation =
                rw::math::vpu::Dot(lContact.mPointOnB - lContact.mPointOnA, lContact.mNormal);

            Vector3 lNormal   = lContact.mNormal;
            Vector3 lPointOnB = lContact.mPointOnB;
            if (lfSeparation < -KF_MAX_WHEEL_PENETRATION)
            {
                // Depth, clamped to the tolerance (the console's vminfp against the same splat;
                // inside this branch the clamp always wins, but the min is spelled as issued).
                const f32 lfPenetration = -lfSeparation;
                const f32 lfDepth = (KF_MAX_WHEEL_PENETRATION < lfPenetration)
                                        ? KF_MAX_WHEEL_PENETRATION : lfPenetration;

                // The car's body RIGHT axis -- the first row of the vehicle-physics transform.
                const Vector3 lCarRightAxis = lrCarModel.GetVehiclePhysics()->GetTransform().xAxis;

                // sign(dot3(N, right)) via the console's two-compare vsel ladder: an unordered
                // compare falls through to -1.0, which is why this is not written as a plain
                // ternary on (lfAlong < 0.0f).
                const f32 lfAlong = rw::math::vpu::Dot(lNormal, lCarRightAxis);
                f32 lfSign = -1.0f;
                if (lfAlong >= 0.0f)
                {
                    lfSign = (lfAlong > 0.0f) ? 1.0f : 0.0f;
                }

                lNormal   = lCarRightAxis * lfSign;
                lPointOnB = lContact.mPointOnA - lNormal * lfDepth;
            }

            lAddContactEvent.mPointOnA = lContact.mPointOnA;
            lAddContactEvent.mPointOnB = lPointOnB;
            lAddContactEvent.mNormal   = rw::math::vpu::Negate(lNormal);

            CGS_ASSERT(liEventIndex >= 0 && liEventIndex < 65535,
                       "liEventIndex >= 0 && liEventIndex < 65535");                          // BrnContactId.h:122
            lAddContactEvent.muTag = static_cast<u32>(
                BrnPhysics::ContactId((static_cast<u32>(liEventIndex) & 0xFFFFu) | 0x04000000u));

            CGS_ASSERT(lpSimInput->GetAddContactQueue()->GetLength() + 1
                           < lpSimInput->GetAddContactQueue()->GetMaxLength(),
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetLength() + 1 < "
                       "lpSimModuleInputBuffer->GetAddContactQueue()->GetMaxLength()");                                        // :2513
            lpSimInput->GetAddContactQueue()->AddEventSafe(lAddContactEvent);
        }
    }

    // =================================================================================================
    // The three pair-builder feeders StartVehicleContactGeneration @0x8262AEE8 calls.
    // AddHingedBodyPartPairs is called UNCONDITIONALLY for EVERY overlapping car-car pair
    // (BrnVehicleManagerContactGeneration.cpp:321), race-car-vs-traffic included. The third
    // (AddRaceCarWheelPair) is a NAMED GATE with one missing leaf, see there.
    //
    // Common shape: resolve the car's deformable model, take its aligned deformed bounding box, take
    // the part's oriented bounding box, and append ONE box-vs-box record to the caller's builder with
    // (part pool slot, car model index) as the two primitive tags. StartVehicleContactGeneration
    // collides the finished builders at the end of its overlap walk.
    // =================================================================================================

    // -------------------------------------------------------------------------------------------------
    // AddRaceCarBodyPartPair @0x82605928 (92)  -- one DETACHED body part vs one car.
    //   0x82605938  the part's pool slot == the LOW 16 bits of the packed volume-instance id
    //               (`clrlwi r31, r5, 16` over the whole 64-bit id the caller loads with `ld r5,8(r21)`;
    //               VolumeInstanceId packs {entityWord<<32 | subA<<16 | subB}, so this is muSubB)
    //   0x82605954  IsPartIndexUsed / GetPart on mDetachedPartManager (console this+48112 == mPartPool)
    //   0x8260597C  mbJoinedToVehicle (+0x1E4) -> STILL HINGED, so AddHingedBodyPartPairs owns it
    //   0x82605990  FindModelIndexByEntityID, -1 == the car has no deformable model, nothing to pair
    //   0x826059C8  the tags: `ld 0x1D0(part)` (mRigidBodyId) low byte == GetPoolIndex, and the model
    //               index as a u16; padding flt_82001DA0 == 0.5f
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddRaceCarBodyPartPair(EntityId lEntityId,
                                                    CgsSceneManager::VolumeInstanceId lVolumeInstanceId,
                                                    PrimitivePairListBuilder* lpBuilder)
    {
        const u16 lu16PoolSlot = static_cast<u16>(lVolumeInstanceId.muId & 0xFFFFu);

        if (!mDetachedPartManager.IsPartIndexUsed(lu16PoolSlot))
        {
            return;
        }
        PhysicalBodyPart* lpPart = mDetachedPartManager.GetPartFromIndex(lu16PoolSlot);
        if (lpPart == 0 || lpPart->IsJoinedToVehicle())
        {
            return;
        }

        const s32 liModelIndex = FindModelIndexByEntityID(lEntityId);
        if (liModelIndex == -1)
        {
            return;
        }

        CgsGeometric::Box lPartBox;
        lpPart->GetBoundingBox(&lPartBox);
        CgsGeometric::Box lCarBox;
        mpaModels[liModelIndex].GetAlignedDeformedBoundingBox(&lCarBox);

        const u8 lu8PartIndex = lpPart->GetPoolIndex();
        CGS_ASSERT(lu8PartIndex < PhysicalBodyPartPool::KU_MAX_DETACHED_PARTS,
                   "luPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");                            // :2186
        // The streamed value tail is lowered to the static prefix per the standing project rule.
        CGS_ASSERT(mDetachedPartManager.IsPartIndexUsed(lu8PartIndex), "Bad Part index: ");    // :2187

        lpBuilder->AddPrimitivePair(&lPartBox, &lCarBox, KF_PART_VS_CAR_CONTACT_PADDING,
                                    lu8PartIndex, static_cast<u16>(liModelIndex));
    }

    // -------------------------------------------------------------------------------------------------
    // AddHingedBodyPartPairs @0x82605A98 (84)  -- every STILL-HINGED panel of car A vs car B's body
    // box, and every still-hinged panel of car B vs car A's. Two fixed 50-iteration walks
    // (`cmpwi r26, 0x32`), gated on maPartStates[i] == E_PART_STATE_HINGED (`lbzx` == 3), reading the
    // panel's pool slot out of maIKParts[i].miPartPoolIndex (`lhz` at stride 0x10, then `extsh`).
    // Padding is flt_82001C98 == 1.0f, not the 0.5f its two siblings use.
    //
    // CONSOLE DEFECT, REPRODUCED: BOTH loops pass modelIndex A as the SECOND primitive tag
    // (`clrlwi r8, r28, 16` at 0x82605B50 AND 0x82605BB0; r28 is loaded once at 0x82605AB8 and never
    // re-derived). The second loop's records therefore name car A while carrying car B's box. Kept as
    // issued -- the tag is a debug/attribution field on the pair record, not a lookup key.
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddHingedBodyPartPairs(EntityId lEntityIdA, EntityId lEntityIdB,
                                                    PrimitivePairListBuilder* lpBuilder)
    {
        const s32 liModelIndexA = FindModelIndexByEntityID(lEntityIdA);
        const s32 liModelIndexB = FindModelIndexByEntityID(lEntityIdB);
        if (liModelIndexA == -1 || liModelIndexB == -1)
        {
            return;
        }

        DeformableObject& lrModelA = mpaModels[liModelIndexA];
        DeformableObject& lrModelB = mpaModels[liModelIndexB];

        CgsGeometric::Box lCarBoxA;
        CgsGeometric::Box lCarBoxB;
        lrModelA.GetAlignedDeformedBoundingBox(&lCarBoxA);
        lrModelB.GetAlignedDeformedBoundingBox(&lCarBoxB);

        // car A's hinged panels vs car B's body box (0x82605B28..0x82605B78)
        for (s32 liPart = 0; liPart < KI_MAX_PARTS_PER_MODEL; ++liPart)
        {
            if (lrModelA.GetPartState(liPart) != DeformableObject::E_PART_STATE_HINGED)
            {
                continue;
            }
            const s16 li16PoolSlot = lrModelA.GetIKPartDebug(liPart).GetPartPoolIndex();
            CgsGeometric::Box lPartBox;
            mDetachedPartManager.GetPartFromIndex(static_cast<u16>(li16PoolSlot))
                                ->GetBoundingBox(&lPartBox);
            lpBuilder->AddPrimitivePair(&lPartBox, &lCarBoxB, KF_HINGED_PART_CONTACT_PADDING,
                                        static_cast<u16>(li16PoolSlot),
                                        static_cast<u16>(liModelIndexA));
        }

        // car B's hinged panels vs car A's body box (0x82605B88..0x82605BD8)
        for (s32 liPart = 0; liPart < KI_MAX_PARTS_PER_MODEL; ++liPart)
        {
            if (lrModelB.GetPartState(liPart) != DeformableObject::E_PART_STATE_HINGED)
            {
                continue;
            }
            const s16 li16PoolSlot = lrModelB.GetIKPartDebug(liPart).GetPartPoolIndex();
            CgsGeometric::Box lPartBox;
            mDetachedPartManager.GetPartFromIndex(static_cast<u16>(li16PoolSlot))
                                ->GetBoundingBox(&lPartBox);
            lpBuilder->AddPrimitivePair(&lPartBox, &lCarBoxA, KF_HINGED_PART_CONTACT_PADDING,
                                        static_cast<u16>(li16PoolSlot),
                                        static_cast<u16>(liModelIndexA));   // A, per the defect note
        }
    }

    // -------------------------------------------------------------------------------------------------
    // AddRaceCarWheelPair @0x82605BE8 (137 insns) -- a torn-off wheel vs a car body.
    // ⭐ LANDED 2026-09-23 (crash parity G22-D1). Until now this was a GATE that logged once and
    // appended nothing, so detached wheels passed straight through every car (rivals, traffic, the
    // player) and BridgeDetachedWheelCarContactsToSimulation had nothing to drain. Its blocker, the
    // CYLINDER-vs-BOX AddPrimitivePair overload @0x828149F8, is bodied in CgsPrimitivePairListBuilder.
    //   0x82605BF8  slot = (u16)volumeInstanceId            (clrlwi r29,r5,16)
    //   0x82605C14  !DetachedWheelManager::IsSlotUsed(slot) -> return   (manager at this+0x11CE0)
    //   0x82605C2C  wheel = Get(slot); null -> return
    //   0x82605C44  model = FindModelIndexByEntityID(entity); -1 -> return
    //   0x82605C58..0x82605CE4  the wheel cylinder, inlined: rows (-r2, r1, r0, r3), +0x40 =
    //               wheel+0x7C (radius), +0x44 = wheel+0x78 (half height) == PhysicalWheel::GetCylinder
    //   0x82605CE8..0x82605D68  the car box, inlined: deformable AABB of model+0x194C's physics,
    //               half = (max-min)*0.5 - unk_82FB95E0 (0.1, 0.3, 0.05), centre transformed, fatness
    //               0 -> Box::Set == DeformableObject::GetAlignedDeformedBoundingBox
    //   0x82605D6C..0x82605DDC  (u8)wheel id (ld 0x70, clrlwi 24) == slot, else "Bad Pool Index: " (:0x914)
    //   0x82605DE0..0x82605E00  builder->AddPrimitivePair(&cyl, &box, 0.5 (flt_82001DA0),
    //               (u8)wheel pool index, (u16)model)
    // Both inlined blocks are de-inlined to the named helpers per the project's inlining rule.
    // -------------------------------------------------------------------------------------------------
    void DeformationManager::AddRaceCarWheelPair(EntityId lEntityId,
                                                 CgsSceneManager::VolumeInstanceId lVolumeInstanceId,
                                                 PrimitivePairListBuilder* lpBuilder)
    {
        const u16 lu16WheelSlot = static_cast<u16>(lVolumeInstanceId.muId & 0xFFFFu);

        if (!mDetachedWheelManager.IsSlotUsed(lu16WheelSlot))
        {
            return;
        }
        const PhysicalWheel* lpWheel = mDetachedWheelManager.GetWheel(lu16WheelSlot);
        if (lpWheel == 0)
        {
            return;
        }

        const s32 liModelIndex = FindModelIndexByEntityID(lEntityId);
        if (liModelIndex == -1)
        {
            return;
        }

        CgsGeometric::Cylinder lWheelCylinder;
        lpWheel->GetCylinder(lWheelCylinder);
        CgsGeometric::Box lCarBox;
        mpaModels[liModelIndex].GetAlignedDeformedBoundingBox(&lCarBox);

        // The streamed value tail is lowered to the static prefix per the standing project rule.
        CGS_ASSERT(lpWheel->GetPoolIndex() == static_cast<u8>(lu16WheelSlot), "Bad Pool Index: ");  // :2324

        lpBuilder->AddPrimitivePair(&lWheelCylinder, &lCarBox, KF_WHEEL_VS_CAR_CONTACT_PADDING,
                                    lpWheel->GetPoolIndex(), static_cast<u16>(liModelIndex));
    }
}
}

// FOLDED FROM BrnDeformationManager_VehicleContactFixUp.cpp
// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_VehicleContactFixUp.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the vehicle-vs-vehicle contact
// FIX-UP family (the big-five #1 closure under PhysicsModule::FixUpVehicleContacts):
//     FixUpVehicleContactByInterpolation     @ 0x82604DE0   (389 insns)
//     FixUpVehicleContactWithBoxes           @ 0x825DBCA0   (966 insns)
//     FixUpVehicleContact                    @ 0x825DCBB8   (668 insns)
//     GetInterpolatedContactPointAndNormal   @ 0x82604948   (293 insns)
//     CalculateTangentPoints                 @ 0x825DB418   (545 insns)
//     ProjectSphereContactOntoBox            (no X360 out-of-line copy -- header-inline on
//                                             console, its :771/:810 asserts cite the .h;
//                                             PS3 DecFIGS out-of-line @0x7A5118)
//     ProjectLineOntoPlane                   (inlined on X360; PS3 out-of-line @0x6B80C8)
//
// SLICE TU (home BrnDeformationManager.cpp, still unmounted with ~25 unresolved);
// fold back when the home TU mounts. Same slice pattern as _Construct/_ContactFixups.
//
// SOURCES. Every body is reconstructed against the BURNOUT_X360_ARTIST.XEX asm
// (offsets, constants, assert lines); the PS3 DecFIGS internal build keeps the
// helpers OUT-OF-LINE with full debug names, which is what pins the parameter
// names, the helper-call structure (which X360 inlined) and the local names the
// X360's own streamed asserts corroborate (lVelocityA / lCarBNormal /
// lbRaceCarHitSideOfTraffic / lAToBDist / lCosTheta ...). The de-SIMD'd math is
// written over the vendor rw::math::vpu API (Dot/Normalize/TransformPoint/...),
// the same house style as CameraUtils.cpp / BrnDeformationSensor.cpp.
//
// VMX -> scalar notes (recurring idioms, cited once here):
//   vmsum3fp128 a,b            == Dot(a,b) (3-lane)
//   vrsqrtefp + 2x Newton      == 1/sqrt   (Normalize / Magnitude)
//   vrefp + 2x Newton          == 1/x      (ProjectLineOntoPlane's divide)
//   vandc x, sign-splat        == fabs(x)
//   vrlimi mask 4 with zero    == v.y = 0  (flatten to the horizontal plane)
//   vspltw lane3               == a sphere's .w radius broadcast
//   per-lane self-compare      == RwMathVPU::IsValid (NaN tripwire)
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include <cmath>   // std::sin / std::cos / std::fabs -- std::sin / std::cos stand in for the console's
                   // inlined XM sin/cos minimax polynomials (the XMVectorSinCos follow-up).
#include "SDKs/XboxMath/XMVectorACos.h"   // XboxMath::XMVectorACos (X360 0x821F0980), the tangent angle

#include "rw/math/vpu/vector3_operation.h"                                                // Dot / Normalize / Negate / operator+-
#include "rw/math/vpu/matrix44affine_operation.h"                                         // TransformPoint / TransformVector / InverseOfMatrixWithOrthonormal3x3

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                 // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                        // CgsGeometric::Sphere
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // PotentialContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h" // DeformationSensor (mpSpec / GetLocalSpaceSphere)
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"                              // SensorSpec (mau8NextBoundarySensor)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (GetTransform/GetLinearVelocity)

namespace BrnPhysics
{
namespace Deformation
{
    namespace
    {
        // gJVector @0x82181510 == (0, 1, 0, 0) -- the world up axis the roll gates dot against.
        const Vector3 KV_AXIS_Y(0.0f, 1.0f, 0.0f);

        // rw::math::fpu::EPSILON, lane 0 of stru_8208F620 == FLT_EPSILON (1.1920929e-07f).
        const f32 KF_RW_EPSILON = 1.1920928955078125e-07f;

        // The console keeps this as a dynamically-
        // initialised `const VecFloat BrnPhysics::Deformation::KVF_PROJECTSPHERE_RADIUS_PADDING`
        // (DWARF BrnDeformationManager.h:12, X360 .data @0x82FB9660), so the raw image bytes read
        // 0.0f -- the PRE-INIT value. Its static initialiser IS in the image, at 0x82C5B7E0:
        //     lis  r11, flt_820047C8@ha / lfs f0, flt_820047C8@l(r11) / stfs f0, -0x10(r1)
        //     lis  r11, unk_82FB9660@ha / lvlx v0 / addi r11, r11, unk_82FB9660@l
        //     vspltw v0, v0, 0          / stvx128 v0, r0, r11
        // and flt_820047C8 == 0x3D4CCCCD == 0.05f. (The earlier lis/addi scan missed it because
        // 0x9660 has the high bit set, so the pair is emitted as lis 0x82FC / addi -0x69A0.)
        //
        // It MATTERS: the out-normal band below is `radius - padding`, and the moved contact point
        // sits EXACTLY at `radius` along the dominant box axis by construction. With padding 0 the
        // dominant axis fails `|axisDot| > radius` on a coin-flip of FP rounding, the axis sum comes
        // out {0,0,0}, and Normalize(zero) returns zero -- a NON-UNIT contact normal that trips
        // :1568/:1580 here, BrnDeformationSensor_ValidateAndAddContact's |normal|==1 gate and the
        // bridge's "Un-normalised race car-traffic car contact". 0.05 m of slack is what makes the
        // face the point was projected onto always count.
        const f32 KF_PROJECTSPHERE_RADIUS_PADDING = 0.05f;   // flt_820047C8, via init @0x82C5B7E0

        inline f32 Sign(f32 lfValue)   // vcmpgtfp/vcmpgefp + vsel chain: >0 -> +1, ==0 -> 0, <0 -> -1
        {
            return lfValue > 0.0f ? 1.0f : (lfValue >= 0.0f ? 0.0f : -1.0f);
        }

        // ARTIST 0x82604A84..0x82604A88 loads the complete packed sphere row.
        inline Vector4 LocalSphereVec(const DeformationSensor& lrSensor)
        {
            return Vector4(lrSensor.GetLocalSpaceSphere()->mData.mV);
        }

        inline Vector3 SphereCentre(const Vector4& lrPosRadius)
        {
            return Vector3(lrPosRadius.mV);
        }

        inline bool IsValid3(const Vector3& lrV)   // per-lane self-compare NaN tripwire
        {
            return lrV.mV.x == lrV.mV.x && lrV.mV.y == lrV.mV.y && lrV.mV.z == lrV.mV.z;
        }

        // v.y = 0 then Normalize -- the vrlimi(4, zero) + vrsqrtefp idiom both interpolation
        // fallback arms share.
        inline Vector3 NormalizeFlattened(Vector3 lVector)
        {
            lVector.mV = __vrlimi(lVector.mV, __vspltisw(0), 4, 0);
            return rw::math::vpu::Normalize(lVector);
        }
    }

    // ==========================================================================================
    // ProjectLineOntoPlane  (X360: inlined into ByInterpolation's tail; PS3 out-of-line @0x6B80C8)
    //
    //   t = Dot(lPointOnPlane - lPointOnLine, lPlaneNormal) / Dot(lLineDir, lPlaneNormal)
    //   return lPointOnLine + lLineDir * t
    // (the console divide is vrefp + two Newton refines).
    // ==========================================================================================
    Vector3 DeformationManager::ProjectLineOntoPlane(Vector3 lPointOnLine, Vector3 lLineDir,
                                                     Vector3 lPointOnPlane, Vector3 lPlaneNormal)
    {
        // ARTIST 0x82605340..0x826053B8: VMX dot products, two reciprocal
        // refinements, multiply and fused point reconstruction (all four lanes).
        const __vector4 dirDot = __vmsum3fp(lLineDir.mV, lPlaneNormal.mV);
        const __vector4 pointDot = __vmsum3fp(__vsubfp(lPointOnPlane.mV, lPointOnLine.mV), lPlaneNormal.mV);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        __vector4 reciprocal = __vrefp(dirDot);
        __vector4 error = __vnmsubfp(reciprocal, dirDot, one);
        reciprocal = __vmaddfp(reciprocal, error, reciprocal);
        error = __vnmsubfp(reciprocal, dirDot, one);
        reciprocal = __vmaddfp(reciprocal, error, reciprocal);
        const __vector4 distance = __vmulfp(reciprocal, pointDot);
        return Vector3(__vmaddfp(lLineDir.mV, distance, lPointOnLine.mV));
    }

    // ==========================================================================================
    // ProjectSphereContactOntoBox  (header-inline on console -- its asserts cite
    // BrnDeformationManager.h:771/:810; PS3 out-of-line @0x7A5118 pins the signature)
    //
    // Slide the contact point along lNormal until it sits on the sphere's surface as measured
    // along the box axis most aligned with the normal, then report which box faces the moved
    // point pokes out of as the out normal.
    // ==========================================================================================
    Vector3 DeformationManager::ProjectSphereContactOntoBox(const CgsGeometric::Sphere& lWorldSphere,
                                                            Vector3 lNormal, Vector3 lContactPoint,
                                                            Matrix44Affine lBoxTransform,
                                                            Vector3* lpNormalOut) const
    {
        const Vector3 lSpherePos = SphereCentre(Vector4(lWorldSphere.mData.mV));
        const f32     lfRadius   = lWorldSphere.mData.mV.w;

        CGS_ASSERT(rw::math::vpu::Dot(lNormal, lContactPoint - lSpherePos) > 0.0f,
                   "RwMathVPU::Dot( lNormal, lContactPoint - lWorldSphere.GetPosition() ) > RwMathVPU::GetVecFloat_Zero()");   // BrnDeformationManager.h:771

        // lDirectionToResolvePlusDot: the box axis row whose |dot lNormal| is largest, carrying
        // its dot in the "plus" lane (the vperm<0,1,2,7> + two vcmpgtfp/vsel chain).
        Vector3 lDirection = lBoxTransform.xAxis;
        f32     lfDot      = rw::math::vpu::Dot(lBoxTransform.xAxis, lNormal);
        {
            const f32 lfDotY = rw::math::vpu::Dot(lBoxTransform.yAxis, lNormal);
            if (std::fabs(lfDotY) > std::fabs(lfDot))
            {
                lDirection = lBoxTransform.yAxis;
                lfDot      = lfDotY;
            }
            const f32 lfDotZ = rw::math::vpu::Dot(lBoxTransform.zAxis, lNormal);
            if (std::fabs(lfDotZ) > std::fabs(lfDot))
            {
                lDirection = lBoxTransform.zAxis;
                lfDot      = lfDotZ;
            }
        }
        // The console compares |dot| against rw::math::fpu::EPSILON (stru_8208F620 lane 0).
        CGS_ASSERT(!(KF_RW_EPSILON >= std::fabs(lfDot)),
                   "!RwMathVPU::IsZero( lDirectionToResolvePlusDot.GetPlus() )");   // BrnDeformationManager.h:810

        // Move the point along lNormal by |(radius - |distance along the axis|) / dot|.
        const f32 lfDistAlong = std::fabs(rw::math::vpu::Dot(lContactPoint - lSpherePos, lDirection));
        const f32 lfStep      = std::fabs((lfRadius - lfDistAlong) * (1.0f / lfDot));
        const Vector3 lResult = lContactPoint + lNormal * lfStep;

        // Out normal: sum the box axes the moved point sits outside of (|axis dot| beyond
        // radius - KVF_PROJECTSPHERE_RADIUS_PADDING), each signed toward the point's side.
        const Vector3 lFromCentre = lResult - lSpherePos;
        const f32     lfLimit     = lfRadius - KF_PROJECTSPHERE_RADIUS_PADDING;

        Vector3 lSum(__vspltisw(0));
        const f32 lfAxisDotX = rw::math::vpu::Dot(lFromCentre, lBoxTransform.xAxis);
        if (std::fabs(lfAxisDotX) > lfLimit) lSum = lSum + lBoxTransform.xAxis * Sign(lfAxisDotX);
        const f32 lfAxisDotY = rw::math::vpu::Dot(lFromCentre, lBoxTransform.yAxis);
        if (std::fabs(lfAxisDotY) > lfLimit) lSum = lSum + lBoxTransform.yAxis * Sign(lfAxisDotY);
        const f32 lfAxisDotZ = rw::math::vpu::Dot(lFromCentre, lBoxTransform.zAxis);
        if (std::fabs(lfAxisDotZ) > lfLimit) lSum = lSum + lBoxTransform.zAxis * Sign(lfAxisDotZ);

        *lpNormalOut = rw::math::vpu::Normalize(lSum);
        return lResult;
    }

    // ==========================================================================================
    // CalculateTangentPoints @ 0x825DB418
    //
    // The external tangent line between two spheres, in the horizontal plane: flatten both
    // centres to their common average height, rotate the A->B direction about the world Y axis
    // by theta = acos((rA - rB) / |AToB|) toward lPointOnSide, and emit the two touch points
    // (centre + dir * radius) plus the tangent direction. The console's acos is a real
    // XMVectorACos call (XboxMath::XMVectorACos here) and the sin/cos pair an inlined XM minimax
    // polynomial, for which std::sin / std::cos still stand in.
    // ==========================================================================================
    void DeformationManager::CalculateTangentPoints(CgsGeometric::Sphere lSphereAIn,
                                                    CgsGeometric::Sphere lSphereBIn,
                                                    Vector3 lPointOnSide, Vector3* lpResultA,
                                                    Vector3* lpResultB, Vector3* lpOutNormal)
    {
        // ARTIST 0x825DB430..0x825DB4E8: retain the sphere's radius lane
        // while inserting the mean Y computed via the reciprocal estimate sequence.
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 two = __vcfsx(__vspltisw(2), 0);
        __vector4 half = __vrefp(two);
        __vector4 error = __vnmsubfp(half, two, one);
        half = __vmaddfp(half, error, half);
        error = __vnmsubfp(half, two, one);
        half = __vmaddfp(half, error, half);
        const __vector4 meanY = __vmulfp(half, __vaddfp(__vspltw(lSphereAIn.mData.mV, 1),
                                                        __vspltw(lSphereBIn.mData.mV, 1)));
        const Vector3 lCentreA(__vrlimi(lSphereAIn.mData.mV, meanY, 4, 0));
        const Vector3 lCentreB(__vrlimi(lSphereBIn.mData.mV, meanY, 4, 0));
        const f32 lfRadiusA = lSphereAIn.mData.mV.w;
        const f32 lfRadiusB = lSphereBIn.mData.mV.w;

        // ARTIST 0x825DB4F4..0x825DB5C4: magnitude with the zero guard,
        // reciprocal refinement and component clamp before XMVectorACos.
        const __vector4 delta = __vsubfp(lCentreB.mV, lCentreA.mV);
        const __vector4 lenSq = __vmsum3fp(delta, delta);
        const __vector4 zero = __vspltisw(0);
        const __vector4 halfFloat = __vcfsx(__vspltisw(1), 1);
        __vector4 rsqrt = __vrsqrtefp(lenSq);
        __vector4 rsqrtError = __vnmsubfp(lenSq, __vmulfp(rsqrt, rsqrt), one);
        rsqrt = __vmaddfp(__vmulfp(rsqrt, halfFloat), rsqrtError, rsqrt);
        rsqrtError = __vnmsubfp(lenSq, __vmulfp(rsqrt, rsqrt), one);
        rsqrt = __vmaddfp(__vmulfp(rsqrt, halfFloat), rsqrtError, rsqrt);
        const __vector4 distance = __vsel(__vmulfp(lenSq, rsqrt), zero, __vcmpeqfp(zero, lenSq));
        __vector4 invDistance = __vrefp(distance);
        error = __vnmsubfp(invDistance, distance, one);
        invDistance = __vmaddfp(invDistance, error, invDistance);
        error = __vnmsubfp(invDistance, distance, one);
        invDistance = __vmaddfp(invDistance, error, invDistance);
        const Vector3 lAToBDir(__vmulfp(invDistance, delta));
        const __vector4 radiusDelta = __vsubfp(__vspltw(lCentreA.mV, 3), __vspltw(lCentreB.mV, 3));
        const __vector4 cosTheta = __vminfp(one, __vmaxfp(__vcfsx(__vspltisw(-1), 0),
                                                          __vmulfp(invDistance, radiusDelta)));
        const f32 lfTheta = XboxMath::XMVectorACos(cosTheta.x);

        // Rotate lAToBDir about the world Y axis by theta; if the rotated direction points away
        // from lPointOnSide (Dot(lPointOnSide - lCentreA, rotated) < 0), rotate by -theta instead
        // (the console re-runs the sin/cos polynomial on the negated angle).
        f32 lfSin = std::sin(lfTheta);
        const f32 lfCos = std::cos(lfTheta);
        Vector3 lRotated(lAToBDir.mV.x * lfCos + lAToBDir.mV.z * lfSin,
                             lAToBDir.mV.y,
                             lAToBDir.mV.z * lfCos - lAToBDir.mV.x * lfSin);
        if (rw::math::vpu::Dot(lPointOnSide - lCentreA, lRotated) < 0.0f)
        {
            lfSin    = -lfSin;   // sin(-theta); cos unchanged
            {
                const Vector3 lAggregateValue(lAToBDir.mV.x * lfCos + lAToBDir.mV.z * lfSin,
                                lAToBDir.mV.y,
                                lAToBDir.mV.z * lfCos - lAToBDir.mV.x * lfSin);
                lRotated = lAggregateValue;
            }
        }

        // Flatten + normalize the tangent direction, emit the touch points.
        const Vector3 lTangentDir = NormalizeFlattened(lRotated);
        *lpResultA  = lCentreA + lTangentDir * lfRadiusA;
        *lpResultB  = lCentreB + lTangentDir * lfRadiusB;
        *lpOutNormal = lTangentDir;

        // Console streams "Bad normal: " + SphereA/SphereB/PointOnSide/lAToB/lAToBDist/lAToBDir/
        // lCosTheta/lTheta/ResultA/lResultB through gpcMessageBuffer; lowered to CGS_ASSERT with
        // the static prefix per the standing rule.
        CGS_ASSERT(IsValid3(lTangentDir), "Bad normal: ");   // BrnDeformationManager.cpp:1409
    }

    // ==========================================================================================
    // GetInterpolatedContactPointAndNormal @ 0x82604948
    //
    // In the model's local space: pick the sensor's boundary neighbour on the side of the other
    // car; if none faces it, report the plain sphere-surface point (FALSE). If the other car sits
    // closer to the neighbour's sphere than to this one's, keep the incoming point (FALSE).
    // Otherwise interpolate: the external tangent line between the two sensor spheres through
    // the contact point (CalculateTangentPoints), returning its touch point + direction (TRUE).
    // Outputs are transformed back to world space either way.
    // ==========================================================================================
    bool DeformationManager::GetInterpolatedContactPointAndNormal(DeformableObject* lpModel,
                                                                  s32 liSensorIndex,
                                                                  Vector3 lWorldPosIn,
                                                                  Vector3 lOtherSensorPos,
                                                                  Vector3* lpOutWorldPos,
                                                                  Vector3* lpInOutNormal)
    {
        CGS_ASSERT(IsValid3(*lpInOutNormal), "RwMathVPU::IsValid(*lpInOutNormal)");   // :1238

        const Matrix44Affine& lTransform = lpModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine  lInverse   = rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lTransform);
        const Vector3 lLocalPosIn    = rw::math::vpu::TransformPoint(lInverse, lWorldPosIn);
        const Vector3 lLocalOtherPos = rw::math::vpu::TransformPoint(lInverse, lOtherSensorPos);

        const DeformationSensor& lrSensor      = lpModel->GetSensorDebug(liSensorIndex);
        const Vector4&           lLocalSphere  = LocalSphereVec(lrSensor);
        const Vector3            lLocalCentre  = SphereCentre(lLocalSphere);

        // Neighbour pick: boundary neighbour 0, else 1, each accepted unless its sphere lies strictly
        // on the far side from the other car. 0x82604AE8 (neighbour 0) and 0x82604B2C (neighbour 1) are
        // `vcmpgtfp. 0 > dot ; beq accept`, and `beq` is taken when the all-true bit is CLEAR -- so the
        // test is !(0 > dot), and a NaN dot ACCEPTS the neighbour. (`dot >= 0` refused a NaN dot and
        // fell to the no-neighbour arm, where the console runs the tangent arm.)
        const DeformationSensor* lpNeighbour = 0;
        const u8 lu8Neighbour0 = lrSensor.mpSpec->mau8NextBoundarySensor[0];
        if (static_cast<s32>(lu8Neighbour0) != liSensorIndex)
        {
            const Vector3 lToOther = lLocalOtherPos - lLocalCentre;

            const DeformationSensor& lrCandidate0 = lpModel->GetSensorDebug(lu8Neighbour0);
            if (!(0.0f > rw::math::vpu::Dot(SphereCentre(LocalSphereVec(lrCandidate0)) - lLocalCentre, lToOther)))   // 0x82604AE8
            {
                lpNeighbour = &lrCandidate0;
            }
            else
            {
                const u8 lu8Neighbour1 = lrSensor.mpSpec->mau8NextBoundarySensor[1];
                if (static_cast<s32>(lu8Neighbour1) != liSensorIndex)
                {
                    const DeformationSensor& lrCandidate1 = lpModel->GetSensorDebug(lu8Neighbour1);
                    if (!(0.0f > rw::math::vpu::Dot(SphereCentre(LocalSphereVec(lrCandidate1)) - lLocalCentre, lToOther)))   // 0x82604B2C
                    {
                        lpNeighbour = &lrCandidate1;
                    }
                }
            }
        }

        Vector3 lLocalPoint;
        Vector3 lLocalNormal;
        bool    lbInterpolated = false;

        if (lpNeighbour == 0)
        {
            // No neighbour faces the other car: plain sphere-surface point along the flattened
            // direction from the sensor centre to the incoming point.
            lLocalNormal = NormalizeFlattened(lLocalPosIn - lLocalCentre);
            lLocalPoint  = lLocalCentre + lLocalNormal * lLocalSphere.mV.w;
        }
        else
        {
            const Vector4& lNeighbourSphere = LocalSphereVec(*lpNeighbour);
            const Vector3  lNeighbourCentre = SphereCentre(lNeighbourSphere);

            // Which sphere is the other car closer to (flattened distances to each surface)?
            Vector3 lToThis = lLocalOtherPos - lLocalCentre;      lToThis.mV = __vrlimi(lToThis.mV, __vspltisw(0), 4, 0);
            Vector3 lToNbr  = lLocalOtherPos - lNeighbourCentre;  lToNbr.mV = __vrlimi(lToNbr.mV, __vspltisw(0), 4, 0);
            const f32 lfDistThisSq = rw::math::vpu::Dot(lToThis, lToThis);
            const f32 lfDistNbrSq  = rw::math::vpu::Dot(lToNbr, lToNbr);
            const f32 lfDistThis   = (lfDistThisSq == 0.0f ? 0.0f : std::sqrt(lfDistThisSq)) - lLocalSphere.mV.w;
            const f32 lfDistNbr    = (lfDistNbrSq  == 0.0f ? 0.0f : std::sqrt(lfDistNbrSq))  - lNeighbourSphere.mV.w;

            if (lfDistThis > lfDistNbr)
            {
                // The other car is closer to the neighbour: keep the incoming point (FALSE arm;
                // the console keeps lLocalPosIn verbatim, `vmr v11, v1`).
                lLocalNormal = NormalizeFlattened(lLocalPosIn - lLocalCentre);
                lLocalPoint  = lLocalPosIn;
            }
            else
            {
                CgsGeometric::Sphere lSphereA;
                {
                    const Vector4 lAggregateValue(lLocalSphere.mV);
                    lSphereA.mData.mV = lAggregateValue.mV;
                }
                CgsGeometric::Sphere lSphereB;
                {
                    const Vector4 lAggregateValue(lNeighbourSphere.mV);
                    lSphereB.mData.mV = lAggregateValue.mV;
                }

                Vector3 lResultB;   // the neighbour-sphere touch point -- computed, unused (console keeps it stack-only)
                CalculateTangentPoints(lSphereA, lSphereB, lLocalPosIn,
                                       &lLocalPoint, &lResultB, &lLocalNormal);
                lbInterpolated = true;
            }
        }

        *lpOutWorldPos  = rw::math::vpu::TransformPoint(lTransform, lLocalPoint);
        *lpInOutNormal  = rw::math::vpu::TransformVector(lTransform, lLocalNormal);

        CGS_ASSERT(IsValid3(*lpInOutNormal), "RwMathVPU::IsValid(*lpInOutNormal)");   // :1337
        return lbInterpolated;
    }

    // ==========================================================================================
    // FixUpVehicleContactByInterpolation @ 0x82604DE0
    //
    // The racecar-vs-vehicle dispatcher: bail to the box fix-up when either car is rolled over;
    // otherwise try to interpolate each side's contact point/normal across its sensor-sphere
    // chain (only where the contact normal is near-horizontal against that car's up axis and
    // the sensor has distinct boundary neighbours); if neither side even attempts, fall back to
    // the box fix-up. Otherwise re-seat the contact from the deeper side's sphere and project
    // both points back onto the (new-normal) contact plane along the ORIGINAL normal.
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContactByInterpolation(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lRaceCarVolInstId,
        CgsSceneManager::VolumeInstanceId lTrafficVolInstId)
    {
        CgsDev::PerfMonCpu::StartMonitor(miFixUpRaceCarTrafficContact);   // this+76724

        const u32 luOwnerA = static_cast<u32>(lRaceCarVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lTrafficVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT(luOwnerA == 1u && (luOwnerB == 2u || luOwnerB == 1u),
                   "(lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)"
                   " || (lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)");   // :1606

        const Vector3 lNormal = lrContact.mNormal;

        const EntityId lEntityRaceCarModelIndex = { static_cast<u32>(lRaceCarVolInstId.muId >> 32) };
        const s32 liRaceCarModelIndex = FindModelIndexByEntityID(lEntityRaceCarModelIndex);
        const EntityId lEntityTrafficModelIndex = { static_cast<u32>(lTrafficVolInstId.muId >> 32) };
        const s32 liTrafficModelIndex = FindModelIndexByEntityID(lEntityTrafficModelIndex);
        CGS_ASSERT(liRaceCarModelIndex != -1, "liRaceCarModelIndex != -1");   // :1631
        CGS_ASSERT(liTrafficModelIndex != -1, "liTrafficModelIndex != -1");   // :1632

        DeformableObject* lpRaceCarModel = &mpaModels[liRaceCarModelIndex];
        DeformableObject* lpTrafficModel = &mpaModels[liTrafficModelIndex];

        const CgsGeometric::Sphere lTrafficSphere =
            lpTrafficModel->GetDeformationSphereFromVolumeInstance(lTrafficVolInstId);
        const CgsGeometric::Sphere lRaceCarSphere =
            lpRaceCarModel->GetDeformationSphereFromVolumeInstance(lRaceCarVolInstId);

        const Matrix44Affine& lRaceCarTransform = lpRaceCarModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lTrafficTransform = lpTrafficModel->GetVehiclePhysics()->GetTransform();

        // Roll gate: either car's up row dotted with world up (yAxis.y) under 0.5 -> boxes.
        if (rw::math::vpu::Dot(lRaceCarTransform.yAxis, KV_AXIS_Y) < 0.5f ||
            rw::math::vpu::Dot(lTrafficTransform.yAxis, KV_AXIS_Y) < 0.5f)
        {
            FixUpVehicleContactWithBoxes(lrContact, lRaceCarVolInstId, lTrafficVolInstId);
            CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
            return;
        }

        // Defaults (overwritten by a successful interpolation on each side).
        Vector3 lTrafficPoint  = lrContact.mPointOnB;
        Vector3 lRaceCarPoint  = lrContact.mPointOnA;
        Vector3 lTrafficNormal = lNormal;
        Vector3 lRaceCarNormal = rw::math::vpu::Negate(lNormal);
        bool    lbUseBoxes     = true;

        // ---- traffic side ----------------------------------------------------------------
        const s32 liTrafficSensorIndex =
            static_cast<s32>(static_cast<u8>(lTrafficVolInstId.muId)) - 1;
        if (std::fabs(rw::math::vpu::Dot(lNormal, lTrafficTransform.yAxis)) < 0.5f)
        {
            const DeformationSensor& lrSensor = lpTrafficModel->GetSensorDebug(liTrafficSensorIndex);
            if (!(static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[0]) == liTrafficSensorIndex &&
                  static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[1]) == liTrafficSensorIndex))
            {
                lbUseBoxes = false;
                if (!GetInterpolatedContactPointAndNormal(lpTrafficModel, liTrafficSensorIndex,
                                                          lrContact.mPointOnB,
                                                          SphereCentre(Vector4(lRaceCarSphere.mData.mV)),
                                                          &lTrafficPoint, &lTrafficNormal))
                {
                    lTrafficNormal = NormalizeFlattened(lTrafficNormal);
                    CGS_ASSERT(IsValid3(lTrafficNormal), "RwMathVPU::IsValid(lTrafficNormal)");   // :1685
                }
            }
        }

        // ---- racecar side ----------------------------------------------------------------
        const s32 liRaceCarSensorIndex =
            static_cast<s32>(static_cast<u8>(lRaceCarVolInstId.muId)) - 1;
        if (std::fabs(rw::math::vpu::Dot(lRaceCarNormal, lRaceCarTransform.yAxis)) < 0.5f)
        {
            const DeformationSensor& lrSensor = lpRaceCarModel->GetSensorDebug(liRaceCarSensorIndex);
            if (!(static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[0]) == liRaceCarSensorIndex &&
                  static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[1]) == liRaceCarSensorIndex))
            {
                lbUseBoxes = false;
                if (!GetInterpolatedContactPointAndNormal(lpRaceCarModel, liRaceCarSensorIndex,
                                                          lrContact.mPointOnA,
                                                          SphereCentre(Vector4(lTrafficSphere.mData.mV)),
                                                          &lRaceCarPoint, &lRaceCarNormal))
                {
                    lRaceCarNormal = NormalizeFlattened(lRaceCarNormal);
                    CGS_ASSERT(IsValid3(lRaceCarNormal), "RwMathVPU::IsValid(lRaceCarNormal)");   // :1710
                }
            }
        }

        if (lbUseBoxes)
        {
            FixUpVehicleContactWithBoxes(lrContact, lRaceCarVolInstId, lTrafficVolInstId);
            CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
            return;
        }

        // ---- commit: pick the deeper side, then re-project both points ---------------------
        const Vector3 lRaceCarCentre = SphereCentre(Vector4(lRaceCarSphere.mData.mV));
        const Vector3 lTrafficCentre = SphereCentre(Vector4(lTrafficSphere.mData.mV));
        const f32     lfRaceCarRadius = lRaceCarSphere.mData.mV.w;
        const f32     lfTrafficRadius = lTrafficSphere.mData.mV.w;

        lrContact.mPointOnB = lTrafficPoint;
        lrContact.mPointOnA = lRaceCarPoint;

        const f32 lfDepthB = rw::math::vpu::Dot(lTrafficPoint - lRaceCarCentre,
                                                rw::math::vpu::Negate(lTrafficNormal)) - lfRaceCarRadius;
        const f32 lfDepthA = rw::math::vpu::Dot(lRaceCarPoint - lTrafficCentre,
                                                rw::math::vpu::Negate(lRaceCarNormal)) - lfTrafficRadius;

        Vector3 lChosenNormal;
        if (lfDepthA > lfDepthB)
        {
            lChosenNormal       = lTrafficNormal;
            lrContact.mNormal   = lTrafficNormal;
            lrContact.mPointOnA = lRaceCarCentre - lTrafficNormal * lfRaceCarRadius;
        }
        else
        {
            lChosenNormal       = rw::math::vpu::Negate(lRaceCarNormal);
            lrContact.mNormal   = lChosenNormal;
            lrContact.mPointOnB = lTrafficCentre + lChosenNormal * lfTrafficRadius;
        }

        // Both points slid onto the new contact plane ALONG THE ORIGINAL normal (the two
        // inlined ProjectLineOntoPlane instances, vrefp + 2x Newton divides).
        lrContact.mPointOnA = ProjectLineOntoPlane(lRaceCarCentre, rw::math::vpu::Negate(lNormal),
                                                   lrContact.mPointOnA, lChosenNormal);
        lrContact.mPointOnB = ProjectLineOntoPlane(lTrafficCentre, lNormal,
                                                   lrContact.mPointOnB, rw::math::vpu::Negate(lChosenNormal));

        CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
    }

    // ==========================================================================================
    // FixUpVehicleContactWithBoxes @ 0x825DBCA0
    //
    // The box fallback: project both contact points onto their car's box along +-mNormal, then
    // choose the final normal -- if the (projected) racecar point sits INSIDE the traffic box
    // on the side the normal points to (lbRaceCarHitSideOfTraffic), snap the normal to the
    // traffic box's lateral X axis; otherwise keep whichever projected face normal opposes the
    // relative velocity harder.
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContactWithBoxes(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lRaceCarVolInstId,
        CgsSceneManager::VolumeInstanceId lTrafficVolInstId)
    {
        const u32 luOwnerA = static_cast<u32>(lRaceCarVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lTrafficVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT(luOwnerA == 1u && (luOwnerB == 2u || luOwnerB == 1u),
                   "lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && (lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE || "
                   "lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)");   // :1456

        const EntityId lEntityRaceCarModelIndex = { static_cast<u32>(lRaceCarVolInstId.muId >> 32) };
        const s32 liRaceCarModelIndex = FindModelIndexByEntityID(lEntityRaceCarModelIndex);
        const EntityId lEntityTrafficModelIndex = { static_cast<u32>(lTrafficVolInstId.muId >> 32) };
        const s32 liTrafficModelIndex = FindModelIndexByEntityID(lEntityTrafficModelIndex);
        CGS_ASSERT(liRaceCarModelIndex != -1, "liRaceCarModelIndex != -1");   // :1484
        CGS_ASSERT(liTrafficModelIndex != -1, "liTrafficModelIndex != -1");   // :1485

        DeformableObject* lpRaceCarModel = &mpaModels[liRaceCarModelIndex];
        DeformableObject* lpTrafficModel = &mpaModels[liTrafficModelIndex];

        const CgsGeometric::Sphere lTrafficSphere =
            lpTrafficModel->GetDeformationSphereFromVolumeInstance(lTrafficVolInstId);
        const CgsGeometric::Sphere lRaceCarSphere =
            lpRaceCarModel->GetDeformationSphereFromVolumeInstance(lRaceCarVolInstId);

        const Matrix44Affine& lRaceCarTransform = lpRaceCarModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lTrafficTransform = lpTrafficModel->GetVehiclePhysics()->GetTransform();

        const Vector3 lNormal = lrContact.mNormal;
        Vector3 lRaceCarNormal(__vspltisw(0));
        Vector3 lTrafficNormal(__vspltisw(0));

        lrContact.mPointOnA = ProjectSphereContactOntoBox(lRaceCarSphere, rw::math::vpu::Negate(lNormal),
                                                          lrContact.mPointOnA, lRaceCarTransform,
                                                          &lRaceCarNormal);
        lrContact.mPointOnB = ProjectSphereContactOntoBox(lTrafficSphere, lNormal,
                                                          lrContact.mPointOnB, lTrafficTransform,
                                                          &lTrafficNormal);

        // The projected racecar point + the contact normal in TRAFFIC local space, tested
        // against the traffic body's box half extents (vehPhys+0x6A0).
        const Matrix44Affine lTrafficInverse =
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lTrafficTransform);
        const Vector3 lPointOnRaceCarLocal =
            rw::math::vpu::TransformPoint(lTrafficInverse, lrContact.mPointOnA);
        const Vector3 lNormalTrafficLocal =
            rw::math::vpu::TransformVector(lTrafficInverse, lrContact.mNormal);
        const Vector3 lHalfExtents = lpTrafficModel->GetHalfExtents();

        // ⚠️ THE X TEST IS NOT THE Y/Z TEST -- the console's three vcmpgefp. have DIFFERENT operand
        // order and the tree had all three the same way round. Verbatim:
        //   0x825DC508  vcmpgefp. v0, v12(|local.x|), v0(halfExtents.x)   -> |local.x| >= hx
        //   0x825DC538  vcmpgefp. v0, v0(halfExtents.y), v12(|local.y|)   -> hy >= |local.y|
        //   0x825DC568  vcmpgefp. v0, v0(halfExtents.z), v13(|local.z|)   -> hz >= |local.z|
        // which is what the name says: the race car's point is BEYOND the traffic box laterally
        // while still level with it and alongside it -- it hit the SIDE. (Inside-the-box on all
        // three axes would be a containment test, and would snap the normal to +-xAxis for
        // head-on hits too.)
        const bool lbRaceCarHitSideOfTraffic =
            std::fabs(lPointOnRaceCarLocal.mV.x) >= lHalfExtents.mV.x &&
            lHalfExtents.mV.y >= std::fabs(lPointOnRaceCarLocal.mV.y) &&
            lHalfExtents.mV.z >= std::fabs(lPointOnRaceCarLocal.mV.z) &&
            (lPointOnRaceCarLocal.mV.x * lNormalTrafficLocal.mV.x) > 0.0f;

        if (lbRaceCarHitSideOfTraffic)
        {
            // Snap the normal to the traffic box's lateral axis, signed by which side was hit.
            lrContact.mNormal = lTrafficTransform.xAxis * Sign(lPointOnRaceCarLocal.mV.x);
        }
        else
        {
            const Vector3 lVelocityA = lpRaceCarModel->GetVehiclePhysics()->GetLinearVelocity();
            const Vector3 lVelocityB = lpTrafficModel->GetVehiclePhysics()->GetLinearVelocity();
            const Vector3 lRelativeVelocity = lVelocityB - lVelocityA;

            const f32 lvfRelativeVelocityDotA = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lRaceCarNormal));
            const f32 lvfRelativeVelocityDotB = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lTrafficNormal));

            lrContact.mNormal = lvfRelativeVelocityDotB > lvfRelativeVelocityDotA
                                    ? rw::math::vpu::Negate(lRaceCarNormal)
                                    : lTrafficNormal;

            // Console streams mNormal/lNormal/the dots/velocities/lPointOnRaceCarLocal/
            // lRaceCarNormal/lTrafficNormal/lbRaceCarHitSideOfTraffic/lTrafficTransform; lowered
            // per the standing rule.
            CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                       "lPotentialContact.mNormal: ");   // :1568
        }

        // Common exit gate -- streams the full local set incl. both spheres and
        // "lpTrafficModel->GetHalfExtents(): "; lowered per the standing rule.
        CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                   "\n lPotentialContact.mNormal: ");   // :1580
    }

    // ==========================================================================================
    // FixUpVehicleContact @ 0x825DCBB8
    //
    // The car-vs-car (or traffic-vs-traffic) box fix-up: project both contact points onto their
    // car's box along +-mNormal, then keep whichever projected face normal opposes the relative
    // velocity harder. (The traffic-vs-traffic contacts arrive here from the driver's queue-[0]
    // walk with both ids already rewritten to physics space.)
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lCarAVolInstId,
        CgsSceneManager::VolumeInstanceId lCarBVolInstId)
    {
        const u32 luOwnerA = static_cast<u32>(lCarAVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lCarBVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT((luOwnerA == 1u && luOwnerB == 1u) || (luOwnerA == 2u && luOwnerB == 2u),
                   "( lCarAVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lCarBVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR ) || "
                   "( lCarAVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && lCarBVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");   // :1772

        const EntityId lEntityCarAModelIndex = { static_cast<u32>(lCarAVolInstId.muId >> 32) };
        const s32 liCarAModelIndex = FindModelIndexByEntityID(lEntityCarAModelIndex);
        const EntityId lEntityCarBModelIndex = { static_cast<u32>(lCarBVolInstId.muId >> 32) };
        const s32 liCarBModelIndex = FindModelIndexByEntityID(lEntityCarBModelIndex);
        CGS_ASSERT(liCarAModelIndex != -1, "liCarAModelIndex != -1");   // :1798
        CGS_ASSERT(liCarBModelIndex != -1, "liCarBModelIndex != -1");   // :1799

        DeformableObject* lpCarAModel = &mpaModels[liCarAModelIndex];
        DeformableObject* lpCarBModel = &mpaModels[liCarBModelIndex];

        const CgsGeometric::Sphere lCarBSphere =
            lpCarBModel->GetDeformationSphereFromVolumeInstance(lCarBVolInstId);
        const CgsGeometric::Sphere lCarASphere =
            lpCarAModel->GetDeformationSphereFromVolumeInstance(lCarAVolInstId);

        const Matrix44Affine& lCarATransform = lpCarAModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lCarBTransform = lpCarBModel->GetVehiclePhysics()->GetTransform();

        const Vector3 lNormal = lrContact.mNormal;
        Vector3 lCarANormal(__vspltisw(0));
        Vector3 lCarBNormal(__vspltisw(0));

        lrContact.mPointOnA = ProjectSphereContactOntoBox(lCarASphere, rw::math::vpu::Negate(lNormal),
                                                          lrContact.mPointOnA, lCarATransform,
                                                          &lCarANormal);
        lrContact.mPointOnB = ProjectSphereContactOntoBox(lCarBSphere, lNormal,
                                                          lrContact.mPointOnB, lCarBTransform,
                                                          &lCarBNormal);

        const Vector3 lVelocityA = lpCarAModel->GetVehiclePhysics()->GetLinearVelocity();
        const Vector3 lVelocityB = lpCarBModel->GetVehiclePhysics()->GetLinearVelocity();
        const Vector3 lRelativeVelocity = lVelocityB - lVelocityA;

        const f32 lvfRelativeVelocityDotA = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lCarANormal));
        const f32 lvfRelativeVelocityDotB = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lCarBNormal));

        lrContact.mNormal = lvfRelativeVelocityDotB > lvfRelativeVelocityDotA
                                ? rw::math::vpu::Negate(lCarANormal)
                                : lCarBNormal;

        // Console streams mNormal/lNormal/the dots/lRelativeVelocity/lVelocityA/lVelocityB/
        // lCarANormal/lCarBNormal/lCarATransform/lCarBTransform; lowered per the standing rule.
        CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                   "lPotentialContact.mNormal: ");   // :1848
    }
}
}
