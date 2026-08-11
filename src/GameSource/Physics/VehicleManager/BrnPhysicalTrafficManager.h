#pragma once

// BrnPhysics::Vehicle::PhysicalTrafficManager - the manager that owns the pool of physical
// traffic vehicles (up to ku8TotalMaxNumPhysicalTraffic == 20) and bridges traffic events
// to/from the physics simulation. This is a minimal OWNING reconstruction of the manager
// class: the member SEQUENCE is taken verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../BrnPhysicalTrafficManager.h), and the 12 functions
// owned by this TU (the accessors, the buffer allocate/bridge, the entity-id helpers and
// ResetAboveGroundTestResults) are defined against these members BY NAME in
// BrnPhysicalTrafficManager.cpp.
//
// POINTER-WIDTH DIVERGENCE (flagged): the X360 build is 32-bit, so its absolute member
// offsets (e.g. mpaTrafficVehicles @ this+103604, mu8GlobalToPhysicalEntityIndexMap @
// this+104688, mUsedTrafficVehicles word base @ this+104552) assume 4-byte pointers. On the
// 64-bit host the void*/pointer members are 8 bytes, so these absolute offsets DO NOT
// reproduce. Per project rule we pin members BY NAME + SEQUENCE and do NOT static_assert
// absolute manager offsets across pointer members. Only pointer-free sub-struct sizes are
// asserted.
//
// The many sibling types this manager embeds/points at (TrafficPhysics, VehicleDriver,
// SimpleVehiclePhysics, the debug component, the IO interfaces) are their own future TUs;
// they are forward-declared or minimally sliced here only as far as this TU's 12 functions
// require, and flagged where a future TU must complete them.
//
// ============================================================================================
// ⚠️⚠️ OPEN FINDINGS, 2026-08-03 (the Construct wave). Read before growing this header.
//
// (1) FIXED HERE: the `ArticulatedJointPool` slice below was **720 bytes too small** (112 vs the
//     real 832). PhysicalTrafficManager::Construct @0x82636CA8 proves 832 two ways:
//       ArticulatedJointPool::Construct(this + 131072 - 0x6B40) == this + 103616, and the next
//       member it writes is `stfsx f0, r31, 0x19800` == this + 104448.  104448 - 103616 == 832.
//     And the REAL class (declared inside BrnArticulatedJointPool.cpp) is exactly 832:
//       10 joints * 80 + BitArray<10> 8 + BitArray<10> 8 + 4 floats == 800 + 16 + 16 == 832.
//     With the old 112 the embedded pool overran into mfJointSwingBreakVelocity the moment
//     anything constructed it, and every member from mfJointSwingBreakVelocity onwards was
//     seated 720 bytes early -- including mUsedTrafficVehicles, whose own comment here claims
//     "X360 word base this+104552", an offset the declarations did not produce.
//
// (2) ✅ CLOSED 2026-08-03 (task #113). ZERO ODR FORKS LEFT, was four (really five). This header
//     used to define, at namespace scope in BrnPhysics::Vehicle, its own private copies of names
//     that already have real definitions elsewhere in the SAME namespace:
//        PhysicalTrafficManagerDebugComponent  retired
//        VehicleDriver                         retired
//        TrafficPhysics                        retired (task #112)
//        ArticulatedJointPool                  retired (task #113) -- had no header to include; it
//                                              has one now: VehiclePhysics/BrnArticulatedJointPool.h
//        ArticulatedJointCreateBuffer          retired (task #113) -- the FIFTH, which nobody had
//                                              counted: a 16-byte opaque standing in for the
//                                              2032-byte class BrnPhysicalTrafficManagerIO.h owns.
//                                              ⚠️ That one was an ALLOCATION BUG, not a stand-in --
//                                              see the note at its old seat.
//     ⭐ THE STANDING LESSON, now four for four: a local stand-in for a type that has (or should
//     have) a real owner elsewhere is THE recurring defect in this header. When you open it, grep
//     every namespace-scope type it declares against the rest of the tree before adding anything.
//
//     ⭐ THE MEASURED CONSEQUENCE: this TU is MOUNTED as of task #113. The previous wave measured
//     the mount at UNRESOLVED COUNT = 1
//         ?SendCreateRemoveJointEvents@ArticulatedJointPool@Vehicle@BrnPhysics@@QEAAXPEBXPEAU...
//     and read that as "one body away". It was not: that mangled name -- `PEBX` for the request
//     interface, non-const `PEAU` for the buffer -- is the FORK's signature, and the DWARF has
//     `(VehicleOutputRequestInterface*, const ArticulatedJointCreateBuffer*)`. No faithful body
//     could ever have defined the symbol that call site asked for. De-forking first, as the
//     TrafficPhysics wave's note instructed, is what made the body land on the right symbol.
//
//     ⚠️ THE OLD NOTE'S REASON FOR LEAVING THE TrafficPhysics FORK ALONE WAS WRONG, TWICE OVER. It
//     said de-forking it "means pulling VehiclePhysics.h in here, which is its own wave" -- pulling
//     VehiclePhysics.h in cost ONE include and broke nothing; and it said the real class is LARGER
//     on the host "(pointer widening -- the same +176 drift this header already tabulates)". It is
//     **SMALLER**: 4960 against the console's 5168, because several embedded sub-types are minimal
//     owning slices (SimpleVehicleAttribs is 20 bytes against 240), and pointer widening does not
//     come close to making that back. The whole-manager drift therefore moved +192 -> **-3968**, in
//     the opposite direction the note predicted. MEASURED, not reasoned.
//
//     ⭐ VehicleDriver is DE-FORKED as of 2026-08-03 (the Construct-blocker wave), and the old
//     note's reasoning was wrong about it: BrnVehicleDriver.h does NOT pull VehiclePhysics.h in
//     (it needs only types.hpp + BrnCommonTypes.h + SharedIO/BrnVehicleDriverControls.h), so that
//     fork was separable from the other two and cost one include. It was also the one fork that
//     BLOCKED WORK: BrnVehicleManager.h includes the real BrnVehicleDriver.h, so this header and
//     that one could not meet, and VehicleManager::Construct @0x8263B7C8 must call both
//     VehicleDriver::Construct and PhysicalTrafficManager::Construct.
//     The gate is real, not a comment: BrnVehicleManager_layout_check.cpp (mounted) now includes
//     THIS header and BrnVehicleManager.h together, so a re-fork fails the build.
//
// (3) STILL OPEN -- the host layout does not reproduce the X360 offsets even where no pointer is
//     involved. Measured host sizes vs the X360 values the asm implies:
//        CgsResource::ResourceHandle       16   vs   8   (VehicleManager's maRaceCar*Handles
//                                                         arrays independently prove 8)
//        Array<EntityId,20>                84   vs  88   (104464 .. 104552)
//        EventQueue<s8,50>                 72   vs  64   (104624 .. 104688)
//        PhysicalTrafficManagerDebugComponent  48 vs 32  (105616 .. 105648)
//     Per project rule this TU is written BY NAME, so the bodies are correct regardless. The old
//     note ended "a PhysicalTrafficManager _AssertLayout() cannot be written until those three are
//     settled" -- that was too pessimistic; see (4). The head of the class IS pointer-free and is
//     now pinned, and the tail is pinned by the DERIVED X360 size below.
//
// (4) ⭐⭐ THE X360 sizeof(PhysicalTrafficManager) IS 105648, DERIVED TWO INDEPENDENT WAYS
//     2026-08-03 (the un-pin wave). This number matters far outside this header: BrnVehicleManager.h
//     carried this class as an opaque span of **103360** bytes (+44768..+148128) and concluded from
//     that span that the real class "overruns by +2480" and so cannot be embedded. THE SPAN WAS
//     WRONG BY 2288 BYTES, and the header's own notes contradicted it -- it simultaneously claimed
//     the span ended at +148128 AND that this class's mu8GlobalToPhysicalEntityIndexMap (X360
//     in-class +104688) folds to VehicleManager +149456 == 44768 + 104688, which is 2128 bytes past
//     the end of the span it had just declared.
//
//     DERIVATION A -- forward from the asm, every link literal in PhysicalTrafficManager::Construct
//     @0x82636CA8 (re-pulled first-hand this wave, 99 instructions):
//        maFullTrafficPhysics   @0      `mulli r11, r29, 0x1430` x20            -> 20*5168 = 103360
//        maTrafficEntityIDs     @103360 `addi r8,r11,0x64F0 ; slwi r9,r8,2 ;
//                                        stwx -1, r9, r31` for i<20             -> 4*(i+25840)
//        mpaTrafficDrivers      @103600 `ori r11,r11,0x194B0 ; stwx 0`          (and 103604/8/12)
//        mArticulatedJointPool  @103616 `addi r3,r3,-0x6B40 ; bl ...Construct`  832 bytes
//        mfJointSwingBreakVel   @104448 `ori r5,r5,0x19800 ; stfsx`             (+4/+8/+0xC bool)
//        mUsedTrafficVehicles   @104552 `addi r6,r6,-0x6798 ; std 0`  -- then EIGHT more `std 0`
//                                        at 104560/68/76/84/104600/08/16: nine 8-byte BitArrays
//                                        spanning 104552..104624 exactly
//        mUnusedPotentialTraffic@104624 `addi r3,r3,-0x6750 ; bl char_50___Construct`  X360 64B
//        mu8GlobalToPhysical... @104688 (104624 + 64; the asm's own `cmplwi 0x258` sizes it 600)
//        mavfLowestPointWorld.. @105296 (105288 -> 16-aligned; 20 * 16 == 320)
//        mDebugComponent        @105616 `addis r29,r31,2 ; addi r29,r29,-0x6370`, then stores at
//                                        +0xC/0x10/0x14/0x18..0x1E -> last byte 0x1E, X360 size 32
//        => 105616 + 32 == 105648, and 105648 % 16 == 0 (this class is 16-aligned: VecFloat).
//
//     DERIVATION B -- backward from VehicleManager, which is an INDEPENDENT closure. The DecFIGS
//     DWARF puts `PotentialContact[128] maNonPhysicalContacts` + `int32_t miNonPhysicalContactCount`
//     between this manager and mDiscardedContacts (BrnVehicleManager.h:847/850/851/854), and
//     mDiscardedContacts is asm-pinned at VehicleManager +160672. sizeof(PotentialContact) is 80
//     (3 x Vector3 + 2 VolumeInstanceId + 2 u32 + 2 u16, 16-aligned -- 80 for either a 4- or an
//     8-byte VolumeInstanceId). So:
//        44768 + S + 128*80 + 4 -> padded to 16 -> 160672   =>   S == 105648, uniquely.
//     S = 105632 leaves a 28-byte hole (impossible, the pad is < 16); S = 105664 overshoots.
//     Two derivations, no shared assumption, same number.
//
//     ⇒ THE MEASURED HOST SIZE IS 101680, SO THE DRIFT IS **-3968** (2026-08-03, the de-fork wave;
//       it was +192 while maFullTrafficPhysics was a byte-pinned `u8[5168]` stand-in). Both numbers
//       measured with `char (*p)[sizeof(T)] = 1;`. BrnVehicleManager.h embeds this class by name at
//       +44768 and adds KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER to every downstream offsetof assert.
//       Do NOT "absorb" it by shrinking a neighbouring pad: maNonPhysicalContacts is a real DWARF
//       member with a derived size, and shrinking it would be inventing layout.
//
//     ⭐ AND IT IS ACCOUNTED FOR MEMBER BY MEMBER -- it is not a residue. Walking both layouts in
//     parallel (X360 offset -> host offset, running delta in the last column):
//        maFullTrafficPhysics       0      ->      0        0    (TrafficPhysics 4960 vs 5168, x20)
//        maTrafficEntityIDs         103360 ->  99200    -4160    ⭐ MEASURED (compiler offsetof)
//        maTrafficCarModelHandles   103440 ->  99280    -4160
//        mpaTrafficDrivers          103600 ->  99600    -4000    (ResourceHandle 16 vs 8, x20: +160)
//        mArticulatedJointPool      103616 ->  99632    -3984    (four pointers 4 -> 8: +16)
//        mfJointSwingBreakVelocity  104448 -> 100464    -3984    (pool is u8[832] on both)
//        maRecycledTrafficThisFrame 104464 -> 100480    -3984
//        mUsedTrafficVehicles       104552 -> 100568    -3984    (Array 84 vs 88 costs -4, then the
//                                                                 BitArray's 8-alignment gives it back)
//        mUnusedPotentialTrafficQ.  104624 -> 100640    -3984    (nine BitArrays, 8 bytes on both)
//        mu8GlobalToPhysical...     104688 -> 100712    -3976    (EventQueue<s8,50> 72 vs 64: +8)
//        mavfLowestPointWorldSpace  105296 -> 101312    -3984    (X360 needs 8 bytes of 16-align pad
//                                                                 after the 600-byte map; the host,
//                                                                 already 16-aligned, needs none)
//        mDebugComponent            105616 -> 101632    -3984
//        end of class               105648 -> 101680    -3968    (debug component 48 vs 32: +16)
//     The two ENDS of that column are compiler-measured (99200 and 101680); the interior is the
//     previously-measured host column shifted by the array's -4160, which is forced rather than
//     assumed: TrafficPhysics is 16-aligned and 99200 % 16 == 0, so nothing behind the array can
//     re-align. Two of the steps are alignment give-and-take rather than width, which is why the
//     naive width sum (160+16+8+16 = 200) never matched the +192 the old table ended on: it
//     double-counts the Array's -4 and misses the map's -8 of pad.
//
//     ⚠️ ONE ASYMMETRY WORTH KNOWING, found by tamper-testing: the 600-vs-601 choice for
//     mu8GlobalToPhysicalEntityIndexMap (asm `cmplwi 0x258` vs the DWARF's 601) does NOT change the
//     X360 size -- 105288 and 105289 both round to the same 105296 -- but it DOES change the host
//     size, because the host map does not start on the same alignment phase as the console's and 601
//     can push the 16-aligned VecFloat array a whole slot out. So the derived X360 constant is
//     robust to that choice and the host drift is not; re-measure the drift if the map is re-sized.
//
// (5) WHY THE CONSTRUCTOR @0x827E42E8 IS DEFINED INLINE IN THIS HEADER (2026-08-03). VehicleManager
//     now embeds this class BY VALUE, VehicleManager is embedded by value in PhysicsModule, and
//     PhysicsModule's constructor is MOUNTED -- so the implicit constructor chain references this
//     symbol from mounted code. Its only definition was in BrnPhysicalTrafficManager.cpp, which is
//     NOT mounted (finding (2)'s remaining ArticulatedJointPool fork makes mounting it a wave of its
//     own), so the link failed with LNK2019.
//     ⛔ THE REJECTED ALTERNATIVE was a link stub -- an empty
//     `PhysicalTrafficManager::PhysicalTrafficManager() {}` in a stubs TU. That is the silent-drop
//     failure class exactly: it links, it runs, and it leaves mpaTrafficDrivers / mpaTrafficVehicles
//     / mpaSimpleVehiclePhysics / mpArticulatedJointCreateBuffer as uninitialised garbage that every
//     later `!= NULL` check reads as "already allocated". Inlining keeps the real initialiser list,
//     gives it exactly one definition, and cannot decay into an empty body later.
// ============================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // EntityId, Vector3, CgsID, ResourceHandle
#include "GameShared/GameClasses/Containers/CgsBitArray.h"             // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"              // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"          // CgsModule::IOBufferStack
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"    // KU_ENTITYTYPE_TRAFFIC_VEHICLE
// ⭐ DE-FORKED 2026-08-03: this header used to declare its own opaque
// `struct PhysicalTrafficManagerDebugComponent { void* mpVTable; u8 mOpaque[60]; };` at namespace
// scope, a second definition of a name that already had a real one. The real class is included.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"
// ⭐ DE-FORKED 2026-08-03 (second one): `struct VehicleDriver { u8 mOpaque[224]; }` used to be
// declared at namespace scope below. The real struct lives here and is 224 bytes, so the stride
// this header depends on is unchanged -- see the note at the old fork's seat.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
// ⭐⭐ DE-FORKED 2026-08-03 (third one): `struct TrafficPhysics { void Construct(); u8[5168]; }` used
// to be declared at namespace scope below. The real class -- and, transitively, VehiclePhysics and
// SimpleVehiclePhysics -- is included here. See the note at the old fork's seat for why the mangled
// name made this a correctness item and not a tidiness one.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"
// ⭐⭐ DE-FORKED 2026-08-03 (fourth and fifth, task #113 -- finding (2) is now CLOSED).
//   * `struct ArticulatedJointPool { int Construct(); void SendCreateRemoveJointEvents(const void*,
//      ArticulatedJointCreateBuffer*); u8 mOpaque[832]; }` used to be declared at namespace scope
//     below. The real class had no header at all (it was declared inside
//     VehiclePhysics/BrnArticulatedJointPool.cpp); it has one now and it is included here.
//   * `struct ArticulatedJointCreateBuffer { u8 mOpaque[16]; }` used to be declared below too --
//     and THAT one was never a layout-neutral stand-in: the real class is 2032 bytes.
// See the notes at the two old fork seats for the measured consequences of each.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"

