#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"

#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue / BaseEventQueue (GetLength / GetEvent)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"   // the REAL sim OutputBuffer (walls leg 4: local model retired)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // CgsPhysics::PhysicsSimulationIO::Event (OutUpdateRigidBody base)
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                         // rw::math::vpu::MagnitudeSquared (transform validation tripwires)

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.cpp
//
// BrnPhysics::Deformation::DetachedPartManager -- the thin owner over the embedded
// 50-slot PhysicalBodyPartPool (mPartPool). Reconstructed store-for-store from the X360
// ARTIST.XEX (big-endian). This file bodies the two functions this group owns:
//
//   MakePartPhysical (X360 "MakePar")  @ 0x82626E30 -- promote a freshly shed body part
//       into the physical pool.
//   UpdatePostPhysics                  @ 0x8260E118 -- advance the pool after the physics
//       step (forward each deformable-part rigid-body update, resolve the still-joined
//       parts' contacts, recompute one bounding box).
//
// MODELLED-vs-asm conventions (identical to the committed sibling slices
// BrnPhysicalBodyPart.cpp / BrnPhysicalBodyPartPool.cpp):
//
//  * VMX128 vector math is modelled lane-by-lane in scalar f32. A `vmsum3fp128` is the xyz
//    dot product; a `vcmpgtfp` is the per-lane greater-than the asm uses for its finite /
//    orthonormal tripwires.
//
//  * Asserts are NON-GATING tripwires: a failed CGS_ASSERT runs Begin/Fire/End and
//    execution CONTINUES past it, exactly as the X360 BeginAssert/FireAssert/EndAssert
//    triple does (no early-out). The assert message strings are the asm's FireAssert
//    strings (the original source paths / line numbers are stripped per project rule).
//
//  * Un-homed callees are called via their declared (DWARF-authoritative) signatures.
//    CgsPhysics::PhysicsSimulationIO::{OutputBuffer, OutUpdateRigidBody} are NOT homed in
//    tree yet (only forward-declared by the frozen header); the minimal declarations this
//    TU needs to read the post-physics rigid-body-update queue are provided locally below
//    (declare-only is sufficient under cl /c). The pool itself (PhysicalBodyPartPool) and
//    PhysicalBodyPart are homed -- they are called by name.
// ============================================================================

