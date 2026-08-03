#pragma once

// BrnPhysics::PhysicsModule -- the top-level physics module (canonical home
// GameSource/Physics/BrnPhysicsModule.h, per the DecFIGS DWARF). It is a
// CgsModule::ModuleSingleBuffered that owns the whole physics runtime: the
// CgsPhysics simulation module, the vehicle manager, the contact-spy data, the
// deformation manager + its IO interfaces, and the prop manager, plus a large
// block of per-stage performance-marker ids and the prepare/release state
// machines.
//
// SCOPE OF THIS HOME (constructor slice): only the default constructor
// (BrnPhysics::PhysicsModule::PhysicsModule, X360 @0x827E5400) is bodied so far.
// The constructor's X360 asm touches exactly four things: the
// ModuleSingleBuffered base sub-object (vtable + the two EA::Thread::RWMutex
// members it constructs inline), the embedded CgsPhysics::PhysicsSimulationModule
// (its own constructor is chained at class +0x230), the embedded
// BrnPhysics::Vehicle::VehicleManager (its constructor is chained at +0x4AA0),
// and a trailing contained interface-with-intrusive-list sub-object at +0x63630
// (it stamps that sub-object's own vtable, then empty-initialises an intrusive
// list whose head/tail/iter pointers self-reference).
//
// LAYOUT MODEL. The full DWARF member set (BrnPhysicsModule.h:189..241) is
// recorded below as DOCUMENTATION, but several of those members (ContactSpyData,
// DeformationManager, PropManager, and the tail interface) have NO complete
// reconstructed type yet, so they cannot be embedded by value without fabricating
// layouts/vtables (which the project rules forbid). They are therefore modelled
// here as correctly-named, asm-sized OPAQUE PLACEHOLDER members so the four
// constructor-touched offsets land where the X360 asm proves them, and the
// constructor reproduces every store BY NAMED MEMBER (no raw-offset pointer
// hacks). Each placeholder is FLAGGED. When the real DeformationManager /
// PropManager / tail-interface layout passes land, the placeholders fold into the
// real typed members -- as mSimulationModule already has (2026-08-03).
//
// X360 byte offsets are the 4-byte-pointer console ABI; member access here is by
// name, so the bodies are faithful regardless of host pointer width.

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; supplies the vtable + the two RWMutexes the ctor constructs inline)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h" // CgsPhysics::PhysicsSimulationModule (embedded by value @ +0x230)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"   // BrnPhysics::Vehicle::VehicleManager (embedded by value @ +0x4AA0)

namespace CgsModule { struct IOBufferStack; }
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Update; } }
namespace BrnResource { namespace GameDataIO { struct AllocatorList; } }

namespace BrnPhysics
{
namespace PhysicsModuleIO { class InputBuffer; class OutputBuffer; }