// UpdateVehiclePhysics-wave collaborator (pointer-only on this surface).
namespace BrnPhysics { namespace Vehicle { struct VehicleManagerOutputInterface; } }

// ⭐ ADDED 2026-08-10 (create-path wave): ProcessTrafficMaintenanceEvents' remaining parameter
// types, pointer use only. CLASS KEYS CHECKED AGAINST THE SINGLE HOME OF EACH BEFORE WRITING
// THEM, per the standing ODR-fork rule -- BrnVehicleInputInterface.h:28 and
// BrnVehicleOutputInterface.h:62 both spell `struct alignas(16)` (the alignment belongs to the
// definition, not the declaration), and BrnDeformationInputInterface.h:20 spells `class`.
namespace BrnPhysics { namespace Vehicle { struct VehicleInputInterface;
                                           struct VehicleOutputInterface; } }
namespace BrnPhysics { namespace Deformation { class DeformationInputInterface; } }

// ⭐ ADDED 2026-08-06 (big-five #2): ValidateTrafficContact collaborators, pointer use only.
// Class key `struct`, matching CgsPotentialContact.h / the TriangleCacheInterface home.
namespace CgsSceneManager { namespace SceneManagerIO { struct PotentialContact;
                                                       struct TriangleCacheInterface; } }

