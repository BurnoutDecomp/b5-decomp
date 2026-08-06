#pragma once

// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager.h
//
// BrnPhysics::Deformation::DeformationManager -- the per-frame deformation
// ORCHESTRATOR. It is the singleton-ish manager (one per PhysicsModule) that owns
// every deformable car model and drives the whole deformation/detachment system:
//
//   * It owns a fixed pool of up to 28 DeformableObject models (mpaModels), the
//     shared DeformationState output (mStateOutput), the detached-PART manager
//     (mDetachedPartManager) and the detached-WHEEL manager (mDetachedWheelManager),
//     plus the per-car id / index lookup tables (maGlobalEntityIDs[28],
//     ma8RaceCarToModelIndex[8], ma8TrafficToModelIndex[20],
//     ma8GlobalTrafficToModelIndex[601]) and a private RNG (mRandom).
//   * Each sim step it processes the deformation event queues (add/remove/deactivate/
//     validate/reset models), reads potential contacts into the per-car sensors,
//     runs the sensor displacement + per-model update, then the post-physics pass
//     (penetration solve, joint spies, detached-part/wheel updates) and finally
//     outputs the deformation state to the entity modules.
//   * It also fixes up vehicle-vs-vehicle / part / wheel contacts, generates the
//     body-part + detached-wheel world contacts, builds the deformation primitive
//     pairs, and answers per-car spatial queries (swept spheres, deformed bbox).
//
// This is a LAYOUT-FIRST FROZEN HEADER: full DWARF member layout + every method
// declaration, NO function bodies (other agents author BrnDeformationManager.cpp).
// Member order/types and the method set are DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../DeformationManager/BrnDeformationManager.h, the
// DeformationManager struct @ BrnDeformationManager.h:73). Each member carries its
// DWARF line. The X360 function set is reconciled against the merged dossier
// (scratchpad/wave5/dos_manager.txt: Construct @0x82621510, Destruct @0x82603F78,
// OutputData @0x826225D8, CreateDetachedWheelContactEvent @0x825B95B0, ...).
//
// X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there, members pinned
// BY NAME + SEQUENCE here. The console build is 32-bit; this PC build is x64, so
// embedded pointer-bearing sub-objects (the two managers, DeformationState) widen
// and the absolute byte offsets do NOT all reproduce on the host -- every access is
// BY MEMBER NAME, so the host size is not load-bearing for the per-TU gate. GROW
// additively.
// ============================================================================

#include "types.hpp"           // s32, s8, u8, u16, u32, f32
#include "BrnCommonTypes.h"    // Vector3, Matrix44Affine, VecFloat, EntityId
// ⛔⛔ THE RIGID-BODY HANDLE HERE IS EIGHT BYTES, AND UNTIL 2026-08-04 THIS HEADER MADE IT FOUR
// (task #141). BrnCommonTypes.h:28 declares a GLOBAL-namespace stand-in `struct RigidBodyId
// { u32 muValue; }`, and the three uses below were UNQUALIFIED, so inside
// `namespace BrnPhysics::Deformation` they all bound to that 4-byte stand-in instead of the real
// 8-byte `CgsPhysics::RigidBodyId`.
// ⛔⛔ ONLY THESE THREE ARE FIXED. THE NAME IS STILL USED FOR A DIFFERENT, 32-BIT HANDLE
// ELSEWHERE IN THIS SUBSYSTEM, AND THAT IS *NOT* SAFE TO "FINISH" WITHOUT ASM WORK.
// BrnDeformationEvents.h types five separate `mHandlingBodyID` fields as the same unqualified
// `RigidBodyId`, and the committed bodies in BrnDeformationManager.cpp do genuinely 32-BIT
// arithmetic on them -- `mHandlingBodyID.muValue >> 24` for the owner type,
// `(... >> 10) & 0x3FFF` for the race-car/traffic index, and at :347/:413 they convert the whole
// thing to a 32-bit `EntityId`. `.muValue` exists ONLY on the stand-in, and that bit layout
// CONTRADICTS CgsRigidBody.h's documented packing (high dword = EntityId, low dword = index).
// So the handling-body handle may legitimately be a different 32-bit id, or those bodies may be
// wrong -- the DWARF spells both `RigidBodyId` and cannot separate them. ⚠️ Deciding it needs
// the X360 asm for AddDeformationModel @0x825A95E0's caller and the Process*Events drains.
// mWorldRigidBodyId is fixed here because its width is settled INDEPENDENTLY by console
// adjacency (below) and because nothing reads or writes it yet, so the change is inert.
// ⭐ THE CONSOLE ADJACENCY IS DECISIVE -- and it closes with ZERO padding only on 8 bytes:
//     mModelsAdded      @ +75904  (BrnDeformationDebugComponent.cpp:736)  BitArray<28> = 8B
//     maGlobalEntityIDs @ +75912  = 75904 + 8                             28 * 4 = 112B
//     mWorldRigidBodyId @ +76024  = 75912 + 112                           <-- 8 bytes
//     mpaModels         @ +76032  = 76024 + 8   ** THE ATTESTED OFFSET ** (Contacts.cpp:13,
//                                                DebugComponent.cpp:737)
//   At 4 bytes the chain lands mpaModels on +76028 and misses its attested seat by four.
//   The DWARF agrees independently: it spells this member's type `RigidBodyId` at
//   BrnDeformationManager.h:364 with the SAME spelling it uses at BrnPhysicsModule.h:83, and
//   that one is already proven 8 bytes by a hard `stdx` plus a committed static_assert
//   (BrnPhysicsModule_layout_check.cpp). There is only one real `CgsPhysics::RigidBodyId` -- the
//   BrnCommonTypes.h struct is a reconstruction stand-in, not a game type.
// ⚠️ WHY IT MATTERED: the handle packs the owning EntityId in the HIGH 32 bits and the body
// index in the LOW 32 (CgsRigidBody.h:3-5), so a 4-byte seat keeps the index and discards the
// owning EntityId with no diagnostic. It was latent only because SetWorldBodyId /
// FindModelIndexByPartID are still
// declaration-only -- nothing writes the member yet.
// ⭐ THE GATE THAT WOULD HAVE CAUGHT IT DID NOT EXIST. It does now, and it is in
// **BrnDeformationManager_Construct.cpp** (DeformationManager::_AssertLayout) -- NOT in
// BrnDeformationManager.cpp, which is UNMOUNTED and where a gate is never compiled. That mistake
// was made and caught by a tamper test on 2026-08-04; see the banner over the gate.
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"   // the REAL CgsPhysics::RigidBodyId (8B)

