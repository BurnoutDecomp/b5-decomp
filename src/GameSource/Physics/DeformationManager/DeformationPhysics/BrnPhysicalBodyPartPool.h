#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h
//
// BrnPhysics::Deformation::PhysicalBodyPartPool -- the fixed-capacity pool of detached
// deformable body parts. It owns a flat array of 50 PhysicalBodyPart slots plus a 50-bit
// "used" set, hands out free slots (CreatePart) and returns them (RemovePart), and drives
// the whole collection each frame: pushing post-physics rigid-body updates out to the
// scene (UpdatePart), trickling one bounding-box recompute per frame (UpdateABoundingBox),
// adding pending parts to the scene (AddPartsToScene), writing transforms into RenderWare
// (UpdateRWBodies), resolving the still-joined parts' contacts (UpdateJoinedParts), and
// emitting the detached-part render/position events (OutputEvents).
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnPhysicalBodyPartPool.cpp).
// Member order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnPhysicalBodyPartPool.h, the
// PhysicalBodyPartPool struct @ BrnPhysicalBodyPartPool.h:58). Each member carries its line.
//
// The pool owns PhysicalBodyPart (and the sibling PhysicalWheel pool lives alongside this
// family), so this header MAY include BrnPhysicalBodyPart.h / BrnPhysicalWheel.h (all three
// are authored together). X360 ARTIST.XEX (big-endian) layout; members pinned BY NAME +
// SEQUENCE. GROW additively.
// ============================================================================

#include "types.hpp"           // s32, u8, u16, u32, f32
#include "BrnCommonTypes.h"    // Vector3, Matrix44Affine, VecFloat, EntityId, RigidBodyId

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"  // PhysicalBodyPart (array element, BY VALUE)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"     // PhysicalWheel (sibling detached-wheel type)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                                 // CgsContainers::BitArray<N>

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----
namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct InSceneUpdateInterface;   // scene add/remove interface (RemovePart)
}
}

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    struct InputBuffer;   // ⚠ class-key fixed 2026-08-06 (struct per CgsPhysicsSimulationModuleIO.h:43)               // sim-input buffer (CreatePart/RemovePart/UpdateRWBodies)
}
}

