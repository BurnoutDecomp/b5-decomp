#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h
//
// BrnPhysics::Deformation::PhysicalBodyPart -- the physics/simulation side of ONE
// deformable body part of a vehicle (a bonnet, door, bumper, panel, ...). While the
// part is still attached it is driven as a hinged/ball-and-socket joint hanging off
// the parent car (the deformation "joint model"); once the joint stress exceeds its
// detach threshold the part breaks off and becomes a free externally-simulated rigid
// body that the part pool keeps updating. PhysicalBodyPart owns the part's
// ExternalPhysicsBody, its oriented bounding box, the packed joint/COM/graphics state,
// the per-frame collision accumulators, and the cross-references back to its static
// IK spec (IKBodyPart) and owning DeformableObject.
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnPhysicalBodyPart.cpp). The
// member order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnPhysicalBodyPart.h, the
// PhysicalBodyPart struct @ BrnPhysicalBodyPart.h:123). Each member carries its DWARF
// source line for provenance.
//
// X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there, members pinned BY
// NAME + SEQUENCE here. The console host size of this class is not load-bearing for the
// per-TU `cl /c` gate (the part pool indexes it as PhysicalBodyPart[50]); GROW
// additively rather than forking.
// ============================================================================

#include "types.hpp"           // s32, u8, s8, u16, f32
#include <cmath>               // std::sqrt (GetSphereRadius, walls leg 4)
#include "BrnCommonTypes.h"    // Vector3, Vector3Plus, Matrix44Affine, VecFloat, EntityId, RigidBodyId

#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"                       // ExternalPhysicsBody (embedded BY VALUE)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h" // BurnoutBodyPartID (embedded BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                       // CgsSceneManager::VolumeInstanceId (returned BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"                               // CgsSceneManager::VolumeId (returned BY VALUE)

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----
// Per project rule these are NOT included (their definitions are other agents' homes);
// PhysicalBodyPart only stores/passes them by pointer or reference.
namespace CgsGeometric { struct Box; }  // GetBoundingBox out-param (DWARF CgsGeometric::Box*).
                                        // `class`->`struct` 2026-08-14 (walls leg 4): unified with the
                                        // provisional home + BrnDeformableObject.h (MSVC mangle fork).

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct InSceneUpdateInterface;       // scene add/remove interface (RemoveFromScene etc.)
}
}

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    struct InputBuffer;   // ⚠ class-key fixed 2026-08-06 (struct per CgsPhysicsSimulationModuleIO.h:43)                   // sim-input event buffer (AddToSim/UpdateRW/...)
    struct OutputBuffer;  // ⚠ class-key fixed 2026-08-06 (struct per CgsPhysicsSimulationModuleIO.h:321)                  // sim-output buffer; OutputBuffer::SceneInputInterface
                                         //   is a typedef of CgsSceneManager::SceneManagerIO::
                                         //   InSceneUpdateInterface (the DWARF spells the Update/
                                         //   SetRigidBodyTransform scene arg as
                                         //   OutputBuffer::SceneInputInterface*).
}
}