// ---- homed sub-objects embedded BY VALUE (full layout required) --------------------
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"                  // DeformationState (mStateOutput)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"      // DetachedPartManager (mDetachedPartManager)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"     // DetachedWheelManager (mDetachedWheelManager)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                                             // CgsNumeric::Random (mRandom)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                                        // CgsContainers::BitArray<28> (mModelsAdded)

// ---- homed types referenced by pointer/value in the public API ---------------------
#include "GameSource/Physics/ContactSpies/BrnContactId.h"   // BrnPhysics::BrnPhysics::ContactId (ReadPotentialContact arg)

// ADDITIVE GROW (flagged by the mgr-contacts body group): the per-frame contact spine needs the
// real IO-buffer-stack + per-update scene-input-buffer APIs (SolvePenetration's CreateIOBuffer/
// DestroyIOBuffer; UpdateTriangleCache's GetSceneUpdateInterface). The two placeholders below
// (lines ~157/158) were bare local forward-decls -- they are now ALIASED to the real homed types so
// the method signatures resolve to them. No layout change (the manager only ever takes these by
// pointer). FLAG: minimal additive include + alias, strictly required for the contact spine to gate.
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                                         // CgsModule::IOBufferStack
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsInputBufferUpdate.h"                      // CgsSceneManager::SceneManagerIO::InputBuffer_Update

// ---- forward declarations (cross-subsystem types referenced only by pointer/ref) ---
// Spelled with the qualified namespace their real home uses (proven by the sibling
// BrnDeformableObject.h / BrnPhysicalBodyPart.h). Where the DWARF spells a NESTED type
// (e.g. OutputBuffer::SceneInputInterface), the comment records the DWARF spelling and the
// signature uses the underlying forward-declared type.
namespace rw { class IResourceAllocator; }   // Prepare's allocator arg (DWARF rw::IResourceAllocator*)

namespace CgsGeometric
{
    class Box;          // GetDeformedBBox out-param (DWARF CgsGeometric::Box*); no CgsBox.h in-tree -> forward-decl
    struct Sphere;      // GetSpheresForCar element (DWARF Sphere)
    struct SweptSphere; // GetSweptSpheresForCar element (DWARF SweptSphere)
}

namespace CgsMemory { class SimpleDataStreamProducer; class LinearMalloc; }

namespace CgsSceneManager
{
    struct VolumeInstanceId;   // == VolumeInstanceId (contact fix-up / pair-build args)
    namespace SceneManagerIO
    {
        struct InSceneUpdateInterface;   // == OutputBuffer::SceneInputInterface (DWARF)
        struct PotentialContact;         // CgsSceneManager contact record (ReadPotentialContact / fix-up args)
        struct InAddPotentialContact;    // SetupPartContact out-param (DWARF InAddPotentialContact&)
    }
}

namespace CgsPhysics
{
    // ⚠ CLASS-KEYS FIXED 2026-08-06 (big-five #2): both are `struct` in the authoritative
    // CgsPhysicsSimulationModuleIO.h (:43/:321); `class` here silently mangled every
    // cross-TU signature carrying them (?AV vs ?AU) -- found by an LNK2019 pair.
    namespace PhysicsSimulationIO { struct InputBuffer; struct OutputBuffer; }   // sim IO buffers
    class CollisionGenerator;   // contact-generation arg (same qualified home as BrnDeformableObject.h)
}

namespace BrnPhysics
{
// The triangle-cache / contact-gen list the world-contact-generation methods feed. Owned by
// the physics contact-gen TU; referenced by pointer only. FLAG: forward-declared (home not
// in-tree). Same spelling as BrnDeformableObject.h.
struct ContactGenList;

// The deformation manager's debug component type. Its full home (BrnDeformationDebugComponent.h)
// is NOT in-tree yet; the manager touches it only by pointer/ref + a static instance, so a
// forward declaration suffices. FLAG: forward-declared.
namespace Deformation { class DeformationDebugComponent; }

namespace PhysicsModuleIO
{
    // The per-frame potential-contact source the bridge/solve/contact-gen methods read the
    // hinged-body-part / detached-wheel contact queues from. AUTHORITATIVE home:
    // BrnPhysicsModuleIO_PotentialContactInterface.h. Referenced by pointer only here.
    // ⚠ CLASS-KEY FIXED 2026-08-06 (big-five #2 wave): this was `class ...;` -- a silent
    // mangling fork (MSVC encodes struct/class in ?AU/?AV) against the authoritative `struct`
    // that every OTHER forward declaration in the tree already uses. Same defect class as the
    // TrafficPhysics fork documented in BrnPhysicalTrafficManager.h.
    struct PotentialContactInterface;