namespace BrnPhysics
{
namespace Deformation
{
    // A single deformation potential-contact record the joint resolves against. Already
    // forward-declared by BrnPhysicalBodyPart.h (AddContact's arg type). The pool reads two
    // asm-attested fields off each record before forwarding it to PhysicalBodyPart::AddContact:
    //   muVolumeInstanceIdA -- the packed volume-instance id whose owner tag must be 6
    //                          (RACECAR_DEFORMABLE_PART) or 7 (TRAFFIC_DEFORMABLE_PART)
    //   muPartIndex         -- the destination pool slot index (< KU_MAX_DETACHED_PARTS)
    // ADDITIVE GROW (own family): the full record is not homed anywhere; this minimal
    // definition carries only the two fields the pool touches plus an opaque tail so AddContact
    // can take it by const reference. FLAG: reconstructed-from-asm, fields best-effort.
    // FLAG: field offsets are SEMANTIC, not byte-exact -- the real X360 record is ~40 bytes and
    // reads muVolumeInstanceIdA at +0 (owner byte == HIBYTE of the high dword) and the destination
    // part index at byte +64 (`*(v45 + 64)`). This minimal model exposes only those two fields by
    // name (plus an opaque tail) so the pool can read them and forward the record; the exact on-disk
    // offsets are deferred to whichever TU eventually homes the full PotentialContact.
    struct PotentialContact
    {
        u64 muVolumeInstanceIdA;   // owner byte == HIBYTE of the high dword
        u32 muVolumeInstanceIdB;
        s32 muPartIndex;           // destination pool slot (X360 record byte +64)
        u8  maOpaque[28];          // remaining record payload (handed through verbatim)
    };
}

namespace PhysicsModuleIO
{
    // The per-frame potential-contact source UpdateJoinedParts reads the hinged-body-part
    // contact queues from (DWARF: BrnPhysics::PhysicsModuleIO::PotentialContactInterface).
    //
    // ⛔ RENAMED 2026-08-06 (bridge de-facade wave): this modelled slice used to CLAIM the real
    // class's name (`class BrnPhysics::PhysicsModuleIO::PotentialContactInterface`) while the
    // AUTHORITATIVE type -- `struct PotentialContactInterface : CgsModule::IOBuffer` with 14
    // EventQueue<CgsSceneManager PotentialContact,2048> custom queues -- is homed in
    // BrnPhysicsModuleIO_PotentialContactInterface.h. The moment one TU included both headers
    // (the de-facaded bridge TU does, through BrnPhysicsModule.h -> BrnDeformationManager.h ->
    // this header), the fork met as a hard C2011 -- the predicted collision in both headers'
    // own NOTEs. Renamed to ...Model so the fork can never meet again; UpdateJoinedParts'
    // DWARF-real signature is restored when this model is rehomed onto the authoritative type
    // (whose maCustomEventQueues[1]/[2] seats -- +163872/+327728 -- are exactly the two queue
    // bases the pool asm reads).
    //
    // ADDITIVE GROW (own family): the X360 asm walks each queue as {count @ +8, PotentialContact
    // array @ +0} and reads the two queues at fixed offsets off the interface base (the world
    // queue and the car queue). This minimal definition models exactly that shape so
    // UpdateJoinedParts can iterate it through declared accessors. FLAG: reconstructed-from-asm,
    // member offsets best-effort.
    class PotentialContactInterfaceModel
    {
    public:
        struct CustomPotentialContactQueue
        {
            const BrnPhysics::Deformation::PotentialContact* GetContact(s32 liIndex) const
            {
                return &mpContacts[liIndex];
            }
            s32 GetNumContacts() const { return miNumContacts; }

            const BrnPhysics::Deformation::PotentialContact* mpContacts;  // +0
            s32 mu32Pad;                                                  // +4
            s32 miNumContacts;                                            // +8
        };

        // The two hinged-body-part potential-contact queues (still-joined parts vs. the world,
        // and vs. other cars). X360 reads them at the interface's two fixed sub-offsets.
        const CustomPotentialContactQueue* GetHingedBodyPartWithWorldQueue() const;
        const CustomPotentialContactQueue* GetHingedBodyPartWithCarQueue() const;
    };
}

namespace Deformation
{
    // Already forward-declared by BrnPhysicalBodyPart.h (re-stated here for local clarity):
    struct IKBodyPart;                // static IK rig spec (CreatePart arg). Owned by BrnIKBodyPart.h.
    class DeformableObject;          // owning deformable model (CreatePart arg). Owned by BrnDeformableObject.h.
    struct OutUpdateRigidBody;       // per-frame rigid-body update event (UpdatePart). FLAG: forward-declared.

    // The contact-spy debug sink UpdateJoinedParts forwards per-part. Owned by the contact-spy TU.
    // FLAG: forward-declared.
    struct ContactSpyData;

    // The two deformation output sinks OutputEvents fills (detached-part render events +
    // current-position events). Homed under DeformationManager/SharedIO. FLAG: forward-declared
    // (referenced by pointer only).
    class DeformationOutputInterface;
    // ⚠️ class-key matters to the MSVC mangling: the real type is a STRUCT
    // (BrnDeformationOutputInterface.h) -- a `class` fwd here made OutputEvents' definition and
    // its callers mangle to two different symbols (found by the link, 2026-08-24).
    struct DeformationOutputInterfaceForEntityModules;

    class PhysicalBodyPartPool
    {
    public:
        // Pool capacity -- 50 detached parts (the BitArray<50u> width and the maParts extent;
        // the X360 asserts liPartIndex < KU_MAX_DETACHED_PARTS == 50).
        static const u32 KU_MAX_DETACHED_PARTS = 50;