// ADDITIVE GROW (FLAG -- PhysicalBodyPart family, declared in this class' own header per the
// per-TU gate rule "if a callee is not declared anywhere, prefer YOUR class header"). These two
// deep IO callees the body-part .cpp reaches are owned by not-yet-homed TUs; only the call site
// shape is known. Declared-only here so BrnPhysicalBodyPart.cpp compiles; the owning TUs emit the
// authoritative bodies (and may relocate/refine these decls).
//
//   * The triangle-cache removal producer RemoveFromScene @0x825E7818 feeds (the asm's
//     CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache on the scene's tri-cache
//     manager). Modelled as a free hook taking the scene interface + the part's tri-cache slot.
//   * The detached-part notification emit at the tail of TestJointForBreaking @0x8260C0F8 (the asm
//     builds a DetachedPartNotificationEvent and AddEventSafe's it onto the deformation output
//     interface's +0x3A0 notification queue). LANDED 2026-08-27 -- no longer a hook; the record is
//     built at the two call sites out of its three named fields.
//   * The "update external body" event emit at the tail of UpdateRW @0x825E7998 (the asm fetches the
//     sim InputBuffer's InUpdateExternalBody queue -- `bl CgsPhysi`(InputBuffer) returning the channel
//     -- and AddEvent's a packed {bodyId, transform, linearVel, angularVel} event onto it). Modelled
//     as a free hook taking the input buffer + the packed event blob.
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface; } }
// ⚠ CLASS-KEYS FIXED 2026-08-06 (big-five #2): `struct` per CgsPhysicsSimulationModuleIO.h.
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InputBuffer; struct OutputBuffer; } }
// ⭐ 2026-08-27 (detach wave): TestJointForBreaking's second parameter is the PHYSICS-MODULE output
// buffer, not the sim one. Proof is the PS3 mangle at 0x75528C:
//   _ZN10BrnPhysics11Deformation16PhysicalBodyPart20TestJointForBreaking
//      EPN10CgsPhysics19PhysicsSimulationIO11InputBufferE PNS_15PhysicsModuleIO12OutputBufferE
// i.e. (CgsPhysics::PhysicsSimulationIO::InputBuffer*, BrnPhysics::PhysicsModuleIO::OutputBuffer*).
// Its two callers agree: DetachedPartManager::TestJointForBreaking (PS3 0x761F2C) and
// DeformableObject::CheckForDetachment (PS3 0x762570) both carry PhysicsModuleIO::OutputBuffer* in
// that seat, and the X360 passes the register straight through all three frames. The old spelling
// forked the type mid-chain, which is why BrnDeformableObject_Detach.cpp had to declare its OWN
// free-function TestJointForBreaking/EmitDetachedPartNotification overloads to compile. (The
// EmitDetachedPartNotification half of that pair is gone entirely as of 2026-08-27.)
// [[odr-forks-link-silently]] -- and one of those forked overloads had NO definition anywhere.
namespace BrnPhysics { namespace PhysicsModuleIO { class OutputBuffer; } }
namespace BrnPhysics
{
namespace Deformation
{
    void RemoveTriangleCacheSlot(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                                 u16 lu16TriangleCacheSlot);                                   // FLAG: provisional
    // EmitDetachedPartNotification's DECLARATION IS RETIRED 2026-08-27 (detach-2 wave). It was
    // FLAG-provisional and it was a fabricated API: an untyped `const void* lpEventBlob` standing in
    // for a NAMED 32-byte record (Deformation::DetachedPartNotificationEvent, three named fields).
    // Both console sites build that record inline and AddEventSafe it onto the deformation output
    // interface's +0x3A0 queue; the two call sites now do the same. See either one's banner.
    void EmitUpdateExternalBodyEvent(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                     const void* lpEventBlob);                                  // FLAG: provisional
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // Static IK rig data for this part (its joints + bbox skin + graphics transform). The
    // physical part holds a const pointer back to its spec. Owned by BrnIKBodyPart.h.
    struct IKBodyPart;

    // The owning aggregate -- the whole vehicle's deformable model. PhysicalBodyPart holds a
    // const back-pointer to it (GetTransformDelta, sensor lookups, ...). Owned by
    // BrnDeformableObject.h.
    class DeformableObject;

    // Per-frame physics solver event the part pool feeds to Update (the post-physics rigid-body
    // transform/velocity update). Owned by the sim-output TU; referenced by pointer only. FLAG:
    // forward-declared (definition not homed in this family).
    struct OutUpdateRigidBody;

    // A single potential contact handed to AddContact (the deformation contact the joint resolves
    // against). Owned by the physics contact-interface TU. FLAG: forward-declared.
    struct PotentialContact;