    // ⭐ ADDED 2026-08-06 (big-five #2): the Bridge* pair's corrected arg-2 type. Class key
    // `class`, matching BrnPhysicsModuleIO.h:147.
    class InputBuffer;
}

namespace GameState { namespace GameStateModuleIO {
    // DWARF Update arg type BrnGameState::GameStateModuleIO::EGameModeType. It is a PLAIN
    // (unscoped, no fixed underlying type) enum in BrnGameStateSharedIO.h and is therefore NOT
    // forward-declarable, and that header is heavy -- so, exactly as BrnDeformableObject.h does
    // for the same arg, Update takes it as a plain s32 here (FLAG); the body TU casts to the
    // real enum. (This nested-namespace block is left intentionally empty -- documentation only.)
} }

namespace Deformation
{
    // ---- DWARF BrnDeformationManager.h namespace-scope constants (:57-58, :752) ----------------
    // KF_PART_DYNAMIC_FRICTION / KF_PART_STATIC_FRICTION and KVF_PROJECTSPHERE_RADIUS_PADDING are
    // rodata (the float / VecFloat VALUES are not in the DWARF); they are owned/defined by the
    // DeformationManager TU, not here. Declared extern so the body TU can define them once.
    extern const f32      KF_PART_DYNAMIC_FRICTION;          // DWARF :57
    extern const f32      KF_PART_STATIC_FRICTION;           // DWARF :58
    extern const VecFloat KVF_PROJECTSPHERE_RADIUS_PADDING;  // DWARF :752

    // Pool capacity -- up to 28 deformable models (the BitArray<28> width, maGlobalEntityIDs[28],
    // and the KU_MAX_DEFORMATION_MODELS the OutputData / sensor loops assert against). This is the
    // same constant DeformationState pins (BrnDeformationState.h KU_MAX_DEFORMATION_MODELS == 28).
    // Capacities of the index tables (8 race cars, 20 traffic, 601 global-traffic) are pinned by
    // the Construct memset loops (dossier Construct @0x82621510: 8 / 20 / 600+1 bytes).

    // Already homed elsewhere; referenced here by pointer/ref only:
    //   * DeformableObject -- the per-car deformable model (BrnDeformableObject.h)
    //   * PhysicalBodyPart / PhysicalWheel -- the part/wheel records (via the managers' includes)
    //   * DeformationInputInterface / DeformationOutputInterface(ForEntityModules) -- the deformation
    //     IO interfaces (SharedIO; declared by the managers' includes / forward-declared below)
    class DeformableObject;
    class DeformationInputInterface;            // ProcessEvents / PostSceneUpdate input interface (SharedIO)
    class DeformationOutputInterface;           // OutputData / OutputSensorState sink (SharedIO)
    class DeformationOutputInterfaceForEntityModules; // OutputData entity-module sink (SharedIO)

    // Contact-spy debug sink (UpdatePostPhysics forwards it). Owned by the contact-spy TU.
    // FLAG: forward-declared. (Also declared by BrnPhysicalBodyPartPool.h.)
    struct ContactSpyData;

    // The shared penetration solver the post-physics pass runs. Owned by BrnPenetrationSolver.h.
    // FLAG: forward-declared (touched by pointer only).
    struct PenetrationSolver;

    // ---- DWARF arg/record types touched by pointer only (homes not in-tree). FLAG: each is a
    //      forward-declared cross-TU type. -------------------------------------------------------
    // ⛔ FORK RETIRED 2026-08-06 (bridge de-facade wave). Three LOCAL forward declarations
    // stood here for the CreateDetached*ContactEvent arg types:
    //     struct PhysicalCarPartContact;  struct OutContactSpy;  struct PotentialContact;
    // Declared in namespace BrnPhysics::Deformation, they were DIFFERENT TYPES from the real
    // records the caller (PhysicsModule::StoreContact @0x825A5DB0) builds and passes --
    // BrnPhysics::ContactSpy::PhysicalCarPartContact, CgsPhysics::PhysicsSimulationIO::
    // OutContactSpy and CgsSceneManager::SceneManagerIO::PotentialContact -- so the two decls
    // below mangled to symbols no caller could ever reference (the silent-ODR-fork shape).
    // Worse, `PotentialContact` bound to BrnPhysicalBodyPartPool.h's REAL
    // Deformation::PotentialContact (the hinged-joint record, a deliberately distinct type,
    // included above). All three arg types are now homed in-tree, so the decls below spell
    // them fully qualified, like the Fixup* pair always did.
}
}
// Pointer-only forward declarations of the REAL records (class keys match their homes).
namespace CgsPhysics { namespace PhysicsSimulationIO { struct OutContactSpy; } }
namespace BrnPhysics { namespace ContactSpy { struct PhysicalCarPartContact; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; } }
namespace CgsSceneManager { namespace CgsCollision { struct PrimitivePairListBuilder; } }

