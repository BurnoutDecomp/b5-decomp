// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_02.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, lander 02, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// THREE FUNCTIONS, all of them the prop-vs-world triangle-collision leg:
//
//   * PropManager::GetTriangleCacheSlotAndRadius  @0x826115C0 (247 insns) -- FRESH this round.
//         Called by the already-landed PropManager_wQ_03.cpp::UpdateTriangleCache, which was
//         gate-green but not link-green without it.
//   * PropManager::BeginPropWorldContactGeneration @0x82628CB0 (89 insns) -- round-1 body,
//         parked at scratchpad/waveQ/parked/PropManager_03_BeginPropWorldContactGeneration.cpp
//         on two missing BaseCollisionGenerator declarations. THEY LANDED (round-2 collgen
//         owner): CgsCollisionGenerator.h:240 / :241-242. Body brought in-tree here.
//   * PropManager::EndPropWorldContactGeneration   @0x82628E18 (37 insns) -- round-1 body,
//         parked on BaseCollisionGenerator::GetNumUsedResultLists(). THAT LANDED TOO
//         (CgsCollisionGenerator.h:114, a public inline). Body brought in-tree here.
//
// ⚠️ ODR NOTE FOR THE CONDUCTOR -- Begin and End STILL HAVE their one-shot conductor-gate
//    bodies in b5-decomp/src/GameSource/Physics/BrnPhysicsConductorGates.cpp:
//        :485  BeginPropWorldContactGeneration          :492  EndPropWorldContactGeneration
//    Re-confirmed by grep this round; both gate bodies are still present and are DELIBERATELY
//    LEFT IN PLACE. AGENTS.md's convention is that a gate is retired only in the same commit
//    that MOUNTS the real body in tools/build/build_game_exe.bat, and that script lists
//    BrnPhysicsConductorGates.cpp but lists NEITHER PropManager_wQ_03.cpp NOR this file. So
//    there is no LNK2005 today; it fires the instant either partfile joins the source list.
//    `cl /c` cannot see this -- reported, not "fixed". (The same commit must also delete the
//    UpdateTriangleCache gate at :514, which PropManager_wQ_03.cpp already reported.)
//    GetTriangleCacheSlotAndRadius has NO gate and no stub anywhere (grepped
//    BrnPhysicsConductorGates.cpp and WorldLinkStubs.cpp) -- this is its only definition.
//
// GROUNDING. Every body below was re-derived this round from the RAW `assembly` array of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json (Hex-Rays pseudocode NOT consulted; its
// prototype for 0x826115C0 is the usual variadic-arg mislabel). Instruction counts stated
// anywhere in this file are counts of the lines in that export listing, which I counted:
// 247 / 89 / 37 (prologue + body + epilogue; the round-1 End banner's "25" was a body-only
// count -- a different measure, not a contradiction).
// Rodata was read out of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3 this
// round (scratchpad/waveQ2/probe_wq2lander/ida/dump.txt) rather than trusted from a banner:
//     flt_82001CC0 = 00 00 00 00 = 0.0f      flt_820138DC = 42 48 00 00 = 50.0f
//     flt_82004014 = 3D CC CC CD = 0.1f
// and every truncated assert string was read in full from the same probe.
//
// ⚠️ NO CONSOLE OFFSET IS USED AS A HOST OFFSET anywhere below (AGENTS.md gotcha 1). Console
// immediates appear only in comments; every member/array is reached by name, every record is
// indexed by its host type. The three places this actually bites are called out inline:
// PropInstance stride 112 vs PropPartInstance stride 64 (console) and PropPartTypeData
// stride 48 console / 64 host -- the last one is why the part-type lookup indexes
// GetParts()[luPartId] instead of doing the console's `base + luPartId*48`.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"       // PropInstance::GetTypeId
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"   // PropPartInstance::GetType / GetPartId
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"    // KU_MAX_PHYSICAL_PROPS / _PROP_PARTS
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                      // PropEntityID
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"             // PropPhysicsDataHeader::GetType
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"               // PropTypeData / PropPartTypeData
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT + the Begin/Fire/End trio
#include "GameShared/GameClasses/Development/CgsStrStream.h"                  // CgsDev::StrStream (the :2259 message)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h" // SimpleDataStreamProducer::Begin/End
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                    // CgsMemory::LinearMalloc (parameter)