    struct PhysicsModule : public CgsModule::ModuleSingleBuffered
    {
        // ADDITIVE (WorldModule::UpdatePhysicsNetworkCatchup @0x827B06E0 forwards
        // here -- X360 BrnPhysics::PhysicsModule::UpdateNetworkCatchup). Declaration-
        // only; body with the physics module's own TU.
        // RETYPED 2026-07-27 (world-drive wave): WorldModule::UpdatePhysicsNetworkCatchup
        // @0x827B06E0 forwards the physics INPUT buffer + the frame update set.
        void UpdateNetworkCatchup( const PhysicsModuleIO::InputBuffer* lpInputBuffer,
                                   BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (WorldModule::Update @0x827D63E8; DWARF BrnPhysicsModule.h
        //      :192..:201). Declaration-only; bodies gated in WorldLinkStubs.cpp
        //      until the physics module's own TU lands. ----
        void UpdateCachedPositions( CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer );  // @0x8259C370
        void PostSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,                                     // @0x825ABC10
                              CgsModule::IOBufferStack* lpOutputBufferStack,
                              const PhysicsModuleIO::InputBuffer* lpInputBuffer,
                              PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                              BrnUpdateSet lUpdateSet );
        void GenerateSceneQueries( PhysicsModuleIO::OutputBuffer* lpOutputBuffer,                               // @0x825A1428
                                   BrnUpdateSet lUpdateSet );
        void Update( CgsModule::IOBufferStack* lpInputBufferStack,                                              // @0x825B0640
                     CgsModule::IOBufferStack* lpOutputBufferStack,
                     const PhysicsModuleIO::InputBuffer* lpInputBuffer,
                     PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                     BrnUpdateSet lUpdateSet );

        // ---- ADDITIVE (attested by WorldModule::Prepare @0x827D53B0 stage 4) ----
        // Declaration-only; the body lands with the physics module's own TU.
        bool Prepare( CgsModule::IOBufferStack* lpInputBufferStack,
                      CgsModule::IOBufferStack* lpOutputBufferStack,
                      CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer,
                      BrnResource::GameDataIO::AllocatorList* lpAllocatorList );

        // ---- ADDITIVE (attested by WorldModule::Prepare @0x827D53B0 stage 9) ----
        void PropPrepareTypes( PhysicsModuleIO::InputBuffer* lpInputBuffer );

        // ---- prepare/release state machines (DWARF BrnPhysicsModule.h:75/89) ----
        // Member VALUES are DWARF-attested enumerators (not invented).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                 = 0,
            E_PREPARESTAGE_MANAGER               = 1,
            E_PREPARESTAGE_CONTACTMAPPER         = 2,
            E_PREPARESTAGE_SIMULATIONMODULE      = 3,
            E_PREPARESTAGE_DEFORMATIONMANAGER    = 4,
            E_PREPARESTAGE_PROPMANAGER           = 5,
            E_PREPARESTAGE_VEHICLEMODULE         = 6,
            E_PREPARESTAGE_CREATE_PLAYER_VEHICLE = 7,
            E_PREPARESTAGE_CREATE_WORLD_RIGIDBODY = 8,
            E_PREPARESTAGE_DONE                  = 9,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START              = 0,
            E_RELEASESTAGE_MANAGER            = 1,
            E_RELEASESTAGE_VEHICLEMODULE      = 2,
            E_RELEASESTAGE_PROPMANAGER        = 3,
            E_RELEASESTAGE_DEFORMATIONMANAGER = 4,
            E_RELEASESTAGE_SIMULATIONMODULE   = 5,
            E_RELEASESTAGE_CONTACTMAPPER      = 6,
            E_RELEASESTAGE_DONE               = 7,
        };

        PhysicsModule();

        // Virtual overrides declared by the DWARF (BrnPhysicsModule.cpp:52/192/348/449).
        // Declare-only here (bodied by their own TUs); the constructor only needs the
        // class to be constructible. These do NOT override the ModuleSingleBuffered
        // base virtuals of the same name -- the module-level Prepare/Release/Construct
        // surface is the no-arg base contract; the argful entry points below are the
        // physics module's own driver methods.
        void Construct() override;

    public:
        // ===================================================================
        // DWARF member set (BrnPhysicsModule.h:189..241), in declaration order.
        // The first two are constructor-touched; the heavy un-reconstructed ones
        // are asm-sized OPAQUE PLACEHOLDERS (FLAGGED); the trailing scalar block
        // is documented but folded into the opaque tail because the constructor
        // does not touch it.
        // ===================================================================

        // mSimulationModule -- CgsPhysics::PhysicsSimulationModule, X360 class offset
        // +0x230 (560). Embedded by value.
        //
        // PLACEHOLDER FOLDED 2026-08-03. This was `u8 maSimulationModulePlaceholder
        // [0x4AA0 - 0x230]`, an opaque span, because the class had no reconstructed
        // layout. It has one now (CgsPhysicsSimulationModule.h), re-derived from the
        // class's own constructor @0x827DF1E0 and from Construct @0x828A1EE8, and it
        // closes to the byte against exactly this span:
        //     0x4864 (mpSimulation) + 4 -> 8-align -> 0x4870 == 18544 == 0x4AA0 - 0x230.
        // ⚠️ The old comment here said that span was "18512 bytes". It is 18544
        // (19104 - 560); the array EXPRESSION was right, the prose number was not.
        //
        // Folding this also un-defers the sub-constructor: the X360 ctor's
        // `bl CgsPhysics::PhysicsSimulationModule::PhysicsSimulationModule(this+0x230)`
        // is now the implicit member construction of this member, which is what seeds
        // the 200 rw::physics::Inertia records and the 36 JointLimits.
        CgsPhysics::PhysicsSimulationModule mSimulationModule; // +0x230

        // mVehicleManager -- BrnPhysics::Vehicle::VehicleManager, X360 class offset
        // +0x4AA0 (19104). Embedded by value (it is a complete, layout-pinned type).
        BrnPhysics::Vehicle::VehicleManager mVehicleManager; // +0x4AA0