    // The per-part contact-spy debug record AddContactSpy writes. Owned by the contact-spy TU.
    // FLAG: forward-declared.
    struct ContactSpyData;

    // The skinned bounding-box control point CalculateSkinnedPoint transforms (the static
    // per-corner skin weights). Already homed in BrnBBoxPointSkinData.h / BrnBodyPartBBoxSpec.h
    // (DWARF BBoxPointSkinData @ BrnIKBodyPartSpec.h:49) -- forward-declared here (used only by
    // const reference in one private method) to avoid an ODR clash with that home.
    struct BBoxPointSkinData;

    // The private volume-id accessors return CgsSceneManager::VolumeInstanceId /
    // CgsSceneManager::VolumeId BY VALUE (the DWARF spells them unqualified); those two tiny
    // packed-id homes are #included above so the by-value returns have a complete type.

    class PhysicalBodyPart
    {
    public:
        // ----- lifecycle -----------------------------------------------------------------

        // BrnPhysicalBodyPart.h:127. Zero/identity-initialise the part (box buffer, packed
        // joint/COM/collision state, flags).
        void Construct();

        // BrnPhysicalBodyPart.h:136. Bind this part to a vehicle + its IK spec, building the
        // local joint/graphics/COM frames from the two passed transforms (graphics + bbox
        // orientation). @ asm calls CalcBoundingBox.
        void Prepare(BurnoutBodyPartID lPartId, EntityId lGlobalVehicleId,
                     const DeformableObject* lpDeformableObject, const IKBodyPart* lpIKPart,
                     Matrix44Affine lGraphicsTransform, Matrix44Affine lBBoxOrientation);

        // BrnPhysicalBodyPart.h:139. Tear down (frees the RW body resources).
        void Release();

        // ----- transforms ----------------------------------------------------------------

        // BrnPhysicalBodyPart.h:142. The render-space transform (rigid-body transform composed
        // with the local graphics offset).
        // ⭐ INLINED walls leg 4 (console-inline): the rigid transform with the local graphics
        // offset rotated into world and added to the translation (the banner's composition; the
        // offset xyz is mLocalGraphicsPositionPlusJointVelocity's vector part).
        Matrix44Affine GetRenderTransform() const
        {
            Matrix44Affine lT = mRwBody.GetTransform();
            const Vector3Plus& lrOff = mLocalGraphicsPositionPlusJointVelocity;
            lT.wAxis.x += lT.xAxis.x * lrOff.x + lT.yAxis.x * lrOff.y + lT.zAxis.x * lrOff.z;
            lT.wAxis.y += lT.xAxis.y * lrOff.x + lT.yAxis.y * lrOff.y + lT.zAxis.y * lrOff.z;
            lT.wAxis.z += lT.xAxis.z * lrOff.x + lT.yAxis.z * lrOff.y + lT.zAxis.z * lrOff.z;
            return lT;
        }

        // BrnPhysicalBodyPart.h:145. The raw rigid-body (physics) transform.
        Matrix44Affine GetRigidBodyTransform() const { return mRwBody.GetTransform(); }   // (inlined walls leg 4)

