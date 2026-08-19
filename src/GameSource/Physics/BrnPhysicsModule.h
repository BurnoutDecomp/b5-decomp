#pragma once

// BrnPhysics::PhysicsModule -- the top-level physics module (canonical home
// GameSource/Physics/BrnPhysicsModule.h, per the DecFIGS DWARF). It is a
// CgsModule::ModuleSingleBuffered that owns the whole physics runtime: the
// CgsPhysics simulation module, the vehicle manager, the contact-spy data, the
// deformation manager + its IO interfaces, and the prop manager, plus a large
// block of per-stage performance-marker ids and the prepare/release state
// machines.
//
// ============================================================================================
// ⭐⭐ RE-SEATED 2026-08-03 (task #123). THE OPAQUE TAIL IS GONE.
//
// Until this pass the class carried `u8 maTailPlaceholder[...]` plus a fabricated
// `ContainedListInterface mContainedList` and ended at +0x636A0 (407,200) -- 26,012 bytes
// SHORT of what the X360 Construct @0x825AE308 writes, with `mContainedList` sitting exactly
// on top of the real `mPropManager`. Both defects are fixed here: all five previously-opaque
// sub-objects are now REAL EMBEDDED MEMBERS of their reconstructed types, and the whole
// trailing state / perf-monitor block is modelled member-by-member from the DWARF.
//
// ⚠️ THE OLD PLACEHOLDER WAS ALSO A SIZE PIN, which is why it has to go rather than grow:
//     u8 maTailPlaceholder[0x63630 - (0x4AA0 + sizeof(VehicleManager))]
// self-SHRANK as the host `sizeof` grew, pinning this x64 object to the CONSOLE total 0x636A0
// no matter what its members did. That is a total-size gate acting as a bug preserver. There
// is no absolute size assert anywhere in the tree holding 407,200 in place (checked); the
// array expression was the only thing doing it.
//
// ⭐ WHAT THE CONSOLE ACTUALLY WRITES -- every offset below is an ASM LITERAL, not arithmetic.
// The seven sub-object bases are the `addi`/`addis`+`addi` pairs that form each sub-Construct's
// r3 in Construct @0x825AE308, corroborated by Destruct @0x8259C310 and the ctor @0x827E5400:
//
//     +560     (0x00230)  mSimulationModule    addi  r3,r31,0x230            [ctor, Construct, Destruct]
//     +19104   (0x04AA0)  mVehicleManager      addi  r3,r31,0x4AA0           [ctor, Construct, Destruct]
//     +191728  (0x2ECF0)  mContactData         addis r3,r31,3 ; addi -0x1310
//     +314272  (0x4CBA0)  mDeformationManager  addis r3,r31,5 ; addi -0x3460 [Construct, Destruct]
//     +391024  (0x5F770)  mDeformationInput    addis r3,r31,6 ; addi -0x890
//     +396096  (0x60B40)  mDeformationOutput   addis r3,r31,6 ; addi  0xB40
//     +407088  (0x63630)  mPropManager         addis r3,r31,6 ; addi  0x3630 [ctor, Construct, Destruct]
//     +433072  (0x69BB0)  mePrepareStage .. mbIsOnlineGameMode  (stwx/stbx literals, see below)
//
// The DWARF's declaration order (BrnPhysicsModule.h:189..:241, reproduced verbatim below) is
// EXACTLY this ascending offset order. Two independent sources, no slack.
//
// ⭐ THE TAIL BLOCK, and the closure that proves it. Construct's `stwx`/`stbx` literals:
//     0x69BB0 433072 (4)  mePrepareStage      = 0  (E_PREPARESTAGE_START)
//     0x69BB4 433076 (4)  meReleaseStage      = 7  (E_RELEASESTAGE_DONE)
//             433080 (8)  mWorldRigidBodyId        -- see the 8-byte proof below
//     0x69BC0 433088 (4)  mWorldEntityId      = K_INVALID_ENTITY_ID (dword_82F2A07C)
//     0x69BC4 433092 (4)  meCurrentGameMode   = -1 == E_MODE_NONE (DWARF enumerator)
//     0x69BC8 433096 (4)  miPhysicsPreSceneUpdatePM ... TWENTY-SEVEN int32 ids, 4 bytes apart ...
//             433200 (4)  miPhysicsUpdateFixUpVehContactsPM
//     0x69C34 433204 (4)  miFramesToForceSuperSlowMotion = 0
//     0x69C38 433208 (1)  mbIsOnlineGameMode  = 0   <- stbx: ONE byte, so the object's last
//                                                      data byte is 433208, not 433211.
// The 27 DWARF `int32` PM members fill 433096..433200 with nothing left over, AND each of the
// 21 monitors Construct registers lands on the slot its own name predicts ("    Simulation" ->
// 433100 == miPhysicsUpdateSimulationPM; "    ReadUpdatedBodies" -> 433104; "        Crash
// Prediction" -> 433112; ...). A 21-way name<->offset agreement against a DWARF list this
// project did not author, closed at BOTH ends by asm literals. That is a derivation.
//
// ⚠️ TWO CORRECTIONS TO THE NOTES THIS PASS REPLACED (both were repeated in WorldLinkStubs.cpp):
//   (1) "the twenty-one perfmon handle ids" -- there are TWENTY-SEVEN PM members. 21 is how many
//       Construct passes to AddMonitor; it zero-stores six more (miPropManagerPreScenePM ..
//       miPropManagerApplyShockwavePM) without registering them. Sizing the block at 21 would
//       leave it 24 bytes short.
//   (2) "the real object is at least 433,212 bytes" -- that reads +433208 as a 4-byte store. The
//       asm is `stbx` (1 byte), so the console object is 433,209 raw.
//
// ⭐ mWorldRigidBodyId IS EIGHT BYTES -- proven twice, from two different functions:
//   (a) PhysicsModule::PrepareWorldRigidBody @0x825A9834 zeroes it with `stdx r30, r29, r4`
//       (r4 == 0x69BB8 == 433080) -- a store-DOUBLEWORD. It zeroes mWorldEntityId separately
//       with a 4-byte `stw` at 0x69BC0, which is what fixes both seats.
//   (b) CgsRigidBody.h independently derives CgsPhysics::RigidBodyId as a single `u64` from
//       PropManager::RoutePropVsRaceCarContactToDummyCar @0x825BACB0 (`ld`/`srdi`/`std`).
//   This is the member that makes the block close: 433080 + 8 == 433088 == mWorldEntityId.
//
// X360 byte offsets are the 4-byte-pointer console ABI. Member access here is BY NAME, so the
// bodies are faithful regardless of host pointer width -- the host offsets differ for every
// sub-object (see BrnPhysicsModule_layout_check.cpp, which gates the console arithmetic).
// ============================================================================================

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; supplies the vtable + the two RWMutexes the ctor constructs inline)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h" // CgsPhysics::PhysicsSimulationModule (embedded by value @ +0x230) AND CgsPhysics::RigidBodyId -- see the fork note below
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"       // CgsSceneManager::EntityId + K_INVALID_ENTITY_ID
// ⭐ CgsPhysics::RigidBodyId WAS FORKED IN THIS TREE UNTIL 2026-08-04; IT IS NOT ANY MORE
// (task #141), and this header no longer avoids CgsRigidBody.h. The two homes had been:
//     CgsRigidBody.h:24                 class,  PRIVATE mId, `static const K_INVALID_RIGID_BODY_ID`
//     CgsPhysicsSimulationModule.h      struct, PUBLIC  mId, `extern const K_INVALID_RIGID_BODY_ID`
// The second one is deleted. CgsPhysicsSimulationModule.h now includes CgsRigidBody.h, so the
// declaration that arrives here with the embedded mSimulationModule IS the real class, and
// mWorldRigidBodyId is seated on it. Width, seat and the `stdx` store are unchanged (one u64
// either way) and the layout gate's sizeof == 8 still holds -- ⚠️ which is exactly why nothing
// would ever have caught the copy drifting.
//
// ⭐ THIS HEADER IS THE PROOF, NOT A CLAIM. The note here used to predict that resolving the
// fork "needs its own boot test" because PhysicsModule "is the closest any TU has come to making
// them meet -- it now includes both this header's chain and BrnVehicleManager.h". That prediction
// was testable and was tested: BrnPhysicsModule.cpp is MOUNTED (its .obj rebuilds with this
// header), it still includes both, and it compiles clean against the de-forked chain with the exe
// SIZE unchanged at 2,926,592. The feared C2011 does not occur, because the collision needed two
// definitions and there is now one. ⚠️ Size, not byte-identity -- the pre-change binary was
// overwritten before a hash was taken, so identical codegen is inferred, not measured.
//
// On the `static`-vs-`extern` linkage question this note left open: the console settles it by
// emitting a PER-TU copy of the sentinel -- qword_82F33E18 in the sim-module TU vs qword_82F2A3A8
// in the vehicle-manager TU, exactly like K_INVALID_ENTITY_ID -- which is what CgsRigidBody.h's
// header-scope `static const` produces. The `extern const` in the deleted copy was the invention.
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // BrnGameState::GameStateModuleIO::EGameModeType (E_MODE_NONE == -1)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"   // BrnPhysics::Vehicle::VehicleManager (embedded by value @ +0x4AA0)
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"     // BrnPhysics::ContactSpy::ContactSpyData        (@ +0x2ECF0)
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                 // Deformation::DeformationManager        (@ +0x4CBA0)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"  // Deformation::DeformationInputInterface  (@ +0x5F770)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // Deformation::DeformationOutputInterface (@ +0x60B40)
#include "GameSource/Physics/PropManager/BrnPropManager.h"         // BrnPhysics::Props::PropManager                (@ +0x63630)

