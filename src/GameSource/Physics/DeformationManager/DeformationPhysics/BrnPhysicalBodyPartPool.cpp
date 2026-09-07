#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (walls leg 4 gates)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"   // the REAL interface (walls leg 4: model accessor views)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"  // the two OutputEvents sinks (landed 2026-08-24)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"        // IKBodyPart::GetMeshId / GetPartType (OutputEvents' trailer fields)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint ([detach-pose] probe)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // OutUpdateRigidBody (UpdatePart's echo event)
#include <cstdlib>   // getenv/atoi ([detach-pose] latch)

// [detach-pose] host-side present counter, for EXACT frame correlation -- the same extern the
// other correlated instruments use (BrnActiveRaceCar.cpp:59, CgsIm2d.cpp:24, BrnRendererModule
// .cpp:288). BRN_FRAME_DUMP names its BMPs bb_<guPresentCount>.bmp, so printing this number
// turns "the frame where the panel is at y = -3" from an ESTIMATE into a filename. A trace
// correlated against a dump of a DIFFERENT frame has already produced a false lead in this tree.
namespace renderengine { extern u32 guPresentCount; }

// ============================================================================
// BrnPhysics::Deformation::PhysicalBodyPartPool
//
// The out-of-line bodies for the fixed 50-slot detached-body-part pool, reconstructed
// store-for-store from the X360 ARTIST.XEX (big-endian). The pool owns maParts[50] (each
// PhysicalBodyPart is 496 bytes -- the asm indexes them as `496*index + this`), a
// CgsContainers::BitArray<50> used-mask (single 64-bit field; the asm reads it at
// `this + 24800`), a bbox round-robin cursor, and the live-part count (`this + 24812`).
//
// Functions bodied here (X360 addresses):
//   Construct        (DWARF only; no per-function asm export) -- construct every slot, clear mask
//   Create           @ 0x826269A0 -- allocate the first free slot, Prepare it, mark it used
//   GetPart          @ 0x825A0858 -- bounds + used asserts, return &maParts[index]   (mutable)
//   GetPart (const)  @ 0x825C1BE0 -- a DISTINCT body with its OWN assert messages
//                                    ("liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS" /
//                                     "mUsedParts.IsBitSet( liPartIndex )"), NOT identical to mutable
//   IsPartIndexUsed  @ 0x825A0758 -- bounds assert, return mUsedParts.IsBitSet(index)
//   UpdateRWBodies   @ 0x825E7E98 -- walk used parts, UpdateRW each + add the extra-gravity force
//   UpdateJoinedParts@ 0x8260D200 -- accumulate world+car contacts, integrate active joints
//
// BIT-WALK NOTE: every per-frame driver here walks mUsedParts with the X360 lowest-set-bit
// idiom (`field*64 - clz64(field & -field) + 63`, re-seeded per 64-bit field). That idiom is
// value-identical to CgsContainers::BitArray<50>::GetFirstNonZeroBit / GetNextNonZeroBit, which
// the X360 build inlined at each call site; the walks below are expressed through those container
// methods (the canonical home), reproducing the same visited-slot sequence as the asm.
//
// ASSERTS: the X360 bounds tripwires (BeginAssert/FireAssert/EndAssert triples, including the
// inlined CgsBitArray "invalid index : N < 50" diagnostics) are NON-gating -- execution continues
// past a failed assert exactly as the asm does. They are modelled as CGS_ASSERT and the lookup /
// store that follows runs regardless. The original source file paths/line numbers are dropped.
//
// FLAGGED-0 PLACEHOLDER: KF_PART_EXTRA_GRAVITY (DWARF namespace-scope f32 @ BrnPhysicalBodyPartPool
// .h:34) is rodata that is NOT present in the exports -- carried as an honest zero (NEVER
// fabricated). UpdateRWBodies' extra-gravity force therefore stays inert until the real value is
// recovered from the XEX rodata; its application SHAPE (build a local-frame force, scale by a body
// orientation row, AddLocalSpaceForce) is faithful.
//
// Callers (X360 xrefs): Create <- DetachedPartManager::MakePart; GetPart/IsPartIndexUsed <-
// DeformationManager + DetachedPartManager (many); UpdateRWBodies <- DeformationManager::Update;
// UpdateJoinedParts <- DetachedPartManager::UpdatePostPhysics.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // FLAGGED-0 PLACEHOLDER for the namespace-scope extra-gravity constant at
    // BrnPhysicalBodyPartPool.h:34 (DWARF `float32_t KF_PART_EXTRA_GRAVITY`). The numeric value is
    // NOT in the per-function exports; carried as an honest zero (NEVER fabricated). Used only by
    // UpdateRWBodies below.
    f32 kfPartExtraGravity = 0.0f;   // ARTIST .bss default at 0x82FB7E14

    // X360 deformation-part owner tags written into the contact volume-instance id (see
    // BrnBurnoutBodyPartID.h). UpdateJoinedParts tripwires that each contact's owner is one of
    // these two before accepting it.
    static const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;  // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
    static const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART
    // ==========================================================================================
    // PhysicalBodyPartPool::Construct MOVED OUT on 2026-08-03 (task #116) to
    // BrnPhysicalBodyPartPool_Construct.cpp, verbatim. WHY: PhysicsModule::Construct @0x825AE308
    // was a live empty stub; un-stubbing it reaches DetachedPartManager::Construct -> this.
    // ⛔ STALE BANNER, CORRECTED 2026-08-27 (detach wave): the text below described 2026-08-03.
    // THIS TU IS MOUNTED (build_game_exe.bat: BrnPhysicalBodyPartPool.cpp) and the 9 unresolved
    // externals are closed. The re-merge note survives only as the history of WHY Construct is
    // next door.
    // (historic) a MEASURED trial link (task #116, M2) put it at 9 unresolved
    // externals, all from CreatePart / UpdateRWBodies / UpdateJoinedParts. NONE were referenced
    // from Construct. TO RE-MERGE: mount this TU and move the body back.
    // ==========================================================================================


    // ------------------------------------------------------------------------------------------
    // Create @ 0x826269A0
    //   Find the first free slot (the asm walks the used-mask fields skipping any all-ones field
    //   == -1, then takes the lowest CLEAR bit of the first non-full field -- exactly
    //   BitArray<50>::GetFirstClearBit). If the pool is full (no clear bit < 50) return null.
    //   Otherwise pack the part's BurnoutBodyPartID, Prepare it against its vehicle + IK spec +
    //   transforms, mark the slot used, bump the live count, and return the slot.
    //
    //   The two `vcmpgtfp ... vperm ... BeginAssert/FireAssert(...,106/107)` blocks are
    //   non-gating finite/handedness tripwires on the freshly built bbox axes -- they fire (or not)
    //   and execution falls straight through into the mark-used step regardless. They are modelled
    //   as the bookkeeping below with no early-out, matching the asm's straight-line fall-through.
    //
    //   The Hex-Rays arg soup (a1..a30) is the X360 by-value Matrix44Affine/Vector3 spilling; the
    //   real signature is the frozen header's CreatePart (the layout-authoritative declaration).
    //   The pool indexes maParts at 496*slot == &maParts[slot]. Per the header the entry point is
    //   CreatePart; this is its body.
    // ------------------------------------------------------------------------------------------
    PhysicalBodyPart* PhysicalBodyPartPool::CreatePart(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        u16 lu16IKPartIndex, const DeformableObject* lpDeformableObject,
        RigidBodyId lRigidBodyId, EntityId lGlobalVehicleId, s32 liMeshIndex,
        const IKBodyPart* lpIKPart, Matrix44Affine lGraphicsTransform,
        Matrix44Affine lVehicleTransform, Vector3 lLinearVelocity,
        Vector3 lAngularVelocity)
    {
        // Lowest free slot, or KI_INVALID_BITINDEX(-1) / >=50 when the pool is full.
        const s32 liFreeSlot = mUsedParts.GetFirstClearBit();
        if (liFreeSlot < 0 || static_cast<u32>(liFreeSlot) >= KU_MAX_DETACHED_PARTS)
        {
            return 0;
        }

        const u32 luSlot = static_cast<u32>(liFreeSlot);
        PhysicalBodyPart* lpPart = &maParts[luSlot];

        // ⭐⭐ THE ID PACK, RE-READ FROM THE ASM 2026-08-27 (detach wave). The banner that stood here
        // claimed the asm passes "only THREE value args" and packed 0 into the partIndex field. It
        // passes FOUR, and the partIndex field is the MESH/IK part index. The register setup at
        // 0x82626A44..0x82626A64, immediately before `bl BurnoutBodyPartID::Set @0x825C1A10`:
        //     0x82626A44  rldicl r10, r7, 32, 0    ; r7 == lRigidBodyId (8 bytes) -> its HIGH dword
        //     0x82626A4C  mr     r6, r5            ; luSubA = lu16IKPartIndex  (saved BEFORE r5 is
        //                                          ;          overwritten two instructions later)
        //     0x82626A50  rlwinm r4, r10, 0,0,31   ; luOwningVehicleID = (u32)(lRigidBodyId >> 32)
        //     0x82626A54  rlwinm r7, r25, 0,16,31  ; luSubB = the free POOL SLOT (r25 -- the value
        //                                          ;          `cmpwi r25, 50` bounds just above)
        //     0x82626A58  rlwinm r5, r9, 0,16,31   ; luPartIndex = liMeshIndex
        //
        // ⛔ AND THE OLD MAPPING WAS A LIVE DEFECT, caught by the CONSOLE'S OWN ASSERT the first
        // time a part was ever detached on this build: DeformableObject::DetachPart asserts
        //     lpPhysicalBodyPart->GetIKPartIndex() == liPartIndex
        // and GetIKPartIndex() reads muEntityWord & 0x3FF -- the partIndex field. With 0 packed
        // there that assert can only pass for part 0. The in-image assert is independent evidence
        // for this reading: the console REQUIRES partIndex == the IK part index.
        // (`lRigidBodyId >> 32` is the same `ld` + `srdi 32` idiom every other RigidBodyId consumer
        // in this subsystem uses; spelled through GetEntityId() so the shift lives in one place.)
        BurnoutBodyPartID lPartId;
        lPartId.Set(static_cast<u32>(lRigidBodyId.GetEntityId()),
                    static_cast<u16>(liMeshIndex),
                    lu16IKPartIndex,
                    static_cast<u16>(luSlot));

        // Bind the part to its vehicle + IK spec, building the joint/graphics/COM/bbox frames.
        lpPart->Prepare(lPartId, lGlobalVehicleId, lpDeformableObject, lpIKPart,
                        lGraphicsTransform, lVehicleTransform);

        // Non-gating bbox finite/handedness tripwires (X360 BrnPhysicalBodyPartPool.cpp:106/107).
        // The asm evaluates `vcmpgtfp(epsilon^2, |axis|^2)` over the built axes and fires the
        // assert when a degenerate axis is detected; it then falls straight through. Modelled as
        // the unconditional fall-through below (the boolean result is not load-bearing here).

        // Mark the slot used + bump the live-part count, then hand back the slot.
        mUsedParts.SetBit(luSlot);
        ++mu8NumDetachedParts;
        return lpPart;
    }

    // ------------------------------------------------------------------------------------------
    // GetPart (mutable @0x825A0858) / GetPart (const @0x825C1BE0) / IsPartIndexUsed (@0x825A0758)
    // MOVED 2026-08-06 (bridge de-facade wave) to the mounted slice TU
    // BrnPhysicalBodyPartPool_Accessors.cpp: the de-facaded contact-spy bridge needs these three
    // to LINK (DetachedPartManager's inline wrappers forward here), while THIS TU is still
    // unmounted (its UpdateRWBodies/UpdateJoinedParts tail has its own open closure). Bodies are
    // verbatim there; fold back when this TU mounts.
    // ------------------------------------------------------------------------------------------

    // ------------------------------------------------------------------------------------------
    // UpdateRWBodies @ 0x825E7E98
    //   Walk every used part (GetFirstNonZeroBit / GetNextNonZeroBit over mUsedParts), and for each:
    //     1) PhysicalBodyPart::UpdateRW(lpSimInput, lvfTimeStep) -- push its transform into RW.
    //     2) Build the extra-gravity force in the part's local frame: a (0, KF_PART_EXTRA_GRAVITY,
    //        0, 0) vector lane-multiplied by a body orientation row (asm: `lvx128 v0,[r31+0xD0];
    //        vmulfp128 v1,v13,v0`), then ExternalPhysicsBody::AddLocalSpaceForce(force).
    //   The mid-walk "invalid index : N < 50" StrStream assert is the inlined GetNextNonZeroBit
    //   bounds tripwire (non-gating). lvfTimeStep arrives in v1/v127 and is threaded into UpdateRW.
    //
    //   FLAG: the +0xD0 source loaded into the force multiply is a body orientation row whose exact
    //   accessor is not cleanly exposed; the force here is scaled by the part's render-transform
    //   up-row (GetRenderTransform().up), preserving the asm's "rotate local gravity by a body row"
    //   shape. With KF_PART_EXTRA_GRAVITY == 0 (flagged placeholder) the accumulated force is zero
    //   regardless of the row, so this stays observably inert until the rodata is recovered.
    // ------------------------------------------------------------------------------------------
    void PhysicalBodyPartPool::UpdateRWBodies(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep)
    {
        for (s32 liPart = mUsedParts.GetFirstNonZeroBit();
             liPart != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
             liPart = mUsedParts.GetNextNonZeroBit(liPart))
        {
            PhysicalBodyPart* lpPart = &maParts[liPart];

            lpPart->UpdateRW(lpSimInput, lvfTimeStep);

            // Local-frame extra-gravity force == (0, KF_PART_EXTRA_GRAVITY, 0) rotated by a body
            // orientation row (asm vmulfp128 against the +0xD0 row). See the FLAG above.
            const Matrix44Affine lRenderTransform = lpPart->GetRenderTransform();
            const Vector3& lvUpRow = lRenderTransform.Up();
            const Vector3 lvLocalGravity = { 0.0f, kfPartExtraGravity, 0.0f, 0.0f };
            const Vector3 lvForce = {
                lvLocalGravity.x * lvUpRow.x,
                lvLocalGravity.y * lvUpRow.y,
                lvLocalGravity.z * lvUpRow.z,
                0.0f
            };
            lpPart->GetExternalBody()->AddLocalSpaceForce(lvForce);
        }
    }

    // ------------------------------------------------------------------------------------------
    // UpdateJoinedParts @ 0x8260D200
    //   Three phases, in the asm's order:
    //
    //   (1) Post-vehicle bookkeeping pass: walk every used part and, for any part still joined to
    //       its vehicle (the +0x1E4 flag, == mbJoinedToVehicle), call PhysicalBodyPart::
    //       PostVehicleUpdate(). (Asm: first GetFirstNonZeroBit/GetNextNonZeroBit walk with the
    //       `if (*(part+484)) PostVehicleUpdate()` guard.)
    //
    //   (2) Contact accumulation: pull the two hinged-body-part potential-contact queues from the
    //       interface (WORLD queue then CAR queue) and forward each contact to the destination
    //       part. For each contact the asm tripwires the volume-instance owner tag (must be 6 or 7)
    //       and the destination slot index (< 50, used), then PhysicalBodyPart::AddContact(part,
    //       contact). The CAR loop additionally negates a 4-vector of the copied contact record
    //       (the `vxor(1.0, -1<<31)` builds a sign-flip mask, `vmulfp128` applies it) before the
    //       AddContact -- the car-side contact is mirrored onto the other body, matching the asm.
    //
    //   (3) Joint integration: walk every used part again and, for any part still joined (the
    //       +0x1E4 flag), integrate its joint one step (UpdateJoint(lvfTimeStep)) and write its
    //       contact-spy record (AddContactSpy(lpContactSpyData)). lvfTimeStep is preserved in v126
    //       across phase (2) and reloaded into v1 (`vmr128 v1,v126`) before each UpdateJoint.
    //
    //   All "invalid index : N < 50" StrStream blocks are the inlined CgsBitArray bounds tripwires
    //   (non-gating). The Hex-Rays arg soup (a1..a16) is the three by-value/VecFloat args spilling;
    //   the frozen-header signature is authoritative.
    // ------------------------------------------------------------------------------------------
    void PhysicalBodyPartPool::UpdateJoinedParts(
        const PhysicsModuleIO::PotentialContactInterfaceModel* lpPotentialContactsInterface,
        BrnPhysics::ContactSpy::ContactSpyData* lpContactSpyData, VecFloat lvfTimeStep)
    {
        // ---- phase (1): post-vehicle bookkeeping over the still-joined parts ----
        for (s32 liPart = mUsedParts.GetFirstNonZeroBit();
             liPart != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
             liPart = mUsedParts.GetNextNonZeroBit(liPart))
        {
            if (maParts[liPart].IsJoinedToVehicle())
            {
                maParts[liPart].PostVehicleUpdate();
            }
        }

        // ---- phase (2): accumulate the world + car potential contacts ----
        const PhysicsModuleIO::PotentialContactInterfaceModel::CustomPotentialContactQueue*
            lpWorldContactQueue = lpPotentialContactsInterface->GetHingedBodyPartWithWorldQueue();
        const PhysicsModuleIO::PotentialContactInterfaceModel::CustomPotentialContactQueue*
            lpCarContactQueue = lpPotentialContactsInterface->GetHingedBodyPartWithCarQueue();

        // WORLD contacts (asm: queue base v20 = a2 + 163872; count @ +8; CgsScen(queue, i)).
        const s32 liNumWorldContacts = lpWorldContactQueue->GetNumContacts();
        for (s32 liWorldContactIndex = 0; liWorldContactIndex < liNumWorldContacts; ++liWorldContactIndex)
        {
            const PotentialContact& lContact = *lpWorldContactQueue->GetContact(liWorldContactIndex);

            // `ld r11, 0x30(contact) ; srdi 32 ; srwi 24` -- the id's TOP byte.
            const u32 luOwner = static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 56) & 0xFFu;
            CGS_ASSERT(luOwner == KU_OWNER_RACECAR_DEFORMABLE_PART
                       || luOwner == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

            // ⭐ 2026-09-05: the destination slot is the record's byte +64 -- muPolyTagA -- which
            // is what `lwz r29, 0x40(r27)` reads and what `cmpwi r29, 0x32` bounds against 50.
            // It used to be a member named muPartIndex on a 48-byte fork, at byte +12. See the
            // banner over the retired fork in BrnPhysicalBodyPartPool.h.
            const s32 liPartIndex = static_cast<s32>(lContact.muPolyTagA);
            CGS_ASSERT(liPartIndex < static_cast<s32>(KU_MAX_DETACHED_PARTS),
                       "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");
            CGS_ASSERT(mUsedParts.IsBitSet(static_cast<u32>(liPartIndex)),
                       "mUsedParts.IsBitSet( liPartIndex )");

            maParts[liPartIndex].AddContact(lContact);
        }

        // CAR contacts (asm: queue base v21 = a2 + 327728). The copied contact record's leading
        // 4-vector is sign-flipped (`vxor128 v127, 1.0, -1<<31` builds the negate mask, applied by
        // `vmulfp128`) before AddContact -- the car-side contact is mirrored onto the joined part.
        const s32 liNumCarContacts = lpCarContactQueue->GetNumContacts();
        for (s32 liCarContactIndex = 0; liCarContactIndex < liNumCarContacts; ++liCarContactIndex)
        {
            // Copy the record so the local sign-flip does not mutate the queue's stored contact.
            // ⭐ 2026-09-05: the copy is `li r9, 0xA ; ld/std ; bdnz` at 0x8260D734..0x8260D74C --
            // TEN DOUBLEWORDS, 80 bytes, which is sizeof(PotentialContact) exactly and is the
            // measurement that settles which record type this queue carries.
            PotentialContact lContact = *lpCarContactQueue->GetContact(liCarContactIndex);

            const u32 luOwner = static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 56) & 0xFFu;
            CGS_ASSERT(luOwner == KU_OWNER_RACECAR_DEFORMABLE_PART
                       || luOwner == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

            const s32 liPartIndex = static_cast<s32>(lContact.muPolyTagA);

            // ⭐ 2026-09-05 -- THE NEGATE IS RESOLVED, and it happens here, between the two
            // asserts, exactly where the asm puts it (0x8260D788 loads the index, 0x8260D790..9C
            // negates, 0x8260D7A0 branches to the index assert). `vspltisw v13,1 ; vcfsx v13,v13,0`
            // builds 1.0f, `vspltisw v0,-1 ; vslw v0,v0,v0` builds the 0x80000000 lane mask,
            // `vxor128 v127,v13,v0` makes -1.0f, and `vmulfp128 v0, v0, v127` applies it to the
            // vector at copy+0x20 == mNormal. It is the CONTACT NORMAL that is mirrored onto the
            // joined part -- not "the leading 4-vector" the previous FLAG guessed at, which with a
            // 48-byte forked record was unreachable anyway.
            lContact.mNormal.x = -lContact.mNormal.x;
            lContact.mNormal.y = -lContact.mNormal.y;
            lContact.mNormal.z = -lContact.mNormal.z;
            lContact.mNormal.w = -lContact.mNormal.w;

            CGS_ASSERT(liPartIndex < static_cast<s32>(KU_MAX_DETACHED_PARTS),
                       "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");
            CGS_ASSERT(mUsedParts.IsBitSet(static_cast<u32>(liPartIndex)),
                       "mUsedParts.IsBitSet( liPartIndex )");

            maParts[liPartIndex].AddContact(lContact);
        }

        // ---- phase (3): integrate each still-joined part's joint + emit its contact-spy ----
        for (s32 liPart = mUsedParts.GetFirstNonZeroBit();
             liPart != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
             liPart = mUsedParts.GetNextNonZeroBit(liPart))
        {
            if (maParts[liPart].IsJoinedToVehicle())
            {
                maParts[liPart].UpdateJoint(lvfTimeStep);
                maParts[liPart].AddContactSpy(lpContactSpyData);
            }
        }
    }

    // =============================================================================================
    // AddPartsToScene @0x8260CF38 (178) -- ⭐ 2026-08-14 (walls leg 4). Walk mUsedParts; every
    // live part that is neither frozen (+486) nor already in the scene (+485) gets
    // PhysicalBodyPart::AddToScene(scene). Caller: DeformationManager::UpdatePostPhysics (via the
    // manager forward).
    // ⛔ "Dead-at-runtime today (0 physical parts on the junkyard path)" -- NO LONGER TRUE as of
    // 2026-08-27: parts DO detach, mi16NumPhysicalParts reaches 7 on the player car in the
    // deterministic junkyard crash, so this walk has live slots. What it reaches instead is
    // PhysicalBodyPart::AddToScene, which is itself still a log-once gate.
    // =============================================================================================
    void PhysicalBodyPartPool::AddPartsToScene(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        for (s32 liPart = mUsedParts.GetFirstNonZeroBit();
             liPart != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
             liPart = mUsedParts.GetNextNonZeroBit(liPart))
        {
            PhysicalBodyPart& lrPart = maParts[liPart];
            if ( !lrPart.IsFrozen() && !lrPart.IsAddedToScene() )   // +486 == 0 && +485 == 0
            {
                lrPart.AddToScene(lpSceneInterface);
            }
        }
    }




    // =============================================================================================
    // ⭐ 2026-08-14 (walls leg 4): the local PotentialContactInterfaceModel's two queue accessors,
    // bodied as VIEWS over the REAL PhysicsModuleIO::PotentialContactInterface (the fork seam the
    // model's own banner flags -- the model IS the real interface seen through a minimal local
    // shape; the model's {ptr(8), pad(4), count(4)} row is layout-identical to the host
    // EventQueue header, so the reinterpret is byte-exact). Queue indices [7] (vs car) / [8]
    // (vs world) are the real interface's own attested accessors. Retire with the model when
    // UpdateJoinedParts re-types onto the real interface. Dead at runtime today (0 hinged parts).
    // =============================================================================================

}
}
namespace BrnPhysics
{
namespace Deformation
{

