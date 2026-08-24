#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // VehiclePhysics::GetAllWheelsHaveTraction (the +0x135B byte, by name)
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Abs, ...}

#include <cmath>                                            // std::fabs

// =====================================================================================================
// BrnPhysics::Deformation::DeformableObject -- CONTACT-GENERATION group.
//
// Four functions that feed the penetration / collision pipeline:
//
//   * GetVehicleWorldRestitution        @0x825E0C78  -- combined restitution for a vehicle-vs-world
//                                                       contact (consulted by ApplyCarWorldImpulse).
//   * AddContactsToPenetrationSolver    @0x82609F98  -- push every deformation sensor's stored contacts
//                                                       into the shared penetration solver.
//   * DoBodyPartWorldContactGeneration  @0x82609278  -- build world contacts for the detached / hinged
//                                                       body parts (primitive-vs-triangle collision).
//   * DoDetachedWheelWorldContactGeneration @0x82609878 -- the same, for detached wheels.
//
// Member-offset map recovered from the X360 ARTIST.XEX asm (BrnDeformableObject.cpp), pinned BY NAME
// onto the frozen header members:
//   this+6368  -> mpDeformationSpec      (spec+1618 == mu8NumDeformationSensors; GetNumSensors()-4)
//   this+6476  -> mVehicleBody's attached VehiclePhysics  (mVehicleBody.GetVehiclePhysics())
//   this+6480  -> maDeformationSensors[] (stride 432 == sizeof(DeformationSensor))
//   this+26180 -> maPartStates[50]       (per-part EPartState)
//   this+26236 -> mau8PhysicalBodyPartPoolIndex[50]
//   this+26286 -> mi16NumPhysicalParts   (GetNumPhysicalParts())
//   this+26288 -> mi16NumHingedParts     (GetNumHingedParts())
//   this+26384 -> mGlobalEntityId        (GetGlobalEntityId())
//
// MODELLED-VS-ASM. The X360 build of the two world-contact-generation methods is dense VMX128 inline
// assembly whose Hex-Rays export is flagged "local variable allocation has failed" -- and almost every
// callee in their inner contact-emission block is a type the frozen header can only forward-declare by
// pointer (DetachedPartManager / DetachedWheelManager / ContactGenList / CgsPhysics::CollisionGenerator
// / InTriangleCacheInterface) or an unresolved external (PrimitivePairListBuilder, BaseCollisionGenerator,
// TriangleCacheInterface::GetCache/GetNumCachedTriangleBatches, TriangleList, ContactGenList::AddEntry,
// sub_82814570 / sub_82814678 / sub_825E8308). Those opaque types carry NO accessors, so the inner
// per-part / per-wheel collision-emission cannot be reconstructed BY NAME without fabricating callee
// signatures and member offsets. Per the project no-fabrication rule, the two Do*WorldContactGeneration
// bodies below reproduce the RECOVERABLE observable skeleton -- the outer loop bounds (via the declared
// const part/sensor-count accessors), the per-part state branch, the exact FireAssert tripwire messages
// (as CGS_ASSERT), and the global world-contact-mode branch flag -- and FLAG the dense VMX
// contact-emission block as deferred (it stays inert until those opaque types/externals are homed).
// GetVehicleWorldRestitution and AddContactsToPenetrationSolver ARE fully reconstructed BY NAME.
// =====================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    // -------------------------------------------------------------------------------------------------
    // Unrecovered .rodata for GetVehicleWorldRestitution. The X360 build loads these from constant
    // pools with no resolvable symbol; per the no-fabrication rule they are honest zero placeholders.
    //   * unk_82FB7FF0 -- the normal-Y threshold the abs(normal.y) is compared against.
    //   * unk_82FB8260 -- the restitution value returned when abs(normal.y) >= that threshold (a broadcast vec4).
    // The compare/select SHAPE is exact; the numeric output stays inert until the rodata is recovered.
    // -------------------------------------------------------------------------------------------------
    // ⭐ RECOVERED 2026-08-03 from the static-init splats (they read zero in the image because they
    // are .data filled at init, not because they have no value). ⚠️ The zero threshold was NOT inert:
    // abs(normal.y) >= 0 is true for every contact, so the restitution branch was taken always --
    // and then returned 0 restitution. A 0.5 threshold is the usual "is this surface floor-like".
    static const f32     KF_WORLD_RESTITUTION_NORMAL_Y_THRESHOLD = 0.5f;   // unk_82FB7FF0 @82C5D3E0 <- flt_82001DA0
    static const VecFloat KVF_WORLD_RESTITUTION_VALUE = { 1.10000002f, 1.10000002f, 1.10000002f, 1.10000002f };  // unk_82FB8260 @82C5D3B4 <- flt_82004A1C

    // The global "spy world-contact mode" selector byte the two Do*WorldContactGeneration methods
    // branch on (asm: DoBodyPart `*(v77 - 23737)`, DoDetachedWheel `byte_82F2A347`): when set, the
    // generator records the contacts through AddPrimitiveListWithTriangleListT (the spy path);
    // when clear, through CollidePrimitiveListAgainstTriangles.
    // ⭐ RECOVERED 2026-08-24 (deform-land wave, P9; physics11 audit cluster C item 6): the
    // STATIC IMAGE VALUE of byte_82F2A347 is 0x01 -- the spy world-contact path IS the console
    // boot default, not a debug opt-in. The old FLAGGED-0 ran the other arm on every contact.
    static const u8 KU_USE_SPY_WORLD_CONTACT_PATH = 1;   // byte_82F2A347 image value == 0x01

    // =================================================================================================
    // GetVehicleWorldRestitution @0x825E0C78
    //
    // Combined restitution for a vehicle-vs-WORLD contact. The asm consults the virtual showtime-style
    // predicate on this car's physics body (mVehicleBody.GetVehiclePhysics(), vtable slot +16 -- the
    // same predicate ApplyCarCarImpulse gates its bounce shaping on). If the predicate is FALSE the
    // restitution is zero. If TRUE it takes |lContact.mNormal.y| (vandc sign-clear of the splatted
    // word-1 lane), and returns ZERO when that abs-Y is BELOW the rodata threshold, else the rodata
    // restitution vector (asm: vcmpgefp threshold then vnot+vsel -> select(value,0, absY < thresh)
    // = ZERO where absY < thresh).
    // Body hint (BrnDeformableObject.cpp:3371): VehicleRigidBody::GetVehiclePhysics, Abs<VectorAxisY>,
    // GetVecFloat_Zero, CompLessThan, Select. The VMX is de-SIMD'd to scalar per project convention.
    // =================================================================================================
    VecFloat DeformableObject::GetVehicleWorldRestitution(const StoredImpulseContact& lContact) const
    {
        // The showtime/eligible predicate on this car's physics body (vtable slot +16). Modelled the
        // same way ApplyCarCarImpulse models the +16 virtual: through the race-car downcast guard.
        const Vehicle::RaceCarPhysics* lpRaceCarPhysics = AsRaceCarPhysics();
        const bool lbPredicate = (lpRaceCarPhysics != nullptr) &&
                                 lpRaceCarPhysics->IsPlayerVehicleActuallyInShowtime();

        if (!lbPredicate)
            return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };

        // abs(normal.y) (asm: vspltw word-1 then vandc the sign bit) compared against the rodata
        // threshold. asm @0x825E0C78: vcmpgefp(absY, threshold) then vnot -> mask = (absY < threshold),
        // then vsel(VALUE, ZERO, mask) = mask ? ZERO : VALUE. So the routine returns ZERO when absY is
        // BELOW the threshold and the rodata restitution VALUE when absY >= threshold (Select).
        const f32 lfAbsNormalY = std::fabs(lContact.mNormal.y);
        if (lfAbsNormalY < KF_WORLD_RESTITUTION_NORMAL_Y_THRESHOLD)
            return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };

        return KVF_WORLD_RESTITUTION_VALUE;
    }

    // =================================================================================================
    // AddContactsToPenetrationSolver @0x82609F98
    //
    // Push every deformation sensor's stored contacts into the shared penetration solver. The asm:
    //   v11 = *(*(this+6476) + 4955);              // +0x135B == VehiclePhysics::mbAllWheelsHaveTraction
    //                                              // (NOT a "world" flag -- see the body's note)
    //   if ( *(*(this+6368) + 1618) )              // mpDeformationSpec->mu8NumDeformationSensors > 0
    //     do
    //       DeformationSensor::AddContactsToPenetrationSolver(sensor, lpSolver, lpDefObjBase,
    //           liWorldObjectIndex, liObjectIndex, v11);
    //       sensor += 432; ++i;
    //     while ( i < *(*(this+6368) + 1618) );
    // The loop count is the spec's BARE sensor count `*(mpDeformationSpec + 1618)` ==
    // mu8NumDeformationSensors (asm @0x82609F98 bounds the do/while on this exact field, NO +4). The
    // asm-attested identity GetNumSensors() == *(spec+1618) + 4 (dossier `return *(*(a1+6368)+1618)+4`)
    // means the bare count is GetNumSensors() - 4; mu8NumDeformationSensors itself is private on
    // StreamedDeformationSpec with no public accessor, so the bare count is spelled GetNumSensors() - 4
    // here -- matching the committed siblings (Update.cpp UpdateSensorDisplacements etc.). const.
    // =================================================================================================
    void DeformableObject::AddContactsToPenetrationSolver(PenetrationSolver* lpSolver,
                                                          DeformableObject* lpDefObjBase,
                                                          s32 liWorldObjectIndex, s32 liObjectIndex) const
    {
        // The byte the asm reads at physics-body +4955 is +0x135B == VehiclePhysics::
        // mbAllWheelsHaveTraction (VehiclePhysics.h:1584, accessor :1624) -- NOT a "world" flag: the
        // sensor uses it as one half of the vehicle-arm normal-flattening gate, and the DWARF spells
        // the parameter it becomes `lbVehicleWheelsAllHaveTraction` (BrnPhysicsUnity2.cpp:6563).
        // The parameter names here are the DWARF's too (BrnDeformableObject.cpp:1877).
        const bool lbVehicleWheelsAllHaveTraction =
            mVehicleBody.GetVehiclePhysics()->GetAllWheelsHaveTraction();

        // The bare deformation-sensor count (asm: *(mpDeformationSpec + 1618)). == GetNumSensors() - 4.
        // FLAG: the frozen header declares GetNumSensors() non-const, so it is reached through a const_cast
        // here (this is a pure read in the asm); drop the cast if GetNumSensors() is later made const.
        const s32 liNumSensors = const_cast<DeformableObject*>(this)->GetNumSensors() - 4;
        if (liNumSensors <= 0)
            return;

        const DeformationSensor* lpSensor = &maDeformationSensors[0];
        s32 liIndex = 0;
        do
        {
            lpSensor->AddContactsToPenetrationSolver(lpSolver, lpDefObjBase, liWorldObjectIndex,
                                                     liObjectIndex, lbVehicleWheelsAllHaveTraction);
            ++liIndex;
            ++lpSensor;
        }
        while (liIndex < liNumSensors);
    }

    // =================================================================================================
    // DoBodyPartWorldContactGeneration @0x82609278
    //
    // Generate world contacts for this car's detached / hinged body parts, against the cached collision
    // triangles, feeding them into the contact generator + ContactGenList. The recoverable skeleton:
    //
    //   * Build this car's scene-entity id from mGlobalEntityId (asm: EntityId::Set + the +26384 read,
    //     owner-tag 6 RACECAR_DEFORMABLE_PART / 7 TRAFFIC_DEFORMABLE_PART by the +1 of the high byte).
    //   * Construct the primitive-pair list builder; Prepare it for the hinged-part block when there is
    //     at least one hinged part (mi16NumHingedParts > 0).
    //   * For each of mi16NumPhysicalParts physical parts: fetch the part from the pool by its
    //     mau8PhysicalBodyPartPoolIndex[i]; skip parts already in the gen list; assert the part state is
    //     consistent (HINGED parts must still be joined to the vehicle; DETACHED parts must not be);
    //     pull the cached triangle batches and, when present, emit primitive-vs-triangle contacts (the
    //     0.05 / 0.15 / 0.5 contact-radius band the VMX builds) through the generator.
    //   * Then the hinged-part block: pull its triangle batches and emit them likewise.
    //   * The generator entry point is chosen by the global spy-mode flag.
    //
    // MODELLED-VS-ASM (see file header): the dense VMX contact-emission block calls only opaque
    // forward-declared types (DetachedPartManager / ContactGenList / CollisionGenerator /
    // InTriangleCacheInterface) and unresolved externals, with an allocation-failed disassembly. It
    // cannot be reconstructed BY NAME without fabrication, so it is FLAGGED-DEFERRED below: the
    // recoverable outer flow + the exact assert tripwires are reproduced; the contact emission itself
    // is left as a documented gap (no fabricated callee signatures / offsets).
    // =================================================================================================
    void DeformableObject::DoBodyPartWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                                            ContactGenList* lpGenList,
                                                            CgsPhysics::CollisionGenerator* lpGen,
                                                            CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                            const DetachedPartManager* lpPartMgr,
                                                            u32 luFlags,
                                                            CgsMemory::LinearMalloc* lpAlloc) const
    {
        (void)lpTriCache; (void)lpGenList; (void)lpGen; (void)lpProducer;
        (void)lpPartMgr;  (void)luFlags;   (void)lpAlloc;
        (void)KU_USE_SPY_WORLD_CONTACT_PATH;

        // Recoverable loop bounds (asm: v32 = mi16NumPhysicalParts, v33 = mi16NumHingedParts).
        const s16 li16NumPhysicalParts = GetNumPhysicalParts();
        const s16 li16NumHingedParts   = GetNumHingedParts();

        // asm: the whole routine is gated on `if ( v32 )` -- nothing to generate with no physical parts.
        if (li16NumPhysicalParts == 0)
            return;

        // --- per physical-part loop (asm: do { ... } while ( i < mi16NumPhysicalParts ) ) -------------
        for (s32 liPart = 0; liPart < li16NumPhysicalParts; ++liPart)
        {
            // asm fetches the part from the pool by its pool-slot index and skips it when it is already
            // present in the gen list (`if ( !*(part + 486) )`). It then branches on the part's stored
            // EPartState (maPartStates[part.GetRigidBodyId().GetPartIndex()]):
            //   E_PART_STATE_HINGED   (3) -> the part MUST still be joined to the vehicle, and its pool
            //                                index MUST be < 50; emit at the 0.05 contact radius.
            //   E_PART_STATE_DETATCHED(4) -> the part MUST NOT be joined to the vehicle; emit at the
            //                                clamped 0.15/0.5 radius computed from the triangle batch.
            // The two asserts below are the X360 non-gating FireAssert tripwires, reproduced verbatim
            // (execution continues past a failed assert in the asm). They are evaluated for the recovered
            // state branch; the part fetch + state read live in the FLAGGED-DEFERRED emission block.
            //
            // FLAG (deferred contact emission): fetching the part (PhysicalBodyPartPool::GetPart), reading
            // its state / joined flag / pool index, pulling the cached triangle batches
            // (TriangleCacheInterface::GetNumCachedTriangleBatches / GetCache), building the
            // primitive-pair list (PrimitivePairListBuilder) and recording the contacts
            // (BaseCollisionGenerator::CollidePrimitiveListAgainstTriangles /
            // AddPrimitiveListWithTriangleListT + ContactGenList::AddEntry) all require opaque
            // forward-declared types / unresolved externals with no accessors -- they are NOT
            // reconstructed here (no fabrication). The assert tripwire messages are kept for parity,
            // grouped by the part's recovered EPartState branch (the part fetch / state read live in the
            // FLAGGED-DEFERRED emission block, so the branch condition is modelled as the inert default).
            //
            // HINGED part (asm: v43 == 3): the part MUST still be joined to the vehicle, and its pool
            // index MUST be < 0x32 (asm lines 3562 and 3580/3587 @ BrnDeformableObject.cpp:3572 / :3573).
            CGS_ASSERT(true /* !*(part+484) -> lpCurrentPart->IsJoinedToVehicle() (HINGED) */,
                       "lpCurrentPart->IsJoinedToVehicle()");
            CGS_ASSERT(true /* *(part+468) < 0x32 (HINGED pool index in range) */,
                       "Bad Part pool index: ");

            // DETACHED part (asm: else, v43 == 4): state MUST read E_PART_STATE_DETATCHED, the part MUST
            // NOT be joined, and its pool index MUST be < 0x32 (asm lines 3598, 3608 and 3675/3682
            // @ BrnDeformableObject.cpp:3586 / :3587 / :3612).
            CGS_ASSERT(true /* maPartStates[...] == E_PART_STATE_DETATCHED */,
                       "maPartStates[lpCurrentPart->GetRigidBodyId().GetEntityId().GetPartIndex()] == E_PART_STATE_DETATCHED");
            CGS_ASSERT(true /* *(part+484) == 0 -> !lpCurrentPart->IsJoinedToVehicle() (DETACHED) */,
                       "!lpCurrentPart->IsJoinedToVehicle()");
            CGS_ASSERT(true /* *(part+468) < 0x32 (DETACHED pool index in range) */,
                       "Bad Part pool index: ");
        }

        // --- hinged-part block (asm: `if ( v84 )` -> at least one hinged part) ------------------------
        // Pull the hinged parts' cached triangle batches off the tri-cache (keyed by the entity id built
        // above) and emit their primitive-vs-triangle contacts through the generator, then add the
        // ContactGenList entry. FLAGGED-DEFERRED for the same opaque-callee reason as the per-part block.
        if (li16NumHingedParts > 0)
        {
            // asm (if(!v31) @ BrnDeformableObject.cpp:3675): the hinged result-list tag must be non-zero.
            // This tripwire lives in the hinged block, NOT the per-physical-part loop (the v31/luResultListTag
            // is the prepared hinged-part list tag). Non-gating; kept for parity while the emission is deferred.
            CGS_ASSERT(true /* luResultListTag > 0 */, "luResultListTag > 0");

            // (deferred contact emission -- see FLAG above)
        }
    }

    // =================================================================================================
    // DoDetachedWheelWorldContactGeneration @0x82609878
    //
    // Generate world contacts for this car's detached wheels. The recoverable skeleton:
    //
    //   * Build this car's scene-entity id (asm: EntityId::Set + the +26384 mGlobalEntityId read) and
    //     construct the primitive-pair list builder.
    //   * For each of the four wheels: if the wheel's physics-body state byte == 2 (detached), fetch the
    //     detached wheel from the wheel manager (sub_825E8308(lpWheelMgr, mGlobalEntityId, wheelIndex));
    //     when found and not already listed (`!*(wheel + 128)`), pull its cached triangle batches
    //     (pool index = wheel.poolIndex + 123) and, when present, emit primitive-vs-triangle contacts at
    //     the 0.5 contact radius, asserting the wheel pool index is < 0x70 (non-gating tripwire), then
    //     add the ContactGenList entry. The generator entry point is chosen by the global spy-mode flag.
    //
    // The wheel loop is a fixed 4 iterations (asm: v15 from 0, stride 224, while v15 < 896 -> 4 wheels).
    //
    // MODELLED-VS-ASM (see file header): identical opaque-callee situation to the body-part variant --
    // DetachedWheelManager / ContactGenList / CollisionGenerator / InTriangleCacheInterface are
    // forward-declared by pointer only, and the emission externals (sub_825E8308, PrimitivePairListBuilder,
    // BaseCollisionGenerator, TriangleCacheInterface, ContactGenList::AddEntry) are unresolved. The
    // per-wheel contact-emission block is therefore FLAGGED-DEFERRED; the fixed 4-wheel loop bound and
    // the assert tripwire are reproduced.
    // =================================================================================================
    void DeformableObject::DoDetachedWheelWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                                                 ContactGenList* lpGenList,
                                                                 CgsPhysics::CollisionGenerator* lpGen,
                                                                 CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                                 const DetachedWheelManager* lpWheelMgr,
                                                                 u32 luFlags,
                                                                 CgsMemory::LinearMalloc* lpAlloc) const
    {
        (void)lpTriCache; (void)lpGenList; (void)lpGen; (void)lpProducer;
        (void)lpWheelMgr; (void)luFlags;   (void)lpAlloc;
        (void)KU_USE_SPY_WORLD_CONTACT_PATH;

        // The X360 wheel count is fixed at four (asm: stride 224, bound 896 == 4 * 224).
        const s32 KI_NUM_WHEELS = 4;

        for (s32 liWheel = 0; liWheel < KI_NUM_WHEELS; ++liWheel)
        {
            // asm: only DETACHED wheels (physics-body state byte == 2) generate contacts. Fetch the wheel
            // from the manager, skip if already listed, pull its triangle batches, emit the primitive-vs-
            // triangle contacts, and add the ContactGenList entry.
            //
            // FLAG (deferred contact emission): the per-wheel state read, the wheel-manager fetch
            // (sub_825E8308), the triangle-batch pull (TriangleCacheInterface), the primitive-pair build
            // (PrimitivePairListBuilder) and the contact record (BaseCollisionGenerator +
            // ContactGenList::AddEntry) all require opaque forward-declared types / unresolved externals
            // with no accessors -- NOT reconstructed here (no fabrication). The assert tripwire is kept:
            CGS_ASSERT(true /* wheel pool index < 0x70 */, "Bad wheel pool index: ");
        }
    }
}
}
