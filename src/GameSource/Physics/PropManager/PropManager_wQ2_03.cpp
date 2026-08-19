// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_03.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, implementer 03, 2026-08-18). Folds back into
// BrnPropManager.cpp.
//
// THE THREE FUNCTIONS THIS FILE OWNS -- the two contact-generation legs
// BeginPropWorldContactGeneration dispatches on, and the queue drain EndPropWorldContactGeneration
// tail-calls:
//
//   * PropManager::DoPartWorldContactGeneration         @0x82611B70 (349 insns) -- ⭐ LANDED 2026-08-19
//   * PropManager::DoPropInstanceWorldContactGeneration @0x826120E8 (342 insns) -- ⭐ LANDED 2026-08-19
//   * PropManager::AddContactResultsToQueue             @0x82612F08 (184 insns) -- ⭐ LANDED 2026-08-18
//
// ⭐⭐ 2026-08-19 (wave Q6, cluster B "prop-vs-world contact generation"): THE TWO Do* LEGS ARE
// NO LONGER PARKED. Both blocking declarations landed this wave --
//   A. CgsSceneManager::CgsCollision::PrimitivePairListBuilder::AddPrimitive(const
//      rw::collision::Volume*, Matrix44Affine, f32, u16)   @0x82814AB8
//      -> CgsPrimitivePairListBuilder.h, above the Sphere overload (the DWARF's own order)
//   B. CgsSceneManager::CgsCollision::BaseCollisionGenerator::
//      CollidePrimitiveListAgainstTriangleList(...)         @0x828141D8
//      -> CgsCollisionGenerator.h, beside CollidePrimitivePairList
// Both bodies below were RE-DERIVED against the raw `assembly` arrays this wave (see the
// per-function decode blocks) rather than pasted from the park, and the park's two file-local
// constants were reconciled: KVF_MAX_CONTACT_GEN_PADDING is GONE -- its recovered name
// KVF_MAX_PROP_PADDING now lives in BrnPropManager.h (HEADER REQUEST D, landed wave Q4) and both
// bodies reach for it there.
//
// Instruction counts are (end-start)/4 over the measured function boundaries, stated
// END-EXCLUSIVE so the arithmetic and the addresses agree (round-2 NIT: the earlier line mixed the
// two conventions and one end address was simply wrong):
//     0x82611B70..0x826120E4   -> 349   (last instruction 0x826120E0 `b __restgprlr_16`)
//     0x826120E8..0x82612640   -> 342   (last instruction 0x8261263C `b __restgprlr_16`;
//                                        0x82612630 is `li r0,-0xA0`, three insns earlier)
//     0x82612F08..0x826131E8   -> 184
// For the first two the export's own `assembly` listing has exactly that many non-blank lines,
// which I also counted (349 / 342); the third has no per-address export and its boundary comes
// from ida_funcs.get_func on the .i64.
//
// -------------------------------------------------------------------------------------------------
// ⭐ HEADER REQUESTS A AND B -- BOTH LANDED 2026-08-19 (wave Q6). THE Do* LEGS ARE IN THIS FILE.
// -------------------------------------------------------------------------------------------------
// The round-2 diagnosis stands re-verified: `selfcheck.py
// scratchpad/waveQ2/probe_wQ2_03b/probe_bodies.cpp` used to return STATUS=fail with EXACTLY TWO
// DISTINCT diagnostics, one per missing declaration and each reported once per body --
//
//   error C2660: "CgsSceneManager::CgsCollision::PrimitivePairListBuilder::AddPrimitive":
//                function does not take 4 arguments
//                  ... "(rw::collision::Volume *, const Matrix44Affine, float, u16)"
//   error C2039: "CollidePrimitiveListAgainstTriangleList" is not a member of
//                "CgsSceneManager::CgsCollision::CollisionGenerator"
//
// -- and both are gone, because both declarations landed this wave.
//
// ---- HEADER REQUEST A -- LANDED ------------------------------------------------------------------
//   CgsPrimitivePairListBuilder.h, inside `struct PrimitivePairListBuilder`, public, ABOVE the
//   committed AddPrimitive(Sphere*, f32, u16) -- the DWARF's own declaration order (source :77):
//     void AddPrimitive(const ::rw::collision::Volume* lpVolume, Matrix44Affine lTransform,
//                       f32 lfPadding, u16 lu16PrimitiveTag);                    // @0x82814AB8
//   Register map re-measured this wave (headless IDA 9.3 on a PRIVATE .i64 copy; IDA leaves the
//   symbol `sub_82814AB8`, the function is 0x82814AB8..0x82814CE8, 140 insns):
//     r3 = the builder; r4 = the volume; r5 = the transform (four 16-byte rows);
//     f1 = the padding; r7 = the tag.
//   ⚠️ GOTCHA 3 IS WHY r6 IS SKIPPED: the f32 rides f1 and consumes its GPR slot, so the u16 tag
//   lands in r7. r6 is never read -- a signature derived from GPRs alone would invent a dead
//   fifth argument.
//   ⛔ ITS BODY IS A LINK HOLE AND IS **NOT** THIS WAVE'S (reported to the conductor, with the
//   whole decode on the declaration): it needs three sibling overloads that are unnamed in the
//   export (measured boundaries this wave -- sub_82814570 = AddPrimitive(Box*) 35 insns,
//   sub_82814600 = AddPrimitive(Capsule*) 29, sub_82814678 = AddPrimitive(Cylinder*) 36),
//   CgsGeometric::Box::Set @0x825E6918 (269, also bodyless), and two CgsGeometric primitive types
//   that do not exist in the tree at all (Capsule 32 bytes, Cylinder 80 bytes).
//
// ---- HEADER REQUEST B -- LANDED, **AND BODIED** --------------------------------------------------
//   CgsCollisionGenerator.h, inside `struct BaseCollisionGenerator`, beside CollidePrimitivePairList:
//     u16 CollidePrimitiveListAgainstTriangleList(const PrimitivePairList*, const TriangleList*,
//                                                 u16, u32, u16, bool);          // @0x828141D8
//   ⭐ The round-2 NIT asked whoever landed it to pick u16-with-a-note or s32-for-family-
//   consistency, but not to leave two undocumented spellings. RESOLVED: the DWARF's u16 is kept
//   (source :257) and the register truth carries the note on the declaration -- 0x8281428C
//   `mr r29,r3`, 0x82814298 `clrlwi r10,r29,16` for the LOCAL use only, 0x82814324 `mr r3,r29`
//   with no clrlwi on the return path. Every call site drops the result.
//   Its BODY landed with it, in CgsCollisionGenerator.cpp (86 insns, all callees present).
//   Seven measured call sites: DeformableObject::DoBodyPartWorldContactGeneration x2,
//   ::DoDetachedWheelWorldContactGeneration, VehicleManager::DoTrafficCarWorldContactGeneration
//   x2, and this pair (0x826120A8 / 0x82612604).
//   ⚠️ IT CANNOT BE DROPPED as "the dead arm": the selector is a RUNTIME byte read
//   (`lbz byte_82F2A39C`), not a compile-time constant -- the console emits BOTH arms.
//
// ---- HEADER REQUEST C (source-shape only, not a compile blocker) --------------------------------
//   `CgsSceneManager::CgsCollision::TriangleList::SetTriangleBuffer(const Triangle4*, s32)`
//   (DWARF CgsTriangleList.h:21) does not exist. The console inlines it to two member stores plus
//   its out-of-line CheckAlignment(), which is why `bl CheckAlignment` appears at 0x82612024 with
//   no `bl SetTriangleBuffer`. The parked bodies open-code the two stores, exactly as the
//   committed BrnVehicleManagerContactGeneration.cpp:591-593 / :619-621 already do. When it lands,
//   all four sites collapse onto it.
//
// ---- HEADER REQUESTS E / F / G (source-shape + names; NONE of them blocks anything) -------------
//   The DecFIGS scope for AddContactResultsToQueue (dwarfdump BrnPropManager.cpp:2975) names three
//   things the tree cannot spell today. All three are behaviour-neutral, and the body below uses
//   the SAME workaround the committed twin BrnVehicleManagerContactGeneration.cpp:684-765 already
//   ships, so this is not a new fork:
//     E. `CgsSceneManager::CgsCollision::CollisionResultList::GetPrimitiveTestResult(u16)` --
//        DWARF-named callee; does not exist. Its bounds tripwire ("lu16Index < mu16NumResults",
//        CgsCollisionResultList.h:148) IS reproduced explicitly below, and the 80-byte record walk
//        is done through a typed pointer. Home: CgsCollisionResult.h beside GetResult.
//        ⚠️ physfix.owner.md §3.3 lists this as a MISSING declaration that BLOCKS this function.
//        It does not block it -- see the "two wrong numbers" block below.
//     F. `CgsSceneManager::VolumeInstanceId::Set(...)` -- DWARF-named TWICE (once per side); does
//        not exist. Its argument list is NOT recoverable from this body (the console inlines it to
//        `extldi/or/std`), so it is NOT invented: the two muId words are assembled inline with the
//        console's own shifts and masks, exactly as the vehicle twin does at :750-753.
//     G. PARAMETER NAMES. The DWARF spells this function's parameters
//        `lpPotentialContactInterface, lpContactGenerator, lWorldEntityId`; the committed
//        AddContactResultsToQueue declaration in BrnPropManager.h spells the first two
//        `lpContactInterface, lpCollisionGenerator`.
//        This file matches the HEADER (a definition must), and the delta is filed rather than
//        silently diverged. Name-only: no type, order or count changes, so no caller moves.
//        (physfix.owner.md §3 says this signature was "left exactly as declared" because there is
//        no export to check registers against -- the DWARF scope line settles the names.)
//
// ---- HEADER REQUEST D (home for two recovered constants) -- CLOSED 2026-08-19 (wave Q6) ---------
//   The parked bodies carried two file-local AUTHORED-NAME constants over MEASURED values whose
//   real home is the KVF_* block in BrnPropManager.h/.cpp:
//     KVF_MAX_CONTACT_GEN_PADDING == Splat(0.3f)   and   KB_USE_CONTACT_GEN_STREAM == true.
//   ⭐ THE FIRST ONE IS HOMED **AND ITS NAME IS RECOVERED**: it is `extern const VecFloat
//   KVF_MAX_PROP_PADDING` in BrnPropManager.h (defined `= { 0.3f, 0.3f, 0.3f, 0.3f }` in
//   BrnPropManager.cpp with this file's own thunk/rodata provenance carried across).
//   ⭐ AND THE LANDING HONOURED IT: the two bodies below reach for the HEADER's constant under
//   that recovered name, and the file-local `KVF_MAX_CONTACT_GEN_PADDING` copy the parks carried
//   is GONE. That authored spelling no longer exists anywhere in the tree.
//   (Wave Q4 matched 0x82FB94F0's initialiser-table slot 0x82CD19AC to the DWARF's own source-order
//   file scope, where BrnPropManager.cpp:54 reads `const VecFloat KVF_MAX_PROP_PADDING;`.)
//   ⭐ THE SECOND ONE IS NOW MEASURED TOO, and its request is DISCHARGED rather than dropped. The
//   park said "KB_USE_CONTACT_GEN_STREAM has no address and no thunk cited anywhere, so there is
//   nothing for the header owner to re-measure". Both halves of that are now false: the address IS
//   cited (byte_82F2A39C, the `lbz` at 0x826125D4 / 0x82612078), and it was RE-MEASURED
//   INDEPENDENTLY this wave on a private .i64 copy (scratchpad/waveQ6/ida_worldc/out.json) --
//   the 32 bytes at 0x82F2A390 read
//       41D80000 41F00000 3CA3D70A 01000000 7F7FFFFF FFFFFFFF FFFFFFFF FFFFFFFF
//   so 0x82F2A39C == 0x01, inside an ordinary initialised .data run, not a zero page.
//   It STAYS FILE-LOCAL below (with that evidence attached) because it is a CgsCollision-side
//   switch with no DWARF name and no PropManager home -- seating an authored name in
//   BrnPropManager.h would be a worse fork than a documented file-local. The open request is
//   therefore narrowed, not closed: find its real home, do not re-measure its value.
//   ⭐ SCOPE OF THE CLAIM (round-2 NIT), now closed: this banner correctly warned that only the
//   0x82FB94F0 thunk had been walked end to end and that every other KVF_*/K_* pair must be
//   re-MEASURED through its own thunk before being seated. Round 3b did exactly that -- all 22
//   constants of BrnPropManager.h were walked individually, including the two Vector3-shaped
//   thunks (per-lane, not splats) and the two self-product _SQ thunks (which carry no rodata word
//   of their own). None was seated off a sibling's pattern.
//   ⭐ AND THE SENTENCE THIS FILE FLAGGED AS WRONG IS RETIRED AT SOURCE: "the export set contains
//   only readers, so the initial values are UNRECOVERED" is gone from BrnPropManager.h /
//   BrnPropManager.cpp / PropManager_wQ2_01 / _04 / _05 and from physfix.owner.md §5 N5. This
//   file's measurement is what started that; the recipe it wrote down is what finished it.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐ 0x82FB94F0 IS Splat(0.3f), **NOT** ZERO -- INDEPENDENTLY RE-MEASURED THIS ROUND
// -------------------------------------------------------------------------------------------------
// BrnPropManager.h's tuning-globals block and scratchpad/waveQ2/physfix.owner.md §5 N5 BOTH USED TO
// SAY (both retired 2026-08-18 round 3b, on the strength of this measurement) that every 0x82FB9xxx
// VecFloat "reads as all-zero on disk ... the export set contains only readers ... do not invent
// values". The first clause is true; the second is FALSE, and the difference is live behaviour.
// MEASURED by me, headless IDA 9.3 on the .i64 (scratchpad/waveQ2/probe_wQ2_03b/p1.txt):
//   * 0x82FB94F0 does read 16 zero bytes in the static image -- confirmed.
//   * A DYNAMIC-INITIALISER THUNK writes it. 0x82C5E750..0x82C5E774, nine instructions, not inside
//     any IDA function (which is exactly why an xrefs-over-exports scan reports "readers only"):
//         lis r11, flt_82004740@ha ; lfs f0, flt_82004740@l(r11) ; stfs f0,-0x10(r1)
//         lvlx v0,r0,r10 ; vspltw v0,v0,0 ; stvx128 v0,r0,unk_82FB94F0 ; blr
//     i.e. `unk_82FB94F0 = Splat(flt_82004740)`.
//   * flt_82004740 reads `3e 99 99 9a` big-endian == 0.30000001192092896f (raw bytes dumped).
//   * The thunk is reached: 0x82C5E750 appears in the MSVC dynamic-initialiser pointer run, at
//     0x82CD19AC (0x82CD1990 reads `82c5de68 82c5de90 82c5df40 82c5dfd8 82c5e6c0 82c5e6e8 82c5e710
//     82c5e750`; raw bytes dumped, not inferred from a name). ⚠️ The run CONTINUES past it --
//     0x82CD19B0 -> 82c5e778 -- so this thunk is one entry of the startup initialiser chain, not
//     its end. (Round-2 NIT: the earlier wording called 0x82C5E750 "the last dword of that run",
//     which was only true of the 32 bytes quoted; the reachability argument is unaffected.)
//  WHY IT MATTERS: the clamp is `Min(padding, that)`. At zero, every prop/part collision primitive
//  is posted with NO swept padding at all -- the same placeholder-zero failure mode the shadow
//  campaign lost a day to. At 0.3 it is a 30 cm cap on the per-frame swept expansion.
//  ⭐ ACTED ON 2026-08-18 round 3b: the constant now lives in BrnPropManager.h/.cpp at Splat(0.3f),
//  and the same walk found that FOURTEEN more of this subsystem's tuning globals were sitting at
//  placeholder zero for the same reason -- including KVF_GRAVITY_SCALE, whose zero made the prop
//  extra-gravity force NEGATIVE (the console posts `gravity * (scale - 1)`), i.e. smashed props
//  were being pushed upward. This file's 0x82FB94F0 measurement is what exposed all of them.
//
// -------------------------------------------------------------------------------------------------
// ⚠️ TWO WRONG NUMBERS IN THE COMMITTED HEADER (reported, NOT edited -- this file owns no header)
// -------------------------------------------------------------------------------------------------
//   1. The AddContactResultsToQueue declaration comment in BrnPropManager.h says its span is
//      "82612F08-826131E8, 736 insns".
//      736 is the BYTE length (0x2E0). The function is 184 instructions. Same slip appears in
//      scratchpad/waveQ2/physfix.owner.md's table ("~736").
//   2. physfix.owner.md §3.3 lists `CollisionResultList::GetPrimitiveTestResult` as a MISSING
//      declaration that AddContactResultsToQueue needs. It does not need it: the console walks the
//      records at the raw 80-byte stride off `mpResults` and bakes the bound assert itself, which
//      is exactly what the committed twin BrnVehicleManagerContactGeneration.cpp:713-722 already
//      does. That entry did not block this function.
//
// -------------------------------------------------------------------------------------------------
// LINK-LEVEL FACTS (gate-green != link-green) -- RE-GREPPED 2026-08-19 (wave Q6)
// -------------------------------------------------------------------------------------------------
//   * NO gate and NO stub exists for ANY of this file's three functions. Re-grepped this wave over
//     GameSource/Physics/BrnPhysicsConductorGates.cpp, GameSource/World/WorldLinkStubs.cpp and the
//     whole of b5-decomp/src: the only hits are the header declarations and comments. This file is
//     the SOLE definition of all three.
//   * ⭐ THIS FILE IS MOUNTED (tools/build/build_game_exe.bat:1796), so the two Do* bodies landing
//     here go straight into the link. Their callees, checked ONE BY ONE this wave:
//       BODIED IN THE TREE -- PropManager::HasProp/HasPartJustBeenRemoved (PropManager_wQ2_04.cpp),
//       ResourcePtr<PropPhysicsDataHeader>::operator-> (BrnPropQueueFacades.cpp:87),
//       PropPhysicsDataHeader::GetType, PropTypeData/PropPartTypeData accessors (header inlines),
//       PropInstance/PropPartInstance velocity accessors (header inlines),
//       PrimitivePairListBuilder::Prepare (CgsPrimitivePairListBuilder.cpp:40),
//       TriangleCacheInterface::GetCache / GetNumCachedTriangleBatches
//       (CgsSceneManagerModuleIO.cpp), TriangleList::CheckAlignment / ValidateTriangles
//       (CgsTriangleList.cpp), Triangle4::AssertIsValid, PropEntityID::GetValue,
//       CgsDev::StrStream + the Begin/Fire/EndAssert trio, rw::math::vpu::{Magnitude, Splat, Min,
//       GetComponent, operator*}, and -- NEW THIS WAVE --
//       BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList
//       (CgsCollisionGenerator.cpp, landed with its declaration).
//     ⛔ **TWO LINK HOLES REMAIN, AND THEY ARE REPORTED, NOT PAPERED OVER** (gotcha 12):
//       1. PrimitivePairListBuilder::AddPrimitive(const rw::collision::Volume*, Matrix44Affine,
//          f32, u16) @0x82814AB8 -- declared this wave, NO BODY. Full decode + the three unnamed
//          sibling overloads + the two missing CgsGeometric types are written up on the
//          declaration in CgsPrimitivePairListBuilder.h. THIS IS THE ONE THAT MATTERS AT RUNTIME:
//          until it exists, a prop's collision volumes never become collision primitives.
//       2. BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream @0x82811D40 --
//          declared since 2026-08-18 (CgsCollisionGenerator.h), still NO BODY; it is the LIVE arm
//          (byte_82F2A39C == 1, re-measured this wave), together with the Create/Run halves
//          @0x82811DD0 / @0x82811F58 whose bodies sit parked at
//          scratchpad/waveQ2/parked/CgsCollisionGenerator_wQ2_PrimitiveStream.cpp on ONE type --
//          PrimitiveListWithTriangleListStreamJobDesc -- plus ONE enum member
//          (E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12, its type-12 worker is
//          already a named gate at ContactGeneratorJob.cpp:230).
//          ⚠️ THAT PARK'S BANNER IS STALE IN THE HELPFUL DIRECTION: it says the descriptor
//          "has no home in this tree", but CgsPrimitiveListWithTriangleListJobDesc.h/.cpp EXIST and
//          are MOUNTED (bat:2864) -- what is missing is only the *Stream* variant beside them.
//   * ⚠️ INHERITED, still open, not this file's to fix: PropManager_wQ_03.cpp's UpdateTriangleCache
//     and PropManager_wQ2_02.cpp's Begin/End still have one-shot gate bodies in
//     BrnPhysicsConductorGates.cpp (re-measured 2026-08-19: Begin :471-476, End :478-483,
//     UpdateTriangleCache :522-527 -- the old ":485/:492/:514" citation had drifted). They collide
//     at LINK time the moment those partfiles are added to tools/build/build_game_exe.bat (the
//     `rem` at :1777-1780 says so). Mount + retire in one commit.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                  // CgsDev::StrStream (the :2478 / :2600 messages)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                                    // CgsMemory::LinearMalloc (parameter)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                                  // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"              // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CollisionGenerator
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"      // CollisionResultList / PrimitiveTestResult
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"         // TriangleList
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"                 // PotentialContact
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"                  // PotentialContactInterface::AddEvent
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"                       // PropInstance
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"                   // PropPartInstance
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                            // UpdatePropEvent
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"                    // KU_MAX_PHYSICAL_PROPS / _PROP_PARTS
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                                      // PropEntityID::GetValue (owner tripwire)
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"                               // PropTypeData / PropPartTypeData
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"                             // PropPhysicsDataHeader::GetType
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                      // rw::collision::Volume