#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h" // CgsPhysics::PhysicsSimulationIO::{InputBuffer,OutputBuffer} nested queue typedefs + OutContactSpy (the bridge-slice method signatures; ADDITIVE 2026-08-06)

namespace CgsModule { struct IOBufferStack; }
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Update; struct PotentialContact; } }
namespace BrnResource { namespace GameDataIO { struct AllocatorList; } }
namespace BrnWorld { enum EEntityTypeID : int; }   // fixed-underlying-type opaque decl (home BrnEntityTypes.h)

namespace BrnPhysics
{
namespace PhysicsModuleIO { class InputBuffer; class OutputBuffer; struct PotentialContactInterface; }
namespace Props           { struct PropRaceCarContactBuffer; }   // DWARF BrnPropManager.h:47 (class key struct; IOBuffer-derived, not yet homed)
namespace Vehicle         { struct VehicleManagerOutputBuffer; } // home BrnVehicleManagerIO.h (the "VehManager" stack buffer Update creates)

    // ==============================================================================================
    // The X360 literals this class's layout is derived from. Consumed by
    // BrnPhysicsModule_layout_check.cpp, which is MOUNTED -- so these are checked, not decorative.
    //
    // Two kinds of constant live here and they must not be confused:
    //   * KU_OFF_*            -- ANCHORS. Each is an instruction-immediate in the X360 asm.
    //   * KU_X360_SIZEOF_*    -- each sub-object's own console size, taken from THAT CLASS's own
    //                            header (its own asm-attested member map), never from the gap
    //                            between two anchors here. That is what makes the walk a check:
    //                            both ends are attested by sources that do not know about each other.
    // ==============================================================================================
    namespace PhysicsModuleX360Layout
    {
        // ---- ANCHORS (X360 asm immediates) ----
        const unsigned KU_OFF_SIMULATIONMODULE   = 560u;     // 0x00230  ctor/Construct/Destruct `addi r3,r31,0x230`
        const unsigned KU_OFF_VEHICLEMANAGER     = 19104u;   // 0x04AA0  ctor/Construct/Destruct `addi r3,r31,0x4AA0`
        const unsigned KU_OFF_CONTACTDATA        = 191728u;  // 0x2ECF0  Construct `addis r3,r31,3 ; addi r3,r3,-0x1310`
        const unsigned KU_OFF_DEFORMATIONMANAGER = 314272u;  // 0x4CBA0  Construct/Destruct `addis r3,r31,5 ; addi r3,r3,-0x3460`
        const unsigned KU_OFF_DEFORMATIONINPUT   = 391024u;  // 0x5F770  Construct `addis r3,r31,6 ; addi r3,r3,-0x890`
        const unsigned KU_OFF_DEFORMATIONOUTPUT  = 396096u;  // 0x60B40  Construct `addis r3,r31,6 ; addi r3,r3,0xB40`
        const unsigned KU_OFF_PROPMANAGER        = 407088u;  // 0x63630  ctor/Construct/Destruct `addis r3,r31,6 ; addi r3,r3,0x3630`