        // Console-INLINE (PhysicalBodyPartPool::OutputEvents @0x8260DBE8, block
        // 0x8260DCF0..0x8260DD64; added 2026-08-24, deform-land wave): the transform both
        // detached-part OUTPUT EVENTS carry -- the rigid transform with the offset
        // (localGraphicsPos - localInitialComPos) rotated into world and ADDED to the
        // translation (asm: vsubfp of part+0x170 and part+0x180 xyz, three splat-madd rows,
        // vaddfp onto row 3). NOTE this is NOT GetRenderTransform above: the event
        // composition subtracts the initial-COM offset, the walls-leg-4 composition does not.
        Matrix44Affine GetEventRenderTransform() const
        {
            Matrix44Affine lT = mRwBody.GetTransform();
            const f32 lfOffX = mLocalGraphicsPositionPlusJointVelocity.x - mLocalInitialComPositionPlusMaxJointAngle.x;
            const f32 lfOffY = mLocalGraphicsPositionPlusJointVelocity.y - mLocalInitialComPositionPlusMaxJointAngle.y;
            const f32 lfOffZ = mLocalGraphicsPositionPlusJointVelocity.z - mLocalInitialComPositionPlusMaxJointAngle.z;
            lT.wAxis.x += lT.xAxis.x * lfOffX + lT.yAxis.x * lfOffY + lT.zAxis.x * lfOffZ;
            lT.wAxis.y += lT.xAxis.y * lfOffX + lT.yAxis.y * lfOffY + lT.zAxis.y * lfOffZ;
            lT.wAxis.z += lT.xAxis.z * lfOffX + lT.yAxis.z * lfOffY + lT.zAxis.z * lfOffZ;
            return lT;
        }

        // ----- per-frame update ----------------------------------------------------------