#include "rw/math/vpu/vector3_operation.h"        // Magnitude
#include "rw/math/vpu/vector4_operation.h"        // Splat / Min / GetComponent / operator*
#include "rw/math/vpu/matrix44affine_operation.h" // Matrix44Affine operator*

// ⚠️ Do NOT add vendor/renderware/collision/AABBox.hpp here -- it drags in the second
//    rw::math::vpu::Vector3 and hard-C2011s against BrnCommonTypes.h (physfix.owner.md REQUEST 1b).
//    volume_debug_access.h only forward-declares AABBox, which is all these bodies need.

namespace BrnPhysics
{
namespace Props
{

namespace vpu = ::rw::math::vpu;

// =================================================================================================
// FILE-LOCAL CONSTANTS FOR THE TWO CONTACT-GENERATION LEGS
// =================================================================================================
// byte_82F2A39C -- the STREAM-vs-SYNCHRONOUS selector both legs read (`lbz` at 0x826125D4 in the
// prop leg and 0x82612078 in the part leg). AUTHORED NAME (no symbol survives), MEASURED VALUE,
// RE-MEASURED INDEPENDENTLY 2026-08-19 on a private copy of the .i64
// (scratchpad/waveQ6/ida_worldc/out.json): the 32 bytes at 0x82F2A390 read
//     41D80000 41F00000 3CA3D70A 01000000 7F7FFFFF FFFFFFFF FFFFFFFF FFFFFFFF
// so the byte at 0x82F2A39C is 0x01 -- one byte inside a plain initialised-data run (its
// neighbours are 27.0f, 30.0f, 0.02f and FLT_MAX), NOT a zero page. So on the shipped console
// build the STREAM arm is the live one.
// ⚠️ THE 0x82FB9xxx "PLACEHOLDER ZERO" TRAP (gotcha 13) DOES NOT APPLY: that trap is
// zero-in-the-static-image plus a dynamic-initialiser thunk that writes the real value at startup.
// This byte is already non-zero in the image, so there is nothing for a thunk to supply. (The
// wave-Q round-2 measurement additionally reported a whole-export-set xref scan returning exactly
// two references, both the `lbz` reads in this pair of functions -- attributed, not re-run here.
// What THIS wave re-measured is the value.)
// ⚠️ THE SYNCHRONOUS ARM STILL CANNOT BE DROPPED: the read is a RUNTIME load, not a folded
// constant, which is why the console emits BOTH arms and why both are reproduced below.
// ⚠️ HOME NOT RECOVERED -- see HEADER REQUEST D in the banner. It is a CgsCollision-side switch
// with no DWARF name, so it stays file-local here rather than being seated under an authored name
// in BrnPropManager.h.
static const bool KB_USE_CONTACT_GEN_STREAM = true;   // byte_82F2A39C == 0x01, measured

// The triangle-cache slot map. Both are DWARF-attested WITH THEIR VALUES in
// references/DecFIGS/dwarfdump/GameSource/Physics/BrnTriangleCacheConstants.h:36 / :37, and both
// are X360-attested in these two bodies: `addi r21, liPropIndex, 0x1C` (0x826122B8, == 28) and
// `addi r20, liPartIndex, 0x2B` (0x82611D40, == 43).
// ⚠️ SAME MOVE-WHEN-IT-LANDS CAVEAT, AND THE SAME FOLD HAZARD, as the sibling part-files:
// b5-decomp/src/GameSource/Physics/BrnTriangleCacheConstants.h -- their DWARF home -- still does
// not exist (re-checked 2026-08-19), and PropManager_wQ2_02.cpp:97-98 and PropManager_wQ2_04.cpp
// declare the SAME two file-local names. Internal linkage keeps that harmless across translation
// units, but every one of these part-files folds back into BrnPropManager.cpp: whichever folds
// second must drop its copy, or all of them switch to BrnTriangleCacheConstants.h in one commit.
// The names deliberately MATCH wQ2_02's spelling so that fold is a delete, not a rename.
static const s32 KI_PROP_CACHE_START_INDEX      = 28;   // DWARF BrnTriangleCacheConstants.h:36
static const s32 KI_PROP_PART_CACHE_START_INDEX = 43;   // DWARF BrnTriangleCacheConstants.h:37

// Console `li r6, 0x40` at all four collide call sites of this pair.
static const u16 KU16_COLLIDE_MAX_RESULTS = 64;

// =================================================================================================
// BrnPhysics::Props::PropManager::AddContactResultsToQueue @0x82612F08  (184 asm insns)
//
// ⚠️ EXPORT HOLE, NOT A MISSING FUNCTION. 0x82612F08 has no .ida-exports per-address JSON and no
// identity.json row, so `work show` and the dossier tooling report it absent. It is present in the
// IDB: `ida_funcs.get_func(0x82612F08)` returns 0x82612F08..0x826131E8 named
// `BrnPhysics::Props::PropManager::AddContactResultsToQueue`, and I disassembled all 184
// instructions with headless IDA 9.3 this round (raw dump:
// scratchpad/waveQ2/probe_wQ2_03b/p1.txt). Every line below is decoded from that dump. This is the
// same situation, and the same resolution, the tree already recorded for VehicleManager::
// IsRaceCarHidden @0x825C2EA0 ("MISSING JSON != MISSING FUNCTION",
// BrnVehicleManagerContactGeneration.cpp:651-655).
//
// ---- THE ICF QUESTION, ANSWERED: IT IS **NOT** AN ICF FOLD OF THE VEHICLE TWIN ------------------
// The task asked whether this is an identical-COMDAT-folding alias of the ledger-done
// BrnPhysics::Vehicle::VehicleManager::AddContactResultsToQueue @0x825EB350
// (BrnVehicleManagerContactGeneration.cpp:684). It is not, and the disproof is structural, not a
// judgement call -- an ICF fold is ONE body at ONE address with several names, and these are two
// distinct address ranges (0x82612F08..0x826131E8, 184 insns; 0x825EB350..0x825EB6C8, 222 insns).
// Eight measured behavioural differences, any one of which is sufficient:
//   1. Outer bound. Vehicle walks [0, miFirstPartContactGenEntry). Prop walks
//      [0, GetNumUsedResultLists()) and opens with an assert that
//      miNumPropsAddedToContactGen == that count (:2813) -- an assert with no Vehicle counterpart.
//   2. Entity words. Vehicle takes BOTH from a ContactGenList entry. Prop takes side A from
//      maPropsAddedToContactGen[liResultsList] and side B from the lWorldEntityId PARAMETER
//      (which the Vehicle signature does not even have).
//   3. Normal. Vehicle SIGN-FLIPS the source normal (vxor against a 0x80000000 splat) and
//      TAG-SELECTS between mPrimitive0Normal and mPrimitive1Normal on mu16UserTagB. Prop does
//      neither: `lvx128 v127, r0, r30` (record +0x00) is stored to the contact's mNormal
//      unmodified. There is no vxor and no vcmp anywhere in the 184 instructions.
//   4. Volume-instance low word. Vehicle uses (entry volume-instance base + primitive index).
//      Prop uses the primitive index TRUNCATED TO 8 BITS (`clrlwi r9,r9,24`).
//   5. AddEvent overload. Vehicle calls sub_825E73D0 == AddEvent(u32 queueID, contact). Prop
//      calls PotentialContactInterface::AddEvent(contact) -- the single-argument overload, r3/r4
//      only, no queue id in sight.
//   6. Prop carries a debug ring-buffer write (mpDebugWorldContacts / miNumDebugWorldContacts)
//      that Vehicle has no trace of.
//   7. Prop carries the E_ENTITYTYPE_PROP owner tripwire (BrnPropEntityID.h:278) per record.
//   8. Vehicle carries the "Bad Part index: " tripwire (:1317) that Prop has no trace of.
// So this is a real, separately-compiled sibling and it is reconstructed here on its own asm. The
// committed Vehicle body is used only as the IDIOM precedent for the shared shapes (the 80-byte
// record walk, the by-value result list, the PotentialContact field mapping).
//
// ---- DWARF GROUNDING (DecFIGS dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:2975)
// The scope's declaration line and every local it names line up with the asm one for one:
//     int32_t liResultsList @2810 - uint16_t lu16Result @2811 - CollisionResultList lResultList
//     @2818 - const PrimitiveTestResult& lResult @2825 - PotentialContact lContact @2829
// and its callee list is
//     BaseCollisionGenerator::GetResultList - CollisionResultList::GetPrimitiveTestResult -
//     BrnWorld::PropEntityID::operator CgsSceneManager::EntityId - rw::math::vpu::Vector3::
//     operator= (x2) - PhysicsModuleIO::PotentialContactInterface::AddEvent -
//     CgsSceneManager::VolumeInstanceId::Set (x2) -
//     CgsModule::BaseEventQueue<PotentialContact>::AddEventSafe.
// Note what is ABSENT from that list: no AddEvent(u32,...), no negation helper, no tag compare --
// three more independent confirmations of the ICF verdict below.
// ⚠️ TWO HONEST DELTAS between the DecFIGS scope and the shipped ARTIST body, stated not hidden:
//   * The scope ENDS with a `{ CgsDev::StrStream; operator<< }` block -- one more assert with a
//     BUILT message. The X360 ARTIST body emits NO StrStream call at all (I scanned all 184
//     instructions: no `bl` to StrStream, StrStreamBase or BasePriorityQueue::Clear). The two
//     builds differ here; the ARTIST body is what is reconstructed, and the missing assert is
//     recorded rather than invented.
//   * The scope names `PropEntityID::operator CgsSceneManager::EntityId` where the body below
//     calls `GetValue()`. Both are the SAME two console instructions -- the owner tripwire then
//     `lwz` the packed word -- and GetValue()'s committed body carries AssertIsProp()
//     (BrnPropEntityID.cpp:50). The conversion operator is NOT used because this tree's
//     `PropEntityID::operator EntityId()` returns the BrnCommonTypes `EntityId {u32 muValue}`,
//     which is a DIFFERENT type from the `CgsSceneManager::EntityId` the DWARF names and the one
//     this function's third parameter has. That two-EntityId fork is pre-existing and is not
//     this file's to resolve.
//
// ---- REGISTER MAP, read off the prologue 0x82612F2C..0x82612F44 ---------------------------------
//     r3 = this (r31) - r4 = lpContactInterface (r21) - r5 = lpCollisionGenerator (r25)
//     r6 = lWorldEntityId (spilled to arg_2C, reloaded per result list)
// Corroborated by the caller: EndPropWorldContactGeneration @0x82628E8C does
// `mr r6,r26 ; mr r5,r30 ; mr r4,r27 ; mr r3,r31 ; bl` with r27 = its own lpContactInterface,
// r30 = its lpCollisionGenerator and r26 = its lWorldEntityId. No float or vector parameter is
// involved, so gotcha 3 does not apply and all three slots are ordinary GPRs.
//
// ---- DECODE, address by address -----------------------------------------------------------------
//   0x82612F38  addis r29,r25,1 ; addi r29,r29,0x23BC  -> r29 = generator + 0x123BC, the address
//        of mu16NumUsedResultLists, HOISTED and re-read through for the whole function.
//        ⚠️ 0x123BC IS A CONSOLE OFFSET (gotcha 1): the host BaseCollisionGenerator is wider
//        (IOBuffer base + 8-byte pointers in mapCollisionResultLists[200]). Reached by accessor.
//   0x82612F48..0x82612F78  `lwz 0x6570` (miNumPropsAddedToContactGen, s32) vs `lhz 0(r29)`
//        (a u16) under a SIGNED `cmpw` -> assert :2813 (li r5,0xAFD), NON-GATING (falls through).
//        String read in full from the image: "miNumPropsAddedToContactGen ==
//        lpContactGenerator->GetNumUsedResultLists()".
//   0x82612F7C..0x82612F88  `cmpwi r10,0 ; ble` -> the whole function is skipped for an empty
//        generator. The bound is RE-READ at 0x826131B4 at the bottom of every outer iteration,
//        so it is live, not hoisted into a local.
//   0x82612F90  addi r26,r31,0x64BC  -> &maPropsAddedToContactGen[0]; `addi r26,r26,4` at
//        0x826131BC walks it in lockstep with the outer counter. Reached by index here.
//   0x82612FCC..0x82612FEC  `clrlwi r30,r20,16` then the bounds assert
//        "luIndex < mu16NumUsedResultLists" (CgsCollisionGenerator.h:303, li r5,0x12F) --
//        that is BaseCollisionGenerator::GetResultList's OWN tripwire, inlined; reproduced by
//        calling the accessor, whose committed body (CgsCollisionGenerator.cpp:130) carries it.
//   0x82612FF0..0x8261301C  `addi r10,r30,0x4820 ; slwi 2 ; lwzx r10,r10,r25` == the pointer in
//        mapCollisionResultLists[index] (console generator+0x12080), then FOUR word copies of the
//        16-byte list header onto the stack -- i.e. GetResultList returns the list BY VALUE.
//   0x82613020..0x82613028  `lhz` at list+0xC == mu16NumResults; zero -> next result list.
//   0x82613034  `lwz` at list+0x00 == mpResults, the record base.
//   0x82613038  `extldi r24,r11,64,32` == (u64)lWorldEntityId << 32. Computed ONCE per result
//        list, which is why the parameter is re-read from its spill slot here and not per record.
//   0x8261303C..0x82613058  assert "lu16Index < mu16NumResults" (CgsCollisionResultList.h:148,
//        li r5,0x94) -- the per-record cursor bound, at the TOP of the inner body.
//   0x8261305C..0x82613068  `r29*5 <<4` == index * 80 -> THE RECORD STRIDE IS 80, i.e. these are
//        PrimitiveTestResults (meResultType == 0), NOT the 112-byte CollisionResult that
//        CollisionResultList::GetResult indexes. Walked through a typed pointer here, exactly as
//        the committed vehicle twin does; no GetPrimitiveTestResult accessor is required.
//   0x8261306C..0x826130D0  `lbz 0(r26) ; cmplwi 3` + assert
//        "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" (BrnPropEntityID.h:278, li r5,0x116).
//        That is PropEntityID::GetValue()'s own inlined AssertIsProp, re-run per record, so it is
//        reproduced by calling GetValue() inside the inner loop rather than spelled out.
//   0x82613078..0x826130B4  the record -> contact copy. Emission order is not source order; the
//        MAPPING is what is measured, and it is exact:
//            contact +0x00 mPointOnA           <- record +0x20 mPrimitive0Contact  (v126)
//            contact +0x10 mPointOnB           <- record +0x30 mPrimitive1Contact  (v125)
//            contact +0x20 mNormal             <- record +0x00 mPrimitive0Normal   (v127)
//            contact +0x40 muPolyTagA          <- record +0x40 muPrimitive0Tag
//            contact +0x44 muPolyTagB          <- record +0x44 muPrimitive1Tag
//            contact +0x48 mu16PrimitiveIndexA <- record +0x48 muPrimitive0Index   (RAW u16)
//            contact +0x4A mu16PrimitiveIndexB <- record +0x4A muPrimitive1Index   (RAW u16)
//        ⚠️ record +0x10 mPrimitive1Normal IS NEVER READ by this function. Measured, stated.
//   0x826130D4..0x82613100  the two 64-bit volume-instance ids:
//            +0x30 muVolumeInstanceIdA = ((u64)maPropsAddedToContactGen[i] << 32)
//                                        | (muPrimitive0Index & 0xFF)
//            +0x38 muVolumeInstanceIdB = ((u64)lWorldEntityId          << 32)
//                                        | (muPrimitive1Index & 0xFF)
//        The `clrlwi r9,r9,24` / `clrlwi r10,r10,24` are the 8-bit truncations; they are in the
//        asm and are reproduced, not tidied away. The <<32 placement matches CgsVolumeInstanceId.h's
//        documented layout (entity word in the HIGH dword, volume index in the low).
//   0x82613104  bl PotentialContactInterface::AddEvent  -- r3, r4 ONLY. The single-argument
//        overload @0x825E72F0, not the (u32, contact) one the vehicle path uses.
//   0x82613108..0x82613198  the debug ring. `lwz 0x64B0` null-guard, then THREE stores at
//        `base + (count % 32) * 48`:  v127(record+0x00) -> +0x20 mNormal,
//        v126(record+0x20) -> +0x00 mPoint0, v125(record+0x30) -> +0x10 mPoint1, then
//        `lwz 0x64B4 ; addi 1 ; stw 0x64B4`. The modulo is the signed
//        `srawi 5 / addze / slwi 5 / subf` idiom == `count % KI_MAX_DEBUG_WORLD_CONTACTS`.
//        ⚠️ The console recomputes the base+index THREE TIMES (it reloads mpDebugWorldContacts and
//        miNumDebugWorldContacts before each store). That is the compiler's aliasing conservatism
//        across the intervening AddEvent, not three different indices -- the counter is not
//        touched between them. Written as one reference + three field stores.
//        ⚠️ The counter is NOT wrapped, only the index is; End() zeroes it each frame
//        (`stw r28,0x64B4` at 0x82628E90, already committed).
//   0x8261319C..0x826131C4  `(u16)(++lu16Result)` inner bound, then the outer `++liResultsList`
//        with the re-read bound.
//
// ⚠️ NOT REPRODUCED, deliberately, and it is not a divergence: the console never calls
// PotentialContact::Construct here (no `bl`, no zero-fill of the record). The stack record is left
// default-initialised and every one of its nine members is assigned below, so nothing carries
// console stack garbage -- but mu16TestIndex/muPad have no counterpart on this struct at all, so
// there is nothing left uninitialised either. Same shape as the committed vehicle twin.
//
// Every console offset above appears in a COMMENT only; every member is reached by name.
// =================================================================================================
void PropManager::AddContactResultsToQueue(
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*      lpCollisionGenerator,
    CgsSceneManager::EntityId                               lWorldEntityId )
{
    typedef CgsSceneManager::CgsCollision::CollisionResultList CollisionResultList;
    typedef CgsSceneManager::CgsCollision::PrimitiveTestResult PrimitiveTestResult;
    typedef CgsSceneManager::SceneManagerIO::PotentialContact  PotentialContact;

    // :2813 -- non-gating. s32 member against a u16 accessor under the console's SIGNED compare;
    // the cast keeps that signedness rather than letting the comparison go unsigned.
    CGS_ASSERT( miNumPropsAddedToContactGen
                    == static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() ),
                "miNumPropsAddedToContactGen == lpContactGenerator->GetNumUsedResultLists()" );