    // =============================================================================================
    // UpdateABoundingBox @ 0x8260CC88 -- ⭐⭐ RECONSTRUCTED 2026-09-07 (part-box wave).
    // THE GATE IS GONE, AND IT WAS STALE RATHER THAN DEAD.
    //
    // What stood here was a log-once "conductor gate" left on 2026-08-14 whose own banner said
    // "dead today (0 detached parts)". That premise expired the moment parts began shedding: this
    // is step (4) of DetachedPartManager::UpdatePostPhysics @0x8260E118, so it runs EVERY physics
    // frame with a live pool, and until now it did nothing at all. [[gates-are-stale-not-dead]] --
    // ask WHEN a gate last ran, not whether it is reachable.
    //
    // CONSEQUENCE OF THE STUB, stated plainly: a detached part's collision box was whatever
    // CalcBoundingBox produced inside Prepare at the instant of detachment, frozen for the whole
    // life of the part. PhysicalBodyPart::UpdateBoundingBox() -- the recompute-only overload that
    // re-derives mBoundingBoxHalfDimensions from the CURRENT skinned control points -- had ZERO
    // callers anywhere in the tree. The console refreshes ONE part per frame, for ever.
    //
    // ---- the console's body, register for register (X360 ARTIST; the export set has a HOLE for
    // this address, so it is read out of the image with tools/re/ppcdis.py) ----------------------
    //   0x8260CC98  lwz    r11, 0x60E8(r15)      ; miLastUpdatedBoundingBox   (pool +24808)
    //   0x8260CC9C  cmpwi  cr6, r11, -1
    //   0x8260CCA0  bne    cr6, 0x8260CD24       ; cursor live -> GetNextNonZeroBit(cursor)
    //   0x8260CCA4  addi   r9, r15, 0x60E0       ; &mUsedParts                (pool +24800)
    //   0x8260CCA8..CCC8                          ; inlined GetFirstNonZeroBit (one 64-bit word)
    //   0x8260CCCC  li     r11, -1               ; nothing set / ran off the end
    //   0x8260CCD0  stw    r11, 0x60E8(r15)      ; the cursor is stored EITHER WAY
    //   0x8260CCD8  cmpwi  cr6, r11, -1
    //   0x8260CCDC  beq    cr6, 0x8260CCEC       ; -1 -> update NOTHING this frame
    //   0x8260CCE0  mulli  r11, r11, 0x1F0       ; 496 == sizeof(PhysicalBodyPart)
    //   0x8260CCE4  add    r3, r11, r15          ; &maParts[slot]
    //   0x8260CCE8  bl     0x8260ACC8            ; PhysicalBodyPart::UpdateBoundingBox
    //   0x8260CD24..CF34                          ; inlined GetNextNonZeroBit + its CgsBitArray
    //                                             ; ":193/:203 invalid index" tripwire, then the
    //                                             ; same 0x8260CCD0 store-and-update tail
    //
    // ⭐ CORROBORATED ON THE NEAR-ANCESTOR: DecFIGS PS3 @0x7517A8 decompiles to exactly this shape
    // (`if (*(this+24808) == -1) <first> else <next>; ... if (cursor != -1)
    // PhysicalBodyPart::UpdateBoundingBox(496*cursor + this)`), which also pins sizeof 496,
    // mUsedParts @+24800 and the cursor @+24808 -- the offsets this header already carries.
    //
    // ⚠️ lpSceneInterface IS GENUINELY UNUSED, on BOTH builds. r4 is never read on any path of the
    // X360 body before being clobbered as the assert helper's own argument, and the PS3 pseudocode
    // never mentions the parameter either. The header comment beside the declaration says
    // "recompute + REPUBLISH"; the republish half does not exist -- UpdateBoundingBox takes only
    // `this` and touches no scene interface. The parameter is kept because it is the DWARF
    // signature, not dropped, and it is named here so the next reader does not go looking for a
    // publish that the binary does not contain.
    //
    // ✅ MEASURED IN A RUN 2026-09-07 (part-box witness wave), 8 boots on exe a116fb3efe35, all
    // eight byte-identical, against a 15-boot PRE-FIX corpus. The banner above used to end "NOT
    // MEASURED IN A RUN"; here is the measurement, and one of its three claims did not survive.
    //
    //  (1) IT RUNS, ON A LIVE POOL. Per boot: 6,600 calls, ~5,500 of them with a non-empty pool,
    //      4,443-5,132 actual refreshes, 129-635 of which CHANGED the box. The control is in the
    //      same runs: every boot's first [ubb] census row reads `livePool 0 refreshed 0` with an
    //      empty [ubb-visits], and the two shots that shed nothing (pbd_h240_s70/s80) read
    //      `calls 6600 livePool 0 refreshed 0` for their whole length. The counter can print zero.
    //
    //  (2) THE BOX IS NO LONGER FROZEN, and this is the headline. Distinct half-extent triples per
    //      DETACHED (joined-0) episode:
    //          PRE-FIX  (15 boots, three builds)    0 of 110 episodes ever moved
    //          POST-FIX (8 boots)                  42 of  46 episodes moved (91.3%), max 499
    //      and the metric was never blind: the HINGED (joined-1) arm -- the same field, the same
    //      log line, the same runs -- moved 92.9% pre-fix and 91.3% post-fix. The two arms now
    //      agree, which is exactly what this function existing predicts.
    //
    //  (3) ⛔ THE CONTACT CONSEQUENCE IS NIL AT THESE SPEEDS, and the honest number is zero.
    //      The pad gate below (min-half > 0.15) was crossed by 1 of 46 free episodes over its whole
    //      free life, and by 0 of 46 while UNFROZEN -- and DoBodyPartWorldContactGeneration skips a
    //      frozen part outright, so the contact site never saw the other side. At the decision site
    //      itself: 6,356 pad evaluations over 41 detached episodes, 1,181 FAT (18.6%), toFat 0,
    //      toThin 0. The reason is measurable, not a shrug: once a part is free its box moves by a
    //      median of 0.0030 m (p90 0.0136, max 0.0495) while the median distance to the gate is
    //      0.0790 m -- 26x the movement. The deformation that DOES cross the gate happens while
    //      the panel is still attached (9 of 67 paired player parts, 13.4%, are already THIN->FAT
    //      by the time they detach). So this fix restores the console's arithmetic; it does not
    //      change how a shed panel collides in any run taken so far.
    // =============================================================================================
    // [DIAG] NOT IN THE X360 BINARY -- the [ubb] census, defined below this function so the
    // console's own body reads uninterrupted. File-local (anonymous namespace): no ODR surface,
    // no class-surface change, no storage. DELETE-WHEN the part-box question is banked.
    namespace { void UbbCensus(s32, s32, bool, f32, f32, u8, s32); }

