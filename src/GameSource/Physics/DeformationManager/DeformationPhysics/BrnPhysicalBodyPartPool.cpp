#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (walls leg 4 gates)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"   // the REAL interface (walls leg 4: model accessor views)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"  // the two OutputEvents sinks (landed 2026-08-24)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"        // IKBodyPart::GetMeshId / GetPartType (OutputEvents' trailer fields)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint ([detach-pose] probe)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // OutUpdateRigidBody (UpdatePart's echo event)
#include <cstdlib>   // getenv/atoi ([detach-pose] latch)

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
    static const f32 KF_PART_EXTRA_GRAVITY = 0.0f;

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
        Matrix44Affine lBBoxOrientation, Vector3 lLinearVelocity,
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
                        lGraphicsTransform, lBBoxOrientation);

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
            const Vector3 lvLocalGravity = { 0.0f, KF_PART_EXTRA_GRAVITY, 0.0f, 0.0f };
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
        ContactSpyData* lpContactSpyData, VecFloat lvfTimeStep)
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

            const u32 luOwner = static_cast<u32>(lContact.muVolumeInstanceIdA >> 56) & 0xFF;
            CGS_ASSERT(luOwner == KU_OWNER_RACECAR_DEFORMABLE_PART
                       || luOwner == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

            const s32 liPartIndex = lContact.muPartIndex;
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
            // Copy the record (asm copies 10 dwords into v130) so the local sign-flip does not
            // mutate the queue's stored contact. FLAG: the asm negates the record's leading 4-vector
            // (the contact normal/point) before AddContact; that vector lives in this minimal model's
            // opaque payload, so the negate is documented but deferred to whichever TU homes the full
            // PotentialContact layout. The copy + forward preserves the observable record routing.
            PotentialContact lContact = *lpCarContactQueue->GetContact(liCarContactIndex);

            const u32 luOwner = static_cast<u32>(lContact.muVolumeInstanceIdA >> 56) & 0xFF;
            CGS_ASSERT(luOwner == KU_OWNER_RACECAR_DEFORMABLE_PART
                       || luOwner == KU_OWNER_TRAFFIC_DEFORMABLE_PART,
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                       "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

            const s32 liPartIndex = lContact.muPartIndex;
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
    // LOG-ONCE GATES 2026-08-14 (walls leg 4): the two remaining declared pool drivers --
    // UpdateABoundingBox (per-frame round-robin bbox refresh) and UpdatePart (post-physics
    // per-part read-back). Both dead today (0 detached parts). Reconstruct and DELETE.
    // =============================================================================================
    void PhysicalBodyPartPool::UpdateABoundingBox(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInterface*/)
    {
        static bool sbLoggedUBB = false;
        if ( !sbLoggedUBB )
        {
            sbLoggedUBB = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalBodyPartPool::UpdateABoundingBox reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }

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
                        *CgsDev::Log::gpDebugPrint
                            << "[detach-pose] slot " << liSlot
                            << " mesh " << lpIKPart->GetMeshId()
                            << " attached " << (lrPart.IsJoinedToVehicle() ? 1 : 0)
                            << " world (" << lTransform.wAxis.x << ", " << lTransform.wAxis.y
                            << ", " << lTransform.wAxis.z << ")\n";
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