    // The bound is re-read every iteration (lhz through r29 at 0x82612F7C and 0x826131B4).
    for ( s32 liResultsList = 0;
          liResultsList < static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() );
          ++liResultsList )
    {
        // Carries the console's own "luIndex < mu16NumUsedResultLists" tripwire
        // (CgsCollisionGenerator.h:303) and returns the 16-byte header BY VALUE, as @0x825B2AE0.
        const CollisionResultList lResultList =
            lpCollisionGenerator->GetResultList( static_cast<u16>( liResultsList ) );

        const u16 lu16NumResults = lResultList.mu16NumResults;
        if ( lu16NumResults == 0 )
        {
            continue;                                                   // 0x82613028 beq
        }

        // 0x82613038 -- hoisted out of the record loop by the console, once per result list.
        const u64 lu64WorldEntityWord = static_cast<u64>( static_cast<u32>( lWorldEntityId ) ) << 32;

        // meResultType == 0 lists carry 80-byte PrimitiveTestResults; the console walks them at
        // that raw stride off mpResults (`index*80`), NOT through the 112-stride GetResult.
        // Identical to the committed twin, BrnVehicleManagerContactGeneration.cpp:713-715.
        const PrimitiveTestResult* lpaResults =
            reinterpret_cast<const PrimitiveTestResult*>( lResultList.mpResults );

        for ( u16 lu16Result = 0; lu16Result < lu16NumResults; ++lu16Result )
        {
            // The per-record cursor bound (CgsCollisionResultList.h:148), at the top of the body.
            CGS_ASSERT( lu16Result < lResultList.mu16NumResults, "lu16Index < mu16NumResults" );
            const PrimitiveTestResult& lrResult = lpaResults[lu16Result];

            // 0x8261306C..0x826130D0 -- GetValue() carries the E_ENTITYTYPE_PROP owner tripwire
            // (BrnPropEntityID.h:278) the console re-runs per record.
            const u32 luPropEntityWord =
                maPropsAddedToContactGen[liResultsList].GetValue();

            PotentialContact lContact;

            // Full 16-byte lane copies (lvx/stvx on the console): Vector3Plus source, Vector3
            // destination, w lane carried verbatim -- the committed twin's spelling.
            lContact.mPointOnA = Vector3{ lrResult.mPrimitive0Contact.x,
                                          lrResult.mPrimitive0Contact.y,
                                          lrResult.mPrimitive0Contact.z,
                                          lrResult.mPrimitive0Contact.w };
            lContact.mPointOnB = Vector3{ lrResult.mPrimitive1Contact.x,
                                          lrResult.mPrimitive1Contact.y,
                                          lrResult.mPrimitive1Contact.z,
                                          lrResult.mPrimitive1Contact.w };

            // ⚠️ UNMODIFIED -- no sign flip and no tag selection on this path. See difference (3)
            // in the ICF block above; the vehicle twin's negation must NOT be copied here.
            lContact.mNormal = lrResult.mPrimitive0Normal;

            // The 8-bit truncations are the console's (`clrlwi rN,rN,24`), not a tidy-up.
            lContact.muVolumeInstanceIdA.muId =
                ( static_cast<u64>( luPropEntityWord ) << 32 )
                | static_cast<u64>( lrResult.muPrimitive0Index & 0x00FFu );
            lContact.muVolumeInstanceIdB.muId =
                lu64WorldEntityWord
                | static_cast<u64>( lrResult.muPrimitive1Index & 0x00FFu );

            lContact.muPolyTagA          = lrResult.muPrimitive0Tag;
            lContact.muPolyTagB          = lrResult.muPrimitive1Tag;
            lContact.mu16PrimitiveIndexA = lrResult.muPrimitive0Index;   // RAW u16, not truncated
            lContact.mu16PrimitiveIndexB = lrResult.muPrimitive1Index;

            // 0x82613104 -- the SINGLE-argument overload (r3/r4 only).
            lpContactInterface->AddEvent( lContact );

            // 0x82613108 -- the debug ring, null-guarded. Store order in the asm is
            // mNormal / mPoint0 / mPoint1; written in struct order here (three independent field
            // stores to the same record, no aliasing between them).
            if ( mpDebugWorldContacts != NULL )
            {
                DebugWorldContactInfo& lrDebugContact =
                    mpDebugWorldContacts[miNumDebugWorldContacts % KI_MAX_DEBUG_WORLD_CONTACTS];

                lrDebugContact.mPoint0 = Vector3{ lrResult.mPrimitive0Contact.x,
                                                  lrResult.mPrimitive0Contact.y,
                                                  lrResult.mPrimitive0Contact.z,
                                                  lrResult.mPrimitive0Contact.w };
                lrDebugContact.mPoint1 = Vector3{ lrResult.mPrimitive1Contact.x,
                                                  lrResult.mPrimitive1Contact.y,
                                                  lrResult.mPrimitive1Contact.z,
                                                  lrResult.mPrimitive1Contact.w };
                lrDebugContact.mNormal = lrResult.mPrimitive0Normal;

                ++miNumDebugWorldContacts;                              // 0x82613190, unwrapped
            }
        }
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::DoPartWorldContactGeneration @0x82611B70  (349 asm insns)
//
// ⭐ LANDED 2026-08-19 (wave Q6). One leg of BeginPropWorldContactGeneration's per-UpdatePropEvent
// dispatch -- the arm taken when the event's PropEntityID carries a NON-ZERO part index, i.e. a
// SHED PANEL of a smashed prop. It builds a primitive-pair list out of that part's collision
// volumes, pairs it with the world triangles cached for the part's triangle-cache slot, and posts
// one collision job.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82611B70.json,
// re-read line by line this wave (scratchpad/waveQ6/asm_82611B70.txt). The Hex-Rays pseudocode in
// that same export was NOT consulted -- its `a5, a6` prototype drops the vector parameter entirely.
// 349 == (0x826120E0 - 0x82611B70)/4 + 1, and the export listing has exactly 349 lines.
//
// ---- CONSOLE ARGUMENT MAP, read off the prologue 0x82611B88..0x82611BA4 ------------------------
//     r3 = this (r19) · r4 = lpContactGenerator (r17) · r5 = lpTriCache (r18)
//     r6 = &lUpdatePartEvent (r22) · r7 = &liNumJobsAdded (r16) · r8 = lpMalloc (r26)
//     v1 = lvfTimeStep (saved to v127 at 0x82611B8C)
//   ⚠️ A VECTOR ARG CONSUMES NO GPR SLOT, so lvfTimeStep's POSITION in the parameter list is
//   DWARF-attested only (BrnPropManager.h carries the same note on the declaration); the asm alone
//   cannot order it.
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x82611BA8  cmplwi r17,0 / bne        assert "lpContactGenerator != NULL", li r5,0x991 ==
//                                         BrnPropManager.cpp:2449. NON-GATING (falls through).
//   0x82611BD0  lhz 0x64(r22) / extsh     liPartIndex = event.miPhysicsSlot -- the member is s16
//                                         and is SIGN-extended into an s32 local.
//   0x82611BD8  cmpwi 0 / bge             assert "liPartIndex >= 0"          li r5,0x9A2 == :2466
//   0x82611BFC  cmpwi 0x1E / blt          assert "liPartIndex < ...", li r5,0x9A3 == :2467.
//                                         0x1E == 30 == KU_MAX_PHYSICAL_PROP_PARTS.
//   0x82611C30  bl HasPartJustBeenRemoved(event.mEntityId @0x60, liPartIndex)
//   0x82611C3C  clrlwi/bne -> 0x826120D0  TRUE => return (branch straight to the epilogue).
//   0x82611C44..0x82611D2C                the SECOND, message-CARRYING range check, which fires
//                                         only when liPartIndex is outside [0,30):
//                                           "Updated part was removed: Part ID = " << u32 id
//                                           << ", Returned part index = " << s32 index
//                                         li r5,0x9AE == :2478, exactly the DWARF's
//                                         `CgsDev::StrStream lStrStream @ BrnPropManager.cpp:2478`.
//                                         ⚠️ It also re-runs the mEntityId.GetOwner() ==
//                                         E_ENTITYTYPE_PROP tripwire (BrnPropEntityID.h:278,
//                                         li r5,0x116) -- that is PropEntityID::GetValue()'s OWN
//                                         inlined assert, not a separate source line, so it is
//                                         reproduced by CALLING GetValue() rather than spelled out.
//   0x82611D30  lwz 0x8C(r19) + slwi r29,6   lpPart = &mpaPartInstances[liPartIndex]
//                                         (console stride 64 == the CONSOLE sizeof(PropPartInstance);
//                                          the host indexes the typed array -- gotcha 1).
//   0x82611D40  addi r20, r29, 0x2B       liCacheSlotIndex = 43 + liPartIndex.
//   0x82611D44/48  lwz 0x34 / lbz 0x38    luTypeId = lpPart->GetType(), luPartId = GetPartId();
//                                         both loaded BEFORE the operator-> call clobbers r3.
//   0x82611D4C  bl ResourcePtr<PropPhysicsDataHeader>::operator-> on this+0x54 (0x822868E0,
//               identified in the tree at BrnPropQueueFacades.cpp:87), then GetType(luTypeId).
//   0x82611D60..0x82611D78  `slwi r9,r11,1 ; add r11,r11,r9 ; slwi r11,r11,4` == luPartId * 48,
//               added to `lwz 0x40(propType)` == PropTypeData::maParts.
//               ⚠️ 48 IS THE CONSOLE STRIDE; the HOST PropPartTypeData is 64 bytes, which is why
//               this indexes GetParts()[luPartId] instead of doing the console's byte arithmetic.
//   0x82611D7C  lbz 0x2C(partType)        lpPartType->GetNumberOfVolumes()
//   0x82611D80  bl PrimitivePairListBuilder::Prepare(lpMalloc, that count)
//
//   ---- THE PADDING (0x82611D84..0x82611E5C) ----------------------------------------------------
//   0x82611D98  lfs 0x28(partType)        the bounding radius, splatted (`vspltw ,0`).
//   0x82611DA8  lvx r31+0x10 / r31+0x20   lpPart->GetLinearVelocity() / GetAngularVelocity()
//   0x82611DB4  vmsum3fp128 x2            the two 3-lane MagnitudeSquared
//   0x82611DD4..0x82611E4C                vrsqrtefp + TWO Newton-Raphson refinements, then
//                                         lenSq * rsqrt == the magnitude, with a
//                                         `vcmpeqfp(0,lenSq)` + `vsel` zero-length guard.
//     ⚠️⚠️ THE VMX OPERAND-ORDER RULE THAT MAKES THIS READABLE (gotcha 9; a previous wave got it
//     backwards): IDA prints the VA-form as (vD, vA, vB, vC) while the arithmetic is vA*vC (+/-)
//     vB. So `vnmsubfp v7,v0,v11,v5` == v11 - v0*v5 == 1 - a*x^2 and `vmaddfp v9,v3,v9,v7` ==
//     v3*v7 + v9 == the NR step x + 0.5x(1 - a x^2). Read the other way the block is nonsense.
//     The 1.0f / 0.5f are `vcfsx v0,0` / `vcfsx v0,1` over `vspltisw v0,1` -- MATERIALISED, not
//     loaded from rodata.
//     HOST SPELLING: rw::math::vpu::Magnitude (an exact std::sqrt of MagnitudeSquared), which the
//     DWARF names TWICE in this scope -- so this is a de-optimisation, not a rewrite. The console's
//     vsel guard exists because rsqrte(0) is +inf and 0*inf is NaN; std::sqrt(0) is 0, so the host
//     needs no guard and none is added.
//   0x82611E50  vmaddfp v0, v13, v0, v5   == v13*v5 + v0 == |angular| * radius + |linear|
//   0x82611E54  vmulfp128 v0, v0, v127    *= lvfTimeStep
//   0x82611E58  vminfp v0, v0, v12        Min(that, unk_82FB94F0) == KVF_MAX_PROP_PADDING
//   0x82611E64  lfs f31, <that slot>      the f32 lane-0 read the AddPrimitive calls pass in f1.
//
//   ---- THE VOLUME LOOP (0x82611E80..0x82611F58) ------------------------------------------------
//   0x82611E84  lwz 0x24(partType) + vol*96   lpPartType->GetCollisionVolume(lu8Vol). 96 ==
//               sizeof(rw::collision::Volume), already static_asserted in BrnPhysicsPropTypeData.h,
//               so the typed index is exact and 96 does NOT widen.
//   0x82611E88..0x82611F40  a full Matrix44Affine product, broadcast+FMA:
//               out.row_i = vol.row_i.x*ev.row0 + vol.row_i.y*ev.row1 + vol.row_i.z*ev.row2
//                           (+ ev.row3 for the position row)
//               == `lpPartVolume->GetRelativeTransform() * lUpdatePartEvent.mTransform`. The
//               volume's four rows come from volume+0x00..+0x30, which IS its embedded transform
//               (volume_debug_access.h already returns it). The DWARF confirms
//               `rw::math::vpu::operator*` here.
//   0x82611E74  clrlwi r29,r29,16         the primitive tag is (u16)liPartIndex.
//   0x82611F44  bl sub_82814AB8           AddPrimitive(volume, transform, padding, tag)
//   0x82611F4C  lbz 0x2C(partType)        THE LOOP BOUND IS RE-READ EVERY ITERATION, and the
//                                         counter is an 8-bit `clrlwi ,24` compared UNSIGNED
//                                         (`cmplw`) -- the DWARF's `uint8_t lu8Vol @2502`.
//
//   ---- THE TRIANGLE LIST (0x82611F5C..0x82612058) ----------------------------------------------
//   0x82611F5C  assert "mpTriangleCacheManager != NULL" (CgsSceneManagerModuleIO.h:1286, 0x506) --
//               the INLINED body of TriangleCacheInterface::GetCache. The DWARF spells that method
//               GetCachedTriangles; the tree committed the same @0x82277810 method as GetCache --
//               same method, no fork. That is why three baked tripwires appear here with no `bl`.
//   0x82611F90..0x82611FE4  slot*48 into the manager's per-slot table (+0x04), `lwz 0x24` == the
//               batch base index, `mulli 0xE0` == * sizeof(Triangle4) off mpaTriangleCache (+0x00),
//               with the manager's own "mpaTriangleCache != NULL" (:153) tripwire between --
//               i.e. exactly the committed TriangleCacheManager::GetTrianglesForCachedObject.
//   0x82611FE8  assert :1295 (0x50F)      the second inlined accessor ==
//               GetNumCachedTriangleBatches, whose count is the per-slot `lwz 0x28`.
//   0x8261201C  two stw + bl CheckAlignment == TriangleList::SetTriangleBuffer INLINED. That method
//               (DWARF CgsTriangleList.h:21) does not exist in the tree, which is why the two
//               member stores are open-coded -- exactly as the committed sibling
//               BrnVehicleManagerContactGeneration.cpp:591-593 already does. HEADER REQUEST C.
//   0x82612028..0x82612058  the Triangle4::AssertIsValid walk at a 0xE0 stride with the count
//               re-read each iteration == TriangleList::ValidateTriangles (committed).
//
//   ---- THE POST (0x8261205C..0x826120CC) -------------------------------------------------------
//   0x82612084  slwi r11,(miNumPropsAddedToContactGen + 0x192F),2 ; stwx r9,r11,r19
//               0x192F*4 == 0x64BC == offsetof(maPropsAddedToContactGen), so this is the array
//               write followed by the ++. NO BOUNDS CHECK in the console (the array is 45 long);
//               none is added.
//   0x82612078  lbz byte_82F2A39C / bne   the stream-vs-synchronous selector.
//   0x826120A8  synchronous arm  CollidePrimitiveListAgainstTriangleList(prims, tris,
//                                  r6=0x40 max, r7=0 tagA, r8=0 tagB, r9=1 useOptimisedBoxTests)
//   0x826120C0  stream arm       AddPrimitiveListWithTriangleListToStream(prims, tris, 0x40,
//                                  r7=1 useOptimisedBoxTests, r8=0 tagA, r9=0 tagB,
//                                  r10 = mpPrimitiveWithTriangleStream (lwz 0xA0))
//     ⚠️ THE TWO CALLEES TAKE THE BOOL AND THE TAGS IN A DIFFERENT ORDER. Both orders are the
//     DWARF's own and both are register-confirmed. Do NOT "tidy" them to match.
//   0x826120C4  ++liNumJobsAdded through the reference.
//
// Every console offset above appears in a COMMENT only; every member is reached by name.
// =================================================================================================
void PropManager::DoPartWorldContactGeneration(
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpContactGenerator,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCache,
    const UpdatePropEvent&                                         lUpdatePartEvent,
    s32&                                                           liNumJobsAdded,
    CgsMemory::LinearMalloc*                                       lpMalloc,
    VecFloat                                                       lvfTimeStep )
{
    typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
    typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

    // BrnPropManager.cpp:2449 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT( lpContactGenerator != NULL, "lpContactGenerator != NULL" );

    // lhz +0x64 / extsh: the event's slot member is s16, SIGN-extended into an s32 local.
    const s32 liPartIndex = lUpdatePartEvent.miPhysicsSlot;

    CGS_ASSERT( liPartIndex >= 0, "liPartIndex >= 0" );                                   // :2466
    CGS_ASSERT( liPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ),
                "liPartIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROP_PARTS )" );  // :2467

    if ( HasPartJustBeenRemoved( lUpdatePartEvent.mEntityId, liPartIndex ) )
    {
        return;                                             // 0x82611C3C bne -> the epilogue
    }

    // :2478. The console streams this into the SHARED global sink CgsDev::Assert::gpcMessageBuffer;
    // that global is not declared anywhere in this tree, so the message is built on a stack buffer
    // of the same KI_MESSAGEBUFFERSIZE -- the committed idiom (CgsID.cpp:73-84,
    // PropManager_wQ2_02.cpp:317). Message content, stream order and the fired line are unchanged.
    if ( liPartIndex < 0 || liPartIndex >= static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ) )
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Updated part was removed: Part ID = ";
        lStrStream << lUpdatePartEvent.mEntityId.GetValue();   // carries the owner tripwire :278
        lStrStream << ", Returned part index = ";
        lStrStream << liPartIndex;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    PropPartInstance* lpPart = &mpaPartInstances[liPartIndex];          // console stride 64
    const s32 liCacheSlotIndex = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;