namespace BrnPhysics
{
namespace Deformation
{
    // ⚠ DE-FORKED 2026-08-06 (big-five #2 wave): this was a bare LOCAL forward declaration
    // (`struct PrimitivePairListBuilder;` in BrnPhysics::Deformation) -- a namespace fork of the
    // real CgsSceneManager::CgsCollision::PrimitivePairListBuilder the PS3 DecFIGS mangles pin
    // for all three Add*Pair methods (@0x760100/@0x75B050/@0x760E6C:
    // ...PNS2_12CgsCollision24PrimitivePairListBuilderE). ALIASED to the real homed type, the
    // IOBufferStack precedent directly below.
    using PrimitivePairListBuilder = CgsSceneManager::CgsCollision::PrimitivePairListBuilder;

    // SolvePenetration / UpdatePostPhysics scratch stack. ALIASED to the real homed CgsModule type
    // (was a bare local forward-decl) so CreateIOBuffer<>/DestroyIOBuffer<> resolve. FLAG: additive.
    using IOBufferStack = CgsModule::IOBufferStack;
    // UpdateTriangleCache arg (DWARF InputBuffer_Update*). ALIASED to the real homed CgsSceneManager
    // type so GetSceneUpdateInterface() resolves. FLAG: additive.
    using InputBuffer_Update = CgsSceneManager::SceneManagerIO::InputBuffer_Update;
    struct VehicleInTriangleCacheInterface; // == VehicleInputInterface::InTriangleCacheInterface (world-contact-gen arg)

    // ========================================================================
    // BrnPhysics::Deformation::DeformationManager
    // ========================================================================
    struct DeformationManager
    {
    public:
        // ====================================================================
        // Public API (DWARF :80-383). DECLARE-ONLY -- bodies are owned by the
        // DeformationManager TU (BrnDeformationManager.cpp). Signatures from the DWARF;
        // X360 Hex-Rays drops unused args, so the DWARF prototype is authoritative.
        // ====================================================================

        // ----- lifecycle (DWARF :80-93) --------------------------------------------------

        // :80 (dossier Construct @0x82621510). Construct every part slot + the debug component,
        // seed mRandom, clear the model/index tables, and register the ~24 CPU perf monitors.
        void Construct();

        // :85 (dossier Prepare @ BrnDeformationManager.cpp:154). Per-level prepare; allocates the
        // deformable-model pool (mpaModels) through the resource allocator.
        bool Prepare(rw::IResourceAllocator* lpAllocator);

        // :89 (dossier Release @ BrnDeformationManager.cpp:199). Per-level release.
        bool Release();

        // :93 (dossier Destruct @0x82603F78). Tear the manager + its collision generator down.
        void Destruct();

        // ----- per-frame spine (DWARF :100-152) ------------------------------------------

        // :100. Pre-physics scene update: process the deformation event queues (add/remove/
        // deactivate/validate models) and push new models into the scene.
        void PostSceneUpdate(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                             DeformationInputInterface* lpInputInterface,
                             CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // :104. Roll every live model's sensor displacements forward by the time step.
        void UpdateSensorDisplacements(VecFloat lvfTimeStep);

        // :114. The main per-step deformation update: per-model Update (sensors, IK, detachment),
        // then the detached part/wheel managers. FLAG: the last arg's DWARF type is
        // BrnGameState::GameStateModuleIO::EGameModeType (a plain enum, not forward-declarable);
        // taken as s32 here, body casts.
        void Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
                    const CgsPhysics::PhysicsSimulationIO::InputBuffer* lpPrevSimInput,
                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpPrevSimOutput,
                    PhysicsModuleIO::PotentialContactInterface* lpContacts, VecFloat lvfTimeStep,
                    s32 leGameMode);

        // :120 (dossier ReadUpdatedBodies @ BrnDeformationManager.cpp:1116). Read the physics
        // output's updated-rigid-body queue back into the matching models / detached bodies.
        // (DWARF arg2 type: OutputBuffer::SceneInputInterface* == InSceneUpdateInterface*.)
        void ReadUpdatedBodies(const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpUpdatedBodies,
                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // :127 (dossier ReadPotentialContact @ BrnDeformationManager.cpp:1867). Route one
        // potential vehicle-vs-vehicle contact into the two cars' sensors (resets their cooldown).
        void ReadPotentialContact(const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
                                  BrnPhysics::ContactId lContactId,
                                  CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput);

        // :134. Route one potential vehicle-vs-WORLD contact into the owning car's sensor.
        void ReadPotentialVehicleWorldContact(const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
                                              BrnPhysics::ContactId lContactId,
                                              CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput);

        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). PhysicsModule::
        // BridgeContactsToSimulation @0x825A99E8 calls the embedded part pool's / detached-wheel
        // manager's OWN out-of-line IsPartIndexUsed/GetPart/IsSlotUsed/GetWheel with the embedded
        // objects' addresses as `this` (module+362384 == mDetachedPartManager(.mPartPool),
        // module+387200 == mDetachedWheelManager) -- an access private members cannot express
        // from PhysicsModule. Exposed as named accessors; FLAG: the accessor NAMES are inferred
        // (no DWARF accessor is dumped for either member) -- the console evidence is the direct
        // embedded-object call itself.
        DetachedPartManager&  GetDetachedPartManager()  { return mDetachedPartManager; }
        DetachedWheelManager& GetDetachedWheelManager() { return mDetachedWheelManager; }

