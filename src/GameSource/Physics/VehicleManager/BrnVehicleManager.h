#pragma once

// BrnPhysics::Vehicle::VehicleManager -- the per-frame vehicle physics manager. It owns the
// race-car rigid bodies and runs the impact/takedown classification when two cars contact.
//
// MINIMAL-SLICE HOME (now extended for the takedown sub-classifiers). The real VehicleManager is
// enormous (64 functions, a ~172 KB class with several parallel per-car arrays). This header
// provides what the takedown CLASSIFIER chain needs to compile: the nested RaceCarResponseInfo
// working-set struct (the per-contact data the classifiers read), the classifier method
// declarations, and the deep VehicleManager data members the classifiers + InstantTakedown +
// SetRaceCarCrashing reach. Everything not modelled is opaque padding so each named member lands
// at its asm-proven byte offset (pinned by the offsetof asserts in _AssertLayout). The
// CheckForAllTypesOfImpacts entry point reads none of the deep members (only its argument), so the
// layout growth does not disturb that body.
//
// RaceCarResponseInfo layout is verbatim from the DecFIGS DWARF (BrnVehicleManager.h:763-802);
// the speed/crashing offsets it exposes (+0x5C/+0x60 speeds, +0x50/+0x51 crash flags) are the
// ones the CheckForAllTypesOfImpacts X360 asm reads (a2+92/+96/+80/+81).

#include "types.hpp"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include <cstddef>                                                // offsetof (layout asserts)
#include "BrnCommonTypes.h"                                       // Vector3, VecFloat, EntityId, Matrix44Affine
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h" // EImpactType, EImpactSituation
#include "GameSource/GameState/BrnTakedownType.h"                 // BrnGameState::ETakedownType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::ERaceCarType (maeRaceCarTypes)
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"  // RaceCarContact (mNormal @+48, mPointOnA @+64)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact (maNonPhysicalContacts[128], promoted 2026-08-06)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<1536,16> (the IO event queue the crash/takedown events push onto)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<DiscardedContact,20> (mDiscardedContacts @+160672)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<N> (live-car bitset, crash-data free-list, taken-down bitset)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"      // CgsSceneManager::EntityId + K_INVALID_ENTITY_ID (GetPhysicsEntityIDFromGlobalEntityID)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"             // CgsNumeric::Random (mRandom @+16)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"          // CgsSystem::Time (mCurrentTime/mStartModeTime @+172364 -- carved 2026-08-06)
#include "SharedClasses/BrnSharedConstants.h"                     // BrnUpdateSet (UpdateVehiclePhysics arg)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h" // VehicleDriver (maRaceCarDrivers @+64, mPlayerAiDriver @+171968)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"  // BrnPhysics::Vehicle::RaceCarPhysics (maRaceCarVehicles @+1856 -- the REAL type as of 2026-08-03; that header does not include this one, so this is not a cycle)
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.h" // BrnPhysics::StuntOffencesManager (mStuntOffencesManager @+44240 -- the ONE contained sub-object whose real x64 type fits its X360 span exactly)
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h" // BrnPhysics::Vehicle::PhysicalTrafficManager (mPhysicalTrafficManager @+44768 -- embedded BY NAME as of 2026-08-03; see the drift note below)
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h" // BrnPhysics::Vehicle::VehicleManagerDebugComponent (mDebugComponent @+161968 -- embedded BY NAME as of 2026-08-03; that header only forward-declares VehicleManager, so this is not a cycle)

// Pointer-only collaborators in RaceCarResponseInfo -- forward-declared in their real namespaces
// (homed by their own TUs; the classifier never dereferences them here).
namespace BrnPhysics { namespace PhysicsModuleIO { class VehicleOutputRequestInterface; } }
namespace BrnPhysics { namespace Deformation { class DeformationInputInterface; } }
namespace BrnGameState { namespace GameStateModuleIO { class VehicleOutputInterface; } }

// Crash-prediction (race-car-vs-world) collaborators -- forward-declared in their real
// namespaces (homed by their own TUs; HandleCrashPredictionForRaceCarAndWorld only takes/forwards
// pointers + one by-value contact, so the declarations need no complete type here).
namespace BrnPhysics { namespace PhysicsModuleIO { struct PotentialContactInterface; } }
namespace BrnPhysics { namespace Vehicle { struct VehicleInputInterface; } }
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact; struct TriangleCacheInterface;
                                                       struct OutEventLineTestNearestResult;  // line-test result queue element (UpdateVehiclePhysics wave)
                                                      struct InputBuffer_Update; } }
// Class key `struct`, matching rw/rwcore_structs.h -- a `class` here mangles differently.
namespace rw { struct IResourceAllocator; }

// ValidateSimulationContacts' queue element (declaration only needs the template + element name;
// class key `struct`, matching CgsPhysicsSimulationIO_Events.h).
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InAddPotentialContact; } }