// ---------------------------------------------------------------------------------------
// Un-homed sim-output declarations the post-physics pass calls through. These mirror the
// DWARF CgsPhysicsSimulationModuleIO.h types (OutUpdateRigidBody @ :356, OutputBuffer @ :684,
// the OutUpdateRigidBodyQueue typedef @ :361, GetTimeStepUsed @ :704, GetUpdateRigidBodyQueue
// @ :715). DECLARE-ONLY: the bodies live in the (not-yet-homed) sim-output TU; this TU only
// needs the signatures to forward the queue. Adopt the real homed types additively when they
// land. FLAG: forward-declared, signatures DWARF-authoritative.
// ---------------------------------------------------------------------------------------
namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    // OutUpdateRigidBody is now HOMED in CgsPhysicsSimulationIO_Events.h (included above) as an
    // opaque, 16-byte-aligned 192-byte span -- the element STRIDE is X360-attested off
    // BaseEventQueue<OutUpdateRigidBody>::AddEvent @ 0x828A66F8 (`slwi r9,r11,1; add; slwi r11,r11,6`
    // == miLength*192). The earlier local placeholder (a best-effort 176-byte span, stride NOT
    // attested) is removed in favour of that homed type; this TU's only observed read off the event
    // is the EntityId owner byte at +4 (well within 192), and the forwarded event pointer is
    // re-spelled to the deformation-side BrnPhysics::Deformation::OutUpdateRigidBody the pool's
    // UpdatePart is declared over (the cast at the forward point bridges the two spellings).

    // ⭐ walls leg 4: the LOCAL OutputBuffer model that stood here is RETIRED -- the REAL
    // CgsPhysicsSimulationModuleIO.h OutputBuffer is homed and mounted (GetTimeStepUsed +
    // GetUpdateRigidBodyQueue both bodied in its own TU); the local decl's BaseEventQueue-typed
    // accessor mangled to a symbol no TU defines (the shadowing-redeclaration shape -- the
    // walls-leg-4 trial link found it).
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // The shared swept-cache emission hook (declared FLAG-provisional in BrnDetachedWheelManager.h;
    // the walls-leg-4 gate body lives in that TU).
    void EmitUpdateTriangleCacheEvent(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface,
                                      u64 lu64VolumeInstanceId, const Vector3& lvSweptPosition,
                                      f32 lfSphereRadius);

    // X360 deformation owner tags (see BrnBurnoutBodyPartID.h). The post-physics pass forwards
    // a rigid-body update event to the part pool ONLY when the updated body's EntityId owner is
    // one of these two -- i.e. the body is a deformable car/traffic part, not some other rigid
    // body that shares the sim queue.
    static const u32 KU_OWNER_RACECAR_DEFORMABLE_PART = 6;  // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
    static const u32 KU_OWNER_TRAFFIC_DEFORMABLE_PART = 7;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART

    // X360-attested byte offset, inside an OutUpdateRigidBody event, of the updated body's
    // EntityId owner byte. The asm reads `BYTE4(*event)` -- byte +4 of the event -- which is
    // the high (owner) byte of the rigid body's packed EntityId word (bits [24..31], stored at
    // event +4 in the X360 sim RigidBodyId). FLAG: the X360 sim RigidBodyId is not homed in
    // tree; this byte offset is taken directly from the asm and is the only field this pass
    // reads off the event before forwarding it.
    static const u32 KU_EVENT_OWNER_BYTE_OFFSET = 4;

    // Transform-validation epsilon (asm: v60[0] = 0.0099999998f, the vcmpgtfp threshold the two
    // MakePartPhysical orthonormal tripwires compare the squared deviation against).
    static const f32 KF_ORTHONORMAL_EPSILON = 0.0099999998f;

    // ------------------------------------------------------------------------------------------
    // MakePartPhysical (X360 "MakePar") @ 0x82626E30
    //
    //   Promote a freshly shed body part into the physical pool. The asm is, observably, a
    //   transform-validation wrapper around PhysicalBodyPartPool::Create:
    //
    //     (1) Two NON-GATING orthonormal tripwires on the supplied transform basis (the asm
    //         restores the by-value matrix args into VMX registers v125/v126 and loads the
    //         third basis from the last stack arg, then runs two vmsum3fp128 / vcmpgtfp blocks):
    //           - BrnDetachedPartManager.cpp:84 -- ORTHOGONALITY: the basis rows are mutually
    //             perpendicular within epsilon (the row*row cross-products deviate from the
    //             identity by < epsilon). Fires (or not) and falls straight through.
    //           - BrnDetachedPartManager.cpp:85 -- NORMALITY: each basis row has unit magnitude
    //             within epsilon (|row|^2 - 1 < epsilon). Fires (or not) and falls straight
    //             through.
    //         Both asserts are non-gating; the asm evaluates them and then unconditionally
    //         continues into the Create call regardless of the result.
    //
    //     (2) Create the part: PhysicalBodyPartPool::Create(this, lpSimInput, ...) with the
    //         scalar args forwarded in the X360 int registers (a2..a8 == the asm's
    //         a2/a3/a4/a5/a22/a24/a26, where a22=a6, a24=a7, a26=a8) and the by-value
    //         Matrix44Affine / Vector3 args forwarded in the VMX registers (v1=v126, v2=v125,
    //         ... -- which is why the Hex-Rays int arg-soup shows only the scalar args). The
    //         frozen-header CreatePart signature is the layout-authoritative declaration; this
    //         body forwards every MakePartPhysical parameter to it and returns the result
    //         (the new part, or null when the pool is full).
    //
    //   FLAG: the Hex-Rays "MakePar" int arg soup (a1..a30) is the by-value Matrix44Affine /
    //   Vector3 spilling across the int + VMX register banks; the real signature is the frozen
    //   header's MakePartPhysical. The two tripwires are reconstructed against the supplied
    //   render-transform basis (the asm's v125/v126/last-arg basis); with the transform already
    //   orthonormal in normal operation neither fires.
    // ------------------------------------------------------------------------------------------
    PhysicalBodyPart* DetachedPartManager::MakePartPhysical(
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        u16 lu16DeformableObjectIndex,
        const DeformableObject* lpDeformableObject,
        RigidBodyId lHandlingBodyId, EntityId lGlobalCarId,
        s32 liPartIndex, const IKBodyPart* lpPart,
        Matrix44Affine lLocalRenderTransform,
        Matrix44Affine lVehicleTransform,
        Vector3 lInitialLinearVelocity,
        Vector3 lInitialAngularVelocity)
    {
        // ---- (1a) ORTHOGONALITY tripwire (BrnDetachedPartManager.cpp:84) -- non-gating ----
        // The three basis rows must be mutually perpendicular: each pair's dot product is ~0.
        // (The asm builds the row*row products, subtracts the identity, sums the squared
        // deviation per row and compares > epsilon; modelled lane-by-lane below.)
        const Vector3 lvRight = lLocalRenderTransform.Right();
        const Vector3 lvUp    = lLocalRenderTransform.Up();
        const Vector3 lvAt    = lLocalRenderTransform.At();

        const f32 lfDotRightUp = lvRight.x * lvUp.x + lvRight.y * lvUp.y + lvRight.z * lvUp.z;
        const f32 lfDotUpAt    = lvUp.x * lvAt.x + lvUp.y * lvAt.y + lvUp.z * lvAt.z;
        const f32 lfDotAtRight = lvAt.x * lvRight.x + lvAt.y * lvRight.y + lvAt.z * lvRight.z;

        CGS_ASSERT(lfDotRightUp * lfDotRightUp <= KF_ORTHONORMAL_EPSILON
                   && lfDotUpAt * lfDotUpAt <= KF_ORTHONORMAL_EPSILON
                   && lfDotAtRight * lfDotAtRight <= KF_ORTHONORMAL_EPSILON,
                   "IsOrthogonal3x3( lLocalRenderTransform )");

        // ---- (1b) NORMALITY tripwire (BrnDetachedPartManager.cpp:85) -- non-gating ----
        // Each basis row must have unit length: |row|^2 - 1 is within epsilon.
        const f32 lfMagSqRight = rw::math::vpu::MagnitudeSquared(lvRight);
        const f32 lfMagSqUp    = rw::math::vpu::MagnitudeSquared(lvUp);
        const f32 lfMagSqAt    = rw::math::vpu::MagnitudeSquared(lvAt);

        CGS_ASSERT((lfMagSqRight - 1.0f) <= KF_ORTHONORMAL_EPSILON
                   && (lfMagSqUp - 1.0f) <= KF_ORTHONORMAL_EPSILON
                   && (lfMagSqAt - 1.0f) <= KF_ORTHONORMAL_EPSILON,
                   "IsNormal3x3( lLocalRenderTransform )");

        // ---- (2) create the pool slot for the part + return it (null when the pool is full) ----
        return mPartPool.CreatePart(lpSimInput, lu16DeformableObjectIndex, lpDeformableObject,
                                    lHandlingBodyId, lGlobalCarId, liPartIndex, lpPart,
                                    lLocalRenderTransform, lVehicleTransform,
                                    lInitialLinearVelocity, lInitialAngularVelocity);
    }

    // ------------------------------------------------------------------------------------------
    // TestJointForBreaking @ 0x8260E3C0  -- 27 instructions, LANDED 2026-08-27 (detach wave)
    //
    // ⛔ THE PRIOR BANNER FOR THIS FUNCTION WAS WRONG ON BOTH ADDRESS AND SIZE. The log-once gate in
    // BrnDeformableObject_Detach.cpp recorded it as "@0x825E??? (PS3 0x761F2C, 401)" -- a 401-insn
    // body at an unknown address. It is 27 instructions at 0x8260E3C0, and it is a PURE FORWARDER
    // whose one real callee (PhysicalBodyPart::TestJointForBreaking @0x8260C0F8) was already bodied
    // and mounted the whole time. [[unnamed-sub-bodies-and-env-faults]] -- "unrecoverable" was a NAME
    // search failing, recorded as an address.
    //
    // The asm, store for store:
    //   r31=this r30=liPartIndex r29=lpSimInput r28=lpSimOutput
    //   bl PhysicalBodyPartPool::IsPartIndexUsed(this, liPartIndex)   @0x825A0758
    //   if (!used) BeginAssert / FireAssert("IsPartIndexUsed( liPoolIndex )",
    //                                       BrnDetachedPartManager.cpp, 207) / EndAssert  [NON-GATING]
    //   extsh r4, r30                                                 (the index narrows to s16)
    //   bl PhysicalBodyPartPool::GetPart(this, (s16)liPartIndex)      @0x825A0858
    //   bl PhysicalBodyPart::TestJointForBreaking(part, lpSimInput, lpSimOutput)  @0x8260C0F8
    //   -> return its result.
    // Both pool accessors reach the pool with the MANAGER address as `this` (mPartPool is this
    // manager's one member, at +0) -- the same header-inline-forward evidence pattern the
    // GetPartFromIndex / IsPartIndexUsed wrappers are spelled with, so they go through mPartPool here.
    //
    // ⚠️ The assert is NON-GATING: the asm falls straight through into GetPart with an unused index,
    // exactly as spelled. GetPart returns the slot regardless; the console does the same.
    // ------------------------------------------------------------------------------------------
    bool DetachedPartManager::TestJointForBreaking(
        s32 liPartIndex,
        CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
        BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(mPartPool.IsPartIndexUsed(liPartIndex), "IsPartIndexUsed( liPoolIndex )");

        PhysicalBodyPart* lpPart = mPartPool.GetPart(static_cast<s16>(liPartIndex));

        return lpPart->TestJointForBreaking(lpSimInput, lpOutput);
    }

    // ------------------------------------------------------------------------------------------
    // UpdatePostPhysics @ 0x8260E118
    //
    //   Advance the detached-part pool after the physics step, in the asm's exact order:
    //
    //     (1) Broadcast the consumed time-step. The asm reads GetTimeStepUsed(outputBuffer) into
    //         a local VecFloat's x lane and splats it across the lanes (memset the tail to 0 then
    //         vspltw128). This broadcast time-step is the lvfTimeStep handed to UpdateJoinedParts.
    //
    //     (2) Forward each deformable-part rigid-body update. Walk the post-physics
    //         OutUpdateRigidBodyQueue (length @ +8 == GetLength()); for each event read its
    //         updated body's EntityId owner byte (asm: BYTE4(*event), event byte +4) and, when it
    //         is a deformable part (owner 6 == RACECAR or 7 == TRAFFIC), forward the event to
    //         PhysicalBodyPartPool::UpdatePart. Events for other rigid bodies sharing the queue
    //         are skipped. (The asm passes only `this` in the int bank to UpdatePart; the event +
    //         scene-interface args travel in the other registers Hex-Rays drops.)
    //
    //     (3) Resolve the still-joined parts' contacts:
    //         PhysicalBodyPartPool::UpdateJoinedParts(potentialContactsInterface, contactSpyData,
    //         timeStep). (asm: UpdateJoinedParts(a1, a5, a4, ..., v1=time-step).)
    //
    //     (4) Recompute + republish ONE part's bounding box this frame:
    //         PhysicalBodyPartPool::UpdateABoundingBox(sceneInterface). (asm: UpdateABoundingBox(
    //         a1, a3); its result is the function's return value -- the C++ return is void.)
    //
    //   FLAG: the X360 sim OutputBuffer / OutUpdateRigidBody event are not homed (declared above);
    //   the owner-byte read uses the asm's attested event byte offset (+4). The forwarded event is
    //   re-spelled to the deformation-side OutUpdateRigidBody the pool's UpdatePart is declared
    //   over (the same underlying event).
    // ------------------------------------------------------------------------------------------
    void DetachedPartManager::UpdatePostPhysics(
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        ContactSpyData* lpContactSpyData,
        const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface)
    {
        // ---- (1) broadcast the consumed time-step into a VecFloat ----
        const f32 lfTimeStep = lpSimModuleOutputBuffer->GetTimeStepUsed();
        const VecFloat lvfTimeStep = { lfTimeStep, lfTimeStep, lfTimeStep, lfTimeStep };

        // ---- (2) forward each deformable-part rigid-body update event ----
        const CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutUpdateRigidBodyQueue*
            lpUpdatedBodyQueue = lpSimModuleOutputBuffer->GetUpdateRigidBodyQueue();

        const s32 liNumUpdatedBodyEvents = lpUpdatedBodyQueue->GetLength();
        for (s32 liUpdatedBodyEventIndex = 0;
             liUpdatedBodyEventIndex < liNumUpdatedBodyEvents;
             ++liUpdatedBodyEventIndex)
        {
            const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody& lUpdatedBodyEvent =
                lpUpdatedBodyQueue->GetEvent(liUpdatedBodyEventIndex);

            // The updated body's EntityId owner byte (asm BYTE4(*event) -- event byte +4).
            const u8 lu8Owner = *(reinterpret_cast<const u8*>(&lUpdatedBodyEvent)
                                  + KU_EVENT_OWNER_BYTE_OFFSET);

            if (lu8Owner == KU_OWNER_RACECAR_DEFORMABLE_PART
                || lu8Owner == KU_OWNER_TRAFFIC_DEFORMABLE_PART)
            {
                // Forward to the pool (re-spelled to the deformation-side OutUpdateRigidBody the
                // pool's UpdatePart is declared over -- the same underlying event).
                mPartPool.UpdatePart(
                    reinterpret_cast<const OutUpdateRigidBody*>(&lUpdatedBodyEvent),
                    lpSceneInterface);
            }
        }

        // ---- (3) resolve the still-joined parts' contacts ----
        mPartPool.UpdateJoinedParts(
            // FLAG (fork seam, header note): the pool still models the interface locally.
            reinterpret_cast<const PhysicsModuleIO::PotentialContactInterfaceModel*>(lpPotentialContactsInterface), lpContactSpyData, lvfTimeStep);

        // ---- (4) recompute + republish one part's bounding box this frame ----
        mPartPool.UpdateABoundingBox(lpSceneInterface);
    }

    // =============================================================================================
    // UpdateTriangleCache @0x8260E1F8 (113) -- ⭐ 2026-08-14 (walls leg 4). The part-pool twin of
    // DetachedWheelManager::UpdateTriangleCache (same swept-sphere event emission, same 1/60 +
    // 0.1-padding immediates): assert the scene interface (:161); for every used pool part that
    // IS in the scene (+485) and NOT frozen (+484 clear on the X360 read), emit an
    // update-cached-position event at the velocity-swept render position with the padded radius.
    // Dead-at-runtime today (0 physical parts); the emission hook is the wheel TU's shared gate.
    // =============================================================================================
    void DetachedPartManager::UpdateTriangleCache(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface)
    {
        CGS_ASSERT(lpSceneUpdateInterface != nullptr, "lpSceneUpdateInterface != NULL");   // :161

        const f32 KF_FRAME_TIMESTEP        = 0.016666668f;   // asm immediate (1/60)
        const f32 KF_TRIANGLE_CACHE_PADDING = 0.1f;          // asm immediate

        for ( u8 lu8Part = 0; lu8Part < 50u; ++lu8Part )
        {
            if ( !mPartPool.IsPartIndexUsed(lu8Part) )
            {
                continue;
            }
            const PhysicalBodyPart* lpPart = mPartPool.GetPart(static_cast<s16>(lu8Part));
            if ( !lpPart->IsAddedToScene() || lpPart->IsFrozen() )
            {
                continue;
            }

            const Vector3 lvVelocity = lpPart->GetLinearVelocity();
            const f32 lfSpeed = std::sqrt(lvVelocity.x * lvVelocity.x
                                        + lvVelocity.y * lvVelocity.y
                                        + lvVelocity.z * lvVelocity.z);
            const f32 lfSweptDistance = lfSpeed * KF_FRAME_TIMESTEP;

            Vector3 lvSweptPosition = lpPart->GetRigidBodyTransform().wAxis;
            if ( lfSpeed != 0.0f )
            {
                const f32 lfInvSpeed = 1.0f / lfSpeed;
                lvSweptPosition.x += lvVelocity.x * lfInvSpeed * lfSweptDistance;
                lvSweptPosition.y += lvVelocity.y * lfInvSpeed * lfSweptDistance;
                lvSweptPosition.z += lvVelocity.z * lfInvSpeed * lfSweptDistance;
            }

            const f32 lfSphereRadius =
                lpPart->GetSphereRadius() + KF_TRIANGLE_CACHE_PADDING + lfSweptDistance;

            EmitUpdateTriangleCacheEvent(lpSceneUpdateInterface,
                                         lpPart->GetContactVolumeInstanceId().muId,
                                         lvSweptPosition, lfSphereRadius);
        }
    }


    // =============================================================================================
    // Update / AddPartsToScene -- ⭐ 2026-08-14 (walls leg 4): pool forwards. The X360 emits no
    // manager bodies (DeformationManager::Update @0x82649B40 / UpdatePostPhysics @0x82630420 call
    // the POOL's UpdateRWBodies / AddPartsToScene directly with the MANAGER address as `this`;
    // the pool is this manager's one member, at +0). Bodied as the forwards those calls spell.
    // =============================================================================================
    void DetachedPartManager::Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                     VecFloat lvfTimeStep)
    {
        mPartPool.UpdateRWBodies(lpSimInput, lvfTimeStep);
    }

    void DetachedPartManager::AddPartsToScene(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        mPartPool.AddPartsToScene(lpSceneInterface);
    }

    // =============================================================================================
    // OutputEvents (BrnDetachedPartManager.h:86; landed 2026-08-24, deform-land wave). The console
    // FOLDS this wrapper: DeformationManager::OutputData @0x826225D8 calls PhysicalBodyPartPool::
    // OutputEvents @0x8260DBE8 directly on the MANAGER's address (mPartPool is this manager's one
    // member, at +0). The wrapper exists so the caller spells the member by name.
    // =============================================================================================
    void DetachedPartManager::OutputEvents(
        DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
        DeformationOutputInterface* lpOutput) const
    {
        mPartPool.OutputEvents(lpOutputForEntityModules, lpOutput);
    }

}
}
