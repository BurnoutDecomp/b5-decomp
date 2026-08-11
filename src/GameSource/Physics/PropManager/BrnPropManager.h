#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                 // ::VecFloat, Vector3, Matrix44Affine
#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"           // PropDebugComponent (by value, +0x00)
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"     // PropInstance, PropEntityID
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h" // PropPartInstance
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // UpdatePropEvent
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                  // CgsContainers::BitArray
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                    // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                      // CgsModule::IOBuffer (PropRaceCarContactBuffer base, 2026-08-09)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // CgsResource::ResourcePtr<T>

namespace CgsPhysics { namespace PhysicsSimulationIO { struct InAddPotentialContact; struct OutContactSpy; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; } }
namespace BrnPhysics { namespace ContactSpy { struct PropContact; } }
namespace CgsMemory { struct SimpleDataStreamProducer; }   // pointer-only member (mpPrimitiveWithTriangleStream)
// ⭐ ADDED 2026-08-06 (big-five #2): SetupAndValidatePropContact collaborators, pointer use only.
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InputBuffer; } }  // class key struct, matching CgsPhysicsSimulationModuleIO.h:43
namespace BrnPhysics { namespace Vehicle { class VehicleManager; } }
// (PropRaceCarContactBuffer is DEFINED below as of 2026-08-09 -- DWARF home
//  BrnPropManager.h:47; the old fwd-decl line stood here.)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                        // CgsPhysics::RigidBodyId (by value)
// Class key `struct`, matching rw/rwcore_structs.h -- a `class` here mangles differently.
namespace rw { struct IResourceAllocator; }

// ⭐ ADDED 2026-08-09 (conductor wave) -- collaborators of the four per-frame prop legs,
// pointer/element use only. Class keys match each committed home.
namespace CgsSceneManager { namespace SceneManagerIO { struct TriangleCacheInterface;
                                                       struct InSceneUpdateInterface; } }
namespace CgsSceneManager { namespace CgsCollision { struct CollisionGenerator; } }
// ⭐ ADDED 2026-08-11 (lifetime wave): UpdateTriangleCache's parameter, pointer only.
// Class key `struct`, matching the single home CgsSceneManagerIO.h:31.
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Update; } }
// ⭐ ADDED 2026-08-10 (create-path wave): ProcessInputsPreScene's first parameter, pointer only.
// Class key `struct`, matching the single home SharedIO/BrnPropInputInterface.h:54.
namespace BrnPhysics { namespace Props { struct PropInputInterface; } }
// (CgsSceneManager::EntityId arrives complete through CgsRigidBody.h -> CgsEntityId.h.)
namespace CgsMemory { class LinearMalloc; }
namespace CgsPhysics { namespace PhysicsSimulationIO { struct OutUpdateRigidBody; } }
namespace BrnPhysics { namespace PhysicsModuleIO { class OutputBuffer; struct PotentialContactInterface; } }

namespace BrnPhysics
{
namespace Props
{
    class PropPhysicsDataHeader;      // ResourcePtr referent only (SharedClasses/Physics/Props)

    // ======================================================================================
    // PropRaceCarContactBuffer -- DWARF home BrnPropManager.h:47.       NEW 2026-08-09
    // (conductor wave). The per-frame prop-vs-racecar contact IO buffer PhysicsModule::
    // Update @0x825B0640 pushes on the output stack ("PropRaceCarContacts"). Console
    // attestation: CreateIOBuffer<PropRaceCarContactBuffer> @0x825AC4A0 allocates 992
    // bytes (`li r4,0x3E0`) and runs Construct, which constructs ONE
    // EventQueue<PropRaceCarContact,30> at +16 (`addi r3,r11,0x10 ; bl
    // EventQueue<PropRaceCarContact,30>::Construct @0x825A81B8`) -- 16 + (16 + 30*32)
    // == 992 with zero slack, and both spans are pointer-free-identical on the host
    // (the queue header pads to 16 on both targets).
    // ======================================================================================
    struct PropRaceCarContactBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<PropRaceCarContact, 30> PropRaceCarContactQueue;  // :62

        // :78 -- raise the buffer status, construct the queue (the X360 stack template's
        // inline; the PC stack template placement-news only, so Update calls this).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mPropRaceCarContactQueue.Construct();
        }