        // ----- lifecycle -----------------------------------------------------------------

        // BrnPhysicalBodyPartPool.h:62. Construct every part slot and clear the used-set.
        void Construct();

        // ----- slot allocation -----------------------------------------------------------

        // BrnPhysicalBodyPartPool.h:76. Allocate + prepare a free part slot for a newly detached
        // body part (binds its deformable model, IK spec, ids, transforms, and seed velocities),
        // registering it with the sim. Returns the part, or null if the pool is full.
        PhysicalBodyPart* CreatePart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                     u16 lu16IKPartIndex, const DeformableObject* lpDeformableObject,
                                     RigidBodyId lRigidBodyId, EntityId lGlobalVehicleId, s32 liMeshIndex,
                                     const IKBodyPart* lpIKPart, Matrix44Affine lGraphicsTransform,
                                     Matrix44Affine lBBoxOrientation, Vector3 lLinearVelocity,
                                     Vector3 lAngularVelocity);

        // BrnPhysicalBodyPartPool.h:82. Release the part in slot lu8Index back to the pool,
        // tearing down its sim/scene presence.
        void RemovePart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                        u8 lu8Index);

        // ----- queries / accessors -------------------------------------------------------

        // BrnPhysicalBodyPartPool.h:85. The number of slots (pool capacity).
        s32 GetSize();

        // BrnPhysicalBodyPartPool.h:89 / :93. The part in a given slot (const + mutable).
        const PhysicalBodyPart* GetPart(s16 li16Index) const;
        PhysicalBodyPart*       GetPart(s16 li16Index);

        // ----- per-frame drivers ---------------------------------------------------------

        // BrnPhysicalBodyPartPool.h:98. Apply one post-physics rigid-body update event to the
        // matching part, republishing its transform to the scene.
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void UpdatePart(const OutUpdateRigidBody* lpUpdateEvent,
                        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPartPool.h:102. Recompute + republish ONE part's bounding box this frame
        // (round-robins via miLastUpdatedBoundingBox to amortise the cost).
        void UpdateABoundingBox(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPartPool.h:106. Add any parts pending scene insertion to the scene.
        void AddPartsToScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnPhysicalBodyPartPool.h:111. Write every used part's transform into RenderWare.
        void UpdateRWBodies(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep);

        // BrnPhysicalBodyPartPool.h:117. Resolve the still-joined parts: pull the per-part
        // potential contacts from the contact interface, accumulate them (PhysicalBodyPart::
        // AddContact), integrate each active joint, and forward to the contact spy.
        void UpdateJoinedParts(const PhysicsModuleIO::PotentialContactInterfaceModel* lpPotentialContactsInterface,
                               ContactSpyData* lpContactSpyData, VecFloat lvfTimeStep);

        // BrnPhysicalBodyPartPool.h:120. Whether the given slot index is currently in use.
        bool IsPartIndexUsed(s32 liIndex);

        // BrnPhysicalBodyPartPool.h:126. Emit the detached-part render + current-position events
        // for every used part into the two deformation output interfaces.
        void OutputEvents(DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
                          DeformationOutputInterface* lpOutput) const;

    private:
        // ----- members (DWARF-authoritative order + types) -------------------------------

        PhysicalBodyPart                     maParts[KU_MAX_DETACHED_PARTS]; // BrnPhysicalBodyPartPool.h:132 -- the 50 part slots
        CgsContainers::BitArray<KU_MAX_DETACHED_PARTS> mUsedParts;           // BrnPhysicalBodyPartPool.h:133 -- which slots are in use
        s32                                  miLastUpdatedBoundingBox;       // BrnPhysicalBodyPartPool.h:134 -- bbox round-robin cursor
        u8                                   mu8NumDetachedParts;            // BrnPhysicalBodyPartPool.h:135 -- count of live parts
    };
}
}