namespace BrnPhysics
{
namespace Props
{

// -------------------------------------------------------------------------------------------------
// The triangle-cache slot map. Both constants are DWARF-attested WITH THEIR VALUES in
// references/DecFIGS/dwarfdump/GameSource/Physics/BrnTriangleCacheConstants.h:
//     BrnTriangleCacheConstants.h:36   const int32_t KI_PROP_CACHE_START_INDEX      = 28;
//     BrnTriangleCacheConstants.h:37   const int32_t KI_PROP_PART_CACHE_START_INDEX = 43;
// and both are X360-attested here: `addi r11, r30, 0x1C` (0x826118B0, the prop arm) and
// `addi r8, r30, 0x2B` (0x826117D8, the part arm). 0x1C == 28, 0x2B == 43.
//
// ⚠️ b5-decomp/src/GameSource/Physics/BrnTriangleCacheConstants.h -- their DWARF home -- DOES
// NOT EXIST in the tree yet (checked: GameSource/Physics/ has no such file), and this lander is
// not allowed to create a header. So they are declared file-local here, in exactly the same
// MOVE-WHEN-IT-LANDS form the sibling prop constants already use (BrnPropInputInterface.h:52/:61
// carry that same caveat for KU_MAX_PHYSICAL_PROPS / KU_MAX_PHYSICAL_PROP_PARTS, and
// PropManager_wQ_03.cpp does it for its file-local KF_TRIANGLE_CACHE_PADDING). When BrnTriangleCacheConstants.h
// lands, delete these two and include it -- the same DWARF file also names the race-car / traffic /
// detached-part / detached-wheel starts, so the whole map wants to collapse onto it at once.
// ⚠️ FOLD-TIME REDEFINITION HAZARD (round-2 NIT, applied): two OTHER round-2 part-files of this
// SAME TU declare the same file-local names at the same scope --
// PropManager_wQ2_04.cpp (both, in its triangle-cache-constants banner) and
// PropManager_wQ2_06.cpp (the prop one, above ReadUpdatedBodies' cache-slot arm). Internal
// linkage keeps that harmless while they are separate translation units, but every one of these
// banners says it "folds back into BrnPropManager.cpp": the moment two of them fold, that is a
// C2374 redefinition, invisible to `cl /c` exactly like the gate ODR noted above. Whichever
// part-file folds second must drop its copy, or all three must switch to
// BrnTriangleCacheConstants.h in the same commit.
static const s32 KI_PROP_CACHE_START_INDEX      = 28;   // DWARF BrnTriangleCacheConstants.h:36
static const s32 KI_PROP_PART_CACHE_START_INDEX = 43;   // DWARF BrnTriangleCacheConstants.h:37

// The upper sanity bound the :2259 radius tripwire compares against.
// ⚠️ AUTHORED NAME -- the console has only the rodata float, and the DWARF's
// BrnTriangleCacheConstants.h:44 KF_DEFAULT_PROP_CACHE_SPHERE_SIZE prints NO value, so this is
// NOT that constant being recovered and the two must not be conflated. What is MEASURED is the
// value: flt_820138DC reads 42 48 00 00 big-endian == 50.0f (headless IDA 9.3 on the .i64, this
// round). Everything else about the name is this file's invention and is flagged as such.
static const f32 KF_MAX_PROP_CACHE_SPHERE_RADIUS = 50.0f;   // flt_820138DC, measured

// =================================================================================================
// BrnPhysics::Props::PropManager::GetTriangleCacheSlotAndRadius @ 0x826115C0  (247 asm insns)
//
// Resolve one live prop-or-part handle to (a) its fixed triangle-cache slot and (b) the radius of
// the collision sphere the scene should cache triangles inside. Returns false -- leaving both
// out-params untouched -- when the entity was removed earlier this frame. Its one caller,
// UpdateTriangleCache @0x826119A0, zeroes both out-params before every call and tests the bool
// with a `beq` early-out (0x82611A68; the `clrlwi r11,r3,24` / `cmplwi cr6,r11,0` pair that feeds
// it is 0x82611A60 / 0x82611A64).
//
// ---- REGISTER MAP, read off the prologue (0x826115CC..0x826115E8) -------------------------------
//     r3 = this (r28) - r4 = lPropEntityId (r31) - r5 = liPhysicalPropIndex (r30)
//     r6 = &liOutCacheSlotIndex (r24) - r7 = &lfOutCacheSphereRadius (r25)
// No float or vector parameter is involved, so gotcha 3 (an f32 riding f1 and SKIPPING its GPR
// slot) does not apply here -- all five slots are ordinary GPRs and the DWARF class declaration
// (dwarfdump BrnPropManager.h:199, dumpfile line 232) agrees register-for-register. ⚠️ Round-2 fix:
// this used to cite ":232", which is the DUMPFILE line -- everywhere else in this file ".h:NNN"
// means a SOURCE line, and this function's own declaration comment in BrnPropManager.h already
// cites :199 for it.
//
// ---- THE BRANCH, and the one honest divergence --------------------------------------------------
//   0x826115D4  clrlwi r11, r31, 22          == lPropEntityId & 0x3FF == the part index
//   0x826115DC..0x826115EC  subfic/subfe/clrlwi 31   the standard (x != 0) boolean idiom
//   0x826115F4/0x82611618   cmplwi / beq -> loc_826117FC (the PROP arm)
// ⚠️ DIVERGENCE, STATED NOT HIDDEN: the console emits that mask INLINE, with NO `bl` between
// 0x826115D0 and 0x826115F4 -- i.e. no owner tripwire at all. The tree's only accessor for that
// field is the OUT-OF-LINE PropEntityID::GetPartIndex() (BrnPropEntityID.cpp:43), whose first
// statement is AssertIsProp(). Using it therefore adds one call plus one owner tripwire per live
// prop per frame that the shipped image does not have. It is VALUE-IDENTICAL (both are
// muValue & KU_PART_INDEX_MASK) and it is the existing tree precedent
// (BrnPropEntityModule_Render.cpp:762), so it is used rather than hand-inlining a mask over the
// member -- but the divergence is recorded here so a later sweep does not mistake the extra
// assert for a recovered console tripwire. The DWARF declares the accessor the original source
// used: `bool IsPart() const` (dwarfdump SharedClasses/Physics/Props/BrnPropEntityID.h:74, dumpfile
// line 27 -- the SOURCE line is 74; GetPartIndex is :71), and
// a baked assert string confirms IsPart() == (partIndex != 0): PropZoneManager::GetProp bakes
// "!lEntityId.IsPart()" against BrnPropZoneManager.h:534 (`li r5, 0x216`), and the tree already
// reproduces it as `GetPartIndex() == 0` at BrnPropZoneManager.cpp:423. (⚠️ The round-1 NIT
// cited that as "BrnPropZoneManager.cpp:405"; re-checked this round -- :405 is a comment line,
// the assert is at :423 and the baked file is the .h, not the .cpp.)
// When PropEntityID grows that non-asserting accessor, switch both
// this dispatch and BeginPropWorldContactGeneration's below to it. Filed as a header request.
//
// ---- THE PART ARM (partIndex != 0), r30 == liPartIndex ------------------------------------------
//   0x8261161C..0x82611640  `cmpwi r30,-1 / bne` -> assert :2202, then `b` STRAIGHT INTO the
//        second assert. That fall-through is the compiler's, not a second source statement: -1
//        also fails `>= 0`, so both messages fire for the same value. Two source asserts.
//   0x82611644..0x8261166C  `cmpwi 0 / blt` + `cmpwi 0x1E / blt` -> assert :2205. 0x1E == 30 ==
//        KU_MAX_PHYSICAL_PROP_PARTS, and the assert STRING names that constant verbatim
//        ("liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )",
//        read in full from the image this round).
//   0x8261167C  bl HasPartJustBeenRemoved(this, lPropEntityId, liPartIndex)
//   0x82611680..0x82611694  clrlwi 24 / beq -> `li r3,0` + epilogue. THE ONLY early return.
//   0x82611698..0x8261172C  the inlined CgsContainers::BitArray<30>::IsBitSet bounds assert
//        (its own file: CgsBitArray.h:203, message "invalid index : " << i << " < " << 30).
//        ⚠️ NOT REPRODUCED, deliberately: that tripwire is IsBitSet's body, not this function's
//        source, and the tree's BitArray header states in its own banner (CgsBitArray.h:15-17)
//        that it deliberately carries no assert-system dependency. Its bound (30) is identical
//        to the :2205 assert emitted just above it, so nothing is lost. Named here so the
//        gap between 0x82611698 and 0x82611730 is not read as a missing branch.
//   0x82611730..0x82611750  the bit math itself: `srwi r11,r30,6` (index/64), `addi r11,0x12`
//        (+18 dwords == this+0x90 == mUsedParts), `ldx` a 64-bit field, `sld 1,(index&63)`.
//        The 18 is a CONSOLE dword offset and is NOT reproduced -- mUsedParts is reached by name.
//   0x82611764..0x82611784  assert :2212 "mUsedParts.IsBitSet( liPartIndex )".
//   0x82611788..0x82611790  `lwz 0x8C` (mpaPartInstances) + `slwi r30,6` -> CONSOLE stride 64.
//        Reproduced as mpaPartInstances[liPartIndex]; the host sizeof does the striding.
//   0x82611794..0x826117B4  assert :2217 "lpPart != NULL".
//   0x826117B8..0x826117CC  `addi r3,r28,0x54` -> ResourcePtr<PropPhysicsDataHeader>::operator->
//        (0x822868E0; IDA truncates the symbol to "BrnPhysics::Props::Prop" -- I dumped the body
//        this round and it is the ResourcePtr null tripwire "Can not instance resource pointer",
//        baked CgsResourcePtr.h:544, which pins the identity), then GetType(luTypeId).
//        luTypeId is `lwz 0x34(lpPart)` and luPartId is `lbz 0x38(lpPart)`, both loaded BEFORE
//        the call because the call clobbers r3.
//   0x826117D4..0x826117EC  `lwz 0x40(type)` == PropTypeData::maParts, then `luPartId*48` and
//        `lfs 0x28`. ⚠️ 48 and 0x28 are CONSOLE numbers: the host PropPartTypeData is 64 bytes
//        with mfSphereRadius at +0x30 (BrnPhysicsPropTypeData.h:281-283 static_asserts both).
//        Written as GetParts()[luPartId].GetBoundingRadius() so the host layout drives it.
//   0x826117D8/0x826117F4  `addi r8, r30, 0x2B` -> liOutCacheSlotIndex = 43 + liPartIndex.
//
// ---- THE PROP ARM (partIndex == 0), r30 == liPropIndex ------------------------------------------
//   Same shape with 0xF == 15 == KU_MAX_PHYSICAL_PROPS (asserts :2235 / :2238), the
//   HasPropJustBeenRemoved early-out branching to the SAME `li r3,0` epilogue (0x82611868 ->
//   loc_8261168C), `mulli r11, r30, 0x70` == console stride 112 for PropInstance, assert :2248,
//   `lwz 0x64(lpProp)` == muTypeId, `lfs 0x44(type)` == PropTypeData::mfSphereRadius (host +0x50),
//   and `addi r11, r30, 0x1C` -> liOutCacheSlotIndex = 28 + liPropIndex.
//   ⚠️ MEASURED ASYMMETRY, not an omission: the prop arm has NO mUsedProps bit assert. There is
//   no bit test of any kind between 0x8261186C and 0x8261189C -- the part arm's IsBitSet has no
//   twin here.
//
// ---- THE COMMON TAIL ----------------------------------------------------------------------------
//   0x826118C0  `lwz r11, 0(r24)` -- the slot is RE-READ back through the out-reference rather
//        than reused from the register, which is why the assert below reads liOutCacheSlotIndex.
//   0x826118C4..0x826118EC  `cmpwi 0x1C / blt` + `cmpwi 0x49 / blt` -> assert :2258. 0x49 == 73
//        == 28 + (15 + 30), and the assert string (read in full this round) is literally
//        "liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX && liOutCacheSlotIndex <
//         KI_PROP_CACHE_START_INDEX + static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS +
//         KU_MAX_PHYSICAL_PROP_PARTS )" -- which is where the two cache constants above are named.
//   0x826118F0..0x82611910  the radius window. ⚠️ NaN POLARITY (gotcha 4), re-derived:
//        `lfs f13, flt_82001CC0(0.0f) ; fcmpu ; ble -> ASSERT` then
//        `lfs f13, flt_820138DC(50.0f) ; fcmpu ; ble -> SKIP`.
//        `ble` is false on an unordered compare, so a NaN radius falls through BOTH tests and
//        asserts. The C form `!( r > 0.0f && r <= 50.0f )` reproduces that exactly (NaN makes the
//        first conjunct false, so the assert fires), and it also reproduces the boundary cases:
//        r == 0.0f asserts (the console's `ble` takes the equal case), r == 50.0f does not.
//        Do NOT rewrite either half as `>= ` / `< ` -- that flips the NaN behaviour.
//   0x82611914..0x8261198C  the assert message is built with a CgsDev::StrStream, which is why
//        the DWARF names `CgsDev::StrStream lStrStream @2259` as a local of THIS function:
//        "Sphere radius: " << lfOutCacheSphereRadius << "\n", fired at :2259.
//   0x82611990  `li r3, 1` -- the single success return.
//
// ---- SOURCE SHAPE: one deliberate re-ordering, stated ------------------------------------------
// The DecFIGS scope for this function (dwarfdump .../BrnPropManager.cpp:2403) names its locals
// with their DECLARATION lines: liPartIndex @2198, lpPart @2200, luTypeId @2220, luPartId @2221,
// lpPartType @2222 / liPropIndex @2231, lpProp @2233, lpPropType @2251 / lStrStream @2259. Note
// lpPart @2200 and lpProp @2233 are declared AHEAD of their arms' asserts (@2202/@2205 and
// @2235/@2238) while the asm computes both addresses AFTER them -- i.e. the original declared the
// two pointers uninitialised at the top of each block and assigned them later. This body declares
// each at its point of assignment instead. That is a source-shape difference only: the assignment
// is a pure address computation with no side effects, and nothing between the two points reads
// the pointer. Written this way rather than emitting an `= NULL` store the console never makes.
// =================================================================================================
bool PropManager::GetTriangleCacheSlotAndRadius( PropEntityID lPropEntityId,
                                                 s32          liPhysicalPropIndex,
                                                 s32&         liOutCacheSlotIndex,
                                                 f32&         lfOutCacheSphereRadius )
{
    // 0x826115D4 -- see the divergence note in the banner: the console open-codes this mask.
    if ( lPropEntityId.GetPartIndex() != 0u )
    {
        s32 liPartIndex = liPhysicalPropIndex;                                  // DWARF :2198

        // :2202 / :2205 -- both non-gating (the asm falls through into the body either way).
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );
        CGS_ASSERT( liPartIndex >= 0
                        && liPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )" );

        if ( HasPartJustBeenRemoved( lPropEntityId, liPartIndex ) )
        {
            return false;                                                       // 0x8261168C
        }

        // :2212. mUsedParts is the BitArray<30> at console this+0x90; reached by name.
        CGS_ASSERT( mUsedParts.IsBitSet( static_cast<u32>( liPartIndex ) ),
                    "mUsedParts.IsBitSet( liPartIndex )" );

        PropPartInstance* lpPart = &mpaPartInstances[liPartIndex];              // DWARF :2200
        CGS_ASSERT( lpPart != NULL, "lpPart != NULL" );                        // :2217

        const u32 luTypeId = lpPart->GetType();                                 // DWARF :2220 (lwz 0x34)
        const u32 luPartId = lpPart->GetPartId();                               // DWARF :2221 (lbz 0x38)

        // DWARF :2222. The console folds the whole chain -- ResourcePtr::operator->, GetType,
        // maParts, the per-part stride -- into one expression; so does this. Note the DWARF
        // names NO PropTypeData local in this arm, only lpPartType, which is what pins the
        // fold as a single statement rather than two.
        const PropPartTypeData* lpPartType =
            &mpPhysicsData->GetType( luTypeId )->GetParts()[luPartId];

        lfOutCacheSphereRadius = lpPartType->GetBoundingRadius();               // stfs 0(r25)
        liOutCacheSlotIndex    = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;  // addi r30, 0x2B
    }
    else
    {
        s32 liPropIndex = liPhysicalPropIndex;                                  // DWARF :2231

        // :2235 / :2238 -- both non-gating.
        CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );
        CGS_ASSERT( liPropIndex >= 0
                        && liPropIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                    "liPropIndex >= 0 && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS )" );

