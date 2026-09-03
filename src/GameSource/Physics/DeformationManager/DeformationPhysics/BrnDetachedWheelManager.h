#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h
//
// BrnPhysics::Deformation::DetachedWheelManager -- the manager for the pool of
// DETACHED wheels: wheels that have popped off a vehicle and are now free
// rigid-body cylinders the deformation system simulates and renders. Unlike the
// part manager it owns its slots DIRECTLY (no separate pool object): a flat array
// of 20 PhysicalWheel records, a parallel table of each slot's owner entity type
// (so all of one car's wheels can be removed together), and a 20-bit "used" set.
//
// It detaches a wheel into a free slot (DetachWheel), removes a single wheel
// (RemoveWheel) or all of a vehicle's wheels (RemoveVehicleWheels), drives the
// collection each frame (Update / UpdatePostPhysics / UpdateTriangleCache), and
// hands out wheels by entity id or by raw slot index (the GetWheel family).
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnDetachedWheelManager.cpp).
// Member order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnDetachedWheelManager.h,
// the DetachedWheelManager struct @ BrnDetachedWheelManager.h:49). Each member
// carries its DWARF line. The X360 function signatures are reconciled against the
// merged dossier (scratchpad/wave5/dos_detachwheel.txt: Get @0x825A0B10,
// IsSlotUsed @0x825A0A10, RemoveVehicleWheels @0x826299E0, RemoveWheel @0x82615778,
// UpdateTriangleCache @0x8260E9F8).
//
// X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there, members pinned
// BY NAME + SEQUENCE here. GROW additively.
//
// NOTE ON THE EMBEDDED DETACHED-WHEEL RECORD: the prompt asks this header to "home
// its embedded DetachedWheel record" too. The DWARF carries no separate "DetachedWheel"
// struct -- the per-detached-wheel record IS the PhysicalWheel array element
// (maWheels[20]), homed by BrnPhysicalWheel.h (included below). It is therefore homed
// transitively via that include and NOT redefined here (ODR -- do not re-home).
//
// SLOT-MATH ANCHOR (RemoveWheel @0x82615778, asm-attested): the per-wheel record stride
// is 144 bytes on the console (`144*liSlot + a1` = &maWheels[liSlot]); the owner-type
// table is read at this+2880 (`v5[v12 + 720]` -> word table at +2880 == 720*4); the
// used-set BitArray starts at this+2960 (`8*(slot>>6) + a1 + 2960`). These confirm the
// member ORDER below: maWheels[20] (2880 bytes), then maWheelOwnerType[20] (+2880),
// then mUsedWheels (+2960). Host pointer widening means the absolute byte offsets do not
// all reproduce on the 64-bit host; indexing is BY NAME (sizeof(PhysicalWheel) stride).
// ============================================================================

#include "types.hpp"           // s32, u8, u16, u32, f32
#include "BrnCommonTypes.h"    // Matrix44Affine, Vector3, VecFloat, EntityId, RigidBodyId

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h" // PhysicalWheel (array element, BY VALUE)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                             // CgsContainers::BitArray<N>

// ---- forward declarations (cross-TU types referenced only by pointer/reference) ----

// BrnWorld::EEntityTypeID -- the per-slot owner entity type (maWheelOwnerType). Its
// canonical home is GameSource/World/BrnEntityTypes.h (not in-tree yet). A bare
// `enum EEntityTypeID;` is ill-formed C++; the fixed-underlying-type opaque-enum
// declaration below is well-formed and matches the live convention in
// BrnContactSpyRunList.h. FLAG: forward-declared opaque enum (full enumerator set
// not pulled in; only the type identity is needed to size/name the array element).
namespace BrnWorld { enum EEntityTypeID : int; }

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // The scene add/remove interface RemoveWheel / RemoveVehicleWheels / UpdateTriangleCache
    // write through. DWARF spells it OutputBuffer::SceneInputInterface in UpdatePostPhysics;
    // same underlying CgsSceneManager::SceneManagerIO::InSceneUpdateInterface.
    struct InSceneUpdateInterface;
}
}

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    struct InputBuffer;   // ⚠ class-key fixed 2026-08-06 (struct per CgsPhysicsSimulationModuleIO.h:43)   // sim-input event buffer (DetachWheel / RemoveWheel / Update)
    struct OutputBuffer;  // ⚠ class-key fixed 2026-08-06 (struct per CgsPhysicsSimulationModuleIO.h:321)  // sim-output buffer (UpdatePostPhysics)
}
}

