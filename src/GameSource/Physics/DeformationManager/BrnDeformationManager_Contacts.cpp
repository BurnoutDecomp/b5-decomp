// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_Contacts.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the per-frame CONTACT + QUERY spine
// of the deformation manager. This TU group bodies the manager's contact-routing,
// penetration-solve, triangle-cache, contact-fixup and per-car spatial-query methods,
// reconstructed STORE-FOR-STORE / BRANCH-FOR-BRANCH from the X360 ARTIST.XEX asm in
// scratchpad/wave5/dos_manager.txt (each function's @addr is cited at its body).
//
// LAYOUT NOTE: the X360 manager keeps a fixed pool of up to KU_MAX_DEFORMATION_MODELS
// (28) DeformableObject models (mpaModels, 26496-byte console stride); parallel id/index
// tables; and a BitArray<28> (mModelsAdded) marking live slots. Every access here is BY
// MEMBER NAME -- the console-relative byte offsets in the asm (mpaModels @ +76032,
// miPlayerModelIndex @ +76040, ma8RaceCarToModelIndex @ +76044,
// ma8GlobalTrafficToModelIndex @ +76072, mDetachedPartManager @ +48112,
// mDetachedWheelManager @ +72928, the mModelsAdded words @ +18976) do NOT reproduce on
// the x64 host, so they are translated to the named members the frozen header pins. The
// asm's `26496 * index + mpaModels` is `&mpaModels[index]`; the inlined BitArray
// lowest-set-bit walk (`field*64 + 63 - clz64(field & -field)`) is the committed
// BitArray<28>::GetFirstNonZeroBit / GetNextNonZeroBit; the heavy `vcmpeqfp.`/`vmsum3fp`
// SIMD blocks are the per-lane self-compare NaN tripwires (RwMathVPU::IsValid),
// assert-path-only and non-gating -- reproduced as CGS_ASSERT(vpu::IsValid(...), "<msg>").
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                                       // CgsModule::IOBufferStack::CreateIOBuffer<T>
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                          // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"                     // CgsSceneManager::SceneManagerIO::PotentialContact (FLAG: newly homed)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"                          // CgsPhysics::PhysicsSimulationIO::OutContactSpy (CreateDetached* in-spy)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                                   // ContactSpy::PhysicalCarPartContact (+EBodyParts placeholder)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"         // DeformableObject (+ DeformationSensor via its includes)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"        // PenetrationSolver
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"         // PhysicalBodyPart
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"            // PhysicalWheel
#include "rw/math/vpu/vector3_operation.h"                                                        // rw::math::vpu::IsValid(Vector3)

namespace BrnPhysics
{
namespace Deformation
{
    // ------------------------------------------------------------------------------------------
    // Local file-scope constants + helpers (reconstructed from the asm; no fabricated rodata).
    // ------------------------------------------------------------------------------------------
    namespace
    {
        // Pool capacity tripwire bound (KU_MAX_DEFORMATION_MODELS == 28).
        const s32 KI_MAX_DEFORMATION_MODELS = 28;

        // BrnWorld::EEntityType owner-byte selectors the asm compares the volume-instance owner
        // bytes against (the full BrnWorld::EEntityType enum is not homed in-tree; these mirror the
        // values baked into the asm assert strings).
        const u32 KU_OWNER_WORLD                   = 0;  // BrnWorld::E_ENTITYTYPE_WORLD
        const u32 KU_OWNER_RACECAR                 = 1;  // BrnWorld::E_ENTITYTYPE_RACECAR
        const u32 KU_OWNER_TRAFFIC_VEHICLE         = 2;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;  // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
        const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART
        const u32 KU_OWNER_DETACHED_RACECAR_WHEEL  = 9;  // BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL
        const u32 KU_OWNER_DETACHED_TRAFFIC_WHEEL  = 10; // BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL

        // Detached-pool capacity bounds (the asm asserts muPolyTagA against these).
        const u32 KU_MAX_DETACHED_PARTS  = 0x32; // 50
        const u32 KU_MAX_DETACHED_WHEELS = 0x70; // 112

        // Race-car / total-traffic index bounds (FindModelIndexByGlobalEntityID asserts).
        const u32 KU_MAX_NUM_RACE_CARS   = 8;     // Vehicle::ku8MaxNumRaceCars
        const u32 KU_MAX_TOTAL_TRAFFIC   = 0x258; // BrnTraffic::KU_MAX_TOTAL_TRAFFIC (600)

        // The owner byte of a packed VolumeInstanceId == high byte of the embedded entity word ==
        // bits [56..63] of the 64-bit id (CgsVolumeInstanceId.h: owner at bits [24..31] of the high
        // dword). The X360 reads this as HIBYTE(*(contact + idOffset)).
        inline u32 GetVolumeInstanceOwner(const CgsSceneManager::VolumeInstanceId& lrId)
        {
            return static_cast<u32>(lrId.muId >> 56) & 0xFFu;
        }

        // RwMathVPU::IsValid(Vector3) -- per-lane self-compare NaN tripwire (the asm's vspltw +
        // vcmpeqfp. over lanes x/y/z). A lane is valid iff it equals itself (not NaN).
        inline bool IsValidVec3(const Vector3& lrV)
        {
            return rw::math::vpu::IsValid(lrV);
        }

        // RwMathVPU::IsValid(Matrix44Affine) -- the four affine rows' xyz lanes are each
        // self-compared (the penetration solve validates the model transform row-by-row). Non-gating.
        inline bool IsValidMatrix(const Matrix44Affine& lrM)
        {
            return IsValidVec3(lrM.xAxis) && IsValidVec3(lrM.yAxis)
                && IsValidVec3(lrM.zAxis) && IsValidVec3(lrM.wAxis);
        }
    }

    // ==========================================================================================
    // ReadPotentialVehicleWorldContact @ 0x82604590 -- ⭐ MOVED 2026-08-06 (big-five #2 wave) to
    // the MOUNTED slice TU BrnDeformationManager_ContactBridges.cpp: it is a direct callee of
    // PhysicsModule::BridgeContactsToSimulation and this TU is still unmounted. Body verbatim
    // there (one flagged tripwire upgrade, documented at the body). Fold back when this TU mounts.
    // ==========================================================================================
    // ==========================================================================================
    // SolvePenetration @ 0x82621B08
    //
    // Run the shared penetration solver over every live model this frame. Create the solver IO
    // buffer; for each live model: validate its transform, add it to the solver as an object, and
    // push its stored contacts into the solver (vs all other models). Add the articulated-joint
    // contacts, Solve(), then read the solved transforms back into each live model's rigid body.
    // The three perf-monitor regions (add / solve / read-back) bracket the three phases.
    // ==========================================================================================
    void DeformationManager::SolvePenetration(
        IOBufferStack* lpIOBufferStack,
        const PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        // Build a PenetrationSolver in the IO-buffer stack. FLAG (cross-TU type-namespace mismatch
        // to reconcile at consolidation): the frozen header types lpIOBufferStack as the local
        // placeholder BrnPhysics::Deformation::IOBufferStack*, whereas the buffer-stack API lives on
        // CgsModule::IOBufferStack (the DWARF type the asm calls CreateIOBuffer/DestroyIOBuffer on).
        // The placeholder must be unified with CgsModule::IOBufferStack; the calls below use that API.
        PenetrationSolver* lpSolver = nullptr;
        lpIOBufferStack->CreateIOBuffer<PenetrationSolver>(&lpSolver, "PenetrationSolver");

        // ---- PHASE 1: add each live model + its stored contacts to the solver. ----
        CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateAddContactsToPenSolverPerfMon);

        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            // NOTE (asm-faithful): the Phase-1 add loop body begins directly with the model
            // transform load + RwMathVPU::IsValid tripwire (asm @0x82621B08, BrnDeformationManager.cpp:947);
            // there is NO 'liObjectIndex < KI_MAX_PENETRATION_BODIES' bound assert at the top of this
            // loop. (That assert exists ONLY in the Phase-3 read-back loop below, BrnPenetrationSolver.h:221,
            // asm guard 'v114 >= 29'.) The 'invalid index : ' FireAssert seen in the Phase-1 asm belongs to
            // the inlined BitArray GetNextNonZeroBit walk (CgsBitArray.h:203), not the loop body.
            DeformableObject& lrModel = mpaModels[liModelIndex];