        if ( HasPropJustBeenRemoved( lPropEntityId, liPropIndex ) )
        {
            return false;                                                       // shares 0x8261168C
        }

        PropInstance* lpProp = &mpaPropInstances[liPropIndex];                  // DWARF :2233
        CGS_ASSERT( lpProp != NULL, "lpProp != NULL" );                        // :2248

        // DWARF :2251. `lwz 0x64(lpProp)` is muTypeId; the type id is loaded before the
        // operator-> call clobbers r3, which is emission order, not a second statement.
        const PropTypeData* lpPropType = mpPhysicsData->GetType( lpProp->GetTypeId() );

        lfOutCacheSphereRadius = lpPropType->GetBoundingRadius();               // lfs 0x44 -> stfs
        liOutCacheSlotIndex    = KI_PROP_CACHE_START_INDEX + liPropIndex;       // addi r30, 0x1C
    }

    // :2258 -- non-gating. The console re-reads the slot back through the out-reference.
    CGS_ASSERT( liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX
                    && liOutCacheSlotIndex < KI_PROP_CACHE_START_INDEX
                           + static_cast<s32>( KU_MAX_PHYSICAL_PROPS
                                               + KU_MAX_PHYSICAL_PROP_PARTS ),
                "liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX && liOutCacheSlotIndex < "
                "KI_PROP_CACHE_START_INDEX + static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS + "
                "KU_MAX_PHYSICAL_PROP_PARTS )" );