        // :142. Post-physics pass: add contacts to the penetration solver, solve penetrations,
        // read transforms back, update models + the detached managers, process the joint spies.
        void UpdatePostPhysics(const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput,
                               CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpPrevSimOutput,
                               ContactSpyData* lpContactSpyData, IOBufferStack* lpIOBufferStack,
                               const PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :147 (dossier OutputData @0x826225D8). Output every live model's deformation/skinned/
        // locator state into the two output interfaces (asserts model index < 28).
        void OutputData(DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
                        DeformationOutputInterface* lpOutput);

        // :152. Reset every model's deformation back to undeformed.
        void ResetDeformation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // ----- ids / contact events (DWARF :156-171) -------------------------------------

        // :156. Set the world (static environment) rigid-body id used for vehicle-vs-world contacts.
        // ⚠️ QUALIFIED DELIBERATELY -- unqualified, this bound to the 4-byte BrnCommonTypes.h
        // stand-in and truncated the caller's handle. See the header note at the top.
        void SetWorldBodyId(CgsPhysics::RigidBodyId lWorldRigidBodyId);

        // :164 (X360 @0x825DD628). Build a detached-PART contact event from a spy + potential
        // contact. Arg types fully qualified 2026-08-06 -- see the fork-retirement note above.
        void CreateDetachedPartContactEvent(BrnPhysics::ContactSpy::PhysicalCarPartContact* lpOutPhysicalCarPartContact,
                                            const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
                                            const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact);

        // :171 (dossier CreateDetachedWheelContactEvent @0x825B95B0). Build a detached-WHEEL
        // contact event (asserts both ids' owners are a detached racecar/traffic wheel).
        void CreateDetachedWheelContactEvent(BrnPhysics::ContactSpy::PhysicalCarPartContact* lpOutPhysicalCarPartContact,
                                             const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
                                             const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact);

        // ----- per-car spatial queries (DWARF :176-195) ----------------------------------

        // :176. The continuous (swept) collision spheres for the car with entity id lEntityId.
        // Returns the count and writes the array pointer.
        s32 GetSweptSpheresForCar(EntityId lEntityId, const CgsGeometric::SweptSphere** lppSpheresOut);

        // :182. The discrete collision spheres for the car with entity id lEntityId.
        s32 GetSpheresForCar(EntityId lEntityId, const CgsGeometric::Sphere** lppSpheresOut);

        // :187. The deformed bounding box for the car with entity id lEntityId. const.
        void GetDeformedBBox(EntityId lEntityId, CgsGeometric::Box* lpBoxOut) const;

        // :191. Whether the car with entity id lEntityId is currently using swept-sphere tests.
        bool IsUsingSweptSpheres(EntityId lEntityId);

        // :195. Whether the car with entity id lEntityId is frozen. const.
        bool IsFrozen(EntityId lEntityId) const;

        // ----- world contact generation / pair building (DWARF :203-242) -----------------

        // :203. Generate the body-part-vs-world contacts for all live models. const. FLAG: arg1's
        // DWARF type is the nested VehicleInputInterface::InTriangleCacheInterface; modelled here
        // as the standalone forward-declared VehicleInTriangleCacheInterface, and CollisionGenerator
        // / LinearMalloc are the qualified CgsSceneManager::CgsCollision:: / CgsMemory:: homes.
        void DoBodyPartWorldContactGeneration(const VehicleInTriangleCacheInterface* lpTriCache,
                                              ContactGenList* lpGenList,
                                              CgsPhysics::CollisionGenerator* lpGen,
                                              CgsMemory::SimpleDataStreamProducer* lpProducer,
                                              CgsMemory::LinearMalloc* lpAlloc) const;

        // :209. Add the race-car body-part collision pair for (entity, volume-instance) to the builder.
        void AddRaceCarBodyPartPair(EntityId lEntityId, CgsSceneManager::VolumeInstanceId lVolumeInstanceId,
                                    PrimitivePairListBuilder* lpBuilder);

        // :215. Add the hinged-body-part pairs between two entities to the builder.
        void AddHingedBodyPartPairs(EntityId lEntityIdA, EntityId lEntityIdB,
                                    PrimitivePairListBuilder* lpBuilder);

        // :221. Add the race-car detached-wheel collision pair for (entity, volume-instance).
        void AddRaceCarWheelPair(EntityId lEntityId, CgsSceneManager::VolumeInstanceId lVolumeInstanceId,
                                 PrimitivePairListBuilder* lpBuilder);

        // :228. Bridge the body-part-vs-car potential contacts back into the simulation.
        // ⚠ ARG-2 TYPE CORRECTED 2026-08-06 (big-five #2 wave): the PS3 DecFIGS mangle
        // (@0x6F728C ...PKNS_15PhysicsModuleIO11InputBufferE...) pins arg 2 as the PHYSICS
        // MODULE IO input buffer (BrnPhysics::PhysicsModuleIO::InputBuffer), not a second sim
        // input buffer -- exactly what the caller BridgeContactsToSimulation passes.
        // ⚠ FLAG: body still a TRAP STUB (348 X360 asm lines -- named, not landed, this wave).
        void BridgeBodyPartCarContactsToSimulation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                   const BrnPhysics::PhysicsModuleIO::InputBuffer* lpInputBuffer,
                                                   PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :234. Bridge the detached-wheel-vs-car potential contacts back into the simulation.
        // Same arg-2 correction as :228 (PS3 mangle @0x741E28); same TRAP-STUB FLAG (392 asm).
        void BridgeDetachedWheelCarContactsToSimulation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                        const BrnPhysics::PhysicsModuleIO::InputBuffer* lpInputBuffer,
                                                        PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :242. Generate the detached-wheel-vs-world contacts. const. (Same FLAG as :203.)
        void DoDetachedWheelWorldContactGeneration(const VehicleInTriangleCacheInterface* lpTriCache,
                                                   ContactGenList* lpGenList,
                                                   CgsPhysics::CollisionGenerator* lpGen,
                                                   CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                   CgsMemory::LinearMalloc* lpAlloc) const;

        // ----- contact set-up / triangle cache (DWARF :247-258) --------------------------

        // :247. Set up a deformation part contact from a potential contact, filling the add-contact event.
        void SetupPartContact(const CgsSceneManager::SceneManagerIO::PotentialContact& lrPotentialContact,
                              CgsSceneManager::SceneManagerIO::InAddPotentialContact& lrAddContact);

        // :252 (dossier UpdateTriangleCache @ BrnDeformationManager.cpp:2075). Update the scene
        // triangle cache for every live model + detached part/wheel.
        void UpdateTriangleCache(InputBuffer_Update* lpSimInputUpdate);

        // :258. Add the body-part-vs-world collision results into the potential-contact queue.
        void AddBodyPartWorldResultsToContactQueue(const ContactGenList* lpGenList,
                                                   CgsPhysics::CollisionGenerator* lpGen,
                                                   PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // ----- reset events / contact fix-up (DWARF :264-305) ----------------------------

        // :264. Process a single per-car reset-deformation event.
        void ProcessResetDeformationModelEvent(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                               CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                               EntityId lEntityId);

        // :270. DEBUG: reset all deformation models.
        void ProcessDebugResetDeformationModels(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // :278/:285/:292. Fix up a vehicle-vs-vehicle contact (by interpolation / against boxes /
        // the dispatcher) so the contact point/normal respect the deformed geometry.
        void FixUpVehicleContactByInterpolation(CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
                                                CgsSceneManager::VolumeInstanceId lVolumeInstanceIdA,
                                                CgsSceneManager::VolumeInstanceId lVolumeInstanceIdB);
        void FixUpVehicleContactWithBoxes(CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
                                          CgsSceneManager::VolumeInstanceId lVolumeInstanceIdA,
                                          CgsSceneManager::VolumeInstanceId lVolumeInstanceIdB);
        void FixUpVehicleContact(CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
                                 CgsSceneManager::VolumeInstanceId lVolumeInstanceIdA,
                                 CgsSceneManager::VolumeInstanceId lVolumeInstanceIdB);

        // :297. Record which race-car entity is the player (drives player-priority handling).
        void SetPlayerRaceCarEntityId(EntityId lEntityId);

        // :301 / :305. Fix up a body-part / detached-wheel vehicle contact in place. Returns true
        // iff the contact was adjusted/kept.
        bool FixupBodyPartVehicleContact(CgsSceneManager::SceneManagerIO::PotentialContact* lpContact);
        bool FixupWheelVehicleContact(CgsSceneManager::SceneManagerIO::PotentialContact* lpContact);

        // ----- debug / part-pool accessors (DWARF :309-383) ------------------------------

        // :309. The (instance) debug component pointer.
        DeformationDebugComponent* GetDebugComponent();

        // :317. DEBUG: verify every live model's part indices are consistent.
        void VerifyPartIndices();

        // :323. Clear the stored contacts on the model owning entity id lEntityId.
        void ClearModelStoredContacts(EntityId lEntityId);

        // :327 / :332. Whether the detached-part / detached-wheel slot at liIndex is in use.
        bool IsPartUsed(s32 liIndex);
        bool IsWheelUsed(s32 liIndex);

        // :337 / :342. The detached body-part / wheel record at liIndex (const).
        const PhysicalBodyPart* GetPhysicalPart(s32 liIndex);
        const PhysicalWheel*    GetPhysicalWheel(s32 liIndex);

        // :347. The player car's deformable model (null if the player has none). DWARF spelling
        // truncates to "GetPlaye"; the full method is GetPlayerCarModel.
        DeformableObject* GetPlayerCarModel();

        // :350. Whether the player's deformation model is currently active.
        bool IsPlayerDeformationModelActive();

        // :383. The static debug component (the file-scope mDebugComponent below) by reference.
        static DeformationDebugComponent& GetDebugComponentStatic();

        // DEBUG: the deformation debug component's rig-picker reads the live-model bit array + model pool
        // to bind the selected rig (asm OnSelectedRigChange: mModelsAdded.IsBitSet(i) gate, then
        // &mpaModels[i]). Exposed by name so the (non-friend) component can resolve a slot.
        // GetDeformableObject is declared-only here (the DeformableObject layout is deliberately NOT
        // pulled into this header -- see the forward declaration above); its one-line body lives in the
        // manager TU where the full type is in scope.
        bool              IsDeformableObjectActive(s32 liIndex) const { return mModelsAdded.IsBitSet(static_cast<u32>(liIndex)); }
        DeformableObject* GetDeformableObject(s32 liIndex);

    private:
        // ⭐ NOT A GAME FUNCTION. Never called; bodied in the MOUNTED BrnDeformationManager.cpp
        // and nothing but static_asserts. Static so it can see the private tail below.
        // This class had NO layout gate of any kind until 2026-08-04 (task #141), which is how a
        // 4-byte mWorldRigidBodyId sat in an 8-byte seat unnoticed. ⚠️ It gates by ADJACENCY
        // (prev + sizeof(prev)) against the console offsets, NOT by a total sizeof -- a total
        // would be absorbed by padding on this pointer-bearing, over-aligned class.
        static void _AssertLayout();

        // ====================================================================
        // Private helpers (DWARF :413-497). DECLARE-ONLY -- bodies owned by the
        // DeformationManager TU.
        // ====================================================================

        // :413. Dispatch the deformation event queues for this scene update.
        void ProcessEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                           DeformationInputInterface* lpInputInterface,
                           CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);

        // :421 / :428 / :435 / :439. The per-event-type processors.
        void ProcessAddDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                              const DeformationInputInterface* lpInputInterface,
                                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);
        void ProcessRemoveDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                 const DeformationInputInterface* lpInputInterface,
                                                 CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);
        void ProcessDeactivateDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                     const DeformationInputInterface* lpInputInterface,
                                                     CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface);
        void ProcessValidateDeformationModelEvents(const DeformationInputInterface* lpInputInterface);

