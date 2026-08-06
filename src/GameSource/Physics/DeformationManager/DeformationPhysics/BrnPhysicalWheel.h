#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h
//
// BrnPhysics::Deformation::PhysicalWheel -- the physics/simulation side of ONE detached
// wheel of a vehicle. It is the small sibling of PhysicalBodyPart: a wheel that has
// popped off the car and is now a free externally-simulated cylinder the deformation
// system keeps in the scene. Unlike a body part it has no joint model (wheels detach
// outright, they do not bend), so PhysicalWheel is a much leaner type -- it caches its
// render transform, its cylinder dimensions (radius + half-height), its linear velocity,
// and its packed wheel id, plus the frozen/in-scene flags.
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnPhysicalWheel.cpp). Member
// order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnPhysicalWheel.h, the
// PhysicalWheel struct @ BrnPhysicalWheel.h:121). Each member carries its DWARF line.
//
// X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there, members pinned BY
// NAME + SEQUENCE here. GROW additively.
// ============================================================================

#include "types.hpp"           // u8, u16, s32, f32
#include "BrnCommonTypes.h"    // Vector3, Matrix44Affine, VecFloat, EntityId

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h" // BurnoutWheelBodyID (embedded BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                       // CgsSceneManager::VolumeInstanceId (returned BY VALUE)
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"                               // CgsSceneManager::VolumeId (returned BY VALUE)

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----
namespace CgsGeometric { class Cylinder; }   // GetCylinder out-param (DWARF Cylinder&)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct InSceneUpdateInterface;   // scene add/remove interface
}
}

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    class InputBuffer;               // sim-input event buffer (Release/AddToSim)
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // Per-frame physics solver event Update consumes (the post-physics rigid-body update). Owned
    // by the sim-output TU; referenced by pointer only. FLAG: forward-declared.
    struct OutUpdateRigidBody;

    class PhysicalWheel
    {
    public:
        // ----- lifecycle -----------------------------------------------------------------

        // BrnPhysicalWheel.h:125. Zero/identity-initialise the wheel (cylinder buffer + flags).
        void Construct();

        // BrnPhysicalWheel.h:132. Bind the detached wheel: its packed id, cylinder radius +
        // half-height, and its initial render transform.
        void Prepare(BurnoutWheelBodyID lWheelBodyId, f32 lfRadius, f32 lfHalfHeight,
                     Matrix44Affine lRenderTransform);

        // BrnPhysicalWheel.h:137. Tear down: remove from the sim + scene.
        void Release(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                     CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // ----- transforms ----------------------------------------------------------------

        // BrnPhysicalWheel.h:140. The cached render-space transform (DWARF returns
        // const rw::math::vpu::Matrix44Affine*).
        const Matrix44Affine* GetRenderTransform() const;

        // BrnPhysicalWheel.h:143. The rigid-body transform (DWARF returns
        // const rw::math::vpu::Matrix44Affine&).
        const Matrix44Affine& GetRigidBodyTransform() const;

        // ----- per-frame update ----------------------------------------------------------

        // BrnPhysicalWheel.h:147. Apply a post-physics rigid-body update event, republishing the
        // transform to the scene (and adding/removing from the scene as the frozen flag flips).
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void Update(const OutUpdateRigidBody* lpUpdateEvent,
                    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalWheel.h:151. Force the rigid-body transform and republish to the scene.
        void SetRigidBodyTransform(Matrix44Affine lTransform,
                                   CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // ----- identity ------------------------------------------------------------------

        // BrnPhysicalWheel.h:154. This wheel's scene-entity id (from mWheelBodyId).
        EntityId GetEntityId() const;

        // BrnPhysicalWheel.h:157. The packed wheel-body id.
        BurnoutWheelBodyID GetWheelBodyId() const;

        // ----- scene membership ----------------------------------------------------------

        // BrnPhysicalWheel.h:160. Add the wheel's collision volume instance to the scene.
        void AddToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // BrnPhysicalWheel.h:163. Remove it from the scene.
        void RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput);

        // ----- simulation ----------------------------------------------------------------

        // BrnPhysicalWheel.h:169. Register the wheel as a free rigid body in the physics sim,
        // seeding world transform + linear/angular velocity.
        void AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, Matrix44Affine lTransform,
                      Vector3 lLinearVelocity, Vector3 lAngularVelocity);

        // BrnPhysicalWheel.h:172. The wheel's current linear velocity (mLinearVelocity).
        Vector3 GetLinearVelocity() const;

        // ----- scene ids / pool ----------------------------------------------------------

        // BrnPhysicalWheel.h:175. The packed scene volume-instance id of this wheel.
        // ⭐ INLINE 2026-08-06 (bridge de-facade wave): no out-of-line emission exists --
        // DeformationManager::FixupWheelVehicleContact @0x825A0F80 `ld`s the whole 8-byte
        // packed record at wheel+0x70 (mWheelBodyId) as the volume-instance id. The host
        // repacks the three fields in the console's byte order (entity word = high dword).
        CgsSceneManager::VolumeInstanceId GetVolumeInstanceId() const
        {
            CgsSceneManager::VolumeInstanceId lId;
            lId.muId = (static_cast<u64>(mWheelBodyId.muEntityWord) << 32)
                     | (static_cast<u64>(mWheelBodyId.muSubA) << 16)
                     |  static_cast<u64>(mWheelBodyId.muSubB);
            return lId;
        }

        // BrnPhysicalWheel.h:178. This wheel's slot index inside its pool.
        u8 GetPoolIndex() const;

        // BrnPhysicalWheel.h:182. The triangle-cache slot this wheel's collision mesh occupies.
        u16 GetTriangleCacheSlot() const;

        // ----- queries / geometry --------------------------------------------------------

        // BrnPhysicalWheel.h:186. Whether the wheel is frozen (settled, sim disabled).
        bool IsFrozen() const;

        // BrnPhysicalWheel.h:191. Write the wheel's world collision cylinder out.
        void GetCylinder(CgsGeometric::Cylinder& lCylinderOut) const;

        // BrnPhysicalWheel.h:194. Whether the wheel's volume instance is currently in the scene.
        bool IsAddedToScene() const;

        // BrnPhysicalWheel.h:198. The wheel radius (mfRadius).
        f32 GetRadius() const;

    private:
        // BrnPhysicalWheel.h:206. The packed scene volume id (shared collision volume).
        CgsSceneManager::VolumeId GetVolumeId() const;

        // ----- members (DWARF-authoritative order + types) -------------------------------
        //
        // NOTE: maCylinderInitialiseBuffer (DWARF BrnPhysicalWheel.h:202, `extern char[96]`) is a
        // file-scope STATIC scratch buffer, NOT an instance member -- omitted here, lives in .cpp.

        Matrix44Affine     mRenderTransform;        // BrnPhysicalWheel.h:208 -- cached render-space transform
        Vector3            mComOffset;              // BrnPhysicalWheel.h:209 -- centre-of-mass offset
        Vector3            mBoundingBoxDimensions;  // BrnPhysicalWheel.h:210 -- bbox extents
        Vector3            mLinearVelocity;         // BrnPhysicalWheel.h:211 -- last linear velocity
        BurnoutWheelBodyID mWheelBodyId;            // BrnPhysicalWheel.h:212 -- packed wheel handle / scene id
        f32                mfHalfHeight;            // BrnPhysicalWheel.h:213 -- cylinder half-height
        f32                mfRadius;                // BrnPhysicalWheel.h:214 -- cylinder radius
        bool               mbFrozen;                // BrnPhysicalWheel.h:215 -- sim frozen / settled
        bool               mbAddedToScene;          // BrnPhysicalWheel.h:216 -- volume instance is in the scene
    };
}
}