        // BrnPhysicalBodyPart.h:150. Apply a post-physics rigid-body update event, pushing the
        // new transform out to the scene via the scene-input interface.
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void Update(const OutUpdateRigidBody* lpUpdateEvent,
                    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPart.h:155. Force the rigid-body transform and republish it to the scene.
        void SetRigidBodyTransform(Matrix44Affine lTransform,
                                   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // ----- identity / queries --------------------------------------------------------

        // BrnPhysicalBodyPart.h:158. This part's own scene-entity id (from mRigidBodyId).
        EntityId GetEntityId() const;

        // BrnPhysicalBodyPart.h:161. The packed body-part id.
        // ⭐ INLINED 2026-08-27 (detach-2 wave): DECLARE-ONLY until now, and its first caller
        // (PhysicalBodyPartPool::UpdatePart's :173 tripwire) turned it into an LNK2019. There is no
        // out-of-line X360 emission -- the whole subsystem reaches the handle with a bare
        // `ld 0x1D0(part)` at every site (AddToSim @0x8260AD80, AddToScene @0x8260A9C0/F8,
        // UpdatePart's own compare, FixupBodyPartVehicleContact @0x825A0D64), i.e. the inlined
        // accessor. Same evidence pattern as IsAddedToScene / IsJoinedToVehicle / IsFrozen above.
        BurnoutBodyPartID GetRigidBodyId() const { return mRigidBodyId; }

        // BrnPhysicalBodyPart.h:164. The owning vehicle's global entity id (mGlobalVehicleId).
        EntityId GetGlobalEntityId() const;

        // BrnPhysicalBodyPart.h:167. The render mesh index of this part (from its IK spec).
        s32 GetMeshIndex() const;

        // BrnPhysicalBodyPart.h:170. Whether the part's volume instance is currently in the scene.
        // ⭐ INLINE 2026-08-14 (deformation-mount wave): no out-of-line emission on either console
        // (same evidence pattern as IsJoinedToVehicle below) -- PhysicalBodyPartPool::RemovePart
        // @0x8260CA78 lbz's mbAddedToScene (console +485) directly.
        bool IsAddedToScene() const { return mbAddedToScene; }

        // ----- scene membership ----------------------------------------------------------

        // BrnPhysicalBodyPart.h:174. Add the part's collision volume instance to the scene.
        void AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // BrnPhysicalBodyPart.h:178. Remove it from the scene.
        void RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // ----- simulation ----------------------------------------------------------------

        // BrnPhysicalBodyPart.h:185. Register the part as a free rigid body in the physics sim,
        // seeding it with a world transform + linear/angular velocity (used when the part detaches).
        // ⭐ PARAMETER NAMES + THE `const Matrix44Affine&` ARE DWARF-AUTHORITATIVE (DecFIGS
        // dwarfdump/.../BrnPhysicalBodyPart.cpp:784 spells the definition
        // `AddToSim(InputBuffer*, const rw::math::vpu::Matrix44Affine& lVehicleTransform,
        //           const Vector3 lInitialLinearVelocity, const Vector3 lInitialAngularVelocity)`),
        // and the reference matches the X360 ABI, which hands the matrix in r5 as a pointer.
        void AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                      const Matrix44Affine& lVehicleTransform,
                      Vector3 lInitialLinearVelocity, Vector3 lInitialAngularVelocity);

        // BrnPhysicalBodyPart.h:188. The part's current linear velocity.
        // ⭐ INLINE 2026-08-06 (bridge de-facade wave): no out-of-line emission exists --
        // DeformationManager::CreateDetachedPartContactEvent @0x825DD7BC lvx's the body row
        // (mRwBody base +0x40) directly. Forwards to the body's own public inline.
        Vector3 GetLinearVelocity() const { return mRwBody.GetLinearVelocity(); }

        // BrnPhysicalBodyPart.h:193. Push the part's transform into RenderWare for this timestep.
        void UpdateRW(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep);

        // ----- pool / spec indices -------------------------------------------------------

        // BrnPhysicalBodyPart.h:196. This part's slot index inside its PhysicalBodyPartPool.
        u8 GetPoolIndex() const { return static_cast<u8>(mRigidBodyId.muSubB); }   // (inlined walls leg 4: the handle's low sub-id, the _Detach '+464 low byte' read)

        // BrnPhysicalBodyPart.h:200. The index of this part's IK spec within the deformable model.
        s32 GetIKPartIndex() const { return static_cast<s32>(mRigidBodyId.muEntityWord & 0x3FFu); }   // (inlined walls leg 4: the entity word's part-index field)

        // ----- collision geometry --------------------------------------------------------

        // BrnPhysicalBodyPart.h:205. Write the part's world oriented bounding box out.
        void GetBoundingBox(CgsGeometric::Box* lpBoxOut) const;

        // BrnPhysicalBodyPart.h:208. The embedded externally-simulated physics body.
        ExternalPhysicsBody* GetExternalBody() { return &mRwBody; }   // (inlined walls leg 4)

        // BrnPhysicalBodyPart.h:212. A bounding sphere radius covering the box (broad-phase).
        // ⭐ INLINED walls leg 4 (console-inline; FLAG role-derived): the broad-phase radius
        // covering the oriented box == |half extents| (the banner's own description).
        f32 GetSphereRadius() const
        {
            const Vector3& lrH = mBoundingBoxHalfDimensions;
            return std::sqrt(lrH.x * lrH.x + lrH.y * lrH.y + lrH.z * lrH.z);
        }

        // BrnPhysicalBodyPart.h:215. The part's world position (rigid-body translation).
        Vector3 GetPosition() const;

        // ----- spec / state accessors ----------------------------------------------------

        // BrnPhysicalBodyPart.h:218. The static IK spec backing this part.
        // ⭐ INLINE 2026-08-06: no out-of-line emission -- the same creator @0x825DD79C lwz's
        // mpIKPart (console +476) directly.
        const IKBodyPart* GetIKPart() const { return mpIKPart; }

        // BrnPhysicalBodyPart.h:222. Whether the part is frozen (sim disabled, settled).
        // ⭐ INLINE 2026-08-06 (big-five #2): no out-of-line X360 emission -- the one console
        // consumer (BridgeContactsToSimulation via PhysicalBodyPartPool::GetPart) reads the
        // byte at part+486 directly, i.e. the inlined accessor.
        bool IsFrozen() const { return mbFrozen; }

        // BrnPhysicalBodyPart.h:226. Whether the part still needs adding to the scene.
        bool NeedsAddingToScene() const;

        // BrnPhysicalBodyPart.h:230. Flag that the part's transform must be written into RW.
        void SetNeedsWritingToRW();

        // BrnPhysicalBodyPart.h:235. Recompute + republish the part's bounding box to the scene.
        void UpdateBoundingBox(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPart.h:238. The triangle-cache slot this part's collision mesh occupies.
        u16 GetTriangleCacheSlot() const { return static_cast<u16>((mRigidBodyId.muSubB & 0xFFu) + 73u); }   // (inlined walls leg 4: the _Remove slice's attested (handle&0xFF)+73)

        // ----- joint model (attached-part deformation) -----------------------------------

        // BrnPhysicalBodyPart.h:242. Whether the part is still joined to its vehicle (vs detached).
        // ⭐ INLINE 2026-08-06: no out-of-line emission -- the creator @0x825DD7B4 lbz's
        // mbJoinedToVehicle (console +484) directly.
        bool IsJoinedToVehicle() const { return mbJoinedToVehicle; }

        // BrnPhysicalBodyPart.h:250. Join the part to the vehicle as an active joint: seed the
        // local joint position (v1) and the max-joint-angle from the COM arg's w lane (v3), then the
        // active-joints tag-point index. NOTE: the asm takes only TWO vector args (v1, v3) + the char
        // tag index; the former limit-stress VMX arg was a fabrication (the asm never writes the
        // +400 limit-stress w lane) and has been dropped.
        void SetJoinedToVehicle(Vector3 lLocalJointPosition, Vector3 lLocalComPosition,
                                s32 liActiveJointsTagPointIndex);

        // BrnPhysicalBodyPart.h:254. Break the joint and hand the part to the sim as a free body.
        void RemoveJointAndAddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput);

        // BrnPhysicalBodyPart.h:258. Integrate the joint one step (gravity + restitution +
        // penetration resolve; updates the packed joint rotation/velocity).
        void UpdateJoint(VecFloat lvfTimeStep);

        // BrnPhysicalBodyPart.h:261. End-of-vehicle-update hook (post-vehicle joint bookkeeping).
        void PostVehicleUpdate();

        // BrnPhysicalBodyPart.h:265. Accumulate one potential contact against the joint
        // (stores the deepest penetration / collision point into the packed accumulators).
        void AddContact(const PotentialContact& lContact);

        // BrnPhysicalBodyPart.h:268. The packed joint rotation angle.
        VecFloat GetJointRotation() const;

        // BrnPhysicalBodyPart.h:272. The joint rotation as a proportion of its max angle.
        VecFloat GetJointRotationProportion() const;

        // BrnPhysicalBodyPart.h:275. The packed joint angular velocity.
        VecFloat GetJointVelocity() const;

        // BrnPhysicalBodyPart.h:281. Test whether the joint's accumulated stress breaks it this
        // frame; if so, emit the detach onto the sim/output buffers. Returns true if it broke.
        // ⭐ PARAM 2 CORRECTED 2026-08-27 to PhysicsModuleIO::OutputBuffer per the PS3 mangle
        // (0x75528C) -- see the note above the free hooks at the top of this header.
        bool TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                  BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput);