    const u32 luTypeId = lpPart->GetType();                             // +0x34
    const u32 luPartId = lpPart->GetPartId();                           // +0x38

    const PropTypeData*     lpPropType = mpPhysicsData->GetType( luTypeId );
    const PropPartTypeData* lpPartType = &lpPropType->GetParts()[luPartId];   // console stride 48

    PrimitivePairListBuilder lPrimPairList;
    lPrimPairList.Prepare( lpMalloc, lpPartType->GetNumberOfVolumes() );      // lbz 0x2C

    // |linear| + |angular| * boundingRadius, scaled by the timestep and capped. See the banner for
    // the vmaddfp/vnmsubfp operand-order rule that makes this the right reading of the console's
    // rsqrt+2NR+vsel block.
    VecFloat lvfPadding = vpu::Splat( vpu::Magnitude( lpPart->GetLinearVelocity() ) );
    lvfPadding = lvfPadding
               + vpu::Splat( vpu::Magnitude( lpPart->GetAngularVelocity() ) )
                     * vpu::Splat( lpPartType->GetBoundingRadius() );         // lfs 0x28
    lvfPadding = lvfPadding * lvfTimeStep;
    lvfPadding = vpu::Min( lvfPadding, KVF_MAX_PROP_PADDING );                // vminfp

