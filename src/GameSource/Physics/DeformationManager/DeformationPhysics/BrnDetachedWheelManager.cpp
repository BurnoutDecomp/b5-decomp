#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>   // std::sqrt (the vrsqrtefp velocity-magnitude refinement converges to this)

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.cpp
//
// BrnPhysics::Deformation::DetachedWheelManager -- the pool of detached wheels. This file
// owns the slot-pool plumbing reconstructed store-for-store from the X360 ARTIST.XEX:
//
//   GetWheel (raw slot)     @ 0x825A0B10  (dossier "Get") -- assert the slot is used, return
//                                          &maWheels[slot] (asm `144*slot + this`).
//   IsSlotUsed              @ 0x825A0A10  -- bounds assert (inlined CgsBitArray "invalid index"),
//                                          return mUsedWheels.IsBitSet(slot).
//   RemoveWheel             @ 0x82615778  -- assert slot in range + used, queue the rigid-body
//                                          removal, remove the volume instance from the scene if
//                                          present, clear the slot's used bit.
//   RemoveVehicleWheels     @ 0x826299E0  -- walk the used set; for every slot whose owner-type +
//                                          packed entity matches the handling vehicle, RemoveWheel it.
//   UpdateTriangleCache     @ 0x8260E9F8  -- walk the used set; for every in-scene wheel, emit a
//                                          swept-sphere triangle-cache position/radius update.
//
// X360 LAYOUT NOTE (offset authority, from the frozen header's SLOT-MATH ANCHOR): the per-wheel
// record stride is 144 bytes on the console (`144*slot + this` == &maWheels[slot]); the owner-type
// word table is at this+2880 (`v5[slot + 720]`); the used-set BitArray is at this+2960
// (`8*(slot>>6) + this + 2960`). Host pointer widening means the absolute byte offsets do not all
// reproduce on the 64-bit host, so indexing here is BY NAME (maWheels[slot], maWheelOwnerType[slot],
// mUsedWheels) -- value-identical to the asm's strided indexing.
//
// BIT-WALK NOTE: every per-frame driver / removal scan below walks mUsedWheels with the X360
// lowest-set-bit idiom (`field*64 - clz64(field & -field) + 63`, re-seeded per 64-bit field, with
// the inlined "invalid index : N < 20" CgsBitArray.h:203 / :241 bounds tripwires). That idiom is
// value-identical to CgsContainers::BitArray<20>::GetFirstNonZeroBit / GetNextNonZeroBit, which the
// X360 build inlined at each call site; the walks below are expressed through those container
// methods (the canonical home), reproducing the same visited-slot sequence as the asm.
//
// ASSERTS: the X360 bounds/used tripwires (BeginAssert/FireAssert/EndAssert triples, including the
// inlined CgsBitArray "invalid index : N < 20" StrStream diagnostics) are NON-GATING -- execution
// continues past a failed assert exactly as the asm does. They are modelled as CGS_ASSERT and the
// lookup / store that follows runs regardless. The original source file paths/line numbers are
// dropped (the macro supplies __FILE__/__LINE__).
//
// UN-HOMED DEEP IO: RemoveWheel's rigid-body-removal AddEvent (InRemoveRigidBody on the sim
// InputBuffer) and UpdateTriangleCache's InEventUpdateCachedPosition (TriangleCacheManagerIO on the
// scene update interface) reach event types not yet homed in-tree. They are routed through the two
// provisional free hooks declared in this class' own header (EmitRemoveRigidBodyEvent /
// EmitUpdateTriangleCacheEvent), exactly as the sibling BrnPhysicalBodyPart slice routes its
// un-homed tri-cache removal through RemoveTriangleCacheSlot. The owning TUs emit the real bodies.
//
// FLAGGED CONSTANTS: the swept-sphere update uses two asm-attested rodata constants -- the frame
// timestep KF_FRAME_TIMESTEP == 0.016666668 (1/60, the asm's `v50[0] = 0.016666668`) and the
// padding radius KF_TRIANGLE_CACHE_PADDING == 0.1 (the asm's `*&v45 = v53 + 0.1`). Both are read
// directly from the asm immediates (NOT fabricated).
//
// Callers (X360 xrefs): GetWheel/IsSlotUsed <- DeformationManager + PhysicsModule (contact bridging);
// RemoveVehicleWheels <- DeformableObject::ResetDeformation / ::Release; RemoveWheel <-
// RemoveVehicleWheels; UpdateTriangleCache <- DeformationManager::UpdateTriangleCache.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // ----- asm-attested swept-sphere constants (UpdateTriangleCache) ----------------------------
    namespace
    {
        // The per-frame timestep the swept-sphere position uses (asm immediate `v50[0] = 0.016666668`).
        static const f32 KF_FRAME_TIMESTEP = 0.016666668f;

        // The padding added to the wheel's collision sphere (asm immediate `v53 + 0.1`).
        static const f32 KF_TRIANGLE_CACHE_PADDING = 0.1f;
    }

    // ============================================================================================
    // GetWheel (raw pool slot) @ 0x825A0B10  (dossier "Get")
    //
    //   if ( !IsSlotUsed(luSlot) ) assert "IsSlotUsed(luSlot)"   (non-gating)
    //   return &maWheels[luSlot]                                 (asm `144*luSlot + this`)
    //
    // The frozen header types the return as const PhysicalWheel* -- the asm's `144*a2 + a1` is the
    // address of the slot's PhysicalWheel record (144 == the console stride; here &maWheels[slot]).
    // ============================================================================================
    const PhysicalWheel* DetachedWheelManager::GetWheel(u16 lu16Slot)
    {
        CGS_ASSERT(IsSlotUsed(lu16Slot), "IsSlotUsed(luSlot)");
        return &maWheels[lu16Slot];
    }

    // ============================================================================================
    // IsSlotUsed @ 0x825A0A10
    //
    //   if ( luSlot >= 20 ) assert "invalid index : 20 < 20"-shaped CgsBitArray.h:203 tripwire
    //                       (non-gating; the StrStream "invalid index : N < 20" diagnostic)
    //   return ((1 << (luSlot & 0x3F)) & mUsedWheels.field[luSlot>>6]) != 0   == mUsedWheels.IsBitSet
    //
    // The Hex-Rays tail `return ((*&a1 << SBYTE3(v14)) & v14) != 0` is the inlined IsBitSet bit-test
    // on the 64-bit field at +2960 (`*&v4[2*(luSlot>>6) + 740]`). Expressed through the canonical
    // BitArray::IsBitSet, which produces the identical bit.
    // ============================================================================================
    bool DetachedWheelManager::IsSlotUsed(u16 lu16Slot)
    {
        CGS_ASSERT(lu16Slot < KI_MAX_DETACHED_WHEELS, "invalid index : ");
        return mUsedWheels.IsBitSet(static_cast<u32>(lu16Slot));
    }

    // ============================================================================================
    // RemoveWheel @ 0x82615778
    //
    //   liSlot < 20 tripwire ("liSlot < KI_MAX_DETACHED_WHEELS")                       (non-gating)
    //   liSlot < 20 tripwire (inlined CgsBitArray "invalid index : N < 20", :203)      (non-gating)
    //   used-bit tripwire ("mUsedWheels.IsBitSet(liSlot)")                             (non-gating)
    //   v20 = &maWheels[liSlot]
    //   v26 = mWheelBodyId (the 8-byte packed id at +112) -> queue the rigid-body removal:
    //         v21 = InRemoveRigidBody queue of lpSimInput ; InRemoveRigidBody_::AddEvent(v21, &v26)
    //   if ( mbAddedToScene (+129) ) PhysicalWheel::RemoveFromScene(v20, lpSceneInterface)
    //   liSlot < 20 tripwire (inlined CgsBitArray "luIndex < NUMBITS", :241)           (non-gating)
    //   mUsedWheels.field[liSlot>>6] &= ~(1 << (liSlot & 0x3F))   == mUsedWheels.UnSetBit(liSlot)
    //
    // The asm reads the wheel record at +112 as an 8-byte id and feeds it to AddEvent; the scene
    // entity the removal keys on is the id's entity word (mWheelBodyId.muEntityWord), routed through
    // the provisional EmitRemoveRigidBodyEvent hook. The two distinct CgsBitArray messages (:203
    // "invalid index" before the lookup, :241 "luIndex < NUMBITS" before the clear) are reproduced
    // in the asm's order.
    // ============================================================================================
    void DetachedWheelManager::RemoveWheel(s32 liSlot,
                                           CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                           CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        CGS_ASSERT(liSlot < KI_MAX_DETACHED_WHEELS, "liSlot < KI_MAX_DETACHED_WHEELS");
        CGS_ASSERT(static_cast<u32>(liSlot) < KI_MAX_DETACHED_WHEELS, "invalid index : ");
        CGS_ASSERT(mUsedWheels.IsBitSet(static_cast<u32>(liSlot)), "mUsedWheels.IsBitSet(liSlot)");

        PhysicalWheel* lpWheel = &maWheels[liSlot];

        // Queue the wheel's rigid body for removal from the sim. The asm: v21 = sub_825BCF58(lpSimInput)
        // (the InputBuffer's InRemoveRigidBody queue), InRemoveRigidBody_::AddEvent(v21, &mWheelBodyId)
        // -- it passes the WHOLE 8-byte packed mWheelBodyId BY ADDRESS (muEntityWord + muSubA + muSubB).
        // FLAG: the provisional EmitRemoveRigidBodyEvent hook only forwards the 32-bit muEntityWord,
        // silently dropping muSubA/muSubB; the real owning IO TU must pass the full packed id.
        EmitRemoveRigidBodyEvent(lpSimInput, lpWheel->GetWheelBodyId().muEntityWord);

        // if ( mbAddedToScene ) remove the wheel's volume instance from the scene.
        if (lpWheel->IsAddedToScene())
        {
            lpWheel->RemoveFromScene(lpSceneInterface);
        }

        // Clear the slot's used bit (the :241 "luIndex < NUMBITS" tripwire fires inline, non-gating).
        CGS_ASSERT(static_cast<u32>(liSlot) < KI_MAX_DETACHED_WHEELS, "luIndex < NUMBITS");
        mUsedWheels.UnSetBit(static_cast<u32>(liSlot));
    }

    // ============================================================================================
    // RemoveVehicleWheels @ 0x826299E0
    //
    // Walk every used slot (GetFirstNonZeroBit / GetNextNonZeroBit over mUsedWheels, with the inlined
    // "invalid index : N < 20" bounds tripwires) and RemoveWheel each slot that passes the asm's
    // two-condition gate. NOTE: only gate-1 references the handling id; gate-2 tests the WHEEL's own
    // packed word in isolation (an earlier transcription wrongly injected the handling entity index
    // into gate-2 -- corrected here).
    //
    //   (1) maWheelOwnerType[slot] == (lHandlingBodyId >> 24)    -- the slot's stored owner entity
    //       type equals the handling id's owner byte (v13 = (a3<<32)|1; the compare uses
    //       HIBYTE(HIDWORD(v13)) == (lHandlingBodyId>>24)&0xFF). This is the ONLY use of the
    //       handling id in the gate.
    //   (2) `v14 = HIDWORD(*&v5[36*slot+28])` is the wheel record's own mWheelBodyId.muEntityWord
    //       (the +112 entity word -- on big-endian X360 the high dword of the 8-byte mWheelBodyId
    //       load is its first 4 bytes). The test is `(((v14>>10) ^ WORD1(v14)) & 0x3FFF) == 0`,
    //       i.e. ((word>>10) ^ (word>>16)) masked to 14 bits == 0, computed entirely from the
    //       wheel's own word. The handling id (a3) does NOT participate.
    //
    // RemoveWheel is then called with (slot, lpSimInput, lpSceneInterface). The Hex-Rays
    // RemoveWheel(v5, v12, a2, a3, a5, HIDWORD(v9)) call carries the register-spilled scene-interface
    // pointer; the frozen header's RemoveWheel(liSlot, lpSimInput, lpSceneInterface) is authoritative.
    //
    // ARG NOTE (FLAG): the frozen header signature is (lpSimInput, lpSceneInterface, lHandlingBodyId);
    // the de-inlined call site is RemoveVehicleWheels(lpWheelMgr, lpInput, mHandlingBodyID,
    // mGlobalEntityId). Only gate-1's owner-byte compare consumes the handling id; gate-2 is purely a
    // property of each wheel's own packed id, matching the asm.
    // ============================================================================================
    void DetachedWheelManager::RemoveVehicleWheels(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                                   RigidBodyId lHandlingBodyId)
    {
        // The handling vehicle's owner entity type -- the high byte of the packed id, the only
        // field gate-1 compares against (the asm builds v13 = (a3<<32)|1 and tests against
        // HIBYTE(HIDWORD(v13)) == (lHandlingBodyId>>24)&0xFF).
        const u32 luHandlingOwner = (lHandlingBodyId.muValue >> 24) & 0xFFu;

        for (s32 liSlot = mUsedWheels.GetFirstNonZeroBit();
             liSlot != CgsContainers::BitArray<KI_MAX_DETACHED_WHEELS>::KI_INVALID_BITINDEX;
             liSlot = mUsedWheels.GetNextNonZeroBit(liSlot))
        {
            // (1) owner-type table match (maWheelOwnerType is at this+2880 -- v5[slot + 720]).
            if (static_cast<u32>(maWheelOwnerType[liSlot]) == luHandlingOwner)
            {
                // (2) the asm's gate-2 is a test over the WHEEL's OWN packed word alone -- the
                // handling id does NOT appear in it. v14 = HIDWORD(*&v5[36*slot+28]) is the wheel
                // record's mWheelBodyId.muEntityWord (the +112 entity word; on big-endian X360 the
                // high dword of the 8-byte mWheelBodyId load is the first 4 bytes). The asm test is
                // `(((v14>>10) ^ WORD1(v14)) & 0x3FFF) == 0`, i.e. the low-14 of (word>>10) (the
                // packed entityIndex field) XOR the low-14 of (word>>16), masked 0x3FFF, equal to
                // zero -- transcribed below store-for-store.
                const u32 luWheelWord = maWheels[liSlot].GetWheelBodyId().muEntityWord;
                if ((((luWheelWord >> 10) ^ (luWheelWord >> 16)) & 0x3FFFu) == 0u)
                {
                    RemoveWheel(liSlot, lpSimInput, lpSceneInterface);
                }
            }
        }
    }

    // ============================================================================================
    // UpdateTriangleCache @ 0x8260E9F8
    //
    //   if ( lpSceneUpdateInterface == NULL ) assert "lpSceneUpdateInterface != NULL"  (non-gating)
    //   walk every used slot (GetFirstNonZeroBit / GetNextNonZeroBit over mUsedWheels):
    //     lpWheel = &maWheels[slot]
    //     if ( mbAddedToScene (+129) ):
    //       timestep = 0.016666668 (1/60)
    //       v = mLinearVelocity ; speed = |v| ; sweptDistance = speed * timestep
    //       sphereRadius = mfRadius + 0.1 + sweptDistance
    //       position = render-transform translation displaced by (v normalised) * sweptDistance
    //       emit InEventUpdateCachedPosition { volumeInstanceId, position, sphereRadius }
    //         (SetRadius + UpdateCachedObjectPosition + AddEvent on the tri-cache queue)
    //
    // The asm's VMX block is the standard normalise-and-sweep: `vmsum3fp128` is the velocity dot
    // product (speed^2), the `vrsqrtefp` + two `vnmsubfp/vmaddfp` steps are the Newton-Raphson
    // reciprocal-sqrt refinement that converges to 1/|v| (modelled as the exact divide), and
    // `vsel ... vcmpeqfp` guards the |v|==0 case (a zero-velocity wheel sweeps nowhere). The
    // padded radius is `mfRadius + 0.1 + sweptDistance`; the 0.1 is the asm immediate (`v53 + 0.1`).
    // The un-homed InEventUpdateCachedPosition build is routed through the provisional
    // EmitUpdateTriangleCacheEvent hook (volume instance id + swept world position + padded radius).
    // ============================================================================================
    void DetachedWheelManager::UpdateTriangleCache(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface)
    {
        CGS_ASSERT(lpSceneUpdateInterface != 0, "lpSceneUpdateInterface != NULL");

        for (s32 liSlot = mUsedWheels.GetFirstNonZeroBit();
             liSlot != CgsContainers::BitArray<KI_MAX_DETACHED_WHEELS>::KI_INVALID_BITINDEX;
             liSlot = mUsedWheels.GetNextNonZeroBit(liSlot))
        {
            const PhysicalWheel* lpWheel = &maWheels[liSlot];

            // Only wheels whose volume instance is currently in the scene get a cache update
            // (the asm's `if ( *(_R11 + 129) )` == mbAddedToScene).
            if (!lpWheel->IsAddedToScene())
            {
                continue;
            }

            // Velocity-based swept distance over one frame.
            const Vector3 lvVelocity = lpWheel->GetLinearVelocity();
            const f32 lfSpeedSquared = lvVelocity.x * lvVelocity.x
                                     + lvVelocity.y * lvVelocity.y
                                     + lvVelocity.z * lvVelocity.z;   // vmsum3fp128
            const f32 lfSpeed = std::sqrt(lfSpeedSquared);            // 1/vrsqrtefp refined -> |v|
            const f32 lfSweptDistance = lfSpeed * KF_FRAME_TIMESTEP;

            // Swept world position: the render-transform translation displaced along the velocity
            // direction by the swept distance (the |v|==0 guard leaves it at the translation).
            const Matrix44Affine* lpRenderTransform = lpWheel->GetRenderTransform();
            Vector3 lvSweptPosition = lpRenderTransform->wAxis;
            if (lfSpeed != 0.0f)
            {
                const f32 lfInvSpeed = 1.0f / lfSpeed;               // vsel(1/|v|, 0, |v|==0)
                lvSweptPosition.x += lvVelocity.x * lfInvSpeed * lfSweptDistance;
                lvSweptPosition.y += lvVelocity.y * lfInvSpeed * lfSweptDistance;
                lvSweptPosition.z += lvVelocity.z * lfInvSpeed * lfSweptDistance;
            }

            // Padded collision sphere radius: wheel radius + the 0.1 cache padding + the swept term
            // (the asm's `v52 = *(_R11 + 31) + v54`, with v54 carrying the `+ 0.1` and the sweep).
            const f32 lfSphereRadius = lpWheel->GetRadius() + KF_TRIANGLE_CACHE_PADDING + lfSweptDistance;

            EmitUpdateTriangleCacheEvent(lpSceneUpdateInterface,
                                         lpWheel->GetVolumeInstanceId().muId,
                                         lvSweptPosition,
                                         lfSphereRadius);
        }
    }
}
}