// ⭐ ADDED 2026-08-10 (producer wave): PrepareTriangleCache's parameter, pointer use only.
// Class key `struct`, matching the single home CgsSceneManagerIO.h:31.
namespace CgsSceneManager { namespace SceneManagerIO { struct InputBuffer_Update; } }

// ⭐ ADDED 2026-08-11 (lifetime wave): the traction-line pair's collaborators, pointer use only.
// Class keys match their single homes -- CgsCollisionGenerator.h spells CollisionGenerator
// `struct`, CgsSimpleDataStreamProducer.h spells both stream types `struct`.
namespace CgsSceneManager { namespace CgsCollision { struct CollisionGenerator; } }
namespace CgsMemory { struct SimpleDataStreamProducer; struct SimpleDataStreamResultIterator; }

namespace BrnPhysics
{
// Forward decl of the streamed deformation model spec (real home
// BrnPhysics::Deformation::StreamedDeformationSpec, BrnStreamedDeformationSpec.h) --
// PhysicalTrafficVehicle::PreparePhysical takes it by pointer and reads its bounding box.
namespace Deformation { struct StreamedDeformationSpec; }
namespace Vehicle
{

// Forward decls for the PhysicalTrafficVehicle wave-8 method params (real homes elsewhere; only
// pointers/refs are taken here so incomplete types suffice).
// ⚠️ CLASS-KEYS CORRECTED 2026-08-03 with the TrafficPhysics de-fork: including the real
// TrafficPhysics.h drags in VehiclePhysics.h / BrnSimpleVehiclePhysics.h, which declare
// VehicleAttribs and BrnPlayerDriverControls for real. The keys here said `struct VehicleAttribs`
// against the real `class`, and `class BrnPlayerDriverControls` against the real `struct` -- a
// C4099 apiece and, more to the point, a second reading of what those names are.
// CreatePhysicalTrafficEvent and RaceCarPhysics keep their forward declarations (neither header is
// pulled in here; only pointers/refs to them appear).
struct CreatePhysicalTrafficEvent;   // spawn event -- SharedIO/BrnVehicleEvents.h
struct CreateAirRamEvent;            // air-ram event -- SharedIO/BrnVehicleEvents.h (ProcessAddAirRamEvent arg)
class  RaceCarPhysics;               // the checking race car -- RaceCarPhysics.h

// VecFloat: a single 16-byte VMX float lane (the DWARF spells the members below as VecFloat).
// Its canonical home is rw::math::vpu; here it is the 16-byte 4-lane vector value used for the
// per-vehicle world-space lowest point. mavfLowestPointWorldSpace is not touched by this TU's
// 12 functions.
typedef rw::math::vpu::Vector4 VecFloat;

// SimpleVehiclePhysics: the per-vehicle physics body, pointed at by mpVehicleBody and by
// mpaSimpleVehiclePhysics. This manager only takes/returns SimpleVehiclePhysics*
// (GetVehiclePhysics) and, in ResetAboveGroundTestResults, reaches into its above-ground-test
// sub-objects.
// ⚠️ THE FORWARD DECLARATION HERE WAS `struct SimpleVehiclePhysics;` AGAINST THE REAL `class`
// (BrnSimpleVehiclePhysics.h:99). It is gone: the real header now arrives with TrafficPhysics.h,
// which is strictly better -- ResetAboveGroundTestResults can reach the sub-objects BY NAME through
// a complete type instead of through the incomplete one it was flagged against.

// ku8TotalMaxNumPhysicalTraffic: the pool capacity asserted throughout this TU
// ("liVehicle < ku8TotalMaxNumPhysicalTraffic", upper bound 20).
const u8 KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC = 20;

// KU8_INVALID_MAP (BrnPhysicalTrafficManager.h:428): the sentinel stored in
// mu8GlobalToPhysicalEntityIndexMap for "no physical traffic vehicle for this global id"
// (the X360 `cmplwi 0x7F` checks in ValidateAndFixUpTrafficTrafficContact).
const u8 KU8_INVALID_MAP = 127;

// KU_NUM_BITS_FOR_ENTITY_NUM: the entity-index field width inside an EntityId
// (CgsEntityId.h:116 assert "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"). The X360
// packs an EntityId as (entityIndex << 10) | (ownerType << 24); the index field is 14 bits.
const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;

// ⭐ KU_ENTITYTYPE_TRAFFIC_VEHICLE MOVED OUT 2026-08-03 (task #113) to BrnVehicleConstants.h,
// included above. It was defined here AND, identically, at BrnArticulatedJoint.h:42 -- each
// comment describing itself as a mirror of the other. That cost nothing while the two headers
// could not meet; the ArticulatedJointPool de-fork made them meet and it became a hard C2374.
// It is the same defect class as the five type forks in finding (2), one level down.

// ---- Minimal slices of un-homed sibling types this TU touches BY NAME ----------------

// TrafficPhysics: the per-vehicle full-physics block (BrnPhysicalTrafficManager.h:396,
// maFullTrafficPhysics[20]).
//
// ⭐⭐ DE-FORKED 2026-08-03 (the TrafficPhysics::Construct wave). This slot used to hold a second,
// opaque `struct TrafficPhysics { void Construct(); u8 mOpaque[5168]; };` at namespace scope in
// BrnPhysics::Vehicle -- a redefinition of the class VehiclePhysics/TrafficPhysics.h already owns,
// disagreeing with it on the class-key AND on the bases. The real header is included above.
//
// WHY IT HAD TO GO, and why it was not tidiness: the mangled name
// `?Construct@TrafficPhysics@Vehicle@BrnPhysics@@QEAAXXZ` encodes neither the class-key nor the base
// list, so the slice's `void Construct();` and the real class's `void Construct();` are THE SAME
// SYMBOL. A body written against the real class would have linked against this call site silently,
// with the array strided by the console's 5168 while the real host class is a different size --
// writing mRandom (host +0x1330) into the neighbouring element's storage on every one of the twenty
// iterations. This is exactly the trade BrnVehicleManager.h refused for maRaceCarVehicles when it
// folded that array from a byte-pinned record to the real RaceCarPhysics[8]; the same fold, for the
// same reason, is done here.
//
// WHAT IT COSTS, stated rather than hidden: sizeof(TrafficPhysics) is 4960 on the host against the
// X360's 5168 (the reconstruction carries minimal owning slices for several embedded sub-types --
// SimpleVehicleAttribs is 20 bytes against the console's 240 -- deliberately, per the project rule
// that unrecovered interiors are never faked as padding). So maFullTrafficPhysics[20] is
// 20 * 208 == 4160 bytes shorter here and every member behind it moves DOWN by that much. That is
// carried explicitly in finding (4) below and in BrnVehicleManager.h's
// KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER, never absorbed into a neighbouring pad.

// VehicleDriver: the AI/driver block, pointed at by mpaTrafficDrivers (stride 224 / 0xE0 from
// GetTrafficDriver).
//
// ⭐ DE-FORKED 2026-08-03. This slot used to be a second, opaque `struct VehicleDriver
// { u8 mOpaque[224]; }` at namespace scope in BrnPhysics::Vehicle -- a redefinition of the real
// struct that VehiclePhysics/BrnVehicleDriver.h already owns. The real header is included above
// and the stride assert below is what keeps the two readings honest.
//
// ⚠️ CORRECTION to finding (2) in the banner, which said de-forking "means pulling VehiclePhysics.h
// in here, which is its own wave". That is true of TrafficPhysics and ArticulatedJointPool -- both
// really do derive from VehiclePhysics -- but it was NOT true of VehicleDriver: BrnVehicleDriver.h
// stands alone (types.hpp + BrnCommonTypes.h + SharedIO/BrnVehicleDriverControls.h) and pulls no
// physics body in. The three forks were not one problem; this one was separable and is gone.
//
// WHY IT MATTERED: while the fork stood, this header and BrnVehicleManager.h (which includes the
// real BrnVehicleDriver.h for maRaceCarDrivers[8] / mPlayerAiDriver) could not be included in the
// same translation unit -- a hard C2011. VehicleManager::Construct @0x8263B7C8 has to call BOTH
// VehicleDriver::Construct and PhysicalTrafficManager::Construct, so the fork was a compile-level
// blocker on that function, not just a latent tidiness item.
static_assert(sizeof(VehicleDriver) == 224,
              "VehicleDriver is 224 bytes (0xE0) -- the X360 GetTrafficDriver stride "
              "(*(this+103600) + 224*idx) and VehicleManager::Construct's `addi r25, r25, 0xE0`");

// PhysicalTrafficVehicle: pointed at by mpaTrafficVehicles (stride 64 / 0x40 from the
// GetTrafficVehicle accessors). The member layout below is verbatim from the DecFIGS DWARF
// (BrnPhysicalTrafficVehicle.h) and is what this TU reads BY NAME:
//   IsTrafficVehicleSimple reads mu8PhysicalType (X360 +0x32 == 50)
//   GetVehiclePhysics reads mpVehicleBody         (X360 +0x1C == 28)
// FLAG: the methods of PhysicalTrafficVehicle are its own future TU; only the data layout is
// reproduced here so this manager TU can index it.
struct PhysicalTrafficVehicle
{
    enum EPhysicalTrafficType
    {
        E_PHYSICAL_TRAFFIC_TYPE_FULL  = 0,
        E_PHYSICAL_TRAFFIC_TYPE_SIMPLE = 1,
        E_PHYSICAL_TRAFFIC_TYPE_COUNT = 2,
    };
    enum EArticulatedJointState
    {
        E_ARTICULATE_JOINT_NONE     = 0,
        E_ARTICULATE_JOINT_ATTACHED = 1,
        E_ARTICULATE_JOINT_COUNT    = 2,
    };
    enum EArticulatedVehicleType
    {
        E_ARTICULATE_VEHICLE_NONE    = 0,
        E_ARTICULATE_VEHICLE_CAB     = 1,
        E_ARTICULATE_VEHICLE_TRAILER = 2,
        E_ARTICULATE_VEHICLE_COUNT   = 3,
    };

