#pragma once

// BrnPhysics::Deformation::DeformableObject -- FULL FROZEN LAYOUT HEADER.
//
// One deformable vehicle body: the per-car owner of the whole deformation simulation. It holds
// the collision/sensor spheres, the IK skeleton (tag points, driven points, IK body parts), the
// glass panes, the embedded VehicleRigidBody that wraps the car's VehiclePhysics, the deformation
// sensors that receive impacts, and all the per-frame deformation/detachment/glass/locator state.
// DeformableObject::ApplyCarCarImpulse (this TU) is the car-on-car shunt impulse orchestrator.
//
// LAYOUT (project rule -- members BY NAME + SEQUENCE, no raw-offset padding): the full member
// sequence below is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnDeformableObject.h:600-692). Each member
// carries its DWARF source-line provenance comment. The console build is the 32-bit X360 ARTIST.XEX;
// this PC build is x64, so embedded pointers / pointer-bearing sub-objects widen and the absolute
// byte offsets do NOT all reproduce on the host -- every access is BY MEMBER NAME (sensor/part
// arrays index by sizeof stride), so the absolute host size is not load-bearing for the per-TU gate.
//
// COMMITTED-SLICE PRESERVATION (BrnDeformableObject.cpp bodies ApplyCarCarImpulse and must keep
// compiling): the prior minimal slice exposed a handful of placeholder members + accessors. They are
// now mapped onto the REAL DWARF members:
//   * GetVehicleBody()      -> the embedded mVehicleBody (VehicleRigidBody)'s attached physics body.
//   * AsRaceCarPhysics()    -> mVehicleBody's attached vehicle, downcast to RaceCarPhysics.
//   * GetGlobalEntityId()   -> the real mGlobalEntityId member (DWARF :645).
//   * GetDeformationSensor()-> indexes the real maDeformationSensors[20] (DWARF :618).
//   * GetGameModeByte()     -> FLAG: there is NO game-mode word member in the DeformableObject DWARF;
//       the cross-car bounce gate's `*(...+26392)` reads it off the OTHER car. It is retained as a
//       reconstructed member mu32GameModeState (NOT a DWARF member) so the committed bounce body
//       keeps compiling -- see the FLAG at the member.
//   * SetHasBouncedThisFrame/HasBouncedThisFrame -> backed by a real DWARF bool member
//       (mbResetDeformationNextUpdate region is unrelated; the bounce-this-frame latch the asm pokes
//       at +26414 has no separately-named DWARF member, so it is retained as the reconstructed
//       mbHasBouncedThisFrame -- FLAG).
//   * SetBounceRandomParity -> reconstructed mbBounceRandomParity (asm +26413; no DWARF name) -- FLAG.
// The reconstructed bounce members are appended AFTER the DWARF sequence (clearly flagged) so they do
// not perturb the recovered member order.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector4, Vector3Plus, Matrix44Affine, VecFloat, EntityId, RigidBodyId

// ---- homed embedded-type headers (types embedded BY VALUE / arrays of value) -------------------
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"        // CgsGeometric::Sphere
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"   // CgsGeometric::SweptSphere
#include "GameSource/Physics/DeformationManager/SharedIO/BrnVehicleLocatorData.h"                  // VehicleLocatorData
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"            // ImpulseParams (+ CollidableBody)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"             // ImpulsePasser
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h"          // VehicleRigidBody
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"         // DeformationSensor, StoredImpulseContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"                  // TagPoint
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"             // IKDrivenPoint
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"                // IKBodyPart
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"   // StreamedDeformationSpec, ETagPointType
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"           // EAbsorptionSets
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"          // EGlassState, DeformationOutputInterface(ForEntityModules)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"                   // EBodyParts, DeformationResetType, AddDeformationModelEvent
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                               // CgsSceneManager::VolumeInstanceId (GetHandlingBodyVolumeInstanceId packs one)
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"                               // ExternalPhysicsBody
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"                       // BrnPhysics::Vehicle::RaceCarPhysics

namespace CgsNumeric { class Random; }
// ---- Forward declarations for the cross-subsystem types DeformableObject only touches by
//      pointer/ref in its DECLARE-ONLY method signatures. Each is spelled with the qualified
//      namespace its real home uses (proven by the sibling BrnPhysicalBodyPart.h); pulling the
//      real headers here is unnecessary for a frozen-layout header and would risk include cycles.
//      Where the DWARF spells a NESTED type (e.g. OutputBuffer::SceneInputInterface), the comment
//      records the DWARF spelling and the signature uses the underlying forward-declared type. ----
namespace CgsGeometric { struct Box; struct AxisAlignedBox; }
namespace CgsDev { class Debug3DImmediateRender; }
// ⚠⚠ CLASS-KEY FIXED 2026-08-27 (detach-3 wave): SimpleDataStreamProducer is a `struct` in its
// real home (CgsSimpleDataStreamProducer.h:39). `class` here mangled every signature carrying it
// as ?AV instead of ?AU -- the same ODR fork the 2026-08-06 note below records for the sim buffers.
namespace CgsMemory { struct SimpleDataStreamProducer; class LinearMalloc; }
// (ResourceHandle is the complete BrnPhysics::Deformation::ResourceHandle from BrnDeformationEvents.h,
//  included above -- ValidateDeformationModel takes it unqualified, same namespace.)
namespace CgsSceneManager
{
    struct VolumeInstanceId;                                    // GetDeformationSphereFromVolumeInstance arg
    namespace SceneManagerIO { struct InSceneUpdateInterface; } // == OutputBuffer::SceneInputInterface (DWARF)
    // ⭐⭐ 2026-08-27 (detach-3 wave). The two world-contact-generation methods below used to take a
    // pointer to a phantom "CgsPhysics::CollisionGenerator" and to a locally invented
    // "InTriangleCacheInterface". NEITHER TYPE EXISTS ANYWHERE IN THIS TREE -- they were
    // declare-only forks with no definition and no possible definition, so every call site mangled
    // to a symbol no TU could ever define ([[shadowing-redeclarations]], the exact fork shape this
    // header warns about 60 lines down). That, not the "opaque callee" story the .cpp banner told,
    // is why the whole detached-part contact chain stayed a gate.
    // The REAL types are these, proven from the X360 asm rather than assumed:
    //   * the tri-cache pointer is handed straight to TriangleCacheInterface::GetNumCachedTriangle-
    //     Batches @0x82277880 and ::GetCache @0x82277810 (bl targets at 0x8260957C / 0x82609594),
    //     so it IS a CgsSceneManager::SceneManagerIO::TriangleCacheInterface. The DWARF's nested
    //     spelling VehicleInputInterface::InTriangleCacheInterface is the same object seen through
    //     the vehicle input buffer -- VehicleManager::StartPartContactGeneration, the only caller,
    //     already carries it as this type and passes it through unchanged.
    //   * the generator pointer is handed to BaseCollisionGenerator::CollidePrimitiveListAgainst-
    //     TriangleList / ::AddPrimitiveListWithTriangleListToStream, i.e. the real
    //     CgsSceneManager::CgsCollision::CollisionGenerator (a struct, CgsCollisionGenerator.h:516).
    namespace SceneManagerIO { struct TriangleCacheInterface; }
    namespace CgsCollision   { struct CollisionGenerator; }
}
namespace CgsPhysics
{
    // ⚠ CLASS-KEYS FIXED 2026-08-06 (big-five #2): `struct` per CgsPhysicsSimulationModuleIO.h.
    namespace PhysicsSimulationIO { struct InputBuffer; struct OutputBuffer; }  // sim IO buffers
}
// NOTE: the DWARF game-mode arg type BrnGameState::GameStateModuleIO::EGameModeType is a PLAIN
// (unscoped, no fixed underlying type) enum in BrnGameStateSharedIO.h -- it is therefore NOT
// forward-declarable, and that header is heavy (Network/BitArray/Scoring) so it must not be pulled
// into this frozen layout header. Update / UpdateAbsorptionSet take it as a plain s32 here (FLAG),
// exactly as BrnDirectorGameState.h models meEventType; the body TU casts to the real enum.