// ADDITIVE GROW (FLAG -- declared-only here per the per-TU gate rule "if a callee is not declared
// anywhere, prefer YOUR class header"; bodies owned by the not-yet-homed sim-input / scene
// triangle-cache TUs). Two deep IO callees the detached-wheel .cpp reaches whose full event types
// are NOT homed in-tree; only the de-inlined X360 call-site shape is known. Declared-only so
// BrnDetachedWheelManager.cpp compiles under `cl /c`; the owning TUs emit the authoritative bodies
// (and may relocate/refine these decls). Signatures are PROVISIONAL.
//
//   * The rigid-body removal producer at the head of RemoveWheel @0x82615778 (the asm fetches the
//     sim InputBuffer's InRemoveRigidBody queue -- `sub_825BCF58(lpSimInput)` returns the channel --
//     and InRemoveRigidBody_::AddEvent's the wheel's packed scene-entity id onto it). Modelled as a
//     free hook taking the input buffer + the packed wheel-body id word.
//
// ⛔⛔ `EmitUpdateTriangleCacheEvent` RETIRED 2026-08-27 (detached-part collision wave). It was a
// FABRICATED API -- an "invented arm" of exactly the class this project already documents, and the
// second one found in the detach path (the first was retired in commit 7dca72e4, "the hook it
// replaced was a fabricated API"). CONFIRMED FABRICATED, not merely unhomed, three ways:
//   1. No such symbol exists anywhere in the X360 export set (nor in DecFIGS DWARF, nor Feb-2007).
//   2. What UpdateTriangleCache @0x8260E9F8 actually calls is one `bl` at 0x8260EC28 to
//      CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPositi<on>::AddEvent @0x825E4768,
//      with r3 = <scene interface> + 0xC5290 == mUpdateCachedPositionQueue. That queue, its 32-byte
//      element, its AddEvent instantiation AND the producer that stages the event
//      (InSceneUpdateInterface::UpdateCachedObjectPosition, CgsSceneManagerIO_SceneUpdate.h:342)
//      are ALL committed and mounted -- the producer's own banner has said since 2026-08-18 that
//      "that gate's whole body collapses to one forward to this producer".
//   3. Its SIGNATURE was wrong in a way that proves it was never read off the binary: it took the
//      wheel's 64-bit VolumeInstanceId, but the event's first field is an s32 CACHE SLOT
//      ((handle & 0xFF) + 123 for a wheel, + 73 for a body part -- the same slot the matching
//      AddToScene claims and RemoveFromScene drops). A volume-instance id would have addressed a
//      slot that does not exist.
// The last provisional hook (EmitRemoveRigidBodyEvent) was RETIRED 2026-09-03: RemoveWheel now AddEvents
// the console's InRemoveRigidBody on the sim input buffer directly (RemoveWheel @0x82615778).
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface; } }

namespace BrnPhysics
{
namespace Deformation
{
    // ========================================================================
    // BrnPhysics::Deformation::DetachedWheelManager
    // ========================================================================
    class DetachedWheelManager
    {
    public:
        // Pool capacity -- 20 detached wheels (the BitArray<20u> width and the maWheels extent;
        // RemoveWheel asserts liSlot < KI_MAX_DETACHED_WHEELS == 20, baked
        // BrnDetachedWheelManager.h:162).
        static const s32 KI_MAX_DETACHED_WHEELS = 20;

        // ----- lifecycle -----------------------------------------------------------------

        // BrnDetachedWheelManager.h:53. Construct: clear the used-set (BitArray<20>::UnSetAll).
        void Construct();

        // BrnDetachedWheelManager.h:56. Tear the manager down.
        void Destruct();

        // BrnDetachedWheelManager.h:59. Per-level prepare (clear the used-set).
        bool Prepare();

        // BrnDetachedWheelManager.h:62. Per-level release.
        bool Release();

        // ----- detach / remove -----------------------------------------------------------