    // :2259 -- non-gating, and the one assert in this function whose message is BUILT, not baked.
    // See the NaN-polarity note in the banner before touching the condition.
    if ( !( lfOutCacheSphereRadius > 0.0f
            && lfOutCacheSphereRadius <= KF_MAX_PROP_CACHE_SPHERE_RADIUS ) )
    {
        // ⚠️ The console streams into the SHARED global sink CgsDev::Assert::gpcMessageBuffer
        // (`lwz r31, gpcMessageBuffer ; stb 0,0(r31)` then the StrStream vtable off_82000D08).
        // That global is not declared anywhere in this tree, so the message is built on a stack
        // buffer of the same size instead -- the committed idiom, see BrnArbStateCarSelect.cpp:218.
        // Message content, stream order and the fired line are unchanged.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Sphere radius: " << lfOutCacheSphereRadius << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:2259
        CgsDev::Assert::EndAssert();
    }

    return true;                                                                // 0x82611990 li r3,1
}

// =================================================================================================
// BrnPhysics::Props::PropManager::BeginPropWorldContactGeneration @ 0x82628CB0  (89 asm insns)
//
// ROUND-2 LANDING of the round-1 parked body
// (scratchpad/waveQ/parked/PropManager_03_BeginPropWorldContactGeneration.cpp). The two
// declarations it was parked on now exist -- BaseCollisionGenerator::
// CreateCollidePrimitiveListWithTriangleListStream (CgsCollisionGenerator.h:240, X360 0x82811DD0,
// exported only as `sub_82811DD0`) and ::RunCollidePrimitiveListWithTriangleListStream
// (:241-242, X360 0x82811F58, IDA symbol truncated to "...RunCollidePrimitiveListWit").
// ⚠️ The round-1 park banner cited them as DWARF CgsCollisionGenerator.h:271/:275 and that is
// CORRECT (the wave spec's :385/:388 were the dumpfile's own line numbers). The round-2 collgen
// owner landed both at those signatures. Their BODIES are still parked (that owner's §4), so
// this file calls two declared-but-unbodied symbols -- reported, `cl /c` cannot see it.
//
// ⛔⛔ THE COMPLETE LINK-HOLE LIST THIS BODY INTRODUCES (AGENTS gotcha 12; re-grepped 2026-08-18.
//     The earlier banner listed only the two BaseCollisionGenerator symbols, which understated it
//     by half -- the two biggest callees below were missing entirely):
//       * PropManager::DoPartWorldContactGeneration          @0x82611B70 -- DECLARED
//         in BrnPropManager.h, NO definition anywhere in b5-decomp/src or vendor; body parked at
//         scratchpad/waveQ2/parked/PropManager_03_DoPartWorldContactGeneration.cpp
//         (PropManager_wQ2_03.cpp's own banner marks it NOT LANDED).
//       * PropManager::DoPropInstanceWorldContactGeneration   @0x826120E8 -- DECLARED
//         in BrnPropManager.h, same status; parked as
//         scratchpad/waveQ2/parked/PropManager_03_DoPropInstanceWorldContactGeneration.cpp.
//       * BaseCollisionGenerator::CreateCollidePrimitiveListWithTriangleListStream
//         (CgsCollisionGenerator.h:240) and ::RunCollidePrimitiveListWithTriangleListStream
//         (:241-242) -- declaration-only, bodies parked with the collgen owner.
//     VERIFIED-BODIED and therefore NOT holes (checked, so the list above is exhaustive):
//     BaseCollisionGenerator::Prepare(void*,s32) and ::Finish (CgsCollisionGenerator.cpp:68/:86),
//     SimpleDataStreamProducer::Begin (CgsSimpleDataStreamProducer_Begin.cpp) and ::End
//     (CgsSimpleDataStreamProducer.cpp:134), DataStreamCommandPoster::Begin/End,
//     BaseEventQueue<UpdatePropEvent>::GetEvent, PropEntityID::GetPartIndex.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//     r3 = this (r31) - r4 = lpTriangleCacheInterface (r29) - r5 = lpCollisionGenerator (r30)
//     r6 = lpLinearMalloc (r28) - v1 = lvfTimeStep
// ⚠️ gotcha 3's cousin: the VecFloat rides a VMX register and consumes NO GPR slot, so r6 really
// is the FOURTH parameter, not the third. The console saves v1 into v127 at 0x82628CC8 and
// restores it into v1 at 0x82628D64 before every dispatch, which is what makes that visible.
// The parameter ORDER itself is DWARF-attested (dwarfdump BrnPropManager.h), not inferred from
// the asm -- a vector arg skips no GPR, so the asm alone cannot order it.
//
// ---- DECODE -------------------------------------------------------------------------------------
//   0x82628CD0..0x82628CE8  `addis r4,r30,1 ; addi r4,r4,0x2400` (r30 + 0x12400), `lis r5,0x20`
//        (0x200000), `mr r3,r30`, `bl BaseCollisionGenerator::Prepare`.
//        ⚠️ r3 == r30 == the THIRD ARGUMENT, not `this` -- PropManager does not derive from the
//        generator. Those two console literals are exactly CollisionGenerator::
//        mau8CollisionResultsMemory and KI_RESULTS_MEMORY_SIZE, i.e. this is the committed
//        derived no-arg override CollisionGenerator::Prepare() (CgsCollisionGenerator.h:400,
//        `return BaseCollisionGenerator::Prepare(mau8CollisionResultsMemory,
//        KI_RESULTS_MEMORY_SIZE);`) INLINED. Written as that one call so the two console numbers
//        stay comments instead of becoming host offsets (gotcha 1). The bool result is dropped
//        by the console; dropped here too.
//   0x82628CFC  stw r26(0), 0x9C(r31)        miNumJobsAdded = 0
//   0x82628CF0/0x82628D00/0x82628D08  li r4,0x64 (100) -> the Create call -> stw r3, 0xA0(r31)
//   0x82628D04..0x82628D2C  assert :2346 (li r5,0x92A) "mpPrimitiveWithTriangleStream".
//        NON-GATING: the asm falls straight through into the loop.
//   0x82628D34  stw r26, 0x6570(r31)         miNumPropsAddedToContactGen = 0
//   0x82628D30/0x82628DA0  `lwz r11, 0x688(r31)` -- mUpdatedProps.GetLength() is read at the top
//        AND re-read at the bottom of every iteration, so the bound is live, not hoisted.
//   0x82628D4C  bl BrnPhysics::Props::UpdatePro   (IDA-truncated) ==
//        BaseEventQueue<UpdatePropEvent>::GetEvent(i) on this+0x680.
//   0x82628D54  lbz 0x68(event) / bne        skip the event when mbFrozen
//   0x82628D60..0x82628D90  lwz 0x60 (mEntityId), `clrlwi r11,r11,22`, the (x != 0) idiom, beq
//        -> the same bare-mask divergence documented at length on GetTriangleCacheSlotAndRadius
//        above. Same value, same extra tripwire, same open header request.
//   0x82628D94 / 0x82628D9C  DoPartWorldContactGeneration / DoPropInstanceWorldContactGeneration
//        with r3=this, r4=generator, r5=triangle cache, r6=the event, r7=r27 (== this+0x9C,
//        i.e. miNumJobsAdded BY REFERENCE -- `addi r27,r31,0x9C` at 0x82628CF8), r8=malloc,
//        v1=lvfTimeStep.
//   0x82628DB0..0x82628DF4  SimpleDataStreamProducer::Begin() emitted INLINE: six lwz/stw pairs
//        copying producer +0x20..+0x34 down to +0x00..+0x14 (mShared <- the private geometry),
//        `stb 1,0x100` (mbIsStreaming = true), `stw producer+0x80, 0x18` (mShared.mpPoster =
//        &mCommandPoster), then `bl DataStreamCommandPoster::Begin` on producer+0x80. That is
//        instruction-for-instruction the committed CgsSimpleDataStreamProducer_Begin.cpp body,
//        so it is written as that one call.
//   0x82628E00  bl RunCollidePrimitiveListWithTriangleListStream(generator, the producer) -- a
//        the LAST CALL BEFORE THE EPILOGUE (tail POSITION, not a tail call: it is an ordinary `bl`
//        followed by 0x82628E04 `addi r1,r1,0xA0` / 0x82628E08 `li r0,-0x50` / 0x82628E0C
//        `lvx128 v127,r1,r0` / 0x82628E10 `b __restgprlr_25`).
//        ⚠️ It returns EA::Jobs::Job* in r3 and r3 survives that epilogue, but
//        the DWARF/header signature of this function is `void` and there is NO store of the
//        result to any PropManager member anywhere in the 89 instructions. The value is
//        genuinely dropped. Stated, not tidied.
// =================================================================================================
void PropManager::BeginPropWorldContactGeneration(
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpCollisionGenerator,
    CgsMemory::LinearMalloc*                                       lpLinearMalloc,
    VecFloat                                                       lvfTimeStep )
{
    // 0x82628CE8 -- the derived no-arg override feeding its own embedded arena
    // (console: generator + 0x12400, size 0x200000). Result dropped by the console.
    lpCollisionGenerator->Prepare();

    miNumJobsAdded = 0;                                                     // stw 0x9C

    mpPrimitiveWithTriangleStream =
        lpCollisionGenerator->CreateCollidePrimitiveListWithTriangleListStream( 100 );  // li r4,0x64
    CGS_ASSERT( mpPrimitiveWithTriangleStream != NULL,
                "mpPrimitiveWithTriangleStream" );                          // :2346, non-gating

    miNumPropsAddedToContactGen = 0;                                        // stw 0x6570

    // The bound is re-read every iteration (lwz 0x688 at both 0x82628D30 and 0x82628DA0).
    for ( s32 liEvent = 0; liEvent < mUpdatedProps.GetLength(); ++liEvent )
    {
        const UpdatePropEvent& lrEvent = mUpdatedProps.GetEvent( liEvent );

        if ( lrEvent.mbFrozen )
        {
            continue;                                                       // lbz 0x68 / bne
        }

        // 0x82628D6C -- bare `clrlwi r11,r11,22`; see the divergence note above.
        if ( lrEvent.mEntityId.GetPartIndex() != 0u )
        {
            DoPartWorldContactGeneration( lpCollisionGenerator, lpTriangleCacheInterface,
                                          lrEvent, miNumJobsAdded, lpLinearMalloc, lvfTimeStep );
        }
        else
        {
            DoPropInstanceWorldContactGeneration( lpCollisionGenerator, lpTriangleCacheInterface,
                                                  lrEvent, miNumJobsAdded, lpLinearMalloc,
                                                  lvfTimeStep );
        }
    }

    // 0x82628DB0..0x82628DF4 -- SimpleDataStreamProducer::Begin() inlined by the console.
    mpPrimitiveWithTriangleStream->Begin();

    // 0x82628E00 -- the last call before the epilogue (tail POSITION, an ordinary `bl`); the
    // returned EA::Jobs::Job* is dropped (no store for it exists in the 89 instructions).
    lpCollisionGenerator->RunCollidePrimitiveListWithTriangleListStream(
        mpPrimitiveWithTriangleStream );
}