        // ---- the tail block (Construct's stwx/stbx immediates; PrepareWorldRigidBody's stdx) ----
        const unsigned KU_OFF_PREPARESTAGE       = 433072u;  // 0x69BB0  stwx 0
        const unsigned KU_OFF_RELEASESTAGE       = 433076u;  // 0x69BB4  stwx 7
        const unsigned KU_OFF_WORLDRIGIDBODYID   = 433080u;  // 0x69BB8  stdx  (EIGHT bytes -- PrepareWorldRigidBody @0x825A9834)
        const unsigned KU_OFF_WORLDENTITYID      = 433088u;  // 0x69BC0  stwx dword_82F2A07C
        const unsigned KU_OFF_CURRENTGAMEMODE    = 433092u;  // 0x69BC4  stwx -1
        const unsigned KU_OFF_FIRSTPERFMON       = 433096u;  // 0x69BC8  miPhysicsPreSceneUpdatePM
        const unsigned KU_OFF_LASTPERFMON        = 433200u;  // 0x69C30  miPhysicsUpdateFixUpVehContactsPM
        const unsigned KU_OFF_FRAMESTOFORCESLOWMO= 433204u;  // 0x69C34  stwx 0
        const unsigned KU_OFF_ISONLINEGAMEMODE   = 433208u;  // 0x69C38  stbx 0  (ONE byte)