    // The bound is RE-READ every iteration (lbz 0x2C at both 0x82611DA0 and 0x82611F4C) and the
    // counter is an 8-bit value compared UNSIGNED -- the DWARF's `uint8_t lu8Vol`.
    for ( u8 lu8Vol = 0; lu8Vol < lpPartType->GetNumberOfVolumes(); ++lu8Vol )
    {
        ::rw::collision::Volume* lpPartVolume = lpPartType->GetCollisionVolume( lu8Vol );

        const Matrix44Affine lPartTransform =
            lpPartVolume->GetRelativeTransform() * lUpdatePartEvent.mTransform;

        lPrimPairList.AddPrimitive( lpPartVolume, lPartTransform,
                                    vpu::GetComponent( lvfPadding, 0 ),   // VecFloat -> f32, in f1
                                    static_cast<u16>( liPartIndex ) );    // clrlwi r29,r29,16
    }

    // The DWARF spells the first accessor GetCachedTriangles; the tree committed the same
    // @0x82277810 method as GetCache. Same method, no fork.
    const CgsGeometric::Triangle4* lp4Tris = lpTriCache->GetCache( liCacheSlotIndex );
    const s32 liNum4Tris = lpTriCache->GetNumCachedTriangleBatches( liCacheSlotIndex );