namespace BrnPhysics
{
namespace Vehicle { class VehiclePhysics; }

// The triangle-cache / contact-gen list the world-contact-generation methods feed. Owned by the
// physics contact-gen TU; referenced by pointer only. FLAG: forward-declared (home not in-tree).
struct ContactGenList;

// ⭐ 2026-08-14 (walls leg 4): the REAL physics-module IO types several method signatures take per
// the PS3 mangles (Update params 3/4, UpdateIKAndLocators/UpdateSpinningDetachment/DetachPart/
// CheckFor*Detachment param 2, UpdateOutputContactSpies' contact interface). Forward-declared here;
// homes are BrnPhysicsModuleIO*.h. (The old `struct PotentialContactInterface;` forward-decl INSIDE
// namespace Deformation was a silent fork of PhysicsModuleIO::PotentialContactInterface -- retired.)
namespace PhysicsModuleIO
{
    class  InputBuffer;
    class  OutputBuffer;
    struct PotentialContactInterface;
}

namespace Deformation
{
    // ---- DWARF BrnDeformableObject.h namespace-scope constants (:67-71, :998) -------------------
    // KU_VOLUME_BUFFER_SCRATCH_PAD_SIZE / KU_DETACHED_PART_SCENEMANAGER_FLAGS are compile-time
    // literals from the DWARF; KF_MIN_SWEPT_SPHERE_SPEED / KVF_CAR_BBOX_SHRINK are rodata (values
    // not in the DWARF) -- those rodata are owned/defined by the DeformableObject TU, not here.
    const u32 KU_VOLUME_BUFFER_SCRATCH_PAD_SIZE  = 150;    // DWARF :67
    const u32 KU_DETACHED_PART_SCENEMANAGER_FLAGS = 1156;  // DWARF :68
    const u8  KU_INVALID_WHEEL_TAG_POINT_INDEX    = 255;   // DWARF :610

    // Deformation-namespace siblings DeformableObject touches only by pointer/ref in its
    // declare-only signatures. CarState is a DWARF forward-declared sibling (BrnDeformationResetType.h);
    // the managers / contact interface / penetration solver have their own homes. DeformationResetType,
    // ETagPointType, DeformationOutputInterface(ForEntityModules), StreamedDeformationSpec, IKBodyPart,
    // TagPoint, VehicleLocatorData, DeformationSensor, StoredImpulseContact, ImpulseParams, EAbsorptionSets,
    // EGlassState and the LocatorPointSpec are all already COMPLETE via the includes above.
    // ⚠️ KEYWORDS CORRECTED 2026-08-14 (walls wave, found by the LINK): the managers are `class`
    // in their real homes (BrnDetachedPartManager.h:88 / BrnDetachedWheelManager.h:118), and
    // PhysicalBodyPart is `class` (BrnPhysicalBodyPart.h:131). The old `struct` forward-decls
    // made every TU that saw ONLY this header mangle PEAU while TUs with the real headers mangled
    // PEAV -- Prepare/Release/ResetDeformation/OutputWheelData were defined-but-unmatchable, the
    // exact [[shadowing-redeclarations]] fork shape (only a LINK finds it; this one did).
    class  DetachedPartManager;
    class  DetachedWheelManager;
    class  PhysicalBodyPart;
    struct PenetrationSolver;
    struct CarState;
    struct OutUpdateRigidBody;       // per-frame rigid-body update event (UpdateHandlingBody). FLAG: forward-declared.
    // ⭐⭐ 2026-08-27 (detach-3 wave): was `struct InTriangleCacheInterface;` -- a declare-only fork
    // with no definition anywhere in the tree. ALIASED to the real homed type, which is the SAME
    // spelling BrnVehicleInputInterface.h:33 already uses for the DWARF's nested name:
    //     typedef CgsSceneManager::SceneManagerIO::TriangleCacheInterface InTriangleCacheInterface;
    // so this is not a choice, it is the tree's own existing answer. See the evidence block beside
    // the CgsSceneManager forward declarations at the top of this header.
    using InTriangleCacheInterface = CgsSceneManager::SceneManagerIO::TriangleCacheInterface;

    class DeformableObject
    {
    public:
        // DWARF BrnDeformableObject.h:160. The lifecycle state of one IK body part slot.
        enum EPartState
        {
            E_PART_STATE_UNUSED         = 0,
            E_PART_STATE_NON_DETACHABLE = 1,
            E_PART_STATE_ATTACHED_IK    = 2,
            E_PART_STATE_HINGED         = 3,
            E_PART_STATE_DETATCHED      = 4,   // (DWARF spelling: DETATCHED)
            NUM_PART_STATES             = 5
        };

        // =========================================================================================
        // The car-car-impulse ledger func owned by THIS TU (bodied in BrnDeformableObject.cpp).
        // =========================================================================================

        // @0x82624C08 (DWARF :259). The two-body car-on-car shunt impulse orchestrator. Computes the
        // closing velocity at the contact, early-outs if separating, solves the two-body collision
        // impulse (ExternalPhysicsBody::CalculateCollisionImpulseWithBody), applies the showtime/
        // bounce-boost shaping, then applies the equal-and-opposite impulse to both cars via
        // ApplySensorImpulse. Returns true iff an impulse was applied.
        bool ApplyCarCarImpulse(const StoredImpulseContact& lContact, VecFloat lvfTimeStep,
                                VecFloat lvfIteration, s32 liSensorIndex, const CgsNumeric::Random& lRandom);

        // =========================================================================================
        // Public API (DWARF :172-589). DECLARE-ONLY -- bodies are owned by the DeformableObject TUs;
        // the later waves will fill them in. Signatures from the DWARF (X360 Hex-Rays drops args).
        // =========================================================================================

