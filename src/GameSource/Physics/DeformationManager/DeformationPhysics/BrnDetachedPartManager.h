#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h
//
// BrnPhysics::Deformation::DetachedPartManager -- the manager for the pool of
// DETACHED body parts: panels/parts that have come off cars and are now free
// rigid bodies the deformation system simulates and renders. It is a thin owner
// over a single embedded PhysicalBodyPartPool (mPartPool, 50 slots): it makes a
// part physical when a car sheds one (MakePartPhysical), removes it back to the
// pool (RemovePart), drives the whole collection each frame (Update /
// UpdatePostPhysics / UpdateTriangleCache / AddPartsToScene), tests still-jointed
// parts for joint breaking (TestJointForBreaking), and emits the detached-part
// render/position events (OutputEvents). CalculateInterest scores a part's
// camera/gameplay "interest" (used to decide which parts to keep when full).
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnDetachedPartManager.cpp).
// Member order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnDetachedPartManager.h,
// the DetachedPartManager struct @ BrnDetachedPartManager.h:47). Each member
// carries its DWARF line. The X360 function signatures are reconciled against the
// merged dossier (scratchpad/wave5/dos_detachpart.txt: MakePar @0x82626E30,
// UpdatePostPhysics @0x8260E118).
//
// X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there, members pinned
// BY NAME + SEQUENCE here. GROW additively.
//
// NOTE ON THE EMBEDDED DETACHED-PART RECORD: the prompt asks this header to "home
// its embedded DetachedPart record" too. The DWARF DetachedPartManager has exactly
// ONE data member -- mPartPool (a PhysicalBodyPartPool) -- and the per-detached-part
// record IS the PhysicalBodyPart array element owned by that pool
// (BrnPhysicalBodyPartPool.h, included below: maParts[50]). There is no separate
// "DetachedPart" struct in the DWARF; PhysicalBodyPart is the detached-part record.
// It is therefore homed transitively via the pool include and NOT redefined here
// (ODR -- do not re-home an already-homed type).
// ============================================================================

#include "types.hpp"           // s32, u8, u16, u32, f32
#include "BrnCommonTypes.h"    // Matrix44Affine, Vector3, VecFloat, EntityId, RigidBodyId