    // X360 0x825B3358: range-checked read of meArticulatedVehicleType (+36). One of
    // PhysicalTrafficVehicle's own methods (FLAG above): bodied against the named +36 member.
    EArticulatedVehicleType GetArticulatedVehicleType() const;

    // ---- wave-7 methods (bodied in BrnPhysicalTrafficManager.cpp) ----------------------------
    // @0x825C0148: assert fully-physical and return mpVehicleBody as the concrete full-physics body.
    TrafficPhysics* GetFullTrafficPhysics();
    // @0x826152E0: forward an air-ram impulse to the full-physics body (only when FULLY physical).
    void            AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay,
                              Vector3 lvCustomImpulse, Vector3 lvCustomPosition, f32 lfTimerTillFire);
    // @0x825B33B8: true iff this is a SIMPLE (non-full-physics) traffic vehicle.
    bool            IsSimple() const;
    // @0x825B3418: true iff this vehicle has an ATTACHED (non-broken) articulation joint.
    bool            HasNonBrokenJoint() const;
    // @0x825C01B8: latch the active-race-car that "checked" this vehicle (asserts not already checked).
    void            SetCheckOwner(EActiveRaceCarIndex leCheckOwner);
    // has miCheckOwner been set to a valid race-car (!= 0xFF sentinel)?
    bool            HasBeenChecked() const { return miCheckOwner != -1; }
    // @0x825C0220 / @0x825C0538: the articulation point in world / local space.
    Vector3         GetArticulationPointWorldSpace() const;
    Vector3         GetArticulationPointLocalSpace() const;

