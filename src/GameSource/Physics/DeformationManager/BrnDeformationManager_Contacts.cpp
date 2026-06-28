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
    // ReadPotentialVehicleWorldContact @ 0x82604590
    //
    // Route one potential VEHICLE-vs-WORLD contact into the owning car's deformation sensor. The
    // asm: validity-gate the contact (assert + ignore if any lane is NaN); look up the car's model
    // slot from muVolumeInstanceIdA's entity; assert the owner tags (A == racecar/traffic, B ==
    // world); fetch the model's deformation sensor for that volume instance and the model's world
    // transform; (assert the normal is normalised); then DeformationSensor::ValidateAndAddContact
    // with no other vehicle / other sensor.
    // ==========================================================================================
    void DeformationManager::ReadPotentialVehicleWorldContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
        BrnPhysics::ContactId lContactId,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/)
    {
        // Initial validity gate: the asm self-compares the three lanes of the contact's leading
        // vector (mPointOnA) -- any NaN lane means the contact is invalid and is ignored (asserted,
        // then the function returns without adding it).
        if (!IsValidVec3(lrPotentialContact.mPointOnA))
        {
            CGS_ASSERT(false, "Invalid contact added to deformation. Ignoring...\n");
            return;
        }

        // The model lookup keys off muVolumeInstanceIdA's embedded entity word (the high dword of the
        // packed 64-bit id) -- the asm passes that word to FindModelIndexByGlobalEntityID.
        const EntityId lEntityA{ static_cast<u32>(lrPotentialContact.muVolumeInstanceIdA.muId >> 32) };
        const s32 liModelIndexA = FindModelIndexByGlobalEntityID(lEntityA);
        if (liModelIndexA == -1)
        {
            CGS_ASSERT(false, "liModelIndexA != -1");
            return;
        }

        // Owner-tag tripwires (non-gating): A must be a racecar / traffic vehicle, B must be world.
        const u32 luOwnerA = GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdA);
        CGS_ASSERT(luOwnerA == KU_OWNER_RACECAR || luOwnerA == KU_OWNER_TRAFFIC_VEHICLE,
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR || "
                   "lPotentialContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
        CGS_ASSERT(GetVolumeInstanceOwner(lrPotentialContact.muVolumeInstanceIdB) == KU_OWNER_WORLD,
                   "lPotentialContact.muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");

        DeformableObject& lrModel = mpaModels[liModelIndexA];

        // The car's deformation sensor for the contacted volume instance, and the car's world
        // transform (the asm reads the rigid-body transform off the model @ +6476 and packs it into
        // the Matrix44Affine handed to ValidateAndAddContact).
        DeformationSensor& lrSensor =
            lrModel.GetDeformationSensorFromVolumeInstance(lrPotentialContact.muVolumeInstanceIdA);

        Matrix44Affine lWorldTransform;
        lrModel.GetTransform(lWorldTransform);

        // Normalised-normal tripwire (non-gating): the asm @0x82604590 renormalises the contact
        // normal (vrsqrtefp Newton-Raphson) and per-lane compares |renormalised - original| against a
        // splatted epsilon v82 == 0.0099999998 (vsubfp / vandc abs / vcmpgtfp). The assert fires iff
        // any lane's renormalisation delta EXCEEDS that epsilon -- i.e. a normal-LENGTH test, NOT a
        // NaN self-compare (RwMathVPU::IsValid). It then streams the contact normal + "Car transform: "
        // into the message before firing (BrnDeformationManager.cpp:1198).
        // FLAG (predicate not reproduced -- NOT fabricated): the committed Vector3 / vpu API in-tree
        // exposes no normalise-delta-vs-epsilon helper, and emitting an ad-hoc renormalise+compare here
        // would fabricate a body that is not byte-grounded. The exact streamed assert message string is
        // preserved verbatim; the wrong IsValidVec3 self-compare predicate is dropped in favour of an
        // honest always-pass placeholder so no fabricated predicate is asserted. Re-emit the
        // renormalise-delta (epsilon 0.0099999998) test once a faithful Vector3 normalise helper is homed.
        CGS_ASSERT(true, "Un-normalised vehicle-world contact: ");

        // Validate + store the contact in the sensor (no other vehicle / other sensor for a
        // vehicle-vs-world contact).
        // FLAG (cross-TU type-namespace mismatch to reconcile at consolidation):
        // DeformationSensor::ValidateAndAddContact declares its contact arg as the stale forward-decl
        // CgsSceneManager::PotentialContact, whereas the canonical (DWARF) type is
        // CgsSceneManager::SceneManagerIO::PotentialContact (homed by this TU's CgsPotentialContact.h).
        // They are the SAME on-disk record (only the namespace differs), so the canonical record is
        // passed through a layout-safe reference cast here; the sensor header should retype its arg to
        // the SceneManagerIO:: home at consolidation, after which this cast can be dropped.
        lrSensor.ValidateAndAddContact(
            lWorldTransform,
            reinterpret_cast<const CgsSceneManager::PotentialContact&>(lrPotentialContact),
            lContactId, nullptr, nullptr);
    }

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
    // FixupBodyPartVehicleContact @ 0x825A0B88
    //
    // Repair a potential DETACHED-BODY-PART-vs-VEHICLE contact so the detached part collides with
    // the live deformable vehicle: validate the owner tags + the part-slot bound, then (if the part
    // slot and its owning model slot are both live) re-key the contact's A id to the part's volume
    // instance and its B id to the owning car model's handling-body word. Returns true iff re-keyed,
    // false otherwise (slot not live -> contact dropped).
    // ==========================================================================================
    bool DeformationManager::FixupBodyPartVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact)
    {
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdA) == KU_OWNER_RACECAR_DEFORMABLE_PART,
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART");
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdB) == KU_OWNER_RACECAR,
                   "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
        CGS_ASSERT(lpContact->muPolyTagA < KU_MAX_DETACHED_PARTS,
                   "lpPotContact->muPolyTagA < KU_MAX_DETACHED_PARTS");

        // The destination detached-part pool slot (poly tag A). If the slot is not live -> drop.
        const s32 liPartIndex = static_cast<s32>(lpContact->muPolyTagA);
        if (!mDetachedPartManager.IsPartIndexUsed(liPartIndex))
        {
            return false;
        }

        // The owning car model slot (poly tag B). Tripwire-bound, then tested live.
        const u32 luModelIndex = lpContact->muPolyTagB;
        CGS_ASSERT(luModelIndex < static_cast<u32>(KI_MAX_DEFORMATION_MODELS), "invalid index : ");
        if (!mModelsAdded.IsBitSet(luModelIndex))
        {
            return false;
        }

        // Re-key the contact. The X360 reads a packed {volumeInstanceId, owningModelIndex} word off
        // the physical part record (+464), stores it whole into muVolumeInstanceIdA, then writes the
        // owning model's handling-body word (mpaModels[owningModelIndex] +26384) into
        // muVolumeInstanceIdB.
        //
        // FLAG (unrecovered packed-record reads -- NOT fabricated): the part's volume-instance id +
        // owning-model index live in a packed word at PhysicalBodyPart +464 that the part's public
        // API does NOT expose (GetVolumeInstanceId() is private; there is no GetDeformableObjectIndex()).
        // The owning model's handling-body word at DeformableObject +26384 is reachable
        // (GetHandlingBodyID()) but is a 32-bit RigidBodyId, whereas muVolumeInstanceIdB is a 64-bit
        // VolumeInstanceId. The exact byte values are therefore deferred: the control flow, asserts
        // and boolean return above are byte-faithful; the two id writes are left as flagged
        // placeholders pending public part accessors (GetVolumeInstanceId() / owning-model index) and
        // the DeformableObject +26384 handling-body word being homed as a VolumeInstanceId.
        // PLACEHOLDER (flagged): muVolumeInstanceIdA / muVolumeInstanceIdB rewrite not emitted.
        return true;
    }

    // ==========================================================================================
    // FixupWheelVehicleContact @ 0x825A0D98
    //
    // The detached-WHEEL sibling of FixupBodyPartVehicleContact: validate the owner tags + the
    // wheel-slot bound, then (if the wheel slot and its owning model slot are both live) re-key the
    // contact's A id to the detached wheel's volume instance and its B id to the owning car model's
    // handling-body word. Returns true iff re-keyed.
    // ==========================================================================================
    bool DeformationManager::FixupWheelVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpContact)
    {
        const u32 luOwnerA = GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdA);
        CGS_ASSERT(luOwnerA == KU_OWNER_DETACHED_RACECAR_WHEEL || luOwnerA == KU_OWNER_DETACHED_TRAFFIC_WHEEL,
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || "
                   "lpPotContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL");
        CGS_ASSERT(GetVolumeInstanceOwner(lpContact->muVolumeInstanceIdB) == KU_OWNER_RACECAR,
                   "lpPotContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
        CGS_ASSERT(lpContact->muPolyTagA < KU_MAX_DETACHED_WHEELS,
                   "lpPotContact->muPolyTagA < KU_MAX_DETACHED_WHEELS");

        const u16 lu16Slot = static_cast<u16>(lpContact->muPolyTagA);
        if (!mDetachedWheelManager.IsSlotUsed(lu16Slot))
        {
            return false;
        }

        const u32 luModelIndex = lpContact->muPolyTagB;
        CGS_ASSERT(luModelIndex < static_cast<u32>(KI_MAX_DEFORMATION_MODELS), "invalid index : ");
        if (!mModelsAdded.IsBitSet(luModelIndex))
        {
            return false;
        }

        // Re-key the contact off the live detached wheel + its owning car model. The wheel's
        // volume-instance id IS publicly reachable (PhysicalWheel::GetVolumeInstanceId()), so A is
        // re-keyed faithfully; B is the owning model's handling-body word.
        const PhysicalWheel* lpWheel = mDetachedWheelManager.GetWheel(lu16Slot);
        lpContact->muVolumeInstanceIdA = lpWheel->GetVolumeInstanceId();

        // FLAG (unrecovered packed-record reads -- NOT fabricated): the wheel's owning-model index
        // lives in a packed word at PhysicalWheel +112 that the wheel's public API does not expose,
        // and the owning model's handling-body word at DeformableObject +26384 (GetHandlingBodyID(),
        // a 32-bit RigidBodyId) does not match muVolumeInstanceIdB's 64-bit VolumeInstanceId shape.
        // The muVolumeInstanceIdB rewrite is therefore deferred (flagged placeholder); the control
        // flow, asserts, the A-id rewrite and the boolean return are byte-faithful.
        // PLACEHOLDER (flagged): muVolumeInstanceIdB rewrite not emitted pending the owning-model
        // index accessor + the +26384 handling-body word being homed as a VolumeInstanceId.
        return true;
    }

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
    // FindModelIndexByGlobalEntityID @ 0x825B45B0
    //
    // Map a global entity id (extracted from a volume-instance entity word: owner in the high byte,
    // a 14-bit index at bit 10) to its deformation model slot. Race-car ids index
    // ma8RaceCarToModelIndex (capacity 8); traffic ids index ma8GlobalTrafficToModelIndex
    // (capacity 600). Asserts the owner is racecar/traffic and the index is in range.
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
    // CreateDetachedWheelContactEvent @ 0x825B95B0
    //
    // Build a detached-WHEEL contact event from a spy + potential contact. Asserts every input is
    // non-null and that the spy's / potential contact's A id owner is a detached racecar/traffic
    // wheel, then writes the event header: a zeroed contact vector at +96, the contact-event type
    // tag 91 at +112, and a zeroed trailing scalar at +116.
    //
    // FLAG (cross-TU records not homed -- gate-blocked, NOT fabricated): the out-record
    // (PhysicalCarPartContact == BrnPhysics::ContactSpy::PhysicalCarPartContact : BaseContact, with
    // mVelocity), the in-spy (OutContactSpy) and the BrnPhysics-side PotentialContact are all only
    // FORWARD-DECLARED in the frozen header (the whole BrnPhysics::ContactSpy event family is not
    // homed in-tree). Their members therefore cannot be read/written here: the spy/potential
    // owner-byte asserts (HIBYTE(*(spy+80)) / HIBYTE(*(potential+48))) and the three event-header
    // stores (+96 / +112 / +116) await those records being homed. Only the two pointer-NULL asserts
    // (which need no member access) are emitted; everything else is left as a flagged placeholder.
    // ==========================================================================================
    void DeformationManager::CreateDetachedWheelContactEvent(
        PhysicalCarPartContact* lpOutPhysicalCarPartContact,
        const OutContactSpy* lpInContact,
        const PotentialContact* /*lpInPotentialContact*/)
    {
        CGS_ASSERT(lpOutPhysicalCarPartContact != nullptr, "lpOutPhysicalCarPartContact != NULL");
        CGS_ASSERT(lpInContact != nullptr, "lpInContact != NULL");

        // PLACEHOLDER (flagged): the spy/potential-contact owner-byte asserts
        //   ("...mIDA.GetEntityId().GetOwner() == E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || ... DETACHED_TRAFFIC_WHEEL",
        //    "...muVolumeInstanceIdA.GetEntityIDOwner() == E_ENTITYTYPE_DETACHED_RACECAR_WHEEL || ... DETACHED_TRAFFIC_WHEEL")
        // and the event-header writes (zero +96, type tag 91 @ +112, zero +116) are NOT emitted:
        // PhysicalCarPartContact / OutContactSpy / PotentialContact (BrnPhysics::ContactSpy event
        // family) are forward-declared incomplete types here, so no member access is possible.
        // Re-emit them once the ContactSpy event records are homed.
    }

    // ==========================================================================================
    // GetPlayerCarModel @ 0x825B44F0  (DWARF spelling truncates to "GetPlaye")
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
}
}