            Matrix44Affine lTransform;
            lrModel.GetTransform(lTransform);
            CGS_ASSERT(IsValidMatrix(lTransform), "RwMathVPU::IsValid( lTransform )");

            // The per-object weighting is the model's weight factor (the asm splats the w-lane of
            // *(model+6476)+4176 -- the model's rigid-body weight).
            lpSolver->AddObject(liModelIndex, lTransform, lrModel.GetWeightFactor());
            lrModel.AddContactsToPenetrationSolver(lpSolver, mpaModels,
                                                   KI_MAX_DEFORMATION_MODELS, liModelIndex);
        }

        // The articulated-joint contacts (traffic joints etc.) go in after the per-model contacts.
        AddArticulatedJointContacts(lpSolver, lpContacts);

        CgsDev::PerfMonCpu::StopMonitor(miPostPhysicsUpdateAddContactsToPenSolverPerfMon);

        // ---- PHASE 2: solve all accumulated penetrations. ----
        CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateSolvePenetrationPerfMon);
        lpSolver->Solve();
        CgsDev::PerfMonCpu::StopMonitor(miPostPhysicsUpdateSolvePenetrationPerfMon);

        // ---- PHASE 3: read the solved transforms back into each live model's rigid body. ----
        CgsDev::PerfMonCpu::StartMonitor(miPostPhysicsUpdateReadTransformsFromPenSolverPerfMon);
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            CGS_ASSERT(liModelIndex < KI_MAX_PENETRATION_BODIES,
                       "liObjectIndex < KI_MAX_PENETRATION_BODIES");

            // The solved transform for this object; the asm validity-tests its four rows before
            // storing it back into the model's rigid-body transform.
            const Matrix44Affine* lpSolvedTransform = lpSolver->GetUpdatedTransform(liModelIndex);
            CGS_ASSERT(IsValidMatrix(*lpSolvedTransform), "RwMathVPU::IsValid( *lpTransform )");

            mpaModels[liModelIndex].SetTransform(lpSolvedTransform);
        }
        CgsDev::PerfMonCpu::StopMonitor(miPostPhysicsUpdateReadTransformsFromPenSolverPerfMon);

        // Tear the solver IO buffer back down.
        lpIOBufferStack->DestroyIOBuffer<PenetrationSolver>(&lpSolver);
    }

    // ==========================================================================================
    // UpdateTriangleCache @ 0x826230E8
    //
    // Update the scene triangle cache for every detached part + detached wheel. Asserts the
    // per-update scene input buffer is non-null, then forwards its scene-update interface to both
    // detached managers' UpdateTriangleCache (the asm fetches the interface once per manager).
    // ==========================================================================================
    void DeformationManager::UpdateTriangleCache(InputBuffer_Update* lpSimInputUpdate)
    {
        CGS_ASSERT(lpSimInputUpdate != nullptr, "lpSceneInputBuffer_Update != NULL");

        // FLAG (cross-TU type-namespace mismatch to reconcile at consolidation): the frozen header
        // types this arg as the local placeholder BrnPhysics::Deformation::InputBuffer_Update*,
        // whereas the DWARF type is CgsSceneManager::SceneManagerIO::InputBuffer_Update, whose
        // GetSceneUpdateInterface() returns the InSceneUpdateInterface the detached managers'
        // UpdateTriangleCache take. The placeholder must be unified with that type.
        mDetachedPartManager.UpdateTriangleCache(lpSimInputUpdate->GetSceneUpdateInterface());
        mDetachedWheelManager.UpdateTriangleCache(lpSimInputUpdate->GetSceneUpdateInterface());
    }

    // ==========================================================================================
    // FixupBodyPartVehicleContact @0x825A0B88 / FixupWheelVehicleContact @0x825A0D98 /
    // CreateDetachedWheelContactEvent @0x825B95B0 / CreateDetachedPartContactEvent @0x825DD628
    // MOVED 2026-08-06 (bridge de-facade wave) to the mounted slice TU
    // BrnDeformationManager_ContactFixups.cpp: the de-facaded contact-spy bridge
    // (PhysicsModule::ProcessContactSpy / StoreContact) links against exactly these four, while
    // the REST of this TU (ReadPotentialVehicleWorldContact / SolvePenetration /
    // UpdateTriangleCache / the spatial queries) still carries ~19 unresolved externals of its
    // own (DeformableObject / PenetrationSolver / DeformationSensor / detached-manager
    // update methods). Bodies are verbatim there; fold back when this TU mounts.
    // ==========================================================================================

    // ==========================================================================================
    // GetSweptSpheresForCar @ 0x825C22D0
    //
    // The continuous (swept) collision spheres for the car with entity id lEntityId. Looks up the
    // model slot, asserts it is live, and forwards to DeformableObject::GetSweptSpheres.
    // ==========================================================================================
    s32 DeformationManager::GetSweptSpheresForCar(EntityId lEntityId,
                                                  const CgsGeometric::SweptSphere** lppSpheresOut)
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        CGS_ASSERT(liIndex != -1, "liIndex != -1");
        return mpaModels[liIndex].GetSweptSpheres(lppSpheresOut);
    }

    // ==========================================================================================
    // IsUsingSweptSpheres @ 0x825C2338
    //
    // Whether the car with entity id lEntityId currently runs continuous (swept-sphere) collision.
    // ==========================================================================================
    bool DeformationManager::IsUsingSweptSpheres(EntityId lEntityId)
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        CGS_ASSERT(liIndex != -1, "liIndex != -1");
        return mpaModels[liIndex].IsUsingSweptSpheres();
    }

    // ==========================================================================================
    // GetDeformedBBox @ 0x825E87A0
    //
    // The deformed bounding box for the car with entity id lEntityId. const. Looks up the model
    // slot, asserts it is live, and forwards to DeformableObject::GetBoundingBox. (FindModelIndexBy-
    // EntityID is const; mpaModels is a const pointer in a const method but its pointees stay
    // mutable, so GetBoundingBox -- a non-const model method -- is reachable, matching the X360.)
    // ==========================================================================================
    void DeformationManager::GetDeformedBBox(EntityId lEntityId, CgsGeometric::Box* lpBoxOut) const
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        CGS_ASSERT(liIndex != -1, "liIndex != -1");
        mpaModels[liIndex].GetBoundingBox(lpBoxOut);
    }

    // ==========================================================================================
    // FindModelIndexByGlobalEntityID @ 0x825B45B0 -- ⭐ MOVED 2026-08-06 (big-five #2 wave) to the
    // MOUNTED slice TU BrnDeformationManager_ContactBridges.cpp (callee of the moved
    // ReadPotentialVehicleWorldContact). Verbatim. Fold back when this TU mounts.
    // ==========================================================================================
    // ==========================================================================================
    // GetPlayerCarModel @ 0x825B44F0 -- ⭐ MOVED 2026-08-14 (deformation-mount wave) to the now-
    // MOUNTED home TU BrnDeformationManager.cpp (it was the one symbol the mount needed from this
    // still-unmounted slice). Verbatim. Fold back when this TU mounts.
    // ==========================================================================================
}
}
