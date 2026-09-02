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
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"          // CgsPhysics::RigidBodyId (GetRigidBodyId/GetRaceCarPhysics, 2026-08-09)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle (maRaceCarModelHandles / maRaceCarGraphicsModelHandles @+43616/+43680 -- split out of mPadAA60, 2026-08-11)

// Pointer-only collaborators in RaceCarResponseInfo -- forward-declared in their real namespaces
// (homed by their own TUs; the classifier never dereferences them here).
// FORK RETIRED 2026-08-09 (conductor wave). This line used to forward-declare
// `PhysicsModuleIO::VehicleOutputRequestInterface` (class key CLASS) and every request-
// interface parameter below was typed on it -- but the REAL committed type is
// `BrnPhysics::Vehicle::VehicleOutputRequestInterface` (struct, BrnVehicleOutputInterface.h),
// and the PS3 DecFIGS mangles put it in Vehicle:: for every one of these methods
// (`PNS0_29VehicleOutputRequestInterfaceE`, NS0_ == BrnPhysics::Vehicle). The old
// wrong-namespace opaque decl forced reinterpret_casts at the seams (UpdateVehiclePhysics'
// SetRaceCarCrashing call carried one); all 21 uses are re-typed and the casts retired.
//
// THE SAME FORK, SECOND HALF, RETIRED 2026-08-11 (consolidation wave) -- this time for
// `VehicleOutputInterface` itself. `InstantTakedown`, `SetRaceCarCrashing`,
// `HandleRaceCarRaceCarContact` and `RaceCarResponseInfo::mpVehicleOutputInterface` were typed
// on a `BrnGameState::GameStateModuleIO::VehicleOutputInterface` that DOES NOT EXIST in the
// DecFIGS DWARF at all. The PS3 mangles are unambiguous (NS0_ == BrnPhysics::Vehicle):
//   _ZN10BrnPhysics7Vehicle14VehicleManager15InstantTakedownE...PNS0_22VehicleOutputInterfaceE...
//   _ZN10BrnPhysics7Vehicle14VehicleManager18SetRaceCarCrashingE...PNS0_22VehicleOutputInterfaceE...
//   _ZN10BrnPhysics7Vehicle14VehicleManager27HandleRaceCarRaceCarContactE...PNS0_22VehicleOutputInterfaceE...
// and the only declaration of the type anywhere in the DWARF is
// `struct BrnPhysics::Vehicle::VehicleOutputInterface` (BrnVehicleConstants.h:385), homed here
// in SharedIO/BrnVehicleOutputInterface.h:62. Both reinterpret_cast seams that the fork forced
// (BrnVehicleManager_DriverArms.cpp's `lpVehicleOutForTakedown`, and the SetRaceCarCrashing call
// in BrnVehicleManager_UpdateVehiclePhysics.cpp) are deleted with it.
//
// REMAINING FORK DEBT -- the same mangle evidence applies. `HandleCrashPredictionForRaceCarAndWorld`
// (:1296) and `HandleRaceCarWorldPotentialContact` (:1200) carried the GameStateModuleIO spelling
// until 2026-09-02, when the crash wave bodied the world arm and retired it (DWARF mangles
// `...PNS0_22VehicleOutputInterfaceE...`, NS0_ == BrnPhysics::Vehicle); the forward declaration of
// the phantom type that lived here is gone with it. The ONE remaining carrier is
// BrnGameState::MugshotManager (BrnMugshotManager.h, mangle
// `PKN10BrnPhysics7Vehicle22VehicleOutputInterfaceE`) -- it declares its own copy of the fork.
namespace BrnPhysics { namespace Vehicle { struct VehicleOutputRequestInterface; } }
namespace BrnPhysics { namespace Deformation { class DeformationInputInterface; } }
// The embedding host. Declared only so VehicleManager can befriend it (see the friend block at
// the end of the public section); BrnPhysicsModule.h includes THIS header, so it must not be
// included back.
namespace BrnPhysics { class PhysicsModule; }

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

// collaborators of the new per-frame surface, pointer
// use only here. Class keys match each committed home exactly (struct OutputBuffer ==
// CgsPhysicsSimulationModuleIO.h:321; struct ContactSpyData == BrnContactSpyData.h:60;
// class VehicleDriverInputInterface == BrnVehicleDriverInputInterface.h:44).
namespace CgsPhysics { namespace PhysicsSimulationIO { struct OutputBuffer; } }
namespace BrnPhysics { namespace ContactSpy { struct ContactSpyData; } }
namespace BrnPhysics { namespace Vehicle { class VehicleDriverInputInterface; } }