        // DWARF :172-251. The lifecycle / per-frame spine. FLAG: the DWARF arg types InputBuffer /
        // OutputBuffer / OutputBuffer::SceneInputInterface are the sim-IO buffers + the scene-update
        // interface; here they are the qualified, forward-declared CgsPhysics::PhysicsSimulationIO::
        // InputBuffer / OutputBuffer and CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (the
        // type OutputBuffer::SceneInputInterface is a typedef of). Referenced by pointer only.
        DeformableObject();   // @0x82603EF0 -- vtable wiring + per-sensor construct (bodied in _Lifecycle.cpp)
        void Construct();                                                                       // :172
        void Destruct();                                                                        // :175
        bool Prepare(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput, u16 lu16Index,
                     const AddDeformationModelEvent& lrEvent,
                     CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                     DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr,
                     CgsNumeric::Random& lrRandom);                                              // :185
        void Release(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                     CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                     DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr);          // :193
        void ResetDeformation(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                              DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr,
                              VecFloat lvfTime, DeformationResetType leResetType, bool lbFlag,
                              CgsNumeric::Random& lrRandom);                                     // :204
        void ResetScratching();                                                                 // :207
        void ResetDeformationNextUpdate(bool lbReset);                                          // :212
        bool ShouldResetDeformationNextUpdate() const;                                          // :216
        void UpdateSensorDisplacements(VecFloat lvfTimeStep);                                   // :222
        // ⭐ PARAMS 3/4 + lpContacts CORRECTED 2026-08-14 (walls leg 4): the PS3 DecFIGS mangle
        // (0x7585D8) types params 3/4 as the PHYSICS-MODULE IO buffers (PKNS_15PhysicsModuleIO
        // 11InputBufferE / PNS8_12OutputBufferE) and lpContacts as PhysicsModuleIO::
        // PotentialContactInterface -- the old sim-buffer/local-fork spellings were
        // declared-only (no body existed to migrate). Manager Update passes exactly these
        // (its own corrected params 3/4) straight through.
        bool Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                    const BrnPhysics::PhysicsModuleIO::InputBuffer* lpModuleInput,
                    BrnPhysics::PhysicsModuleIO::OutputBuffer* lpModuleOutput, VecFloat lvfTimeStep,
                    DetachedPartManager* lpPartMgr, DetachedWheelManager* lpWheelMgr,
                    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContacts,
                    CgsNumeric::Random& lrRandom,
                    s32 liGameMode);  // :235 (FLAG: DWARF arg = BrnGameState::GameStateModuleIO::EGameModeType)
        // ⭐ PARAM 2 CORRECTED 2026-08-14 (walls leg 4): PhysicsModuleIO::OutputBuffer per the
        // PS3 mangle (0x765220); declared-only before this wave.
        void UpdateIKAndLocators(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                 BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                 VecFloat lvfTimeStep, DetachedPartManager* lpPartMgr,
                                 DetachedWheelManager* lpWheelMgr, CgsNumeric::Random* lpRandom); // :244
        bool ApplyCarWorldImpulse(const StoredImpulseContact& lContact, VecFloat lvfTimeStep,
                                  VecFloat lvfIteration, s32 liSensorIndex);                     // :251

        // @0x826078B0 (DWARF :271). Apply one impulse to a sensor of THIS car (the heavy per-sensor
        // apply that forwards the built ImpulseParams to the body's ApplyLocalImpulse and updates the
        // spy). DECLARE-ONLY (its body is a separate TU; ApplyCarCarImpulse calls it BY NAME).
        void ApplySensorImpulse(VecFloat lvfTimeStep, const StoredImpulseContact& lContact,
                                const ImpulseParams& lImpulseParams, Vector3 lRelativeMotion,
                                Vector3 lImpulseDir, VecFloat lvfImpulseMagnitude,
                                DeformationSensor* lpSensor, bool lbAddToSpy,
                                bool lbUseNormalScaledFriction);                                 // :271

