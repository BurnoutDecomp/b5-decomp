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
#include "BrnCommonTypes.h"    // Vector3, Matrix44Affine, VecFloat, EntityId, RigidBodyId

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
    namespace PhysicsSimulationIO { class InputBuffer; class OutputBuffer; }   // sim IO buffers
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
    // hinged-body-part / detached-wheel contact queues from. Homed (minimal) by
    // BrnPhysicalBodyPartPool.h, included transitively via the managers above. Referenced by
    // pointer only here. FLAG: forward-declared (full layout not required for declare-only).
    class PotentialContactInterface;
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
    struct PhysicalCarPartContact;   // CreateDetached{Part,Wheel}ContactEvent out-record (DWARF PhysicalCarPartContact*)
    struct OutContactSpy;            // CreateDetached{Part,Wheel}ContactEvent in-spy (DWARF OutContactSpy*)
    struct PotentialContact;         // BrnPhysics-side potential contact (CreateDetached*ContactEvent in-record)
    struct PrimitivePairListBuilder; // AddRaceCar*Pair / AddHingedBodyPartPairs out-builder

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
        void SetWorldBodyId(RigidBodyId lWorldRigidBodyId);

        // :164. Build a detached-PART contact event from a spy + potential contact.
        void CreateDetachedPartContactEvent(PhysicalCarPartContact* lpOutPhysicalCarPartContact,
                                            const OutContactSpy* lpInContact,
                                            const PotentialContact* lpInPotentialContact);

        // :171 (dossier CreateDetachedWheelContactEvent @0x825B95B0). Build a detached-WHEEL
        // contact event (asserts both ids' owners are a detached racecar/traffic wheel).
        void CreateDetachedWheelContactEvent(PhysicalCarPartContact* lpOutPhysicalCarPartContact,
                                             const OutContactSpy* lpInContact,
                                             const PotentialContact* lpInPotentialContact);

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
        void BridgeBodyPartCarContactsToSimulation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                   const CgsPhysics::PhysicsSimulationIO::InputBuffer* lpPrevSimInput,
                                                   PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :234. Bridge the detached-wheel-vs-car potential contacts back into the simulation.
        void BridgeDetachedWheelCarContactsToSimulation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                        const CgsPhysics::PhysicsSimulationIO::InputBuffer* lpPrevSimInput,
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

    private:
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
        s32 FindModelIndexByEntityID(EntityId lEntityId) const;
        s32 FindModelIndexByGlobalEntityID(EntityId lGlobalEntityId);
        s32 FindModelIndexByPartID(RigidBodyId lPartBodyId);

        // :454. Output every live model's sensor state into the output interface.
        void OutputSensorState(DeformationOutputInterface* lpOutput);

        // :462. Project a sphere contact point onto a box (deformation contact fix-up helper). const.
        // FLAG: arg1's DWARF type is the nested InEventTriangleCollisionSphereTest::SphereArg; taken
        // as Vector3 here (the sphere's centre lane the projection uses).
        Vector3 ProjectSphereContactOntoBox(Vector3 lSphereCentre, Vector3 lPointA, Vector3 lPointB,
                                            Matrix44Affine lBoxTransform, Vector3* lpOut) const;

        // :467 (dossier SolvePenetration). Run the penetration solver over the accumulated contacts.
        void SolvePenetration(IOBufferStack* lpIOBufferStack,
                              const PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :472. Add the articulated-joint contacts into the penetration solver.
        void AddArticulatedJointContacts(PenetrationSolver* lpSolver,
                                         const PhysicsModuleIO::PotentialContactInterface* lpContacts);

        // :481. Interpolate a contact point + normal across a deformable model's sphere chain.
        bool GetInterpolatedContactPointAndNormal(DeformableObject* lpModel, s32 liSensorIndex,
                                                  Vector3 lPointA, Vector3 lPointB,
                                                  Vector3* lpPointOut, Vector3* lpNormalOut);

        // :490. The two tangent points between two spheres (contact fix-up geometry helper).
        void CalculateTangentPoints(CgsGeometric::Sphere lSphereA, CgsGeometric::Sphere lSphereB,
                                    Vector3 lAxis, Vector3* lpTangentA, Vector3* lpTangentB,
                                    Vector3* lpTangentC);

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
        RigidBodyId          mWorldRigidBodyId;       // :364 the static-world rigid-body id (SetWorldBodyId)
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
}
}
