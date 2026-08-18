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
//   * PropManager::DoPartWorldContactGeneration         @0x82611B70 (349 insns) -- ⛔ NOT LANDED
//   * PropManager::DoPropInstanceWorldContactGeneration @0x826120E8 (342 insns) -- ⛔ NOT LANDED
//   * PropManager::AddContactResultsToQueue             @0x82612F08 (184 insns) -- ⭐ LANDED BELOW
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
// ⛔ WHY THE TWO Do* LEGS ARE NOT HERE -- TWO MISSING DECLARATIONS, COMPILE-PROVED
// -------------------------------------------------------------------------------------------------
// Both bodies are COMPLETE, re-derived this round from the raw `assembly` arrays of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82611B70.json and /0x826120E8.json (the Hex-Rays
// pseudocode in those exports was NOT consulted -- its `a5, a6` prototype drops the vector
// parameter entirely), and they are parked at
//
//     scratchpad/waveQ2/parked/PropManager_03_DoPartWorldContactGeneration.cpp
//     scratchpad/waveQ2/parked/PropManager_03_DoPropInstanceWorldContactGeneration.cpp
//
// `selfcheck.py scratchpad/waveQ2/probe_wQ2_03b/probe_bodies.cpp` (BOTH bodies verbatim, in one
// TU) returns STATUS=fail with EXACTLY TWO DISTINCT diagnostics, one per missing declaration and
// each reported once per body -- nothing else is missing:
//
//   error C2660: "CgsSceneManager::CgsCollision::PrimitivePairListBuilder::AddPrimitive":
//                function does not take 4 arguments
//                  ... "(rw::collision::Volume *, const Matrix44Affine, float, u16)"
//   error C2039: "CollidePrimitiveListAgainstTriangleList" is not a member of
//                "CgsSceneManager::CgsCollision::CollisionGenerator"
//
// ---- HEADER REQUEST A ---------------------------------------------------------------------------
//   FILE: b5-decomp/src/GameShared/GameClasses/SceneManager/Collision/Primitives/
//         CgsPrimitivePairListBuilder.h -- inside `struct PrimitivePairListBuilder`, public
//         section, ABOVE the committed AddPrimitive(Sphere*, f32, u16) at :52:
//
//     // AddPrimitive(Volume*, Matrix44Affine, f32, u16) @0x82814AB8 (140 insns): switch on the
//     // rwcollision volume's type id, build the matching CgsGeometric primitive from the volume
//     // payload + the caller's world transform, and append it.
//     void AddPrimitive(const ::rw::collision::Volume* lpVolume, Matrix44Affine lTransform,
//                       f32 lfPadding, u16 lu16PrimitiveTag);
//
//   MEASURED (headless IDA 9.3 on IDA Files/BURNOUT_X360_ARTIST.XEX.i64, this round -- IDA leaves
//   the symbol as `sub_82814AB8`, the function is 0x82814AB8..0x82814CE8):
//     r3 = the builder (`mr r31,r3`); r4 = the volume (`lwz r10,0x40(r11)` == the rwcollision
//     per-type descriptor, `lwz r10,0(r10)` == its typeID, `addi -1`, 5-case jump table at
//     jpt_82814B08); r5 = the transform, read as four 16-byte rows at +0x00/+0x10/+0x20/+0x30;
//     f1 = the padding (`fmr f31,f1`); r7 = the tag (`mr r30,r7`, forwarded as r6 to every leaf).
//     ⚠️ GOTCHA 3 IS WHY r6 IS SKIPPED: the f32 rides f1 and consumes its GPR slot, so the u16
//     tag lands in r7. r6 is never read. A signature derived from GPRs alone would invent a dead
//     fifth argument.
//   Its only two callers in the whole image are this pair (0x82611F44 / 0x826124A0) -- measured
//   xrefs, plus the .pdata entry at 0x821D87C0.
//
// ---- HEADER REQUEST B ---------------------------------------------------------------------------
//   FILE: b5-decomp/src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
//         CgsCollisionGenerator.h -- inside `struct BaseCollisionGenerator`, public section:
//
//     // @0x828141D8 (86 insns). The SYNCHRONOUS twin of
//     // AddPrimitiveListWithTriangleListToStream. Returns the result-list index.
//     // ⚠️ RETURN TYPE, for whoever lands this (round-2 NIT): the DWARF says uint16_t, but the
//     // console returns the index UNTRUNCATED -- 0x8281428C `mr r29,r3`, 0x82814298 `clrlwi r10,r29,16`
//     // for the LOCAL use only, 0x82814324 `mr r3,r29` with no clrlwi on the return path. That is
//     // exactly the register truth for which this tree deliberately types the two committed
//     // Add*...ToStream siblings `s32` (see CgsCollisionGenerator.h's own banner on
//     // AddPrimitiveListWithTriangleListToStream). Land it `s32` for family consistency, or land the
//     // DWARF's u16 and carry that same one-line note -- but do not leave two undocumented
//     // spellings of one value in one class. Every call site drops the result, so neither breaks.
//     u16 CollidePrimitiveListAgainstTriangleList(const PrimitivePairList* lpPrimitiveList,
//                                                 const TriangleList* lpTriangleList,
//                                                 u16 lu16MaxResults,
//                                                 u32 luUserTagA, u16 lu16UserTagB,
//                                                 bool lbUseOptimisedBoxTests);
//
//   The export for 0x828141D8 EXISTS (IDA truncates its symbol to "...CollidePrimitiveListAgainst
//   Triang"); its six parameters are ordinary GPRs, so no slot is skipped and r4..r9 confirm the
//   order register-for-register. Seven measured call sites, six of them already reconstructed
//   functions: DeformableObject::DoBodyPartWorldContactGeneration (x2),
//   ::DoDetachedWheelWorldContactGeneration, VehicleManager::DoTrafficCarWorldContactGeneration
//   (x2), and this pair.
//   ⚠️ IT CANNOT BE DROPPED as "the dead arm": the selector is a RUNTIME byte read
//   (`lbz byte_82F2A39C`), not a compile-time constant -- the console emits BOTH arms. See the
//   ⭐⭐ constants block in the parked files.
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
// ---- HEADER REQUEST D (home for two recovered constants) -- HALF LANDED 2026-08-18 round 3b -----
//   The parked bodies carry two file-local AUTHORED-NAME constants over MEASURED values whose real
//   home is the KVF_* block in BrnPropManager.h/.cpp, which this implementer does not own:
//     KVF_MAX_CONTACT_GEN_PADDING == Splat(0.3f)   and   KB_USE_CONTACT_GEN_STREAM == true.
//   ⭐ THE FIRST ONE IS NOW HOMED: `extern const VecFloat KVF_MAX_CONTACT_GEN_PADDING` in
//   BrnPropManager.h, defined `= { 0.3f, 0.3f, 0.3f, 0.3f }` in BrnPropManager.cpp with this file's
//   own thunk/rodata provenance carried across. When the parked bodies land they should reach for
//   the header's constant and drop their file-local copy.
//   ⚠️ THE SECOND ONE IS NOT LANDED and is still an open request: KB_USE_CONTACT_GEN_STREAM has no
//   address and no thunk cited anywhere in this banner or in the parks, so there is nothing for the
//   header owner to re-measure and nothing was invented for it.
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
// LINK-LEVEL FACTS (gate-green != link-green)
// -------------------------------------------------------------------------------------------------
//   * NO gate and NO stub exists for any of these three names. Grepped
//     GameSource/Physics/BrnPhysicsConductorGates.cpp, GameSource/World/WorldLinkStubs.cpp and the
//     whole of b5-decomp/src: the only pre-existing hits are the header declarations and comments
//     in PropManager_wQ2_02.cpp. This file is the sole definition of AddContactResultsToQueue.
//   * Callees of the body below that are DECLARED WITH A BODY IN THE TREE (checked one by one):
//     BaseCollisionGenerator::GetNumUsedResultLists (header inline, CgsCollisionGenerator.h:114),
//     ::GetResultList (CgsCollisionGenerator.cpp:130), PropEntityID::GetValue
//     (BrnPropEntityID.cpp:50), PotentialContactInterface::AddEvent(const PotentialContact&)
//     (BrnPhysicsModuleIO_PotentialContactInterface.cpp:46). NONE of this file's callees is
//     body-less -- see the report for the parked files' own lists.
//   * ⚠️ INHERITED, still open, not this file's to fix: PropManager_wQ_03.cpp's UpdateTriangleCache
//     and PropManager_wQ2_02.cpp's Begin/End still have one-shot gate bodies at
//     BrnPhysicsConductorGates.cpp:485/:492/:514. They collide at LINK time the moment those
//     partfiles are added to tools/build/build_game_exe.bat. Mount + retire in one commit.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                                  // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CollisionGenerator
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"      // CollisionResultList / PrimitiveTestResult
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"                 // PotentialContact
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"                  // PotentialContactInterface::AddEvent
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                                      // PropEntityID::GetValue (owner tripwire)

namespace BrnPhysics
{
namespace Props
{

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

}
}
