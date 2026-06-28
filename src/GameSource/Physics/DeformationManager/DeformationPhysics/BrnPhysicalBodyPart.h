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
#include "BrnCommonTypes.h"    // Vector3, Vector3Plus, Matrix44Affine, VecFloat, EntityId, RigidBodyId

#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"                       // ExternalPhysicsBody (embedded BY VALUE)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h" // BurnoutBodyPartID (embedded BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                       // CgsSceneManager::VolumeInstanceId (returned BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"                               // CgsSceneManager::VolumeId (returned BY VALUE)

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----
// Per project rule these are NOT included (their definitions are other agents' homes);
// PhysicalBodyPart only stores/passes them by pointer or reference.
namespace CgsGeometric { class Box; }   // GetBoundingBox out-param (DWARF CgsGeometric::Box*)

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
    class InputBuffer;                   // sim-input event buffer (AddToSim/UpdateRW/...)
    class OutputBuffer;                  // sim-output buffer; OutputBuffer::SceneInputInterface
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
//     builds a DetachedPartNotificationEvent and AddEventSafeAppend's it onto the sim OutputBuffer's
//     notification queue). Modelled as a free hook taking the output buffer + the packed event blob.
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface; } }
namespace CgsPhysics { namespace PhysicsSimulationIO { class OutputBuffer; } }
namespace BrnPhysics
{
namespace Deformation
{
    void RemoveTriangleCacheSlot(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                                 u16 lu16TriangleCacheSlot);                                   // FLAG: provisional
    void EmitDetachedPartNotification(CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
                                      const void* lpEventBlob);                                 // FLAG: provisional
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // Static IK rig data for this part (its joints + bbox skin + graphics transform). The
    // physical part holds a const pointer back to its spec. Owned by BrnIKBodyPart.h.
    class IKBodyPart;

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
        Matrix44Affine GetRenderTransform() const;

        // BrnPhysicalBodyPart.h:145. The raw rigid-body (physics) transform.
        Matrix44Affine GetRigidBodyTransform() const;

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
        BurnoutBodyPartID GetRigidBodyId() const;

        // BrnPhysicalBodyPart.h:164. The owning vehicle's global entity id (mGlobalVehicleId).
        EntityId GetGlobalEntityId() const;

        // BrnPhysicalBodyPart.h:167. The render mesh index of this part (from its IK spec).
        s32 GetMeshIndex() const;

        // BrnPhysicalBodyPart.h:170. Whether the part's volume instance is currently in the scene.
        bool IsAddedToScene() const;

        // ----- scene membership ----------------------------------------------------------

        // BrnPhysicalBodyPart.h:174. Add the part's collision volume instance to the scene.
        void AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // BrnPhysicalBodyPart.h:178. Remove it from the scene.
        void RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // ----- simulation ----------------------------------------------------------------

        // BrnPhysicalBodyPart.h:185. Register the part as a free rigid body in the physics sim,
        // seeding it with a world transform + linear/angular velocity (used when the part detaches).
        void AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, Matrix44Affine lTransform,
                      Vector3 lLinearVelocity, Vector3 lAngularVelocity);

        // BrnPhysicalBodyPart.h:188. The part's current linear velocity.
        Vector3 GetLinearVelocity() const;

        // BrnPhysicalBodyPart.h:193. Push the part's transform into RenderWare for this timestep.
        void UpdateRW(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep);

        // ----- pool / spec indices -------------------------------------------------------

        // BrnPhysicalBodyPart.h:196. This part's slot index inside its PhysicalBodyPartPool.
        u8 GetPoolIndex() const;

        // BrnPhysicalBodyPart.h:200. The index of this part's IK spec within the deformable model.
        s32 GetIKPartIndex() const;

        // ----- collision geometry --------------------------------------------------------

        // BrnPhysicalBodyPart.h:205. Write the part's world oriented bounding box out.
        void GetBoundingBox(CgsGeometric::Box* lpBoxOut) const;

        // BrnPhysicalBodyPart.h:208. The embedded externally-simulated physics body.
        ExternalPhysicsBody* GetExternalBody();

        // BrnPhysicalBodyPart.h:212. A bounding sphere radius covering the box (broad-phase).
        f32 GetSphereRadius() const;

        // BrnPhysicalBodyPart.h:215. The part's world position (rigid-body translation).
        Vector3 GetPosition() const;

        // ----- spec / state accessors ----------------------------------------------------

        // BrnPhysicalBodyPart.h:218. The static IK spec backing this part.
        const IKBodyPart* GetIKPart() const;

        // BrnPhysicalBodyPart.h:222. Whether the part is frozen (sim disabled, settled).
        bool IsFrozen() const;

        // BrnPhysicalBodyPart.h:226. Whether the part still needs adding to the scene.
        bool NeedsAddingToScene() const;

        // BrnPhysicalBodyPart.h:230. Flag that the part's transform must be written into RW.
        void SetNeedsWritingToRW();

        // BrnPhysicalBodyPart.h:235. Recompute + republish the part's bounding box to the scene.
        void UpdateBoundingBox(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPart.h:238. The triangle-cache slot this part's collision mesh occupies.
        u16 GetTriangleCacheSlot() const;

        // ----- joint model (attached-part deformation) -----------------------------------

        // BrnPhysicalBodyPart.h:242. Whether the part is still joined to its vehicle (vs detached).
        bool IsJoinedToVehicle() const;

        // BrnPhysicalBodyPart.h:250. Join the part to the vehicle as an active joint: seed the
        // local joint position/COM, the limit stress, and the active-joints tag-point index.
        void SetJoinedToVehicle(Vector3 lLocalJointPosition, Vector3 lLocalComPosition,
                                VecFloat lvfLimitStress, s32 liActiveJointsTagPointIndex);

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
        bool TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                  CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput);

        // BrnPhysicalBodyPart.h:285. Write this part's contact-spy debug record.
        void AddContactSpy(ContactSpyData* lpContactSpyData);

        // BrnPhysicalBodyPart.h:288. Recompute the local bounding box (no scene publish).
        void UpdateBoundingBox();

        // BrnPhysicalBodyPart.h:292. Set the packed joint angular velocity.
        void SetJointVelocity(VecFloat lvfJointVelocity);

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