    // ---- wave-8 methods (bodied in BrnPhysicalTrafficManager.cpp) --------------------------------
    // @0x82641058: seed this vehicle's id/state fields from the spawn event; when fully physical,
    // delegate the full-physics body prepare to the TrafficPhysics TU.
    bool PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent, VehicleAttribs* lpAttribs,
                         const Deformation::StreamedDeformationSpec* lpModelData,
                         const Vector3* lpWheelPositions, const f32* lpafWheelRadii);
    // @0x8261E360: latch the checking race car + reset the check-notify timer; when the checker is
    // hard enough and this car is fully physical, arm the full body's crashing state.
    void OnChecked(EActiveRaceCarIndex leOwner, const RaceCarPhysics* lpRaceCarPhysics,
                   Vector3 lContactPointOnTraffic);
    // @0x826411C0: per-frame update -- accumulate the check-notify timer; forward the full-physics
    // per-frame tick to the TrafficPhysics TU.
    void Update(f32 lfSimTimerTimeStep, f32 lfGameTimerTimeStep, const Matrix44Affine* lpCameraMatrix,
                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis);
    // @0x825F3B68: mark this vehicle as an articulated cab/trailer + (delegated) compute its
    // articulation point from the deformation model's hitch locator.
    void SetArticulated(const CreatePhysicalTrafficEvent& lrCreateTrafficEvent,
                        EArticulatedVehicleType leVehicleType);

    Vector3                 mArticulationPointLocal;     // +0
    CgsID                   mCgsID;                      // +16 (u64; +12..15 padding)
    f32                     mfTimeSinceCheckNotify;      // +24
    SimpleVehiclePhysics*   mpVehicleBody;               // +28 (X360 4B ptr) -> GetVehiclePhysics
    u32                     mePhysicalTrafficState;      // +32 (BrnPhysics::Vehicle::ETrafficType)
    EArticulatedVehicleType meArticulatedVehicleType;    // +36
    EArticulatedJointState  meArticulatedJointState;     // +40
    s32                     miJointIndex;                // +44
    bool                    mbRammed;                    // +48
    u8                      mu8PhysicsPoolIndex;         // +49
    u8                      mu8PhysicalType;             // +50 -> IsTrafficVehicleSimple
    s8                      miCheckOwner;                // +51
    bool                    mbUsingBoxWithWorld;         // +52
    // pad to the X360 0x40 stride (mOpaque keeps element stride == 64 on the host too, but the
    // pointer member above is 8B on host so the trailing pad just keeps the type non-degenerate)
};

// ArticulatedJointCreateBuffer: the IO buffer pushed onto the input stack by
// AllocateInternalBuffers (CreateIOBuffer<ArticulatedJointCreateBuffer>).
//
// ⭐⭐ DE-FORKED 2026-08-03 (task #113). This slot used to hold
//     struct ArticulatedJointCreateBuffer { u8 mOpaque[16]; };
// -- a second definition, at namespace scope in BrnPhysics::Vehicle, of a class
// BrnPhysicalTrafficManagerIO.h has owned in full since its own wave. The real header is included
// at the top.
//
// ⚠️⚠️ AND THIS ONE WAS NOT A LAYOUT-NEUTRAL STAND-IN, IT WAS AN ALLOCATION BUG WITH A FUSE.
// The real class is IOBuffer(16) + InAddJoint[10](1920) + InRemoveJoint[10](80) + 2*BitArray<10>
// == 2032 bytes. AllocateInternalBuffers below calls
//     lpInputBufferStack->CreateIOBuffer<ArticulatedJointCreateBuffer>(...)
// -- a TEMPLATE, so the type's SIZE and its CLASS-KEY both go into the instantiation. With the
// 16-byte fork in scope that call would have allocated 16 bytes for a 2032-byte IO buffer, and the
// first FlagJointToBeCreated would have written 2 KB past the end of it. (The class-key disagreed
// too: the X360 mangles the real instantiation `??$CreateIOBuffer@VArticulatedJointCreateBuffer@...`
// -- `V` for class -- against the fork's `U` for struct.) The fuse had not lit only because
// this TU has never been mounted; it is mounted as of this wave.