        // DWARF BrnPhysicsModule.h:211..:237 -- the perf-monitor id run, counted from the
        // declaration list, NOT from (KU_OFF_LASTPERFMON - KU_OFF_FIRSTPERFMON)/4.
        const unsigned KU_NUM_PERFMON_IDS        = 27u;

        // ---- SUB-OBJECT CONSOLE SIZES (each from that class's OWN header) ----
        // CgsPhysicsSimulationModule.h: mpSimulation @0x4864, +4 -> pad to 0x4870.
        const unsigned KU_X360_SIZEOF_SIMULATIONMODULE   = 18544u;
        // BrnVehicleManager.h: last member muTakedownEventsThisFrame ends at X360 +172616 -> 16-align.
        const unsigned KU_X360_SIZEOF_VEHICLEMANAGER     = 172624u;
        // BrnContactSpyData.h states it outright: "sizeof(ContactSpyData) == 0x1DEB0".
        const unsigned KU_X360_SIZEOF_CONTACTSPYDATA     = 122544u;  // 0x1DEB0
        // BrnDeformationManager.h: the 16 perf-mon ids occupy this+76676..+76736, +4 -> 16-align.
        const unsigned KU_X360_SIZEOF_DEFORMATIONMANAGER = 76752u;
        // BrnDeformationInputInterface.h: mbResetPlayerScratches @ X360 +5056, +1 -> 16-align.
        const unsigned KU_X360_SIZEOF_DEFORMATIONINPUT   = 5072u;
        // BrnDeformationOutputInterface.h: maLocatorData[28] @ +0x2A04 stride 8 -> ends 0x2AE4 -> 16-align.
        const unsigned KU_X360_SIZEOF_DEFORMATIONOUTPUT  = 10992u;
        // BrnPropManager.h: miNumPropsAddedToContactGen @ X360 +0x6570, +4 -> 16-align.
        const unsigned KU_X360_SIZEOF_PROPMANAGER        = 25984u;
    }

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

        // ---- DWARF BrnPhysicsModule.h:207 (body BrnPhysicsModule.cpp:472) ----
        // X360 @0x825A9750 (165 insns). Creates the WORLD's static rigid body -- the body every
        // prop/part/car world contact names as its other half -- and seats the two module ids
        // (mWorldRigidBodyId / mWorldEntityId) it is addressed by. Called only from Prepare's
        // stage 8, with the transient "Simulation" IO buffer that stage allocates.
        //
        // ⚠️ THE PARAMETER IS THE **SIMULATION MODULE'S** INPUT BUFFER, NOT PhysicsModuleIO's.
        // The dwarfdump prints the type unqualified as `InputBuffer *`, exactly as it does for
        // PropPrepareTypes/UpdateNetworkCatchup below, so the name alone does not choose between
        // the two. The asm does, twice over: the body calls
        // `InputBuffer::AppendAddRigidBodyQueue<1>` @0x825A8298 on it (a
        // CgsPhysics::PhysicsSimulationIO::InputBuffer member) and then hands the same pointer to
        // mSimulationModule's ProcessInput slot; and Prepare @0x825ADE28 fills it from
        // `CreateIOBuffer<CgsPhysics::PhysicsSimulationIO::InputBuffer>`. The console's own assert
        // string names it too: "lpSimulationModuleInputBuffer != NULL".
        bool PrepareWorldRigidBody( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimulationModuleInputBuffer );

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
        // Only Construct is bodied so far (BrnPhysicsModule.cpp); the module-level
        // Prepare/Release/Destruct surface is the no-arg base contract, while the argful
        // entry points above are the physics module's own driver methods.
        void Construct() override;

