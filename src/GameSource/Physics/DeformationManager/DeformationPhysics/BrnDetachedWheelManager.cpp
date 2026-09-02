#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"

#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"   // OutputBuffer + OutUpdateRigidBody queue (walls leg 4)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags (walls leg 4 gates)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"    // rw::math::vpu::IsOrthogonal3x3 / IsNormal3x3 (DetachWheel's :95/:96 tripwires)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface::UpdateCachedObjectPosition (the real tri-cache producer)

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
// InputBuffer) still reaches an event type not homed in-tree, and is routed through the one
// remaining provisional free hook declared in this class' own header (EmitRemoveRigidBodyEvent).
// ⛔ THE SECOND HOOK IS GONE. This banner used to name `EmitUpdateTriangleCacheEvent` alongside it
// and claimed UpdateTriangleCache's InEventUpdateCachedPosition was likewise "not yet homed
// in-tree". That was false as of 2026-08-18 and the hook itself was a FABRICATED API -- retired
// 2026-08-27, see BrnDetachedWheelManager.h's banner. The event, its 32-byte element, its queue
// (mUpdateCachedPositionQueue), its AddEvent instantiation and its producer
// (InSceneUpdateInterface::UpdateCachedObjectPosition) are all committed and mounted.
//
// FLAGGED CONSTANTS: the swept-sphere update uses two asm-attested rodata constants -- the frame
// timestep KF_FRAME_TIMESTEP (flt_82095EE0 == 1/60) and the padding radius
// KF_TRIANGLE_CACHE_PADDING (flt_82004014 == 0.1, byte-verified). Both are named rodata loads, NOT
// bare immediates as this banner previously said, and NOT fabricated.
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
        // The per-frame timestep the swept RADIUS term uses. ⭐ NAMED SOURCE 2026-08-27: it is not
        // a bare "asm immediate" -- it is `lfs f31, flt_82095EE0` @0x8260EAE4 (IDA's read: 1/60).
        static const f32 KF_FRAME_TIMESTEP = 0.016666668f;

        // The padding added to the wheel's collision sphere: `lfs f30, flt_82004014` @0x8260EAD4,
        // the same rodata word CgsSceneManagerIO_SceneUpdate.h:311 already byte-verified as
        // 3D CC CC CD == 0.1f.
        static const f32 KF_TRIANGLE_CACHE_PADDING = 0.1f;

        // The wheel's triangle-cache slot base: `addi r9, r9, 0x7B` @0x8260EB74, and the SAME 123
        // PhysicalWheel::AddToScene @0x8260C540 claims and RemoveFromScene @0x825E8258 drops. The
        // three must never drift; a mismatch would silently reposition somebody else's slot.
        static const u32 KU_WHEEL_TRIANGLE_CACHE_SLOT_BASE = 0x7Bu;   // 123
    }

    // ============================================================================================
    // GetWheel (@0x825A0B10) / IsSlotUsed (@0x825A0A10) MOVED 2026-08-06 (bridge de-facade
    // wave) to the mounted slice TU BrnDetachedWheelManager_Accessors.cpp: the contact-spy
    // bridge's FixupWheelVehicleContact links against both, while THIS TU's RemoveWheel /
    // UpdateTriangleCache tail carries its own open closure. Bodies verbatim there; fold back
    // when this TU mounts.
    // ============================================================================================

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
    // DetachWheel @ 0x8260E430 (369 insns) -- bodied 2026-09-02 (deform close-out wave), the
    // detach arm of DeformableObject::UpdateWheels @0x826254C0. X360 flow, store for store:
    //   r3 this, r4 simIn, r5 = the 8-byte handling RigidBodyId (one `ld` at the call site,
    //   0x826264B4), r6 = deformable index (lhz 0x66B2), r7 = wheel index, f1 = half-height,
    //   f2 = radius, r10 = &lRenderTransform, stack = &lVehicleTransform, v1/v2 = velocities.
    //
    //   assert IsOrthogonal3x3(lVehicleTransform, 0.01)  (:95, message = the matrix, sub_821F0FC0)
    //   assert IsNormal3x3(lVehicleTransform, 0.01)      (:96, message = the matrix)
    //   liSlot = mUsedWheels.GetFirstZeroBit()           (:99, inlined -- see CgsBitArray.h)
    //   if (liSlot >= 20 /*0x8260E7A0 cmplwi 0x14*/): debug print "Run out of space for
    //       detached wheels\n" (gxMessageFilterFlags & 1) and return
    //   assert !mUsedWheels.IsBitSet(liSlot)             (:103)
    //   lBodyId.Set(entity word = high dword of the handling id (srdi 32), partIndex = wheel
    //       index (clrlwi 16 of the r7 spill), subA = deformable index (r14), subB = liSlot)
    //       -- 0x8260E8A0..0x8260E8C0
    //   maWheels[liSlot] (144*slot): PhysicalWheel::Prepare INLINED at 0x8260E8FC..0x8260E930:
    //       +0x70 id (std), +0x00..+0x30 lRenderTransform (four lvx128/stvx128 off r19),
    //       +0x78 half-height (f30 = f1), +0x7C radius (f29 = f2), +0x80/+0x81 = 0 (mbFrozen,
    //       mbAddedToScene), +0x60 mLinearVelocity = zero (stvx128 v0(zero), r31, r6(0x60))
    //   maWheelOwnerType[liSlot] = entity word >> 24   (stwx r5, (slot+0x2D0)*4, this)
    //   mUsedWheels.SetBit(liSlot)                        (the 64-bit `or` on +0xB90)
    //   maWheels[liSlot].AddToSim(simIn, lVehicleTransform, v1, v2)   (0x8260E9F0)
    // The DWARF prototype (BrnDetachedWheelManager.cpp:93) takes both matrices by const&; the
    // frozen header here passes them by value -- same bytes, kept as declared.
    // ============================================================================================
    void DetachedWheelManager::DetachWheel(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                           RigidBodyId lHandlingBodyId, u16 lu16DeformableIndex, s32 liWheelIndex,
                                           f32 lfHalfHeight, f32 lfRadius,
                                           Matrix44Affine lRenderTransform, Matrix44Affine lVehicleTransform,
                                           Vector3 lInitialLinearVelocity, Vector3 lInitialAngularVelocity)
    {
        CGS_ASSERT(rw::math::vpu::IsOrthogonal3x3(lVehicleTransform, 0.01f),
                   "IsOrthogonal3x3( lVehicleTransform, 0.01f )");   // :95 (message = the matrix)
        CGS_ASSERT(rw::math::vpu::IsNormal3x3(lVehicleTransform, 0.01f),
                   "IsNormal3x3( lVehicleTransform, 0.01f )");       // :96 (message = the matrix)

        const s32 liSlot = mUsedWheels.GetFirstZeroBit();   // :99
        if (liSlot < 0 || liSlot >= KI_MAX_DETACHED_WHEELS)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Run out of space for detached wheels\n";
            return;
        }
        CGS_ASSERT(!mUsedWheels.IsBitSet(static_cast<u32>(liSlot)), "!mUsedWheels.IsBitSet(liSlot)");   // :103

        // :106 -- the packed wheel id. The owning-vehicle word is the entity half of the handling
        // id (its HIGH dword -- `srdi r11, r15, 32 ; clrlwi r31, r11, 0`).
        const u32 luVehicleEntityWord = static_cast<u32>(static_cast<u64>(lHandlingBodyId) >> 32);
        BurnoutWheelBodyID lBodyId;
        lBodyId.Set(luVehicleEntityWord, static_cast<u16>(liWheelIndex), lu16DeformableIndex,
                    static_cast<u16>(liSlot));

        PhysicalWheel& lrWheel = maWheels[liSlot];
        lrWheel.Prepare(lBodyId, lfRadius, lfHalfHeight, lRenderTransform);

        maWheelOwnerType[liSlot] = static_cast<BrnWorld::EEntityTypeID>(luVehicleEntityWord >> 24);   // +0xB40
        mUsedWheels.SetBit(static_cast<u32>(liSlot));                                                 // +0xB90

        lrWheel.AddToSim(lpSimInput, lVehicleTransform, lInitialLinearVelocity, lInitialAngularVelocity);
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
        // The handling vehicle's owner entity type -- the owner byte of the entity word packed in
        // the id, the only field gate-1 compares against.
        // ⭐ 2026-08-11 (handle-widening wave): the asm's own shape is the proof this is a 64-bit
        // handle and the byte lives at bits 56..63. It builds `v13 = (a3<<32)|1` -- the entity word
        // promoted into the HIGH dword, exactly like ProcessCollisionEvents' `extldi r,r,64,32` --
        // and then tests `HIBYTE(HIDWORD(v13))`, i.e. the top byte OF THE HIGH DWORD. That is
        // precisely GetEntityIDOwner() (`srdi 32` then `srwi 24`), so it is spelled as that instead
        // of a hand-rolled shift on a 4-byte stand-in. Same bits, one place.
        const u32 luHandlingOwner = lHandlingBodyId.GetEntityIDOwner();

        for (s32 liSlot = mUsedWheels.GetFirstNonZeroBit();
             liSlot != CgsContainers::BitArray<KI_MAX_DETACHED_WHEELS>::KI_INVALID_BITINDEX;
             liSlot = mUsedWheels.GetNextNonZeroBit(liSlot))
        {
            // (1) owner-type table match (maWheelOwnerType is at this+2880 -- v5[slot + 720]).
            if (static_cast<u32>(maWheelOwnerType[liSlot]) == luHandlingOwner)
            {
                // ⛔⛔⛔ GATE 2 RE-DECODED 2026-09-02 (deform close-out wave) -- THIS GATE COULD
                // NEVER PASS, so RemoveVehicleWheels removed NOTHING, and every wheel a car shed
                // stayed in this manager's slot table for the rest of the session.
                //
                // The note that stood here said, emphatically, "the asm's gate-2 is a test over the
                // WHEEL's OWN packed word alone -- the handling id does NOT appear in it ... (an
                // earlier transcription wrongly injected the handling entity index into gate-2 --
                // corrected here)". THE "CORRECTION" WAS THE REGRESSION: the earlier transcription
                // was right. Read the asm (0x82629ABC..0x82629AE8), with r14 established at
                // 0x82629A60 as `srdi r11, r6, 32` == the HANDLING id's entity word:
                //     0x82629AC0  srwi r10, r14, 10      ; HANDLING entityWord >> 10
                //     0x82629AC4  add  r11, r31, r11     ; slot*9 ...
                //     0x82629AC8  slwi r11, r11, 4       ; ... *16 == slot*144, the record stride
                //     0x82629AD0  ld   r11, 0x70(r11)    ; the wheel record's 8-byte mWheelBodyId
                //     0x82629AD4  srdi r11, r11, 32      ; its entity word
                //     0x82629AD8  srwi r11, r11, 10      ; WHEEL entityWord >> 10
                //     0x82629ADC  xor  r11, r11, r10     ; <- XOR WITH THE HANDLING INDEX, not itself
                //     0x82629AE0  clrlwi r11, r11, 18    ; keep the low 14 bits
                //     0x82629AE4  cmplwi cr6, r11, 0 ; bne -> skip
                // i.e. "does this slot's wheel belong to the vehicle being reset" -- the ordinary
                // 14-bit entity-index compare, written as an XOR. The self-XOR that stood here,
                // `(w>>10) ^ (w>>16)`, is a property of one word's own bits and is zero only for a
                // handful of accidental patterns.
                //
                // MEASURED, and this is what sent me back to the asm: with ResetDeformation's
                // `bl RemoveVehicleWheels` finally emitted (4625e6a0), the [wheelreset] witness
                // still showed `rec 1` for the reset wheel -- the record survived the reset in BOTH
                // the fixed and the control build (runs wrst_A2 / wrst_CTL). The call was reaching
                // a gate that cannot fire.
                const u32 luWheelWord = maWheels[liSlot].GetWheelBodyId().muEntityWord;
                const u32 luHandlingWord = static_cast<u32>(static_cast<u64>(lHandlingBodyId) >> 32);
                if ((((luWheelWord >> 10) ^ (luHandlingWord >> 10)) & 0x3FFFu) == 0u)
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
    //       timestep = flt_82095EE0 = 0.016666668 (1/60)     [f31, loaded @0x8260EAE4]
    //       v = mLinearVelocity (+0x60) ; speed = |v| ; sweptDistance = speed * timestep
    //       UpdateCachedObjectPosition( (handle & 0xFF) + 123,
    //                                   mRenderTransform.wAxis,           <-- NOT swept
    //                                   mfRadius + 0.1 + sweptDistance )
    //
    // The asm's VMX block is the standard normalise: `vmsum3fp128` is the velocity dot product
    // (speed^2), the `vrsqrtefp` + two `vnmsubfp/vmaddfp` steps are the Newton-Raphson
    // reciprocal-sqrt refinement that converges to 1/|v| (modelled as the exact divide), and
    // `vsel ... vcmpeqfp` guards the |v|==0 case. The 0.1 is f30 = flt_82004014 (byte-read
    // 3D CC CC CD), loaded @0x8260EAD4.
    //
    // ⛔⛔ TWO CORRECTIONS 2026-08-27 (detached-part collision wave):
    //
    //  (1) THE POSITION IS NOT SWEPT, AND THE SWEEP THIS BODY USED TO APPLY WAS FABRICATED.
    //      The banner above used to say "position = render-transform translation displaced by
    //      (v normalised) * sweptDistance", and the code did exactly that. Read the stores:
    //        0x8260EB50  lvx128   v9, r11, 0x30        ; v9 = mRenderTransform.wAxis
    //        0x8260EB80  vrlimi128 v9, v12, 1, 0       ; v9 = { pos.xyz, 0 }
    //        0x8260EBAC  stvx128  v9, r1+var_100       ; [0x80] = that, UNMODIFIED
    //        0x8260EBB0  vmr      v9, v12              ; v9 is now the ZERO vsel arm -- the
    //                                                  ;   position register is DEAD from here
    //        0x8260EC18  stfs     f0, r1+var_F4        ; [0x8C] == the W LANE of [0x80] = radius
    //        0x8260EC20  lvx128   v0, r1+var_100       ; reload [0x80]
    //        0x8260EC24  stvx128  v0, event+0x10       ; -> mNewPositionAndRadius
    //      The swept distance NEVER touches xyz. It goes into the RADIUS only (0x8260EBF4
    //      `fadds f0, f0, f30` then 0x8260EC14 `fadds f0, f0, f13`), i.e. the console inflates the
    //      cache sphere to cover a frame of travel and leaves the sphere CENTRED on the wheel.
    //      The displaced centre was an invention that moved the cached region off the wheel by up
    //      to one frame of travel in the direction of motion -- in the WRONG place at both ends.
    //
    //  (2) The emission is a real producer, not a hook -- see the header banner for why
    //      EmitUpdateTriangleCacheEvent was fabricated. `UpdateCachedObjectPosition` takes an
    //      s32 CACHE SLOT, not a volume-instance id: `ld r9, 0x70(r11) ; clrlwi r9,r9,24 ;
    //      addi r9,r9,0x7B ; clrlwi r9,r9,16` @0x8260EB48..0x8260EB84 -- (handle & 0xFF) + 123,
    //      the SAME slot PhysicalWheel::AddToScene claims and RemoveFromScene drops.
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

            // Velocity-based swept distance over one frame. It feeds the RADIUS only (see the
            // banner's correction (1)); the |v|==0 vsel arm makes it exactly 0 for a still wheel.
            const Vector3 lvVelocity = lpWheel->GetLinearVelocity();
            const f32 lfSpeedSquared = lvVelocity.x * lvVelocity.x
                                     + lvVelocity.y * lvVelocity.y
                                     + lvVelocity.z * lvVelocity.z;   // vmsum3fp128
            const f32 lfSpeed = std::sqrt(lfSpeedSquared);            // 1/vrsqrtefp refined -> |v|
            const f32 lfSweptDistance = lfSpeed * KF_FRAME_TIMESTEP;

            // The cache sphere is centred on the wheel's own render translation, w lane cleared
            // (`vrlimi128 v9, v12, 1, 0`) because that lane carries the radius in the event.
            const Matrix44Affine* lpRenderTransform = lpWheel->GetRenderTransform();
            Vector3 lvPosition = lpRenderTransform->wAxis;
            lvPosition.w = 0.0f;

            // Padded collision sphere radius: wheel radius + the 0.1 cache padding + the swept term
            // (`fadds f0, f0, f30` then `fadds f0, f0, f13`).
            const f32 lfSphereRadius =
                lpWheel->GetRadius() + KF_TRIANGLE_CACHE_PADDING + lfSweptDistance;

            // The wheel's triangle-cache slot: (packed handle & 0xFF) + 123.
            const s32 liCacheSlot = static_cast<s32>(
                (lpWheel->GetVolumeInstanceId().muId & 0xFFull) + KU_WHEEL_TRIANGLE_CACHE_SLOT_BASE);

            lpSceneUpdateInterface->UpdateCachedObjectPosition(liCacheSlot, lvPosition, lfSphereRadius);
        }
    }

    // =============================================================================================
    // UpdatePostPhysics @0x82627150 (144) -- ⭐ 2026-08-14 (walls leg 4, FILTERED WALK + inner
    // gate). Drain the sim output's updated-rigid-body queue; every event whose entity owner byte
    // is a DETACHED wheel (9 == racecar wheel, 10 == traffic wheel; slot index asserted < 20)
    // updates that wheel slot's transform/velocity from the event. The FILTER is real; the inner
    // per-wheel update is a LOG-ONCE GATE (dead until a wheel detaches -- 0 detached wheels on
    // the junkyard path).
    // =============================================================================================
    void DetachedWheelManager::UpdatePostPhysics(
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutUpdateRigidBodyQueue* lpQueue =
            lpSimOutput->GetUpdateRigidBodyQueue();

        const s32 liNumEvents = lpQueue->GetLength();
        for ( s32 li = 0; li < liNumEvents; ++li )
        {
            const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody& lrEvent =
                lpQueue->GetEvent(li);

            // owner byte = bits 56..63 of the event's 8-byte rigid-body handle (entity word high).
            const u32 luOwner = static_cast<u32>(lrEvent.mID >> 56);
            if ( luOwner != 9u && luOwner != 10u )   // detached racecar / traffic wheel only
            {
                continue;
            }

            // ---- inner update, LANDED 2026-09-02 (deform close-out wave) from @0x82627150 ----
            //   0x826271E4  ld r11, 0(event)         ; the 8-byte packed BurnoutWheelBodyID
            //   0x826271E8  srdi 32 ; srwi 24        ; owner byte == 9 (racecar wheel) or 10 (traffic)
            //   0x82627200  clrlwi r29, r11, 16      ; lBodyId.GetPartPoolIndex() == the low 16 bits (muSubB)
            //   0x82627204  cmplwi r29, 0x14         ; the BitArray "invalid index : N < 20" tripwire (CgsBitArray.h:203)
            //   0x82627310  IsBitSet(slot)           ; assert "mUsedWheels.IsBitSet(lBodyId.GetPartPoolIndex())" (:150)
            //   0x82627360  r3 = this + 144*slot ; r4 = event ; r5 = scene  ; bl PhysicalWheel::Update
            // (DWARF BrnDetachedWheelManager.cpp:148 names the local lBodyId.)
            const u32 luSlot = static_cast<u32>(lrEvent.mID & 0xFFFFu);
            CGS_ASSERT(luSlot < static_cast<u32>(KI_MAX_DETACHED_WHEELS), "invalid index : lBodyId.GetPartPoolIndex() < 20");
            CGS_ASSERT(mUsedWheels.IsBitSet(luSlot), "mUsedWheels.IsBitSet(lBodyId.GetPartPoolIndex())");
            if ( luSlot < static_cast<u32>(KI_MAX_DETACHED_WHEELS) )
            {
                // FLAG (type plumbing, same shape as PhysicalBodyPartPool -> PhysicalBodyPart::Update):
                // PhysicalWheel::Update is declared on the Deformation-namespace forward declaration
                // of OutUpdateRigidBody (BrnPhysicalWheel.h:57) and reads the CgsPhysics event by
                // byte offset; the queue element IS that event, so the pointer is re-labelled here.
                maWheels[luSlot].Update(reinterpret_cast<const OutUpdateRigidBody*>(&lrEvent), lpSceneInterface);
            }
        }
    }

    // =============================================================================================
    // ⛔⛔ EmitUpdateTriangleCacheEvent DELETED 2026-08-27 -- a FABRICATED API. See the header's
    // banner for the three-way confirmation (no such symbol in any build; the real call is
    // InSceneUpdateInterface::UpdateCachedObjectPosition -> mUpdateCachedPositionQueue; and its
    // signature took a 64-bit volume-instance id where the event's field is an s32 cache slot).
    // Both its call sites now go straight to the real producer.
    //
    // The remaining provisional hook (declared FLAG-provisional in the header) -- ⚠️ LOG-ONCE
    // GATE 2026-08-14 (walls leg 4). Dead until a wheel detaches; the real body is the
    // sim input buffer's InRemoveRigidBody queue AddEvent.
    // =============================================================================================
    void EmitRemoveRigidBodyEvent(CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/,
                                  u32 /*luWheelEntityWord*/)
    {
        static bool sbLoggedEmitRemoveGate = false;
        if ( !sbLoggedEmitRemoveGate )
        {
            sbLoggedEmitRemoveGate = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: EmitRemoveRigidBodyEvent reached but not reconstructed "
                       "[FLAG PC boot gate]\n";
        }
    }

}
}