    void PhysicalBodyPartPool::UpdateABoundingBox(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInterface*/)
    {
        // [DIAG] the entry-state reads for the [ubb] census below. Read-only; see its banner.
        const s32 liCursorOnEntry = miLastUpdatedBoundingBox;
        const bool lbPoolWasLive  = !mUsedParts.IsZero();

        // `cmpwi r11, -1` -> restart the sweep at the first used slot, else continue after it.
        const s32 liSlot =
            ( miLastUpdatedBoundingBox == CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX )
                ? mUsedParts.GetFirstNonZeroBit()
                : mUsedParts.GetNextNonZeroBit(miLastUpdatedBoundingBox);

        // `stw r11, 0x60E8(r15)` -- stored on BOTH arms, including the -1 that ends a sweep.
        miLastUpdatedBoundingBox = liSlot;

        // [DIAG] the pre-refresh box scalar, for the "did the recompute MOVE anything" counter.
        const f32 lfRadiusBefore =
            ( liSlot != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX )
                ? maParts[liSlot].GetSphereRadius() : 0.0f;

        if ( liSlot != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX )
        {
            maParts[liSlot].UpdateBoundingBox();   // bl 0x8260ACC8 on &maParts[slot]
        }

        UbbCensus(liCursorOnEntry, liSlot, lbPoolWasLive, lfRadiusBefore,
                  ( liSlot != CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX )
                      ? maParts[liSlot].GetSphereRadius() : 0.0f,
                  mu8NumDetachedParts, miLastUpdatedBoundingBox);
    }