        // Layout gate. Never called; defined in the MOUNTED BrnPhysicsModule_layout_check.cpp
        // so its static_asserts are actually compiled. It needs member-function context to
        // take offsetof on the private members below.
        static void _AssertLayout();

    private:
        // ===================================================================
        // The contact-spy bridge slice (DWARF-private driver methods, home TU
        // BrnPhysicsModuleBridgeFunctions.cpp -- each decl carries its DWARF
        // BrnPhysicsModuleBridgeFunctions.cpp line + X360 address). DE-FACADED
        // 2026-08-06: these were raw free functions over u8* in the TU; they are
        // the real private members now, signatures per the DWARF class dump.
        // ===================================================================

        // :832 @0x825B0448. End-of-frame bridge: resolve the sim's contact spies into
        // mContactData, drain the vehicle manager's discarded contacts, publish through the
        // output buffer's contact-spy interface. Caller: Update @0x825B0640.
        void BridgeSimulationToOutput( PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                                       const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
                                       const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
                                       const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimModuleOutputBuffer,
                                       VecFloat lvfTimeStep );

        // :1049 @0x825B0300. Resolve every raw contact spy, then sort + build the four typed
        // contact run lists.
        void ProcessContactSpies( const CgsPhysics::PhysicsSimulationIO::OutputBuffer::OutContactSpyQueue* lpRawContactSpies,
                                  const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
                                  const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
                                  VecFloat lvfTimeStep );

        // :1094 @0x825AB4D8. Resolve ONE raw contact spy (spy BY VALUE, per the DWARF -- the
        // X360 passes the 112-byte record in r4..r10 + stack).
        void ProcessContactSpy( CgsPhysics::PhysicsSimulationIO::OutContactSpy lContactSpy,
                                const PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface,
                                const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer,
                                VecFloat lvfTimeStep );

        // :1206 @0x825A5DB0. Store one resolved contact into mContactData, dispatched on the
        // raw contact's A-side entity owner.
        void StoreContact( const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpRawContact,
                           const CgsSceneManager::SceneManagerIO::PotentialContact* lpPotentialContact,
                           const Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer );

        // ⭐ ADDED 2026-08-06 (FixUpVehicleContacts wave). @0x825A6010, home TU
        // BrnPhysicsModuleUpdateFunctions.cpp (:915 -- its own baked assert path names the file).
        // The big-five opener: walk the three vehicle-vs-vehicle potential-contact queues and
        // deform-fix each contact -- racecar-vs-traffic ([8], the B id rewritten global->physical
        // through VehicleManager::GetPhysicsEntityIDFromGlobalEntityID), racecar-vs-racecar ([7]),
        // and the scene queue's traffic-vs-traffic pairs ([0], both ids rewritten). Caller:
        // PhysicsModule::Update @0x825B0640 (still a link stub).
        void FixUpVehicleContacts( PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface );

        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). @0x825A99E8, home THIS TU
        // (BrnPhysicsModuleBridgeFunctions.cpp -- the body's own baked asserts span :49..:463).
        // The producer bridge: convert every merged potential contact into an
        // InAddPotentialContact sim event (world/deformation id resolution, prop validation,
        // per-material frictions), then drain the five typed custom queues ([6] vehicle-world,
        // [9] traffic-world, [7] racecar-racecar, [13] traffic-traffic, [8] racecar-traffic)
        // into the deformation sensors, bridge the two simple-traffic passes, and validate the
        // sim queue. Caller: Update @0x825B0640 (still a link stub). Signature per the PS3
        // DecFIGS mangle @0x6999C0.
        void BridgeContactsToSimulation( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                         const PhysicsModuleIO::InputBuffer* lpInputBuffer,
                                         PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
                                         Props::PropRaceCarContactBuffer* lpPropRaceCarContactBuffer );

        // @0x825A5618 (PS3 DecFIGS 0x699594 -- same TU, immediately before the bridge driver).
        // Forward the simple-traffic-with-world potential contacts into the sim add-contact
        // queue. ⚠ FLAG: DECLARED for the bridge driver's closure; body still a TRAP STUB
        // (484 X360 asm lines to reconstruct -- named, not landed, this wave).
        void BridgeSimpleTrafficWithWorldContactsToSimulation(
            CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* lpContactQueue,
            const PhysicsModuleIO::PotentialContactInterface* lpContactInterface );

        // :552 @0x825A1100. Diagnostics: when the sim contact queue is FULL, dump a per-owner
        // histogram and assert. Caller: BridgeContactsToSimulation.
        void CheckContactQueueSize( const CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* lpContactQueue );

        // :604 @0x825A1368. Validate each queued sim contact's entity-type pair, then defer to
        // the vehicle manager's own pass.
        void ValidateSimulationContacts( const CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddContactQueue* lpContactQueue );

        // :649 @0x8259C3F8. The per-pair entity-type legality matrix (13-case switch on type A).
        void ValidateSimulationContactTypes( BrnWorld::EEntityTypeID leEntityTypeA,
                                             BrnWorld::EEntityTypeID leEntityTypeB );

        // :1313 @0x825AB968. Forward the vehicle manager's per-frame joint requests into the
        // sim input buffer (remove-joint only; the add-joint queue must already be empty).
        void BridgeVehicleManagerRequestsToSimulation( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                                       const Vehicle::VehicleOutputRequestInterface* lpRequestInterface );

        // ==========================================================================
        // ⭐ ADDED 2026-08-09 (conductor wave -- PhysicsModule::Update @0x825B0640
        // lands). Signatures per the PS3 DecFIGS mangles; home TU
        // BrnPhysicsModuleBridgeFunctions.cpp for the two bridges (their baked
        // asserts cite :795/:796 and :993/:994 of that file).
        // ==========================================================================

        // :795 @0x825ADEA8 (PS3 0x691CFC). Construct a 60-slot InUpdateExternalBody queue,
        // read the module input's solver-iteration cap into the sim input, harvest every
        // live vehicle body (VehicleManager::GetUpdatedVehicleBodies) and append the queue.
        void BridgeUpdatedVehiclesToSimulation( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                                const PhysicsModuleIO::InputBuffer* lpInputBuffer );

        // :993 @0x825ADF60. Drain the vehicle manager's remove-rigid-body / add-rigid-body /
        // change-inertia request queues into the sim input buffer (that exact console order).
        void BridgeVehicleManagerToSimulation_PostPhysics( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                                           const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer );

        // :1025 (PS3 `PhysicsModule::BridgeVehicleManagerToOutput`; the X360 inlines the whole
        // body into Update @0x825B2408..0x825B2510). Lock both buffers, then forward the
        // request interface with VehicleOutputRequestInterface::Append (five queue appends;
        // the inertia queue is deliberately excluded -- the _PostPhysics bridge owns it).
        void BridgeVehicleManagerToOutput( PhysicsModuleIO::OutputBuffer* lpOutputBuffer,
                                           const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer );

        // @0x825A72F0 (PS3 0x69CD60: HandleGameActions(const BaseGameActionQueue<13312>*,
        // OutputBuffer*)). The per-frame game-action dispatch (mode prepare/stop, impact
        // time, showtime, player stats, ...). The queue type is the committed
        // GameStateModuleIO::GameActionQueue (== the DWARF's BaseGameActionQueue<13312>
        // == VariableEventQueue<13312,16>; see BrnGameStateSharedIO.h's collapse note),
        // which is byte-compatible with the InputBuffer's GameActionQueueStorage member the
        // caller feeds it. ⚠ FLAG: DECLARED for Update's closure; body still a LOUD
        // one-shot gate (BrnPhysicsConductorGates.cpp) -- its 185-insn switch drags ~10
        // VehicleManager mode/showtime methods that are not reconstructed yet.
        void HandleGameActions( const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                PhysicsModuleIO::OutputBuffer* lpOutputBuffer );

        // ---- ADDITIVE 2026-08-10 (create-path wave): the two PostSceneUpdate callees ----
        // @0x825A70C0 (63 insns). The POST-SCENE game-action dispatch -- a second, smaller
        // switch over the same queue than HandleGameActions above: ids 23/34/99 reach
        // VehicleManager::OnPrepareGameMode / OnStartGameMode / OnJunkYardDriveThru, id 97
        // reaches DeformationManager::ProcessResetDeformationModelEvent between two
        // VerifyPartIndices sweeps, and the whole loop is preceded by
        // DeformationManager::ProcessDebugResetDeformationModels. ⚠ FLAG: DECLARED for
        // PostSceneUpdate's closure; body is a LOUD one-shot gate (BrnPhysicsConductorGates.cpp)
        // -- every one of its four arms is itself unreconstructed.
        void HandleGameActionsPostScene( const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                         CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                         CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface );

        // ⭐⭐ @0x825AB408 (52 insns). THE SIM FIREWALL -- BODIED 2026-08-11 (prepare-chain wave)
        // in BrnPhysicsModuleBridgeFunctions.cpp; its conductor gate is deleted.
        // This is the ONLY road from the vehicle manager's rigid-body request queues into the
        // simulation input buffer -- four appends, in the console's order:
        //     AppendRemoveRigidBodyQueue<50>(simIn, vehOut->GetRemoveRigidBodyQueue()     /* +9616  */)
        //     AppendAddRigidBodyQueue   <50>(simIn, vehOut->GetRequiredRigidBodiesQueue() /* +0     */)
        //     AppendAddJointQueue       <10>(simIn, vehOut->GetAddJointQueue()            /* +39904 */)
        //     AppendRemoveJointQueue    <10>(simIn, vehOut->GetRemoveJointQueue()         /* +41840 */)
        // (offsets cross-checked against BrnVehicleOutputInterface.h's own queue seats).
        // VehicleManager::ProcessCreateEvents @0x82616770 posts its InAddRigidBody event into
        // mRequiredRigidBodiesQueue, which lives in the "VehManager" stack IO buffer that
        // PostSceneUpdate creates AND destroys in the same call -- so this function is what makes
        // that event reach the simulation at all.
        // ⛔ THE OLD HOLD ("do not land until the traction-line chain is closed") IS RETIRED: both
        // halves of that lifetime -- StartVehicleTractionLineTests @0x82629CE0 and
        // EndVehicleTractionLineTests @0x82633CD8 -- are real bodies in
        // BrnVehicleManager_TractionLineTests.cpp as of the 2026-08-11 lifetime wave.
        void BridgeVehicleManagerToSimulation_PostScene( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                                         const Vehicle::VehicleManagerOutputBuffer* lpVehManagerOutputBuffer );

        // ===================================================================
        // DWARF member set (BrnPhysicsModule.h:189..241), in declaration order --
        // which is also ascending X360 offset order. Every member is now a REAL
        // typed member; there are no opaque spans left in this class.
        // ===================================================================

        // mSimulationModule -- CgsPhysics::PhysicsSimulationModule, X360 +0x230 (560).
        // Its own constructor @0x827DF1E0 is chained here by implicit member construction,
        // which is what seeds the 200 rw::physics::Inertia records and the 36 JointLimits.
        CgsPhysics::PhysicsSimulationModule       mSimulationModule;    // :189  X360 +560

        // mVehicleManager -- X360 +0x4AA0 (19104).
        BrnPhysics::Vehicle::VehicleManager       mVehicleManager;      // :190  X360 +19104

        // mContactData -- X360 +0x2ECF0 (191728). ContactSpyData is the one sub-object whose
        // host sizeof equals its console sizeof exactly (122,544): it is queues and run-lists
        // of PODs with no pointer members anywhere.
        ContactSpy::ContactSpyData                mContactData;         // :192  X360 +191728

        // mDeformationManager -- X360 +0x4CBA0 (314272).
        Deformation::DeformationManager           mDeformationManager;  // :194  X360 +314272

        // mDeformationInput -- X360 +0x5F770 (391024).
        Deformation::DeformationInputInterface    mDeformationInput;    // :195  X360 +391024

        // mDeformationOutput -- X360 +0x60B40 (396096).
        Deformation::DeformationOutputInterface   mDeformationOutput;   // :196  X360 +396096

        // mPropManager -- X360 +0x63630 (407088).
        // ⭐ THIS SEAT IS WHAT THE OLD `ContainedListInterface mContainedList` WAS SITTING ON.
        // The ctor @0x827E5400's trailing stores are NOT a separate contained interface: the
        // vtable stamp at +0x63630 is mPropManager.mDebugComponent's vptr (PropDebugComponent
        // derives from CgsDev::DebugComponent, so the compiler emits it), and the "intrusive
        // list" at +0x63684 is mPropManager.mpPhysicsData -- a CgsResource::ResourcePtr, whose
        // BaseResourcePtr() @0x82204E20 is documented in CgsBaseResourcePtr.cpp as exactly
        // "stw 0 ->+0,+4,+8 (then a redundant 0 ->+0), stw this ->+0xC,+0x10,+0x14, stw 0 ->+0x18"
        // -- instruction for instruction what this class's ctor inlines at +0x63684. Declaring
        // the real member therefore REPRODUCES both stores through the real constructors, and
        // the hand-rolled versions are deleted from the ctor rather than dropped.
        Props::PropManager                        mPropManager;         // :198  X360 +407088

        // ---- the trailing state / perf-monitor block, X360 +433072 .. +433208 ----
        // ⭐ THIS BLOCK IS WIDTH-INVARIANT between the console and the host: two enums, a u64
        // handle, a 4-byte EntityId, an enum, 28 int32s and a bool -- no pointer, no vptr.
        // That is why BrnPhysicsModule_layout_check.cpp is allowed to gate it with RELATIVE
        // host offsetof against the console deltas, and why that gate is not vacuous.
        //
        // ⚠️ mePrepareStage / meReleaseStage SHADOW two same-named PRIVATE members of the
        // ModuleSingleBuffered base (its EManagerPrepareStage / EManagerReleaseStage pair).
        // That is what the console does too -- they live at different offsets and hold
        // different enums. The base's are private, so there is no ambiguity here.
        EPrepareStage                             mePrepareStage;       // :200  X360 +433072
        EReleaseStage                             meReleaseStage;       // :201  X360 +433076

        CgsPhysics::RigidBodyId                   mWorldRigidBodyId;    // :204  X360 +433080 (8 bytes)
        CgsSceneManager::EntityId                 mWorldEntityId;       // :205  X360 +433088

        BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode; // :207  X360 +433092

        // The 27 CPU perf-monitor handles, DWARF :211..:237, X360 +433096 .. +433200.
        // Construct registers 21 of them via PerfMonCpu::AddMonitor and zero-stores the six
        // miPropManager*PM ones it does not register.
        s32 miPhysicsPreSceneUpdatePM;                    // :211  X360 +433096
        s32 miPhysicsUpdateSimulationPM;                  // :212  X360 +433100
        s32 miPhysicsUpdateReadUpdatedBodiesPM;           // :213  X360 +433104
        s32 miPhysicsUpdateVehiclePhysicsPM;              // :214  X360 +433108
        s32 miPhysicsUpdateCrashPredictionPM;             // :215  X360 +433112
        s32 miDeformationMaintenancePM;                   // :216  X360 +433116
        s32 miContactSpyListGenerationPM;                 // :217  X360 +433120
        s32 miGenerateSceneQueriesPM;                     // :218  X360 +433124
        s32 miPhysicsProcessRaceCarContactsPM;            // :219  X360 +433128
        s32 miDeformationManagerPM;                       // :220  X360 +433132
        s32 miPhysicsBridgeContactsPM;                    // :221  X360 +433136
        s32 miPhysicsUpdateContactGenAsyncPM;             // :222  X360 +433140
        s32 miPhysicsUpdateDoVehicleContactGenStartPM;    // :223  X360 +433144
        s32 miPhysicsUpdateDoVehicleContactGenEndPM;      // :224  X360 +433148
        s32 miPhysicsUpdateDoPartContactGenStartPM;       // :225  X360 +433152
        s32 miPhysicsUpdateDoPartContactGenEndPM;         // :226  X360 +433156
        s32 miPhysicsUpdateDoPropContactGenStartPM;       // :227  X360 +433160
        s32 miPhysicsUpdateDoPropContactGenEndPM;         // :228  X360 +433164
        s32 miPhysicsUpdateValidateRaceCarWorldContactPM; // :229  X360 +433168
        s32 miPropManagerPM;                              // :230  X360 +433172
        s32 miPropManagerPreScenePM;                      // :231  X360 +433176
        s32 miPropManagerWorldContactGenPM;               // :232  X360 +433180
        s32 miPropManagerProcessInputsPM;                 // :233  X360 +433184
        s32 miPropManagerReadUpdatedBodiesPM;             // :234  X360 +433188
        s32 miPropManagerOutputUpdatedPropsPM;            // :235  X360 +433192
        s32 miPropManagerApplyShockwavePM;                // :236  X360 +433196
        s32 miPhysicsUpdateFixUpVehContactsPM;            // :237  X360 +433200

        s32  miFramesToForceSuperSlowMotion;              // :240  X360 +433204
        bool mbIsOnlineGameMode;                          // :241  X360 +433208 (stbx: one byte)
    };
}