        // :443 / :447 / :451. Model-index lookups (by local entity id / global entity id / part body id).
        // ⭐ FindModelIndexByEntityID is DEFINED INLINE below the class as of 2026-08-06
        // (FixUpVehicleContacts wave): the X360 out-of-line copy @0x8259BBD0 bakes THIS HEADER's
        // path into its asserts (BrnDeformationManager.h:695/:701/:710), proving the definition
        // text lives here; the PS3 DecFIGS build both inlines it into the FixUp* family and keeps
        // an out-of-line copy @0x7AF16C with the same header-cited asserts.
        // ⭐ ACCESS WIDENED 2026-08-06 (big-five #2): FindModelIndexByEntityID moved to a public
        // stanza -- VehicleManager::StartVehicleContactGeneration @0x8262B0xx calls the
        // out-of-line copy @0x8259BBD0 from OUTSIDE this class (register-truth bl), which a
        // private member cannot express.
    public:
        s32 FindModelIndexByEntityID(EntityId lEntityId) const;
    private:
        s32 FindModelIndexByGlobalEntityID(EntityId lGlobalEntityId);
        s32 FindModelIndexByPartID(CgsPhysics::RigidBodyId lPartBodyId);   // ⚠️ qualified -- see header note

        // :454. Output every live model's sensor state into the output interface.
        void OutputSensorState(DeformationOutputInterface* lpOutput);