// ArticulatedJointPool: embedded by value (BrnPhysicalTrafficManager.h:406).
//
// ⭐⭐ DE-FORKED 2026-08-03 (task #113). This slot used to hold an opaque
//     struct ArticulatedJointPool { int Construct(); void SendCreateRemoveJointEvents(const void*,
//                                   ArticulatedJointCreateBuffer*); u8 mOpaque[832]; };
// The real class had NO header -- it was declared inside VehiclePhysics/BrnArticulatedJointPool.cpp
// -- which is exactly why this fork existed. It has a header now (BrnArticulatedJointPool.h) and it
// is included at the top; that header's banner carries the full reasoning.
//
// WHY IT HAD TO GO: the previous wave measured `ArticulatedJointPool::Construct` as RESOLVED when
// this TU was trial-mounted, and read that as progress. It is the hazard. The mangled name encodes
// neither the class-key (`struct` here vs `class` there) nor the member layout, so the slice's
// declaration and the real class's definition were ONE symbol -- the identical silent-link trap the
// TrafficPhysics fork carried until the wave before.
//
// ⭐ THE FOLD IS LAYOUT-NEUTRAL, MEASURED: the real class is pointer-free (10*80 + 8 + 8 + 4*4)
// and is 832 bytes on the host as well as on the console, so -- unlike the TrafficPhysics fold,
// which moved everything behind it by -4160 -- NOTHING in this class's layout moves. The drift
// table in finding (4) is unchanged by this wave.
//
// ⚠️ ONE SIGNATURE CORRECTION CAME WITH IT, and it is the reason the fork could never have worked:
// `SendCreateRemoveJointEvents` is `(VehicleOutputRequestInterface*, const ArticulatedJointCreateBuffer*)`
// in the DWARF (BrnArticulatedJointPool.h:104). The fork declared it `(const void*,
// ArticulatedJointCreateBuffer*)` -- wrong on both parameters -- so the symbol its call site
// demanded (`...QEAAXPEBXPEAU...`) was one no faithful body could ever define. And `Construct` is
// `void`, not `int`; see the pool header.

// Forward-declared IO/interface dependencies (their own TUs).
// ⭐ VehicleOutputRequestInterface HAS A REAL HOME as of 2026-08-03 --
// SharedIO/BrnVehicleOutputInterface.h, where its six-queue layout is derived and gated. It is kept
// as a forward declaration HERE (rather than an include) deliberately: this TU only ever passes the
// pointer through, and the real header is ~42 KB of event-queue storage that nothing in this
// translation unit needs the complete type of.
struct VehicleOutputRequestInterface;

// The OWNER. VehicleManager embeds this manager by value at its +44768 and, in two places, reads
// two of its tables with BARE loads -- no accessor, no assert (see the friend declaration below).
class VehicleManager;

// ⭐ The X360 sizeof(PhysicalTrafficManager), DERIVED (finding (4) in the banner), not measured on
// the host and not guessed. VehicleManager subtracts this from the host sizeof to get the drift it
// carries through the rest of its own layout, so a change to either number is visible there.
const u32 KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER = 105648u;

// =====================================================================================
// PhysicalTrafficManager
// =====================================================================================
class PhysicalTrafficManager
{
public:
    typedef Array<EntityId, 20u>            RecycledTrafficIdArray;
    typedef CgsContainers::BitArray<20u>    TotalPhysicalTrafficBitArray;
    typedef CgsContainers::BitArray<20u>    FullyPhysicalTrafficBitArray;
    typedef CgsContainers::BitArray<1u>     SimpleTrafficBitArray;

    // ---- the 12 functions owned by THIS TU ----------------------------------------