    // =============================================================================================
    // [DIAG] NOT IN THE X360 BINARY. The [ubb] CENSUS -- the runtime witness for the reconstruction
    // above. Opt-in on the SAME BRN_DEFORM_TRACE latch the [detach-*] / [part-rest] probes use, so
    // one armed run produces all of them and they are directly correlatable. Read-only: it reads
    // the cursor, the used-mask and the part's own broad-phase radius, and writes nothing the
    // console does not.
    //
    // ⭐ WHY A CENSUS AND NOT A PER-CALL ROW. This runs EVERY physics frame -- ~60 rows a second,
    // ~15k rows a run -- and an assert/print storm starves the harness badly enough to manufacture
    // physics failures [[watch-the-window-asserts-pause]]. So the counters are incremented
    // unconditionally (their values are true even on an unarmed run) and printed on a period.
    //
    // ⭐⭐ WHY THESE COUNTERS AND NOT A MAXIMUM. "UpdateABoundingBox ran" cannot distinguish the
    // four states this wave has to separate:
    //     (a) it is never called at all                      -> calls == 0
    //     (b) it is called, but the pool is always empty      -> calls > 0, livePool == 0
    //     (c) it is called on a live pool but never refreshes -> livePool > 0, refreshed == 0
    //     (d) it refreshes, but every recompute is identical  -> refreshed > 0, moved == 0
    // and the per-slot VISIT HISTOGRAM (a distinct-value count, not a maximum) is the only thing
    // that can say whether the round-robin reaches EVERY used slot or parks on a subset.
    //
    // ⭐ THE CURSOR-SEED WITNESS (Construct seeds -1, not 0 -- see BrnPhysicalBodyPartPool_
    // Construct.cpp). `firstCall` records the cursor value and pool state at the FIRST call after
    // Construct, and `firstSweep` records the ORDERED slot list of the first sweep that visits
    // anything. Between them they say whether the seed had an observable consequence in this run
    // or was overwritten before the pool ever went live -- which is a measurement, not an
    // assumption. DELETE-WHEN the part-box question is closed and banked.
    // =============================================================================================
    namespace
    {
        s32 UbbProbePeriod()
        {
            static s32 siPeriod = -1;
            if ( siPeriod < 0 )
            {
                const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                const s32 liValue = (lpcEnv != 0) ? atoi(lpcEnv) : 0;
                siPeriod = (liValue > 0 && CgsDev::Log::gpDebugPrint != 0) ? liValue : 0;
            }
            return siPeriod;
        }