// ⭐ ADDED 2026-08-06 (PhysicsModule::Update leaves wave) -- the collaborators of the four
// per-frame leaves (FreeAllocations / UpdateVehicleEffects / ReadUpdatedBodyProperties /
// ProcessDeformationStates). Class keys match each type's committed home exactly.
namespace CgsModule { struct IOBufferStack; }                              // CgsIOBufferStack.h
// ⭐ ADDED 2026-08-06 (big-five #2): StartVehicleContactGeneration collaborators, pointer/
// template-arg use only in this header.
namespace CgsMemory { class LinearMalloc; }
namespace CgsMemory { struct SimpleDataStreamProducer; }   // stream-producer members (pointers)
namespace EA { namespace Jobs { struct Job; } }            // job members (pointers)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder (five BY-VALUE members)
namespace CgsSceneManager { namespace SceneManagerIO { struct OutOverlapPair; } }
namespace BrnPhysics { namespace Deformation { struct DeformationManager; } }   // class key `struct`, matching BrnDeformationManager.h:237
namespace BrnPhysics { struct ContactGenList; }                            // BrnContactGenerationList.h
// The REAL namespace, pinned by the X360 mangling of FreeAllocations' DestroyIOBuffer bl target
// (VCollisionGenerator@CgsCollision@CgsSceneManager@@) -- see the member carve note below.
namespace CgsSceneManager { namespace CgsCollision { struct CollisionGenerator; } }
namespace BrnPhysics { namespace Deformation { class DeformationOutputInterface; } } // BrnDeformationOutputInterface.h
// ReadUpdatedBodyProperties' queue element (CgsPhysicsSimulationIO_Events.h, class key `struct`).
namespace CgsPhysics { namespace PhysicsSimulationIO { struct InChangeRigidBodyInertia; } }

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics;                 // pointer-only collaborator
    struct VehicleEffectsInputInterface;  // UpdateVehicleEffects arg (SharedIO/BrnVehicleEffectsInputInterface.h)

    // MINIMAL-SLICE definition of the manager-side output interface the crash/takedown events fan
    // out through. The real home is BrnVehicleConstants.h (DWARF); only the surface SetRaceCarCrashing
    // + HandleRaceCarRaceCarContact poke is modelled here, declare-only. FLAG: the EXACT event-queue
    // plumbing is MODELLED -- the X360 reaches a CgsModule::VariableEventQueue<1536,16> at sink+26096,
    // a secondary "remapped id" sub-queue at sink+1872, and two driver-feedback bytes at sink+27648/9
    // by raw offset. Here those are exposed by NAME (accessors / declare-only methods) rather than by
    // reproducing the full ~27KB byte layout; the method bodies belong to the interface's own TU.
    // RECONCILE 2026-07-24 (WorldModule fleet embed): the canonical
    // VehicleManagerOutputInterface home is SharedIO/BrnVehicleOutputInterface.h
    // (DWARF :82, full member layout). The declare-only shell that lived here was
    // retired; its method surface moved to the canonical struct additively.


    // ==========================================================================================
    // ⭐⭐ KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER -- the ONE place this class stops being byte-pinned.
    //
    // VehicleManager embeds PhysicalTrafficManager by value at +44768. That class is 105648 bytes
    // on the X360 (derived; BrnPhysicalTrafficManager.h finding (4)) and 105840 on the host, so
    // every member after it sits this many bytes later here than it did there. The divergence is
    // real host/console width, not a reconstruction error, and it is accounted for member by member
    // in BrnPhysicalTrafficManager.h finding (4): ResourceHandle 16 vs 8 over a 20-element array
    // (+160), four pointers 4 -> 8 (+16), EventQueue<s8,50> 72 vs 64 (+8), the debug component 48 vs
    // 32 (+16), and two alignment give-and-takes worth -8 between them.
    //
    // WHY IT IS A NAMED CONSTANT AND NOT A HIDDEN PAD ADJUSTMENT: the project rule is x64 parity by
    // NAMED MEMBERS, not byte offsets. Absorbing the 192 into a neighbouring padding run would keep
    // the pretty absolute numbers and quietly make one modelled DWARF member the wrong size --
    // exactly the "inventing layout to buy a green build" trap. Carrying it explicitly keeps every
    // downstream offsetof assert LIVE (they read `<X360 offset> + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER`,
    // so a wrong padding run still fails the build) and keeps the X360 offset visible in the source.
    //
    // ⚠️ It is a literal, deliberately, NOT `sizeof(PhysicalTrafficManager) - KU_X360_SIZEOF_...`.
    // Defining it from sizeof would make the asserts self-fulfilling; as a literal it is a tripwire
    // in both directions, and BrnVehicleManager_layout_check.cpp ties the three numbers together.
    //
    // There are now THREE drift terms, because there are three embedded sub-objects whose host
    // width differs from the console's. They apply to disjoint address ranges and they ACCUMULATE:
    //     X360 +0        .. +43584    ->  0               (nothing before the race-car array moves)
    //     X360 +43584    .. +44768    ->  -1664           KU_HOST_DRIFT_AFTER_RACECAR_ARRAY
    //     X360 +44768    .. +163264   ->  -5632           KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER
    //     X360 +163264   .. end       ->  -5600           KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT
    //
    // ⚠️ THE TERMS ARE SIGNED AND THE FIRST IS NEGATIVE. The old shape of this block predicted
    // exactly that ("a third one will appear the day RaceCarVehicleRecord becomes a real
    // RaceCarPhysics -- and that one will be NEGATIVE"), which is why they were never folded into
    // one number. That day is 2026-08-03. The old note's arithmetic is stale, though: it said the
    // host class was 4816. It is **5008** -- the VehiclePhysics/SimpleVehiclePhysics own-member
    // blocks grew it between the two waves -- so the term is -1664, not -3200. MEASURED with the
    // compiler, not carried forward from the note.
    // ==========================================================================================

    // ⭐⭐ THE THIRD TERM, added 2026-08-03 (the record-fold wave). `maRaceCarVehicles` is now
    // `RaceCarPhysics[8]`, the real class, not the byte-pinned 5216-byte `RaceCarVehicleRecord`
    // stand-in it carried for ten waves. sizeof(RaceCarPhysics) is 5008 on the host against the
    // 5216 the X360 bakes as a literal stride (`mulli r11, r22, 0x1460` in SetRaceCarCrashing), so
    // the array is 8 * 208 == 1664 bytes shorter and everything after it moves DOWN.
    //
    // WHY THE FOLD HAD TO HAPPEN, and why it is not just tidying: VehicleManager::Construct calls
    // a constructor on each element. On a byte-pinned stand-in that call can only be a
    // reinterpret_cast, which writes the real class's members at HOST offsets into storage whose
    // readers use CONSOLE offsets -- the silent-corruption trade this header has warned about since
    // the blocker table was written. ⛔ AND THE TREE WAS ALREADY MAKING THAT TRADE: six live
    // `reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[i])` sites existed in
    // BrnVehicleManager.cpp / BrnVehicleManagerPlayerStats.cpp before this wave, latent only
    // because neither TU is mounted. The fold retires all six.
    //
    // WHAT IT COSTS, stated rather than hidden: the record's ten IN-RECORD offsetof asserts are
    // gone, because a host class does not reproduce console offsets. They are not lost, they moved:
    // RaceCarPhysics_layout_check.cpp and VehiclePhysics_layout_check.cpp assert the same seats as
    // CONSOLE ARITHMETIC over the X360Layout literals, and their chain closes on this very 5216.
    // That is the same trade VehiclePhysics_layout_check.cpp already argued for its own block.
    //
    // ⭐⭐ RE-DERIVED 2026-08-09 (attribs-setup wave): the term is now **0**. The whole -1664 was
    // 8 * -208, and the -208 per element was EXACTLY the SimpleVehicleAttribs gap (the tree's
    // 32-byte {mCOMOffset, mbIsValid} slice against the console's 240). That slice is now the
    // full width-identical 240 (BrnSimpleVehiclePhysics.h), so sizeof(RaceCarPhysics) is 5216 on
    // the host == the X360's literal stride (`mulli r11, r22, 0x1460`), and the array term
    // vanishes. MEASURED with the compiler via the gate below, not carried from this note.
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_RACECAR_ARRAY = 0;

    // ⭐⭐ RE-MEASURED 2026-08-03 (the TrafficPhysics de-fork wave): the second term is now **-3968**,
    // not +192. `maFullTrafficPhysics` inside PhysicalTrafficManager was folded from a byte-pinned
    // `u8[5168]` stand-in to the real `TrafficPhysics[20]`, for exactly the reason maRaceCarVehicles
    // was folded one level up (BrnPhysicalTrafficManager.h's de-fork note): the console's
    // per-element Construct call cannot be spelled against a stand-in without writing host-offset
    // members into console-strided storage. sizeof(TrafficPhysics) is 4960 on the host against the
    // X360's 5168, so the array is 20 * 208 == 4160 shorter and the traffic manager goes
    // 105840 -> 101680 while the X360 stays at its derived 105648:
    //     101680 - 105648 == -3968     (and -3968 % 16 == 0, so every 16-aligned member behind it
    //                                   keeps its alignment)
    // MEASURED with the compiler (`char (*p)[sizeof(T)] = 1;`), not carried forward from a note.
    //
    // ⭐⭐ RE-DERIVED 2026-08-09 (attribs-setup wave): the step is back to **+192**, the value it
    // had before the TrafficPhysics de-fork -- because the de-fork's -4160 was 20 * -208, and the
    // -208 per element was the SimpleVehicleAttribs slice gap, now closed (see the race-car term
    // above). sizeof(TrafficPhysics) is 5168 on the host == the console's `mulli 0x1430` stride,
    // so the only remaining widening in the traffic manager is its own +192 (pointer members +
    // the 48-vs-32 debug component span).
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER =
        KU_HOST_DRIFT_AFTER_RACECAR_ARRAY + 192;   // +192

    // ⭐ The third term, added 2026-08-03 in an earlier wave. VehicleManagerDebugComponent is 1328
    // bytes on the host against the 1296-byte X360 span at +161968..+163264 -- +32, because its
    // base's vptr and its mpVehicleManager both widen 4 -> 8. Unlike the traffic manager's span,
    // this 1296 was never a guess: both ends are asm-literal (Construct(this + 161968, this), then
    // the stride-1024 walk from +163264), so the +32 is real and the only honest answer is to carry
    // it. 32 % 16 == 0, so every 16-aligned member past it keeps its alignment.
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT =
        KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER + 32;  // +224 (re-derived 2026-08-09)

    // ⭐ The fourth term, added 2026-08-06 (big-five #2): the contact-generation block carve
    // (+172465..+172592 console -> real members). Growth = the pointer-pair carve's +12 (4
    // alignment + 2x pointer widening, already asserted at the head), five builders 12 -> 16
    // (+20), nine more pointers 4 -> 8 (+36), one alignment pad before the producer run (+4),
    // and one before the traction-line pointer (+4): +12+20+36+4+4 == +76. MEASURED against the
    // compiled layout by the gate's own seat asserts (BrnVehicleManager_layout_check.cpp).
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK =
        KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT + 76;  // +300 (re-derived 2026-08-09)

    class VehicleManager
    {
    public:
        // ⭐ The eight-car array bound, asm-literal in VehicleManager::Construct
        // (`li r23, 8` then `addi r23, r23, -1` per iteration) and the width of every one of this
        // class's per-car arrays. The console spells it BrnWorld::KI_MAX_ACTIVE_RACE_CARS -- the
        // disconnect asserts name it -- but that constant has no committed home in this tree, so it
        // is declared at the class that owns the arrays. DELETE-WHEN BrnWorld homes it.
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

        // ⭐⭐⭐ Construct @0x8263B7C8, 943 instructions -- BODIED 2026-08-03 in
        // BrnVehicleManager.cpp. Its only caller is BrnPhysics::PhysicsModule::Construct
        // @0x825AE308, which is still a link stub (WorldLinkStubs.cpp). See the big ⭐ recipe block
        // further down in this header for the full instruction-level shape and every default the
        // tuning bank seeds; the body is written straight off it.
        void Construct();

        // ADDITIVE (WorldModule::Prepare @0x827D53B0 stage-8 success path reads the
        // surface-property table once the world entity module prepared). Static on the
        // X360 (a global manager pair). Declaration-only; body with this manager's TU.
        // ⚠️⚠️ SIGNATURE FORK, FLAGGED 2026-08-06 (UpdateVehiclePhysics wave): the DWARF
        // (BrnVehicleManager.h:1085) declares ONE ReadSurfaceProperties -- the NON-static
        // one-argument member below -- and the X360 caller in UpdateVehiclePhysics
        // @0x8264513C passes this + the 64-bit key. This static no-arg form exists only in
        // this tree (WorldLinkStubs.cpp's simplified stub + BrnWorldModule.cpp's call).
        // The WorldModule wave owns re-pointing that call to the real member; do not add
        // more callers to this overload.
        static void ReadSurfaceProperties();

        // ==================================================================================
        // ⭐⭐ ADDED 2026-08-06 (big-five #3, UpdateVehiclePhysics wave). The per-frame FORCE
        // PRODUCER and its sibling surface. DWARF lines cited per member; bodies either in
        // BrnVehicleManager_UpdateVehiclePhysics.cpp (slice TU) or named FLAG trap stubs in
        // BrnVehicleManagerLinkStubs.cpp (dead until PhysicsModule::Update lands).
        // ==================================================================================

        // DWARF h:896; X360 @0x82644FA8 (1,038 insns). The manager-level per-frame conductor.
        // BODIED in BrnVehicleManager_UpdateVehiclePhysics.cpp.
        void UpdateVehiclePhysics(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            BrnUpdateSet lUpdateSet,
            CgsSystem::Time& lrCurrentTime,
            f32 lfSimTimerTimeStep,
            f32 lfGameTimerTimeStep,
            const VehicleInputInterface* lpInputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            bool lbIsOnlineGameMode,
            CgsSceneManager::EntityId lWorldEntityId);

        // DWARF h:947; X360 @0x825B5690. BODIED in the slice TU.
        bool IsRaceCarCrashing(s32 liRaceCarIndex);

        // DWARF h:1239 -- the FIVE-argument overload; X360 @0x82635B78 (the unnamed sub the
        // export set skipped; identity proof in the slice TU banner). BODIED in the slice TU.
        // (The 6-arg + EntityId overload @0x82635B00, DWARF h:1242, is CrashFatalRaceCars'
        // callee and lands with that body.)
        void ForceRaceCarCrash(
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            EActiveRaceCarIndex leRaceCarIndex);

        // DWARF h:866; X360 @0x826183F8. BODIED in the slice TU. (Raw EventQueue
        // instantiation spelling == VehicleInputInterface::InLineTestResultQueue; the
        // typedef does not change the type -- same precedent as ValidateSimulationContacts.)
        void ProcessAboveGroundLineTestsResults(
            const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult, 2000>* lpLineTestResults);

        // DWARF h:1113 (second param spelled BrnGameState::GameStateModuleIO::GameEventQueue*
        // there == this VariableEventQueue instantiation; the GameState header keeps the
        // class incomplete, so the instantiation is named directly). X360 @0x82633DE8.
        // BODIED in the slice TU.
        void ProcessAftertouchEvents(s32 liRaceCarIndex,
                                     CgsModule::VariableEventQueue<1536, 16>* lpOutputQueue);

        // ---- named FLAG trap stubs (BrnVehicleManagerLinkStubs.cpp) until their bodies land:
        // DWARF h:953; X360 @0x82635C00 (322 insns).
        void UpdateVehicleImpacts(
            const CgsModule::EventQueue<ImpactEvent, 16>* lpImpactEventQueue,   // == VehicleInputInterface::ImpactEventQueue
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // DWARF h:1236; X360 @0x82640690 (264 insns).
        void UpdateAggressiveDriving(
            f32 lfTimeStep,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // DWARF h:1224; X360 address not in this TU's dossier (the ledger mis-keys it to the
        // CgsBitArray.h TU -- re-derive at its own wave).
        void UpdateCrashes(f32 lfTimeStep);

        // DWARF h:893; X360 body is an export-set hole (absent-from-JSON, not absent-from-image).
        void EndVehicleTractionLineTests(CgsModule::IOBufferStack* lpInputBufferStack,
                                         const VehicleInputInterface* lpInputInterface);

        // DWARF h:1287; X360 body is an export-set hole (image-only).
        void CrashFatalRaceCars(
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            CgsSceneManager::EntityId lWorldEntityId);

        // DWARF h:1085 `void ReadSurfaceProperties(Attribute::Key)`; X360 @0x825C7BB8
        // (187 insns, AttribSys walk). ⚠️ WIDTH: the caller loads the key with a 64-bit `ld`
        // (qword_82FB7F10) while the committed Attribute::Key typedef is u32 -- the
        // parameter is spelled u64 off the asm; the typedef conflict is FLAGGED here, not
        // resolved (attribhash64 keys are 64-bit; the u32 typedef has its own note).
        void ReadSurfaceProperties(u64 luSurfaceListKey);

        // ADDITIVE 2026-08-04 (task #135) -- X360 @0x82633568, the stage-6 arm of
        // BrnPhysics::PhysicsModule::Prepare @0x825ADB68 (`bl` at 0x825ADDFC, result tested
        // with a `bne` so the return is a bool). Arguments are the physics resource allocator
        // (bank 23) and the scene input buffer PhysicsModule::Prepare was handed.
        // Declaration-only; the body is a named LINK STUB in WorldLinkStubs.cpp until this
        // manager's own prepare pass lands. It is declared here rather than left implicit so
        // the drop is one greppable symbol instead of a silent `return true` for the whole
        // physics module.
        bool Prepare( rw::IResourceAllocator* lpAllocator,
                      CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer );

        // ⭐ ADDED 2026-08-06 (bridge de-facade wave). DWARF BrnVehicleManager.h:667; X360
        // @0x825C8990 (home BrnVehicleManager.cpp:9877 per its own baked assert path). Debug
        // validation pass over the outgoing simulation contact queue: for each contact whose
        // A/B entity owner is TRAFFIC_VEHICLE, assert the packed 14-bit entity index is < 20
        // and that mPhysicalTrafficManager.mUsedTrafficVehicles has that traffic slot live.
        // Bodied in BrnVehicleManager_ValidateSimulationContacts.cpp (slice TU -- the home TU
        // is still unmounted). The queue element is InAddPotentialContact (stride 80); the
        // spelling below is the same EventQueue instantiation InputBuffer::InAddContactQueue
        // names (typedefs do not change the mangling).
        void ValidateSimulationContacts(
            const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* lpContactQueue );

        // ⭐ ADDED 2026-08-06. DWARF BrnVehicleManager.h:562; header-inline on the console (no
        // out-of-line emission -- PhysicsModule::BridgeSimulationToOutput @0x825B0574 reaches
        // the member as a bare `this + 0x2BE40` addi, i.e. the inlined accessor). The bridge
        // drains this queue into the module's ContactSpyData every frame.
        const CgsModule::EventQueue<BrnPhysics::ContactSpy::DiscardedContact, 20>*
        GetDiscardedContacts() const { return &mDiscardedContacts; }

        // ==========================================================================================
        // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). The contact-generation /
        // bridge surface of PhysicsModule::Update. DWARF-authoritative signatures (:941 / :1052 /
        // :1073).
        // ==========================================================================================

        // @0x825EAC28 (DWARF h:941; PS3 DecFIGS 0x6E6178). Thin forwarder: null tripwires
        // (BrnVehicleManager.cpp:7727/:7728) + the A-owner==TRAFFIC tripwire (:7730), then
        // mPhysicalTrafficManager.ValidateTrafficContact. Bodied in
        // BrnVehicleManager_PerFrameLeaves.cpp (home BrnVehicleManager.cpp still unmounted).
        bool ValidateTrafficContact( CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
                                     const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
                                     f32 lfTimeStep );

        // @0x825C83B0 (DWARF h:1073). Forward the simple-traffic-with-car potential contacts into
        // the sim add-contact queue. ⚠ FLAG: DECLARED for BridgeContactsToSimulation's closure;
        // body still a TRAP STUB (375 X360 asm lines -- named, not landed, this wave).
        void BridgeSimpleTrafficWithCarContactsToSimulation(
            CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact, 1024>* lpContactQueue,
            const BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface );

        // @0x8262AEE8 (DWARF h:1052; PS3 DecFIGS 0x78A754), home TU
        // BrnVehicleManagerContactGeneration.cpp (the body's own baked assert path). Start the
        // per-frame vehicle contact generation: prepare the five primitive-pair builders, create
        // the ContactGenList / CollisionGenerator IO buffers + the three collide-stream
        // producers, walk the scene's overlap pairs (car-car pairs -> DoCarCarContactGeneration +
        // hinged-part pairs; part-vs-car / wheel-vs-car pairs -> the deformation pair builders),
        // run the per-car and per-traffic world contact generation, then kick the three collide
        // stream jobs. Caller: PhysicsModule::Update @0x825B0640 (still a link stub).
        void StartVehicleContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* lpOverlapPairs,
            f32 lfTimeStep,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
            CgsModule::IOBufferStack* lpIOBufferStack,
            CgsMemory::LinearMalloc* lpLinearMalloc,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface );

        // ⭐ ADDED 2026-08-06 (big-five #2). PhysicsModule::BridgeContactsToSimulation calls the
        // embedded traffic manager's ValidateAndFixUpTrafficTrafficContact with the embedded
        // object's address as `this` (module+63872 == this+44768) -- access a private member
        // cannot express from PhysicsModule. FLAG: the accessor NAME is inferred (no DWARF
        // accessor is dumped); the console evidence is the direct embedded-object call.
        PhysicalTrafficManager& GetPhysicalTrafficManager() { return mPhysicalTrafficManager; }

        // ⭐ ADDED 2026-08-06 (big-five #2). Header-inline reset of the non-physical contact
        // count -- BridgeContactsToSimulation @0x825A9A50 opens with a bare `stw 0` at
        // module+179760 == this+160656 == miNonPhysicalContactCount (the int32 the DWARF seats
        // directly after maNonPhysicalContacts[128], :851). Spelled BY NAME here.
        void ResetNonPhysicalContacts() { miNonPhysicalContactCount = 0; }

        // ==========================================================================================
        // ⭐ ADDED 2026-08-06 (big-five #2): StartVehicleContactGeneration's own contact-generation
        // callees. Signatures per the PS3 DecFIGS mangles (@0x75C0C8 / @0x788190 / @0x789760);
        // ⚠ 0x8261BF28 and 0x825C2EA0 are .ida-exports HOLES -- the PS3 twins are the signature
        // authority for both. ⚠ FLAG: all four bodies are TRAP STUBS this wave
        // (BrnVehicleManagerContactGeneration.cpp) -- named, not landed.
        // ==========================================================================================

        // @0x8261BB38 (PS3 0x75C0C8). Generate the car-vs-car contacts for one overlap pair.
        // lu16QueueIndex is the custom potential-contact queue the pair's contacts route to --
        // the driver passes 7 (racecar-racecar), 8 (racecar-traffic) or 13 (traffic-traffic),
        // exactly the bridge's queue-index/ContactId binding.
        void DoCarCarContactGeneration( CgsSceneManager::EntityId lGlobalIdA,
                                        CgsSceneManager::EntityId lGlobalIdB,
                                        CgsSceneManager::EntityId lPhysicsIdA,
                                        CgsSceneManager::EntityId lPhysicsIdB,
                                        BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
                                        BrnPhysics::ContactGenList* lpContactGenList,
                                        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
                                        CgsMemory::SimpleDataStreamProducer* lpSphereSphereStream,
                                        u16 lu16QueueIndex,
                                        f32 lfTimeStep );

        // @0x825EB140 (PS3 0x788190). Generate one race car's car-vs-world contacts.
        void DoRaceCarWorldContactGeneration( s32 liRaceCarIndex,
                                              BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
                                              const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
                                              BrnPhysics::ContactGenList* lpContactGenList,
                                              CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
                                              CgsMemory::SimpleDataStreamProducer* lpSphereTriangleStream,
                                              CgsMemory::SimpleDataStreamProducer* lpSweptSphereTriangleStream,
                                              u32 luQueueIndex );

        // @0x8261BF28 (⚠ export HOLE; PS3 0x789760). Generate one traffic car's car-vs-world
        // contacts.
        void DoTrafficCarWorldContactGeneration( s32 liTrafficIndex,
                                                 BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
                                                 const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
                                                 BrnPhysics::ContactGenList* lpContactGenList,
                                                 CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
                                                 CgsMemory::SimpleDataStreamProducer* lpSphereTriangleStream,
                                                 u32 luQueueIndex,
                                                 CgsMemory::LinearMalloc* lpLinearMalloc );

        // @0x825C2EA0 (⚠ export HOLE -- no export json AND no PS3 out-of-line twin surfaced;
        // signature from the two SVCG call sites: (this, race-car index) -> bool in r3).
        bool IsRaceCarHidden( s32 liRaceCarIndex );

        // ==========================================================================================
        // ⭐ ADDED 2026-08-06 (PhysicsModule::Update leaves wave). The four small per-frame leaves of
        // PhysicsModule::Update @0x825B0640, DWARF-authoritative signatures, bodied in the slice TU
        // BrnVehicleManager_PerFrameLeaves.cpp (home BrnVehicleManager.cpp is still unmounted --
        // the established slice pattern; fold back when the home mounts).
        // ==========================================================================================

        // @0x8261BAE0 (DWARF h:647). End-of-frame teardown of the two contact-generation IO
        // allocations: DestroyIOBuffer<CollisionGenerator>(&mpContactGenerator) FIRST, then
        // DestroyIOBuffer<ContactGenList>(&mpContactGenList) -- the console's call order.
        void FreeAllocations(CgsModule::IOBufferStack* lpIOBufferStack);

        // @0x82629E18 (DWARF h:908). Drain the air-ram event queue: owner RACE_CAR (1) forwards to
        // maRaceCarVehicles[idx].AddAirRam, owner TRAFFIC_VEHICLE (2) to
        // mPhysicalTrafficManager.ProcessAddAirRamEvent, anything else fires the
        // "Invalid Entity type in air ram UpdateVehicleEffects Effects" assert (:3544).
        void UpdateVehicleEffects(const VehicleEffectsInputInterface* lpEffectsInterface);

        // @0x825C5520 (DWARF h:917 -- the arg is the sim InputBuffer's InChangeRigidBodyInertiaQueue
        // typedef == this exact EventQueue instantiation; typedefs do not change the type). Per
        // queued event whose id owner is RACE_CAR: bounds-assert the 14-bit index (:4321), require
        // the FULL 64-bit event mID to equal maRaceCarHandlingBodyIDs[idx] (asm `cmpld` -- the
        // Hex-Rays renders a 32-bit compare, the asm is authoritative), then forward to the car's
        // ExternalPhysicsBody::ReadPropertiesFromChangeInertiaEvent.
        void ReadUpdatedBodyProperties(
            const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia, 200>* lpInertiaQueue);

        // @0x825EA580 (DWARF h:926). In SHOWTIME game modes (meCurrentGameModeType == 2 or == 16),
        // look up the player car's deformation state record (GetCarStateF on the interface's
        // mpDeformationState, keyed by maRaceCarEntityIDs[mePlayerActiveRaceCarIndex]) and feed it
        // to the player car's RaceCarPhysics::UpdateShowtimeBounceModifiers.
        void ProcessDeformationStates(const Deformation::DeformationOutputInterface* lpDeformationInterface);

        // The per-contact working set the impact classifiers read/populate. Verbatim DWARF
        // layout (BrnVehicleManager.h:763). Pointer members use the forward-declared collaborators.
        struct RaceCarResponseInfo
        {
            BrnPhysics::ContactSpy::RaceCarContact*           mpContact;                  // +0x00
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* mpRequestOutputInterface; // +0x04
            BrnGameState::GameStateModuleIO::VehicleOutputInterface*    mpVehicleOutputInterface; // +0x08
            VehicleManagerOutputInterface*                    mpManagerOutputInterface;   // +0x0C
            BrnPhysics::Deformation::DeformationInputInterface* mpDeformationInterface;    // +0x10
            EntityId             mRaceCarAEntityID;            // +0x14
            EntityId             mRaceCarBEntityID;            // +0x18
            EActiveRaceCarIndex  meActiveRaceCarIndexA;        // +0x1C
            EActiveRaceCarIndex  meActiveRaceCarIndexB;        // +0x20
            RaceCarPhysics*      mpRaceCarA;                   // +0x24
            RaceCarPhysics*      mpRaceCarB;                   // +0x28
            Vector3              mClosingVelocityAtoB;         // +0x30 (16-aligned)
            VecFloat             mvfSlamMagnitude;             // +0x40
            bool                 mbRaceCarAIsCrashing;         // +0x50
            bool                 mbRaceCarBIsCrashing;         // +0x51
            bool                 mbRaceCarAIsPlayer;           // +0x52
            bool                 mbRaceCarBIsPlayer;           // +0x53
            bool                 mbRaceCarAIsNetworkCar;       // +0x54
            bool                 mbRaceCarBIsNetworkCar;       // +0x55
            bool                 mbOtherCarIsAI;               // +0x56
            f32                  mfClosingSpeed;               // +0x58
            f32                  mfRaceCarASpeed;              // +0x5C
            f32                  mfRaceCarBSpeed;              // +0x60
            f32                  mfNormalStressSq;             // +0x64
            Matrix44Affine       mRaceCarATransform;           // +0x70 (16-aligned)
            Matrix44Affine       mRaceCarBTransform;           // +0xB0
            f32                  mfAngleBetweenCars;           // +0xF0
            EImpactType          meImpactType;                 // +0xF4
            EActiveRaceCarIndex  meAggressorActiveRaceCarIndex; // +0xF8
            EActiveRaceCarIndex  meVictimActiveRaceCarIndex;   // +0xFC
            bool                 mbCrashRaceCarA;              // +0x100
            bool                 mbCrashRaceCarB;              // +0x101
            bool                 mbPlayerWonImpact;            // +0x102
            u32                  muImpactScore;                // +0x104
            EImpactSituation     meImpactSitutation;           // +0x108
        };

        // --- the car-vs-car contact entry point this slice bodies (DWARF h:1149; X360 @0x82642F78) ---
        // STAGE 1 of the takedown chain. Called by ProcessContactSpies once per resolved race-car-vs-
        // race-car contact. Decodes the two EntityIds, gates on mbTakedownsEnabled + the live-car
        // bitset, populates a stack-local RaceCarResponseInfo (indices, crash/player flags, speeds,
        // mfAngleBetweenCars), runs the grinding pre-pass + CheckForAllTypesOfImpacts, commits any
        // flagged crash via SetRaceCarCrashing, then drives GenerateContactSituation -> ApplySlam/
        // ApplyShunt + the last-attacker/revenge bookkeeping. Signature is DWARF-authoritative
        // (the interface order is Request, Vehicle, Manager, Deformation -- note it differs from the
        // RaceCarResponseInfo member order).
        void HandleRaceCarRaceCarContact(BrnPhysics::ContactSpy::RaceCarContact lContact,
                                         BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                         VehicleManagerOutputInterface* lpManagerOutputInterface,
                                         BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                         f32 lfTimestep);

        // --- the takedown impact classifier this slice bodies (DWARF h:1182; X360 @0x82642E58) ---
        // Runs the per-type sub-classifiers in strict priority order and stops at the first that
        // fires; the classification's side effects happen inside the sub-classifiers, so this
        // returns void. Called by HandleRaceCarRaceCarContact.
        void CheckForAllTypesOfImpacts(RaceCarResponseInfo* lpInfo);

        // --- the priority-ordered sub-classifiers ---
        // Bodied here: PlayerSlammingAIIntoAI, HittingAlreadyCrashingCar, VerticalTakedown, TBone,
        // HeadToHead, ShuntAndNudge, SlamAndTradingPaint, StationaryTargetTakedown.
        bool CheckForPlayerSlammingAIIntoAI(RaceCarResponseInfo* lpInfo);
        bool CheckForHittingAlreadyCrashingCar(RaceCarResponseInfo* lpInfo);
        bool CheckForVerticalTakedown(RaceCarResponseInfo* lpInfo);
        bool CheckForTBoneTakedown(RaceCarResponseInfo* lpInfo);
        bool CheckForHeadToHead(RaceCarResponseInfo* lpInfo);
        bool CheckForShuntAndNudge(RaceCarResponseInfo* lpInfo);
        bool CheckForSlamAndTradingPaint(RaceCarResponseInfo* lpInfo);
        bool CheckForStationaryTargetTakedown(RaceCarResponseInfo* lpInfo);

        // --- the takedown COMMIT routine the classifiers call once a takedown is decided ---
        // (DWARF h:1257; X360 @0x82636108). Decodes the victim/aggressor EntityIds to active-car
        // indices, crashes the victim via SetRaceCarCrashing (unless it is already in the fatal
        // crash state), then stamps the per-car last-attacker / taken-down bookkeeping. lfNormalStressSq
        // is consumed by the classifier but NOT forwarded to SetRaceCarCrashing.
        void InstantTakedown(EntityId lVictimEntityId,
                             EntityId lAggressorEntityId,
                             Vector3 lCollisionNormal,
                             Vector3 lContactPoint,
                             f32 lfNormalStressSq,
                             BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                             BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                             BrnGameState::ETakedownType leTakedownType);

        // --- declare-only callees (bodied by their own TUs / not in this dossier) ---------------
        // The crash commit itself (DWARF h:1218; 9-param TU, X360 @0x82634C90).
        void SetRaceCarCrashing(EntityId lVictimEntityId,
                                EntityId lAggressorEntityId,
                                Vector3 lCollisionNormal,
                                Vector3 lContactPoint,
                                BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                VehicleManagerOutputInterface* lpManagerOutputInterface,
                                BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
                                BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                BrnGameState::ETakedownType leTakedownType);

        // The shunt/slam force-physics appliers (X360 ApplyShunt @0x8261A5B0, ApplySlam @0x8261A738).
        // DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        void ApplyShunt(RaceCarResponseInfo* lpInfo);
        void ApplySlam(RaceCarResponseInfo* lpInfo);

        // Per-victim "does this impact qualify to crash the car" predicate (consumed by #1/#2).
        // No standalone export in this dossier -- declare-only; FLAG: signature inferred from the
        // call sites (this, victim active-index, victim RaceCarPhysics*, other RaceCarPhysics*).
        bool ShouldRaceCarCrashOnCarImpact(s32 liVictimActiveRaceCarIndex,
                                           RaceCarPhysics* lpVictim,
                                           RaceCarPhysics* lpOther);

        // The vertical-takedown geometric sub-test (#3 helper). Not in this dossier -- declare-only.
        // FLAG: signature inferred (this, victim RaceCarPhysics*, aggressor RaceCarPhysics*).
        bool CheckForVerticalTakedownSituation(RaceCarPhysics* lpVictim, RaceCarPhysics* lpOther);

        // The T-bone side-plane containment test (#4 helper). Not in this dossier -- declare-only.
        // The X360 builds two parallel planes from the victim transform + half-width and tests the
        // contact point against them. FLAG: signature inferred (point + the two plane vectors); the
        // VMX plane math is delegated to this (unrecovered) helper rather than reconstructed inline.
        bool IsPointBetweenTwoParallelPlanes(Vector3 lPoint, Vector3 lPlaneA, Vector3 lPlaneB);

        // The recency throttle (X360 @0x825B4EB8). Its body is NOT in this dossier -- declare-only.
        // FLAG: body unrecovered; signature inferred (this, victim active-index).
        bool HasRaceCarHadRecentImpact(s32 liActiveRaceCarIndex);

        // The grind/rubbing pre-pass detector (X360 CheckForGrindingAndRubbing @0x825B5450). Returns
        // true when the player is grinding/rubbing the other car this frame. Declare-only -- bodied by
        // its own TU. DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        bool CheckForGrindingAndRubbing(RaceCarResponseInfo* lpInfo);

        // Resolves the EImpactSituation that selects ApplySlam vs ApplyShunt (X360
        // GenerateContactSituation). Writes lpInfo->meImpactSitutation. Declare-only -- bodied by its
        // own TU. DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        void GenerateContactSituation(RaceCarResponseInfo* lpInfo);

        // ==========================================================================================
        // Player-stats / showtime / network / lookup surface (X360 wave-10 fan-out). These nine
        // functions are independent of the takedown classifier chain above; they read/write the deep
        // §7 members (player active index, the player-stats + showtime blocks, the network-hidden
        // bitset/countdown, the traffic global->physical map). Offsets/constants are asm-proven.
        // ==========================================================================================

        // @0x8259BF00: copy the per-frame player-car stats action into the manager's stats block and
        // the player car's record. lpSendCarStatsAction points at >=6 floats: [0..3] -> maPlayerCarStats
        // [0..3]; [4] -> mfShowtimePlayerCarDamageLimit; [5] -> maPlayerCarStats[4] AND the player
        // record's mfPlayerBoostStrengthStat; (s32)[1] * 0.1f -> mfShowtimePlayerCarStrength.
        void ApplyPlayerStats(const f32* lpSendCarStatsAction);

        // @0x825B4DE0: resolve a GLOBAL entity id to a PHYSICS traffic entity id via the
        // global->physical index map. Returns true and writes *lpOutPhysicsEntityId (packed
        // (physicalIndex << 10) | E_ENTITYTYPE_TRAFFIC_VEHICLE bits) when the map slot is not the 0x7F
        // "no vehicle" sentinel; returns false otherwise.
        bool GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(u32 luGlobalEntityId,
                                                              EntityId* lpOutPhysicsEntityId);

        // ⭐ ADDED 2026-08-06 (FixUpVehicleContacts wave). DWARF :1025 -- resolve a GLOBAL entity
        // id to its LOCAL PHYSICS entity id. The X360 build fully INLINES this (three instances in
        // PhysicsModule::FixUpVehicleContacts @0x825A6010 alone -- the "Fixup* id-rewrite"
        // territory); the PS3 DecFIGS build keeps it out-of-line @0x6AC834 with this exact body
        // and parameter name, and its own baked asserts pin the definition to THIS header
        // (BrnVehicleManager.h:2031/:2039 on X360; the PS3 copy cites :2030/:2038 -- FIGS-branch
        // line drift). Defined inline below the class, matching the console inlining.
        //   * TRAFFIC arm: global index -> physical index through
        //     mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap (KU8_INVALID_MAP == 127
        //     -> streamed "Failed to find local physics ID of traffic vehicle with global entity
        //     ID: " assert @BrnPhysicalTrafficManager.h:1030 + K_INVALID_ENTITY_ID), else
        //     (physical << 10) | (E_ENTITYTYPE_TRAFFIC_VEHICLE << 24) -- the exact idiom the
        //     committed PTM::ValidateAndFixUpTrafficTrafficContact already carries.
        //   * RACECAR arm: pure VALIDATION -- for every live car (mUsedRaceCars) whose
        //     maRaceCarEntityIDs[i] matches, assert the re-packed id equals the global id
        //     ("lEntityId_Physics == lGlobalEntityID" :2031); the id is returned UNCHANGED.
        //   * any other owner: streamed "Attempting to get local physics ID of unsupported
        //     entity type" :2039 + 0xFFFFFFFF.
        CgsSceneManager::EntityId GetPhysicsEntityIDFromGlobalEntityID(CgsSceneManager::EntityId lGlobalEntityID);

        // @0x825B4F50: resolve a packed physics-vehicle id to its physics body. Owner==RACECAR (1)
        // returns &maRaceCarVehicles[index] (as the VehiclePhysics base); owner==TRAFFIC_VEHICLE (2)
        // delegates to the contained PhysicalTrafficManager. Returns an untyped body pointer (the two
        // branch types -- RaceCarPhysics : VehiclePhysics and SimpleVehiclePhysics : ExternalPhysicsBody
        // -- share no base, matching the X360's raw-pointer return).
        void* GetVehiclePhysi(EntityId lPhysicsVehicleId);

        // @0x825C3040: mark a NETWORK race car hidden for at least luFrames frames (sets its bit in
        // mHiddenNetworkRaceCars and stores luFrames into maHiddenForFrames[index]).
        void SetNetworkRaceCarHidden(EActiveRaceCarIndex leActiveRaceCarIndex, s32 liFrames);

        // @0x8259C028: store the local player's active-race-car slot (gated 0..7).
        void SetPlayerActiveRaceCarIndex(EActiveRaceCarIndex lePlayerActiveRaceCarIndex);

        // @0x8259C098: store the current showtime behaviour mode (gated 0..2).
        void SetShowtimeBehaviour(u32 luShowtimeBehaviour);

        // @0x8259C108: drive the player car into (or out of) showtime: forwards the cached showtime
        // strength/damage-limit to RaceCarPhysics::SetPlayerVehicleInShowtime on the player car and
        // latches the global player-in-showtime byte.
        void SetPlayerCarToShowtimeMode(bool lbInShowtime);

        // ==========================================================================================
        // Race-car-vs-world crash-prediction slice (X360 @0x82640C28). DWARF-authoritative
        // signatures (BrnVehicleManager.h:1296/1200/1215). HandleCrashPredictionForRaceCarAndWorld
        // walks the interface's already-validated race-car-world potential-contact queue, groups the
        // contacts by their volume-A entity word (VolumeInstanceId high dword), orders each group by
        // predicted impact time via PotentialContactOrderer, and dispatches every surviving contact
        // to HandleRaceCarWorldPotentialContact. The two callees are bodied by their own TUs; declared
        // here (declare-only) so this slice can call them.
        // ------------------------------------------------------------------------------------------
        // @0x82640C28 -- the crash-prediction driver bodied by THIS slice.
        void HandleCrashPredictionForRaceCarAndWorld(
            f32 lfTimestep,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            const VehicleInputInterface* lpVehicleInputInterface,
            BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // Declare-only callees (bodied by their own TUs). Signatures are DWARF-authoritative
        // (:1200 / :1215). The tri-cache arg is the nested VehicleInputInterface::InTriangleCacheInterface,
        // which is CgsSceneManager::SceneManagerIO::TriangleCacheInterface.
        void HandleRaceCarWorldPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::PhysicsModuleIO::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnGameState::GameStateModuleIO::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            f32 lfTimestep);

        VecFloat PredictCarWorldContactTime(const CgsSceneManager::SceneManagerIO::PotentialContact& lContact);

    private:
        // ------------------------------------------------------------------------------------------
        // Deep VehicleManager data members, recovered by LAYOUT RECOVERY WITH PADDING from the X360
        // asm offsets. The full VehicleManager is ~172 KB across many parallel per-car arrays;
        // everything not modelled is opaque padding so each named member lands at its proven byte
        // offset (pinned by the offsetof asserts in _AssertLayout / _AssertLayoutPlayerStats). The
        // gate FAILS if any padding run is wrong, which is the signal.
        //
        // ⭐ RE-SEATED 2026-08-03. Every member from the class head down to +44768 has now been
        // re-derived directly from `VehicleManager::Construct` @0x8263B7C8 (943 instructions) and
        // cross-checked against the DWARF's member ORDER (BrnVehicleManager.h:815-970), which lists
        // the same members in the same sequence. Four committed errors were corrected -- the whole
        // per-car driver array was 64 bytes too low, a stride-8 array was modelled at stride 4, a
        // car-TYPE array was named as a crash-STATE array, and three live bitsets were buried in
        // padding -- and nine members were newly pinned. Names below are the DWARF's wherever the
        // DWARF names that seat; the remaining "FLAG" names are still role-derived.
        //
        // ⚠️ Members reached by ABSOLUTE offset from inside a contained sub-object (the
        // PhysicalTrafficManager interior at +148128 / +149456) stay siblings here, because the X360
        // build folds them to absolute class offsets; that is deliberate, not an oversight.
        // ------------------------------------------------------------------------------------------
        //
        // ==========================================================================================
        // ⭐⭐⭐ `VehicleManager::Construct` @0x8263B7C8 IS BODIED as of 2026-08-03, in
        //    BrnVehicleManager.cpp. All 943 instructions: the thirty monitors, the spine, the
        //    eight-car loop and all 91 tuning seats. The rest of this block is the recipe it was
        //    written from and is kept because it is the evidence; the two corrections THIS wave
        //    forced are recorded first, because both were claims this file made about itself.
        //
        // ⛔ CORRECTION 1 -- "⇒ Construct is NOT blocked on link closure" (below) IS WRONG, and the
        //    ✅ READY column of the blocker table is what misled it. READY there means "the real x64
        //    class FITS its X360 span, so the call is spellable BY NAME". It does NOT mean the symbol
        //    resolves. MEASURED by mounting BrnVehicleManager.cpp and reading the linker: of
        //    Construct's own callees, TWO are unresolved in the mounted build --
        //        BrnPhysics::StuntOffencesManager::Construct
        //        BrnPhysics::Vehicle::PhysicalTrafficManager::Construct
        //    -- and both BODIES EXIST (BrnStuntOffencesManager.cpp / BrnPhysicalTrafficManager.cpp);
        //    neither TU is in tools/build/build_game_exe.bat. Mounting the whole TU costs 15
        //    unresolved externals in total (the other 13 belong to the takedown chain that shares
        //    this file -- see the ⛔ note at the mount site in the build script for the list).
        //    ⇒ Construct IS blocked on link closure. It is blocked on TWO mount lines, not on
        //    reconstruction, which is a much better place to be -- but "not blocked" was false.
        //
        // ⛔ CORRECTION 2 -- the blocker table's remaining ⛔ row (RaceCarVehicleRecord vs
        //    VehiclePhysics) IS RETIRED, but not the way the table expected. The record is GONE:
        //    maRaceCarVehicles is the real `RaceCarPhysics[8]` and the host/console difference is
        //    carried as KU_HOST_DRIFT_AFTER_RACECAR_ARRAY. See the fold-in note further down.
        //    ⚠️ AND IT HAD A COST THE TABLE DID NOT ANTICIPATE: embedding a POLYMORPHIC class by
        //    value made the already-mounted BrnPhysicsModule.cpp (which embeds a VehicleManager)
        //    odr-use the whole RaceCarPhysics vtable, which turned two long-standing DECLARE-ONLY
        //    virtuals into link errors -- SimpleVehiclePhysics::SetCrashing and
        //    VehiclePhysics::IsIgnoringPassedOnImpulses. Both now have vtable-closure GATES (loud
        //    asserts, not quiet no-ops) in their own TUs; read those banners before touching them.
        //
        // ⭐ WHAT WAS STILL MISSING FOR `VehicleManager::Construct` @0x8263B7C8 -- MEASURED 2026-08-03,
        //    not estimated. The layout wave above mined this function for OFFSETS; this note records
        //    what re-reading all 943 instructions says about BODYING it, so the next wave starts from
        //    a measurement instead of from a label.
        //
        // ⭐ NOT AN EXPORT HOLE, AND EVERY SUB-CONSTRUCTOR IS ALREADY BODIED. From the function's own
        //    `xrefs_from` (`.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8263B7C8.json`), the complete
        //    callee set is twelve symbols, and after 2026-08-03 all six real ones have bodies:
        //      CgsDev::PerfMonCpu::AddMonitor                @0x82824C30  (declared, PC-leaf)
        //      VehicleManagerDebugComponent::Construct       @0x825B5A78  bodied
        //      VehicleDriver::Construct                      @0x825B83C8  bodied
        //      VehiclePhysics::Construct                     @0x8262DBD0  bodied THIS wave
        //      PhysicalTrafficManager::Construct             @0x82636CA8  bodied
        //      StuntOffencesManager::Construct               @0x825E8C08  bodied
        //    plus __savegprlr_14 / __savefpr_22 / __restfpr_22 and the three CgsDev::Assert entries.
        //    ⛔⛔ THE SENTENCE THAT STOOD HERE -- "⇒ Construct is NOT blocked on link closure" --
        //    IS FALSE; see CORRECTION 1 at the top of this block. "bodied" in the list above means
        //    the body EXISTS SOMEWHERE IN THE TREE, and for two of the six the TU that holds it is
        //    not mounted, so the symbol does not resolve. Construct IS blocked on link closure, on
        //    exactly two mount lines (BrnStuntOffencesManager.cpp, BrnPhysicalTrafficManager.cpp).
        //    MEASURED with the linker 2026-08-03, not reasoned.
        //    Its only caller is PhysicsModule::Construct @0x825AE308, which is still a
        //    WorldLinkStubs stub for exactly two reasons: this function, and
        //    PhysicsSimulationModule::Construct.
        //    ⛔ BUT DO NOT STOP READING HERE. "Has a body" is not "can be called": three of those
        //    six sub-constructors take a `this` that VehicleManager cannot supply, because the
        //    member is an X360-sized opaque span and the real x64 class does not fit in it. The
        //    measured table is a few paragraphs down and it is the actual blocker.
        //
        // THE SHAPE, measured (instruction counts are from the disassembly, not guessed):
        //    ~1..310   THIRTY `CgsDev::PerfMonCpu::AddMonitor` calls, each storing its s32 handle into
        //              a FILE-SCOPE global in the run dword_82F2A14C..dword_82F2A1A0. Every call uses
        //              the same register shape CgsPerfMonCpu.h already documents
        //              (`li r4,0xC ; li r5,0 ; fmr f1,f22 ; li r7,1`) with f22 == flt_82004A20, and
        //              every monitor NAME is an inline string ("VMan: Update Stunt Offences",
        //              "VMan: Update Vehicle Impacts", "VMan: Process Above Ground LTs", ...).
        //              ⇒ Fully recoverable; the work is homing 30 named globals, not decoding.
        //    ~311..410 mePrepareStage/meReleaseStage, then an INLINED CgsNumeric::Random seeding:
        //              muSeed = 0x1AD0891BC87CD8C9, index = 0, ring[0] = 1.0f, then seven draws of the
        //              LCG `seed = seed*0x5851F42D4C957F2D + 1` with `inslwi rX,hi32,23,9` building a
        //              float in [1,2) -- the same generator VehiclePhysics::UpdateRoadNoise already
        //              documents. ⇒ Recoverable; belongs in CgsRandom.h as the real seed/Construct.
        //    ~411..510 the EIGHT-CAR LOOP: VehicleDriver::Construct(&maRaceCarDrivers[i]) and
        //              VehiclePhysics::Construct(&maRaceCarVehicles[i]), plus per-record writes at
        //              in-record +0x1070/+0x13E4/+0x13F0/+0x1400/+0x1408/+0x140C/+0x140D.
        //              ⛔⛔ CORRECTED 2026-08-03: this note used to insist "it constructs the
        //              VEHICLEPHYSICS BASE of each RaceCarPhysics, **not** a RaceCarPhysics::
        //              Construct", and called the trailing writes unnamed per-record scratch. Both
        //              are wrong, and the PS3 DecFIGS build settles it in one function. Its
        //              RaceCarPhysics::Construct @0x6EB3D4 is, in full:
        //                  bl   VehiclePhysics::Construct
        //                  stvx v13(0), this, 0x13E0        <- X360 +0x13F0
        //                  stfs 0.0f, 0x13F8(this)          <- X360 +0x1408
        //                  stfs 0.0f, 0x13F0(this)          <- X360 +0x1400
        //                  stb  0, 0x13FD(this)             <- X360 +0x140D
        //                  stb  0, 0x13FC(this)             <- X360 +0x140C
        //                  <lvx/vperm/stvx at this+0x1060>  <- X360 +0x1070, the Z-lane insert
        //              i.e. the X360 loop body IS RaceCarPhysics::Construct with its base call
        //              inlined, at the uniform Δ = −16 that separates the two builds in this region,
        //              and every one of those "unnamed" writes is a named RaceCarPhysics member:
        //              mPropCollisionImpulseSum, mfBeachedTime, mfTimeSinceTookDownPlayer,
        //              mbUsingAftertouch, mbPlayerCarInShowtime. (+0x13E4 is the debug-component
        //              pointer and belongs to VehiclePhysics; it is NOT in RaceCarPhysics::Construct.)
        //              ⇒ when this function is finally bodied, the loop body is
        //              `maRaceCarVehicles[i].Construct();`, not a base-only call plus six pokes.
        //    ~511..600 PhysicalTrafficManager::Construct, the mDiscardedContacts queue bind, the
        //              mCameraMatrix identity stamp, the second VehicleDriver::Construct
        //              (mPlayerAiDriver) and the four RaceCarBitArray clears -- all already pinned.
        //    ~601..943 the TUNING BANK -- ✅ **RESOLVED 2026-08-03**, see the big block further down
        //              at `mbSlamsAndShuntsOn`. Corrections to the sizing that stood here before:
        //              it is **89** indexed `st{fs,b,w}x` stores hitting **89 DISTINCT seats** (one
        //              store per seat, no duplicates), not "96 stores / 88 seats" -- the apparent
        //              duplicate was a stack-spill reload of the offset register (`stw r10,var_150`
        //              @0x8263C278, `lwz r8,var_150` @0x8263C38C) that a naive scan mis-resolves.
        //              There are also **two VECTOR stores** the old sizing missed entirely:
        //              +172432 <- the 16 bytes at `unk_82181520` ({0,0,1,0}), and +172592 <- 16 zero
        //              bytes. All 89 + 2 seats are now declared, named and defaulted.
        //              ⛔ AND THE OLD NOTE'S METHOD ADVICE WAS WRONG: it said the ~40 constants
        //              "must be read off the asm" because "a literal scan of pseudocode will not
        //              find them", and warned they might be silent-zero .data. In fact this
        //              function's Hex-Rays renders every literal directly (`*(_R31 + 171468) = 4.0;`)
        //              -- the export has a GOOD pseudocode, unlike VehiclePhysics::Construct's. No
        //              image read was needed. There are 33 distinct scalars, not ~40.
        //
        // ⛔ DO NOT SHIP A PARTIAL Construct. A body that runs the first ~510 instructions and skips
        //    the tuning bank would leave every takedown/slam/shunt threshold at zero while LOOKING
        //    complete -- the silent-drop-stub failure class. Either the tuning bank lands with it or
        //    the function stays unbodied. (The bank's DEFAULTS are now recorded member-by-member
        //    below, so the body can be written straight off this header.)
        //
        // ==========================================================================================
        // ⛔⛔ AND HERE IS WHY IT IS STILL UNBODIED -- MEASURED 2026-08-03 (the Construct-blocker
        //     wave), with the compiler, not reasoned. The previous note ended "now that the layout
        //     is settled", and the wave brief that followed it said Construct was "no longer blocked
        //     on layout". **BOTH ARE WRONG.** The TUNING BANK is settled -- that is the last ~340
        //     instructions. The FIRST ~600 are the sub-constructor spine, and three of its six calls
        //     target members that this class carries as X360-SIZED OPAQUE SPANS whose real
        //     reconstructed types are LARGER on x64. You cannot call a constructor on a span that
        //     cannot hold the object.
        //
        //     Measured with `char (*p)[sizeof(T)] = 1;` against the committed headers (MSVC 19,
        //     /std:c++17, x64) -- the C2440 diagnostic prints the array bound:
        //
        //       call site in Construct                          real x64 sizeof   span here   verdict
        //       ---------------------------------------------   ---------------   ---------   -------
        //       VehicleDriver::Construct(&maRaceCarDrivers[i])              224         224   ✅ READY
        //       VehicleDriver::Construct(&mPlayerAiDriver)                  224         224   ✅ READY
        //       StuntOffencesManager::Construct(this + 44240)               464         464   ✅ READY
        //       mDiscardedContacts bind (this + 160672)                    1296        1296   ✅ READY
        //       PhysicalTrafficManager::Construct(this + 44768)          105840      105648   ✅ READY   <- 2026-08-03
        //       VehicleManagerDebugComponent::Construct(+161968)           1328        1296   ✅ READY   <- 2026-08-03
        //       RaceCarPhysics::Construct(&maRaceCarVehicles[i])           5008    (5216)  ✅ READY   <- 2026-08-03
        //         (was "VehiclePhysics::Construct ... 4752 (rec 5216) ⛔". The record is GONE and the
        //          call is by name on the real type. The two numbers no longer have to match: the
        //          difference is KU_HOST_DRIFT_AFTER_RACECAR_ARRAY == -1664. See the fold-in note.)
        //
        //     THREE of the READY rows were opaque byte arrays before and are typed now
        //     (mStuntOffencesManager, mDiscardedContacts, mPhysicalTrafficManager) or were already
        //     typed (maRaceCarDrivers / mPlayerAiDriver). Nothing moved that was not meant to: the
        //     compiled layout gate in BrnVehicleManager_layout_check.cpp is what proves it, and
        //     growing any of them by one byte fires its asserts (tamper-tested).
        //
        //     ⭐⭐ THE PhysicalTrafficManager ROW WAS WRONG, and it was the biggest of the three
        //       blockers. It read "105840 vs 103360, ⛔ +2480". The 103360 was this class's own
        //       opaque-span guess, and it was 2288 bytes short -- the real X360 size is 105648,
        //       derived twice over (BrnPhysicalTrafficManager.h finding (4); the span's own header
        //       note already contradicted it by placing a manager member at 44768 + 104688). The
        //       genuine host overrun is +192, small enough to CARRY as
        //       KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER rather than to block on. The member is embedded
        //       by name and `mPhysicalTrafficManager.Construct()` is callable today.
        //       ⇒ Lesson for the two rows still marked ⛔: **re-derive the span before trusting the
        //         verdict**. A blocker computed against an opaque byte array is a claim about the
        //         array, not about the class.
        //     * VehicleManagerDebugComponent grows 32 bytes because its base's vptr and its
        //       `mpVehicleManager` both widen 4 -> 8. ⚠️ Unlike the traffic manager, this span is
        //       NOT a guess: 161968..163264 is bracketed by two asm-literal anchors (the Construct
        //       call at +161968 and the stride-1024 walk from +163264), so the +32 is real -- which
        //       is why it is CARRIED (KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT) rather than derived away.
        //       ⚠️⚠️ AND A CLAIM WRITTEN HERE EARLIER IN THIS SAME WAVE WAS WRONG, recorded because
        //       it is the exact failure mode this file keeps warning about: it said the component
        //       "cannot be embedded at all today, because BrnVehicleManagerDebugComponent.h includes
        //       THIS header, so embedding it by value would be an include cycle". There is no cycle.
        //       That header includes types.hpp / BrnCommonTypes.h / CgsDebugComponent.h /
        //       CgsPhysicsSimulationIO_Events.h / CgsPotentialContact.h / BrnVehicleConstants.h and
        //       FORWARD-DECLARES `class VehicleManager;`. The claim came from a grep whose only hits
        //       were the string "BrnVehicleManager.h" inside that header's COMMENTS -- a blocker
        //       asserted from a text match rather than from the include list, which is the same
        //       shape as the 103360 span above: a verdict about the model, not about the code.
        //     * VehiclePhysics is SMALLER than the 5216-byte record, which is worse, not better:
        //       RaceCarVehicleRecord reproduces the X360 IN-RECORD offsets that the mounted takedown
        //       chain reads by name (mbIsCrashingOrDisabled @+1808, mvWorldPosition @+1920,
        //       mCrashMatrix @+3328, ...). Constructing a real x64 VehiclePhysics in that storage
        //       would write its members at x64 offsets while every reader still looks at X360 ones --
        //       a silent-corruption trade, not a fix. (sizeof(RaceCarPhysics) == 4816 vs 5216.)
        //
        //     ⇒ THE BLOCKER IS NOT LINK CLOSURE AND NOT THE TUNING BANK. It is that this class was
        //       BYTE-PINNED to X360 offsets (deliberately -- most of it is unreconstructed padding),
        //       and a byte-pinned class cannot embed real x64 sub-objects. The fix is the project's
        //       own standing rule -- parity by NAMED MEMBERS, with the host/console divergence
        //       carried as an explicit, tripwired constant instead of pretending it is zero.
        //       ⭐ DONE TWICE, 2026-08-03 -- for the traffic manager (KU_HOST_DRIFT_AFTER_TRAFFIC_
        //       MANAGER) and for the debug component (KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT): both
        //       members are real, the two mis-identified "siblings" are retired, and every
        //       downstream assert still runs. Two of the three blockers are gone.
        //       ⚠️ REMAINING WORK -- ONE item, and it is NOT symmetric with the two just done:
        //         - RaceCarVehicleRecord -> RaceCarPhysics: genuinely multi-wave. It is not a size
        //           argument at all. The ten named in-record fields below DO NOT EXIST as members of
        //           the reconstructed RaceCarPhysics/VehiclePhysics -- that class is not byte-pinned,
        //           it declares only the handful of members its own bodies touch. Swapping the record
        //           for the real type therefore does not "move" those fields, it DELETES them, and
        //           each one has to be re-recovered as a named member (with a DWARF-ordered seat)
        //           before any reader can be re-pointed.
        //           ⭐ PROGRESS 2026-08-03: **3 of the 10 are done**, and the whole
        //           RaceCarPhysics OWN-MEMBER BLOCK (all sixteen DWARF members, X360 +0x13F0..+0x1460)
        //           now exists, is seated, and is gated by a MOUNTED TU
        //           (RaceCarPhysics_layout_check.cpp). Its derivation closes independently on THIS
        //           class's 5216 stride. See the fold-in map over RaceCarVehicleRecord below for
        //           what is settled, what the remaining seven need (the VehiclePhysics /
        //           SimpleVehiclePhysics own blocks, a different class's layout) and the two live
        //           name conflicts that surfaced en route. Until then the record stays byte-pinned.
        //
        //     ⚠️ THE TEMPTING WRONG FIX, written down so it is not re-invented: the 32-byte debug
        //       component overflow could be "absorbed" by shrinking the maRaceCarDebugComponent pad
        //       behind it (both are opaque, nothing reads either by name, and the tuning bank would
        //       keep its offsets). Do not. Construct stores &maRaceCarDebugComponent[i] into each car
        //       record at an asm-literal 1024 stride (`addi r27, r27, 0x400`); silently re-striding
        //       that array to make an unrelated call compile is inventing layout to buy a green
        //       build. If it is ever done it must be a deliberate, argued decision with its own gate.
        //
        // ⭐ WHAT IS ALREADY DECODED, so the unblocking wave writes zero new decode. All of the
        //    below is re-derived first-hand from the freshly pulled asm (943 instructions) and
        //    cross-checked against the same function's Hex-Rays; the seat/constant join reproduced
        //    the tuning bank below with ZERO conflicts across 33 rodata symbols and 91 seats.
        //
        //    (a) THE PERFMON BLOCK, exactly 30 AddMonitor calls. All thirty pass page/min/budget/tag
        //        as (r4, r5, f1, r7); r6 is never written, so these are the FIVE-argument
        //        CgsDev::PerfMonCpu::AddMonitor (CgsPerfMonCpu.h settled that). Budget f22 ==
        //        flt_82004A20 == 10.0f for all thirty. Page is 12 for the first 29 and **6** for the
        //        30th. Twenty-nine store to file-scope globals; the 30th stores to the member
        //        miRaceCarWorldContactValidationPM and is followed by the console's own
        //        `miRaceCarWorldContactValidationPM >= 0` assert (BrnVehicleManager.cpp:778).
        //        ⚠️ SEVEN of the 29 are GUARDED -- `if (global < 0) global = AddMonitor(...)` -- so
        //        those seven globals must be initialised to a NEGATIVE sentinel or they never
        //        register at all (the CgsNetworkPlayer.cpp `s_i*PM = -1` + register-once pattern is
        //        the committed precedent). The other 22 are unconditional.
        //        The monitor strings carry their own tree in their LEADING SPACES (that indentation
        //        is data, not a guess), in call order:
        //          82F2A1A0  "VMan: Update Stunt Offences"
        //          82F2A14C  "VMan: Update Vehicle Impacts"
        //          82F2A150  "VMan: Process Above Ground LTs"
        //          82F2A154  "VMan: Traction LTs"
        //          82F2A158  "        GetLines"
        //          82F2A15C  "        LineTests"
        //          82F2A168  "           Begin"
        //          82F2A16C  "           RunStream"
        //          82F2A170  "           Finish"
        //          82F2A174  "           End"
        //          82F2A160  "        ProcessResults"
        //          82F2A164  "        Traffic"
        //          82F2A178  "VMan: Crash Fatal"
        //          82F2A17C  "VMan: Update Race Cars"
        //          82F2A180  "        Drivers"
        //          82F2A184  "        Vehicles"
        //          82F2A278  "          VPhys::Update"        <- guarded
        //          82F2A27C  "            Switch Attribs"     <- guarded
        //          82F2A280  "            Update Crashing"    <- guarded
        //          82F2A284  "            Update Air Rams"    <- guarded
        //          82F2A288  "            Update Spin"        <- guarded
        //          82F2A28C  "            Update Driving"     <- guarded
        //          82F2A290  "            Update LV"          <- guarded
        //          82F2A188  "        RB Change"
        //          82F2A18C  "        AfterTouch"
        //          82F2A190  "VMan: Update Traffic"
        //          82F2A194  "VMan: Update Aggressive Driving"
        //          82F2A198  "VMan: Update Crashes"
        //          82F2A19C  "VMan: Update PassBys"
        //          (member)  "PHYS ValidateRCWorldContact"    <- page 6, the 30th
        //        The seven guarded ones are precisely the VehiclePhysics-level sub-monitors, which
        //        is what the guard is for: register-once across whoever constructs first.
        //
        //    (b) THE RANDOM SEEDING IS ALREADY A COMMITTED FUNCTION. The inlined block at
        //        0x8263BCDC..0x8263BEE8 is `CgsNumeric::Random::Construct()` verbatim: muSeed =
        //        0xC87CD8C91AD0891B (built by `insrdi r10, r9, 32, 0`, i.e. 0xC87CD8C9 in the HIGH
        //        half -- KU_RANDOM_DEFAULT_SEED), muOldestBufferIndex = 0, ring[0] = 0x3F800000,
        //        then SEVEN AddRandomFloatToBuffer draws (`inslwi r6, hi32, 23, 9` == 0x3F800000 |
        //        (hi32 >> 9)), then one final `index = (index + 1) & 7`. ⇒ the body is
        //        `mRandom.Construct();` -- nothing to write.
        //
        //    (c) THE 8-CAR LOOP body, seat by seat (r29 walks the record at stride 0x1460 == 5216;
        //        every offset below is IN-RECORD, i.e. minus 1856):
        //          VehicleDriver::Construct(&maRaceCarDrivers[i])         (r25, stride 0xE0)
        //          VehiclePhysics::Construct(record + 0)                  (r29 - 0x140D)
        //          +0x1070  lvx128 / vrlimi128 v0,v127,2,0 / stvx128      -- inserts 0 into the Z LANE
        //                   only (mask 8/4/2/1 == x/y/z/w); the same +0x1070 seat
        //                   VehiclePhysics::Construct itself touches
        //          +0x13F0  stvx128 v127  -- 16 zero bytes
        //          +0x1400  stfs 0.0f
        //          +0x1408  stfs 0.0f
        //          +0x140C  stb 0
        //          +0x140D  stb 0
        //          +0x13E4  stw &maRaceCarDebugComponent[i]  (after the NULL assert, VehiclePhysics.h:2228)
        //        and, outside the record: maRaceCarEntityIDs[i] = dword_82F2A3A4,
        //        maRaceCarHandlingBodyIDs[i] = qword_82F2A3A8, maeRaceCarTypes[i] = 3,
        //        mauNetworkCarHiddenFramesRemaining[i] = 0.
        //        ⚠️ SEVEN per-record writes, not the six an earlier sizing banked.
        //
        //    (d) THE REST OF THE SPINE, in issue order:
        //          mePrepareStage = 0; meReleaseStage = 3;
        //          mUsedRaceCars = 0; mUsedRaceCarCrashesList = 0;
        //          VehicleManagerDebugComponent::Construct(this + 161968, this)   <- TWO args; the
        //              Hex-Rays renders it with none
        //          mRandom.Construct();
        //          <the 8-car loop>
        //          PhysicalTrafficManager::Construct(this + 44768)   -- ⭐ callable by name today:
        //              `mPhysicalTrafficManager.Construct();`
        //          mDiscardedContacts = { buffer = this + 160688, capacity = 20, count = 0 }
        //              (+ the console's `lpEventBuffer != NULL` assert, CgsBaseEventQueue.h:160)
        //          mCameraMatrix = identity with a ZERO fourth row: rows {1,0,0,0} {0,1,0,0}
        //              {0,0,1,0} {0,0,0,0}. The 1.0f lanes are flt_82001C98 -- which is the same
        //              rodata slot the tuning bank's mfTailgatingVunerabilityTime reads, so THIS
        //              FUNCTION ALONE proves that value is 1.0f, independently of the PS3 build.
        //          VehicleDriver::Construct(&mPlayerAiDriver)
        //          mHiddenRaceCars = mRaceCarsAddedForCollision =
        //              mNetworkCarsAddedForCollisionThisFrame = mNetworkCarsRecievedFirstUpdate = 0
        //          <the tuning bank -- all 91 seats, defaults recorded member-by-member below>
        //          StuntOffencesManager::Construct(this + 44240)   (issued LATE, between the
        //              +172580 and +172584 counter stores)
        // ==========================================================================================

        // ==========================================================================================
        // ⭐ RE-SEATED 2026-08-03 (VehicleManager layout wave). This array used to be declared as
        // `RaceCarStatusRecord maRaceCarStatus[8]` at class offset **0**. It is really the DWARF's
        // `VehicleDriver maRaceCarDrivers[8]` at class offset **+64**, and the 64 bytes ahead of it
        // are mePrepareStage / meReleaseStage / mRandom (see the class head below).
        //
        // Why it mattered: the three named in-record fields were 64 bytes too high. They are
        // byte-faithful ONLY while the region is padding -- the instant a wave turns those 64 bytes
        // into real members, the three writes corrupt real driver state and the symptom (cars
        // mis-flagged) looks like a bug in the new constructor. Corrected while provably inert.
        //
        // [V] BOTH the base and the stride are asm-literal in VehicleManager::Construct @0x8263B7C8:
        //     0x8263BE90  addi r25, r31, 0x40      <- &maRaceCarDrivers[0] == this + 64
        //     0x8263BF08  bl   VehicleDriver::Construct
        //     0x8263BF80  addi r25, r25, 0xE0      <- stride 224, x8 -> ends at 1856
        // and 1856 is exactly where maRaceCarVehicles starts (asm below), so the array closes.
        //
        // ⭐⭐ STAND-IN RETIRED 2026-08-03. This slot used to be an opaque 224-byte
        // `RaceCarDriverRecord` carrying three role-named bytes at in-record 59/60/61 behind an
        // explicit HYPOTHESIS flag ("assumes an empty CgsModule::Event base and has not been
        // checked against a use site"). It is now the real
        // `BrnPhysics::Vehicle::VehicleDriver` (VehiclePhysics/BrnVehicleDriver.h), recovered from
        // VehicleDriver::Construct @0x825B83C8 with every offset asm-literal.
        //
        // The hypothesis was WRONG in its arithmetic and RIGHT in its instinct. The committed
        // BrnPlayerDriverControls layout it walked put the bool run at 0x35..0x3E; the X360 build
        // carries a thirteenth control float at +0x34, so the run is really 0x39..0x42. The three
        // bytes therefore resolve one slot LOWER than the guess -- and each lands on a DWARF member
        // whose name is an exact match for the role the bodies had already derived:
        //     in-record 59 (0x3B)  was "mbBoostImpactEligible" -> mControls.mbBoost
        //     in-record 60 (0x3C)  was "mbTakenDown"           -> mControls.mbIsInvulnerableToVehicles
        //     in-record 61 (0x3D)  was "mbSuppressByCause"     -> mControls.mbIsInvulnerableToWorld
        // (the boost button promoting SLAM->BOOST_SLAM; the two invulnerability flags suppressing a
        // vehicle-caused and a world-caused crash respectively). The offsetof asserts in
        // BrnVehicleDriver.h pin all three, and the call sites in this class's .cpp now read them
        // by their real names.

        // ==========================================================================================
        // ⭐⭐⭐ THE RECORD IS GONE -- FOLDED IN 2026-08-03. This slot carried
        // `RaceCarVehicleRecord maRaceCarVehicles[8]`, a byte-pinned 5216-byte stand-in with ten
        // named fields poking through opaque padding, for ten waves. It is now the real
        // `BrnPhysics::Vehicle::RaceCarPhysics[8]`.
        //
        // ⛔ WHY THE BLOCKER TABLE ABOVE SAID THIS COULD NOT BE DONE, AND WHAT ACTUALLY CHANGED.
        // The table's ⛔ row read "VehiclePhysics::Construct(&maRaceCarVehicles[i]) 4752 (rec
        // 5216)", and the reasoning under it was correct at the time: the ten field names did not
        // exist as members of the reconstructed classes, so swapping the type would DELETE them.
        // Two waves since then recovered the SimpleVehiclePhysics, VehiclePhysics and
        // RaceCarPhysics own-member blocks in full, and all ten now resolve to a DWARF-seated
        // member -- three of them to members that were never real (they were rows of the base
        // `mTransform`). The blocker retired itself; nothing here argues it away.
        //
        // ⛔⛔ AND THE TREE WAS ALREADY BROKEN IN THE DIRECTION THE TABLE FEARED. Before this wave
        // BrnVehicleManager.cpp (x5) and BrnVehicleManagerPlayerStats.cpp (x1) held live
        //     RaceCarPhysics* p = reinterpret_cast<RaceCarPhysics*>(&maRaceCarVehicles[i]);
        // -- the real class read at HOST offsets over storage laid out at CONSOLE offsets, which is
        // precisely the silent corruption the table refused to allow in the other direction. It was
        // latent only because neither TU is mounted. All six are retired by this fold; they are now
        // plain `&maRaceCarVehicles[i]`, correct by construction.
        //
        // THE ARITHMETIC, measured: sizeof(RaceCarPhysics) == 5008 on the host, 5216 on the X360
        // (`mulli r11, r22, 0x1460`). alignof == 16 and 1856 % 16 == 0, so element 0 still starts
        // at the asm-literal +1856 and every element stays 16-aligned. The array ends at
        // 1856 + 8*5008 == 41920 instead of 43584, hence KU_HOST_DRIFT_AFTER_RACECAR_ARRAY == -1664
        // for everything past it.
        //
        // WHERE THE TEN IN-RECORD SEATS ARE NOW ASSERTED. They are NOT asserted here any more --
        // a host class cannot reproduce console offsets, so an `offsetof(RaceCarPhysics, ...) ==
        // <X360 seat>` would simply be false. The seats live in the two mounted console-arithmetic
        // gates, which is where they belong:
        //     RaceCarPhysics_layout_check.cpp   +0x13F0..+0x1460, closing on the 5216 stride
        //     VehiclePhysics_layout_check.cpp   +0x130..+0x720 and +0x720..+0x13F0
        // and every one of the ten is additionally named in a `(void)offsetof(...)` existence check
        // there, so a rename or a deletion still breaks the build.
        //
        //   in-record  the record called it ...        the member it IS
        //   ---------  ------------------------------  ---------------------------------------------
        //   +16..+80   mTransform                      ExternallySimulatedBody::mTransform (base)
        //   +1808      mbCrashing                      SimpleVehiclePhysics::mbCrashing      (0x710)
        //   +3824      mvSpeedOnLastCrashMPH_...       VehiclePhysics:: same name             (0xEF0)
        //   +4308      meDriverType                    VehiclePhysics::mPreviousControls's    (0x10D4)
        //                                                meDriverType, i.e. +0x1090 + 0x44
        //   +4953      mbDeformationModelIsActive      VehiclePhysics:: same name            (0x1359)
        //   +5084      meCarType                       VehiclePhysics::meCarType             (0x13DC)
        //   +5120      mfTimeSinceTookDownPlayer       RaceCarPhysics:: same name            (0x1400)
        //   +5184      mCrashNormal                    RaceCarPhysics:: same name            (0x1440)
        //   +5200      mEntityCausingCrash             RaceCarPhysics:: same name            (0x1450)
        //   (+1904/+1920 were PHANTOMS -- rows of mTransform. There is nothing to fold.)
        //
        // ⚠️ `mbCrashing` is `protected` and the RaceCarPhysics own block is `private`, so this
        // class is a `friend` of RaceCarPhysics (declared there, with its reason). The X360 reaches
        // both bare, off an absolute per-car offset, with no accessor and no assert -- routing them
        // through getters would add asserts the console does not fire. Same precedent as the two
        // bare PhysicalTrafficManager loads this class already makes.
        //
        // [V] BOTH the base and the stride are asm-literal:
        //     0x8263BF0C  addi  r3, r29, -0x140D   <- &maRaceCarVehicles[0] == this + 1856
        //     0x82635310  mulli r11, r22, 0x1460   <- the 5216 per-car stride
        // (The member itself is declared with the rest of the class head below.)
        // ==========================================================================================

        // The takedown-type record pool. 32 entries, 12-byte stride, @ class offset +43808.
        // SetRaceCarCrashing allocates a slot {entity id, ETakedownType, priority} here for the
        // scoring/UI layer to read back by entity id (doc §3b).
        // FLAG: struct + field names proposed; the 12-byte stride and +43808 base are asm-proven.
        struct RaceCarCrashData
        {
            u32 mEntityId;   // +0
            u32 meType;      // +4 (BrnGameState::ETakedownType, stored as a 4-byte word)
            f32 mfPriority;  // +8
        };

        // ==========================================================================================
        // THE CLASS HEAD -- re-derived 2026-08-03 from VehicleManager::Construct @0x8263B7C8 and
        // cross-checked against the DWARF member ORDER (BrnVehicleManager.h:815-847), which lists
        // exactly these members in exactly this sequence.
        //
        //   0x8263BCC0  stw  r30(0), 0(r31)      -> mePrepareStage = 0
        //   0x8263BCC8  stw  r24(3), 4(r31)      -> meReleaseStage = 3
        //   0x8263BCEC  addi r11, r31, 0x10      -> &mRandom == this + 16, then
        //               stw 1.0f, 0(r11) / stwx buf[i], 4*i(r11) / std seed, 0x20(r11) /
        //               stw index, 0x28(r11)
        //
        // ⚠️ mRandom is at +16, NOT +8. CgsNumeric::Random is
        // `union { f32[8]; u32[8]; VectorIntrinsic[2] } + u64 muSeed(+0x20) + u32 index(+0x28)`
        // == 44 bytes but **16-byte aligned** because of the VectorIntrinsic[2] member, so it
        // cannot sit at +8 and its sizeof is 48. 16 + 48 == 64 == &maRaceCarDrivers[0]: the head
        // closes on three independently-attested numbers. (An earlier brief reached the right
        // +64 answer from the wrong arithmetic -- mRandom@8 -- which would have left an 8-byte
        // hole in a different place.)
        // ==========================================================================================
        s32                  mePrepareStage;    // +0   EPrepareStage (Construct: 0)
        s32                  meReleaseStage;    // +4   EReleaseStage (Construct: 3)
        // ⭐ The eight bytes at +8 are NOT a modelled pad any more -- they are the alignment the
        // 16-aligned CgsNumeric::Random forces, and the compiler now inserts them itself. (The
        // class was carrying an explicit `mPad0008[8]` while mRandom was an opaque blob; with the
        // real 16-aligned type both the padding and the blob are redundant. CgsRandom.h was
        // alignas(8) until this wave -- see the note there.)
        CgsNumeric::Random   mRandom;           // +16  (sizeof 48; ends at 64)

        VehicleDriver        maRaceCarDrivers[8];   // +64      (224 * 8 = 1792; ends at 1856)
        // ⭐⭐ THE REAL TYPE as of 2026-08-03 -- see the fold-in note above. X360 5216 * 8 = 41728
        // ending at 43584; host 5008 * 8 = 40064 ending at 41920, which is what
        // KU_HOST_DRIFT_AFTER_RACECAR_ARRAY (-1664) carries for every member past it.
        RaceCarPhysics       maRaceCarVehicles[8];  // +1856

        // Per-car EntityId validation table @ +43584. Stride 4 (asm: 4*(idx+10896) == 4*idx+43584;
        // Construct seeds it from dword_82F2A3A4 through a stride-4 cursor). SetRaceCarCrashing
        // asserts the packed victim/aggressor id matches the stored id here. Spelling per the DWARF
        // (`EntityId[8] maRaceCarEntityIDs`).
        EntityId             maRaceCarEntityIDs[8];   // +43584 (4 * 8 = 32; ends 43616)

        // ⭐ The 128 bytes at +43616..+43744 are the DWARF's two ResourceHandle arrays
        // (`ResourceHandle[8] maRaceCarModelHandles` then `ResourceHandle[8]
        // maRaceCarGraphicsModelHandles`, BrnVehicleManager.h:824/825). They fill the gap exactly at
        // **8 bytes per handle** -- 43616 + 64 = 43680, + 64 = 43744 -- which is the independent
        // confirmation that ResourceHandle is 8 bytes here. Modelled as an opaque span because
        // CgsResource::ResourceHandle has no committed home in this tree yet and Construct does not
        // touch either array; the two names + the 8-byte width are recorded so the next wave can
        // split it without re-deriving anything. DELETE-WHEN ResourceHandle lands.
        unsigned char        mPadAA60[43744 - 43616];  // maRaceCarModelHandles / maRaceCarGraphicsModelHandles

        // ⭐ CORRECTED 2026-08-03: this was committed as `EntityId
        // maAggressiveDrivingVictimEntityId[8]` at **stride 4**, which left 32 bytes of the span
        // unaccounted and would have mis-seated every element. It is the DWARF's
        // `RigidBodyId[8] maRaceCarHandlingBodyIDs` at **stride 8**:
        //   0x8263BE78/0x8263BE88  addis r26,r31,1 ; addi r26,r26,-0x5520  -> this + 43744
        //   0x8263BF44/0x8263BF48  ld r11, qword_82F2A3A8 ; std r11, 0(r26)   <- an 8-BYTE store
        //   0x8263BF78             addi r26, r26, 8                          <- stride 8, x8
        // 43744 + 64 == 43808, which is exactly where maRaceCarCrashes starts. RigidBodyId is
        // modelled as u64 (the sentinel it is seeded with, qword_82F2A3A8, is a 64-bit value --
        // the same K_INVALID_RIGID_BODY_ID idiom CgsPhysicsSimulationModule.h already names).
        u64                  maRaceCarHandlingBodyIDs[8]; // +43744 (8 * 8 = 64; ends 43808)

        // The crash-data pool @ +43808 (32 * 12 = 384; ends at 44192, abutting maeRaceCarTypes).
        // Spelling per the DWARF (`RaceCarCrashData[32] maRaceCarCrashes`).
        RaceCarCrashData     maRaceCarCrashes[32];  // +43808

        // ⭐ CORRECTED 2026-08-03: this was committed as `s32 maRaceCarCrashState[8]` with the note
        // "sentinel 2 == fatal crash state". It is the DWARF's `BrnWorld::ERaceCarType[8]
        // maeRaceCarTypes`, and the comparisons in the bodies are TYPE tests, not crash-state tests:
        //   Construct: `stw r24, 0(r28)` with r24 == 3 and r28 == this + 44192, stride 4, x8
        //              -- i.e. every slot is seeded E_RACE_CAR_TYPE_INACTIVE (== 3).
        //   the classifiers' `== 1`   is E_RACE_CAR_TYPE_AI      ("both cars are AI")
        //   SetRaceCarCrashing's `!= 2` is != E_RACE_CAR_TYPE_NETWORK ("not a network car")
        // The literals are numerically unchanged, so this is a NAMING correction with no behaviour
        // change -- but the old name made every read of it mean the wrong thing.
        BrnWorld::ERaceCarType maeRaceCarTypes[8];  // +44192   (4 * 8 = 32; ends at 44224)

        // The live-car bitset and the crash-data free-list, given their REAL CgsBitArray type so the
        // bodies use the container's named ops (IsBitSet/GetFirstNonZeroBit/SetBit) instead of raw
        // offset access. Each BitArray<N<=64> is a single 8-byte u64 field == the same image the X360
        // scans. Both offsets are asm-literal (Construct: `addis r11,r31,1; addi r11,r11,-0x5340`
        // -> 44224 and `addi r10,r10,-0x5338` -> 44232, each `std 0`). Names per the DWARF
        // (`mUsedRaceCars`, `BitArray<32u> mUsedRaceCarCrashesList`).
        CgsContainers::BitArray<8>  mUsedRaceCars;             // +44224 (live-car bitset)
        CgsContainers::BitArray<32> mUsedRaceCarCrashesList;   // +44232 (crash-data free-list)

        // ⭐ NEWLY PINNED: the contained StuntOffencesManager subobject. Construct calls
        // `StuntOffencesManager::Construct(this + 65536 - 0x5330)` == this + 44240 @0x8263C620, and
        // the next pinned member (mHiddenRaceCars) is at 44704, so the subobject occupies exactly
        // 464 bytes.
        //
        // ⭐⭐ TYPED 2026-08-03 (was `unsigned char mStuntOffencesManager[464]`). This is the ONE
        // contained sub-object of VehicleManager whose real reconstructed class FITS ITS X360 SPAN
        // ON x64 -- measured, not assumed:
        //     sizeof(BrnPhysics::StuntOffencesManager)  == 464   ==  44704 - 44240   ✅
        //     alignof(BrnPhysics::StuntOffencesManager) == 16,  and 44240 % 16 == 0  ✅
        // and it fits because the class contains NO POINTER MEMBERS at all: its last member ends at
        // 0x1C4 == 452, which its own header pins, and 452 rounds to 464 at align 16 on both ISAs.
        // Nothing in this class moves as a result -- the layout gate in
        // BrnVehicleManager_layout_check.cpp is what proves that, and it is compiled.
        //
        // WHY IT IS WORTH TYPING: `VehicleManager::Construct` has to call
        // `StuntOffencesManager::Construct(this + 44240)`. With an opaque byte array that call can
        // only be spelled as a reinterpret_cast off a raw offset, which is the offset-poke this
        // project forbids. Typed, it is `mStuntOffencesManager.Construct()` -- by name. The other
        // four contained sub-objects CANNOT be typed today; see the measured blocker table in the
        // Construct note above.
        BrnPhysics::StuntOffencesManager mStuntOffencesManager;      // +44240 (464 bytes)

        // ⭐ NEWLY PINNED / RENAMED: FOUR RaceCarBitArrays, not one. Construct zero-stores all four
        // back to back (0x8263C0A8..0x8263C0C0), and the DWARF lists exactly these four names in
        // this order (BrnVehicleManager.h:838-841):
        //   addi r11,r11,-0x5160 -> 44704   mHiddenRaceCars          (was mHiddenNetworkRaceCars)
        //   addi r9, r9, -0x5158 -> 44712   mRaceCarsAddedForCollision
        //   addi r8, r8, -0x5150 -> 44720   mNetworkCarsAddedForCollisionThisFrame
        //   addi r10,r10,-0x5148 -> 44728   mNetworkCarsRecievedFirstUpdate   (DWARF's spelling)
        // The committed header modelled 44712..44736 as padding, so three real bitsets were
        // invisible.
        CgsContainers::BitArray<8> mHiddenRaceCars;                        // +44704
        CgsContainers::BitArray<8> mRaceCarsAddedForCollision;             // +44712
        CgsContainers::BitArray<8> mNetworkCarsAddedForCollisionThisFrame; // +44720
        CgsContainers::BitArray<8> mNetworkCarsRecievedFirstUpdate;        // +44728
        // Per-car "hide for at least N frames" countdown @ +44736. Stride 4 (asm: `stw r30, 0x220(r28)`
        // off the stride-4 cursor at 44192 -> 44192 + 544 == 44736); SetNetworkRaceCarHidden stores
        // the requested frame count here. DWARF name (`uint32_t[8] mauNetworkCarHiddenFramesRemaining`).
        u32                  mauNetworkCarHiddenFramesRemaining[8];        // +44736 (ends 44768)

        // ==========================================================================================
        // ⭐⭐ UN-PINNED 2026-08-03 -- THE CONTAINED PhysicalTrafficManager IS NOW A REAL NAMED
        //     MEMBER, and the two members that used to poke through it as "siblings" are gone.
        //
        // WHAT IT WAS: `unsigned char mPadAEE0[148128 - 44768]` (103360 bytes) followed by
        //     `EntityId maRaceCarEntityIdRemap[8]` @+148128 and
        //     `unsigned char mau8GlobalToPhysicalEntityIndexMap[600]` @+149456, on the theory that
        //     "the X360 build folds every contained-manager member the VehicleManager methods touch
        //     to its absolute class offset".
        //
        // ⛔ THAT MODEL WAS SELF-CONTRADICTORY, and it is what produced the "+2480 overrun" verdict
        //     that has blocked VehicleManager::Construct. It declared the manager's span to END at
        //     +148128 while, four lines later, correctly identifying +149456 as the manager's own
        //     mu8GlobalToPhysicalEntityIndexMap "== 44768 + 104688" -- i.e. 2128 bytes PAST the end
        //     of the span it had just declared. The span was short by 2288 bytes.
        //
        // ⭐ THE REAL X360 SIZE IS 105648 (KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER), derived twice
        //     over in BrnPhysicalTrafficManager.h finding (4): once forward from ten asm-literal
        //     anchors inside PhysicalTrafficManager::Construct @0x82636CA8, and once BACKWARD from
        //     this class -- 44768 + S + 128*sizeof(PotentialContact) + 4, padded to 16, must equal
        //     the asm-pinned mDiscardedContacts at +160672, which forces S == 105648 uniquely.
        //     Host sizeof is 105840 (measured), so the real overrun is **+192, not +2480**.
        //
        // ⭐ AND THE TWO "SIBLINGS" WERE MIS-IDENTIFIED MEMBERS OF THIS OBJECT:
        //     +148128 == 44768 + 103360 == mPhysicalTrafficManager.maTrafficEntityIDs -- proven by
        //       PhysicalTrafficManager::Construct's own `addi r8,r11,0x64F0 ; slwi r9,r8,2 ;
        //       stwx -1,r9,r31` loop, which seeds 4*(i+25840) == 4i+103360 for i<20. The committed
        //       name `maRaceCarEntityIdRemap` and its [8] bound were both wrong: the reader is
        //       SetRaceCarCrashing's owner==2 (TRAFFIC_VEHICLE) branch, so the index is a TRAFFIC
        //       index in [0,20) and the array is EntityId[20]. Reading it as an 8-element race-car
        //       table was an out-of-bounds read for any traffic slot >= 8 -- offsets right, meaning
        //       wrong, the usual shape.
        //     +149456 == 44768 + 104688 == mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap
        //       (the committed header already said so; it just could not act on it).
        //
        // ⇒ Both readers now go through the named members (VehicleManager is a friend of
        //   PhysicalTrafficManager precisely so those two BARE loads stay bare -- routing them
        //   through the accessors would add asserts the console does not fire there), and
        //   `PhysicalTrafficManager::Construct(this + 44768)` is spelled
        //   `mPhysicalTrafficManager.Construct();` BY NAME. One of the three Construct blockers is
        //   gone. (The other two -- VehicleManagerDebugComponent +32 and the RaceCarVehicleRecord /
        //   VehiclePhysics mismatch -- are unchanged; see the blocker table above.)
        //
        // ⚠️ THE PRICE, PAID EXPLICITLY: every member after this one now sits
        //    KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER (192) bytes later on the host than on the X360.
        //    That is carried, not hidden: the three _AssertLayout* functions add the constant to
        //    every downstream offsetof, so they still fail if any padding run is wrong, and the
        //    constant itself is tripwired against sizeof(PhysicalTrafficManager) below.
        //    ⛔ Do NOT "absorb" the 192 by shrinking mPadNonPhysicalContacts. That span is the
        //    DWARF's maNonPhysicalContacts[128] + miNonPhysicalContactCount and its size is derived,
        //    not spare; shrinking it to buy back the X360 offsets would be inventing layout -- the
        //    same trap the maRaceCarDebugComponent note below warns about.
        // ==========================================================================================
        PhysicalTrafficManager mPhysicalTrafficManager;   // +44768 (X360 105648; host 105840)

        // ⭐⭐ PROMOTED 2026-08-06 (big-five #2, contact-generation wave) -- the old
        // `mPadNonPhysicalContacts[10256]` opaque span's own DELETE-WHEN clause has come due:
        // CgsSceneManager::SceneManagerIO::PotentialContact has a committed home
        // (SharedIO/CgsPotentialContact.h, 80 bytes host == 80 bytes console), so this is now
        // the DWARF's real pair -- `PotentialContact[128] maNonPhysicalContacts` (:850) +
        // `int32_t miNonPhysicalContactCount` (:851). X360 span 150416..160672 == 128*80 + 4 +
        // 12 tail pad up to mDiscardedContacts' 16-alignment; the host reproduces it exactly
        // (static_assert in _AssertLayoutTuningBank's TU is NOT extended -- the span sits before
        // the tuning bank; the count seat is proven by BridgeContactsToSimulation @0x825A9A50's
        // opening `stw 0` at module+179760 == this+160656 == 150416 + 128*80).
        CgsSceneManager::SceneManagerIO::PotentialContact maNonPhysicalContacts[128];  // +150416 :850
        s32                  miNonPhysicalContactCount;                                // +160656 :851
        unsigned char        mPadAfterNonPhysicalContacts[160672 - 160660];            // 12 tail-pad bytes

        // ⭐ NEWLY PINNED: the discarded-contact queue. Construct binds it in place @0x8263C048:
        //   addis r29,r31,2 ; addi r29,r29,0x73A0   -> this + 160672
        //   addi  r28,r29,0x10                      -> the buffer, this + 160688
        //   stw r28,0(r29) ; stw 0x14,4(r29) ; stw 0,8(r29)   -> {buffer, capacity 20, count 0}
        // plus the console's own `lpEventBuffer != NULL` assert (CgsBaseEventQueue.h:160). The
        // 16-byte header + 20 entries fills exactly to mDebugComponent, so each entry is 64 bytes.
        // DWARF: `ContactSpyData::DiscardedContactQueue mDiscardedContacts` (BrnVehicleManager.h:854).
        //
        // ⭐⭐ TYPED 2026-08-03 (was `unsigned char mDiscardedContacts[1296]`), and it fits for a
        // reason worth writing down: the X360 header is {T* @0, s32 maxLen @4, s32 len @8} == 12
        // bytes rounded to 16 by DiscardedContact's alignment. On x64 the pointer widens to 8, and
        // 8 + 4 + 4 == 16 -- the widening lands exactly in the padding the X360 already had. So
        //     sizeof(CgsModule::EventQueue<ContactSpy::DiscardedContact, 20>) == 1296 == the span,
        // MEASURED, and 16 + 20*64 == 1296 confirms the 64-byte entry the span implied.
        // ⇒ Construct's three-store bind is spelled `mDiscardedContacts.Construct();` BY NAME --
        // BaseEventQueue<T>::Construct sets {mpEvents = maEvents, miMaxLength = 20, miLength = 0}
        // and fires the same `lpEventBuffer != NULL` assert the console does.
        // The 20 is the DWARF/asm capacity (`stw 0x14, 4(r29)`), spelled in BrnContactSpyData.h as
        // ContactSpyData::KI_MAX_DISCARDED_CONTACTS; the canonical spelling of this whole type is
        // that header's `ContactSpyData::DiscardedContactQueue` typedef. It is written out longhand
        // here so this header keeps its light include set (BrnContactSpyEvents.h was already in).
        CgsModule::EventQueue<BrnPhysics::ContactSpy::DiscardedContact, 20>
                             mDiscardedContacts;                  // +160672 (1296 bytes)

        // ⭐ The manager's own debug component. Construct calls
        // `VehicleManagerDebugComponent::Construct(this + 161968, this)` @0x8263BCD8 -- note it takes
        // TWO arguments (r3 = the component, r4 = r31 = the manager); Hex-Rays renders it with none.
        //
        // ⭐⭐ UN-PINNED 2026-08-03 (was `unsigned char mDebugComponent[163264 - 161968]`). This was
        // the second of the three Construct blockers: real host class 1328 vs the 1296-byte X360
        // span, "⛔ +32". Both the old header and BrnVehicleManagerDebugComponent.h's own banner
        // concluded from that that "VehicleManager keeps its own mDebugComponent as the X360-sized
        // 1296-byte opaque span so its byte-pinned offsetof chain stays intact". That conclusion
        // followed only from the byte-pinning, and the byte-pinning is what this wave retired: with
        // KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT the chain stays intact WITH the real type in place.
        // ⚠️ Note the 1296 span itself was NEVER wrong (unlike the traffic manager's 103360) -- both
        // of its ends are asm-literal. The +32 is a genuine host/console width difference and it is
        // carried, not argued away.
        // ⇒ `VehicleManagerDebugComponent::Construct(this + 161968, this)` is now spelled
        //   `mDebugComponent.Construct(this);` BY NAME.
        VehicleManagerDebugComponent mDebugComponent;               // +161968 (X360 1296; host 1328)

        // ⭐ NEWLY PINNED: the per-car debug components. Construct walks them in the 8-car loop with
        // `addis r27,r31,2 ; addi r27,r27,0x7DC0` -> this + 163264 and `addi r27,r27,0x400`
        // (stride 1024), storing each into the matching maRaceCarVehicles[] record and firing the
        // console's own `lpDebugComponent != NULL` assert. DWARF:
        // `BrnPhysics::Vehicle::DebugComponent[8] maRaceCarDebugComponent` (BrnVehicleManager.h:860),
        // immediately followed by `bool[8] mabRaceCarDebugComponentRegistered` (:861).
        // ⭐ THE CHAIN CLOSES TO THE BYTE: 163264 + 8*1024 == 171456, + 8 == 171464, which is the
        // independently asm-proven offset of the gate byte below. Four numbers, one closure.
        unsigned char        maRaceCarDebugComponent[8][1024];        // +163264 (ends 171456)
        bool                 mabRaceCarDebugComponentRegistered[8];   // +171456 (ends 171464)

        // ==========================================================================================
        // ⭐⭐ THE TUNING BANK, RESOLVED 2026-08-03. Everything from +171464 to +172616 below is now
        // DWARF-NAMED and, where `VehicleManager::Construct` @0x8263B7C8 seeds it, carries its
        // DEFAULT CONSTANT in the trailing comment. This span used to be 17 `mPad*` runs with 23
        // role-guessed members poking through; it is now 100% declared.
        //
        // HOW IT WAS PROVEN -- three independent readings, zero conflicts:
        //   1. the X360 ASM, symbolically simulated (GPR/FPR/stack-spill) -> seat -> flt_XXXXXXXX
        //   2. the X360 HEX-RAYS for the same function -> seat -> literal value
        //   3. the PS3 DecFIGS build's `VehicleManager::Construct` @0x6EB6BC -> seat -> literal,
        //      at a uniform shift of Δ = 672 (X360 offset == PS3 offset + 672)
        // Every one of the 33 distinct rodata symbols yields the SAME literal at every seat it
        // feeds, and every seat below matches the PS3 build at Δ=672. Source 3 also supplied the one
        // value X360 Hex-Rays elided into a register (mfTailgatingVunerabilityTime).
        //
        // WHY THE NAMES ARE TRUSTWORTHY: the DWARF member ORDER
        // (references/DecFIGS/dwarfdump/.../BrnVehicleManager.h:865-1088) lays onto the asm seats and
        // closes on SIX independent anchors -- the gate byte @171464; 171468 + 44*4 == 171644;
        // mPlayerWonImpact 8-aligned @171736; mPlayerAiDriver @171968; mePlayerActiveRaceCarIndex
        // @172204 with the six-float run @172208..172228 landing mCameraMatrix on 172240; and
        // mpCachedCarA/B + mbCachedCarCarPredictionResult + the 16-aligned prediction normal
        // @172416/172420/172424/172432. The store OPCODES corroborate the types independently
        // (stfsx at 172320/172324 for the two float32_t; stwx at 172328..172344 for the four
        // int32_t + the enum).
        //
        // ⚠️⚠️ SEVEN COMMITTED ROLE-GUESSES WERE REFUTED. The offsets were all correct, so no byte
        // ever moved -- but the bodies that read them were reasoning about the wrong thing. Each is
        // called out at its member below with "was <old name>". The load-bearing one:
        // `miAttackerToRecord` -> `mfMinSecondsBetweenImpacts` and `maRaceCarLastAttacker[8]` ->
        // `mafNoImpactTimeSeconds[8]`; the copy between them is ARMING A PER-CAR IMPACT COOLDOWN,
        // not recording an attacker index. Both ends were mis-named consistently, which is exactly
        // why it never looked wrong -- and `HasRaceCarHadRecentImpact` had already noticed the X360
        // reads that slot AS A FLOAT and worked around it with a memcpy.
        // ==========================================================================================

        // ---- the two master gates (DWARF :865/:866; both asm-proven, both seeded TRUE) -----------
        bool                 mbSlamsAndShuntsOn;                    // +171464 = true   [was mbTakedownsEnabled]
        bool                 mbAllowSlamsAndShuntsEffectsForRivals; // +171465 = true   [was mbSlamShuntPhysicsEnabled]
        unsigned char        mPad29DCA[171468 - (171465 + 1)];      // 2 bytes, 4-align

        // ---- the 44-float tuning run (DWARF :868..:920). CONTIGUOUS: 171468 + 44*4 == 171644,
        //      which is exactly where maeImpactType starts -- the run closes with no gaps. Every
        //      default below is agreed by all three sources. ------------------------------------
        f32 mfFrontRaySensorLength;                 // +171468 = 4.0f
        f32 mfFrontRayLength;                       // +171472 = 1.5f
        f32 mfRearRayLength;                        // +171476 = 1.5f
        f32 mfPlayerShuntScale;                     // +171480 = 0.325f
        f32 mfAIShuntScale;                         // +171484 = 0.2f
        f32 mfShuntDecay;                           // +171488 = 0.15f
        f32 mfVulnerabilityFactorMax;               // +171492 = 4.0f
        f32 mfPlayerVulnerabilityDurationSeconds;   // +171496 = 2.0f
        f32 mfAIVulnerabilityDurationSeconds;       // +171500 = 4.0f
        f32 mfMinSteeringOverrideTimeSlam;          // +171504 = 0.2f
        f32 mfMinSteeringOverrideTimeShunt;         // +171508 = 0.2f
        f32 mfPlayerMaxSteeringOverrideTimeSlam;    // +171512 = 0.7f
        f32 mfAIMaxSteeringOverrideTimeSlam;        // +171516 = 0.9f
        f32 mfPlayerMaxSteeringOverrideTimeShunt;   // +171520 = 0.4f
        f32 mfAIMaxSteeringOverrideTimeShunt;       // +171524 = 0.7f
        f32 mfPlayerSlamForceScale;                 // +171528 = 0.25f
        f32 mfAISlamForceScale;                     // +171532 = 0.25f
        f32 mfMaxSlamClosingXSpeed;                 // +171536 = 16.0f
        // ⚠️ REFUTED ROLE: committed as `s32 miAttackerToRecord`. It is a FLOAT cooldown, and the
        // body that copies it into mafNoImpactTimeSeconds[victim] is starting that car's timer.
        f32 mfMinSecondsBetweenImpacts;             // +171540 = 0.3f   [was miAttackerToRecord (s32)]
        f32 mfMinAmountOfSlamForce;                 // +171544 = 0.2f
        f32 mfMinAmountOfShuntForce;                // +171548 = 0.25f
        // ⭐ The one value X360 Hex-Rays carried in a register; PS3 (170880) gives it literally, and
        // flt_82001C98 is the same slot this function uses for the mCameraMatrix identity diagonal.
        f32 mfTailgatingVunerabilityTime;           // +171552 = 1.0f
        f32 mfBaseSlamMagnitude;                    // +171556 = 3.0f
        f32 mfBaseShuntMagnitude;                   // +171560 = 22.5f
        f32 mfTBoneTakedownMaxAngle;                // +171564 = 35.0f  [was mfTBoneAngleBandDegrees]
        // ⚠️ REFUTED ROLE: committed as `mfTBoneSidePlaneHalfWidth`. It is a SPEED, not a width.
        f32 mfTBoneTakedownSpeed;                   // +171568 = 30.0f  [was mfTBoneSidePlaneHalfWidth]
        f32 mfMaxShuntAngle;                        // +171572 = 25.0f
        f32 mfMinNudgeSpeed;                        // +171576 = 8.0f
        // ⚠️ REFUTED ROLE: committed as `mfNudgeMaxClosingSpeed` / `mfShuntMaxClosingSpeed`. They are
        // the shunt MINIMUM and the FATAL shunt speed -- opposite ends of the scale from the guess.
        f32 mfMinShuntSpeed;                        // +171580 = 12.0f  [was mfNudgeMaxClosingSpeed]
        f32 mfFatalShuntSpeed;                      // +171584 = 140.0f [was mfShuntMaxClosingSpeed]
        f32 mfSlamDecayRate;                        // +171588 = 0.13f
        f32 mfSlamEffectMinMagnitude;               // +171592 = 0.4f
        f32 mfSlamEffectMaxMagnitude;               // +171596 = 2.0f
        f32 mfMinShuntMagnitude;                    // +171600 = 0.2f
        f32 mfMaxShuntMagnitude;                    // +171604 = 0.4f
        f32 mfMinShuntBackwardsMagnitude;           // +171608 = 0.3f
        f32 mfMaxShuntBackwardsMagnitude;           // +171612 = 0.75f
        f32 mfMinTradingPaintSpeed;                 // +171616 = 0.8f   [was mfTradingPaintMinSpeed]
        // ⚠️ REFUTED ROLE: committed as `mfTradingPaintMaxSpeed`; it is the FATAL SLAM speed (and it
        // shares its value, 140.0f, with mfFatalShuntSpeed -- which is what made the guess plausible).
        f32 mfFatalSlamSpeed;                       // +171620 = 140.0f [was mfTradingPaintMaxSpeed]
        f32 mfFatalHitCrashingCarSpeed;             // +171624 = 50.0f
        f32 mfMaxHeadToHeadAngle;                   // +171628 = 45.0f  [was mfHeadToHeadAngleToleranceDeg]
        f32 mfMinHeadToHeadSpeed;                   // +171632 = 40.0f
        f32 mfMinHeadToHeadIndividualSpeed;         // +171636 = 40.0f  [was mfHeadToHeadMinClosingSpeed]
        f32 mfAngleForVerticleTakedown;             // +171640 = 60.0f  (DWARF's spelling)

        // ---- per-car impact bookkeeping (DWARF :923..:926) ---------------------------------------
        // ⚠️ REFUTED ROLE: committed as `f32 maRaceCarLastImpactMagnitude[8]`. It is the per-car
        // IMPACT TYPE enum array; the value written into it is the classifier's result, not a
        // magnitude. Same 32 bytes, same seats.
        EImpactType          maeImpactType[8];              // +171644 (32; ends 171676) [was maRaceCarLastImpactMagnitude]
        // ⚠️ REFUTED ROLE: committed as `maRaceCarTakenDownThisFrame[8]`; it is the per-car impact SCORE.
        unsigned char        mauImpactScore[8];             // +171676 (8;  ends 171684) [was maRaceCarTakenDownThisFrame]
        // ⚠️ REFUTED ROLE: committed as `s32 maRaceCarLastAttacker[8]`. It is a FLOAT per-car
        // "seconds until this car may be impacted again" countdown, seeded from
        // mfMinSecondsBetweenImpacts. HasRaceCarHadRecentImpact tests it > 0.0f -- which is why that
        // body already had to memcpy the s32 through to a float to stay byte-faithful.
        f32                  mafNoImpactTimeSeconds[8];     // +171684 (32; ends 171716) [was maRaceCarLastAttacker]
        signed char          maiPhysicsSlamIndex[8];        // +171716 (8;  ends 171724)
        f32                  mfContactDisplaySeconds;       // +171724
        EImpactType          meDisplayImpactType;           // +171728
        bool                 mbPlayerWonDisplayImpact;      // +171732
        unsigned char        mPad29F95[171736 - (171732 + 1)]; // 3 bytes; BitArray is 8-aligned
        // ⚠️ REFUTED ROLE: committed as `mTakenDownRaceCarsBitArray`. DWARF :934 calls it
        // mPlayerWonImpact -- a per-car "the player won this impact" bitset, not a taken-down set.
        CgsContainers::BitArray<8> mPlayerWonImpact;        // +171736 (8; ends 171744) [was mTakenDownRaceCarsBitArray]

        // ---- the per-car vulnerability / grinding arrays (DWARF :937..:951) -----------------------
        // ⚠️⚠️ REFUTED SHAPE, not just a name: the committed header declared SCALARS
        // `mfGrindingThresholdA` @171868 and `mfGrindingThresholdB` @171900. Those two addresses are
        // ELEMENT 7 of the two per-car grinding-duration arrays below (171840 + 7*4 == 171868;
        // 171872 + 7*4 == 171900). The grind pre-pass in this class's .cpp therefore reads car
        // index 7's durations at a hard-coded offset. Left as-is and FLAGGED rather than "fixed":
        // per the standing rule an asm-derived index is never changed to match a label -- the wave
        // that re-reads CheckForGrindingAndRubbing against its own asm owns that call.
        f32                  mafVulnerableTimeSeconds[8];              // +171744 (ends 171776)
        f32                  mafVulnerabilityFactor[8];                // +171776 (ends 171808)
        f32                  mafTotalVulnerableTime[8];                // +171808 (ends 171840)
        f32                  mafPlayerGrindingOtherDurationSeconds[8]; // +171840 (ends 171872) [7] == old mfGrindingThresholdA
        f32                  mafOtherGrindingPlayerDurationSeconds[8]; // +171872 (ends 171904) [7] == old mfGrindingThresholdB
        f32                  mafRubbingDurationSeconds[8];             // +171904 (ends 171936)
        unsigned char        mau8FramesSincePlayerGrindingOther[8];    // +171936 (ends 171944)
        unsigned char        mau8FramesSinceOtherGrindingPlayer[8];    // +171944 (ends 171952)
        bool                 mabRubbingThisUpdate[8];                  // +171952 (ends 171960)
        unsigned char        mPad29FF8[171968 - 171960];               // 8 bytes; VehicleDriver is 16-aligned

        // ⭐ The manager's own spare AI driver. Construct calls
        // `VehicleDriver::Construct(this + 3*65536 - 0x6040)` == this + 171968 @0x8263C088 -- the
        // SECOND VehicleDriver::Construct call in the function, the first being the 8-car array at
        // +64. DWARF: `VehicleDriver mPlayerAiDriver` (BrnVehicleManager.h:953).
        VehicleDriver        mPlayerAiDriver;         // +171968 (224; ends 172192)

        // DWARF :954-956 -- the run that closes 172192 onto the asm-proven mePlayerActiveRaceCarIndex.
        // Now DECLARED (was opaque): the run has to be exactly bool + f32 + f32 for 172192 + 12 to
        // land on 172204, and no other DWARF member sits between them.
        bool                 mbPlayerAiDriverValid;      // +172192
        unsigned char        mPad2A041[172196 - (172192 + 1)];
        f32                  mfPlayerRecentSteering;     // +172196
        f32                  mfSteeringUpdateRemainder;  // +172200

        // The local player's active-race-car slot @ +172204. DWARF-attested name (BrnVehicleManager.h:959).
        EActiveRaceCarIndex  mePlayerActiveRaceCarIndex; // +172204 (ends 172208)

        // ---- the six world/traffic crash thresholds (DWARF :962..:968). Previously left opaque
        //      because the placement came from DWARF order only; Construct writes all six with
        //      `stfsx` at exactly these seats, and the PS3 build agrees at Δ=672. -----------------
        f32 mfCrashingAICollisionCrashThresholdMPH; // +172208 = 50.0f
        f32 mfHeadOnWorldCrashThreshold;            // +172212 = 40.5f
        f32 mfSideOnWorldCrashThreshold;            // +172216 = 50.0f
        f32 mfTrafficCollisionCheckThresholdMPH;    // +172220 = 30.0f
        f32 mfMinRCTrafficTranslateSpeedMPH;        // +172224 = 40.0f
        f32 mfVerticalTakedownAngleDeg;             // +172228 = 65.0f
        unsigned char        mPad2A074[172240 - (172228 + 4)];  // 8 bytes; Matrix44Affine is 16-aligned

        // ⭐ The camera matrix Construct stamps with the identity. Asm @0x8263C068:
        //   addis r11,r31,3 ; addi r11,r11,-0x5F30   -> this + 172240
        //   stvx128 v0,r0,r11 / v13,r11,0x10 / v12,r11,0x20 / v11,r11,0x30
        // -- four 16-byte lanes built on the stack from flt_82001C98 (1.0f) and flt_82001CC0 (0.0f).
        // DWARF: `Matrix44Affine mCameraMatrix` (BrnVehicleManager.h:970).
        Matrix44Affine       mCameraMatrix;           // +172240 (64; ends 172304)

        // ---- the 16 gameplay/debug bools (DWARF :972..:988). Construct seeds 12 of them; the four
        //      it leaves alone are marked "(not seeded)". ----------------------------------------
        bool mbImpactTime;                          // +172304 = false
        bool mbEasyCrashingEnabled;                 // +172305   (not seeded)
        bool mbStopPlayerCrashing;                  // +172306 = false  [was mbSuppressPlayerCrash]
        // ⚠️ REFUTED ROLE: committed as `mbSuppressIfAlreadyCrashState1`; it is the AI twin of the
        // byte above, nothing to do with a crash state value.
        bool mbStopAICrashing;                      // +172307 = false  [was mbSuppressIfAlreadyCrashState1]
        bool mbCrashOnHandbrakeTurn;                // +172308 = false
        bool mbCrashPlayerNextUpdate;               // +172309 = false
        bool DEBUG_mbAlwaysCrashRaceCarToRaceCar;   // +172310   (not seeded)
        bool DEBUG_mbHornTakedownEnabled;           // +172311   (not seeded)  [was mbHornTakedownEnabled]
        bool mbDebugModifyTrafficContacts;          // +172312   (not seeded)
        bool mbTrafficCheckingAllowed;              // +172313 = true
        bool mbAftertouchIsForceAdditive;           // +172314 = false
        // ⚠️ REFUTED ROLE: committed as `mbStationaryTakedownsEnabled`. It is the online-mode flag.
        bool mbIsOnlineGameMode;                    // +172315 = false  [was mbStationaryTakedownsEnabled]
        bool mbUpdatedPlayerDriver;                 // +172316   (not seeded)
        bool mbForceNoSlowMo;                       // +172317   (not seeded)
        bool mbInOnlineGameModeStartLine;           // +172318 = false
        bool mbPlayerCarInJunkYard;                 // +172319 = false

        // ---- the player/car stat block (DWARF :993..:1007) ---------------------------------------
        // ⚠️⚠️ REFUTED TYPE: the committed header modelled +172328..+172348 as `f32
        // maPlayerCarStats[5]`. It is four `int32_t` plus a `BrnResource::ECarType`, and Construct
        // proves it at the opcode level -- the two seats above are written with `stfsx` and these
        // five with `stwx`. Same 20 bytes; the accessors were reading integers as floats.
        f32 mfPlayerStatStrength;                   // +172320 = 0.0f  [was mfShowtimePlayerCarStrength]
        f32 mfPlayerStatDamageLimit;                // +172324 = 0.0f  [was mfShowtimePlayerCarDamageLimit]
        s32 miCarSpeed;                             // +172328 = 0     [was maPlayerCarStats[0]]
        s32 miCarStrength;                          // +172332 = 0     [was maPlayerCarStats[1]]
        s32 miCarControl;                           // +172336 = 0     [was maPlayerCarStats[2]]
        s32 miCarBoost;                             // +172340 = 0     [was maPlayerCarStats[3]]
        // FLAG: BrnResource::ECarType has no committed home; kept as s32 (Construct seeds 3).
        s32 meCarType;                              // +172344 = 3     [was maPlayerCarStats[4]]
        s32 miPlayerSpeed;                          // +172348   (not seeded)
        s32 miPlayerStrength;                       // +172352   (not seeded)
        s32 miPlayerControl;                        // +172356   (not seeded)
        s32 miPlayerBoost;                          // +172360   (not seeded; ends 172364)

        // ⭐ CARVED 2026-08-06 (UpdateVehiclePhysics wave). +172364..+172380 is the DWARF's
        // `Time mCurrentTime` / `Time mStartModeTime` pair (:1010/:1011). The old note left the
        // span OPAQUE because "nothing pins sizeof(Time) here" -- UpdateVehiclePhysics
        // @0x8264514C..0x82645170 now pins it: `stw lrCurrentTime+0 -> this+172364` then
        // `lfs/stfs lrCurrentTime+4 -> this+172368`, i.e. Time == {s32 miSeconds, f32 mfFraction}
        // == 8 bytes, exactly the committed CgsSystem::Time. Two 8-byte Times fill the 16-byte
        // span with zero slack; the next member stays asm-proven.
        CgsSystem::Time      mCurrentTime;     // +172364 (:1010)
        CgsSystem::Time      mStartModeTime;   // +172372 (:1011; never written by the recovered set)

        // FLAG: BrnGameState::GameStateModuleIO::EGameModeType has no committed home; kept as s32.
        // Construct seeds -1 ("no mode"), asm-proven (`stwx` of 0xFFFFFFFF).
        s32 meCurrentGameModeType;                  // +172380 = -1

        // ---- the eight car-stat strength scalars (DWARF :1015..:1023). All eight asm-proven and
        //      PS3-confirmed; note the Max/Min symmetry (2.0/0.5 slam, 2.0/0.05 shunt). -----------
        f32 mfCarStatStrengthSlamMax;               // +172384 = 2.0f
        f32 mfCarrStatStrengthSlamMin;              // +172388 = 0.5f   (DWARF's spelling: "Carr")
        f32 mfCarStatStrengthShuntMax;              // +172392 = 2.0f
        f32 mfCarrStatStrengthShuntMin;             // +172396 = 0.05f
        f32 mfCarStatStrengthBeingSlammedMax;       // +172400 = 2.0f
        f32 mfCarStatStrengthBeingSlammedMin;       // +172404 = 0.5f
        f32 mfCarStatStrengthBeingShuntedMax;       // +172408 = 2.0f
        f32 mfCarrStatStrengthBeingShuntedMin;      // +172412 = 0.05f

        // ⚠️ POINTER WIDTH. The DWARF types these two `const SimpleVehiclePhysics*`. They are
        // modelled as u32 slots so the 16-aligned mCachedCarCarPredictionNormal below keeps its
        // asm-proven +172432 seat on x64 -- two 8-byte pointers would push it to +172440 and silently
        // break the rest of the class. Construct only NULLs them; nothing in this tree dereferences
        // them yet. DELETE-WHEN a RaceCarPhysics/SimpleVehiclePhysics cache pass needs them live.
        u32 muCachedCarASlot;                       // +172416 = NULL  (DWARF: mpCachedCarA)
        u32 muCachedCarBSlot;                       // +172420 = NULL  (DWARF: mpCachedCarB)
        bool mbCachedCarCarPredictionResult;        // +172424 = false
        unsigned char        mPad2A189[172432 - (172424 + 1)];  // 7 bytes; Vector3 is 16-aligned
        // ⭐ NEWLY PINNED, and the reason the whole tail closes: Construct loads 16 bytes from
        // `unk_82181520` and stores them here (`stvx128 v0, r31, r9` @0x8263C48C, r9 == 172432).
        // unk_82181500/10/20 are the identity basis rows {1,0,0,0}/{0,1,0,0}/{0,0,1,0} -- already
        // settled in-repo twice (ICECameraSpaceHandler.cpp:124, BrnShadowMap.cpp:955/998). So the
        // cached car-vs-car prediction normal is seeded to the world +Z axis.
        Vector3 mCachedCarCarPredictionNormal;      // +172432 = {0,0,1,0}  (16; ends 172448)

        // FLAG: VehicleManager::EStationaryPlayerWheelAngle has no committed home; kept as s32.
        s32 meStationaryPlayerWheelAngle;           // +172448 = 2
        bool mbCrashRaceCarWhenFatal;               // +172452 = true
        unsigned char        mPad2A1A5[172456 - (172452 + 1)];
        // FLAG: BrnGameState::EShowtimeBehaviour has no committed home; kept as u32. Gated 0..2
        // against E_SHOWTIME_MODE_COUNT==3 by SetShowtimeBehaviour; Construct seeds 2.
        u32 meShowtimeBehaviour;                    // +172456 = 2     [was muShowtimeBehaviour]
        // ⭐ The 30th PerfMonCpu monitor handle -- the only one of the thirty that is stored INTO the
        // object rather than into a file-scope global. Named by the console's OWN assert text,
        // `"miRaceCarWorldContactValidationPM >= 0"` (BrnVehicleManager.cpp:778), which matches
        // DWARF :1041 exactly.
        s32 miRaceCarWorldContactValidationPM;      // +172460 = AddMonitor("PHYS ValidateRCWorldContact", ...)
        unsigned char mn8RoundRobinControlWord;     // +172464   (not seeded; DWARF :1042)

        // ⭐ CARVED 2026-08-06 (PhysicsModule::Update leaves wave): the HEAD of the old opaque
        // +172465..+172580 span is the DWARF's two contact-generation pointers (:1045/:1046), and
        // both seats are asm-literal in the per-frame chain:
        //   FreeAllocations @0x8261BAE0:            DestroyIOBuffer<CollisionGenerator>(&this+172472)
        //                                           then DestroyIOBuffer<ContactGenList>(&this+172468)
        //   StartVehicleTractionLineTests @0x82629CE0: `lwz` this+172472 + the baked assert
        //                                           "mpContactGenerator != NULL" (BrnVehicleManager.cpp:2346)
        // The DestroyIOBuffer bl target's mangled name pins mpContactGenerator's exact qualified
        // type: CgsSceneManager::CgsCollision::CollisionGenerator (NOT the `CgsPhysics::
        // CollisionGenerator` some sibling headers forward-declare -- that fork is flagged at those
        // headers' consumers, not here).
        //   * console: 3 pad bytes [172465..172468) + two 4-byte pointers [172468..172476)
        //   * host:    7 pad bytes (3 + 4 more so the 8-byte pointers seat on an 8 boundary) + 16
        // The +12 host growth is absorbed by the REMAINING opaque run below -- legal here, unlike
        // the drift-constant cases, because everything both sides of the carve inside this span is
        // UNRECONSTRUCTED padding, not a modelled DWARF member; the span's two ENDS
        // (mn8RoundRobinControlWord / miContactStreamCounterA) keep their pinned seats, which the
        // mounted layout gate (_AssertLayoutTuningBank) checks.
        unsigned char        mPad2A1D1[7];  // console [172465..172468) + 4B host pointer alignment
        BrnPhysics::ContactGenList*                        mpContactGenList;    // +172468 (DWARF :1045)
        CgsSceneManager::CgsCollision::CollisionGenerator* mpContactGenerator;  // +172472 (DWARF :1046)

        // ⭐⭐ CARVED IN FULL 2026-08-06 (big-five #2, contact-generation wave): the REST of the
        // DWARF's contact-generation block, :1047..:1076, every member BY NAME. The consumer that
        // forced the carve is StartVehicleContactGeneration @0x8262AEE8, whose store/load seats
        // walk this block end to end and match the DWARF names 1:1:
        //     +172476 mDetachedPartPrimPairBuilder    (Prepare'd cap 600; "Too many part Vs. Car
        //              contact tests" gate on its mu16NumTests @+172482)
        //     +172488 mDetachedWheelPrimPairBuilder   ("Too many wheel Vs. Car..." @+172494)
        //     +172500 mHingedPartVsVehiclePairBuilder (AddHingedBodyPartPairs' builder)
        //     +172512 mpTractionLineTestsJob          +172516 miFirstPartContactGenEntry (=0)
        //     +172520 mpSphereSphereStreamProducer    <- CreateCollideSphereListWithSphereList-
        //              Stream @0x82811A78 (and so on: each producer seat is filled by exactly the
        //              Create* whose DWARF name matches it)
        //     +172524 mpSphereTriangleStreamProducer  <- @0x828113C8
        //     +172528 mpSweptSphereTriangleStreamProducer <- @0x82811720
        //     +172532/+172536/+172540 the three *StreamJob seats <- the matching Run* results
        //     +172544 mOverlappingRaceCars            (BitArray<64>; the 8*idxA+idxB / 8*idxB+idxA
        //              symmetric pair marking, asm 0x8262B890/0x8262B970)
        //     +172552 mTrafficSimpleTrafficPrimPairBuilder  (gated on its mu16NumTests @+172558)
        //     +172564 mRaceCarSimpleTrafficPrimPairBuilder  (gate @+172570)
        //     +172576 mpBodyPartWithWorldStream
        // Host layout drifts (pointers widen, the builders are 16 host vs 12 console); the gate
        // below the class carries the block's own drift constant.
        CgsSceneManager::CgsCollision::PrimitivePairListBuilder mDetachedPartPrimPairBuilder;     // +172476 :1047
        CgsSceneManager::CgsCollision::PrimitivePairListBuilder mDetachedWheelPrimPairBuilder;    // +172488 :1048
        CgsSceneManager::CgsCollision::PrimitivePairListBuilder mHingedPartVsVehiclePairBuilder;  // +172500 :1049
        EA::Jobs::Job*                       mpTractionLineTestsJob;                              // +172512 :1050
        s32                                  miFirstPartContactGenEntry;                          // +172516 :1051
        CgsMemory::SimpleDataStreamProducer* mpSphereSphereStreamProducer;                        // +172520 :1052
        CgsMemory::SimpleDataStreamProducer* mpSphereTriangleStreamProducer;                      // +172524 :1053
        CgsMemory::SimpleDataStreamProducer* mpSweptSphereTriangleStreamProducer;                 // +172528 :1054
        EA::Jobs::Job*                       mpSphereSphereStreamJob;                             // +172532 :1055
        EA::Jobs::Job*                       mpSphereTriangleStreamJob;                           // +172536 :1056
        EA::Jobs::Job*                       mpSweptSphereTriangleStreamJob;                      // +172540 :1057
        CgsContainers::BitArray<64>          mOverlappingRaceCars;                                // +172544 :1060 (kiRaceCarPairs == 64)
        CgsSceneManager::CgsCollision::PrimitivePairListBuilder mTrafficSimpleTrafficPrimPairBuilder; // +172552 :1065
        CgsSceneManager::CgsCollision::PrimitivePairListBuilder mRaceCarSimpleTrafficPrimPairBuilder; // +172564 :1066
        CgsMemory::SimpleDataStreamProducer* mpBodyPartWithWorldStream;                           // +172576 :1069

        // ⭐ RENAMED AT CARVE (was miContactStreamCounterA/B, both FLAGGED role-neutral): with the
        // block carved the DWARF sequence pins the roles uniquely -- the counter at +172580 is
        // miNumTrafficSphereWorldTests (:1072) and the `stwx 0` seat at +172584 is the CONSOLE
        // POINTER mpTractionLineStreamProducer (:1075; the zero-store is a null-store), with
        // miNumSPUTractionLineTests (:1076) closing the run to mStuckInCollisionTestCacheSphere's
        // asm-pinned +172592 exactly.
        s32                                  miNumTrafficSphereWorldTests;                        // +172580 :1072 (=0; SVCG resets it too)
        CgsMemory::SimpleDataStreamProducer* mpTractionLineStreamProducer;                        // +172584 :1075 (=NULL)
        s32                                  miNumSPUTractionLineTests;                           // +172588 :1076
        // ⭐ NEWLY PINNED, and it closes the class tail to the byte: Construct zero-stores 16 bytes
        // here (`stvx128 v127, r31, r11` @0x8263C664, r11 == 172592) and a zero byte at 172608 --
        // exactly the DWARF's adjacent `Sphere mStuckInCollisionTestCacheSphere` (:1087) +
        // `bool mbPlayerCarStuckInCollision` (:1088) pair. Modelled as raw bytes because Sphere has
        // no committed home; the 16-byte size is what the single stvx128 proves.
        unsigned char        mStuckInCollisionTestCacheSphere[16];   // +172592 = {0,0,0,0} (ends 172608)
        bool                 mbPlayerCarStuckInCollision;            // +172608 = false
        unsigned char        mPad2A241[172612 - (172608 + 1)];
        // Per-frame takedown-event cap counter @ +172612 (throttled < 32). FLAG: name still proposed
        // -- this seat is past the end of the DWARF member list dumped for this class.
        u32                  muTakedownEventsThisFrame;        // +172612 = 0 (ends 172616)

        // Pin every recovered offset. Never called -- exists only so offsetof can see the private
        // members (offsetof on a private member needs member-function context). The gate FAILS if any
        // padding run is wrong, which is the intended signal.
        //
        // ⚠️⚠️ 2026-08-03: _AssertLayout and _AssertLayoutPlayerStats DO NOT CURRENTLY RUN. They are
        // defined in BrnVehicleManager.cpp / BrnVehicleManagerPlayerStats.cpp, and NEITHER TU is
        // mounted in tools/build/build_game_exe.bat -- so a static_assert in them is a comment, not
        // a gate, and a green build says nothing at all about this class's layout. That was true for
        // every wave that has touched this header. _AssertLayoutTuningBank below is the fix for the
        // span this wave resolved; it lives in its own mounted TU
        // (BrnVehicleManager_layout_check.cpp) precisely so that it is actually compiled.
        static void _AssertLayout();

        // As _AssertLayout, but pins the wave-10 player-stats / showtime / network / map members added
        // for the second .cpp (BrnVehicleManagerPlayerStats.cpp). Never called. NOT MOUNTED -- see above.
        static void _AssertLayoutPlayerStats();

        // ⭐ THE ONE LAYOUT GATE THAT IS ACTUALLY COMPILED. Defined in the mounted
        // BrnVehicleManager_layout_check.cpp; pins the whole +171464..+172616 tuning bank. Never
        // called -- static_assert fires at compile time, so /OPT:REF discarding it afterwards is
        // irrelevant. Keep this TU mounted.
        static void _AssertLayoutTuningBank();
    };

    // ==========================================================================================
    // GetPhysicsEntityIDFromGlobalEntityID -- header-inline definition (see the declaration's
    // banner). Byte truth: the three X360 inlined instances in PhysicsModule::FixUpVehicleContacts
    // @0x825A617C/@0x825A6BF4/@0x825A67A8; structural oracle: the PS3 DecFIGS out-of-line body
    // @0x6AC834 (same asserts, same arms, same returns). All member reads BY NAME -- the console
    // offsets (map @ this+149456 == mPhysicalTrafficManager+104688, table @ this+43584, bitset
    // @ this+44224) do not reproduce on the x64 host.
    // ==========================================================================================
    inline CgsSceneManager::EntityId VehicleManager::GetPhysicsEntityIDFromGlobalEntityID(
        CgsSceneManager::EntityId lGlobalEntityID)
    {
        const u32 luGlobalId = static_cast<u32>(lGlobalEntityID);
        const u32 luOwner    = luGlobalId >> 24;                       // EntityId::GetOwner()

        // EntityId::SetOwner tripwire (CgsEntityId.h:153) -- the X360 keeps the owner splice's
        // bound check even though the owner is only re-packed verbatim.
        CGS_ASSERT(luOwner <= 0xCu, "Burnout Specfic: Bad entity type set");
        u32 luEntityId_Physics = luOwner << 24;

        if (luOwner == 2u)   // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        {
            const u32 luGlobalIndex = (luGlobalId >> 10) & 0x3FFFu;    // GetEntityIndex()
            CGS_ASSERT(luGlobalIndex < sizeof(mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap),
                       "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");   // BrnPhysicalTrafficManager.h:944

            const u8 lu8Physical =
                mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
            if (lu8Physical == KU8_INVALID_MAP)
            {
                // Console streams "Failed to find local physics ID of traffic vehicle with global
                // entity ID: 0x%X" through gpcMessageBuffer; lowered to CGS_ASSERT with the static
                // prefix per the standing rule (BrnPhysicalTrafficManager.h:1030).
                CGS_ASSERT(false, "Failed to find local physics ID of traffic vehicle with global entity ID: ");
                return CgsSceneManager::K_INVALID_ENTITY_ID;           // dword_82F2A07C
            }
            // EntityId ctor bound check (CgsEntityId.h:116) -- dead for a u8 index, emitted on console.
            CGS_ASSERT(lu8Physical < (1u << 14), "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
            return CgsSceneManager::EntityId((static_cast<u32>(lu8Physical) << 10) | (2u << 24));
        }

        if (luOwner == 1u)   // BrnWorld::E_ENTITYTYPE_RACECAR -- VALIDATION-ONLY arm
        {
            // For every live race car whose stored entity id matches, re-pack the physics id from
            // the slot index and assert it equals the global id. The id returned is UNCHANGED.
            for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
                 liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
                 liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
            {
                if (maRaceCarEntityIDs[liCar].muValue == luGlobalId)
                {
                    // EntityId::SetEntityIndex tripwire (CgsEntityId.h:160) -- dead for liCar < 8.
                    CGS_ASSERT(static_cast<u32>(liCar) < (1u << 14),
                               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                    // X360 @0x825A6380: keep the owner byte + low 10 bits, splice the entity index
                    // (rlwinm mask 0xFF0003FF | index << 10).
                    luEntityId_Physics = (luEntityId_Physics & 0xFF0003FFu)
                                       | (static_cast<u32>(liCar) << 10);
                    CGS_ASSERT(luEntityId_Physics == luGlobalId,
                               "lEntityId_Physics == lGlobalEntityID");   // BrnVehicleManager.h:2031
                }
            }
            return lGlobalEntityID;
        }

        // Console streams through gpcMessageBuffer; lowered per the standing rule
        // (BrnVehicleManager.h:2039).
        CGS_ASSERT(false, "Attempting to get local physics ID of unsupported entity type");
        return CgsSceneManager::EntityId(0xFFFFFFFFu);
    }
}
}