        // :462. Project a sphere contact point onto a box (deformation contact fix-up helper). const.
        // ⭐ SIGNATURE CORRECTED 2026-08-06 (FixUpVehicleContacts wave): the PS3 DecFIGS mangled
        // name (_ZNK...27ProjectSphereContactOntoBoxERKN12CgsGeometric6SphereEN2rw4math3vpu7Vector3ES9_
        // NS8_14Matrix44AffineEPS9_, out-of-line @0x7A5118) pins the argument list as
        // (const CgsGeometric::Sphere&, Vector3, Vector3, Matrix44Affine, Vector3*) const -- the
        // old "SphereArg taken as Vector3" FLAG guess is RETIRED. Parameter names per the body's
        // own baked asserts ("lWorldSphere" / "lNormal" / "lContactPoint",
        // BrnDeformationManager.h:771/:810 -- i.e. the definition text lives in this header on
        // console; here it is bodied in the FixUp slice TU, an inlining-neutral placement).
        // Returns the projected contact point; *lpNormalOut receives the box-face normal.
        Vector3 ProjectSphereContactOntoBox(const CgsGeometric::Sphere& lWorldSphere, Vector3 lNormal,
                                            Vector3 lContactPoint, Matrix44Affine lBoxTransform,
                                            Vector3* lpNormalOut) const;