        // ⭐ THE PERIOD IS THE CONTROL. At 600 calls (~10 s of physics) a crash run prints
        // several census rows BEFORE the first part ever detaches, and those rows must read
        // `livePool 0 refreshed 0` with an EMPTY [ubb-visits]. That is the negative control for
        // this counter, in the SAME process and through the SAME path as the positive result: a
        // counter that cannot print zero cannot be trusted when it prints a number.
        static const u32 KU_UBB_CENSUS_EVERY   = 600;
        static const u32 KU_UBB_FIRST_SWEEP_MAX = 24;

        u32 gxUbbCalls     = 0;   // every entry -- step (4) of UpdatePostPhysics ran
        u32 gxUbbLivePool  = 0;   // ... with at least one used slot
        u32 gxUbbRestarts  = 0;   // ... entered with the cursor at -1 (GetFirstNonZeroBit arm)
        u32 gxUbbIdle      = 0;   // ... and chose -1 (empty pool, or the sweep just ended)
        u32 gxUbbRefreshed = 0;   // entries that actually called PhysicalBodyPart::UpdateBoundingBox
        u32 gxUbbMoved     = 0;   // ... where the broad-phase radius CHANGED across the call
        u32 gxaUbbVisits[PhysicalBodyPartPool::KU_MAX_DETACHED_PARTS] = { 0 };