        // FLAG: PLACEHOLDER TAIL. Everything past mVehicleManager up to the trailing
        // contained interface-with-list sub-object at +0x63630. The DWARF members in
        // this span are mContactData (ContactSpyData), mDeformationManager
        // (DeformationManager), mDeformationInput (DeformationInputInterface),
        // mDeformationOutput (DeformationOutputInterface), mPropManager (PropManager),
        // the EPrepareStage/EReleaseStage state words, mWorldRigidBodyId,
        // mWorldEntityId, meCurrentGameMode, the ~37 int32 performance-marker ids
        // (miPhysicsPreSceneUpdatePM .. miPhysicsUpdateFixUpVehContactsPM),
        // miFramesToForceSuperSlowMotion, and mbIsOnlineGameMode. None of these is
        // touched by the constructor, and several have no complete reconstructed
        // type, so the whole span is modelled as one opaque placeholder. (sizeof on
        // the X360 ABI; the byte count below pins the tail interface to its proven
        // +0x63630 offset under the X360 sizes used here.)
        // ⛔⛔ 2026-08-03 (task #116) -- THIS PLACEHOLDER IS THE THING BLOCKING PhysicsModule::
        // Construct, and it is 26,012 BYTES SHORT. The class as modelled here ends at +0x636A0
        // (407,200). The X360 Construct @0x825AE308 writes as far as `*(this + 433208)`, so the real
        // object is at least 433,212 bytes. MEASURED, from the ARTIST asm, the exact console offsets
        // and the span each sub-object therefore occupies:
        //
        //     +19104   (0x04AA0)  mVehicleManager        span 172,624   [modelled, by value]
        //     +191728  (0x2ECF0)  mContactData           span 122,544   ContactSpyData
        //     +314272  (0x4CBA0)  mDeformationManager    span  76,752   DeformationManager
        //     +391024  (0x5F770)  mDeformationInput      span   5,072   DeformationInputInterface
        //     +396096  (0x60B40)  mDeformationOutput     span  10,992   DeformationOutputInterface
        //     +407088  (0x63630)  mPropManager           span  25,984   PropManager
        //     +433072..+433208              the state words + the twenty-one perfmon handle ids
        //
        // ⚠️⚠️ NOTE +407088 == 0x63630 -- that is EXACTLY where `mContainedList` sits below. The
        // "trailing contained interface with an intrusive list" is not a separate sub-object at all:
        // it is the leading part of mPropManager, which the constructor stamps and which
        // PropManager::Construct(this + 407088) then finishes. The constructor's stores are still
        // correct as stores; the TYPE they are attributed to is wrong.
        //
        // ⇒ Until the five sub-objects above are real embedded members and the trailing state/perfmon
        // block is modelled, PhysicsModule::Construct CANNOT be bodied: every one of its stores would
        // land inside this opaque array or past the end of the object. Its whole link closure is
        // already green (see WorldLinkStubs.cpp) -- the layout is the only thing left.
        u8 maTailPlaceholder[0x63630 - (0x4AA0 + sizeof(BrnPhysics::Vehicle::VehicleManager))]; // ends at +0x63630

        // The trailing contained interface sub-object (X360 +0x63630). The ctor
        // stamps its vtable (off_820CDF60) and then initialises an intrusive list at
        // +0x63684: three head ints, three self-referencing node pointers (head ==
        // tail == iter == &the list itself, i.e. an empty circular list), and a
        // trailing count word. FLAG: the owning interface type is not reconstructed;
        // its vtable + list are modelled here as named placeholder members so the
        // constructor reproduces the exact stores by name.
        struct ContainedListInterface
        {
            void* mpVTable;        // +0x00 (X360 +0x63630): vtable off_820CDF60

            // The embedded intrusive list begins at +0x54 (X360 +0x63684).
            u8    maPad0004[0x54 - sizeof(void*)];

            // --- intrusive list (empty / self-referencing on construction) ---
            s32   miListHead0;     // +0x54 (+0x63684): 0
            s32   miListHead1;     // +0x58 (+0x63688): 0
            s32   miListHead2;     // +0x5C (+0x6368C): 0
            void* mpListNext;      // +0x60 (+0x63690): &miListHead0 (self)
            void* mpListPrev;      // +0x64 (+0x63694): &miListHead0 (self)
            void* mpListIter;      // +0x68 (+0x63698): &miListHead0 (self)
            s32   miListCount;     // +0x6C (+0x6369C): 0
        };
        ContainedListInterface mContainedList; // +0x63630
    };
}