        // :467 (dossier SolvePenetration). Run the penetration solver over the accumulated contacts.
        void SolvePenetration(IOBufferStack* lpIOBufferStack,
                              const PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :472. Add the articulated-joint contacts into the penetration solver.
        void AddArticulatedJointContacts(PenetrationSolver* lpSolver,
                                         const PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :481. Interpolate a contact point + normal across a deformable model's sensor-sphere
        // chain. Parameter names per the PS3 DecFIGS debug info (@0x73C1F4); *lpInOutNormal is
        // seeded by the caller and rewritten (world space) whether or not the interpolation
        // succeeds. Returns true iff a neighbour tangent interpolation was performed.
        bool GetInterpolatedContactPointAndNormal(DeformableObject* lpModel, s32 liSensorIndex,
                                                  Vector3 lWorldPosIn, Vector3 lOtherSensorPos,
                                                  Vector3* lpOutWorldPos, Vector3* lpInOutNormal);

        // :490. The external tangent line between two (height-flattened) spheres through a side
        // point: *lpResultA / *lpResultB get the tangent points on lSphereAIn / lSphereBIn,
        // *lpOutNormal the tangent direction. Names per the PS3 DecFIGS debug info (@0x73B150).
        void CalculateTangentPoints(CgsGeometric::Sphere lSphereAIn, CgsGeometric::Sphere lSphereBIn,
                                    Vector3 lPointOnSide, Vector3* lpResultA, Vector3* lpResultB,
                                    Vector3* lpOutNormal);

        // :497 (dossier ProjectLineOntoPlane @ BrnDeformationManager.cpp:1430). Project a line onto
        // a plane (returns the intersection point).
        Vector3 ProjectLineOntoPlane(Vector3 lPointOnLine, Vector3 lLineDir,
                                     Vector3 lPointOnPlane, Vector3 lPlaneNormal);

        // ====================================================================
        // FULL DWARF MEMBER SEQUENCE (BrnDeformationManager.h:358-405). Members BY NAME +
        // SEQUENCE. Construct (dossier @0x82621510) corroborates the leading members:
        //   mRandom seeded at this+32/+40 (0x3F8000001AD0891B = the default seed); the part pool
        //   constructed via a 50-iter loop at this+48112; the index tables memset at this+76044
        //   (8), +76052 (20), +76072 (600); the ~24 perf monitors registered at this+76676..+76736.
        // ====================================================================

        CgsNumeric::Random   mRandom;                 // :358 private RNG (seeded in Construct)
        DeformationState     mStateOutput;            // :359 the shared deformation-state output (BY VALUE)
        DetachedPartManager  mDetachedPartManager;    // :360 the detached-PART manager (BY VALUE)
        DetachedWheelManager mDetachedWheelManager;   // :361 the detached-WHEEL manager (BY VALUE)
        CgsContainers::BitArray<28u> mModelsAdded;    // :362 which model slots are live
        EntityId             maGlobalEntityIDs[28];   // :363 global scene-entity id per model slot
        CgsPhysics::RigidBodyId mWorldRigidBodyId;    // :364 the static-world rigid-body id (SetWorldBodyId)
                                                      //      X360 +76024, EIGHT bytes -- see the header note
        DeformableObject*    mpaModels;               // :365 the allocated pool of deformable models (Prepare)
        s32                  miNumUsedModels;         // :366 live model count
        s32                  miPlayerModelIndex;      // :367 player car's model slot (-1 if none)
        s8                   ma8RaceCarToModelIndex[8];          // :370 race-car index  -> model slot
        s8                   ma8TrafficToModelIndex[20];         // :371 traffic index   -> model slot
        s8                   ma8GlobalTrafficToModelIndex[601];  // :374 global-traffic index -> model slot
        s32                  miLastBodyToHaveIKUpdate;           // :376 round-robin IK-update cursor

        // :385. FLAG: the DWARF lists mDebugComponent here as an `extern ...
        // DeformationDebugComponent` -- i.e. it is a FILE-SCOPE STATIC, not a per-instance member
        // (Construct's asm constructs it at the fixed address off_82F2A440, NOT at a `this`-relative
        // offset). It is therefore modelled as a STATIC data member so it keeps its DWARF position
        // in the sequence WITHOUT perturbing the instance layout (the perf-mon ints below follow at
        // their console this-relative offsets +76676.. exactly). Defined once by the manager TU.
        static DeformationDebugComponent mDebugComponent;       // :385 (file-scope static)

        // The ~24 CPU performance-monitor handles (perf-mon ids returned by PerfMonCpu::AddMonitor;
        // Construct @0x82621510 registers them in this exact order at this+76676..+76736).
        s32 miTotalDeformationPerfMon;                                   // :389
        s32 miPostSceneUpdatePerfMon;                                    // :390
        s32 miUpdateSensorDisplPerfMon;                                  // :391
        s32 miUpdatePerfMon;                                             // :392
        s32 miUpdatePostPhysicsPerfMon;                                  // :393
        s32 miPostPhysicsUpdateAddContactsToPenSolverPerfMon;            // :394
        s32 miPostPhysicsUpdateSolvePenetrationPerfMon;                  // :395
        s32 miPostPhysicsUpdateReadTransformsFromPenSolverPerfMon;       // :396
        s32 miPostPhysicsUpdateModelsPerfMon;                            // :397
        s32 miPostPhysicsUpdateDetachedPartsManPerfMon;                  // :398
        s32 miPostPhysicsProcessJointSpiesPerfMon;                       // :399
        s32 miUpdateIkAndDetachingPerfMon;                               // :400
        s32 miFixUpRaceCarTrafficContact;                                // :401
        s32 miUpdateModelsPerfMon;                                       // :403
        s32 miUpdateDetachedPartsPerfMon;                                // :404
        s32 miUpdateSkinnedJointsPerfMon;                                // :405
    };

    // ==========================================================================================
    // FindModelIndexByEntityID -- header-inline definition (see the in-class banner). Byte truth:
    // the X360 out-of-line copy @0x8259BBD0, whose asserts cite THIS header (:695/:701/:710).
    // Dispatch on the LOCAL entity id's owner byte: RACECAR -> ma8RaceCarToModelIndex, TRAFFIC ->
    // ma8TrafficToModelIndex; any other owner streams "Bad entityID finding car: 0x%X" (lowered
    // to CGS_ASSERT with the static prefix per the standing rule) and falls through to the
    // traffic table, exactly as the console control flow does.
    // ==========================================================================================
    inline s32 DeformationManager::FindModelIndexByEntityID(EntityId lEntityId) const
    {
        const u32 luOwner = (lEntityId.muValue >> 24) & 0xFFu;
        if (luOwner == 1u)   // BrnWorld::E_ENTITYTYPE_RACECAR
        {
            const u32 lu16RaceCarIndex = (lEntityId.muValue >> 10) & 0x3FFFu;
            CGS_ASSERT(lu16RaceCarIndex < 8u, "lu16RaceCarIndex < Vehicle::ku8MaxNumRaceCars");   // :701
            return ma8RaceCarToModelIndex[lu16RaceCarIndex];
        }

        CGS_ASSERT(luOwner == 2u, "Bad entityID finding car: ");   // :695 (streamed on console)
        const u32 lu16TrafficIndex = (lEntityId.muValue >> 10) & 0x3FFFu;
        CGS_ASSERT(lu16TrafficIndex < 0x14u, "lu16TrafficIndex < Vehicle::ku8TotalMaxNumPhysicalTraffic");   // :710
        return ma8TrafficToModelIndex[lu16TrafficIndex];
    }
}
}