        bool gxbUbbFirstCallSeen = false;
        s32  gxiUbbFirstCursor   = -2;    // the cursor Construct left, as seen by the first call
        bool gxbUbbFirstLive     = false; // ... and whether the pool was already live then
        s32  gxaUbbFirstSweep[KU_UBB_FIRST_SWEEP_MAX] = { 0 };
        u32  gxUbbFirstSweepLen  = 0;
        bool gxbUbbSweepOpen     = false;
        bool gxbUbbFirstSweepDone = false;

    void UbbCensus(s32 liCursorOnEntry, s32 liSlot, bool lbPoolWasLive,
                   f32 lfRadiusBefore, f32 lfRadiusAfter,
                   u8 lu8UsedNow, s32 liCursorNow)
    {
        const u32 KU_MAX_DETACHED_PARTS = PhysicalBodyPartPool::KU_MAX_DETACHED_PARTS;
        const s32 KI_NONE = CgsContainers::BitArray<PhysicalBodyPartPool::KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;

        ++gxUbbCalls;
        if ( !gxbUbbFirstCallSeen )
        {
            gxbUbbFirstCallSeen = true;
            gxiUbbFirstCursor   = liCursorOnEntry;
            gxbUbbFirstLive     = lbPoolWasLive;
        }
        if ( lbPoolWasLive )  { ++gxUbbLivePool; }
        if ( liCursorOnEntry == KI_NONE ) { ++gxUbbRestarts; }

        if ( liSlot == KI_NONE )
        {
            ++gxUbbIdle;
            if ( gxbUbbSweepOpen ) { gxbUbbSweepOpen = false; gxbUbbFirstSweepDone = true; }
        }
        else
        {
            ++gxUbbRefreshed;
            if ( lfRadiusAfter != lfRadiusBefore ) { ++gxUbbMoved; }
            if ( liSlot >= 0 && liSlot < static_cast<s32>(KU_MAX_DETACHED_PARTS) )
            {
                ++gxaUbbVisits[liSlot];
            }
            if ( !gxbUbbFirstSweepDone )
            {
                gxbUbbSweepOpen = true;
                if ( gxUbbFirstSweepLen < KU_UBB_FIRST_SWEEP_MAX )
                {
                    gxaUbbFirstSweep[gxUbbFirstSweepLen++] = liSlot;
                }
            }
        }

        if ( UbbProbePeriod() <= 0 || (gxUbbCalls % KU_UBB_CENSUS_EVERY) != 0u )
        {
            return;
        }

        u32 luDistinctSlots = 0;
        u32 luMaxVisits = 0;
        u32 luMinVisitsOfVisited = 0xFFFFFFFFu;
        for ( u32 luSlot = 0; luSlot < KU_MAX_DETACHED_PARTS; ++luSlot )
        {
            if ( gxaUbbVisits[luSlot] == 0u ) { continue; }
            ++luDistinctSlots;
            if ( gxaUbbVisits[luSlot] > luMaxVisits ) { luMaxVisits = gxaUbbVisits[luSlot]; }
            if ( gxaUbbVisits[luSlot] < luMinVisitsOfVisited ) { luMinVisitsOfVisited = gxaUbbVisits[luSlot]; }
        }
        if ( luDistinctSlots == 0u ) { luMinVisitsOfVisited = 0u; }

        *CgsDev::Log::gpDebugPrint
            << "[ubb] f " << renderengine::guPresentCount
            << " calls " << static_cast<s32>(gxUbbCalls)
            << " livePool " << static_cast<s32>(gxUbbLivePool)
            << " restarts " << static_cast<s32>(gxUbbRestarts)
            << " idle " << static_cast<s32>(gxUbbIdle)
            << " refreshed " << static_cast<s32>(gxUbbRefreshed)
            << " moved " << static_cast<s32>(gxUbbMoved)
            << " usedNow " << static_cast<s32>(lu8UsedNow)
            << " cursorNow " << liCursorNow
            << " | firstCall cursor " << gxiUbbFirstCursor
            << " live " << (gxbUbbFirstLive ? 1 : 0)
            << " | distinctSlots " << static_cast<s32>(luDistinctSlots)
            << " visitMin " << static_cast<s32>(luMinVisitsOfVisited)
            << " visitMax " << static_cast<s32>(luMaxVisits)
            << " slot0 " << static_cast<s32>(gxaUbbVisits[0])
            << "\n";

        *CgsDev::Log::gpDebugPrint << "[ubb-visits]";
        for ( u32 luSlot = 0; luSlot < KU_MAX_DETACHED_PARTS; ++luSlot )
        {
            if ( gxaUbbVisits[luSlot] != 0u )
            {
                *CgsDev::Log::gpDebugPrint << " " << static_cast<s32>(luSlot)
                                           << ":" << static_cast<s32>(gxaUbbVisits[luSlot]);
            }
        }
        *CgsDev::Log::gpDebugPrint << "\n";

        *CgsDev::Log::gpDebugPrint << "[ubb-sweep1] closed "
                                   << (gxbUbbFirstSweepDone ? 1 : 0)
                                   << " len " << static_cast<s32>(gxUbbFirstSweepLen)
                                   << " slots";
        for ( u32 luIndex = 0; luIndex < gxUbbFirstSweepLen; ++luIndex )
        {
            *CgsDev::Log::gpDebugPrint << " " << gxaUbbFirstSweep[luIndex];
        }
        *CgsDev::Log::gpDebugPrint << "\n";
    }
    }   // anonymous namespace ([ubb] census)