// The embedded pool (BY VALUE) -- brings in PhysicalBodyPart (the detached-part
// record), PhysicalWheel, CgsContainers::BitArray, and the PotentialContactInterface /
// ContactSpyData / output-interface forward-decls the pool already declares.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----
namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // The scene add/remove interface RemovePart / UpdateTriangleCache write through.
    // DWARF spells this as OutputBuffer::SceneInputInterface in some sibling methods;
    // it is the same underlying CgsSceneManager::SceneManagerIO::InSceneUpdateInterface.
    struct InSceneUpdateInterface;
}
}

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    class InputBuffer;   // sim-input event buffer (MakePartPhysical / RemovePart / Update)
    class OutputBuffer;  // sim-output buffer (UpdatePostPhysics / TestJointForBreaking)
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // Already homed elsewhere; referenced here by pointer/ref only:
    //   * PhysicalBodyPart   -- the detached-part record (BrnPhysicalBodyPart.h, via the pool include)
    //   * DeformableObject   -- the owning deformable car (BrnDeformableObject.h)
    //   * IKBodyPart         -- the static IK rig spec a part is built from (BrnIKBodyPart.h)
    //   * PotentialContactInterface / ContactSpyData / DeformationOutputInterface(ForEntityModules)
    //                        -- declared by BrnPhysicalBodyPartPool.h (included above)
    class DeformableObject;          // MakePartPhysical owning-model arg. Owned by BrnDeformableObject.h.
    class IKBodyPart;                // MakePartPhysical IK-spec arg. Owned by BrnIKBodyPart.h.

    // ========================================================================
    // BrnPhysics::Deformation::DetachedPartManager
    // ========================================================================
    class DetachedPartManager
    {
    public:
        // ----- lifecycle -----------------------------------------------------------------

        // BrnDetachedPartManager.h:51. Construct the embedded part pool.
        void Construct();

        // BrnDetachedPartManager.h:54. Tear the manager down.
        void Destruct();

        // BrnDetachedPartManager.h:57. Per-level prepare (clear the pool's used set).
        bool Prepare();

        // BrnDetachedPartManager.h:60. Per-level release.
        bool Release();

        // ----- detach / attach -----------------------------------------------------------

        // BrnDetachedPartManager.h:75 (dossier "MakePar" @0x82626E30). Make a freshly-shed
        // body part PHYSICAL: validate the transforms / seed velocities, then allocate + prepare
        // a free pool slot for it (PhysicalBodyPartPool::Create), binding its deformable model,
        // IK spec, handling-body id, global car id, mesh/part index and initial linear/angular
        // velocity. Returns the new part (null if the pool is full). Called by
        // DeformableObject::DetachPart.
        PhysicalBodyPart* MakePartPhysical(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                           u16 lu16DeformableObjectIndex,
                                           const DeformableObject* lpDeformableObject,
                                           RigidBodyId lHandlingBodyId, EntityId lGlobalCarId,
                                           s32 liPartIndex, const IKBodyPart* lpPart,
                                           Matrix44Affine lLocalRenderTransform,
                                           Matrix44Affine lVehicleTransform,
                                           Vector3 lInitialLinearVelocity,
                                           Vector3 lInitialAngularVelocity);

        // BrnDetachedPartManager.h:81. Release the part in slot lu8Index back to the pool,
        // tearing down its sim + scene presence.
        void RemovePart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                        u8 lu8Index);

        // ----- output --------------------------------------------------------------------

        // BrnDetachedPartManager.h:86. Emit the detached-part render + current-position events
        // for every live part into the two deformation output interfaces. const.
        void OutputEvents(DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
                          DeformationOutputInterface* lpOutput) const;

        // ----- per-frame drivers ---------------------------------------------------------

        // BrnDetachedPartManager.h:91. Integrate every used part this frame (write transforms
        // into RenderWare, apply per-part forces).
        void Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep);

        // BrnDetachedPartManager.h:98. Post-physics pass: forward each post-physics rigid-body
        // update to its part, resolve the still-joined parts' contacts and recompute one bbox.
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void UpdatePostPhysics(const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer,
                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                               ContactSpyData* lpContactSpyData,
                               const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface);

        // BrnDetachedPartManager.h:103. Update each live part's entry in the scene triangle cache
        // (swept-sphere position/radius from its velocity).
        void UpdateTriangleCache(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface);

        // BrnDetachedPartManager.h:106. Add any parts pending scene insertion to the scene.
        // (DWARF arg type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void AddPartsToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // ----- queries / accessors -------------------------------------------------------

        // BrnDetachedPartManager.h:110 / :114. The detached part in a given pool slot (const + mutable).
        const PhysicalBodyPart* GetPartFromIndex(u16 lu16Index) const;
        PhysicalBodyPart*       GetPartFromIndex(u16 lu16Index);

        // BrnDetachedPartManager.h:117. Whether the given pool slot index is currently in use.
        bool IsPartIndexUsed(s32 liIndex);

        // BrnDetachedPartManager.h:124. Test the joint of the part in slot liPartIndex for
        // breaking this frame (consults the post-physics body update), detaching it if the
        // joint exceeded its break threshold.
        bool TestJointForBreaking(s32 liPartIndex,
                                  CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                  CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput);

        // BrnDetachedPartManager.h:129. Debug consistency check: every live joint references a
        // live part and vice versa (DEBUG-only tripwire).
        void DebugCheckJointPartConsistancy();   // (DWARF spelling: Consistancy)

    private:
        // BrnDetachedPartManager.h:139. Score a part's camera/gameplay "interest" (used to pick
        // which detached parts to keep alive when the pool is under pressure).
        f32 CalculateInterest(const PhysicalBodyPart* lpPart);

        // ----- members (DWARF-authoritative order + types) -------------------------------
        //
        // The DWARF DetachedPartManager has exactly one member: the embedded pool of detached
        // parts. The pool owns the PhysicalBodyPart[50] array -- the per-detached-part records.

        PhysicalBodyPartPool mPartPool;   // BrnDetachedPartManager.h:136 -- the 50-slot detached-part pool
    };
}
}