        void UpdateIK(VecFloat lvfTimeStep);                                                    // :276
        void CheckForDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                DetachedPartManager* lpPartMgr, f32 lfTimeStep);                 // :283 (param 2: PS3 mangle 0x762570)
        void CheckForForcedDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                      BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                      DetachedPartManager* lpPartMgr, CgsNumeric::Random* lpRandom,
                                      f32 lfTimeStep);                                           // :291 (param 2: PS3 mangle 0x757C08)
        void RemovePhysicalPartsAndJoints(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                          CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                                          DetachedPartManager* lpPartMgr);                       // :310
        void RemovePhysicalWheels(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                  CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene,
                                  DetachedWheelManager* lpWheelMgr);                             // :316
        void UpdateSkinningOffsets();                                                           // :319
        void UpdateWheels(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                          DetachedWheelManager* lpWheelMgr, f32 lfTimeStep,
                          CgsNumeric::Random* lpRandom);                                         // :326
        void UpdateGlass(f32 lfTimeStep, DeformationOutputInterface* lpOut,
                         DeformationOutputInterfaceForEntityModules* lpOutEM);                   // :332
        void UpdatePostPhysics(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene); // :336

        // :341. FLAG: DWARF arg type is OutUpdateRigidBody (a per-frame rigid-body update event,
        // passed BY VALUE in the DWARF). Forward-declared in this namespace below; passed by const
        // ref here to avoid requiring its (not-yet-homed) full layout for a declare-only method.
        void UpdateHandlingBody(const OutUpdateRigidBody& lrUpdate);                            // :341
        RigidBodyId GetHandlingBodyID();                                                        // :344

        // The OWNER byte of the entity word packed in the handling-body id (console +26384). The
        // X360 reads it as the player/game-mode SELECTOR in UpdateAbsorptionSet /
        // ApplySensorImpulse / the crash gates.
        // ⭐ RE-SPELLED 2026-08-11 (handle-widening wave), same bits, now honestly derived. It used
        // to be `mHandlingBodyID.muValue >> 24` on a 4-byte stand-in; the member is really the
        // 8-byte CgsPhysics::RigidBodyId, whose entity word is the HIGH dword, so the console's
        // HIBYTE(*(this+26384)) is the byte at bits 56..63 == GetEntityId().GetOwner() ==
        // GetEntityIDOwner(). That accessor already exists and is itself asm-witnessed
        // (PhysicsSimulationModule::ProcessRemoveAllRigidBodiesQueue @0x8289F274 inlines it as
        // `srdi r10,r10,32` + `srwi r10,r10,24`) -- the same two shifts, in one place.
        u8 GetHandlingBodyIdHighByte() const { return mHandlingBodyID.GetEntityIDOwner(); }

        // ⭐ ADDED 2026-08-06 (FixUpVehicleContacts wave): the 8-byte handling-body volume-instance
        // word both Fixup*VehicleContact bodies re-key a contact's B id from -- `ld 0x6710(model)`
        // @0x825A0D78 / @0x825A0F94, stored WHOLE into muVolumeInstanceIdB.
        // ⭐⭐ CORRECTED 2026-08-11 (handle-widening wave). This used to read the 8 bytes at +26384
        // as the PAIR {mHandlingBodyID(4), mGlobalEntityId(4)} and re-pack them. It is not a pair:
        // DeformableObject::Prepare @0x82642180 writes +26384 with a single `std` (0x826421B0, from
        // the event's own 8-byte `ld 8(evt)`) and writes the entity id SEPARATELY four bytes later
        // at +26392 with `stw` (0x826421BC). So model+26384..26391 IS mHandlingBodyID alone, and
        // the console's `ld` returns it verbatim. Which is also the coherent reading: a rigid-body
        // id and a volume-instance id are the same shape (entity word in the high dword, index in
        // the low), so the handle IS the volume-instance id -- no re-packing required.
        CgsSceneManager::VolumeInstanceId GetHandlingBodyVolumeInstanceId() const
        {
            CgsSceneManager::VolumeInstanceId lId;
            lId.muId = static_cast<u64>(mHandlingBodyID);
            return lId;
        }

        // DWARF :347. This car's global scene-entity id. DECLARE-ONLY (its body reads mGlobalEntityId).
        EntityId GetGlobalEntityId() { return mGlobalEntityId; }                                // :347 (inlined walls leg 4 -- console-inline member read)

        bool IsActive();                                                                        // :351
        void InvalidateDeformationModel();                                                      // :354
        void ValidateDeformationModel(ResourceHandle lHandle);                                  // :358 (BrnPhysics::Deformation::ResourceHandle)
        BrnPhysics::Vehicle::VehiclePhysics*       GetVehiclePhysics();                         // :361
        const BrnPhysics::Vehicle::VehiclePhysics* GetVehiclePhysics() const;                   // :365
        void GetTransform(Matrix44Affine& lrOut) const;                                         // :370
        Matrix44Affine GetTransformDelta() const;                                              // :373
        void SetTransform(const rw::math::vpu::Matrix44Affine* lpTransform);                    // :378
        void GetInverseTransform(Matrix44Affine& lrOut);                                        // :383
        void GetLinearVelocity(Vector3& lrOut) const;                                           // :387
        void GetAngularVelocity(Vector3& lrOut) const;                                          // :391
        Vector3 GetHalfExtents();                                                               // :394
        Vector3 GetLocalVelocityFromLocalPoint(Vector3 lPoint) const;                           // :398
        Vector3 GetLocalVelocityFromWorldPoint(Vector3 lPoint) const;                           // :403

        // DWARF :407. The deformation sensor at the given index (indexes maDeformationSensors).
        // DECLARE-ONLY (ApplyCarCarImpulse calls it BY NAME).
        DeformationSensor* GetDeformationSensor(s32 liSensorIndex) { return &maDeformationSensors[liSensorIndex]; }   // :407 (inlined walls leg 4)

        // :410. ⭐ HEADER-INLINE (2026-08-14, deformation-mount wave): no export on either
        // console (console-inline). The count is the spec's sensor count PLUS the four appended
        // wheel spheres -- _Update.cpp's own member map records the relation verbatim
        // ("spec+1618 == mu8NumDeformationSensors; == GetNumSensors()-4"), and ResetSensors
        // seeds exactly numSensors+4 world spheres.
        s32 GetNumSensors() { return static_cast<s32>(mpDeformationSpec->mu8NumDeformationSensors) + 4; }   // :410
        DeformationSensor& GetDeformationSensorFromVolumeInstance(CgsSceneManager::VolumeInstanceId lId); // :415
        CgsGeometric::Sphere GetDeformationSphereFromVolumeInstance(CgsSceneManager::VolumeInstanceId lId); // :419
        // :422 / :426 -- HEADER-INLINE as of 2026-08-24 (deform-land wave; they were
        // DECLARE-ONLY with no body anywhere): no out-of-line X360 emission -- OutputData
        // @0x826225D8's pass-1 pushes read both as bare member addresses (model+4320 ==
        // &maVerletOffsets_Scratch[0], model+1152 == &mLocatorData).
        const rw::math::vpu::Vector3Plus* GetOffset_ScratchArray() { return maVerletOffsets_Scratch; } // :422
        const VehicleLocatorData* GetLocatorData() { return &mLocatorData; }                   // :426
        const IKBodyPart* GetIKPart(s32 liIndex);                                               // :430
        // :434. ⭐ HEADER-INLINE (2026-08-14, walls leg 1): no export on X360 -- the console
        // inlines it into DeformationManager::GetSpheresForCar @0x825C2260 (an export hole,
        // lifted from the image: `*out = model base` == &maWorldSensorSpheres[0], the class's
        // first member; count = spec+1618 (+4) == GetNumSensors()). GetNumSensors precedent.
        // Unlike GetSweptSpheres there is NO mbDoSweptSphereTests tripwire on this one.
        s32 GetWorldSpaceSpheres(const CgsGeometric::Sphere** lppSpheresOut)                    // :434
        {
            *lppSpheresOut = maWorldSensorSpheres;   // model + 0 (0x825C229C stw r11, 0(r30))
            return GetNumSensors();                  // spec+1618 + 4 (0x825C22A0..0x825C22A8)
        }
        s32 GetSweptSpheres(const CgsGeometric::SweptSphere** lppSpheresOut);                   // :443
        bool IsUsingSweptSpheres();                                                             // :452
        VecFloat GetWeightFactor();                                                             // :455

        // DWARF :459. Combined restitution for a vehicle-vs-vehicle contact (consults the two cars'
        // absorption sets / materials). DECLARE-ONLY (ApplyCarCarImpulse calls it BY NAME).
        VecFloat GetVehicleWorldRestitution(const StoredImpulseContact& lContact) const;        // :459

        void OutputWheelData(s32 liWheelIndex, DeformationOutputInterfaceForEntityModules* lpOutEM,
                             DetachedWheelManager* lpWheelMgr);                                  // :465
        void UpdateAndOutputJointStates(DeformationOutputInterface* lpOut,
                                        DetachedPartManager* lpPartMgr);                         // :470

        // Console-INLINE (no standalone X360 symbol): DeformationManager::OutputData
        // @0x826225D8 emits this in line immediately before each model's OutputWheelData call
        // (0x82622A64..0x82622AC8): re-seed the entity sphere size --
        //   mLastLinearVelocityPlusEntityRadius.w = |vehicle->GetHalfExtent()|  (zero-guarded
        //   rsqrt normalize-magnitude idiom, vrlimi mask 1 == w lane only).
        // Exists so OutputData can spell the private members BY NAME. Bodied beside the other
        // output-family members in BrnDeformableObject_GlassState.cpp.
        void RefreshEntitySphereSizeFromVehicleExtent();                                        // (OutputData inline slice)
        void OutputState(CarState* lpCarState);                                                 // :473
        s16 GetNumPhysicalParts() const { return mi16NumPhysicalParts; }                        // :476 (inlined walls leg 4)
        s16 GetNumHingedParts() const { return mi16NumHingedParts; }                            // :480 (inlined walls leg 4)
        // :491/:501. World-space contact generation for the body parts / detached wheels. FLAG: the
        // DWARF spells arg1 as the nested VehicleInputInterface::InTriangleCacheInterface; here it is the
        // standalone forward-declared InTriangleCacheInterface (its enclosing VehicleInputInterface is
        // not in-tree), and CollisionGenerator / LinearMalloc are the qualified CgsPhysics:: /
        // CgsMemory:: forward-declared homes. All referenced by pointer only.
        void DoBodyPartWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                              ContactGenList* lpGenList, CgsSceneManager::CgsCollision::CollisionGenerator* lpGen,
                                              CgsMemory::SimpleDataStreamProducer* lpProducer,
                                              const DetachedPartManager* lpPartMgr, u32 luResultListTag,
                                              CgsMemory::LinearMalloc* lpAlloc) const;            // :491
        void DoDetachedWheelWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                                   ContactGenList* lpGenList, CgsSceneManager::CgsCollision::CollisionGenerator* lpGen,
                                                   CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                   const DetachedWheelManager* lpWheelMgr, u32 luResultListTag,
                                                   CgsMemory::LinearMalloc* lpAlloc) const;       // :501
        Vector3 GetMeshOffset() const;                                                          // :504
        Vector3 GetComOffset() const;                                                           // :507
        Vector3 GetRigidBodySpecPosition() const;                                               // :511
        // :516. HEADER-INLINE: no X360 export under any name; the console
        // inlines the maPartStates read at every use site (DeformationManager::AddHingedBodyPartPairs
        // @0x82605B28 `lbzx r11, r23, r26`). GetNumSensors / GetWorldSpaceSpheres precedent.
        EPartState GetPartState(s32 liIndex) const                                              // :516
        {
            return static_cast<EPartState>(maPartStates[liIndex]);
        }
        bool HasDeformedThisFrame() const;                                                      // :519
        void ClearDeformedThisFrameFlag();                                                      // :522

        // DWARF :527/:531. Bounding boxes (later waves body these against the sphere/part state).
        void GetBoundingBox(CgsGeometric::Box* lpBoxOut);                                       // :527
        void GetAlignedDeformedBoundingBox(CgsGeometric::Box* lpBoxOut);                        // :531

        void ConstructUpdatePerformanceMonitors();                                             // :535
        void ConstructUpdateIKAndLocatorsPerformanceMonitors();                                // :538
        void ConstructPostPhysicsPerformanceMonitors();                                        // :541
        void GetDeformedBoundingBox(CgsGeometric::AxisAlignedBox* lpBoxOut);                    // :545

        // DWARF :553. Push this car's stored contacts into the shared penetration solver (vs the
        // model-pool base). const. DECLARE-ONLY.
        // Parameter names are the DWARF's (BrnDeformableObject.cpp:1877): a3 is the WORLD
        // pseudo-body index (the caller passes KI_MAX_DEFORMATION_MODELS), a4 is THIS car's model
        // index -- not the other way round.
        void AddContactsToPenetrationSolver(PenetrationSolver* lpSolver, DeformableObject* lpDefObjBase,
                                            s32 liWorldObjectIndex, s32 liObjectIndex) const;    // :553

        EAbsorptionSets GetAbsorptionSet();                                                     // :556
        void ResetEntitySphereSize();                                                           // :560
        bool IsFrozen() const;                                                                  // :563
        void SetFrozen(bool lbFrozen);                                                          // :567
        void ResetCoolDown();                                                                   // :570
        bool IsIKUpdateRequired() const;                                                        // :573
        void ClearStoredContacts();                                                             // :578
        void GetWheelTagPoints(Vector3* lpaOut) const;                                          // :583
        void VerifyPartIndicies(DetachedPartManager* lpPartMgr);                                // :589

        // =========================================================================================
        // Committed-slice accessors (kept compiling for BrnDeformableObject.cpp) -- mapped to real
        // members. These are inline, bodied, and reach only the real (or flagged reconstructed)
        // members.
        // =========================================================================================

        // The attached externally-simulated physics body -- the "A" body of the two-body impulse
        // solve. Resolved through the embedded VehicleRigidBody (mVehicleBody): its attached vehicle
        // physics IS-A ExternalPhysicsBody (VehiclePhysics derives the physics body). FLAG: the
        // VehicleRigidBody slice exposes GetVehiclePhysics() (a VehiclePhysics*); the impulse solver
        // wants an ExternalPhysicsBody&. The body resolves the downcast -- here we expose the vehicle
        // physics as the body via a reinterpret to keep the committed call sites (GetVehicleBody()
        // .GetLocalVelocity/.CalculateCollisionImpulseWithBody/.SetFrozen) compiling. Promote to a
        // typed GetExternalPhysicsBody() accessor on VehicleRigidBody when that slice grows.
        // ⭐⭐ CAST FIXED 2026-08-14 (walls leg 4, PROBED at boot 3): the old reinterpret_cast did
        // NOT adjust for the polymorphic derived's vptr -- SimpleVehiclePhysics introduces the
        // vtable, so the (non-polymorphic) ExternalPhysicsBody base subobject sits 16 bytes into
        // the derived object on the host. The reinterpret view therefore read every transform row
        // one row LOW (the probe's "position" was a unit z-row) and CalculateNewVelocity through
        // it STOMPED the vptr + transform -- the boot-2/3 car teleport. The implicit derived->base
        // upcast is pointer-adjusting and exact.
        ExternalPhysicsBody& GetVehicleBody()
        {
            return *mVehicleBody.GetVehiclePhysics();
        }
        const ExternalPhysicsBody& GetVehicleBody() const
        {
            return *mVehicleBody.GetVehiclePhysics();
        }

        // The attached vehicle physics, viewed as a RaceCarPhysics (the inlined downcast the bounce
        // path uses). Null when the attached vehicle is not a race car. Resolved from mVehicleBody's
        // attached vehicle. FLAG: the static_cast is the bounce-boost entry point's downcast; the
        // body verifies the vehicle is actually a race car before using it.
        Vehicle::RaceCarPhysics* AsRaceCarPhysics()
        {
            return static_cast<Vehicle::RaceCarPhysics*>(mVehicleBody.GetVehiclePhysics());
        }
        const Vehicle::RaceCarPhysics* AsRaceCarPhysics() const
        {
            return static_cast<const Vehicle::RaceCarPhysics*>(mVehicleBody.GetVehiclePhysics());
        }

        // The other car's packed game-mode/state word (the asm's `*(...+26392)`); the bounce logic
        // tests its HIGH byte == 2 (the takedown/showtime game-mode selector). FLAG: there is NO
        // game-mode word in the DeformableObject DWARF member sequence -- the bounce gate reads it off
        // the OTHER car. It is retained as the reconstructed member mu32GameModeState (appended below,
        // NOT a DWARF member) so the committed bounce body keeps compiling. Re-home it onto the real
        // game-state source (likely the entity module) when that read is re-derived.
        u8 GetGameModeByte() const { return static_cast<u8>(mu32GameModeState >> 24); }

        // Cross-car bounce flag mutators (named-member writes -- no raw-offset pokes). FLAG: backed by
        // the reconstructed mbHasBouncedThisFrame / mbBounceRandomParity members (asm +26414 / +26413;
        // no separately-named DWARF members) appended below.
        void SetHasBouncedThisFrame(bool lb) { mbHasBouncedThisFrame = lb ? 1u : 0u; }
        bool HasBouncedThisFrame() const     { return mbHasBouncedThisFrame != 0u; }
        void SetBounceRandomParity(bool lb)  { mbBounceRandomParity = lb ? 1u : 0u; }

        // =========================================================================================
        // Debug-visualiser accessors (for BrnPhysics::Deformation::DeformationDebugComponent).
        //
        // The deformation debug component walks this car's whole rig -- the streamed spec, the sensor /
        // tag-point / driven-point / IK-part pools, and the drive-time limit box -- to render and edit it.
        // It is NOT a friend, so it reaches the (private) pools through these named accessors. Each is a
        // by-member-name view of the X360 raw-offset read the component's asm performs (the console
        // offset is noted for provenance); none widen/poke raw bytes. Reconstructed alongside
        // BrnDeformationDebugComponent.cpp.
        // =========================================================================================

        // The streamed deformation spec (asm rig+6368 == mpDeformationSpec). Null until prepared.
        const StreamedDeformationSpec* GetDeformationSpec() const { return mpDeformationSpec; }

        // The attached vehicle physics (asm rig+6476 == mVehicleBody.GetVehiclePhysics()). The
        // detached-wheel debug draw reads the wheel array off it. May be null.
        BrnPhysics::Vehicle::VehiclePhysics* GetVehiclePhysicsDebug() { return mVehicleBody.GetVehiclePhysics(); }

        // Live tag-point pool (asm rig+19216 count, rig+19232 base; TagPoint stride 0x20).
        s32             GetNumTagPointsDebug() const { return miNumTagPoints; }
        TagPoint&       GetTagPointDebug(s32 liIndex) { return maTagPoints[liIndex]; }
        const TagPoint& GetTagPointDebug(s32 liIndex) const { return maTagPoints[liIndex]; }

        // Live driven-point pool (asm count == miNumDrivenPoints, base rig+19232+...; IKDrivenPoint
        // stride 0x30). Note: maTagPoints precedes maDrivenPoints, so the driven-point base in the asm
        // is computed from the tag-point base; here it is the named maDrivenPoints array.
        s32                  GetNumDrivenPointsDebug() const { return miNumDrivenPoints; }
        IKDrivenPoint&       GetDrivenPointDebug(s32 liIndex) { return maDrivenPoints[liIndex]; }
        const IKDrivenPoint& GetDrivenPointDebug(s32 liIndex) const { return maDrivenPoints[liIndex]; }

        // Live IK-part pool (asm rig+26232 count, rig+25388 base).
        s32               GetNumIKPartsDebug() const { return miNumIKBodyParts; }
        IKBodyPart&       GetIKPartDebug(s32 liIndex) { return maIKParts[liIndex]; }
        const IKBodyPart& GetIKPartDebug(s32 liIndex) const { return maIKParts[liIndex]; }

        // Deformation sensor pool (asm rig+15120 base; DeformationSensor stride 0x1B0 == 432). The
        // component's selected-sensor read seeds the index at +15 (the first 15 sensor slots are the
        // reserved world/swept entries that precede the live grid in the X360 layout -- see asm
        // `432*(liIndex+15)+rig`). DebugSensor takes the already-biased index.
        DeformationSensor&       GetSensorDebug(s32 liIndex) { return maDeformationSensors[liIndex]; }
        const DeformationSensor& GetSensorDebug(s32 liIndex) const { return maDeformationSensors[liIndex]; }

        // Drive-time deformation limit box (asm rig+26352 min, rig+26368 max == mDriveTimeBBoxLimitMin /
        // mDriveTimeBBoxLimitMax, written by CalculateDriveTimeLimits).
        const Vector3& GetDriveTimeLimitsMin() const { return mDriveTimeBBoxLimitMin; }
        const Vector3& GetDriveTimeLimitsMax() const { return mDriveTimeBBoxLimitMax; }

        // The rig's "is this deformable model live" flag (asm rig+26402 == mbActive). The debug
        // component's RenderWorld walks the manager's active-rig bit array, then skips any rig whose
        // mbActive byte is clear before rendering its points. Named view of that direct byte read
        // (the IsActive() body lives in another TU; this exposes the same member without it).
        bool IsActiveDebug() const { return mbActive; }

        // Mark the rig "deformed / needs re-skin this update" (asm rig+26408 = 1, written when the debug
        // component nudges a sensor/driven point). Backed by the named mbHasDeformedThisFrame flag.
        void SetDeformedThisFrameDebug() { mbHasDeformedThisFrame = true; }

        // ⭐ 2026-08-14 (walls leg 4): the per-frame latch clear DeformationManager::Update's
        // opening loop stores (model+26408 = 0, X360 @0x82649BD4). Inline console store.
        void ClearHasDeformedThisFrame() { mbHasDeformedThisFrame = false; }

        // Public debug wrappers around the (private) per-frame rebuild helpers the debug component drives
        // directly (the X360 calls UpdateDeformedBBox / CalculateDriveTimeLimits BY ADDRESS from the
        // component). Forward to the private members so the component does not need friendship.
        void UpdateDeformedBBoxDebug()       { UpdateDeformedBBox(); }
        void CalculateDriveTimeLimitsDebug() { CalculateDriveTimeLimits(); }

        // Public wrapper around the (private) sensor render the debug component's RenderWorld calls
        // (asm rig RenderSensors). Forwards to the private member.
        void RenderSensorsDebug(CgsDev::Debug3DImmediateRender* lpRender, s32 liFlags) const
        {
            RenderSensors(lpRender, liFlags);
        }

        // ⭐ ADDITIVE named seat write (2026-08-14, walls wave). DeformationManager::
        // ProcessValidateDeformationModelEvents @0x825DB0E0 stores the re-resolved (or NULLed)
        // spec pointer STRAIGHT into model+0x18E0 == mpDeformationSpec (0x825DB164 `stw r9` /
        // 0x825DB174 `stw r26,0`) -- a direct cross-object poke on the console. This named setter
        // is that store's by-name equivalent, same shape as the debug wrappers above (no
        // friendship, no raw offset).
        void SetDeformationSpec(const StreamedDeformationSpec* lpSpec) { mpDeformationSpec = lpSpec; }

        // ⭐ 2026-08-14 (walls wave): the one friend grant the manager's Prepare needs --
        // DeformationManager::Prepare @0x82630230 calls the PRIVATE ClearVariables on each pool
        // model (0x826303CC `bl ...ClearVariables` inside manager code). The tree reaches it via
        // the free trampoline the home TU declares; this friend line makes that trampoline (defined
        // beside the private body in BrnDeformableObject_Lifecycle.cpp) legal without widening the
        // member's access.
        friend void DeformableObject_ClearVariables(DeformableObject* lpModel);

    private:
        // =========================================================================================
        // FULL DWARF MEMBER SEQUENCE (BrnDeformableObject.h:600-677). Members BY NAME + SEQUENCE.
        // =========================================================================================

        CgsGeometric::Sphere      maWorldSensorSpheres[24];     // :600 world-space collision spheres
        CgsGeometric::SweptSphere maSweptSpheres[24];           // :604 per-frame swept (continuous) spheres
        VehicleLocatorData        mLocatorData;                 // :606 camera/light/generic locator table (BY VALUE)
        VecFloat                  mAngularVelocitySum;          // :608 accumulated angular velocity (cooldown)
        u8                        mu8WheelTagPointIndices[4];   // :611 wheel -> tag-point index map (255 = none)
        CgsGeometric::Sphere      maLocalSensorSpheres[24];     // :613 body-local collision spheres
        Vector3Plus               maVerletOffsets_Scratch[128]; // :614 per-driven-point skinning offset scratch
        const StreamedDeformationSpec* mpDeformationSpec;       // :615 the streamed (resource) deformable spec
        ImpulsePasser             mImpulsePasser;               // :616 impulse-chain map (BY VALUE)
        VehicleRigidBody          mVehicleBody;                 // :617 the car's collidable rigid body (BY VALUE)
        DeformationSensor         maDeformationSensors[20];     // :618 impact sensors (BY VALUE)
        TagPoint                  maTagPoints[128];             // :620 skinned tag points (BY VALUE)
        s32                       miNumTagPoints;               // :621 live tag-point count
        IKDrivenPoint             maDrivenPoints[128];          // :623 IK-driven mesh points (BY VALUE)
        s32                       miNumDrivenPoints;            // :624 live driven-point count
        IKBodyPart                maIKParts[50];                // :626 IK body parts / panels (BY VALUE)
        u8                        maPartStates[50];             // :627 per-part EPartState (stored as u8)
        s32                       miNumIKBodyParts;             // :628 live IK-part count
        u8                        mau8PhysicalBodyPartPoolIndex[50]; // :630 part -> physical-body-pool slot
        s16                       mi16NumPhysicalParts;         // :631 live physical-part count
        s16                       mi16NumHingedParts;           // :632 live hinged-part count
        u16                       mu16DeformableObjectIndex;    // :634 this object's index in the manager pool
        Vector3                   mLastAngularVelocity;         // :636 prev-frame angular velocity
        Vector3Plus               mLastLinearVelocityPlusEntityRadius; // :637 prev linear vel (w = entity radius)
        u8                        mau8WheelToSensorMap[4];      // :639 wheel -> sensor index map
        Vector3                   mDriveTimeBBoxLimitMin;       // :641 drive-time deformed-bbox clamp min
        Vector3                   mDriveTimeBBoxLimitMax;       // :642 drive-time deformed-bbox clamp max
        // ⭐⭐ EIGHT BYTES as of 2026-08-11 (handle-widening wave), and the widening puts BOTH of
        // these on their exact console seats for the first time. DeformableObject::Prepare
        // @0x82642180 writes them as a matched pair, and the widths are three instructions apart:
        //     0x826421A0  ld  r8,  8(evt)      0x826421B0  std r8,  0x6710(this)   <- +26384, 8B
        //     0x826421B8  lwz r27, 0x10(evt)   0x826421BC  stw r27, 0x6718(this)   <- +26392, 4B
        // At the old 4-byte width mGlobalEntityId sat at +26388 -- four bytes short of where the
        // console writes it. ⛔⛔ SEE THE mu32GameModeState FLAG at that member's declaration below:
        // that reconstructed member is this mGlobalEntityId, seen through the narrow handle.
        RigidBodyId               mHandlingBodyID;              // :644 the handling rigid-body id (console +26384)
        EntityId                  mGlobalEntityId;              // :645 global scene-entity id (GetGlobalEntityId) (console +26392)
        f32                       mfNoDamageTimer;              // :647 no-damage cooldown timer
        s16                       miNumAttachedExhausts;        // :649 attached exhaust count

        bool                      mbActive;                     // :651 is this deformable object live?

        // :653 culling group. FLAG: DWARF types this as InEventAddForCollision::CullingGroup
        // (a.k.a. SetRaceCarCullingGroupEvent::CullingGroup) -- a small int culling-group id
        // (CgsSceneManager::CullingGroupManager::CullingGroup == u32). Modelled as u32 to match the
        // underlying width; re-type to the qualified alias when that event header is in-tree.
        u32                       mCullGroup;                   // :653

        bool                      mbHasDeformedThisFrame;       // :655 deformed since last frame? (HasDeformedThisFrame)
        bool                      mbIKUpdateRequired;           // :656 IK needs re-solve? (IsIKUpdateRequired)
        bool                      mbDoSweptSphereTests;         // :658 run continuous (swept) collision? (IsUsingSweptSpheres)
        bool                      mbBonnetHasOpened;            // :660 bonnet latch opened this session
        bool                      mbBonnetLatchedDown;          // :661 bonnet currently latched down
        bool                      mbForceWheelsToDetach;        // :663 force all wheels off next update
        bool                      mbShowtimeShunting;           // :664 currently in a showtime shunt
        bool                      mbDontPlayGlassPaneEffects;   // :665 suppress glass-pane vfx
        bool                      mbResetDeformationNextUpdate; // :668 deferred deformation reset (ShouldResetDeformationNextUpdate)
        s8                        mi8NumPartsToForceHinging;    // :671 # parts to force-hinge next update
        EGlassState               maGlassPaneStates[10];        // :673 per-glass-pane state
        EAbsorptionSets           meAbsorptionSet;              // :674 active absorption profile (GetAbsorptionSet)
        Vector4                   mWheelTwistLimits;            // :676 per-wheel twist clamp band
        s8                        miNumBrokenWheels;            // :677 broken-wheel count

        // =========================================================================================
        // RECONSTRUCTED CROSS-CAR BOUNCE MEMBERS (committed ApplyCarCarImpulse slice -- NOT DWARF).
        // FLAG: the X360 bounce gate pokes a packed game-mode word at +26392 and two per-frame flag
        // bytes at +26413 / +26414. None of these appear as separately-named members in the
        // DeformableObject DWARF (:600-677 above ends at miNumBrokenWheels). They are retained here,
        // AFTER the recovered DWARF sequence (so they do not perturb the recovered member order), so
        // the committed BrnDeformableObject.cpp (GetGameModeByte / SetHasBouncedThisFrame /
        // HasBouncedThisFrame / SetBounceRandomParity) keeps compiling. Promote / re-home them when the
        // cross-car game-mode + bounce-latch reads are re-derived against their true owners.
        // =========================================================================================
        // ⛔⛔⛔ FLAG RAISED 2026-08-11 (handle-widening wave) -- READ BEFORE USING THIS MEMBER.
        // **mu32GameModeState IS ALMOST CERTAINLY mGlobalEntityId.** It was introduced because the
        // asm pokes "a packed game-mode word at +26392" that had no named member; +26392 had no
        // named member only because mHandlingBodyID was four bytes too narrow, which shoved
        // mGlobalEntityId down to +26388. With the handle at its real 8 bytes, mGlobalEntityId sits
        // at +26392 -- and DeformableObject::Prepare @0x82642180 writes exactly that seat from the
        // event's entity id: `0x826421B8 lwz r27, 0x10(evt)` ; `0x826421BC stw r27, 0x6718(this)`.
        // There is no separate game-mode word in the DWARF and none in Prepare.
        // ⇒ GetGameModeByte() (`mu32GameModeState >> 24`) is then the entity id's OWNER byte, i.e.
        //   the same entity-type selector GetHandlingBodyIdHighByte() returns for the other id --
        //   which is a coherent reading of a "game mode / player selector" byte, and explains why
        //   the two accessors always looked like duplicates of each other.
        // ⛔ NOT RETIRED IN THIS WAVE, deliberately: three bodies read it
        //   (BrnDeformableObject_Detach.cpp:531, _GlassState.cpp:291 and :404) and ALL THREE ARE IN
        //   UNMOUNTED TUs, so nothing this wave ships depends on the answer and no boot test can
        //   discriminate. Retiring it means re-reading those three console sites against
        //   mGlobalEntityId and re-checking _Detach's `RigidBodyId{ mu32GameModeState }` wrap.
        //   That is its own wave with its own evidence. ⚠️ Until then do NOT "clean this up" by
        //   deleting the member: the three readers would silently rebind to a different field.
        u32 mu32GameModeState;     // asm +26392 -- SEE THE FLAG ABOVE: this offset is mGlobalEntityId
        u8  mbBounceRandomParity;  // asm +26413 -- double-bounce random parity flag
        u8  mbHasBouncedThisFrame; // asm +26414 -- bounced-this-frame latch (double-bounce damp gate)

    private:
        // =========================================================================================
        // Private helpers (DWARF :696-832). DECLARE-ONLY -- bodies owned by the DeformableObject TUs.
        // =========================================================================================
        void SetLastLinearVelocity(Vector3 lVelocity);                                          // :696
        Vector3 GetLastLinearVelocity() const;                                                  // :699
        void SetEntitySphereSize(VecFloat lvfSize);                                             // :703
        // :706. ⭐ INLINED 2026-08-14 (walls leg 4): console-inline on both consoles -- the w-lane
        // splat of mLastLinearVelocityPlusEntityRadius (this+26320 lane 3; obj Update's
        // `lvx128 v0, this, 26320 ; vspltw v0, v0, 3` size read + the :1687 tripwire operand).
        VecFloat GetEntitySphereSize() const
        {
            const f32 lfSize = mLastLinearVelocityPlusEntityRadius.w;
            return VecFloat{ lfSize, lfSize, lfSize, lfSize };
        }
        void ResetSensors(Vector3Plus lA, Vector3 lB, Vector3 lC, Vector3 lD);                  // :713
        void PrepareIKPart(s32 liIndex);                                                        // :717
        void ClearVariables();                                                                  // :720
        IKBodyPart* GetBodyPartByType(EBodyParts leType);                                       // :723
        s32 GetNonDetachedJointedPart(s32 liIndex);                                             // :727
        s32 GetAttachedNonJointedPart(s32 liIndex);                                             // :731
        s32 GetHingedPart(s32 liIndex);                                                         // :735
        void UpdateContacts(VecFloat lvfTimeStep, CgsNumeric::Random& lrRandom);                // :740
        void UpdateOutputContactSpies(CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                      BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContacts); // :745 (PS3 mangle 0x74E9C0)
        void UpdateSpinningDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                      BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                      DetachedPartManager* lpPartMgr, VecFloat lvfTimeStep,
                                      CgsNumeric::Random& lrRandom);                             // :753 (param 2: PS3 mangle 0x757E80)
        void DetachPart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                        BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                        DetachedPartManager* lpPartMgr,
                        s32 liPartIndex, s32 liJointIndex, bool lbFlag);                         // :762
        void ApplyBreakingJointForces(PhysicalBodyPart* lpPart);                                // :766
        void SendGlassUpdateEvents(s32 liPaneIndex, DeformationOutputInterface* lpOut,
                                   DeformationOutputInterfaceForEntityModules* lpOutEM);         // :772
        void UpdateDeformedBBox();                                                              // :775
        void CalculateDriveTimeLimits();                                                        // :778
        void UpdateIKSuspensionOffsets();                                                       // :781
        void UpdateAbsorptionSet(s32 liGameMode);  // :785 (FLAG: DWARF arg = GameStateModuleIO::EGameModeType)
        bool UpdateGlassSmashedState(s32 liPaneIndex, VecFloat lvfTimeStep);                    // :790
        void PrepareLocators();                                                                 // :793
        void UpdateLocators(DetachedPartManager* lpPartMgr);                                    // :797
        void UpdateLocator(Matrix44Affine& lrTransform, ETagPointType& lreType,
                           const LocatorPointSpec* lpSpec,
                           const rw::math::vpu::Matrix44Affine& lrParent,
                           DetachedPartManager* lpPartMgr);                                      // :805
        void UpdateVelocity(VecFloat lvfTimeStep);                                              // :809
        bool ShouldDetachWheel(const TagPoint* lpTagPoint);                                     // :814
        void ResetJointVelocities(DetachedPartManager* lpPartMgr);                              // :818
        void GetInitialCompressionScalesAndLimits(DeformationResetType leResetType, VecFloat lvfTime,
                                                  Vector3Plus& lrScales, Vector3& lrLimitA,
                                                  Vector3& lrLimitB);                            // :826
        void RenderSensors(CgsDev::Debug3DImmediateRender* lpRender, s32 liFlags) const;        // :832
    };
}
}