    // HEADER REQUEST C: the console called TriangleList::SetTriangleBuffer(lp4Tris, liNum4Tris) and
    // the compiler inlined it to these two member stores plus its out-of-line CheckAlignment().
    TriangleList lListOfCachedTris;
    lListOfCachedTris.mpTriangles    = const_cast<CgsGeometric::Triangle4*>( lp4Tris );
    lListOfCachedTris.miNumTriangles = liNum4Tris;
    lListOfCachedTris.CheckAlignment();
    lListOfCachedTris.ValidateTriangles();

    // 0x82612084: maPropsAddedToContactGen[miNumPropsAddedToContactGen] = the event's id.
    // The console does NOT bounds-check the 45-entry array; none is added.
    maPropsAddedToContactGen[miNumPropsAddedToContactGen] = lUpdatePartEvent.mEntityId;
    ++miNumPropsAddedToContactGen;

    if ( !KB_USE_CONTACT_GEN_STREAM )
    {
        // 0x826120A8 -- r7 = 0 tag A, r8 = 0 tag B, r9 = 1 the bool. Return value dropped.
        lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, 0u, 0u, true );
    }
    else
    {
        // 0x826120C0 -- NOTE the different order: r7 = 1 the bool, r8 = 0 tag A, r9 = 0 tag B,
        // r10 = mpPrimitiveWithTriangleStream (lwz 0xA0). Return value dropped.
        lpContactGenerator->AddPrimitiveListWithTriangleListToStream(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, true, 0u, 0u,
            mpPrimitiveWithTriangleStream );
    }

    ++liNumJobsAdded;                                       // 0x826120C4 through the reference
}