        // BrnDetachedWheelManager.h:68 (dossier RemoveVehicleWheels @0x826299E0). Remove EVERY
        // detached wheel whose owner matches lHandlingBodyId's vehicle -- scans the used-set and
        // for each used slot whose maWheelOwnerType/owning vehicle matches, calls RemoveWheel.
        // Called when a deformable car resets or is released.
        void RemoveVehicleWheels(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                 CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                 RigidBodyId lHandlingBodyId);

        // BrnDetachedWheelManager.h:81 (dossier DetachWheel @ BrnDetachedWheelManager.cpp:93).
        // Detach one wheel into a free slot: validate transforms / seed velocities, claim the
        // first free slot (BitArray::GetFirstZeroBit), pack its BurnoutWheelBodyID + RigidBodyId,
        // and Prepare the PhysicalWheel with its cylinder radius/half-height and render transform.
        void DetachWheel(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                         RigidBodyId lHandlingBodyId, u16 lu16DeformableIndex, s32 liWheelIndex,
                         f32 lfHalfHeight, f32 lfRadius,
                         Matrix44Affine lRenderTransform, Matrix44Affine lVehicleTransform,
                         Vector3 lInitialLinearVelocity, Vector3 lInitialAngularVelocity);

        // ----- per-frame drivers ---------------------------------------------------------

        // BrnDetachedWheelManager.h:86. Integrate every used wheel this frame.
        void Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput, VecFloat lvfTimeStep);

        // BrnDetachedWheelManager.h:91. Post-physics pass: forward each post-physics rigid-body
        // update to its wheel and republish the wheel transforms to the scene.
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void UpdatePostPhysics(const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer,
                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // BrnDetachedWheelManager.h:96 (dossier UpdateTriangleCache @0x8260E9F8). Update each
        // live wheel's entry in the scene triangle cache (swept-sphere position/radius derived
        // from its velocity + radius).
        void UpdateTriangleCache(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface);

        // ----- queries / accessors -------------------------------------------------------

        // BrnDetachedWheelManager.h:100. The detached wheel currently owned by global entity id
        // lEntityId (scans the used slots), const. Null if no live wheel matches.
        const PhysicalWheel* GetWheel(EntityId lEntityId) const;

        // BrnDetachedWheelManager.h:105. The li-th detached wheel of the vehicle with entity id
        // lEntityId (const). Used to enumerate one car's detached wheels.
        const PhysicalWheel* GetWheel(EntityId lEntityId, s32 liWheelOrdinal) const;

        // BrnDetachedWheelManager.h:108 (dossier IsSlotUsed @0x825A0A10). Whether slot lu16Slot
        // is currently occupied (mUsedWheels.IsBitSet). Asserts lu16Slot < 20.
        bool IsSlotUsed(u16 lu16Slot);

        // BrnDetachedWheelManager.h:112 (dossier "Get" @0x825A0B10). The detached wheel in a raw
        // pool slot (&maWheels[lu16Slot]; asm `144*slot + this`). Asserts IsSlotUsed(lu16Slot).
        const PhysicalWheel* GetWheel(u16 lu16Slot);

    private:
        // BrnDetachedWheelManager.h:124 (dossier RemoveWheel @0x82615778). Remove the wheel in
        // slot liSlot: remove its rigid body from the sim, remove its volume instance from the
        // scene (if added), and clear the slot's used bit. Asserts liSlot < 20 and that the slot
        // is currently used.
        void RemoveWheel(s32 liSlot, CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                         CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // ----- members (DWARF-authoritative order + types) -------------------------------
        //
        // Order + asm offsets attested by RemoveWheel/RemoveVehicleWheels (see SLOT-MATH ANCHOR
        // in the file banner): maWheels (144-byte stride), then the owner-type word table at
        // this+2880, then the used-set BitArray at this+2960.

        PhysicalWheel        maWheels[KI_MAX_DETACHED_WHEELS];        // BrnDetachedWheelManager.h:126 -- the 20 detached-wheel records (BY VALUE)
        BrnWorld::EEntityTypeID maWheelOwnerType[KI_MAX_DETACHED_WHEELS]; // BrnDetachedWheelManager.h:127 -- each slot's owner entity type (for RemoveVehicleWheels)
        CgsContainers::BitArray<KI_MAX_DETACHED_WHEELS> mUsedWheels;  // BrnDetachedWheelManager.h:128 -- which slots are in use
    };
}
}