        // :81.
        void Destruct()
        {
            CgsModule::IOBuffer::Destruct();
        }

    private:
        u8                      maStatusPad[15];           // +1..+15 (force +16)
        PropRaceCarContactQueue mPropRaceCarContactQueue;  // +16  (:85)
    };

    // ---- The prop-physics tuning globals ------------------------------------------------
    // Namespace-scope, defined in BrnPropManager.cpp -- which is exactly where the DecFIGS
    // dwarfdump puts them (BrnPropManager.cpp:45..51). They are the twelve values the prop
    // debug component registers with the debug UI, and the seven VecFloats are what its
    // OnChange* callbacks splat-store into (X360 0x825BAEF0..0x825BB040, PS3 0x6B5AA4..0x6B5C7C
    // where the relocations carry the names).
    //
    // ⚠️ The five f32s below have real static initialisers, read straight out of the shipped
    //    image; the seven VecFloats read as all-zero on disk (their whole 0x82FB9xxx page is
    //    zero-filled) and the ARTIST export set contains NO function that initialises them --
    //    only readers and the OnChange* writers. Their initial values are therefore
    //    UNRECOVERED and are NOT invented here; see the definitions in BrnPropManager.cpp.
    extern ::VecFloat KVF_GRAVITY_SCALE;                   // X360 0x82FB94E0
    extern ::VecFloat KVF_INERTIA_SCALE;                   // X360 0x82FB9400
    extern ::VecFloat KVF_ANTI_HERD_UPWARD_SCALE;          // X360 0x82FB93E0
    extern ::VecFloat KVF_ANTI_HERD_SIDE_SCALE;            // X360 0x82FB9450
    extern ::VecFloat KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE; // X360 0x82FB9D70
    extern ::VecFloat KVF_MAX_SPEED_FOR_SIDE_FORCE;        // X360 0x82FB93A0
    extern ::VecFloat KVF_SPEED_CLAMP;                     // X360 0x82FB94A0

    extern f32 KF_PROP_ANGULAR_DRAG;                       // X360 0x82F2A388
    extern f32 KF_PROP_LINEAR_DRAG;                        // X360 0x82F2A38C
    extern f32 KF_PROP_MAX_ANGULAR_VEL;                    // X360 0x82F2A390
    extern f32 KF_PROP_MAX_LINEAR_VEL;                     // X360 0x82F2A394
    extern f32 KF_PROP_RESTITUTION;                        // X360 0x82F2A398

    // =====================================================================================
    // BrnPhysics::Props::PropManager -- FULL member sequence, DWARF declaration order
    // (references/DecFIGS/.../BrnPropManager.h:245..301), landing gap-free on every offset
    // the X360 asm touches. The offset column is NOT a host layout claim (x64 widens the
    // pointers); it is the evidence column -- each line is an actual load or store in the
    // ARTIST image, and the run being gap-free is what makes the name<->offset mapping a
    // proof rather than a proposal. The derivation, its arithmetic self-checks, and the
    // (now settled) history are in BrnPropManager.cpp's banner.
    //
    // ⭐ THE CLASS HAS NO BASE. Two things settle it: the dwarfdump prints base classes and
    //    prints `struct BrnPhysics::Props::PropManager {` with none, and Construct @0x82627390
    //    calls PropDebugComponent::Construct with r3 == r4 == this, i.e. &mDebugComponent ==
    //    this. The `bl BaseCollisionGenerator::Destruct` in Destruct's tail (which an older
    //    note read as a base-class call) is an ICF fold: THREE different empty `void f(T*)`
    //    bodies in this one subsystem call that same address -- PropDebugComponent::Construct
    //    @0x825BAD74 where DebugComponent::Construct belongs, PropDebugComponent::OnRegister
    //    @0x822A9750 (a bare `b` to it) where DebugComponent::OnRegister belongs, and
    //    PropManager::Destruct @0x825E33E4 where mDebugComponent's own Destruct tail belongs.
    // =====================================================================================
    class PropManager
    {
    public:
        // DWARF BrnPropManager.h:103..107 (nested). Sized by Construct's own allocation
        // request: 0x600 bytes for KI_MAX_DEBUG_WORLD_CONTACTS(32) entries == 48 bytes each,
        // which is exactly three Vector3s.
        struct DebugWorldContactInfo
        {
            Vector3 mPoint0;
            Vector3 mPoint1;
            Vector3 mNormal;
        };

        static const s32 KI_MAX_DEBUG_WORLD_CONTACTS = 32;    // DWARF BrnPropManager.h:110

        // ADDITIVE 2026-08-04 (task #135) -- X360 @0x82C08ED0, the stage-5 arm of
        // BrnPhysics::PhysicsModule::Prepare @0x825ADB68 (`bl` at 0x825ADDCC, result tested
        // with a `bne` so the return is a bool). Its one argument is the physics resource
        // allocator (bank 23). Declaration-only; the body is a named LINK STUB in
        // WorldLinkStubs.cpp until this manager's own prepare pass lands, so the drop is one
        // greppable symbol rather than a silent `return true` for the whole physics module.
        bool Prepare( rw::IResourceAllocator* lpAllocator );

        // ==========================================================================
        // ⭐ ADDED 2026-08-09 (conductor wave -- PhysicsModule::Update @0x825B0640's
        // prop legs). Signatures per the PS3 DecFIGS mangles (0x77F694 names
        // OutputUpdatedProps; the generation pair and ReadUpdatedBodies carry their
        // param lists in the same export set). ⚠ FLAG: DECLARED for the conductor's
        // closure; all four bodies are LOUD one-shot gates in
        // BrnPhysicsConductorGates.cpp until reconstructed.
        // ==========================================================================
        void BeginPropWorldContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpCollisionGenerator,
            CgsMemory::LinearMalloc* lpLinearMalloc,
            VecFloat lvfTimeStep );                              // @0x82628CB0

        void EndPropWorldContactGeneration(
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpCollisionGenerator,
            CgsSceneManager::EntityId lWorldEntityId );          // @0x82628E18

        void ReadUpdatedBodies(
            const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodyQueue,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer,
            VecFloat lvfTimeStep );                              // @0x82632918

        void OutputUpdatedProps(
            BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutputBuffer ); // @0x82627EC8

        // ⭐ ADDED 2026-08-10 (create-path wave -- PhysicsModule::PostSceneUpdate @0x825ABC10's
        // prop leg, its fifth call). @0x8263AF30 (209 insns). Drains the prop input interface's
        // add/remove prop-and-part instance queues and re-runs the jointed-prop update; its own
        // callees (ProcessRemovePropInstanceEvents / ProcessRemovePartInstanceEvents /
        // RemoveAllPropsAndParts / ProcessAddPropInstanceEvents / ProcessAddPartInstanceEvents /
        // UpdateJointedProps) are none of them reconstructed. ⚠ FLAG: DECLARED for
        // PostSceneUpdate's closure; body is a LOUD one-shot gate (BrnPhysicsConductorGates.cpp).
        // The bool is PostSceneUpdate's own `lUpdateSet & 1` (the network-catchup flag).
        void ProcessInputsPreScene(
            const BrnPhysics::Props::PropInputInterface* lpPropInputInterface,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
            bool lbNetworkCatchup,
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer ); // @0x8263AF30

        // @0x826119A0 (116 insns; 573-instruction closure). Arm 2 of
        // PhysicsModule::UpdateCachedPositions @0x8259C370: per live prop, post one
        // InEventUpdateCachedPosition for that prop's triangle-cache slot. Signature from the PS3
        // DWARF (..PropManager19UpdateTriangleCacheEPN15CgsSceneManager14SceneManagerIO18
        // InputBuffer_UpdateE). ⚠ FLAG: DECLARED for UpdateCachedPositions' closure; body is a
        // LOUD one-shot gate (BrnPhysicsConductorGates.cpp) -- props own ZERO triangle-cache
        // slots today, so a gate here drops nothing.
        void UpdateTriangleCache(
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update);

        static const s32 KI_PROP_INDEX_NOT_FOUND     = -1;    // DWARF BrnPropManager.h:245

        typedef CgsModule::EventQueue<UpdatePropEvent, 200> UpdatePropEventQueue;
        typedef CgsModule::EventQueue<UpdatePropEvent, 15>  UpdateJointedPropEventQueue;

        PropDebugComponent           mDebugComponent;              // X360 +0x0000  (sizeof 0x48)
        bool                         mbRenderCOM;                  // X360 +0x0048  stb 0
        bool                         mbUseOverides;                // X360 +0x0049  stb 0
        f32                          mfMassOverride;               // X360 +0x004C  = 10.0f
        f32                          mfMaxLeanAngleOverride;       // X360 +0x0050  =  0.0f

        // X360 +0x0054. PropDebugComponent::RenderStats @0x826131E8 reaches it as
        // `addi r3,r11,0x54` into ResourcePtr<T>::operator-> -- identified by that call's baked
        // assert line 0x220 == 544, the non-const operator->'s line in CgsResourcePtr.h -- and
        // then reads word 0 of the header == muNumberOfPropTypes. Construct does NOT touch it.
        CgsResource::ResourcePtr<PropPhysicsDataHeader> mpPhysicsData;

        f32                          mfStaticFriction;             // X360 +0x0074  = 0.3f
        f32                          mfDynamicFriction;            // X360 +0x0078  = 0.6f

        // The instance-array slice. The three query getters (FindPropIndex /
        // HasPropJustBeenRemoved / HasPartJustBeenRemoved) attest each stride/offset:
        // PropInstance stride 112 with mEntityId @+0x60; PropPartInstance stride 64 with
        // mEntityId @+0x30; the two BitArrays read at this+0x80 / this+0x90.
        PropInstance*                mpaPropInstances;             // X360 +0x007C
        CgsContainers::BitArray<15>  mUsedProps;                   // X360 +0x0080  std 0
        u32                          muNumberOfPropInstances;      // X360 +0x0088
        PropPartInstance*            mpaPartInstances;             // X360 +0x008C
        CgsContainers::BitArray<30>  mUsedParts;                   // X360 +0x0090  std 0
        u32                          muNumberOfPartInstances;      // X360 +0x0098

        // The perf-monitor slice. miNumJobsAdded is attested by
        // BeginPropWorldContactGeneration @0x82628CFC (`stw r26,0x9C(r31)`, r26 == 0);
        // mpPrimitiveWithTriangleStream by Construct (`stw r30,0xA0(r31)`) and by that same
        // function storing a stream producer there; the five monitor ids by the two
        // Construct*PerfMonitors bodies, whose own NAMES name the members they zero.
        // miProcessBreakPropPM is written by NEITHER constructor -- a fact of the shipped
        // image, stated rather than smoothed over.
        s32                          miNumJobsAdded;               // X360 +0x009C
        CgsMemory::SimpleDataStreamProducer* mpPrimitiveWithTriangleStream;  // X360 +0x00A0
        s32                          miContactGeneratorWaitPM;     // X360 +0x00A4
        s32                          miProcessRemovePropPM;        // X360 +0x00A8
        s32                          miProcessRemovePartPM;        // X360 +0x00AC
        s32                          miProcessAddPropInstancePM;   // X360 +0x00B0
        s32                          miProcessAddPartInstancePM;   // X360 +0x00B4
        s32                          miProcessBreakPropPM;         // X360 +0x00B8

        // The prop-joint block. Untouched by Construct except the two bit-sets; the strides
        // are what close the run from +0xC0 to +0x680 exactly (15*16 + 15*16 + 15 (padded to
        // 16) + 15*64 == 0x5B0, i.e. 0xC0 + 0x5B0 == 0x670).
        Vector3                      maPropJointPositions[15];     // X360 +0x00C0
        Vector3                      maLastJointRotation[15];      // X360 +0x01B0
        u8                           mauPropIndexForJoint[15];     // X360 +0x02A0
        Matrix44Affine               maCurrentJointTransforms[15]; // X360 +0x02B0
        CgsContainers::BitArray<15>  mUsedPropJoints;              // X360 +0x0670  std 0
        CgsContainers::BitArray<15>  mBreakPropJoints;             // X360 +0x0678  std 0

        // Construct calls EventQueue<UpdatePropEvent,200>::Construct on this+0x680 and
        // EventQueue<UpdatePropEvent,15>::Construct on this+0x5E10. The difference,
        // 0x5790 == 0x10 + 200*112, and 0x64B0 - 0x5E10 == 0x6A0 == 0x10 + 15*112, are the
        // two arithmetic self-checks on the already-committed sizeof(UpdatePropEvent) == 112.
        UpdatePropEventQueue         mUpdatedProps;                // X360 +0x0680
        UpdateJointedPropEventQueue  mUpdatedJointedProps;         // X360 +0x5E10

        DebugWorldContactInfo*       mpDebugWorldContacts;         // X360 +0x64B0
        s32                          miNumDebugWorldContacts;      // X360 +0x64B4  stw 0
        bool                         mbDisableFreezing;            // X360 +0x64B8  stb 0
        PropEntityID                 maPropsAddedToContactGen[45]; // X360 +0x64BC  (45*4 == 0xB4)
        s32                          miNumPropsAddedToContactGen;  // X360 +0x6570  stw 0

        // X360 0x82627390 (82 asm). Defined in BrnPropManager.cpp.
        void Construct();

        // X360 0x825BAC60. Four instructions: `li r11,0 ; stw r11,0xA4(r3) ; blr`.
        void ConstructContactGenerationPerfMonitors();

        // X360 0x825BAC70. Six instructions: zero the four pre-scene process-event monitor ids.
        void ConstructPreScenePerfMonitors();

        // X360 0x825BACB0 (private prop/race-car helper; DWARF BrnPropManager.h:311).
        // Retargets the RACE-CAR side of a prop/race-car potential contact onto the shared
        // "dummy" race car by overwriting that RigidBodyId's EntityId owner-type with the
        // dummy-car owner (11). Does not touch PropManager state (no `this` use). Defined
        // out-of-line in BrnPropManager_RoutePropVsRaceCarContactToDummyCar.cpp.
        void RoutePropVsRaceCarContactToDummyCar(
            bool                                             lbPropIsEntityA,
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact );

        // ⭐ ADDED 2026-08-06 (bridge de-facade wave). DWARF BrnPropManager.h:172; X360 emission
        // @0x825A53A0 (bl target of PhysicsModule::StoreContact @0x825A5FA0 -- the console body
        // was header-inline here, its asserts bake BrnPropManager.h:529/530/533/534/550/563).
        // Build a ContactSpy::PropContact from a resolved prop spy + its potential contact:
        // BaseContact::Construct, entity word/type/state/flags from the prop or prop-part
        // instance tables, and the smash-gate / billboard graphics-id flag bits. Defined in
        // BrnPropManager.cpp.
        void CreateContactEvent( ContactSpy::PropContact* lpOutPropContact,
                                 const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
                                 const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact );

        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). @0x82628190 (PS3 DecFIGS
        // 0x79008C -- the mangle is the signature authority). Validate + set up one prop-vs-X
        // potential contact for the simulation (called by PhysicsModule::
        // BridgeContactsToSimulation when either owner is a PROP); returns false to drop the
        // contact. ⚠ FLAG: DECLARED for the bridge driver's closure; body still a TRAP STUB
        // (572 X360 asm lines / 24 callees -- named, not landed, this wave).
        bool SetupAndValidatePropContact(
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*        lpAddContactEvent,
            const CgsSceneManager::SceneManagerIO::PotentialContact*       lpPotentialContact,
            BrnPhysics::Vehicle::VehicleManager*                           lpVehicleManager,
            CgsPhysics::PhysicsSimulationIO::InputBuffer*                  lpSimModuleInputBuffer,
            PropRaceCarContactBuffer*                                      lpPropRaceCarContactBuffer,
            CgsPhysics::RigidBodyId                                        lWorldRigidBodyId,
            bool                                                           lbFrozen,
            f32                                                            lfTimeStep );

        // X360 0x82606148 (DWARF BrnPropManager.h:250). Linear-scan the used-prop bit-set;
        // return the slot whose stored PropEntityID matches, else -1.
        int32_t FindPropIndex( PropEntityID lEntityId ) const;

        // X360 0x825DEAC0 (DWARF :314). True iff the prop slot is now free OR holds a
        // different entity than lEntityId (i.e. the prop was removed/recycled this frame).
        bool HasPropJustBeenRemoved( PropEntityID lEntityId, int32_t liPropIndex );

        // X360 0x825DE930 (DWARF :317). Part-instance analogue of HasPropJustBeenRemoved.
        bool HasPartJustBeenRemoved( PropEntityID lEntityId, int32_t liPartIndex );
    };
}
}