        // BrnPhysicalBodyPart.h:285. Write this part's contact-spy debug record.
        void AddContactSpy(ContactSpyData* lpContactSpyData);

        // BrnPhysicalBodyPart.h:288. Recompute the local bounding box (no scene publish).
        void UpdateBoundingBox();

        // BrnPhysicalBodyPart.h:292. Set the packed joint angular velocity -- the w ("Plus") lane
        // of mLocalGraphicsPositionPlusJointVelocity, xyz untouched. ⭐ HEADER-INLINED 2026-08-14
        // (deformation-mount wave): NO out-of-line export exists on EITHER console (it was inline
        // there too); the write shape is attested by DeformableObject::ResetJointVelocities
        // @0x825DF868 (`vrlimi128 v13, v0, 1, 0` == w lane <- 0, xyz kept; PS3 @0x6F8E64 vperm
        // <0,1,2,7> against zeros, same thing).
        void SetJointVelocity(VecFloat lvfJointVelocity)
        {
            mLocalGraphicsPositionPlusJointVelocity.SetPlus(lvfJointVelocity.x);
        }

        // ⭐ ADDED 2026-08-14 (deformation-mount wave). PhysicalBodyPartPool::RemovePart tears
        // down the released slot's bindings with four direct stores on the console
        // (0x8260CAF0..0x8260CAFC, a cross-object poke): mpIKPart = 0 (part+0x1DC),
        // mpDeformableObject = 0 (part+0x1E0), mbAddedToScene = false (part+0x1E5), and
        // mRigidBodyId re-seeded from qword_82F2A3A8 == CgsPhysics::K_INVALID_RIGID_BODY_ID
        // (~0ull -- all three packed fields go all-ones). This named method is the by-name
        // equivalent (the SetDeformationSpec / GetContactVolumeInstanceId precedent -- no
        // friendship, no raw offsets).
        void ClearPoolSlotBindings()
        {
            mpIKPart           = nullptr;        // stw 0 -> +0x1DC
            mpDeformableObject = nullptr;        // stw 0 -> +0x1E0
            mbAddedToScene     = false;          // stb 0 -> +0x1E5
            mRigidBodyId.muEntityWord = 0xFFFFFFFFu;   // std K_INVALID_RIGID_BODY_ID (~0ull)
            mRigidBodyId.muSubA       = 0xFFFFu;       //   spread over the packed id's
            mRigidBodyId.muSubB       = 0xFFFFu;       //   three fields
        }