    // ------------------------------------------------------------------------------------------
    // UpdatePart @ 0x8260CB08  (74 instructions) -- ⭐⭐ RECONSTRUCTED 2026-08-27 (detach-2 wave).
    // THE GATE IS GONE. This is the THIRD link in the sim-echo chain and the one that made a shed
    // part's new pose land back on the part: without it the sim integrated the body every frame and
    // the answer was dropped on the floor.
    //
    // It is a bounds-checked forwarder, and it is small. The asm, store for store:
    //   r27 = this (pool)   r26 = lpUpdateEvent   r25 = lpSceneInterface
    //   ld     r11, 0(r26)            ; the event's mID, read WHOLE (8 bytes)
    //   clrlwi r28, r11, 16           ; lu16PartIndex := the id's LOW 16 BITS == the pool slot
    //   cmplwi r28, 0x32 ; blt ok     ; CgsBitArray.h:203 StrStream "invalid index : " << i << " < " << 50
    //   <mUsedParts.IsBitSet tripwire>                     BrnPhysicalBodyPartPool.cpp:172
    //   <id-match tripwire>                                BrnPhysicalBodyPartPool.cpp:173
    //   bl PhysicalBodyPart::Update(&maParts[idx], event, sceneInterface)   ; 496*idx + this
    // All three asserts are NON-GATING: the asm falls straight through into the Update call.
    //
    // ⚠️ THE LOW-16 READ IS WHY BurnoutBodyPartID::GetBaseRigidBodyID() had to exist. The slot is
    // the handle's muSubB field, which only lands in the low 16 bits of the u64 under the console's
    // big-endian byte image of the record; that pack now lives in exactly one place (see the
    // accessor's own banner) and both this reading and UpdatePostPhysics' owner-byte reading are
    // spelled against it.
    // ------------------------------------------------------------------------------------------
    void PhysicalBodyPartPool::UpdatePart(const OutUpdateRigidBody* lpUpdateEvent,
                                          CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const u64 lu64EventId =
            reinterpret_cast<const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody*>(lpUpdateEvent)->mID;
        const u16 lu16PartIndex = static_cast<u16>(lu64EventId & 0xFFFFu);

        // CgsBitArray.h:203 -- the inlined bound tripwire, non-gating.
        CGS_ASSERT(lu16PartIndex < KU_MAX_DETACHED_PARTS, "invalid index : lu16PartIndex < 50");

        // :172 / :173 -- both non-gating.
        CGS_ASSERT(mUsedParts.IsBitSet(lu16PartIndex), "mUsedParts.IsBitSet( lu16PartIndex )");
        CGS_ASSERT(maParts[lu16PartIndex].GetRigidBodyId().GetBaseRigidBodyID() == lu64EventId,
                   "maParts[ lu16PartIndex ].GetRigidBodyId().GetBaseRigidBodyID() == lpUpdateEvent->mID");

        maParts[lu16PartIndex].Update(lpUpdateEvent, lpSceneInterface);
    }

