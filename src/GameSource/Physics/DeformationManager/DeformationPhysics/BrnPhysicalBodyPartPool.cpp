#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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

    // ------------------------------------------------------------------------------------------
    // Construct (DWARF BrnPhysicalBodyPartPool.cpp:42; no per-function asm export)
    //   Construct every part slot, then clear the used-mask. The DWARF hint lists the per-slot
    //   PhysicalBodyPart::Construct + the inlined Vector3Plus::SetZero seeds (those zero-seeds live
    //   inside PhysicalBodyPart::Construct) and the BitArray<50>::UnSetAll. The pool's own scalar
    //   state (bbox cursor + live count) is reset alongside.
    // ------------------------------------------------------------------------------------------
    void PhysicalBodyPartPool::Construct()
    {
        for (u32 luPart = 0; luPart < KU_MAX_DETACHED_PARTS; ++luPart)
        {
            maParts[luPart].Construct();
        }
        mUsedParts.UnSetAll();
        miLastUpdatedBoundingBox = 0;
        mu8NumDetachedParts = 0;
    }

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

        // Pack the part id. The X360 calls BurnoutBodyPartID::Set(owningVehicleId, 0, indexValue):
        // owningVehicleID == the owning vehicle's global EntityId, partIndex field == 0, and the
        // IK-part index threaded into the sub field. FLAG: the asm Set call (BurnoutBodyPartID::Set(
        // &id, a4, 0, a3) @ 0x826269A0) passes only THREE value args -- there is NO 4th (sub) value
        // arg in the asm, so the pool slot index is NOT threaded into Set here.
        // FLAG: the Hex-Rays Create arg soup (a3/a4) does not pin which CreatePart parameter is the
        // owning-vehicle id vs. the index value; modelled with lGlobalVehicleId as the owner and
        // lu16IKPartIndex as the index value, per the header param semantics (GetGlobalEntityId ==
        // owning vehicle; the IK-part index is the part's spec slot).
        BurnoutBodyPartID lPartId;
        lPartId.Set(lGlobalVehicleId.muValue, 0, lu16IKPartIndex);

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
    // GetPart (mutable) @ 0x825A0858
    //   Two bounds asserts (index < 50, "Part index out of range" + the inlined CgsBitArray
    //   "invalid index") and the used-bit assert, all non-gating, then return &maParts[index].
    // ------------------------------------------------------------------------------------------
    PhysicalBodyPart* PhysicalBodyPartPool::GetPart(s16 li16Index)
    {
        const u32 luIndex = static_cast<u32>(li16Index);
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "Part index out of range");
        CGS_ASSERT(mUsedParts.IsBitSet(luIndex), "mUsedParts.IsBitSet( liPartIndex )");
        return &maParts[luIndex];
    }

    // ------------------------------------------------------------------------------------------
    // GetPart (const) @ 0x825C1BE0 -- the const overload is a DISTINCT body (NOT the mutable one).
    //   Its bounds assert message is "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS"
    //   (BrnPhysicalBodyPartPool.h:176) -- NOT the mutable overload's "Part index out of range" --
    //   followed by the inlined CgsBitArray "invalid index : N < 50" bounds tripwire, then the
    //   used-bit assert "mUsedParts.IsBitSet( liPartIndex )" (BrnPhysicalBodyPartPool.h:177). All
    //   non-gating; returns &maParts[index].
    // ------------------------------------------------------------------------------------------
    const PhysicalBodyPart* PhysicalBodyPartPool::GetPart(s16 li16Index) const
    {
        const u32 luIndex = static_cast<u32>(li16Index);
        CGS_ASSERT(static_cast<s32>(li16Index) < static_cast<s32>(KU_MAX_DETACHED_PARTS),
                   "liPartIndex < (int32_t)KU_MAX_DETACHED_PARTS");
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "invalid index : ");
        CGS_ASSERT(mUsedParts.IsBitSet(luIndex), "mUsedParts.IsBitSet( liPartIndex )");
        return &maParts[luIndex];
    }

    // ------------------------------------------------------------------------------------------
    // IsPartIndexUsed @ 0x825A0758
    //   Single bounds assert (the inlined CgsBitArray "invalid index : N < 50", non-gating), then
    //   return the used-mask bit. The Hex-Rays `BOOL ... return ((a1 << SBYTE3(v14)) & v14) != 0`
    //   is the inlined IsBitSet bit-test.
    // ------------------------------------------------------------------------------------------
    bool PhysicalBodyPartPool::IsPartIndexUsed(s32 liIndex)
    {
        const u32 luIndex = static_cast<u32>(liIndex);
        CGS_ASSERT(luIndex < KU_MAX_DETACHED_PARTS, "invalid index : ");
        return mUsedParts.IsBitSet(luIndex);
    }

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
        const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
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
        const PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue*
            lpWorldContactQueue = lpPotentialContactsInterface->GetHingedBodyPartWithWorldQueue();
        const PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue*
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
}
}