        // ⭐ ADDED 2026-08-06 (FixUpVehicleContacts wave): the packed 8-byte word
        // DeformationManager::FixupBodyPartVehicleContact re-keys a contact's A id from. The
        // console reads the part's 64-bit BurnoutBodyPartID whole (`ld 0x1D0(part)`
        // @0x825A0D64 -- mRigidBodyId spans part+464..471 exactly) and stores it into
        // muVolumeInstanceIdA: the packed part handle IS the part's scene volume-instance id
        // (entity word == muEntityWord in the big-endian HIGH dword, {muSubA, muSubB} the low).
        // The DWARF's GetVolumeInstanceId() (:301) is PRIVATE; the console manager reached this
        // state inline (its access path -- friendship or a public overload -- is not recoverable
        // from the stripped build), so this is a PUBLIC documented accessor over the same bytes,
        // following the DeformableObject::GetHandlingBodyIdHighByte precedent.
        CgsSceneManager::VolumeInstanceId GetContactVolumeInstanceId() const
        {
            CgsSceneManager::VolumeInstanceId lId;
            // ⭐ 2026-08-27: the pack itself moved onto BurnoutBodyPartID::GetBaseRigidBodyID()
            // (the console's own name for it, read off UpdatePart's assert string) so the sim-side
            // and scene-side readings of this handle cannot drift apart. Same three shifts.
            lId.muId = mRigidBodyId.GetBaseRigidBodyID();
            return lId;
        }

    private:
        // ----- private helpers (declarations only) ---------------------------------------

        // BrnPhysicalBodyPart.h:301. The packed scene volume-instance id of this part.
        CgsSceneManager::VolumeInstanceId GetVolumeInstanceId() const;

        // BrnPhysicalBodyPart.h:304. The packed scene volume id (shared collision volume).
        CgsSceneManager::VolumeId GetVolumeId() const;

        // BrnPhysicalBodyPart.h:309. Compute the local-space min/max bbox extents (out-params).
        void CalculateBoundingBoxExtents(Vector3& lvBoundingBoxMin, Vector3& lvBoundingBoxMax);

        // BrnPhysicalBodyPart.h:312. Compute the axis-aligned bbox half-extents.
        Vector3 CalculateAABBExtents();

