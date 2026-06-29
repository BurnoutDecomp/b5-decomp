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
// recorded below as DOCUMENTATION, but several of those members
// (PhysicsSimulationModule, ContactSpyData, DeformationManager, PropManager, and
// the tail interface) have NO complete reconstructed type yet, so they cannot be
// embedded by value without fabricating layouts/vtables (which the project rules
// forbid). They are therefore modelled here as correctly-named, asm-sized OPAQUE
// PLACEHOLDER members so the four constructor-touched offsets land where the X360
// asm proves them, and the constructor reproduces every store BY NAMED MEMBER (no
// raw-offset pointer hacks). Each placeholder is FLAGGED. When the real
// PhysicsSimulationModule / DeformationManager / PropManager / tail-interface
// layout passes land, the placeholders fold into the real typed members.
//
// X360 byte offsets are the 4-byte-pointer console ABI; member access here is by
// name, so the bodies are faithful regardless of host pointer width.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; supplies the vtable + the two RWMutexes the ctor constructs inline)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"   // BrnPhysics::Vehicle::VehicleManager (embedded by value @ +0x4AA0)

namespace BrnPhysics
{
    struct PhysicsModule : public CgsModule::ModuleSingleBuffered
    {
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

        // FLAG: PLACEHOLDER. mSimulationModule == CgsPhysics::PhysicsSimulationModule,
        // X360 class offset +0x230 (560). Its real class layout is not reconstructed
        // (only its JointData/DriveData slot tables exist), so it is modelled as an
        // asm-sized opaque buffer spanning to mVehicleManager (+0x4AA0 - +0x230 ==
        // 18512 bytes on the X360 4-byte-pointer ABI). The constructor chains its
        // sub-constructor; see the .cpp note (DEFERRED, see below).
        u8 maSimulationModulePlaceholder[0x4AA0 - 0x230]; // +0x230

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