// =================================================================================================
// BrnPhysics::Props::PropManager::EndPropWorldContactGeneration @ 0x82628E18  (37 asm insns)
//
// ROUND-2 LANDING of the round-1 parked body
// (scratchpad/waveQ/parked/PropManager_03_EndPropWorldContactGeneration.cpp).
// ⚠️ THE ROUND-1 PARK BANNER'S DIAGNOSIS WAS WRONG AND IS CORRECTED HERE, per the round-2
// collgen owner's re-derivation: it said BaseCollisionGenerator::GetNumUsedResultLists() "does
// not exist" and wondered whether the console was reaching a private member. It does exist --
// the DWARF declares it public at CgsCollisionGenerator.h:298; it simply has no out-of-line
// export because it is a one-line header inline that the compiler folded into this one call
// site. The `lhzx` below IS that inlined accessor. So the park's DIAGNOSIS was wrong while its
// requested FIX (land it as a public header inline) was right, and that is what landed:
// CgsCollisionGenerator.h:114, over the already-correctly-typed member. No friendship, no
// offset poke.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//     r3 = this (r31) - r4 = lpContactInterface (r27) - r5 = lpCollisionGenerator (r30)
//     r6 = lWorldEntityId (r26)
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82628E38  bl BaseCollisionGenerator::Finish   r3 == r30, i.e. the GENERATOR, not `this`.
//   0x82628E3C  lwz r29, 0xA0(r31)                  mpPrimitiveWithTriangleStream
//   0x82628E40/0x82628E44  addi r3,r29,0x80 ; bl DataStreamCommandPoster::End
//   0x82628E54  stb r28(0), 0x100(r29)              mbIsStreaming = false
//        ^ those two, in that order, are exactly the committed
//          CgsMemory::SimpleDataStreamProducer::End() (`mCommandPoster.End(); mbIsStreaming =
//          false;`), so the pair is written as that one call.
//   0x82628E58  stw r28, 0xA0(r31)                  mpPrimitiveWithTriangleStream = NULL
//        ^ this happens BEFORE the assert below, not after. Order preserved.
//   0x82628E48..0x82628E5C  `lis r11,1 ; ori r11,r11,0x23BC ; lhzx r11, r30, r11` -- a HALFWORD
//        read at console generator+0x123BC == mu16NumUsedResultLists.
//        ⚠️ 0x123BC IS A CONSOLE OFFSET and must NOT be reproduced (gotcha 1): the host
//        BaseCollisionGenerator is wider (IOBuffer base + 8-byte CollisionResultList* pointers
//        in mapCollisionResultLists[200]), so that byte offset lands somewhere else entirely.
//        Reached through the accessor instead.
//   0x82628E60..0x82628E88  `lwz r10, 0x9C(r31)` (miNumJobsAdded, an s32) compared to that u16
//        under a SIGNED `cmpw`, assert :2416 (li r5,0x970), NON-GATING. Reproduced as an s32
//        comparison with an explicit widening cast so the console's signedness survives rather
//        than the compare silently becoming unsigned.
//   0x82628E90  stw r28, 0x64B4(r31)                miNumDebugWorldContacts = 0
//   0x82628EA0  bl AddContactResultsToQueue         the last call before the epilogue (tail
//        POSITION, not a tail call -- `bl`, then 0x82628EA4 `addi r1,r1,0x90` / 0x82628EA8
//        `b __restgprlr_26`): r3=this, r4=r27, r5=r30, r6=r26
//
// ⚠️ AddContactResultsToQueue @0x82612F08 has NO per-address JSON export and no identity.json
// row (an export hole, not a missing function: two independent headless-IDA reads of the .i64 put
// BrnPhysics::Props::PropManager::AddContactResultsToQueue at 82612F08..826131E8). That EXPORT hole
// is real and is worth keeping written down.
// ✅ IT IS NOT A LINK HOLE. Round-2 correction (the earlier banner said it "is NOT bodied anywhere
// in the tree", which was false the day it was written): the body landed this same wave in the
// sibling part-file b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_03.cpp, as
// `PropManager::AddContactResultsToQueue` -- the only function that file defines. (Cited by name:
// the :348/:454 line pair this banner used to carry is exactly the kind of citation that goes
// stale on the next edit.) Mount wQ2_03 alongside this file; do NOT write
// a second definition or a trap stub for it.
// =================================================================================================
void PropManager::EndPropWorldContactGeneration(
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*      lpCollisionGenerator,
    CgsSceneManager::EntityId                               lWorldEntityId )
{
    lpCollisionGenerator->Finish();                                     // 0x82628E38 (r3 == r30)

    // 0x82628E40..0x82628E54 -- SimpleDataStreamProducer::End() inlined by the console.
    mpPrimitiveWithTriangleStream->End();
    mpPrimitiveWithTriangleStream = NULL;                               // stw 0xA0, BEFORE the assert

    // :2416 -- non-gating. Halfword read, SIGNED compare against the s32 job counter.
    CGS_ASSERT( miNumJobsAdded == static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() ),
                "miNumJobsAdded == lpContactGenerator->GetNumUsedResultLists()" );

    miNumDebugWorldContacts = 0;                                        // stw 0x64B4

    // 0x82628EA0 -- the last call before the epilogue (tail POSITION, an ordinary `bl`).
    AddContactResultsToQueue( lpContactInterface, lpCollisionGenerator, lWorldEntityId );
}

}
}