// the collaborators of the four
// per-frame leaves (FreeAllocations / UpdateVehicleEffects / ReadUpdatedBodyProperties /
// ProcessDeformationStates). Class keys match each type's committed home exactly.
namespace CgsModule { struct IOBufferStack; }                              // CgsIOBufferStack.h
// StartVehicleContactGeneration collaborators, pointer/
// template-arg use only in this header.
namespace CgsMemory { class LinearMalloc; }
namespace CgsMemory { struct SimpleDataStreamProducer; }   // stream-producer members (pointers)
namespace CgsMemory { struct SimpleDataStreamResultIterator; }  // traction harvest cursor (pointer)
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
namespace BrnGameState { namespace GameStateModuleIO { class GameEventQueue; } }

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
    // KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER -- the ONE place this class stops being byte-pinned.
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
    // It is a literal, deliberately, NOT `sizeof(PhysicalTrafficManager) - KU_X360_SIZEOF_...`.
    // Defining it from sizeof would make the asserts self-fulfilling; as a literal it is a tripwire
    // in both directions, and BrnVehicleManager_layout_check.cpp ties the three numbers together.
    //
    // There are now THREE drift terms, because there are three embedded sub-objects whose host
    // width differs from the console's. They apply to disjoint address ranges and they ACCUMULATE:
    //     X360 +0        .. +43616    ->  0               KU_HOST_DRIFT_AFTER_RACECAR_ARRAY
    //                                                      (the race-car array itself is now
    //                                                      width-identical, so this term is ZERO;
    // its range ENDS at +43616, not +43584)
    //     X360 +43616    .. +44768    ->  +128            KU_HOST_DRIFT_AFTER_MODEL_HANDLES
    //     X360 +44768    .. +163264   ->  +320            KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER
    //     X360 +163264   .. +172465   ->  +352            KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT
    //     X360 +172465   .. end       ->  +428            KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK
    //
    // NAMING CONVERGED 2026-08-11 (the merge of the two independent create-drain waves). Both
    // waves split `mPadAA60[128]` into the two real ResourceHandle arrays and both measured the
    // same +128; they disagreed only on WHERE to put it. One folded the +128 into
    // KU_HOST_DRIFT_AFTER_RACECAR_ARRAY (leaving a constant whose name no longer described its
    // cause); the other added KU_HOST_DRIFT_AFTER_MODEL_HANDLES as its own term and left the
    // race-car term at its true 0. **The second form is what survives**, because the two costs are
    // independently derived from two different sizeofs and each is separately asserted in
    // BrnVehicleManager_layout_check.cpp -- neither can absorb an error in the other. The zone
    // boundary at +43616 is what makes them separable at all: maRaceCarEntityIDs (+43584) is
    // BEFORE the split and takes the race-car term; everything from +43744 on takes the handle one.
    //
    // TABLE CORRECTED 2026-08-11 (consolidation wave). It used to read -1664 / -5632 / -5600,
    // which were the values from the 2026-08-03 record-fold wave and have been dead since
    // closing the SimpleVehicleAttribs slice gap (240 vs 32 per element) took the
    // race-car and traffic arrays back to their console strides, which retired the whole negative
    // side of the table. THE TERMS ARE STILL SIGNED -- nothing here assumes they are positive --
    // but as of today all four are positive, and each is defined below as the previous term plus
    // its own MEASURED step: +128 (handle arrays, ResourceHandle 16 vs 8), +192 (traffic manager's
    // pointers + debug-component span), +32 (debug component's vptr + back-pointer), +76 (the
    // contact-generation block carve). Every number in this table is the cumulative constant that
    // the offsetof asserts in BrnVehicleManager_layout_check.cpp actually add, so read them from
    // the four `const std::ptrdiff_t` definitions below, never from this comment.
    // ==========================================================================================

    // THE THIRD TERM, added 2026-08-03 (the record-fold wave). `maRaceCarVehicles` is now
    // `RaceCarPhysics[8]`, the real class, not the byte-pinned 5216-byte `RaceCarVehicleRecord`
    // stand-in it carried for ten waves. sizeof(RaceCarPhysics) is 5008 on the host against the
    // 5216 the X360 bakes as a literal stride (`mulli r11, r22, 0x1460` in SetRaceCarCrashing), so
    // the array is 8 * 208 == 1664 bytes shorter and everything after it moves DOWN.
    //
    // WHY THE FOLD HAD TO HAPPEN, and why it is not just tidying: VehicleManager::Construct calls
    // a constructor on each element. On a byte-pinned stand-in that call can only be a
    // reinterpret_cast, which writes the real class's members at HOST offsets into storage whose
    // readers use CONSOLE offsets -- the silent-corruption trade this header has warned about since
    // the blocker table was written. AND THE TREE WAS ALREADY MAKING THAT TRADE: six live
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
    // RE-DERIVED 2026-08-09 (attribs-setup wave): the term is now **0**. The whole -1664 was
    // 8 * -208, and the -208 per element was EXACTLY the SimpleVehicleAttribs gap (the tree's
    // 32-byte {mCOMOffset, mbIsValid} slice against the console's 240). That slice is now the
    // full width-identical 240 (BrnSimpleVehiclePhysics.h), so sizeof(RaceCarPhysics) is 5216 on
    // the host == the X360's literal stride (`mulli r11, r22, 0x1460`), and the array term
    // vanishes. MEASURED with the compiler via the gate below, not carried from this note.
    //
    // IT STAYS **0** THROUGH THE 2026-08-11 CREATE-DRAIN WAVE. One of the two waves that landed
    // that day folded the model-handle split's +128 into THIS constant; that is not done here,
    // because the +128 has nothing to do with the race-car array and hiding it behind this name
    // would make the constant undiagnosable the next time either component moves. It has its own
    // term, immediately below.
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_RACECAR_ARRAY = 0;

    // NEW TERM 2026-08-11 (the create-drain wave): **+128**, and it is the FIRST widening this
    // class has ever taken that is a pure pointer-width cost rather than a folded stand-in.
    // `mPadAA60[128]` -- the opaque span the header has carried at +43616 since 2026-08-03 -- is
    // the DWARF's two `ResourceHandle[8]` arrays (maRaceCarModelHandles then
    // maRaceCarGraphicsModelHandles, BrnVehicleManager.h:824/:825). They are now REAL, because
    // ProcessCreateEvents @0x82616770 stores into both (`stdx` at this+43616+8i and
    // this+43680+8i) and then double-dereferences the first as a StreamedDeformationSpec: that
    // cannot be spelled against a byte span without the offset-poke this project forbids.
    //   CgsResource::ResourceHandle is 8 bytes on console ({u32 mpResourceMemory, u32 mpSourceEntry})
    //   and 16 on the host (two real pointers) -- MEASURED at runtime last wave, `sizeofRH=16`.
    //   2 arrays * 8 elements * (16 - 8) == +128.
    // THIS IS *NOT* A SERIALIZED RECORD. VehicleManager is carved at runtime by Construct and
    // never streamed, so the [[serialized-slots-stay-32-bit]] rule does NOT apply here and the
    // handles widen -- the opposite call from the one a bundle record takes.
    // 128 % 16 == 0, so every 16-aligned member behind it keeps its alignment.
    // Members between +43584 and +43616 (maRaceCarEntityIDs) are BEFORE the split and keep
    // KU_HOST_DRIFT_AFTER_RACECAR_ARRAY; everything from +43744 on takes this one.
    //
    // DERIVED TWICE, INDEPENDENTLY, AND THE TWO AGREE. The second create-drain wave reached the
    // same +128 from a different direction: it MEASURED the handle on a real boot before writing
    // the split (`offPad=43616 sizeofRH=16` from the create-drain probe) rather than reading the
    // host layout out of the compiler, and it read the same two `stdx` seats plus the same
    // double-dereference. Two derivations, one number, and the compile-time gate in
    // BrnVehicleManager_layout_check.cpp ties it to `sizeof(CgsResource::ResourceHandle)` so it
    // cannot drift from either.
    //
    // WHY THE SPLIT HAD TO HAPPEN, stated once: FOUR bodies that landed this day --
    // ProcessCreateEvents (the only writer of both arrays), ProcessValidationEvents,
    // ProcessRemoveEvents and AddRaceCarDeformationModel -- reach these arrays BY ELEMENT. Against
    // an opaque `u8[128]` that can only be spelled as a raw offset cast, which is the offset-poke
    // this project forbids AND the exact family of live corruption (console stride 8 vs host
    // stride 16) that would have hidden inside it with nothing asserting.
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_MODEL_HANDLES =
        KU_HOST_DRIFT_AFTER_RACECAR_ARRAY + 128;   // step +128 -> cumulative +128

    // RE-MEASURED 2026-08-03 (the TrafficPhysics de-fork wave): the second term is now **-3968**,
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
    // RE-DERIVED 2026-08-09 (attribs-setup wave): the step is back to **+192**, the value it
    // had before the TrafficPhysics de-fork -- because the de-fork's -4160 was 20 * -208, and the
    // -208 per element was the SimpleVehicleAttribs slice gap, now closed (see the race-car term
    // above). sizeof(TrafficPhysics) is 5168 on the host == the console's `mulli 0x1430` stride,
    // so the only remaining widening in the traffic manager is its own +192 (pointer members +
    // the 48-vs-32 debug component span).
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER =
        KU_HOST_DRIFT_AFTER_MODEL_HANDLES + 192;   // step +192 -> cumulative +320 (rebased 2026-08-11
                                                   // onto the model-handle split, which sits in
                                                   // front of the traffic manager)

    // The third term, added 2026-08-03 in an earlier wave. VehicleManagerDebugComponent is 1328
    // bytes on the host against the 1296-byte X360 span at +161968..+163264 -- +32, because its
    // base's vptr and its mpVehicleManager both widen 4 -> 8. Unlike the traffic manager's span,
    // this 1296 was never a guess: both ends are asm-literal (Construct(this + 161968, this), then
    // the stride-1024 walk from +163264), so the +32 is real and the only honest answer is to carry
    // it. 32 % 16 == 0, so every 16-aligned member past it keeps its alignment.
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT =
        KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER + 32;  // step +32 -> cumulative +352 (was mislabelled
                                                   // +224 until 2026-08-11: that was the cumulative
                                                   // value from BEFORE the race-car term became +128)

    // The fourth term, added 2026-08-06 (big-five #2): the contact-generation block carve
    // (+172465..+172592 console -> real members). Growth = the pointer-pair carve's +12 (4
    // alignment + 2x pointer widening, already asserted at the head), five builders 12 -> 16
    // (+20), nine more pointers 4 -> 8 (+36), one alignment pad before the producer run (+4),
    // and one before the traction-line pointer (+4): +12+20+36+4+4 == +76. MEASURED against the
    // compiled layout by the gate's own seat asserts (BrnVehicleManager_layout_check.cpp).
    const std::ptrdiff_t KU_HOST_DRIFT_AFTER_CONTACT_GEN_BLOCK =
        KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT + 76;  // step +76 -> cumulative +428 (same stale-label
                                                   // fix as the term above; +428 is the number
                                                   // BrnVehicleManager_layout_check.cpp:411 quotes)

    // Pointer-only use here; the complete type is
    // BrnPotentialContactAverager.h (the DWARF only ever forward-declares it too --
    // BrnVehicleConstants.h:1597 -- because the console defines it inside BrnVehicleManager.cpp).
    struct PotentialContactAverager;

    class VehicleManager
    {
    public:
        // The eight-car array bound, asm-literal in VehicleManager::Construct
        // (`li r23, 8` then `addi r23, r23, -1` per iteration) and the width of every one of this
        // class's per-car arrays. The console spells it BrnWorld::KI_MAX_ACTIVE_RACE_CARS -- the
        // disconnect asserts name it -- but that constant has no committed home in this tree, so it
        // is declared at the class that owns the arrays. DELETE-WHEN BrnWorld homes it.
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

        // Construct @0x8263B7C8, 943 instructions -- BODIED 2026-08-03 in
        // BrnVehicleManager.cpp. Its only caller is BrnPhysics::PhysicsModule::Construct
        // @0x825AE308, which is still a link stub (WorldLinkStubs.cpp). See the big recipe block
        // further down in this header for the full instruction-level shape and every default the
        // tuning bank seeds; the body is written straight off it.
        void Construct();

        // ==================================================================================
        // The per-frame FORCE
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
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            bool lbIsOnlineGameMode,
            CgsSceneManager::EntityId lWorldEntityId);

        // DWARF h:947; X360 @0x825B5690. BODIED in the slice TU.
        bool IsRaceCarCrashing(s32 liRaceCarIndex);

        // DWARF h:938; X360 @0x825C6088 (988 insns), PS3
        // DecFIGS 0x70AB20 (the mangle is the signature authority). Validate ONE race-car-vs-
        // world potential contact (may REWRITE the contact's normal/point in place -- the
        // wall-normal flatten and the wheel/bottom-plane projection); returns whether the
        // contact survives into the Validated queue. BODIED in
        // BrnVehicleManager_ValidateRaceCarWorldContact.cpp (slice TU; home BrnVehicleManager.cpp
        // is still unmounted -- RaceCarPhysics_Construct precedent).
        bool ValidateRaceCarWorldContact(
            CgsSceneManager::SceneManagerIO::PotentialContact* lpInOutContact,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
            f32 lfTimeStep);

        // DWARF h:1239 -- the FIVE-argument overload; X360 @0x82635B78 (the unnamed sub the
        // export set skipped; identity proof in the slice TU banner). BODIED in the slice TU.
        // (The 6-arg + EntityId overload @0x82635B00, DWARF h:1242, is CrashFatalRaceCars'
        // callee and lands with that body.)
        void ForceRaceCarCrash(
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            EActiveRaceCarIndex leRaceCarIndex);

        // ⭐⭐ X360 @0x82633990 -- THE PRODUCER HALF of the above-ground down-ray whose RESULT
        // half (ProcessAboveGroundLineTestsResults, below) has been live all along. Its ONLY
        // caller is PhysicsModule::GenerateSceneQueries @0x825A1428.
        //
        // ⛔ WHAT ITS ABSENCE COST, MEASURED (aicar_reset wave, 2026-08-26): with no producer,
        // RaceCarState::mAboveGroundTestResult.mbValid was FALSE on every frame of every drive
        // ([collision-tag] aboveGroundValid=0, tag 0xFFFF8000 == AboveGroundTestResult::Reset's
        // clear value), so RCEM::UpdateRaceCarCollisionTagging early-returned, muCurrAISection
        // stayed KI_INVALID_SECTION_INDEX (0x7FFF) for every car, NO CAR EVER ENTERED THE AI
        // SECTION SYSTEM, and ActiveRaceCar::UpdateResetTransform never pushed a single entry
        // into the four-deep mPrevTransforms reset ring.
        // ⭐ It is NOT a blocker for crash recovery -- GetResetCoords' empty-ring arm hands out
        // the car's live transform -- but it is the difference between "put the car back where
        // it crashed" and "put it back on the last road pose it held".
        //
        // ⛔⛔ LANDED 2026-08-26 AND IT CHANGED NOTHING YET -- MEASURED, SAY IT PLAINLY. On the
        // very run that proved the reset pump (rp_crash3) the witness still reads
        //     [collision-tag] car 0 aboveGroundValid=0 tag=0xFFFF8000 section=32767
        // on every sample. The producer runs and posts the query; THE ANSWER NEVER COMES BACK.
        // ⭐⭐ AND THE PREVIOUS WAVE'S NOTE IS WHY THAT WAS A SURPRISE: it recorded "the RESULT
        // half of that round trip is already fully live", which was true of the VEHICLE-MANAGER
        // side (WorldBridgeSceneToPhysics case 2 -> AddLineTestResult ->
        // ProcessAboveGroundLineTestsResults -> SetAboveGroundTestResult, all bodied) and says
        // NOTHING about whether anything ANSWERS a fine query. Nothing does: the SceneManager
        // query pipeline is severed in five places (SceneManagerModule::ProcessSceneQueries is a
        // WorldLinkStubs stub, ProcessFineQueries / ProcessLineTestFine are absent, and
        // FineIntersectionTestModule::ComputeLineTestFine is an EMPTY body with no callers) --
        // the same five severances BrnPlaceOnTrackManager.cpp's own bring-up leg was written for.
        // ⭐ THE LESSON, because it is the third time this campaign: "THE CONSUMER IS BODIED" IS
        // NOT "THE QUESTION GETS ANSWERED". A round trip has three parts, and the middle one here
        // is a different subsystem.
        // DELETE-WHEN the SceneManager fine-query pipeline answers a query; this producer is then
        // already in place and the AI section system starts filling on its own.
        // BODIED in the slice TU.
        void GenerateAboveGroundLineTests(
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestInterface);

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
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // DWARF h:1236; X360 @0x82640690 (264 insns).
        void UpdateAggressiveDriving(
            f32 lfTimeStep,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // DWARF h:1224; X360 address not in this TU's dossier (the ledger mis-keys it to the
        // CgsBitArray.h TU -- re-derive at its own wave).
        void UpdateCrashes(f32 lfTimeStep);

        // DWARF h:893; X360 @0x82633CD8 (68 insns).
        // THE 2026-08-10 "ARITY CORRECTED TO ONE PARAMETER" IS ITSELF RETRACTED, 2026-08-11
        // (lifetime wave). That note reasoned entirely from the CALLEE ("r5 is never touched in
        // any of the 68 instructions ... the interface argument never existed") and concluded the
        // second parameter was fabricated. An unread argument is not an absent one, and the
        // CALLER settles it:
        //     0x8264565C  mr   r5, r29                 <- UpdateVehiclePhysics DOES set r5
        //     0x82645660  lwz  r4, arg_1C(r1)
        //     0x82645664  mr   r3, r24
        //     0x82645668  bl   VehicleManager::EndVehicleTractionLineTests
        // and r29 is loaded fresh from an incoming argument slot at 0x82645638. The PS3 DWARF
        // types it:  ..VehicleManager27EndVehicleTractionLineTestsEPN9CgsModule13IOBufferStackE
        //            PKNS0_21VehicleInputInterfaceE
        // i.e. (IOBufferStack*, const VehicleInputInterface*) -- and UpdateVehiclePhysics has
        // exactly one VehicleInputInterface parameter in scope. RESTORED to two.
        // This is the standing "prefer the DWARF arity when the console never reads an
        // argument" rule; nothing behavioural changes, the declaration just stops claiming
        // something the image contradicts.
        // (Still RETRACTED, correctly: the old "export-set hole" claim -- the body IS exported.)
        void EndVehicleTractionLineTests(CgsModule::IOBufferStack* lpInputBufferStack,
                                         const VehicleInputInterface* lpInputInterface);

        // DWARF h:1287; X360 body is an export-set hole (image-only).
        void CrashFatalRaceCars(
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            CgsSceneManager::EntityId lWorldEntityId);

        // DWARF h:1085 `void ReadSurfaceProperties(Attribute::Key)`; X360 @0x825C7BB8
        // (187 insns, AttribSys walk). WorldModule::Prepare @0x827D5B60..0x827D5B70
        // passes its embedded manager plus StringToKey("340654"); UpdateVehiclePhysics
        // passes the same TU-local initialized key on player reset. WIDTH: both
        // callers load the key with a 64-bit `ld`
        // (qword_82FB7F10) while the committed Attribute::Key typedef is u32 -- the
        // parameter is spelled u64 off the asm; the typedef conflict is FLAGGED here, not
        // resolved (attribhash64 keys are 64-bit; the u32 typedef has its own note).
        void ReadSurfaceProperties(u64 luSurfaceListKey);

        // ADDITIVE 2026-08-04 (task #135) -- X360 @0x82633568, the stage-6 arm of
        // BrnPhysics::PhysicsModule::Prepare @0x825ADB68 (`bl` at 0x825ADDFC, result tested
        // with a `bne` so the return is a bool). Arguments are the physics resource allocator
        // (bank 23) and the scene input buffer PhysicsModule::Prepare was handed.
        // in BrnVehicleManager_Prepare.cpp; the
        // WorldLinkStubs gate is DELETED. A resumable three-stage fall-through FSM over
        // mePrepareStage: PrepareData -> PrepareTriangleCache -> done. Every store in its tail
        // arm is reached BY NAME (meReleaseStage +4, mn8RoundRobinControlWord +172464,
        // mbTrafficCheckingAllowed +172313 -- all three already mapped in this header).
        bool Prepare( rw::IResourceAllocator* lpAllocator,
                      CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer );

        // X360 @0x82633568 (161 insns). Prepare's stage-1 arm: the per-car data build --
        // 8x VehiclePhysics::Construct + 8x { VehicleDriver::Prepare, VehiclePhysics::Construct,
        // Vehicle::DebugComponent::Construct }, then PhysicalTrafficManager::Prepare
        // @0x8262CA48, VehicleDriver::Prepare on the 9th (traffic) driver, and ~30 scalar seeds.
        // NOT RECONSTRUCTED 2026-08-10 -- a named LINK STUB in WorldLinkStubs.cpp, deliberately,
        // for two measured reasons rather than one felt one:
        //   (1) its own callee closure is ~470 further instructions across four functions of
        //       which only VehiclePhysics::Construct exists in this tree, and
        //   (2) the Hex-Rays view degenerates into `_R28`/`_R31` inline-asm with every store at a
        //       raw console byte offset past mPhysicalTrafficManager -- i.e. past the +224 host
        //       drift this header documents -- so reproducing it from the pseudocode would be
        //       exactly the offset hack the project forbids.
        // It always returns 1 on the console (there is no failure path in the body), which is what
        // makes the FSM above landable without it; the drop is one greppable symbol.
        bool PrepareData( rw::IResourceAllocator* lpPhysicsAllocator );

        // X360 @0x82615BA0 (37 insns); home
        // BrnVehicleManager.cpp:891 per its own baked assert path. Prepare's stage-2 arm and
        // **the function that registers a car with the triangle cache at all**: 8 InEventAddToCache
        // (slots 0..7, radius KF_TRIANGLE_CACHE_SPHERE_RADIUS) into the scene input's
        // mAddToCacheQueue, then the 20 traffic slots via
        // PhysicalTrafficManager::PrepareTriangleCache @0x825EE5A0.
        bool PrepareTriangleCache(
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update );

        // DWARF BrnVehicleManager.h:667; X360
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

        // DWARF BrnVehicleManager.h:562; header-inline on the console (no
        // out-of-line emission -- PhysicsModule::BridgeSimulationToOutput @0x825B0574 reaches
        // the member as a bare `this + 0x2BE40` addi, i.e. the inlined accessor). The bridge
        // drains this queue into the module's ContactSpyData every frame.
        const CgsModule::EventQueue<BrnPhysics::ContactSpy::DiscardedContact, 20>*
        GetDiscardedContacts() const { return &mDiscardedContacts; }

        // ==========================================================================================
        // The contact-generation /
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
        // the sim add-contact queue. FLAG: DECLARED for BridgeContactsToSimulation's closure;
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
        // stream jobs. Caller: PhysicsModule::Update @0x825B0640 is a REAL BODY
        // (BrnPhysicsModuleUpdateFunctions.cpp, mounted) and drives this every non-catchup frame --
        // it has NOT been a link stub since the 2026-08-09 conductor wave.
        void StartVehicleContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* lpOverlapPairs,
            f32 lfTimeStep,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
            CgsModule::IOBufferStack* lpIOBufferStack,
            CgsMemory::LinearMalloc* lpLinearMalloc,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface );

        // PhysicsModule::BridgeContactsToSimulation calls the
        // embedded traffic manager's ValidateAndFixUpTrafficTrafficContact with the embedded
        // object's address as `this` (module+63872 == this+44768) -- access a private member
        // cannot express from PhysicsModule. FLAG: the accessor NAME is inferred (no DWARF
        // accessor is dumped); the console evidence is the direct embedded-object call.
        PhysicalTrafficManager& GetPhysicalTrafficManager() { return mPhysicalTrafficManager; }

        // Header-inline reset of the non-physical contact
        // count -- BridgeContactsToSimulation @0x825A9A50 opens with a bare `stw 0` at
        // module+179760 == this+160656 == miNonPhysicalContactCount (the int32 the DWARF seats
        // directly after maNonPhysicalContacts[128], :851). Spelled BY NAME here.
        void ResetNonPhysicalContacts() { miNonPhysicalContactCount = 0; }

        // ==========================================================================================
        // StartVehicleContactGeneration's own contact-generation
        // callees. Signatures per the PS3 DecFIGS mangles (@0x75C0C8 / @0x788190 / @0x789760);
        // 0x8261BF28 and 0x825C2EA0 are .ida-exports HOLES -- the PS3 twins are the signature
        // authority for both.
        // STALE FLAG RETIRED 2026-08-19 (wave Q7, cluster `carcar`): this block used to end
        // "FLAG: all four bodies are TRAP STUBS this wave -- named, not landed". Three of the four
        // have been real for waves (DoRaceCarWorldContactGeneration since 2026-08-14, IsRaceCarHidden
        // since 2026-08-11) and DoCarCarContactGeneration is real as of this wave; only
        // DoTrafficCarWorldContactGeneration is still a trap.
        // ==========================================================================================

        // @0x8261BB38 (251; PS3 0x75C0C8) -- REAL as of 2026-08-19 (wave Q7), body in
        // BrnVehicleManagerContactGeneration.cpp. Generate the car-vs-car contacts for one overlap
        // pair: sphere-list-vs-sphere-list into the collide stream when neither car is a SIMPLE
        // traffic vehicle, box-vs-box into a simple-traffic pair builder when one is (that arm is a
        // documented PARTIAL -- see the body's banner).
        //
        // PARAMETER ORDER, DO NOT "FIX" IT: the u16 comes BEFORE the f32. That is the DWARF's
        // own declaration order (dwarfdump BrnVehicleManagerContactGeneration.cpp:184) AND what the
        // X360 asm says once the Xbox 360 parameter save area is read correctly -- 8-byte
        // doubleword slots, values right-justified, so `lwz arg_54` (producer, slot 0x50+4) and
        // `lhz arg_5E` (queue id, slot 0x58+6) are CONSECUTIVE parameters, not a skipped slot.
        // lfTimeStep rides f1 and its slot 0x60 is never written. Swapping them compiles fine and
        // silently transposes the queue id with the timestep.
        //
        // lu16QueueID is the custom potential-contact queue the pair's contacts route to -- the
        // driver passes 7 (racecar-racecar), 8 (racecar-traffic) or 13 (traffic-traffic), exactly
        // the bridge's queue-index/ContactId binding. It rides through to the posted command's
        // UserTagA, which is what AddContactResultsToQueue posts the harvested contacts into.
        void DoCarCarContactGeneration( CgsSceneManager::EntityId lCarIdA,
                                        CgsSceneManager::EntityId lCarIdB,
                                        CgsSceneManager::EntityId lCarPhysicsIdA,
                                        CgsSceneManager::EntityId lCarPhysicsIdB,
                                        BrnPhysics::Deformation::DeformationManager* lpDefMan,
                                        BrnPhysics::ContactGenList* lpContactGenList,
                                        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
                                        CgsMemory::SimpleDataStreamProducer* lpSphereSphereStream,
                                        u16 lu16QueueID,
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

        // @0x8261BF28 (export HOLE; PS3 0x789760). Generate one traffic car's car-vs-world
        // contacts.
        void DoTrafficCarWorldContactGeneration( s32 liTrafficIndex,
                                                 BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
                                                 const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
                                                 BrnPhysics::ContactGenList* lpContactGenList,
                                                 CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
                                                 CgsMemory::SimpleDataStreamProducer* lpSphereTriangleStream,
                                                 u32 luQueueIndex,
                                                 CgsMemory::LinearMalloc* lpLinearMalloc );

        // @0x825C2EA0 (export HOLE -- no export json AND no PS3 out-of-line twin surfaced;
        // signature from the two SVCG call sites: (this, race-car index) -> bool in r3).
        bool IsRaceCarHidden( s32 liRaceCarIndex );

        // ==========================================================================================
        // The four small per-frame leaves of
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

        // ==========================================================================================
        // The remaining per-frame surface Update calls. Signatures are DWARF-authoritative
        // (references/DecFIGS/dwarfdump/.../BrnVehicleManager.h) and corroborated by the PS3
        // DecFIGS out-of-line mangles. Split by status:
        //   REAL THIS WAVE (bodied in BrnVehicleManager_ConductorLeaves.cpp): CheckState,
        //     UpdateCameraMatrix, GetForceNoSlowMo, ResetForceNoSlowMo, ProcessWheelContacts,
        //     ReadUpdatedBodies.
        // FLAG -- DECLARED FOR THE CONDUCTOR'S CLOSURE, body still a LOUD one-shot gate in
        //     BrnPhysicsConductorGates.cpp (each gate names its X360 address + insn count):
        //     the rest. Reconstruct each and DELETE its gate (LNK2005 is the tripwire).
        // ==========================================================================================

        // @0x825EADA8 (DWARF dump :977). Debug validation sweep: for every live race car
        // (mUsedRaceCars) run ExternalPhysicsBody::CheckState on its physics body; a failure
        // streams the car's entity id into the assert buffer. Called FIFTEEN times per
        // Update as stage brackets.
        void CheckState();

        // DWARF :264. Latch this frame's camera matrix (Update copies the input buffer's
        // camera block here; the per-car force model reads it for camera-relative effects).
        void UpdateCameraMatrix(const Matrix44Affine* lpCameraMatrix);

        // DWARF :656 / :660 (both non-const there; kept verbatim). The super-slow-motion
        // inhibit latch (Update's crashed-car slow-mo block reads it, then resets it).
        bool GetForceNoSlowMo()   { return mbForceNoSlowMo; }
        void ResetForceNoSlowMo() { mbForceNoSlowMo = false; }

        // DWARF :989. The per-slot handling-body handle. X360-attested by Update's
        // slow-motion block, which inlines slot 0's read (`ldx` this+43744).
        CgsPhysics::RigidBodyId GetRigidBodyId(s32 liRaceCarIndex)
        {
            return CgsPhysics::RigidBodyId(maRaceCarHandlingBodyIDs[liRaceCarIndex]);
        }

        // DWARF :995. Resolve a handling-body id to its race car: linear-search the
        // per-car EntityId table for the id's entity word; miss returns NULL. X360-attested
        // by Update's slow-motion block, which inlines exactly this loop
        // (0x825B0C5C..0x825B0C94: 8 x `lwz`/`cmplw` over this+43584, then
        // `mulli 0x1460` + `addi 0x740` into maRaceCarVehicles).
        RaceCarPhysics* GetRaceCarPhysics(CgsPhysics::RigidBodyId lRigidBodyId)
        {
            const u32 luEntityId = static_cast<u32>(lRigidBodyId.GetEntityId());
            for (s32 liCar = 0; liCar < 8; ++liCar)
            {
                if (maRaceCarEntityIDs[liCar].muValue == luEntityId)
                {
                    return &maRaceCarVehicles[liCar];
                }
            }
            return 0;
        }

        // DWARF :375, X360 @0x8284CB38. EMPTY AS SHIPPED: the retail X360 body is a single
        // `blr`, ICF-folded with BaseCollisionGenerator::Destruct (which is why the Update
        // call site's `bl` appears to target that symbol with r3 == &mVehicleManager and
        // f1 == the timestep). The PS3 DecFIGS build keeps the (equally empty) function
        // out of line under its own name. Reconstructed as the empty member it is.
        void ProcessWheelContacts(f32 lfTimeStep,
                                  BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface);

        // @0x82619A10 (DWARF :366). BODIED 2026-08-10 (create-path wave) in
        // BrnVehicleManager_ReadUpdatedBodies.cpp -- the conductor gate is deleted.
        //
        // THE DESCRIPTION THAT USED TO SIT HERE WAS WRONG IN BOTH CLAUSES, and it is worth
        // saying why rather than just replacing it. It read: "first the fallback integration for
        // live cars NOT owned by the sim this frame ..., then drain the sim's OutUpdateRigidBody
        // queue into the owning cars." The X360 body does NEITHER. There is no ownership test --
        // the only per-car guard is ExternallySimulatedBody::mbFrozen -- and the queue is never
        // dereferenced here at all: it arrives in r4, is stashed at var_1C and is forwarded
        // untouched to PhysicalTrafficManager::ReadUpdatedBodies, whose only use of it is a dev
        // duplicate-id assert. Nothing in this call chain reads a transform back from rw::physics.
        //
        // What it IS: the per-frame **gravity + integration step** for every live race car --
        //     mLinearVelocity.y -= KF_GRAVITY * dt ;  ExternalPhysicsBody::IntegrateTransform(dt)
        // -- then the same for traffic. Because a race car is an ExternalPhysicsBody the game
        // integrates itself, this is the ONLY place gravity enters a car and the only place a
        // car's pose advances. See the TU banner for the full asm decode.
        void ReadUpdatedBodies(
            const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodyQueue,
            VecFloat lvfTimeStep);

        // ==========================================================================================
        // THE VEHICLE MAINTENANCE FAMILY -- ADDED 2026-08-10 (create-path wave).
        //
        // ProcessVehicleMaintenanceEvents @0x8264AB38 (118 insns) is BODIED for real in
        // BrnVehicleManager_MaintenanceEvents.cpp. It is pure orchestration: seven null asserts
        // (BrnVehicleManager.cpp:1141..:1147, in parameter order) then six calls with no branch
        // between them. Its ONLY console caller is PhysicsModule::PostSceneUpdate @0x825ABC10,
        // which is why nothing in this family has ever executed on this build.
        //
        // THE FIVE ARMS BELOW ARE ONE-SHOT GATES, and the middle one is the point of the whole
        // campaign: ProcessCreateEvents @0x82616770 is the ONLY writer anywhere in the XEX that
        // SETS a bit in mUsedRaceCars (the tree's only other write is Construct's UnSetAll()).
        // Until it lands, no race car exists to the physics vehicle manager.
        //
        // AND IT MUST NOT LAND BEFORE THE GROUND DOES. Setting one bit of mUsedRaceCars
        // turns on FOUR already-mounted, already-called per-frame loops that walk that bitset --
        // ReadUpdatedBodies (gravity + IntegrateTransform, i.e. the fall itself),
        // UpdateVehiclePhysics (both loops, including RaceCarPhysics::Update),
        // StartVehicleContactGeneration and the traction-line harvest. See the ground-cost census
        // in BrnPhysicsConductorGates.cpp: the ORDER is traction chain -> create path.
        // ==========================================================================================
        void ProcessVehicleMaintenanceEvents(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            const VehicleInputInterface* lpInputInterface,
            VehicleOutputRequestInterface* lpOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            VehicleOutputInterface* lpVehicleOutputInterface,
            Deformation::DeformationInputInterface* lpDeformationInterface);   // @0x8264AB38

        // @0x825C7EA8 (321). Arm 1: record which NETWORK race cars were added for collision this
        // frame. BODIED 2026-08-11 (create-drain wave), BrnVehicleManager_MaintenanceEvents.cpp.
        void RecordNetworkRaceCarsAddedForCollision(const VehicleInputInterface* lpInputInterface);

        // @0x826160C8 (426). Arm 2: drain the remove-race-car queue.
        // BrnVehicleManager_MaintenanceEvents.cpp. It is part of the CREATE
        // wave, not a later one: the three create events on one boot name race-car slots 0, 1 and 2
        // in turn, and ProcessCreateEvents' own "Race Car Index Already Used" assert says the
        // console expects the slot to be free before it is claimed.
        void ProcessRemoveEvents(const VehicleInputInterface* lpInputInterface,
                                 VehicleOutputRequestInterface* lpOutputInterface,
                                 VehicleManagerOutputInterface* lpManagerOutputInterface,
                                 Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x82616770 (1,067). Arm 3: THE CREATE PATH -- the only writer in the whole XEX that
        // SETS a bit in mUsedRaceCars. Bodied 2026-08-11 in its OWN slice TU,
        // BrnVehicleManager_ProcessCreateEvents.cpp, which is deliberately kept out of
        // build_game_exe.bat until RaceCarPhysics::Prepare @0x82639CB8 has a declaration -- see
        // that file's banner for why the seat and the bit-set cannot be separated.
        void ProcessCreateEvents(const VehicleInputInterface* lpInputInterface,
                                 VehicleOutputRequestInterface* lpOutputInterface,
                                 VehicleManagerOutputInterface* lpManagerOutputInterface,
                                 Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x825E9010 (65). Arm 4: drain the validate-race-car queue.
        // BrnVehicleManager_MaintenanceEvents.cpp.
        void ProcessValidationEvents(const VehicleInputInterface* lpInputInterface,
                                     Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x825E8F28 (57). Arm 5: drain the set-race-car-collision + set-culling-group queues.
        // BrnVehicleManager_MaintenanceEvents.cpp.
        // THE "EXPORT HOLE" IS RETIRED. Every previous banner recorded this address as "a HOLE
        // in the IDA export set -- no per-function JSON, insn count genuinely unknown". It is a
        // fully analysed, correctly NAMED function in the ARTIST .i64: start 0x825E8F28, end
        // 0x825E900C, 57 instructions, Hex-Rays clean, one caller. It was simply MISSING FROM THE
        // EXPORT RUN (the exporter walks idautils.Functions() and writes one JSON per function;
        // this one's file was never written). Recovered with the targeted headless-idat technique
        // of commit b53e2523. ⇒ "absent from .ida-exports" is not evidence about the database.
        void ProcessCollisionEvents(const VehicleInputInterface* lpInputInterface,
                                    Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x825E9118 (153) -- ADDED 2026-08-11 (create-drain wave). DWARF-attested shape
        // (BrnVehicleManager.h:1131 `void AddRaceCarDeformationModel(DeformationInputInterface*,
        // int32_t, Matrix44Affine, float32_t, DeformationResetType)`), and the X360 prologue agrees
        // with the PPC float-arg rule exactly: r4 = interface, r5 = index, r6 = the transform (a
        // large by-value struct passed by hidden reference), f1 = the damage amount -- which SKIPS
        // r7 -- and r8 = the reset type.
        // Its only caller is ProcessCreateEvents. Bodied in BrnVehicleManager_MaintenanceEvents.cpp.
        void AddRaceCarDeformationModel(Deformation::DeformationInputInterface* lpDeformationInterface,
                                        s32 liVehicleIndex,
                                        Matrix44Affine lInitialWorldSpaceTransform,
                                        f32 lfInitialDamageAmount,
                                        Deformation::DeformationResetType leBaseDeformationType);

        // @0x825E9380 (175) -- ADDED 2026-08-11 (create-drain wave). DWARF BrnVehicleManager.h
        // :1335. Walk the live-car bitset and hide every NETWORK car for at least liFrames frames.
        // Its only caller is ProcessCreateEvents. Bodied in BrnVehicleManager_MaintenanceEvents.cpp.
        void SetAllNetworkRaceCarsHidden(s32 liFrames);

        // DWARF BrnVehicleManager.h:986
        // `EntityId GetEntityId(RigidBodyId)`. Inline on the console -- its body is what
        // AddRaceCarDeformationModel @0x825E9210..0x825E931C is, asserts and all
        // (BrnVehicleManager.h:1985 "Entity id X does not match stored entity id Y").
        // Defined out-of-line below this class so it can see the private tables.
        EntityId GetEntityId(CgsPhysics::RigidBodyId lRigidBodyId);

        // ==========================================================================================
        // TRACTION-LINE CHAIN, the four members bodied for real 2026-08-10 (ground wave), in
        // BrnVehicleManager_TractionLineTests.cpp. A Burnout car does NOT rest on contacts --
        // contacts are the body-shell/crash path; it rests on TRACTION LINE TESTS, and this is the
        // producer lifecycle plus the race-car harvest that ends in Wheel::mRoadContact.mbIsOnGround.
        // ALL FOUR ARE UNREACHED TODAY: their only callers are StartVehicleTractionLineTests
        // (gate-bodied below) and EndVehicleTractionLineTests (link stub) -- see those two for why
        // the generation half cannot land yet. They are mounted so the LINK closure is enforced.
        // ==========================================================================================

        // @0x825B5098 (52 insns). Carve the traction-line command stream out of the contact
        // generator's result arena and seat it in mpTractionLineStreamProducer.
        void DoVehicleTractionLineAllocations(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen);

        // @0x825B5168 (64 insns). Publish the stream's geometry (SimpleDataStreamProducer::Begin,
        // which the console inlines here) and dispatch the line-test job tree.
        void RunTractionLineTestJobs(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen);

        // @0x825B5268 (37 insns). Release the seat. The stream memory itself is arena-owned by the
        // generator, so this is a pointer null-store, not a free.
        void DoVehicleTractionLineDecallocations(CgsModule::IOBufferStack* lpInputBufferStack);

        // @0x82618058 (231 insns). THE PAYOFF: per live race car, per wheel that reported a hit,
        // feed {position, normal, surface tag} into RaceCarPhysics::AddTractionPoint -- which is
        // what sets Wheel::mRoadContact.mbIsOnGround and so what UpdateSuspensionSprings pushes
        // against. Takes the result cursor BY POINTER: EndVehicleTractionLineTests copies the
        // producer's own iterator to its stack (`ld r11, 0x38(producer)`, one 8-byte value) and
        // hands the same copy to all three harvests in turn, so each resumes where the last stopped.
        void ReadRaceCarTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator* lpResultIterator);

        // @0x825E9640 (313 insns) -- BODIED 2026-08-11 (lifetime wave). THE GENERATION HALF
        // of the race-car leg: one 176-byte stream command per LIVE race car, carrying that car's
        // window into the shared triangle cache and its four wheels' suspension probes.
        // Signature from the PS3 DWARF (..VehicleManager27AddRaceCarTractionLineTestsE
        // PN15CgsSceneManager12CgsCollision18CollisionGeneratorEPKNS2_14SceneManagerIO22Triangle
        // CacheInterfaceE) and matched by the X360 asserts, which name both arguments.
        // Returns the number of commands posted (the caller accumulates it).
        s32 AddRaceCarTractionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface);

        // @0x82615C38 (240 insns) -- BODIED 2026-08-11 (lifetime wave). THE POSITION HALF:
        // per live race car, post one InEventUpdateCachedPosition carrying {world position,
        // bounding radius} for that car's triangle-cache slot, then chain to the traffic pool.
        // Without it every claimed cache slot's sphere stays at the WORLD ORIGIN and the fill
        // worker caches triangles from the wrong place entirely.
        void UpdateTriangleCache(
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update);

        // ---- FLAG: gate-bodied (BrnPhysicsConductorGates.cpp) from here down ------------------

        // THE TWO SIDE LEGS OF THE TRACTION-LINE PRODUCER, GATED IN MATCHED Add<->Read PAIRS
        // StartVehicleTractionLineTests and
        // EndVehicleTractionLineTests are REAL as of this wave and run every frame; these six are
        // the traffic and player-stuck legs, which a race car on the ground does not need
        // (1,319 console instructions measured). THEY MUST STAY PAIRED: each Add posts one
        // command per live object and the matching Read consumes exactly that many records off a
        // SHARED cursor. Landing an Add without its Read leaks records into the next harvest;
        // landing a Read without its Add makes it consume the race-car leg's answers. Reconstruct
        // a PAIR at a time and delete both gates together.
        //   0x825E9B28 (171) AddPlayerStuckInCollisionLineTests   <-> 0x825C3898 (118) ReadPlayerStuck...
        //   0x825E9DD8  (87) UpdatePlayerStuckInCollisionTest      (same leg; drives the spheres)
        //   0x825C4AB8 (147) UpdatePlayerStuckInCollisionSpheres   (same leg)
        // (the traffic pair lives on PhysicalTrafficManager -- see BrnPhysicalTrafficManager.h)
        // Returns the command count, like its two siblings -- StartVehicleTractionLineTests
        // accumulates all three returns into miNumSPUTractionLineTests (`add r11, r3, r11 ;
        // stw r11, 0(r30)` after each of the three `bl`s at 0x82629D8C/DB8/DD8).
        s32 AddPlayerStuckInCollisionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager);
        void UpdatePlayerStuckInCollisionTest(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface,
            f32 lfTimeStep);
        void UpdatePlayerStuckInCollisionSpheres(f32 lfTimeStep);
        void ReadPlayerStuckTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator* lpResultIterator);

        // @0x82629CE0 (DWARF :308; 78 insns).
        // STILL GATED 2026-08-10 (ground wave) and here is the measurement, so nobody re-derives
        // it: this calls its six callees UNCONDITIONALLY, and the two that build the commands
        // (AddRaceCarTractionLineTests @0x825E9640, PhysicalTrafficManager::AddTrafficTraction-
        // LineTests @0x8261D580) read the triangles they test against out of a per-object TRIANGLE
        // CACHE -- they assert "lpCacheInterface != NULL" / "mpTriangleCacheManager != NULL" /
        // "mpaTriangleCache != NULL" and then dereference it.
        // the previous text claimed
        // "CgsSceneManager::TriangleCacheManager is NOT in this tree (~1,688 console insns
        // across [all six methods])". It IS in the tree and its TU has been mounted throughout;
        // the whole READ side these two callers use (Prepare 88 / GetTrianglesForCachedObject 32 /
        // TriangleCacheInterface::GetCache 27 / GetNumCachedTriangleBatches 31) was already
        // bodied, and the slot bookkeeping (ProcessAddToCacheEvents 222 /
        // ProcessRemoveFromCacheEvents 346 / ProcessUpdateCachedPositionEvents 279 /
        // CacheSlot::UpdateCachedObject 54) landed this wave.
        // The real blocker is narrower and is the FILL half. RE-MEASURED 2026-08-10 (producer
        // wave) -- three of the five names this paragraph used to list have LANDED since:
        // StartUpdateTriangleCaches 278, EndUpdateTriangleCaches 475 and SceneManagerModule::
        // StartUpdateTriangleCache 73 are all bodied and run every frame, and PrepareTriangleCache
        // 37 (the one that registers a CAR with the cache at all) is bodied too -- `usedSlots=28`
        // is runtime-witnessed. What is left, in the order it must happen:
        //   1. VehicleManager::ProcessCreateEvents @0x82616770 (1,067) -- nothing sets
        //      mUsedRaceCars today (its only write in this tree is Construct's UnSetAll), so no
        //      car exists to be positioned;
        //   2. PhysicsModule::UpdateCachedPositions @0x8259C370 (34) + the six per-manager
        //      UpdateTriangleCache bodies (~1,029) -- what marks a slot DIRTY;
        //   3. the PolygonSoupTesterJob fill path (~1,183 across 11) -- what puts triangles in it.
        // Until those run, the cache is allocated and CLAIMED but holds ZERO batches, so this
        // would read a valid pointer and get an empty triangle list -- a [[silent-drop-stubs]]
        // result, not a crash. Still not landable.
        // SimpleVehiclePhysics::GetTractionLine @0x825D85C0 remains genuinely absent: re-verified
        // as a true export hole (dir gap 0x825D8490+76insns -> 0x825D8878; no name-index hit in
        // 30,084 exports), **174 instructions**, image-only.
        void StartVehicleTractionLineTests(CgsModule::IOBufferStack* lpInputBufferStack,
                                           const VehicleInputInterface* lpInputInterface,
                                           Deformation::DeformationManager* lpDeformationManager,
                                           f32 lfTimeStep);

        // DWARF h:1116; X360 @0x825EB350 (222 insns), PS3
        // DecFIGS 0x70F454 (the mangle is the parameter-order authority). THE HARVEST: drain
        // every pre-part contact-gen entry's CollisionResultList (result list i belongs to
        // entry i -- the two are appended in lockstep by the Do*ContactGeneration family) into
        // 76-byte PotentialContacts posted to maCustomEventQueues[UserTagA]. Called only by
        // EndVehicleContactGeneration.
        void AddContactResultsToQueue(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
            BrnPhysics::ContactGenList* lpContactGenList,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface);

        // @0x8261AC38 (DWARF dump :1055; 661 insns -- the async-generation harvest half of
        // StartVehicleContactGeneration; same seven parameters).
        void EndVehicleContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* lpOverlapPairs,
            f32 lfTimeStep,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
            CgsModule::IOBufferStack* lpIOBufferStack,
            CgsMemory::LinearMalloc* lpLinearMalloc,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface);

        // @0x8262C220 (DWARF :1058; 114 insns).
        void StartPartContactGeneration(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            f32 lfTimeStep,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
            CgsModule::IOBufferStack* lpIOBufferStack,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface,
            CgsMemory::LinearMalloc* lpLinearMalloc);

        // @0x8261B690 (DWARF :1061; 276 insns).
        void EndPartContactGeneration(f32 lfTimeStep,
                                      BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
                                      CgsModule::IOBufferStack* lpIOBufferStack,
                                      BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface);

        // @0x825EB6C8 (DWARF :1064; 416 insns).
        void DoRaceCarWorldContactValidation(
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            f32 lfTimeStep,
            BrnPhysics::Deformation::DeformationManager* lpDeformationManager);

        // @0x825C8F18 (DWARF :1067; 143 insns).
        void DoTrafficWorldContactOrdering(
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface);

        // @0x82645FE0 (DWARF :341; 814 insns + the L1/L2 web -- see the census banner below).
        void DoCrashPrediction(CgsModule::IOBufferStack* lpInputBufferStack,
                               CgsModule::IOBufferStack* lpOutputBufferStack,
                               f32 lfTimeStep,
                               const VehicleInputInterface* lpInputInterface,
                               VehicleOutputInterface* lpVehicleOutputInterface,
                               BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                               VehicleManagerOutputInterface* lpManagerOutputInterface,
                               BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                               BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface);

        // @0x82642C68 (DWARF :273; 120 insns -- the per-driver-type control dispatch).
        void UpdateDrivers(f32 lfTimeStep,
                           const VehicleDriverInputInterface* lpDriverInputInterface,
                           BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                           VehicleManagerOutputInterface* lpManagerOutputInterface,
                           BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                           VehicleOutputInterface* lpVehicleOutputInterface);

        // UpdateDrivers' own four callees. Every one is
        // DWARF-attested VERBATIM (references/DecFIGS/.../BrnVehicleManager.h:875/:878/:881/:1254)
        // and every one is X360-attested with the SAME register map the DWARF implies, read off
        // UpdateDrivers @0x82642D38..0x82642E28. ALL FOUR ARE STILL BODYLESS -- they are named
        // BRN_CONDUCTOR_GATEs in BrnPhysicsConductorGates.cpp (401 / 185 / 258 / 333 console
        // instructions), so declaring them here buys the drain's compile, not its behaviour.
        // The `BitArray<8u>&` is the per-frame "which race cars did a driver update touch" set that
        // UpdateDrivers builds on its own stack and hands to all four in turn.
        void UpdatePlayerDriver(const BrnPlayerDriverControls* lpControls,
                                CgsContainers::BitArray<8u>& lrUpdatedCars);   // @0x825E9F38 (401)
        void UpdateNetworkDriver(const BrnNetworkDriverControls* lpControls,
                                 CgsContainers::BitArray<8u>& lrUpdatedCars);  // @0x825C4D08 (258)
        void UpdateAIDriver(const BrnAIDriverControls* lpControls,
                            CgsContainers::BitArray<8u>& lrUpdatedCars);       // @0x825C5110 (185)

        // @0x8263CC68 (DWARF :1254; 333 insns). The horn-triggered instant-takedown sweep
        // UpdateDrivers runs after the queue drain. The four pointers are r4..r7 at the console
        // call site @0x82642E14..0x82642E24, which is exactly the DWARF's declaration order.
        void DoHornTakedowns(BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                             VehicleOutputInterface* lpVehicleOutputInterface,
                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x8261A8D0 (DWARF :679; 217 insns).
        void ClearSnappedNetworkCarContacts(Deformation::DeformationManager* lpDeformationManager);

        // @0x82619340 (DWARF :361; 435 insns -- was an .ida-exports hole, pulled headless from
        // the IDB 2026-09-02). Harvest every live vehicle body into the sim's
        // InUpdateExternalBody queue: ONE event per live race car, addressed to the car's
        // E_ENTITYTYPE_PROP_COLLISION_RACECAR (11) proxy body (the handling-body handle with the
        // owner byte re-stamped), velocities divided by the AI-crash slow-mo factor while
        // mbAISlowMo is set (and mbWroteIntoRWInSlowMo latched either way); then the traffic
        // twin. Body: BrnVehicleManager_GetUpdatedVehicleBodies.cpp.
        void GetUpdatedVehicleBodies(
            CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>* lpUpdatedBodyQueue);

        // @0x826426E0 (DWARF :353; 354 insns -- the post-sim per-car pass; the seam that
        // publishes each car's stepped transform back to the game side).
        void UpdateVehiclePhysicsPostSimulation(
            const VehicleInputInterface* lpInputInterface,
            const CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutputBuffer,
            f32 lfTimeStep,
            BrnGameState::GameStateModuleIO::GameEventQueue* lpGameEventQueue);

        // @0x8263C7C0 (DWARF :235 -- .ida-exports HOLE, image-only).
        void ProcessCrashingNetworkCars(const VehicleDriverInputInterface* lpDriverInputInterface,
                                        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                        VehicleManagerOutputInterface* lpManagerOutputInterface,
                                        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                        VehicleOutputInterface* lpVehicleOutputInterface);

        // @0x8263F460 (DWARF :393; 380 insns).
        void WriteOutVehicleStats(VehicleOutputInterface* lpVehicleOutputInterface);

        // @0x82617820 (DWARF :220; 526 insns).
        void ProcessResetEvents(const VehicleInputInterface* lpInputInterface,
                                BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                VehicleManagerOutputInterface* lpManagerOutputInterface,
                                BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x82646C98 (DWARF :385; 118 insns -- the driver over the four typed contact runs).
        void ProcessContactSpies(const ContactSpy::ContactSpyData* lpContactSpyData,
                                 BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                 VehicleOutputInterface* lpVehicleOutputInterface,
                                 VehicleManagerOutputInterface* lpManagerOutputInterface,
                                 BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                 Deformation::DeformationManager* lpDeformationManager,
                                 f32 lfTimeStep);

        // @0x825EA970 (DWARF :397; 173 insns).
        void UpdateFatalCrashFlags(VehicleOutputInterface* lpVehicleOutputInterface);

        // The per-contact working set the impact classifiers read/populate. Verbatim DWARF
        // layout (BrnVehicleManager.h:763). Pointer members use the forward-declared collaborators.
        struct RaceCarResponseInfo
        {
            BrnPhysics::ContactSpy::RaceCarContact*           mpContact;                  // +0x00
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* mpRequestOutputInterface; // +0x04
            // RE-TYPED 2026-08-11: DWARF (BrnVehicleManager.h:766) spells this member
            // `VehicleOutputInterface*` unqualified inside BrnPhysics::Vehicle::VehicleManager,
            // and no GameStateModuleIO::VehicleOutputInterface exists in the DWARF at all.
            BrnPhysics::Vehicle::VehicleOutputInterface*      mpVehicleOutputInterface;  // +0x08
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
                                         BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                         BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
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
                             BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                             VehicleManagerOutputInterface* lpManagerOutputInterface,
                             BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                             BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                             BrnGameState::ETakedownType leTakedownType);

        // --- declare-only callees (bodied by their own TUs / not in this dossier) ---------------
        // The crash commit itself (DWARF h:1218; 9-param TU, X360 @0x82634C90).
        void SetRaceCarCrashing(EntityId lVictimEntityId,
                                EntityId lAggressorEntityId,
                                Vector3 lCollisionNormal,
                                Vector3 lContactPoint,
                                BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
                                VehicleManagerOutputInterface* lpManagerOutputInterface,
                                BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                                BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
                                BrnGameState::ETakedownType leTakedownType);

        // The shunt/slam force-physics appliers (X360 ApplyShunt @0x8261A5B0, ApplySlam @0x8261A738).
        // DWARF/asm shape is (VehicleManager* this, RaceCarResponseInfo* lpInfo).
        // BODIED 2026-08-24 (physics mount wave B3b) in BrnVehicleManager_ImpactHelpers.cpp,
        // together with their calculate/report callees below.
        void ApplyShunt(RaceCarResponseInfo* lpInfo);
        void ApplySlam(RaceCarResponseInfo* lpInfo);

        // The slam/shunt data derivations + the network impact report (X360 CalculateSlamData
        // @0x825C7568, CalculateShuntData @0x825C7880, SendImpactMessage @0x825EACF0). Signatures
        // are asm-authoritative: the out-parameters are the stack slots the X360 callers pass in
        // r5..r10 (+ one stack arg for CalculateSlamData's vulnerable-time out). Bodied in
        // BrnVehicleManager_ImpactHelpers.cpp (wave B3b).
        void CalculateShuntData(RaceCarResponseInfo* lpInfo,
                                Vector3* lpvWorldDirection,
                                VecFloat* lpvfMagnitude,
                                f32* lpfVulnerableTime,
                                f32* lpfSteeringDirection);
        void CalculateSlamData(RaceCarResponseInfo* lpInfo,
                               f32* lpfDuration,
                               f32* lpfCounterDuration,
                               f32* lpfSteeringDirection,
                               f32* lpfRecoveryTime,
                               Vector3* lpvDirection,
                               u8* lpu8Score,
                               f32* lpfVulnerableTime);
        void SendImpactMessage(VehicleOutputInterface::ImpactEventQueue* lpImpactEventQueue,
                               EImpactType leImpactType,
                               EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                               EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                               Vector3 lvDirection,
                               f32 lfMagnitude, f32 lfDuration,
                               f32 lfSteeringDirection, f32 lfRecoveryTime,
                               u8 lu8Score);

        // Per-victim "does this impact qualify to crash the car" predicate (consumed by #1/#2).
        // ⭐ THE 3-ARGUMENT INFERRED SPELLING THAT USED TO SIT HERE IS DELETED (2026-08-25). It was
        // an ODR FORK of the real console entry, whose one true declaration is the 5-argument
        // DWARF/ARTIST shape further down this class (@0x825C6FF8) -- already bodied in
        // BrnVehicleManager_RaceCarTrafficContact.cpp:157. All four call sites in
        // BrnVehicleManager.cpp (CheckForHittingAlreadyCrashingCar x2,
        // CheckForPlayerSlammingAIIntoAI x2) now build the two vector arguments from asm and call
        // that one. The stop-gap CGS_ASSERT(false) trap in BrnVehicleManager_ImpactHelpers.cpp is
        // deleted with it. See the 5-arg declaration below for the decode.

        // The vertical-takedown geometric sub-test (X360 @0x825C56D8). SIGNATURE CORRECTED
        // 2026-08-24 (wave B3b): the old FLAGGED guess (lpVictim, lpOther) was wrong -- the asm's
        // second argument is the CONTACT POINT in v1, and the test asks whether it falls inside
        // 80% of the victim's deformable-AABB footprint. Bodied in
        // BrnVehicleManager_ImpactHelpers.cpp.
        bool CheckForVerticalTakedownSituation(RaceCarPhysics* lpVictim, Vector3 lvContactPoint);

        // The parallel-plane containment test (X360 @0x825C5660). SIGNATURE CORRECTED 2026-08-24
        // (wave B3b): the asm takes FOUR vector args -- the point, one point on each plane, and
        // the shared plane NORMAL in v4 (the old 3-arg guess was FLAGGED inferred and wrong).
        // True when the two plane-side dot signs differ. `this` is unread. Bodied in
        // BrnVehicleManager_ImpactHelpers.cpp.
        bool IsPointBetweenTwoParallelPlanes(Vector3 lvPoint, Vector3 lvPlaneA, Vector3 lvPlaneB,
                                             Vector3 lvPlaneNormal);

        // The recency throttle (X360 @0x825B4EB8): mafNoImpactTimeSeconds[index] > 0. BODIED
        // 2026-08-24 (wave B3b) in BrnVehicleManager_ImpactHelpers.cpp.
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

        // DWARF :1025 -- resolve a GLOBAL entity
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
        // returns &maRaceCarVehicles[index]; owner==TRAFFIC_VEHICLE (2) delegates to the contained
        // PhysicalTrafficManager.
        //
        // WRONG COMMENT CORRECTED 2026-08-19 (wave Q7, cluster `carcar`) -- it read: "Returns an
        // untyped body pointer (the two branch types -- RaceCarPhysics : VehiclePhysics and
        // SimpleVehiclePhysics : ExternalPhysicsBody -- share no base, matching the X360's
        // raw-pointer return)". THEY DO SHARE A BASE, and it is the whole point of the accessor:
        // `struct VehiclePhysics : public SimpleVehiclePhysics` (VehiclePhysics.h:164), so
        // RaceCarPhysics IS-A SimpleVehiclePhysics and both branches return one. The DWARF types the
        // console accessor accordingly -- `SimpleVehiclePhysics* GetVehiclePhysics(EntityId)`
        // (dwarfdump BrnVehicleManager.h:1299) -- and its callers rely on the shared prefix
        // (DoCarCarContactGeneration reads mbFrozen and mLinearVelocity off either branch with no
        // per-branch test at all).
        //
        // The `void*` return is therefore a TREE ARTEFACT, not console truth, and it is kept ONLY
        // because narrowing it would change the signature of a definition this cluster does not own
        // (the body is in BrnVehicleManagerPlayerStats.cpp:167). Callers static_cast to
        // SimpleVehiclePhysics*. FIX-WHEN that TU is next opened: return SimpleVehiclePhysics* and
        // drop the casts. (The truncated NAME is the IDA symbol's; that one is deliberate.)
        void* GetVehiclePhysi(EntityId lPhysicsVehicleId);

        // @0x825C3040: mark a NETWORK race car hidden for at least luFrames frames (sets its bit in
        // mHiddenNetworkRaceCars and stores luFrames into maHiddenForFrames[index]).
        void SetNetworkRaceCarHidden(EActiveRaceCarIndex leActiveRaceCarIndex, s32 liFrames);

        // SetAllNetworkRaceCarsHidden @0x825E9380 (175 insns) and AddRaceCarDeformationModel
        // @0x825E9118 (153) are BOTH DECLARED ABOVE, with the create arm's other callees -- see the
        // block above maRaceCarDrivers. A duplicate declaration of the first, and a note explaining
        // why the second was deliberately left undeclared, stood here after the 2026-08-11 merge of
        // the two create-drain waves; both are retired. The second wave's reason for not declaring
        // AddRaceCarDeformationModel (that DeformationInputInterface::AddDeformationModel's first
        // two parameters are 4-byte `Deformation::ResourceHandle` / `RigidBodyId` slices while the
        // console loads EIGHT bytes for each) is REAL and is now recorded where the body lives, in
        // BrnVehicleManager_MaintenanceEvents.cpp -- with the reason our body is not the truncating
        // cast that concern describes.

        // @0x8259C028: store the local player's active-race-car slot (gated 0..7).
        void SetPlayerActiveRaceCarIndex(EActiveRaceCarIndex lePlayerActiveRaceCarIndex);

        // @0x8259C098: store the current showtime behaviour mode (gated 0..2).
        void SetShowtimeBehaviour(u32 luShowtimeBehaviour);

        // @0x8259C108: drive the player car into (or out of) showtime: forwards the cached showtime
        // strength/damage-limit to RaceCarPhysics::SetPlayerVehicleInShowtime on the player car and
        // latches the global player-in-showtime byte.
        void SetPlayerCarToShowtimeMode(bool lbInShowtime);

        // ==========================================================================================
        // ⭐ ADDED 2026-08-27 (showtime S3 wave). The five leaf methods PhysicsModule::
        // HandleGameActions @0x825A72F0 calls out of its game-action switch. Every one is a
        // handful of instructions read straight out of the X360 asm; none of them was declared
        // anywhere in the tree before this wave, which is part of why the 185-insn dispatch above
        // them stayed a boot gate. Bodies: BrnVehicleManagerPlayerStats.cpp.
        // ==========================================================================================

        // @0x8262AEC8 (7 insns). `lwzx r11,r3,0x2A0AC ; mulli 0x1460 ; addi 0x740 ; b
        // VehiclePhysics::SwitchAIDonuttingAttribs` -- the player car's own AI-donutting attrib
        // swap. 0x1460/0x740 are exactly the stride/base of maRaceCarVehicles (the same pair
        // SetPlayerCarToShowtimeMode above resolves), and 0x2A0AC == +172204 ==
        // mePlayerActiveRaceCarIndex, so this is spelled by name with no offset arithmetic.
        void SwitchPlayerAIDonuttingAttribs(bool lbDonutting);

        // @0x825B5708 (4 insns). `stwx r4, r3, 0x2A15C` -- meCurrentGameModeType = the mode type.
        // ⭐ THAT MEMBER (+172380) IS THE ONE ProcessDeformationStates GATES SHOWTIME ON
        // (2 == E_MODE_OFFLINE_SHOWTIME, 16 == E_MODE_ONLINE_SHOWTIME). Before this wave nothing
        // in the tree wrote it, which is why the showtime-gated deformation arm could never run.
        void OnGameModePrepare(s32 leGameModeType);

        // @0x825B5718 (5 insns). `li r10,-1 ; stwx r10, r3, 0x2A15C` -- meCurrentGameModeType = -1.
        // ⚠️ The call site passes r4 (`lwz r4, 0(r29)`); Hex-Rays drops it and the body ignores it.
        // Declared without the argument because the CONSOLE BODY takes none -- the caller's extra
        // register write is a dead store, not a parameter. [[invented-arms-and-the-c4715-ratchet]]
        void OnGameModeStop();

        // @0x825B5730 (8 insns). `stbx 1 -> +0x2A110 ; stbx r5 -> +0x2A11A`, i.e.
        // mbImpactTime = true ; mbAftertouchIsForceAdditive = lbForceAdditive.
        // ⚠️⚠️ HEX-RAYS RENDERS THE CALL AS `StartImpactTime(vm, *_R29)` -- ONE ARGUMENT, WRONG
        // TYPE. The asm at the call site is `lfs f1, 0(r29) ; lbz r5, 4(r29)`: a FLOAT in f1 and a
        // BYTE in r5. On this ABI a float argument consumes its GPR slot, which is exactly why the
        // byte lands in r5 and not r4 -- that displacement is the proof the float parameter is
        // real. The body never reads f1; the duration is accepted and dropped by the console
        // itself, so it is (void)-cast here rather than deleted from the signature.
        void StartImpactTime(f32 lfImpactTimeDuration, bool lbForceAdditiveAftertouch);

        // @0x825B5750 (8 insns). mbImpactTime = false ; mbAftertouchIsForceAdditive = false.
        void EndImpactTime();

        // ==========================================================================================
        // DoCrashPrediction @0x82645FE0 (814 insns) -- BODIED 2026-08-22 (wave T3 r2 owner B fix
        // round) in BrnVehicleManager_DoCrashPrediction.cpp. The 2026-08-09 census below is kept
        // only as the closure map; every "absent from the tree" claim in it is now STALE.
        // Spine, in ASM order (0x82645FE0..0x82646C90):
        //   asserts x8 -> muCachedCarA/BSlot = 0 -> AllocateInternalBuffers -> the stack-local
        //   PotentialContactAverager (sp+0x150, count sp+0x7E0) -> mDiscardedContacts.Clear()
        //   -> queue[13] traffic-vs-traffic owner check + HandleTrafficCarTrafficCarPotentialContact
        //   -> queue[8] race-car-vs-traffic -> DoCrashPredictionForRaceCarAndTrafficVehicle
        //   -> HandleCrashPredictionForRaceCarAndTrafficVehicle
        //   -> HandleCrashPredictionForRaceCarAndWorld
        //   -> the ~410-insn VMX128 triangle-cache block that CLEARS mbForceNoSlowMo
        //      (0x82646450..0x82646B8C -- it sits AFTER the world arm, not in the middle; the old
        //      "its middle is the VMX loop" reading below was wrong)
        //   -> queue[9] traffic-vs-world -> HandleTrafficCarWorldPotentialContact
        //   -> BridgeArticulatedJointRequestsToSim -> DeallocateInternalBuffers.
        // The two BasePriorityQueue::Clear calls are the assert StrStream constructions at
        // :2911/:2912, not queue drains. sub_8259D670 / CgsScen are EventQueue<T>::GetEvent(s32).
        // ONE NAMED GATE remains inside the landed body, not on the race-car-vs-traffic path:
        //   the mbForceNoSlowMo triangle-cache clear                      -- no cache reader yet
        // RETIRED 2026-09-02 (traffic crash wave): HandleTrafficCarTrafficCarPotentialContact
        //   @0x8263EC90 and HandleTrafficCarWorldPotentialContact @0x8263F0F0 are bodied in
        //   BrnVehicleManager_TrafficCrashArms.cpp and called from queue [13] / queue [9].
        // RETIRED 2026-09-02 (crash wave): HandleCrashPredictionForRaceCarAndWorld @0x82640C28 is
        //   mounted and CALLED -- its two callees (HandleRaceCarWorldPotentialContact @0x8263E3B8,
        //   PredictCarWorldContactTime @0x825B5300) are bodied in BrnVehicleManager_WorldCrashArm.cpp.
        // DWARF signature (BrnVehicleManager.h:899): as declared at :1019 above.
        // ==========================================================================================

        // ==========================================================================================
        // Race-car-vs-world crash-prediction slice (X360 @0x82640C28). DWARF-authoritative
        // signatures (BrnVehicleManager.h:1296/1200/1215). HandleCrashPredictionForRaceCarAndWorld
        // walks the interface's already-validated race-car-world potential-contact queue, groups the
        // contacts by their volume-A entity word (VolumeInstanceId high dword), orders each group by
        // predicted impact time via PotentialContactOrderer, and dispatches every surviving contact
        // to HandleRaceCarWorldPotentialContact. The two callees are bodied by their own TUs; declared
        // here (declare-only) so this slice can call them.
        // ------------------------------------------------------------------------------------------
        // @0x82640C28 -- the crash-prediction driver bodied by BrnVehicleManagerCrashPrediction.cpp.
        // FORK RETIRED 2026-09-02 (crash wave): arg 4 was the phantom
        // `BrnGameState::GameStateModuleIO::VehicleOutputInterface*` (see the banner at the top of
        // this header); the DWARF mangle is `PNS0_22VehicleOutputInterfaceE` == BrnPhysics::Vehicle::,
        // the same type SetRaceCarCrashing takes, and the arm now forwards it there unchanged.
        void HandleCrashPredictionForRaceCarAndWorld(
            f32 lfTimestep,
            BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
            const VehicleInputInterface* lpVehicleInputInterface,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // BODIED 2026-09-02 (crash wave) in BrnVehicleManager_WorldCrashArm.cpp -- these two were
        // declare-only for the whole life of the tree, which is exactly why the world arm above was
        // unmountable and a wall could not crash the car. Signatures are DWARF-authoritative
        // (:1200 / :1215), with the same fork retirement on arg 3 as above. The tri-cache arg is the
        // nested VehicleInputInterface::InTriangleCacheInterface, which is
        // CgsSceneManager::SceneManagerIO::TriangleCacheInterface; the console body never reads it.
        void HandleRaceCarWorldPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
            f32 lfTimestep);

        VecFloat PredictCarWorldContactTime(const CgsSceneManager::SceneManagerIO::PotentialContact& lContact);

        // BODIED 2026-09-02 (traffic crash wave) in BrnVehicleManager_TrafficCrashArms.cpp -- the two
        // traffic-side arms DoCrashPrediction dispatches from queue [13] / queue [9]. Signatures are
        // the DecFIGS mangles verbatim (0x797750 / 0x7972AC); DoCrashPrediction's own outgoing-arg
        // stores (r19 request / r20 vehicle-out / r17 manager-out / r21 deformation, f1 timestep)
        // confirm the order. Neither body reads the request interface; the WORLD arm reads none of
        // the four and commits nothing on the console (see that TU's banner before "fixing" it).
        void HandleTrafficCarTrafficCarPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            f32 lfTimestep);

        void HandleTrafficCarWorldPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            f32 lfTimestep);

        // ==========================================================================================
        // WAVE T3 ROUND 2, OWNER B -- the RACE-CAR-vs-PHYSICAL-TRAFFIC contact branch of the
        // crash-prediction web. Bodied in BrnVehicleManager_RaceCarTrafficContact.cpp; signatures
        // verbatim from the DecFIGS DWARF (BrnVehicleManager.h :1290 / :1293 / :1143 / :1197 /
        // :1194 / :1286) and confirmed against the ARTIST prologues.
        //
        // THE FLOAT ARGUMENT SKIPS A GPR. HandleCrashPredictionForRaceCarAndTrafficVehicle takes
        // (averager, float32_t, veh, req, mgr, deform) and the console loads r4=averager, r6/r7/r8/r9
        // for the four interfaces with **r5 unused** and f1 carrying the timestep -- the standing
        // PPC float-arg GPR-skip. That is why the DWARF's 2nd parameter looks "missing" in the asm.
        // ==========================================================================================

        // @0x82643D30 (159). Validate one race-car-vs-traffic potential contact, fold it into the
        // averager, and FLUSH the averager (through the Handle* driver below) when it is full.
        void DoCrashPredictionForRaceCarAndTrafficVehicle(
            PotentialContactAverager* lpContactPairAverager,
            const CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
            f32 lfTimestep,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x82640AB0 (93). Drain every averaged pair through HandleRaceCarTrafficCarPotentialContact
        // and reset the averager.
        void HandleCrashPredictionForRaceCarAndTrafficVehicle(
            PotentialContactAverager* lpContactPairAverager,
            f32 lfTimestep,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface);

        // @0x8263FA50 (783; export HOLE closed by the wave-T3 scout). THE handler: decide the
        // outcome of one race-car-vs-traffic contact and commit it.
        void HandleRaceCarTrafficCarPotentialContact(
            CgsSceneManager::SceneManagerIO::PotentialContact lContact,
            BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
            BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
            VehicleManagerOutputInterface* lpManagerOutputInterface,
            BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
            f32 lfTimestep);

        // @0x825C70A0 (305). The classifier: crash / check / slam / nothing, plus the slam
        // magnitude the SLAMMED arm scales its event by.
        void DecideOutcomeOfRaceCarTrafficContact(u16 luActiveRaceCarIndex, u16 lu16TrafficCarIndex,
                                                  Vector3 lContactNormal, Vector3 lPointOnRaceCar,
                                                  Vector3 lPointOnTraffic,
                                                  VecFloat* lpTrafficSlamMagnitude,
                                                  u32* lpxOutResponseFlags);

        // @0x825C6FF8 (42). The impact-severity predicate, DWARF shape (:1194). THE ONLY
        // DECLARATION -- the ODR fork that used to shadow it is gone (see the note above).
        // The DWARF and the ARTIST prologue BOTH give five parameters -- r4 = victim index,
        // r5 = victim RaceCarPhysics, r6 = the OTHER body (a SimpleVehiclePhysics, which is what a
        // traffic car is), v1 = the impact speed, v2 = a per-arm scale.
        //
        // ⭐ THE DELETE-WHEN IS DISCHARGED (2026-08-25). All four car-vs-car call sites are
        // re-decoded onto this spelling; the two vector builds, read from asm, are:
        //   CheckForHittingAlreadyCrashingCar @0x8263DC30 / @0x8263DE40 (both arms symmetric):
        //     v1 = |dot3( mNormal - victimUp*dot3(mNormal, victimUp), mClosingVelocityAtoB )|
        //     v2 = splat(1.0f)                     [vspltisw v124,1 ; vcsxwfp128 v127,v124,0]
        //     -- the SAME flat-normal closing-speed the traffic arm builds
        //        (BrnVehicleManager_RaceCarTrafficContact.cpp:306-312), with the VICTIM's Up axis.
        //   CheckForPlayerSlammingAIIntoAI @0x8263E1B4 / @0x8263E2CC:
        //     v1 = splat(lpInfo->mfClosingSpeed)   [lfs f0,0x58(r31) ; stfs ; lvx ; vspltw lane 0]
        //     v2 = <player is this car's recorded attacker> ? 2.0f : 0.75f
        // BOTH scale constants are READ FROM THE IMAGE, not guessed: 2.0f is materialised inline
        // (vspltisw v13,2 ; vcfsx v0,v13,0) and unk_82FB8320 = splat(flt_82004018) = 0.75f via its
        // static-init thunk @0x82C5BB60..BB84 (lis 0x8200 ; lfs 0x4018 ; lis 0x82FC ; addi 0x8320 ;
        // vspltw ; stvx) -- the same scalar the tree already reads for unk_82FB8310.
        // The selector is the shared 16-byte-PAIR vsel mask table at 0x8327F240, indexed
        // `cntlzw; rlwinm ..,31,27,27; xori 0x10`, whose polarity ([0]=false, [0x10]=all-ones=true)
        // is proven in BrnDeformationSensor.cpp:977 / BrnCrashTriangleCache.cpp:219.
        // ⚠ FLAG (the ONE thing still unmodelled): the attacker predicate itself reads three
        // RaceCarPhysics fields this tree does not declare -- +0x13E0 and +0x1150 (both compared
        // against mePlayerActiveRaceCarIndex) and lane .z of the vector at +0x1050 (gated
        // 0.25f > it, unk_82FB7F80 = splat(flt_8208F834)). Until those are homed, the slamming
        // call sites pass the flag-FALSE arm 0.75f; see BrnVehicleManager.cpp for why that is the
        // faithful majority arm and not an invented value.
        bool ShouldRaceCarCrashOnCarImpact(EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                           const RaceCarPhysics* lpVictim,
                                           const SimpleVehiclePhysics* lpOtherBody,
                                           VecFloat lvfImpactSpeed, VecFloat lvfScale) const;

        // @0x825C57B0 (565). Swept-box prediction: will these two cars actually meet inside the
        // step? NAMED GATE this round -- see the .cpp.
        bool PredictCarCarIntersection(const SimpleVehiclePhysics* lpBodyA,
                                       const SimpleVehiclePhysics* lpBodyB,
                                       f32 lfTimestep);

        // NOT AN X360 SYMBOL. HandleRaceCarTrafficCarPotentialContact emits this test TWICE
        // inline (0x82640510 / 0x826405B8, instruction-identical with the two cars swapped);
        // outlined here per the de-optimisation rule. STATIC because it reads nothing off the
        // manager -- only the body it is handed.
        static bool IsFrontCornerClip(const SimpleVehiclePhysics& lrBody, Vector3 lContactPoint);

        // ==========================================================================================
        // ⭐ ADDED 2026-08-27 (showtime S3 wave).
        // PhysicsModule::HandleGameActions @0x825A72F0 does NOT go through accessors for six of the
        // flags its switch writes -- it stores them with `stbx r11, r31, rN` off the PHYSICS MODULE
        // base, because on the console mVehicleManager is embedded at +0x4AA0 and the compiler
        // folded the two offsets together:
        //     this+190568 -> mbSlamsAndShuntsOn                    this+191413 -> mbCrashPlayerNextUpdate
        //     this+190569 -> mbAllowSlamsAndShuntsEffectsForRivals this+191417 -> mbTrafficCheckingAllowed
        //     this+191409 -> mbEasyCrashingEnabled                 this+191552 -> meStationaryPlayerWheelAngle
        // There is NO console accessor for any of them (the export name index has no
        // SetEasyCrashing / SetTrafficCheckingAllowed / ... at all), so inventing six setters would
        // be inventing API. Friendship reproduces the console's bare stores by NAME instead --
        // exactly the reason, and exactly the wording, already used for
        // `friend class VehicleManager;` in BrnPhysicalTrafficManager.h:953: routing them through
        // accessors would add asserts the console does not fire there.
        friend class BrnPhysics::PhysicsModule;

        // 2026-09-02 (crash wave). VehicleManagerDebugComponent::RecordCrashContact @0x825B7880
        // reads this class BARE through its mpVehicleManager -- `lwzx r9, mgr, 0x2A0AC`
        // (mePlayerActiveRaceCarIndex) and `lbz 0xE50(mgr + 0x1460*idx)` (maRaceCarVehicles[idx]
        // .mbCrashing) -- and the export set has NO getter for the player slot (only
        // SetPlayerActiveRaceCarIndex @0x8259C028 exists). Same reasoning as the grant above:
        // friendship reproduces the console's bare reads by name instead of inventing accessors.
        // The grant is mutual (the component already befriends this class for the slam/shunt seats).
        friend class VehicleManagerDebugComponent;

    private:
        // DecFIGS BrnVehicleManager.h:1454/:1459; both are X360-attested calls in
        // UpdateVehiclePhysicsPostSimulation @0x82642828/@0x82642C04.
        void DoPlayerTractionLineTestsPostSimulation(
            const VehicleInputInterface* lpInputInterface, f32 lfTimeStep);
        void DoPlayerStuckLineTests(const VehicleInputInterface* lpInputInterface);

        // ------------------------------------------------------------------------------------------
        // Deep VehicleManager data members, recovered by LAYOUT RECOVERY WITH PADDING from the X360
        // asm offsets. The full VehicleManager is ~172 KB across many parallel per-car arrays;
        // everything not modelled is opaque padding so each named member lands at its proven byte
        // offset (pinned by the offsetof asserts in _AssertLayout / _AssertLayoutPlayerStats). The
        // gate FAILS if any padding run is wrong, which is the signal.
        //
        // RE-SEATED 2026-08-03. Every member from the class head down to +44768 has now been
        // re-derived directly from `VehicleManager::Construct` @0x8263B7C8 (943 instructions) and
        // cross-checked against the DWARF's member ORDER (BrnVehicleManager.h:815-970), which lists
        // the same members in the same sequence. Four committed errors were corrected -- the whole
        // per-car driver array was 64 bytes too low, a stride-8 array was modelled at stride 4, a
        // car-TYPE array was named as a crash-STATE array, and three live bitsets were buried in
        // padding -- and nine members were newly pinned. Names below are the DWARF's wherever the
        // DWARF names that seat; the remaining "FLAG" names are still role-derived.
        //
        // Members reached by ABSOLUTE offset from inside a contained sub-object (the
        // PhysicalTrafficManager interior at +148128 / +149456) stay siblings here, because the X360
        // build folds them to absolute class offsets; that is deliberate, not an oversight.
        // ------------------------------------------------------------------------------------------
        //
        // ==========================================================================================
        // `VehicleManager::Construct` @0x8263B7C8 IS BODIED as of 2026-08-03, in
        //    BrnVehicleManager.cpp. All 943 instructions: the thirty monitors, the spine, the
        //    eight-car loop and all 91 tuning seats. The rest of this block is the recipe it was
        //    written from and is kept because it is the evidence; the two corrections THIS wave
        //    forced are recorded first, because both were claims this file made about itself.
        //
        // CORRECTION 1 -- "⇒ Construct is NOT blocked on link closure" (below) IS WRONG, and the
        // READY column of the blocker table is what misled it. READY there means "the real x64
        //    class FITS its X360 span, so the call is spellable BY NAME". It does NOT mean the symbol
        //    resolves. MEASURED by mounting BrnVehicleManager.cpp and reading the linker: of
        //    Construct's own callees, TWO are unresolved in the mounted build --
        //        BrnPhysics::StuntOffencesManager::Construct
        //        BrnPhysics::Vehicle::PhysicalTrafficManager::Construct
        //    -- and both BODIES EXIST (BrnStuntOffencesManager.cpp / BrnPhysicalTrafficManager.cpp);
        //    neither TU is in tools/build/build_game_exe.bat. Mounting the whole TU costs 15
        //    unresolved externals in total (the other 13 belong to the takedown chain that shares
        // this file -- see the note at the mount site in the build script for the list).
        //    ⇒ Construct IS blocked on link closure. It is blocked on TWO mount lines, not on
        //    reconstruction, which is a much better place to be -- but "not blocked" was false.
        //
        // CORRECTION 2 -- the blocker table's remaining row (RaceCarVehicleRecord vs
        //    VehiclePhysics) IS RETIRED, but not the way the table expected. The record is GONE:
        //    maRaceCarVehicles is the real `RaceCarPhysics[8]` and the host/console difference is
        //    carried as KU_HOST_DRIFT_AFTER_RACECAR_ARRAY. See the fold-in note further down.
        // AND IT HAD A COST THE TABLE DID NOT ANTICIPATE: embedding a POLYMORPHIC class by
        //    value made the already-mounted BrnPhysicsModule.cpp (which embeds a VehicleManager)
        //    odr-use the whole RaceCarPhysics vtable, which turned two long-standing DECLARE-ONLY
        //    virtuals into link errors -- SimpleVehiclePhysics::SetCrashing and the +0x10 slot
        //    (then role-named "IsIgnoringPassedOnImpulses"; image-settled 2026-08-09 as
        //    VehiclePhysics::IsPlayerVehicleInShowtime, whose recovered default retires that
        //    trap). SetCrashing's vtable-closure GATE still stands in its own TU.
        //
        // WHAT WAS STILL MISSING FOR `VehicleManager::Construct` @0x8263B7C8 -- MEASURED 2026-08-03,
        //    not estimated. The layout wave above mined this function for OFFSETS; this note records
        //    what re-reading all 943 instructions says about BODYING it, so the next wave starts from
        //    a measurement instead of from a label.
        //
        // NOT AN EXPORT HOLE, AND EVERY SUB-CONSTRUCTOR IS ALREADY BODIED. From the function's own
        //    `xrefs_from` (`.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8263B7C8.json`), the complete
        //    callee set is twelve symbols, and after 2026-08-03 all six real ones have bodies:
        //      CgsDev::PerfMonCpu::AddMonitor                @0x82824C30  (declared, PC-leaf)
        //      VehicleManagerDebugComponent::Construct       @0x825B5A78  bodied
        //      VehicleDriver::Construct                      @0x825B83C8  bodied
        //      VehiclePhysics::Construct                     @0x8262DBD0  bodied THIS wave
        //      PhysicalTrafficManager::Construct             @0x82636CA8  bodied
        //      StuntOffencesManager::Construct               @0x825E8C08  bodied
        //    plus __savegprlr_14 / __savefpr_22 / __restfpr_22 and the three CgsDev::Assert entries.
        // THE SENTENCE THAT STOOD HERE -- "⇒ Construct is NOT blocked on link closure" --
        //    IS FALSE; see CORRECTION 1 at the top of this block. "bodied" in the list above means
        //    the body EXISTS SOMEWHERE IN THE TREE, and for two of the six the TU that holds it is
        //    not mounted, so the symbol does not resolve. Construct IS blocked on link closure, on
        //    exactly two mount lines (BrnStuntOffencesManager.cpp, BrnPhysicalTrafficManager.cpp).
        //    MEASURED with the linker 2026-08-03, not reasoned.
        //    Its only caller is PhysicsModule::Construct @0x825AE308, which is still a
        //    WorldLinkStubs stub for exactly two reasons: this function, and
        //    PhysicsSimulationModule::Construct.
        // BUT DO NOT STOP READING HERE. "Has a body" is not "can be called": three of those
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
        // this note used to insist "it constructs the
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
        // ~601..943 the TUNING BANK -- **RESOLVED 2026-08-03**, see the big block further down
        //              at `mbSlamsAndShuntsOn`. Corrections to the sizing that stood here before:
        //              it is **89** indexed `st{fs,b,w}x` stores hitting **89 DISTINCT seats** (one
        //              store per seat, no duplicates), not "96 stores / 88 seats" -- the apparent
        //              duplicate was a stack-spill reload of the offset register (`stw r10,var_150`
        //              @0x8263C278, `lwz r8,var_150` @0x8263C38C) that a naive scan mis-resolves.
        //              There are also **two VECTOR stores** the old sizing missed entirely:
        //              +172432 <- the 16 bytes at `unk_82181520` ({0,0,1,0}), and +172592 <- 16 zero
        //              bytes. All 89 + 2 seats are now declared, named and defaulted.
        // AND THE OLD NOTE'S METHOD ADVICE WAS WRONG: it said the ~40 constants
        //              "must be read off the asm" because "a literal scan of pseudocode will not
        //              find them", and warned they might be silent-zero .data. In fact this
        //              function's Hex-Rays renders every literal directly (`*(_R31 + 171468) = 4.0;`)
        //              -- the export has a GOOD pseudocode, unlike VehiclePhysics::Construct's. No
        //              image read was needed. There are 33 distinct scalars, not ~40.
        //
        // DO NOT SHIP A PARTIAL Construct. A body that runs the first ~510 instructions and skips
        //    the tuning bank would leave every takedown/slam/shunt threshold at zero while LOOKING
        //    complete -- the silent-drop-stub failure class. Either the tuning bank lands with it or
        //    the function stays unbodied. (The bank's DEFAULTS are now recorded member-by-member
        //    below, so the body can be written straight off this header.)
        //
        // ==========================================================================================
        // AND HERE IS WHY IT IS STILL UNBODIED -- MEASURED 2026-08-03 (the Construct-blocker
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
        // VehicleDriver::Construct(&maRaceCarDrivers[i])              224         224   READY
        // VehicleDriver::Construct(&mPlayerAiDriver)                  224         224   READY
        // StuntOffencesManager::Construct(this + 44240)               464         464   READY
        // mDiscardedContacts bind (this + 160672)                    1296        1296   READY
        // PhysicalTrafficManager::Construct(this + 44768)          105840      105648   READY   <- 2026-08-03
        // VehicleManagerDebugComponent::Construct(+161968)           1328        1296   READY   <- 2026-08-03
        // RaceCarPhysics::Construct(&maRaceCarVehicles[i])           5008    (5216)  READY   <- 2026-08-03
        // (was "VehiclePhysics::Construct ... 4752 (rec 5216) ". The record is GONE and the
        //          call is by name on the real type. The two numbers no longer have to match: the
        //          difference is KU_HOST_DRIFT_AFTER_RACECAR_ARRAY == -1664. See the fold-in note.)
        //
        //     THREE of the READY rows were opaque byte arrays before and are typed now
        //     (mStuntOffencesManager, mDiscardedContacts, mPhysicalTrafficManager) or were already
        //     typed (maRaceCarDrivers / mPlayerAiDriver). Nothing moved that was not meant to: the
        //     compiled layout gate in BrnVehicleManager_layout_check.cpp is what proves it, and
        //     growing any of them by one byte fires its asserts (tamper-tested).
        //
        // THE PhysicalTrafficManager ROW WAS WRONG, and it was the biggest of the three
        // blockers. It read "105840 vs 103360, +2480". The 103360 was this class's own
        //       opaque-span guess, and it was 2288 bytes short -- the real X360 size is 105648,
        //       derived twice over (BrnPhysicalTrafficManager.h finding (4); the span's own header
        //       note already contradicted it by placing a manager member at 44768 + 104688). The
        //       genuine host overrun is +192, small enough to CARRY as
        //       KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER rather than to block on. The member is embedded
        //       by name and `mPhysicalTrafficManager.Construct()` is callable today.
        // ⇒ Lesson for the two rows still marked : **re-derive the span before trusting the
        //         verdict**. A blocker computed against an opaque byte array is a claim about the
        //         array, not about the class.
        //     * VehicleManagerDebugComponent grows 32 bytes because its base's vptr and its
        // `mpVehicleManager` both widen 4 -> 8. Unlike the traffic manager, this span is
        //       NOT a guess: 161968..163264 is bracketed by two asm-literal anchors (the Construct
        //       call at +161968 and the stride-1024 walk from +163264), so the +32 is real -- which
        //       is why it is CARRIED (KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT) rather than derived away.
        // AND A CLAIM WRITTEN HERE EARLIER IN THIS SAME WAVE WAS WRONG, recorded because
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
        // DONE TWICE, 2026-08-03 -- for the traffic manager (KU_HOST_DRIFT_AFTER_TRAFFIC_
        //       MANAGER) and for the debug component (KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT): both
        //       members are real, the two mis-identified "siblings" are retired, and every
        //       downstream assert still runs. Two of the three blockers are gone.
        // REMAINING WORK -- ONE item, and it is NOT symmetric with the two just done:
        //         - RaceCarVehicleRecord -> RaceCarPhysics: genuinely multi-wave. It is not a size
        //           argument at all. The ten named in-record fields below DO NOT EXIST as members of
        //           the reconstructed RaceCarPhysics/VehiclePhysics -- that class is not byte-pinned,
        //           it declares only the handful of members its own bodies touch. Swapping the record
        //           for the real type therefore does not "move" those fields, it DELETES them, and
        //           each one has to be re-recovered as a named member (with a DWARF-ordered seat)
        //           before any reader can be re-pointed.
        // PROGRESS 2026-08-03: **3 of the 10 are done**, and the whole
        //           RaceCarPhysics OWN-MEMBER BLOCK (all sixteen DWARF members, X360 +0x13F0..+0x1460)
        //           now exists, is seated, and is gated by a MOUNTED TU
        //           (RaceCarPhysics_layout_check.cpp). Its derivation closes independently on THIS
        //           class's 5216 stride. See the fold-in map over RaceCarVehicleRecord below for
        //           what is settled, what the remaining seven need (the VehiclePhysics /
        //           SimpleVehiclePhysics own blocks, a different class's layout) and the two live
        //           name conflicts that surfaced en route. Until then the record stays byte-pinned.
        //
        // THE TEMPTING WRONG FIX, written down so it is not re-invented: the 32-byte debug
        //       component overflow could be "absorbed" by shrinking the maRaceCarDebugComponent pad
        //       behind it (both are opaque, nothing reads either by name, and the tuning bank would
        //       keep its offsets). Do not. Construct stores &maRaceCarDebugComponent[i] into each car
        //       record at an asm-literal 1024 stride (`addi r27, r27, 0x400`); silently re-striding
        //       that array to make an unrelated call compile is inventing layout to buy a green
        //       build. If it is ever done it must be a deliberate, argued decision with its own gate.
        //
        // WHAT IS ALREADY DECODED, so the unblocking wave writes zero new decode. All of the
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
        // SEVEN of the 29 are GUARDED -- `if (global < 0) global = AddMonitor(...)` -- so
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
        // SEVEN per-record writes, not the six an earlier sizing banked.
        //
        //    (d) THE REST OF THE SPINE, in issue order:
        //          mePrepareStage = 0; meReleaseStage = 3;
        //          mUsedRaceCars = 0; mUsedRaceCarCrashesList = 0;
        //          VehicleManagerDebugComponent::Construct(this + 161968, this)   <- TWO args; the
        //              Hex-Rays renders it with none
        //          mRandom.Construct();
        //          <the 8-car loop>
        // PhysicalTrafficManager::Construct(this + 44768)   -- callable by name today:
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
        // RE-SEATED 2026-08-03 (VehicleManager layout wave). This array used to be declared as
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
        // STAND-IN RETIRED 2026-08-03. This slot used to be an opaque 224-byte
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
        // THE RECORD IS GONE -- FOLDED IN 2026-08-03. This slot carried
        // `RaceCarVehicleRecord maRaceCarVehicles[8]`, a byte-pinned 5216-byte stand-in with ten
        // named fields poking through opaque padding, for ten waves. It is now the real
        // `BrnPhysics::Vehicle::RaceCarPhysics[8]`.
        //
        // WHY THE BLOCKER TABLE ABOVE SAID THIS COULD NOT BE DONE, AND WHAT ACTUALLY CHANGED.
        // The table's row read "VehiclePhysics::Construct(&maRaceCarVehicles[i]) 4752 (rec
        // 5216)", and the reasoning under it was correct at the time: the ten field names did not
        // exist as members of the reconstructed classes, so swapping the type would DELETE them.
        // Two waves since then recovered the SimpleVehiclePhysics, VehiclePhysics and
        // RaceCarPhysics own-member blocks in full, and all ten now resolve to a DWARF-seated
        // member -- three of them to members that were never real (they were rows of the base
        // `mTransform`). The blocker retired itself; nothing here argues it away.
        //
        // AND THE TREE WAS ALREADY BROKEN IN THE DIRECTION THE TABLE FEARED. Before this wave
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
        // `mbCrashing` is `protected` and the RaceCarPhysics own block is `private`, so this
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
        // mRandom is at +16, NOT +8. CgsNumeric::Random is
        // `union { f32[8]; u32[8]; VectorIntrinsic[2] } + u64 muSeed(+0x20) + u32 index(+0x28)`
        // == 44 bytes but **16-byte aligned** because of the VectorIntrinsic[2] member, so it
        // cannot sit at +8 and its sizeof is 48. 16 + 48 == 64 == &maRaceCarDrivers[0]: the head
        // closes on three independently-attested numbers. (An earlier brief reached the right
        // +64 answer from the wrong arithmetic -- mRandom@8 -- which would have left an 8-byte
        // hole in a different place.)
        // ==========================================================================================
        s32                  mePrepareStage;    // +0   EPrepareStage (Construct: 0)
        s32                  meReleaseStage;    // +4   EReleaseStage (Construct: 3)
        // The eight bytes at +8 are NOT a modelled pad any more -- they are the alignment the
        // 16-aligned CgsNumeric::Random forces, and the compiler now inserts them itself. (The
        // class was carrying an explicit `mPad0008[8]` while mRandom was an opaque blob; with the
        // real 16-aligned type both the padding and the blob are redundant. CgsRandom.h was
        // alignas(8) until this wave -- see the note there.)
        CgsNumeric::Random   mRandom;           // +16  (sizeof 48; ends at 64)

        VehicleDriver        maRaceCarDrivers[8];   // +64      (224 * 8 = 1792; ends at 1856)
        // THE REAL TYPE as of 2026-08-03 -- see the fold-in note above. X360 5216 * 8 = 41728
        // ending at 43584; host 5008 * 8 = 40064 ending at 41920, which is what
        // KU_HOST_DRIFT_AFTER_RACECAR_ARRAY (-1664) carries for every member past it.
        RaceCarPhysics       maRaceCarVehicles[8];  // +1856

        // Per-car EntityId validation table @ +43584. Stride 4 (asm: 4*(idx+10896) == 4*idx+43584;
        // Construct seeds it from dword_82F2A3A4 through a stride-4 cursor). SetRaceCarCrashing
        // asserts the packed victim/aggressor id matches the stored id here. Spelling per the DWARF
        // (`EntityId[8] maRaceCarEntityIDs`).
        EntityId             maRaceCarEntityIDs[8];   // +43584 (4 * 8 = 32; ends 43616)

        // SPLIT 2026-08-11 (create-drain wave). This was `unsigned char mPadAA60[128]` with a
        // note saying "CgsResource::ResourceHandle has no committed home in this tree yet ...
        // DELETE-WHEN ResourceHandle lands". BOTH halves of that note were stale: the type HAS had
        // a committed home (GameShared/GameClasses/System/Resource/CgsResourceHandle.h) since the
        // resource-handle wave, and four bodies landed this wave reach these arrays by element.
        //
        // The DWARF names them at BrnVehicleManager.h:824/825, and the X360 addresses both by the
        // same pair of console strides in ProcessCreateEvents @0x82616A58..0x82616A68:
        //     addi r9, r27, 0x154C ; slwi r28, r9, 3 ; stdx <modelHandle>, r28, r24
        //     addi r8, r27, 0x1554 ; slwi r9,  r8, 3 ; stdx <gfxHandle>,   r9,  r24
        // 8 * 0x154C == 43616 and 8 * 0x1554 == 43680 -- two 8-element arrays at CONSOLE stride 8,
        // back to back, closing on 43744 exactly. ProcessValidationEvents @0x825E9078..0x825E90B0
        // writes the same two seats, and AddRaceCarDeformationModel @0x825E91A0 double-derefs the
        // model one (`lwzx r11,r20,r29 ; lwz r11,0(r11)` -- the SafeResourceHandle shape,
        // mpResourceMemory then the SmallResource's main-memory pointer).
        //
        // THE SECOND WAVE READ THE SAME DOUBLE-DEREFERENCE AT A DIFFERENT SEAT and reached the
        // same conclusion: `lwzx r11,r28,r24 ; lwz r28,0(r11)` @0x82616CF4/CFC, feeding the wheel
        // loop's StreamedDeformationSpec. Two call sites, one shape -- and a byte span cannot carry
        // either of them without an offset poke.
        //
        // THE HOST STRIDE IS 16, NOT 8 ({void* mpResourceMemory, Entry* mpSourceEntry}), which
        // is exactly why these must be reached BY NAME: a body carrying the console's stride-8
        // arithmetic over an opaque span would index the wrong element with no diagnostic. The
        // +128 that costs is carried explicitly in KU_HOST_DRIFT_AFTER_MODEL_HANDLES above.
        CgsResource::ResourceHandle maRaceCarModelHandles[8];          // +43616 (console 64B, host 128B)
        CgsResource::ResourceHandle maRaceCarGraphicsModelHandles[8];  // console +43680 (host +43744)

        // this was committed as `EntityId
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

        // this was committed as `s32 maRaceCarCrashState[8]` with the note
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

        // NEWLY PINNED: the contained StuntOffencesManager subobject. Construct calls
        // `StuntOffencesManager::Construct(this + 65536 - 0x5330)` == this + 44240 @0x8263C620, and
        // the next pinned member (mHiddenRaceCars) is at 44704, so the subobject occupies exactly
        // 464 bytes.
        //
        // TYPED 2026-08-03 (was `unsigned char mStuntOffencesManager[464]`). This is the ONE
        // contained sub-object of VehicleManager whose real reconstructed class FITS ITS X360 SPAN
        // ON x64 -- measured, not assumed:
        // sizeof(BrnPhysics::StuntOffencesManager)  == 464   ==  44704 - 44240   
        // alignof(BrnPhysics::StuntOffencesManager) == 16,  and 44240 % 16 == 0  
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

        // NEWLY PINNED / RENAMED: FOUR RaceCarBitArrays, not one. Construct zero-stores all four
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
        // UN-PINNED 2026-08-03 -- THE CONTAINED PhysicalTrafficManager IS NOW A REAL NAMED
        //     MEMBER, and the two members that used to poke through it as "siblings" are gone.
        //
        // WHAT IT WAS: `unsigned char mPadAEE0[148128 - 44768]` (103360 bytes) followed by
        //     `EntityId maRaceCarEntityIdRemap[8]` @+148128 and
        //     `unsigned char mau8GlobalToPhysicalEntityIndexMap[600]` @+149456, on the theory that
        //     "the X360 build folds every contained-manager member the VehicleManager methods touch
        //     to its absolute class offset".
        //
        // THAT MODEL WAS SELF-CONTRADICTORY, and it is what produced the "+2480 overrun" verdict
        //     that has blocked VehicleManager::Construct. It declared the manager's span to END at
        //     +148128 while, four lines later, correctly identifying +149456 as the manager's own
        //     mu8GlobalToPhysicalEntityIndexMap "== 44768 + 104688" -- i.e. 2128 bytes PAST the end
        //     of the span it had just declared. The span was short by 2288 bytes.
        //
        // THE REAL X360 SIZE IS 105648 (KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER), derived twice
        //     over in BrnPhysicalTrafficManager.h finding (4): once forward from ten asm-literal
        //     anchors inside PhysicalTrafficManager::Construct @0x82636CA8, and once BACKWARD from
        //     this class -- 44768 + S + 128*sizeof(PotentialContact) + 4, padded to 16, must equal
        //     the asm-pinned mDiscardedContacts at +160672, which forces S == 105648 uniquely.
        //     Host sizeof is 105840 (measured), so the real overrun is **+192, not +2480**.
        //
        // AND THE TWO "SIBLINGS" WERE MIS-IDENTIFIED MEMBERS OF THIS OBJECT:
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
        // THE PRICE, PAID EXPLICITLY: every member after this one now sits
        //    KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER (192) bytes later on the host than on the X360.
        //    That is carried, not hidden: the three _AssertLayout* functions add the constant to
        //    every downstream offsetof, so they still fail if any padding run is wrong, and the
        //    constant itself is tripwired against sizeof(PhysicalTrafficManager) below.
        // Do NOT "absorb" the 192 by shrinking mPadNonPhysicalContacts. That span is the
        //    DWARF's maNonPhysicalContacts[128] + miNonPhysicalContactCount and its size is derived,
        //    not spare; shrinking it to buy back the X360 offsets would be inventing layout -- the
        //    same trap the maRaceCarDebugComponent note below warns about.
        // ==========================================================================================
        PhysicalTrafficManager mPhysicalTrafficManager;   // +44768 (X360 105648; host 105840)

        // PROMOTED 2026-08-06 (big-five #2, contact-generation wave) -- the old
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

        // NEWLY PINNED: the discarded-contact queue. Construct binds it in place @0x8263C048:
        //   addis r29,r31,2 ; addi r29,r29,0x73A0   -> this + 160672
        //   addi  r28,r29,0x10                      -> the buffer, this + 160688
        //   stw r28,0(r29) ; stw 0x14,4(r29) ; stw 0,8(r29)   -> {buffer, capacity 20, count 0}
        // plus the console's own `lpEventBuffer != NULL` assert (CgsBaseEventQueue.h:160). The
        // 16-byte header + 20 entries fills exactly to mDebugComponent, so each entry is 64 bytes.
        // DWARF: `ContactSpyData::DiscardedContactQueue mDiscardedContacts` (BrnVehicleManager.h:854).
        //
        // TYPED 2026-08-03 (was `unsigned char mDiscardedContacts[1296]`), and it fits for a
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

        // The manager's own debug component. Construct calls
        // `VehicleManagerDebugComponent::Construct(this + 161968, this)` @0x8263BCD8 -- note it takes
        // TWO arguments (r3 = the component, r4 = r31 = the manager); Hex-Rays renders it with none.
        //
        // UN-PINNED 2026-08-03 (was `unsigned char mDebugComponent[163264 - 161968]`). This was
        // the second of the three Construct blockers: real host class 1328 vs the 1296-byte X360
        // span, "+32". Both the old header and BrnVehicleManagerDebugComponent.h's own banner
        // concluded from that that "VehicleManager keeps its own mDebugComponent as the X360-sized
        // 1296-byte opaque span so its byte-pinned offsetof chain stays intact". That conclusion
        // followed only from the byte-pinning, and the byte-pinning is what this wave retired: with
        // KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT the chain stays intact WITH the real type in place.
        // Note the 1296 span itself was NEVER wrong (unlike the traffic manager's 103360) -- both
        // of its ends are asm-literal. The +32 is a genuine host/console width difference and it is
        // carried, not argued away.
        // ⇒ `VehicleManagerDebugComponent::Construct(this + 161968, this)` is now spelled
        //   `mDebugComponent.Construct(this);` BY NAME.
        VehicleManagerDebugComponent mDebugComponent;               // +161968 (X360 1296; host 1328)

        // NEWLY PINNED: the per-car debug components. Construct walks them in the 8-car loop with
        // `addis r27,r31,2 ; addi r27,r27,0x7DC0` -> this + 163264 and `addi r27,r27,0x400`
        // (stride 1024), storing each into the matching maRaceCarVehicles[] record and firing the
        // console's own `lpDebugComponent != NULL` assert. DWARF:
        // `BrnPhysics::Vehicle::DebugComponent[8] maRaceCarDebugComponent` (BrnVehicleManager.h:860),
        // immediately followed by `bool[8] mabRaceCarDebugComponentRegistered` (:861).
        // THE CHAIN CLOSES TO THE BYTE: 163264 + 8*1024 == 171456, + 8 == 171464, which is the
        // independently asm-proven offset of the gate byte below. Four numbers, one closure.
        unsigned char        maRaceCarDebugComponent[8][1024];        // +163264 (ends 171456)
        bool                 mabRaceCarDebugComponentRegistered[8];   // +171456 (ends 171464)

        // ==========================================================================================
        // THE TUNING BANK, RESOLVED 2026-08-03. Everything from +171464 to +172616 below is now
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
        // SEVEN COMMITTED ROLE-GUESSES WERE REFUTED. The offsets were all correct, so no byte
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
        // REFUTED ROLE: committed as `s32 miAttackerToRecord`. It is a FLOAT cooldown, and the
        // body that copies it into mafNoImpactTimeSeconds[victim] is starting that car's timer.
        f32 mfMinSecondsBetweenImpacts;             // +171540 = 0.3f   [was miAttackerToRecord (s32)]
        f32 mfMinAmountOfSlamForce;                 // +171544 = 0.2f
        f32 mfMinAmountOfShuntForce;                // +171548 = 0.25f
        // The one value X360 Hex-Rays carried in a register; PS3 (170880) gives it literally, and
        // flt_82001C98 is the same slot this function uses for the mCameraMatrix identity diagonal.
        f32 mfTailgatingVunerabilityTime;           // +171552 = 1.0f
        f32 mfBaseSlamMagnitude;                    // +171556 = 3.0f
        f32 mfBaseShuntMagnitude;                   // +171560 = 22.5f
        f32 mfTBoneTakedownMaxAngle;                // +171564 = 35.0f  [was mfTBoneAngleBandDegrees]
        // REFUTED ROLE: committed as `mfTBoneSidePlaneHalfWidth`. It is a SPEED, not a width.
        f32 mfTBoneTakedownSpeed;                   // +171568 = 30.0f  [was mfTBoneSidePlaneHalfWidth]
        f32 mfMaxShuntAngle;                        // +171572 = 25.0f
        f32 mfMinNudgeSpeed;                        // +171576 = 8.0f
        // REFUTED ROLE: committed as `mfNudgeMaxClosingSpeed` / `mfShuntMaxClosingSpeed`. They are
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
        // REFUTED ROLE: committed as `mfTradingPaintMaxSpeed`; it is the FATAL SLAM speed (and it
        // shares its value, 140.0f, with mfFatalShuntSpeed -- which is what made the guess plausible).
        f32 mfFatalSlamSpeed;                       // +171620 = 140.0f [was mfTradingPaintMaxSpeed]
        f32 mfFatalHitCrashingCarSpeed;             // +171624 = 50.0f
        f32 mfMaxHeadToHeadAngle;                   // +171628 = 45.0f  [was mfHeadToHeadAngleToleranceDeg]
        f32 mfMinHeadToHeadSpeed;                   // +171632 = 40.0f
        f32 mfMinHeadToHeadIndividualSpeed;         // +171636 = 40.0f  [was mfHeadToHeadMinClosingSpeed]
        f32 mfAngleForVerticleTakedown;             // +171640 = 60.0f  (DWARF's spelling)

        // ---- per-car impact bookkeeping (DWARF :923..:926) ---------------------------------------
        // REFUTED ROLE: committed as `f32 maRaceCarLastImpactMagnitude[8]`. It is the per-car
        // IMPACT TYPE enum array; the value written into it is the classifier's result, not a
        // magnitude. Same 32 bytes, same seats.
        EImpactType          maeImpactType[8];              // +171644 (32; ends 171676) [was maRaceCarLastImpactMagnitude]
        // REFUTED ROLE: committed as `maRaceCarTakenDownThisFrame[8]`; it is the per-car impact SCORE.
        unsigned char        mauImpactScore[8];             // +171676 (8;  ends 171684) [was maRaceCarTakenDownThisFrame]
        // REFUTED ROLE: committed as `s32 maRaceCarLastAttacker[8]`. It is a FLOAT per-car
        // "seconds until this car may be impacted again" countdown, seeded from
        // mfMinSecondsBetweenImpacts. HasRaceCarHadRecentImpact tests it > 0.0f -- which is why that
        // body already had to memcpy the s32 through to a float to stay byte-faithful.
        f32                  mafNoImpactTimeSeconds[8];     // +171684 (32; ends 171716) [was maRaceCarLastAttacker]
        signed char          maiPhysicsSlamIndex[8];        // +171716 (8;  ends 171724)
        f32                  mfContactDisplaySeconds;       // +171724
        EImpactType          meDisplayImpactType;           // +171728
        bool                 mbPlayerWonDisplayImpact;      // +171732
        unsigned char        mPad29F95[171736 - (171732 + 1)]; // 3 bytes; BitArray is 8-aligned
        // REFUTED ROLE: committed as `mTakenDownRaceCarsBitArray`. DWARF :934 calls it
        // mPlayerWonImpact -- a per-car "the player won this impact" bitset, not a taken-down set.
        CgsContainers::BitArray<8> mPlayerWonImpact;        // +171736 (8; ends 171744) [was mTakenDownRaceCarsBitArray]

        // ---- the per-car vulnerability / grinding arrays (DWARF :937..:951) -----------------------
        // REFUTED SHAPE, not just a name: the committed header declared SCALARS
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

        // The manager's own spare AI driver. Construct calls
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

        // The camera matrix Construct stamps with the identity. Asm @0x8263C068:
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
        // REFUTED ROLE: committed as `mbSuppressIfAlreadyCrashState1`; it is the AI twin of the
        // byte above, nothing to do with a crash state value.
        bool mbStopAICrashing;                      // +172307 = false  [was mbSuppressIfAlreadyCrashState1]
        bool mbCrashOnHandbrakeTurn;                // +172308 = false
        bool mbCrashPlayerNextUpdate;               // +172309 = false
        bool DEBUG_mbAlwaysCrashRaceCarToRaceCar;   // +172310   (not seeded)
        bool DEBUG_mbHornTakedownEnabled;           // +172311   (not seeded)  [was mbHornTakedownEnabled]
        bool mbDebugModifyTrafficContacts;          // +172312   (not seeded)
        bool mbTrafficCheckingAllowed;              // +172313 = true
        bool mbAftertouchIsForceAdditive;           // +172314 = false
        // REFUTED ROLE: committed as `mbStationaryTakedownsEnabled`. It is the online-mode flag.
        bool mbIsOnlineGameMode;                    // +172315 = false  [was mbStationaryTakedownsEnabled]
        bool mbUpdatedPlayerDriver;                 // +172316   (not seeded)
        bool mbForceNoSlowMo;                       // +172317   (not seeded)
        bool mbInOnlineGameModeStartLine;           // +172318 = false
        bool mbPlayerCarInJunkYard;                 // +172319 = false

        // ---- the player/car stat block (DWARF :993..:1007) ---------------------------------------
        // REFUTED TYPE: the committed header modelled +172328..+172348 as `f32
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

        // CARVED 2026-08-06 (UpdateVehiclePhysics wave). +172364..+172380 is the DWARF's
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

        // POINTER WIDTH. The DWARF types these two `const SimpleVehiclePhysics*`. They are
        // modelled as u32 slots so the 16-aligned mCachedCarCarPredictionNormal below keeps its
        // asm-proven +172432 seat on x64 -- two 8-byte pointers would push it to +172440 and silently
        // break the rest of the class. Construct only NULLs them; nothing in this tree dereferences
        // them yet. DELETE-WHEN a RaceCarPhysics/SimpleVehiclePhysics cache pass needs them live.
        u32 muCachedCarASlot;                       // +172416 = NULL  (DWARF: mpCachedCarA)
        u32 muCachedCarBSlot;                       // +172420 = NULL  (DWARF: mpCachedCarB)
        bool mbCachedCarCarPredictionResult;        // +172424 = false
        unsigned char        mPad2A189[172432 - (172424 + 1)];  // 7 bytes; Vector3 is 16-aligned
        // NEWLY PINNED, and the reason the whole tail closes: Construct loads 16 bytes from
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
        // The 30th PerfMonCpu monitor handle -- the only one of the thirty that is stored INTO the
        // object rather than into a file-scope global. Named by the console's OWN assert text,
        // `"miRaceCarWorldContactValidationPM >= 0"` (BrnVehicleManager.cpp:778), which matches
        // DWARF :1041 exactly.
        s32 miRaceCarWorldContactValidationPM;      // +172460 = AddMonitor("PHYS ValidateRCWorldContact", ...)
        unsigned char mn8RoundRobinControlWord;     // +172464   (not seeded; DWARF :1042)

        // CARVED 2026-08-06 (PhysicsModule::Update leaves wave): the HEAD of the old opaque
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

        // CARVED IN FULL 2026-08-06 (big-five #2, contact-generation wave): the REST of the
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

        // RENAMED AT CARVE (was miContactStreamCounterA/B, both FLAGGED role-neutral): with the
        // block carved the DWARF sequence pins the roles uniquely -- the counter at +172580 is
        // miNumTrafficSphereWorldTests (:1072) and the `stwx 0` seat at +172584 is the CONSOLE
        // POINTER mpTractionLineStreamProducer (:1075; the zero-store is a null-store), with
        // miNumSPUTractionLineTests (:1076) closing the run to mStuckInCollisionTestCacheSphere's
        // asm-pinned +172592 exactly.
        s32                                  miNumTrafficSphereWorldTests;                        // +172580 :1072 (=0; SVCG resets it too)
        CgsMemory::SimpleDataStreamProducer* mpTractionLineStreamProducer;                        // +172584 :1075 (=NULL)
        s32                                  miNumSPUTractionLineTests;                           // +172588 :1076
        // NEWLY PINNED, and it closes the class tail to the byte: Construct zero-stores 16 bytes
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
        // _AssertLayout and _AssertLayoutPlayerStats DO NOT CURRENTLY RUN. They are
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

        // THE ONE LAYOUT GATE THAT IS ACTUALLY COMPILED. Defined in the mounted
        // BrnVehicleManager_layout_check.cpp; pins the whole +171464..+172616 tuning bank. Never
        // called -- static_assert fires at compile time, so /OPT:REF discarding it afterwards is
        // irrelevant. Keep this TU mounted.
        static void _AssertLayoutTuningBank();
    };

    // ==========================================================================================
    // GetEntityId(RigidBodyId) -- header-inline definition, ADDED 2026-08-11 (create-drain wave).
    //
    // The console has NO out-of-line symbol for this; the body below IS what
    // VehicleManager::AddRaceCarDeformationModel @0x825E9210..0x825E931C compiles to, read
    // instruction for instruction. The handle's OWNER byte selects the arm:
    //   * RACECAR (1): the id is returned UNCHANGED, and the pass is pure VALIDATION -- the console
    //     asserts the stored maRaceCarEntityIDs[index] equals it (`lwzx r11,r25,r29 ; cmplw r11,r28
    //     ; beq` -> BrnVehicleManager.h:1985). The message is streamed through gpcMessageBuffer on
    //     the console ("Entity id %d does not match stored entity id %d\n"); lowered to the static
    //     prefix per the standing rule.
    //   * TRAFFIC_VEHICLE (2): remap through the contained manager's own table --
    //     `extrwi r11,r28,14,8 ; addis r11,r11,1 ; addi r11,r11,-0x6F58 ; slwi r11,r11,2 ; lwzx`
    //     == this + 148128 + 4*index == &mPhysicalTrafficManager.maTrafficEntityIDs[index], reached
    //     BY NAME here (the friendship that already exists for exactly this read).
    //   * anything else: the id is returned unchanged.
    // The console tests owner==2 TWICE in a row (`cmpwi r11,2 ; bne ... ; cmplwi r11,2 ; bne`);
    // the duplicate is a compiler artifact of the two-arm if/else and is not reproduced.
    // ==========================================================================================
    inline EntityId VehicleManager::GetEntityId(CgsPhysics::RigidBodyId lRigidBodyId)
    {
        // The embedded entity word is the handle's HIGH dword (`ld ; srdi r11,r11,32`).
        const u32 luEntityWord = static_cast<u32>(lRigidBodyId.GetEntityId());
        const u32 luOwner      = luEntityWord >> 24;                    // EntityId::GetOwner()
        const u32 luIndex      = (luEntityWord >> 10) & 0x3FFFu;        // GetEntityIndex()

        if (luOwner == 1u)   // BrnWorld::E_ENTITYTYPE_RACECAR -- VALIDATION-ONLY arm
        {
            CGS_ASSERT(maRaceCarEntityIDs[luIndex].muValue == luEntityWord,
                       "Entity id does not match stored entity id");   // BrnVehicleManager.h:1985
        }
        else if (luOwner == 2u)   // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
        {
            return mPhysicalTrafficManager.maTrafficEntityIDs[luIndex];
        }

        EntityId lResult;
        lResult.muValue = luEntityWord;
        return lResult;
    }

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