        // BrnPhysicalBodyPart.h:316. (Re)build the oriented bounding box from a transform.
        void CalcBoundingBox(Matrix44Affine lTransform);

        // BrnPhysicalBodyPart.h:320. Clamp the part's linear/angular velocities to the
        // time-scaled caps. Returns true if a clamp was applied.
        bool LimitVelocities(VecFloat lvfTimeStep);

        // BrnPhysicalBodyPart.h:324. Transform one skinned bbox control point through the part's
        // current pose. (Uses the homed BBoxPointSkinData spec by const reference.)
        Vector3 CalculateSkinnedPoint(const BBoxPointSkinData& lSkinData);

        // ----- members (DWARF-authoritative order + types) -------------------------------
        //
        // NOTE: maBoxInitialiseBuffer (DWARF BrnPhysicalBodyPart.h:297, `extern char[96]`) is a
        // file-scope STATIC scratch buffer for box initialisation, NOT an instance member -- it
        // is intentionally omitted from the layout and lives in the .cpp.

        ExternalPhysicsBody mRwBody;                               // BrnPhysicalBodyPart.h:326 -- the part's rigid body
        Matrix44Affine      mBBoxOrientation;                     // BrnPhysicalBodyPart.h:327 -- oriented-bbox basis

        // The four Vector3Plus packings each carry a vec3 in xyz + a scalar in the w lane (the
        // "Plus"). The w lanes pack the joint angle/velocity, the max-angle, and the limit stress
        // alongside the joint/graphics/COM/initial-joint positions.
        Vector3Plus mLocalJointPositionPlusRotation;              // BrnPhysicalBodyPart.h:330 -- local joint pos + joint rotation (w)
        Vector3Plus mLocalGraphicsPositionPlusJointVelocity;      // BrnPhysicalBodyPart.h:331 -- local graphics pos + joint velocity (w)
        Vector3Plus mLocalInitialComPositionPlusMaxJointAngle;    // BrnPhysicalBodyPart.h:332 -- initial COM pos + max joint angle (w)
        Vector3Plus mLocalInitialJointPositionPlusLimitStress;    // BrnPhysicalBodyPart.h:333 -- initial joint pos + limit stress (w)

        Vector3     mBoundingBoxHalfDimensions;                   // BrnPhysicalBodyPart.h:334 -- bbox half extents

        // Per-frame collision accumulators (also Vector3Plus: the w lanes pack the collision
        // magnitude and the running collision count for the running average).
        Vector3Plus mWorldPenetrationPlusCollisionMagnitude;      // BrnPhysicalBodyPart.h:335 -- deepest penetration + magnitude (w)
        Vector3Plus mAverageCollisionPointPlusNumCollisions;      // BrnPhysicalBodyPart.h:336 -- avg contact point + num collisions (w)

        BurnoutBodyPartID mRigidBodyId;                           // BrnPhysicalBodyPart.h:337 -- packed part handle / scene id
        EntityId          mGlobalVehicleId;                       // BrnPhysicalBodyPart.h:338 -- owning vehicle global id

        const IKBodyPart*       mpIKPart;                         // BrnPhysicalBodyPart.h:339 -- static IK spec (not owned)
        const DeformableObject* mpDeformableObject;               // BrnPhysicalBodyPart.h:340 -- owning deformable model (not owned)

        bool mbJoinedToVehicle;                                   // BrnPhysicalBodyPart.h:341 -- still attached via joint
        bool mbAddedToScene;                                      // BrnPhysicalBodyPart.h:342 -- volume instance is in the scene
        bool mbFrozen;                                            // BrnPhysicalBodyPart.h:343 -- sim frozen / settled
        bool mbNeedsWritingIntoRenderware;                        // BrnPhysicalBodyPart.h:344 -- transform dirty for RW

        s8   mi8ActiveJointsTagPointIndex;                        // BrnPhysicalBodyPart.h:346 -- tag-point of the active joint
    };
}
}