    // Accessors into the per-vehicle arrays (X360 0x825B4880 / 0x825B4800 share a body):
    // both the const and non-const GetTrafficVehicle return &mpaTrafficVehicles[idx].
    PhysicalTrafficVehicle*       GetTrafficVehicle(s32 liVehicle);
    const PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle) const;

    // ⭐ ADDED 2026-08-10 (producer wave). X360 @0x825EE5A0 (39 insns); home
    // BrnPhysicalTrafficManager.cpp:244 per its own baked assert path. Claims this manager's
    // 20 triangle-cache slots (indices 8..27, i.e. immediately after the 8 race cars) by
    // posting one InEventAddToCache each into the scene input's mAddToCacheQueue.
    // Sole caller: VehicleManager::PrepareTriangleCache @0x82615BA0.
    // ⚠️ `this` IS UNUSED in the console body -- r3 is never read after the prologue. Kept as a
    // non-static member because that is what the console's `bl` with r3 = this + 44768 is.
    bool PrepareTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update);

    // ⭐⭐ @0x825EE640 (261 insns) -- BODIED 2026-08-11 (lifetime wave). The traffic half of the
    // per-frame cache-position push: per live traffic vehicle, post one
    // InEventUpdateCachedPosition carrying {world position, bounding radius} for cache slot
    // 8 + liVehicle (the 8 race-car slots come first -- together they are the 28 slots
    // TriangleCacheManager reports as used). Sole caller:
    // VehicleManager::UpdateTriangleCache @0x82615C38.
    void UpdateTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update);

    // ---- ⚠ FLAG: gate-bodied (BrnPhysicsConductorGates.cpp) ---------------------------------
    // The TRAFFIC traction-line pair, gated together 2026-08-11 (lifetime wave). A race car on
    // the ground needs neither, and they are 709 console instructions. ⛔ KEEP THEM PAIRED: the
    // Add posts one command per live traffic vehicle and the Read consumes exactly that many
    // records off the SHARED result cursor EndVehicleTractionLineTests hands all three harvests
    // in turn. One without the other mis-seats every harvest downstream of it.
    //   0x8261D580 (418) AddTrafficTractionLineTests <-> 0x8262D2B8 (291) ReadTrafficTraction...
    // ⚠️ AddTraffic is also the witness that a stream command carries FIVE line slots, not four:
    // it writes `miNumLines = 5` (0x8261D9F4 `stw r10, 0xA8(r30)`) where both race-car and
    // player-stuck producers write 4.
    s32 AddTrafficTractionLineTests(
        CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
        CgsMemory::SimpleDataStreamProducer* lpStreamProducer,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface);
    void ReadTrafficTractionLineTestResults(
        CgsMemory::SimpleDataStreamResultIterator* lpResultIterator);

    // ⭐ ADDED 2026-08-11 (prepare-chain wave). @0x825CA8A0 (169 insns). The TRAFFIC arm of
    // VehicleManager::UpdateDrivers' four-way control dispatch -- case 3 of the switch, reached
    // with `add r3, r31, r28` where r28 == 0xAEE0 == 44768 == the manager's own
    // mPhysicalTrafficManager seat, so the `this` really is this class and not the vehicle manager.
    // DWARF-attested verbatim (references/DecFIGS/.../BrnPhysicalTrafficManager.h:158).
    // ⛔ STILL BODYLESS -- a named BRN_CONDUCTOR_GATE in BrnPhysicsConductorGates.cpp.
    void UpdateTrafficDriver(const BrnTrafficDriverControls* lpControls,
                             CgsContainers::BitArray<8u>& lrUpdatedCars);

    // ⭐ ADDED 2026-08-10 (create-path wave). X360 0x825EF608 (334 insns), bodied in
    // GameSource/Physics/VehicleManager/BrnVehicleManager_ReadUpdatedBodies.cpp alongside its
    // only caller, VehicleManager::ReadUpdatedBodies @0x82619A10 (xrefs_to == that one entry).
    // Despite the name it reads no body state back: the queue feeds a dev duplicate-id assert
    // and nothing else, and the real work is the per-vehicle
    // `mLinearVelocity.y -= KF_GRAVITY*dt ; IntegrateTransform(dt)` over the fully-physical
    // entries of mUsedTrafficVehicles.
    void ReadUpdatedBodies(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
        VecFloat lvfTimeStep);

    // ⭐ ADDED 2026-08-10 (create-path wave). X360 0x82649768 (246 insns) -- the traffic twin of
    // VehicleManager::ProcessVehicleMaintenanceEvents and its sixth and last call, taking that
    // function's whole argument list verbatim (r3 = &mPhysicalTrafficManager, i.e. the vehicle
    // manager + 44768). ⚠ FLAG: DECLARED for the maintenance closure; body is a LOUD one-shot
    // gate (BrnVehicleManager_MaintenanceEvents.cpp) -- its own create/remove/crash arms over
    // mUsedTrafficVehicles are unreconstructed, and the same ground ordering applies to traffic
    // as to race cars.
    void ProcessTrafficMaintenanceEvents(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputRequestInterface* lpOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface); // @0x82649768

    // X360 0x825B4900: &mpaTrafficDrivers[idx] (stride 224).
    VehicleDriver* GetTrafficDriver(s32 liVehicle);

    // @0x8261DC08 (DWARF BrnPhysicalTrafficManager.h:194). Forward a queued air-ram event whose
    // volume id names a GLOBAL traffic entity: assert the owner byte is TRAFFIC_VEHICLE, map the
    // global index through mu8GlobalToPhysicalEntityIndexMap (KU8_INVALID_MAP = no physical
    // vehicle -> drop), and hand the impulse to that PhysicalTrafficVehicle's AddAirRam.
    // ADDED 2026-08-06 (PhysicsModule::Update leaves wave); bodied in this manager's own TU.
    void ProcessAddAirRamEvent(const CreateAirRamEvent* lpEvent);

    // X360 0x825B4A28: mpaTrafficVehicles[ lPhysicsVehicleId.GetEntityIndex() ].mpVehicleBody
    SimpleVehiclePhysics* GetVehiclePhysics(EntityId lPhysicsVehicleId);

    // X360 0x825B4A98: true iff mpaTrafficVehicles[idx].mu8PhysicalType == E_..._SIMPLE
    bool IsTrafficVehicleSimple(EntityId lPhysicsVehicleId) const;

    // X360 0x825B4980: pack a physics-traffic EntityId from a traffic index.
    EntityId GetPhysicsEntityId(s32 liTrafficIndex) const;

    // X360 0x825C2C38: look up the GLOBAL entity id stored for a traffic-car slot.
    EntityId GetGlobalTrafficEntityId(u16 lu16TrafficCarIndex) const;

    // X360 0x82615958: push the ArticulatedJointCreateBuffer onto the input IO stack.
    void AllocateInternalBuffers(CgsModule::IOBufferStack* lpInputBufferStack,
                                 CgsModule::IOBufferStack* lpOutputBufferStack);

    // X360 0x82615B10: drain the create-buffer into the output request interface.
    void BridgeArticulatedJointRequestsToSim(VehicleOutputRequestInterface* lpOutputRequestInterface);

    // ---------------------------------------------------------------------------------------
    // PhysicalTrafficManager (constructor)   @ 0x827E42E8
    // X360: walks maFullTrafficPhysics[0..19] (stride 0x1430), placing each element's three
    // vtables and running the vector-constructor-iterators for its embedded
    // CgsCollision::BaseCollisionGenerator sub-arrays; then sets the unused-potential-traffic
    // queue head sentinel (*(this+104544) = -1) and the debug-component vtable (*(this+105616)).
    //
    // ⭐ UPDATED 2026-08-03 (the de-fork wave). maFullTrafficPhysics is now a real
    // `TrafficPhysics[20]`, so the compiler's own implicit default-initialisation of that array IS
    // the per-element vtable walk the X360 constructor does -- it is no longer "left to an un-homed
    // TU", it happens because the member is the real type. What remains genuinely un-homed is the
    // CgsSceneManager::CgsCollision::BaseCollisionGenerator vector-constructor-iterator run over
    // that class's embedded sub-arrays; those sub-objects are not modelled in the reconstruction, so
    // there is nothing to iterate and nothing is faked in their place.
    //
    // ⭐⭐ MOVED HERE (INLINE) FROM BrnPhysicalTrafficManager.cpp, 2026-08-03 (the un-pin wave),
    // VERBATIM -- the eight member initialisers are byte-for-byte the ones that were in the .cpp
    // and all three FLAG notes came with them; nothing was dropped or simplified. See finding (5)
    // in the banner for WHY it had to move and what the rejected alternative was.
    // ---------------------------------------------------------------------------------------
    PhysicalTrafficManager()
        : mpaTrafficDrivers(nullptr)
        , mpaTrafficVehicles(nullptr)
        , mpaSimpleVehiclePhysics(nullptr)
        , mpArticulatedJointCreateBuffer(nullptr)
        , mfJointSwingBreakVelocity(0.0f)
        , mfJointTwistBreakVelocity(0.0f)
        , mfJointLinearBreakMph(0.0f)
        , mbAllowArticulatedJointBreaking(false)
    {
        // X360 *(this+104544) = -1: the EventQueue head/read-index sentinel of
        // mUnusedPotentialTrafficQueue. FLAG: the exact member is inside the un-homed
        // CgsModule::EventQueue body layout; the observable effect is the queue starting empty.
        // Its Construct()/reset is owned by the EventQueue TU; here the member is value-initialised.

        // X360 *(this+105616) = &debug-component vtable: mDebugComponent's vtable. FLAG: the debug
        // component is its own TU; the vtable store happens in its constructor, not reproduced here.

        // FLAG: the maFullTrafficPhysics[20] per-element placement walk (TrafficPhysics ctor +
        // BaseCollisionGenerator vector-constructor-iterators) is left to those un-homed TUs.
    }

    // X360 0x82636CA8 (99 instructions; DWARF BrnPhysicalTrafficManager.h:475 `void Construct()`).
    // One-shot construction: build all 20 full-physics bodies and the joint pool, invalidate the
    // 20 traffic entity ids, null the four pool pointers, seed the joint-break limits, clear the
    // traffic bitsets, construct the unused-potential queue and the debug component.
    // Its ONLY caller in the image is VehicleManager::Construct @0x8263B7C8.
    void Construct();

    // X360 0x825E8808: reset the above-ground (down-ray) test results for every used vehicle.
    void ResetAboveGroundTestResults();

    // ⭐ ADDED 2026-08-06 (big-five #3, UpdateVehiclePhysics wave). Two DWARF-attested
    // per-frame members the manager's conductor calls (BrnPhysicalTrafficManager.h:152/:155).
    // Both DECLARED for the conductor's closure; bodies are named FLAG TRAP STUBS in
    // BrnVehicleManagerLinkStubs.cpp (dead until PhysicsModule::Update lands):
    //   * UpdateTrafficPhysics @0x82644418 is an .ida-exports HOLE (image-only; the
    //     VehiclePhysicsLinkStubs banner already records it as the console caller of the
    //     traffic Update leaves) -- its body is its own wave.
    //   * PassNearbyCrashingTrafficIdsToRaceCarModule's X360 address is not yet pinned
    //     (absent from this TU's dossier; recover by caller set at its own wave).
    // Conductor call sites: asm 0x82645E1C..44 (f1/f2 = sim/game dt, r6 = &mCameraMatrix,
    // r7 = mbImpactTime, r8 = 0) and 0x82645EB0..D0 (r4 = manager-out, v1 = player pos).
    void UpdateTrafficPhysics(f32 lfSimTimeStep, f32 lfGameTimeStep,
                              const Matrix44Affine* lpCameraMatrix,
                              bool lbImpactTime, bool lbUnknownFalse);
    void PassNearbyCrashingTrafficIdsToRaceCarModule(
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface, Vector3 lPlayerPosition);

    // X360 0x8259BD10: validate (and fix up) a traffic-vs-traffic potential contact.
    bool ValidateAndFixUpTrafficTrafficContact(void* lpContact) const;

    // ⭐ ADDED 2026-08-06 (big-five #2, contact-generation wave). @0x825CACB8 (PS3 DecFIGS
    // 0x6E5DF8; DWARF h:260). Validate one traffic-vs-world potential contact against the
    // vehicle-input triangle cache. ⚠ FLAG: DECLARED for VehicleManager::ValidateTrafficContact's
    // closure; body still a TRAP STUB (169 X360 asm lines / 6 callees -- named, not landed,
    // this wave). The tri-cache arg is VehicleInputInterface::InTriangleCacheInterface ==
    // CgsSceneManager::SceneManagerIO::TriangleCacheInterface.
    bool ValidateTrafficContact(CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
                                const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
                                f32 lfTimeStep);