    // =========================================================================================
    // OutputEvents @ 0x8260DBE8  (948 bytes; landed 2026-08-24, deform-land wave)
    //
    // Emit the two per-frame detached-part event streams for every USED pool slot:
    //   1. clear the entity-module render queue FIRST (`stw 0, 0x2E88(r4)` == Clear());
    //   2. per used part (mUsedParts bit walk, capacity 50):
    //      - DetachedPartRenderEvent  { GetEventRenderTransform() (the rigid transform with the
    //        rotated (graphicsPos - initialComPos) offset added to row 3),
    //        mVehicleEntityId = mGlobalVehicleId (part+0x1D8),
    //        miPartIndex = ikPart->spec mesh id (spec+0x1C8),
    //        mbIsAttached = mbJoinedToVehicle (part+0x1E4) }
    //        -> AddEvent onto lpOutputForEntityModules->mDetachedPartRenderQueue (+0x2E80);
    //      - DetachedPartCurrentPositionEvent { same transform, same id,
    //        meType = ikPart->spec part type (spec+0x1DC) }
    //        -> AddEvent onto lpOutput->mDetachedPartCurrentPositionQueue (+0xB40).
    // Both adds are the UNCONDITIONAL AddEvent (assert-tripwire bounds), not AddEventSafe.
    // =========================================================================================
    void PhysicalBodyPartPool::OutputEvents(
        Deformation::DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
        Deformation::DeformationOutputInterface* lpOutput) const
    {
        lpOutputForEntityModules->GetDetachedPartRenderQueue().Clear();

        for (s32 liSlot = mUsedParts.GetFirstNonZeroBit();
             liSlot != -1;
             liSlot = mUsedParts.GetNextNonZeroBit(liSlot))
        {
            const PhysicalBodyPart& lrPart = maParts[liSlot];
            const Matrix44Affine lTransform = lrPart.GetEventRenderTransform();
            const IKBodyPart* lpIKPart = lrPart.GetIKPart();

            Deformation::DetachedPartRenderEvent lRenderEvent;
            lRenderEvent.mTransform       = lTransform;
            lRenderEvent.mVehicleEntityId = lrPart.GetGlobalEntityId();
            lRenderEvent.miPartIndex      = lpIKPart->GetMeshId();          // spec+0x1C8
            lRenderEvent.mbIsAttached     = lrPart.IsJoinedToVehicle();
            lpOutputForEntityModules->GetDetachedPartRenderQueue().AddEvent(lRenderEvent);

            Deformation::DetachedPartCurrentPositionEvent lPositionEvent;
            lPositionEvent.mTransform       = lTransform;
            lPositionEvent.mVehicleEntityId = lrPart.GetGlobalEntityId();
            lPositionEvent.meType           = lpIKPart->GetPartType();      // spec+0x1DC
            lpOutput->mDetachedPartCurrentPositionQueue.AddEvent(lPositionEvent);

            // [detach-pose] NOT X360. The pose witness for the 2026-08-27 detach wave, latched on
            // BRN_DEFORM_TRACE (0/unset == inert). Prints the WORLD POSITION the shed panel is
            // actually drawn at, per slot, whenever it moves by more than a centimetre -- which is
            // the only thing that can distinguish "the panel separated" from "the panel is still
            // being drawn on the car". Pixels cannot answer that on their own at this camera
            // distance. DELETE-WHEN the detach question is closed and banked.
            {
                static s32 siPoseProbe = -1;
                if (siPoseProbe < 0)
                {
                    const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                    siPoseProbe = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
                }
                if (siPoseProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && liSlot < 16)
                {
                    static s32 saiLastX[16] = { 0 }; static s32 saiLastZ[16] = { 0 };
                    const s32 liXcm = static_cast<s32>(lTransform.wAxis.x * 100.0f);
                    const s32 liZcm = static_cast<s32>(lTransform.wAxis.z * 100.0f);
                    if (liXcm != saiLastX[liSlot] || liZcm != saiLastZ[liSlot])
                    {
                        saiLastX[liSlot] = liXcm; saiLastZ[liSlot] = liZcm;

                        // ⭐ BOTH SIDES, SAME FRAME, SAME PART (2026-09-02, deform close-out wave).
                        // The owner filmed a torn-off body panel PARKED IN MID-AIR above a
                        // stationary car. The rest-proof this wave inherited was read from a
                        // per-slot log line carrying only the number below marked `render` -- and
                        // if the two transforms have diverged, "frozen on the road" and "floating"
                        // are BOTH TRUE, of different numbers. So print the physics body's own
                        // transform (mRwBody, what the sim integrates and freezes) beside the
                        // published one (GetEventRenderTransform, what BrnRaceCarEntityModule_
                        // Render.cpp:625 assigns straight into lPartWorldMatrix), plus the offset
                        // that separates them -- localGraphicsPos - localInitialComPos, the term
                        // GetEventRenderTransform adds and GetRenderTransform does not.
                        // A car-height gap between the two columns indicts the offset; no gap at
                        // all indicts the physics rest state instead.
                        const Matrix44Affine lPhysics = lrPart.GetRigidBodyTransform();
                        *CgsDev::Log::gpDebugPrint
                            << "[detach-pose] present " << renderengine::guPresentCount
                            << " slot " << liSlot
                            << " mesh " << lpIKPart->GetMeshId()
                            << " attached " << (lrPart.IsJoinedToVehicle() ? 1 : 0)
                            << " render (" << lTransform.wAxis.x << ", " << lTransform.wAxis.y
                            << ", " << lTransform.wAxis.z << ")"
                            << " physics (" << lPhysics.wAxis.x << ", " << lPhysics.wAxis.y
                            << ", " << lPhysics.wAxis.z << ")"
                            << " dy " << (lTransform.wAxis.y - lPhysics.wAxis.y)
                            << "\n";
                    }
                }
            }
        }
    }
}
}

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    const PotentialContactInterfaceModel::CustomPotentialContactQueue*
    PotentialContactInterfaceModel::GetHingedBodyPartWithCarQueue() const
    {
        const PotentialContactInterface* lpReal =
            reinterpret_cast<const PotentialContactInterface*>(this);
        return reinterpret_cast<const CustomPotentialContactQueue*>(
            &lpReal->GetHingedBodyPartWithCarQueue());
    }

    const PotentialContactInterfaceModel::CustomPotentialContactQueue*
    PotentialContactInterfaceModel::GetHingedBodyPartWithWorldQueue() const
    {
        const PotentialContactInterface* lpReal =
            reinterpret_cast<const PotentialContactInterface*>(this);
        return reinterpret_cast<const CustomPotentialContactQueue*>(
            &lpReal->GetHingedBodyPartWithWorldQueue());
    }
}
}