// =================================================================================================
// BrnPhysics::Props::PropManager::DoPropInstanceWorldContactGeneration @0x826120E8  (342 insns)
//
// ⭐ LANDED 2026-08-19 (wave Q6). The other leg -- the arm taken when the event's PropEntityID
// carries a ZERO part index, i.e. a WHOLE prop.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x826120E8.json,
// re-read this wave (scratchpad/waveQ6/asm_826120E8.txt); the Hex-Rays pseudocode was NOT
// consulted. 342 == (0x8261263C - 0x826120E8)/4 + 1, and the listing has exactly 342 lines.
//
// ---- IT IS THE SAME FUNCTION AS ITS DoPart TWIN, WITH THE PROP ARM SUBSTITUTED -----------------
// MEASURED, not assumed: strip the addresses from both export listings and diff them -- the two
// bodies are instruction-for-instruction the same shape, and every difference is one of the eleven
// below (plus register allocation and branch targets). That is why the two bodies read as
// near-copies here: the console's two bodies are too.
//
//   #  DoPart @0x82611B70                        DoPropInstance @0x826120E8
//   1  lhz 0x64(event) / extsh -> liPartIndex    same, -> liPropIndex
//   2  asserts :2449 :2466 :2467 :2478           asserts :2574 :2588 :2589 :2600
//                                                (li r5 0x991/0x9A2/0x9A3/0x9AE vs
//                                                       0xA0E/0xA1C/0xA1D/0xA28)
//   3  bound `cmpwi 0x1E` == 30                  bound `cmpwi 0xF` == 15
//      (KU_MAX_PHYSICAL_PROP_PARTS)              (KU_MAX_PHYSICAL_PROPS)
//   4  bl HasPartJustBeenRemoved                 bl HasPropJustBeenRemoved
//   5  "Updated part was removed: Part ID = "    "Updated prop was removed: Prop ID = "
//      ", Returned part index = "                ", Returned prop index = "
//   6  lwz 0x8C(this) + `slwi 6` (stride 64)     lwz 0x7C(this) + `mulli 0x70` (stride 112)
//      == mpaPartInstances                       == mpaPropInstances
//   7  `addi r20, idx, 0x2B` == slot 43 + i      `addi r21, idx, 0x1C` == slot 28 + i
//   8  lwz 0x34 / lbz 0x38 == GetType/GetPartId, lwz 0x64 == GetTypeId ONLY -- no part id, and NO
//      then `lwz 0x40(type)` + partId*48         maParts indexing at all
//   9  volume count `lbz 0x2C(partType)`         volume count `lbz 0x5E(propType)`
//      radius       `lfs 0x28(partType)`         radius       `lfs 0x44(propType)`
//      volumes      `lwz 0x24(partType)`         volumes      `lwz 0x3C(propType)`
//  10  velocities   lvx part+0x10 / part+0x20    velocities   lvx prop+0x40 / prop+0x50
//  11  DWARF locals lPartTransform @2505         DWARF locals lpRelativeTransform @2625 (a POINTER)
//                                                + lVolumeMatrix @2626
//
// EVERYTHING ELSE IS IDENTICAL, and was re-checked line by line this wave: the null-generator
// assert, the double range check, the ResourcePtr operator-> + PropPhysicsDataHeader::GetType pair,
// PrimitivePairListBuilder::Prepare, the whole rsqrt+2NR+vsel Magnitude pair, the `vminfp` against
// unk_82FB94F0, the per-volume Matrix44Affine product and AddPrimitive, the triangle-cache fetch
// with its three inlined asserts, the Triangle4::AssertIsValid walk, the
// maPropsAddedToContactGen post, the `lbz byte_82F2A39C` selector with its two arms IN THE SAME
// ARGUMENT ORDERS, and the `++liNumJobsAdded` through the reference. See the DoPart banner above
// for the shared decode; only the deltas are annotated inline below.
//
// ---- CONSOLE ARGUMENT MAP, read off the prologue 0x82612100..0x8261211C -----------------------
//     r3 = this (r19) · r4 = lpContactGenerator (r17) · r5 = lpTriCache (r18)
//     r6 = &lUpdatePropEvent (r22) · r7 = &liNumJobsAdded (r16) · r8 = lpMalloc (r27)
//     v1 = lvfTimeStep (saved to v127) -- again, a vector arg consumes NO GPR slot, so its
//     POSITION is DWARF-attested, not register-attested.
//
// ---- ONE SOURCE-SHAPE NOTE, STATED NOT HIDDEN ------------------------------------------------
// The DecFIGS scope names `Matrix44Affine* lpRelativeTransform @2625` -- a POINTER, because the
// SDK's own Volume::GetRelativeTransform returns `Matrix44Affine*`. This tree's accessor
// (volume_debug_access.h) deliberately returns a REFERENCE, so the local is a reference here.
// Same object, same product; recorded so nobody reads the difference as a recovered semantic.
// =================================================================================================
void PropManager::DoPropInstanceWorldContactGeneration(
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpContactGenerator,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCache,
    const UpdatePropEvent&                                         lUpdatePropEvent,
    s32&                                                           liNumJobsAdded,
    CgsMemory::LinearMalloc*                                       lpMalloc,
    VecFloat                                                       lvfTimeStep )
{
    typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
    typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

    // BrnPropManager.cpp:2574 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT( lpContactGenerator != NULL, "lpContactGenerator != NULL" );

    // lhz +0x64 / extsh: the event's slot member is s16, SIGN-extended into an s32 local.
    const s32 liPropIndex = lUpdatePropEvent.miPhysicsSlot;                       // DWARF :2576

    CGS_ASSERT( liPropIndex >= 0, "liPropIndex >= 0" );                           // :2588
    CGS_ASSERT( liPropIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                "liPropIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROPS )" ); // :2589

    if ( HasPropJustBeenRemoved( lUpdatePropEvent.mEntityId, liPropIndex ) )
    {
        return;                                            // 0x826121B4 bne -> the epilogue
    }

    // :2600 -- same shared-message-buffer treatment as the DoPart twin.
    if ( liPropIndex < 0 || liPropIndex >= static_cast<s32>( KU_MAX_PHYSICAL_PROPS ) )
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Updated prop was removed: Prop ID = ";
        lStrStream << lUpdatePropEvent.mEntityId.GetValue();  // carries the owner tripwire :278
        lStrStream << ", Returned prop index = ";
        lStrStream << liPropIndex;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    // console `mulli r11, liPropIndex, 0x70` -- stride 112 is a CONSOLE size; the host indexes the
    // typed array and lets sizeof do the striding (gotcha 1).
    PropInstance* lpProp = &mpaPropInstances[liPropIndex];                        // DWARF :2577
    const s32 liCacheSlotIndex = KI_PROP_CACHE_START_INDEX + liPropIndex;

    // `lwz 0x64(lpProp)` is muTypeId, loaded before the ResourcePtr operator-> call clobbers r3;
    // that is emission order, not a second statement.
    const PropTypeData* lpPropType = mpPhysicsData->GetType( lpProp->GetTypeId() );

    PrimitivePairListBuilder lPrimPairList;                                       // DWARF :2613
    lPrimPairList.Prepare( lpMalloc, lpPropType->GetNumberOfVolumes() );          // lbz 0x5E

    VecFloat lvfPadding = vpu::Splat( vpu::Magnitude( lpProp->GetLinearVelocity() ) );
    lvfPadding = lvfPadding
               + vpu::Splat( vpu::Magnitude( lpProp->GetAngularVelocity() ) )
                     * vpu::Splat( lpPropType->GetBoundingRadius() );             // lfs 0x44
    lvfPadding = lvfPadding * lvfTimeStep;
    lvfPadding = vpu::Min( lvfPadding, KVF_MAX_PROP_PADDING );                    // vminfp

    // The bound is RE-READ every iteration (lbz 0x5E at both 0x82612328 and 0x826124A8).
    for ( u8 lu8Vol = 0; lu8Vol < lpPropType->GetNumberOfVolumes(); ++lu8Vol )
    {
        // console `lwz 0x3C(propType)` + `vol*96`; 96 == sizeof(rw::collision::Volume), already
        // static_assert'd in BrnPhysicsPropTypeData.h, so the typed index is exact.
        ::rw::collision::Volume* lpVolume = lpPropType->GetCollisionVolume( lu8Vol ); // DWARF :2624

        const Matrix44Affine& lrRelativeTransform = lpVolume->GetRelativeTransform();

        const Matrix44Affine lVolumeMatrix =
            lrRelativeTransform * lUpdatePropEvent.mTransform;                    // DWARF :2626

        lPrimPairList.AddPrimitive( lpVolume, lVolumeMatrix,
                                    vpu::GetComponent( lvfPadding, 0 ),  // VecFloat -> f32, in f1
                                    static_cast<u16>( liPropIndex ) );   // clrlwi r26,r26,16
    }

    const CgsGeometric::Triangle4* lp4Tris = lpTriCache->GetCache( liCacheSlotIndex );  // DWARF :2579
    const s32 liNum4Tris = lpTriCache->GetNumCachedTriangleBatches( liCacheSlotIndex ); // DWARF :2578

    // HEADER REQUEST C -- the inlined TriangleList::SetTriangleBuffer, open-coded.
    TriangleList lListOfCachedTris;                                               // DWARF :2580
    lListOfCachedTris.mpTriangles    = const_cast<CgsGeometric::Triangle4*>( lp4Tris );
    lListOfCachedTris.miNumTriangles = liNum4Tris;
    lListOfCachedTris.CheckAlignment();
    lListOfCachedTris.ValidateTriangles();

    // 0x826125E0: maPropsAddedToContactGen[miNumPropsAddedToContactGen] = the event's id.
    // The console does NOT bounds-check the 45-entry array; none is added.
    maPropsAddedToContactGen[miNumPropsAddedToContactGen] = lUpdatePropEvent.mEntityId;
    ++miNumPropsAddedToContactGen;

    if ( !KB_USE_CONTACT_GEN_STREAM )
    {
        // 0x82612604 -- r7 = 0 tag A, r8 = 0 tag B, r9 = 1 the bool. Return value dropped.
        lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, 0u, 0u, true );
    }
    else
    {
        // 0x8261261C -- NOTE the different order: r7 = 1 the bool, r8 = 0 tag A, r9 = 0 tag B,
        // r10 = mpPrimitiveWithTriangleStream (lwz 0xA0). Return value dropped.
        lpContactGenerator->AddPrimitiveListWithTriangleListToStream(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, true, 0u, 0u,
            mpPrimitiveWithTriangleStream );
    }

    ++liNumJobsAdded;                                       // 0x82612620 through the reference
}

}
}