private:
    // ⭐ 2026-08-03 (the un-pin wave). VehicleManager owns this object by value and reaches TWO of
    // its tables directly, with bare loads that fire none of the asserts the matching accessors do.
    // Both are asm-proven, and both used to be modelled as SIBLING members of VehicleManager at
    // absolute class offsets that in fact fall INSIDE this object:
    //   * maTrafficEntityIDs -- SetRaceCarCrashing @0x82634C90 on the owner==2 (TRAFFIC) branch:
    //       extrwi r11, r31, 14,8 ; addis r11,r11,1 ; addi r11,r11,-0x6F58 ; slwi r11,r11,2
    //       lwzx r25, r11, r18                       ==  *(u32*)(vehicleManager + 4*idx + 148128)
    //     and 148128 == 44768 + 103360 == &mPhysicalTrafficManager.maTrafficEntityIDs[0]. Note the
    //     plain `lwzx`: the console does NOT go through GetGlobalTrafficEntityId there (that one
    //     fires an index assert AND an mUsedTrafficVehicles.IsBitSet assert), so routing the read
    //     through the accessor would ADD two asserts the X360 never fires at that site.
    //   * mu8GlobalToPhysicalEntityIndexMap -- GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe
    //     @0x825B4DE0 reads vehicleManager + 149456 + idx == 44768 + 104688 + idx.
    // Friendship is the minimum accommodation that lets those two sites read BY NAME; it makes no
    // layout claim and adds no API. (Adding public accessors instead would have invented API that
    // the console does not have, and would have hidden the assert-count difference above.)
    friend class VehicleManager;

    // ---- member SEQUENCE verbatim from the DecFIGS DWARF (offsets are X360/32-bit) --------
    TrafficPhysics                maFullTrafficPhysics[20];                 // :396
    EntityId                      maTrafficEntityIDs[20];                   // :397
    CgsResource::ResourceHandle   maTrafficCarModelHandles[20];             // :398
    VehicleDriver*                mpaTrafficDrivers;                        // :400 (X360 this+103600)
    PhysicalTrafficVehicle*       mpaTrafficVehicles;                       // :401 (X360 this+103604)
    SimpleVehiclePhysics*         mpaSimpleVehiclePhysics;                  // :402 (X360 this+103608)
    ArticulatedJointCreateBuffer* mpArticulatedJointCreateBuffer;          // :405 (X360 this+103612)
    ArticulatedJointPool          mArticulatedJointPool;                    // :406 (X360 this+103616)
    f32                           mfJointSwingBreakVelocity;                // :407
    f32                           mfJointTwistBreakVelocity;                // :408
    f32                           mfJointLinearBreakMph;                    // :409
    bool                          mbAllowArticulatedJointBreaking;          // :410
    RecycledTrafficIdArray        maRecycledTrafficThisFrame;               // :413
    TotalPhysicalTrafficBitArray  mUsedTrafficVehicles;                     // :415 (X360 word base this+104552)
    FullyPhysicalTrafficBitArray  mUsedFullTrafficPhysics;                  // :416
    SimpleTrafficBitArray         mUsedSimpleVehiclePhysics;                // :417
    TotalPhysicalTrafficBitArray  mPotentialTrafficVehicles;                // :419
    TotalPhysicalTrafficBitArray  mTrafficDeformationModelsActive;          // :420
    TotalPhysicalTrafficBitArray  mTestedTrafficVehicles;                   // :421
    TotalPhysicalTrafficBitArray  mAddedTrafficVehicles;                    // :422
    TotalPhysicalTrafficBitArray  mRemovedTrafficVehicles;                  // :423
    TotalPhysicalTrafficBitArray  mMadeSimpleTrafficVehicles;               // :424
    CgsModule::EventQueue<s8, 50> mUnusedPotentialTrafficQueue;             // :425
    // :429 (X360 this+104688). The X360 bound check in ValidateAndFixUpTrafficTrafficContact is
    // `GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)` and the build computes that
    // sizeof as exactly 600 (asm `cmplwi 0x258`). The DecFIGS DWARF spells the element count as
    // 601, but the asm is authoritative, so the map is sized 600 here to keep the `< sizeof(map)`
    // assert evaluating to the same 600 the X360 used.
    u8                            mu8GlobalToPhysicalEntityIndexMap[600];
    VecFloat                      mavfLowestPointWorldSpace[20];            // :432
    PhysicalTrafficManagerDebugComponent mDebugComponent;                   // :435
};

}   // namespace Vehicle
}   // namespace BrnPhysics
