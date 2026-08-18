// GameSource/Physics/PropManager/PropManager_wQ2_04.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 04.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class banner,
// the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     CreatePart() @ 0x826278D0   (382 instructions, 0x826278D0..0x82627EC4)
//     RemovePart() @ 0x8260F988   (117 instructions, 0x8260F988..0x8260FB58)
//     RemoveProp() @ 0x8260F540   (273 instructions, 0x8260F540..0x8260F980)
//
// All three counts were COUNTED this round from each export's own `assembly` text -- the JSON
// stores it as a single NEWLINE-DELIMITED STRING, not an array, so the cross-check is
// (last - first)/4 + 1 against the NON-BLANK LINE COUNT (round-2 NIT: "the array length" named a
// check that cannot be run as written): 382 / 117 / 273, agreeing both ways. They also agree with
// scratchpad/waveQ2/physfix.owner.md's table, which is a cross-check, not the source.
//
// These are the SPAWN and the two RETIRE legs of the breakable-prop lifetime:
//   * CreatePart turns one shed panel of a smashed prop into a live rigid body -- it claims a
//     part slot, seeds the PropPartInstance, registers the part's triangle-cache slot with the
//     scene, and posts one InAddRigidBody carrying the transform, the two velocities, the
//     inverse inertia (GetPartInertia x KVF_INERTIA_SCALE, reciprocated per axis), the inverse
//     mass and the four KF_PROP_* drag/limit scalars.
//   * RemovePart / RemoveProp are the reverse: evict the triangle-cache slot, free the slot bit,
//     (for a prop) release its joint, and post one InRemoveRigidBody.
//
// Callers, from the image's own xrefs_to, re-measured 2026-08-18 (CreatePart has ONE; RemovePart
// and RemoveProp have TWO each -- the earlier "each function has exactly one" was wrong and
// contradicted its own next line):
//   CreatePart  <- ProcessAddPartInstanceEvents    @0x826280F8  (PropManager_wQ_02.cpp)
//   RemovePart  <- ProcessRemovePartInstanceEvents @0x82627818  (PropManager_wQ_02.cpp)
//                  and ReadUpdatedBodies           @0x82632918  (PropManager_wQ2_01.cpp, the
//                                                   frozen-part arm)
//   RemoveProp  <- ProcessRemovePropInstanceEvents @0x82627778  (an EXPORT HOLE -- no per-address
//                                                   JSON -- and still un-bodied in the tree)
//                  and ReadUpdatedBodies           @0x82632918  (PropManager_wQ2_01.cpp, the
//                                                   frozen-prop arm)
//   ⚠️ So RemoveProp is REACHABLE FROM LANDED CODE TODAY: PropManager_wQ2_01.cpp calls both it
//   and RemovePart. Neither is dead code waiting on the un-bodied Process* leg.
//
// ==================================================================================================
// GOTCHA 1 (console literals are not host values) -- WHERE EACH ONE WENT
// ==================================================================================================
// Every console byte offset below appears in a COMMENT only. Specifically:
//   * `lwz 0x8C(this)` / `slwi idx,6`  == mpaPartInstances + idx*64 (CONSOLE stride) -> written as
//     mpaPartInstances[liPropPartIndex]; the host sizeof does the striding.
//   * `lwz 0x7C(this)` / `mulli idx,0x70` == mpaPropInstances + idx*112 (CONSOLE stride) -> same.
//   * `slwi partIdx,1 ; add ; slwi 4` == partIdx*48, the CONSOLE PropPartTypeData stride
//     (the host record is 64 -- BrnPhysicsPropTypeData.h:_AssertLayout pins both) -> written as
//     lpType->GetParts()[ li16PartIndex ].
//   * `addis r3,scene,0xC ; addi r3,r3,0x4930` == scene + 0xC4930 == &mAddToCacheQueue and
//     `+0xC77E0` == &mRemoveFromCacheQueue -- CONSOLE offsets into a struct whose host layout
//     differs (every embedded EventQueue header is 8 bytes wider on LLP64). Reached BY NAME.
//   * `(idx>>6 + 0x12)*8 + this` == &mUsedParts, `+0x10` == &mUsedProps, `+0xCE` ==
//     &mUsedPropJoints -- CONSOLE dword offsets. Reached BY NAME.
//   * the InAddRigidBody / InRemoveRigidBody / InEventAddToCache / InEventRemoveFromCache stack
//     records are filled BY MEMBER, never by offset; the offset map is quoted in the banners so
//     the field identification is auditable, and every one of those members is already pinned by
//     a committed static_assert (CgsPhysicsSimulationIO_Events.h:279-282 / :102-106,
//     rw/physics/inertia.h:_rw_physics_Inertia_AssertLayout).
// GOTCHA 3 (a float rides an FPR and SKIPS its GPR slot): no float or vector PARAMETER exists on
//   RemovePart/RemoveProp, so there is no site. CreatePart takes two Vector3s; they ride v1/v2 and
//   consume no GPR slot, which is exactly why r8 (liSlotIndex) follows r7 (&lTransform) with the
//   two vectors "in between" in the declaration -- the committed signature already encodes that
//   and the ProcessAddPartInstanceEvents call site matches it positionally.
// GOTCHA 4 (NaN polarity): exactly TWO compare-and-select sites exist, both in CreatePart, both
//   `fcmpu ; blt <skip> ; fmr` == `(a < b) ? a : b`. That is a MIN with the unordered case falling
//   into the `fmr` (so a NaN in the first operand yields the SECOND). Written as the explicit
//   ternary, not as any fpu::Min/Max helper -- see the SetInverseInertia note.
// ==================================================================================================
//
// ⛔ CALLEES DECLARED BUT NOT BODIED ANYWHERE IN THE TREE (`cl /c` cannot see an unresolved
//    external, so this file gating green does NOT mean the TU links). Reported, not stubbed:
//      BrnPhysics::Props::PropManager::GetPartInertia   @0x82612AC8  (declared in BrnPropManager.h;
//          its round-1 body is parked at scratchpad/waveQ/parked/
//          PropManager_04_GetPropInertia_GetPartInertia.cpp, blocked on the rw::collision::AABBox
//          include clash -- physfix.owner.md §5.1 REQUEST 1b)
//      BrnPhysics::Props::PropPartInstance::Construct   (BrnPropPartInstance.h:37, declaration-only)
//    Everything else this file calls has a real body -- each one is named at its call site.

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"       // PropInstance
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"   // PropPartInstance
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"    // KU_MAX_PHYSICAL_PROP_PARTS
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                      // PropEntityID
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"             // PropPhysicsDataHeader::GetType
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"               // PropTypeData / PropPartTypeData / KU_MAX_PROP_TYPES
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // InputBuffer + the two queue getters
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // InAddRigidBody / InRemoveRigidBody / NewRigidBody
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"// InSceneUpdateInterface + its two cache queues
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // InEventAddToCache / InEventRemoveFromCache
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"                  // CgsDev::StrStream (the :778 / :859 messages)
#include "rw/physics/rigidbody.h"                                             // rw::physics::ACTIVE_BODY
#include "rw/physics/inertia.h"                                               // rw::physics::Inertia setters

namespace BrnPhysics
{
namespace Props
{

// -------------------------------------------------------------------------------------------------
// The triangle-cache slot map. Both constants are DWARF-attested WITH THEIR VALUES in
// references/DecFIGS/dwarfdump/GameSource/Physics/BrnTriangleCacheConstants.h:
//     BrnTriangleCacheConstants.h:36   const int32_t KI_PROP_CACHE_START_INDEX      = 28;
//     BrnTriangleCacheConstants.h:37   const int32_t KI_PROP_PART_CACHE_START_INDEX = 43;
// and both are X360-attested in THIS file's three bodies: `addi r11, r31, 0x1C` (0x8260F558,
// RemoveProp) and `addi r11, r29, 0x2B` (0x8260F99C, RemovePart) / `addi r11, r30, 0x2B`
// (0x82627CD4, CreatePart). 0x1C == 28, 0x2B == 43. (The 43 is also 28 + 15, i.e. the prop slots
// 28..42 are followed immediately by the 30 part slots 43..72 -- an arithmetic self-check, not
// the grounding.)
//
// ⚠️ DUPLICATED FILE-LOCAL DEFINITION, DELIBERATE AND FLAGGED. Their DWARF home
// b5-decomp/src/GameSource/Physics/BrnTriangleCacheConstants.h DOES NOT EXIST in the tree
// (re-checked this round), and this lander may not create a header. PropManager_wQ2_02.cpp
// declares the same two constants file-locally (in its own triangle-cache-constants banner), with the same MOVE-WHEN-IT-LANDS caveat; both are
// `static` (internal linkage) so this is not an ODR break, but it IS a second copy of one fact and
// is reported as a header request rather than left silent. When BrnTriangleCacheConstants.h lands,
// delete BOTH copies and include it.
static const s32 KI_PROP_CACHE_START_INDEX      = 28;   // DWARF BrnTriangleCacheConstants.h:36
static const s32 KI_PROP_PART_CACHE_START_INDEX = 43;   // DWARF BrnTriangleCacheConstants.h:37


// =================================================================================================
// BrnPhysics::Props::PropManager::CreatePart @ 0x826278D0   (382 instructions)
// DWARF: class decl BrnPropManager.h:347, body scope BrnPropManager.cpp:837.
//
// THE PART SPAWN. Its one caller, ProcessAddPartInstanceEvents @0x826280F8, hands it one
// AddPhysicalPartEvent's fields plus two ZERO Vector3s as the initial velocities.
//
// ---- REGISTER MAP, read off the prologue 0x826278F0..0x82627928 --------------------------------
//   r3  = this (r16)          r4 = lEntityId (r17)      r5 = luPropTypeIndex (r19)
//   r6  = li16PartIndex (r15) r7 = &lTransform (r14)
//        ⚠️ WHAT IS MEASURED IS THE POINTER, NOT ITS PROVENANCE (round-2 NIT): the DWARF declares
//        this parameter `const rw::math::vpu::Matrix44Affine&` (dwarfdump BrnPropManager.cpp:874),
//        while the committed header spells it by value -- the tree-wide convention, shared by
//        AddPropToSim (:434) and BreakJoint (:2037). Both spellings put the same pointer in r7 (the
//        by-value form adds a caller-side copy), so the asm cannot tell them apart; stated here so
//        the by-value spelling is not read back as a measurement.
//   r8  = liSlotIndex (r30)   r9 = lpSceneInput (spilled to arg_6C)
//   r10 = lpSimModuleInputBuffer (spilled to arg_74)
//   v1  = lLinearVelocity (v127)   v2 = lAngularVelocity (v126)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE (all BrnPropManager.cpp unless stated) -------------------
//   :851 (0x353)  "lEntityId.IsPart()"
//   :856 (0x358)  "liPropPartIndex != -1"
//   :857 (0x359)  "!mUsedParts.IsBitSet( liPropPartIndex )"
//   :859 (0x35B)  StrStream "Too many physical parts: " << liPropPartIndex << "\n"
//   :874 (0x36A)  "luPropTypeIndex < KU_MAX_PROP_TYPES"
//   :876 (0x36C)  "li16PartIndex < lpType->muNumberOfParts"
//   :880 (0x370)  "luPropTypeIndex < KU_MAX_PROP_TYPES"      <-- the SAME message again
//   :881 (0x371)  "li16PartIndex < lpType->muNumberOfParts"  <-- and the same again
//   BrnPropEntityID.h:278 (0x116) "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
//   CgsBitArray.h:203 (0xCB) / :222 (0xDE) -- IsBitSet's and SetBit's own bound tripwires.
// The 874/876 pair and the 880/881 pair are FOUR DISTINCT SOURCE LINES with two distinct messages;
// the emission holds both pairs in full. Reproduced as four asserts -- do NOT tidy either pair
// away (same treatment as the twin :735/:737 asserts in PropManager_wQ_02.cpp).
//
// ---- liPropPartIndex IS liSlotIndex -----------------------------------------------------------
// The DWARF names `int32_t liPropPartIndex` at source :839 -- the first line of the body -- and
// r30 (the liSlotIndex parameter) is what every one of the asserts above tests and what indexes
// mUsedParts and mpaPartInstances, with no arithmetic anywhere between the prologue and its first
// use. So :839 is `int32_t liPropPartIndex = liSlotIndex;` (or the parameter under its DWARF name).
//
// ---- THE DWARF LOCALS, AND THE TWO THAT HAVE NO EMISSION --------------------------------------
// The DecFIGS scope names ten locals: liPropPartIndex @839, PropVolumeInstanceID
// lPropVolumeInstanceID @841, RigidBodyId lRigidBodyId @842, InAddRigidBody
// lSimulationAddBodyEvent @844, Vector3 lInverseInertia @845, const PropTypeData* lpType @847,
// PropPartTypeData* lpPartType @848, PropPartInstance* lpPropPart @849, Vector3 lInertia @914,
// CgsDev::StrStream lStrStream @859.
// ⚠️ `lPropVolumeInstanceID` @841 has NO counterpart in the emission and NOTHING IS SYNTHESISED
//    for it. Negative evidence, measured: the 382 instructions contain no 64-bit volume-instance
//    traffic at all -- the only scene-side call in the whole body is the InEventAddToCache append
//    at 0x82627CE4, there is no AddVolumeInstance / SetVolumeInstanceTransform / VolumeInstanceId
//    ::Set among the fifteen `bl` targets (xrefs_from), and no `std` outside the InAddRigidBody
//    record and the assert frames. It is dead in this build. This note exists so a later sweep that
//    re-reads the DWARF does not "restore" a statement the image disproves.
// ⚠️ `lInertia` is declared at :914 -- far below the rest -- which is why the GetPartInertia call
//    and the reciprocal fold sit at the END of the body rather than beside the other inertia
//    fields. The emission agrees (0x82627D88 onwards).
//
// ---- lpType / lpPartType are `const` here ------------------------------------------------------
// The DWARF spells lpPartType `PropPartTypeData*` (non-const). The tree's only route to it,
// PropTypeData::GetParts(), returns `const PropPartTypeData*`, and this body only READS it
// (GetBoundingRadius / GetMass, plus the const GetPartInertia parameter). Kept const rather than
// const_cast-ing to match a DWARF spelling nothing here needs.
// =================================================================================================
void PropManager::CreatePart( PropEntityID                                             lEntityId,
                              u32                                                      luPropTypeIndex,
                              s16                                                      li16PartIndex,
                              Matrix44Affine                                           lTransform,
                              Vector3                                                  lLinearVelocity,
                              Vector3                                                  lAngularVelocity,
                              s32                                                      liSlotIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // :839 -- the DWARF's own name for the slot this part takes (see the banner).
    const s32 liPropPartIndex = liSlotIndex;

    // :851. 0x82627900 `clrlwi r11, r17, 22` == lEntityId & 0x3FF == the part-index field, then
    // the standard subfic/subfe/clrlwi-31 (x != 0) boolean idiom -> `IsPart()`.
    // ⚠️ DIVERGENCE, STATED NOT HIDDEN: the console open-codes that mask with NO `bl` between
    // 0x826278FC and 0x82627928 -- i.e. no owner tripwire. The tree's only accessor for the field,
    // PropEntityID::GetPartIndex(), is out-of-line and opens with AssertIsProp(), so calling it
    // here adds one call + one owner assert the console does not emit at this point. The VALUE is
    // identical and the owner is asserted twelve instructions later anyway (see the RigidBodyId
    // build below), so this is a note, not a bug. The DWARF-declared `bool IsPart() const`
    // (BrnPropEntityID.h:27) is what this line wants; it is not declared in this tree yet.
    CGS_ASSERT( lEntityId.GetPartIndex() != 0u, "lEntityId.IsPart()" );

    // :856. 0x82627964 `cmpwi cr6, r30, -1` -- SIGNED, against the class's own
    // KI_PROP_INDEX_NOT_FOUND == -1.
    CGS_ASSERT( liPropPartIndex != KI_PROP_INDEX_NOT_FOUND, "liPropPartIndex != -1" );

    // :857. mUsedParts is the BitArray<30> at console this+0x90; reached by name.
    // The console emits IsBitSet's OWN bound tripwire inline first (0x826279A8..0x82627A30:
    // StrStream "invalid index : " << liPropPartIndex << " < " << 30, fired at CgsBitArray.h:203).
    // CgsBitArray.h's banner (:15-17) states that tripwire is the CALLER's to emit because the
    // container header carries no assert-system dependency, and BrnPropManager_PropInstanceQueries
    // .cpp:59/:78 is the committed precedent for spelling it as a plain "invalid index" here.
    // Note the console's compare is `cmplwi ... 0x1E` -- UNSIGNED -- which is why a -1 index falls
    // straight through the :856 assert into this one (the emission literally branches from the
    // :856 assert into the bound-assert block at 0x8262799C).
    CGS_ASSERT( static_cast<u32>( liPropPartIndex ) < mUsedParts.GetCapacity(), "invalid index" );
    CGS_ASSERT( !mUsedParts.IsBitSet( static_cast<u32>( liPropPartIndex ) ),
                "!mUsedParts.IsBitSet( liPropPartIndex )" );

    // :858. SetBit's own bound tripwire first (0x82627A94..0x82627B1C: StrStream "Index: " <<
    // liPropPartIndex << ", Number of bits: " << 30, fired at CgsBitArray.h:222 -- a DIFFERENT
    // line from IsBitSet's :203 and from UnSetBit's :241), then the ld/or/std at
    // 0x82627B28..0x82627B30.
    CGS_ASSERT( static_cast<u32>( liPropPartIndex ) < mUsedParts.GetCapacity(), "Index" );
    mUsedParts.SetBit( static_cast<u32>( liPropPartIndex ) );

    // :859. ⚠️ MEASURED ORDER, not a transcription slip: the bit is SET at 0x82627B30 and the
    // ceiling is asserted afterwards (the `cmpwi cr6, r30, 0x1E` at 0x82627B24 is scheduled into
    // the middle of the bit math but its `blt` is at 0x82627B34, after the `stdx`). This compare is
    // SIGNED (`cmpwi`), unlike the container's unsigned one above. The DWARF's `CgsDev::StrStream
    // lStrStream @859` independently confirms the stream lives on this line.
    if ( !( liPropPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ) ) )
    {
        // The console builds the message in CgsDev::Assert::gpcMessageBuffer, a global this tree
        // does not declare; a stack buffer of the same size is the committed substitute
        // (PropManager_wQ2_02.cpp does the same in its cache-slot-range assert;
        // BrnArbStateCarSelect.cpp:218). Content, stream order and
        // the fired line are unchanged. `<< liPropPartIndex` binds the s32 overload -- the console
        // calls the "%d" formatter (0x821F0E50) here, not the "%u" one it uses for the bound
        // messages above.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Too many physical parts: " << liPropPartIndex << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:859
        CgsDev::Assert::EndAssert();
    }

    // :849. `lwz r11, 0x8C(r16)` + `slwi r10, r30, 6` == mpaPartInstances + idx*64 (CONSOLE
    // stride); the host sizeof strides the array.
    PropPartInstance* lpPropPart = &mpaPartInstances[ liPropPartIndex ];

    // 0x82627BD4..0x82627BE8 -- SIX stores into the slot, all plain and inline (no `bl`), in the
    // emission's own order:
    //     stb 0x39 = 0            mbUpdated        <- no setter exists on PropPartInstance
    //     stvx +0x10 = {0,0,0,0}  mLinearVelocity  <- SetLinearVelocity is a `bl`; this is not it
    //     stw  0x30 = r17         mEntityId        -> SetEntityId
    //     stvx +0x20 = {0,0,0,0}  mAngularVelocity <- ALSO Construct's store; this body never calls
    //                                                  SetAngularVelocity (there is no such `bl`)
    //     stw  0x34 = r19         muTypeId         -> SetType
    //     stb  0x38 = r15         mu8PartId        -> SetPartId (the low byte of li16PartIndex)
    // and NOTHING is stored to +0x00 (mPos) -- SetPosition writes it two instructions later.
    //
    // ⚠️ INFERENCE, FLAGGED: the three fields with no matching named setter (mbUpdated and the two
    // zeroed velocity lanes) are written here as PropPartInstance::Construct(). What is MEASURED
    // is the three plain stores; what is INFERRED is that they are that DWARF-declared method
    // inlined (the DecFIGS callee list for CreatePart names PropPartInstance::Construct, which is
    // what makes the inference more than a guess). It IS load-bearing for the link:
    // PropPartInstance::Construct (BrnPropPartInstance.h:37) is declaration-only in this tree, so
    // this call is an unresolved external and is reported as such rather than stubbed.
    //
    // ⚠️⚠️ CONTRACT FOR WHOEVER BODIES PropPartInstance::Construct (round-2 MUST_FIX -- the earlier
    // "even a Construct() that zeroed the whole record would be indistinguishable" argument covered
    // only a Construct that zeroes MORE, and the dangerous direction is one that zeroes LESS):
    // at this call site the console zeroes mLinearVelocity (+0x10), mAngularVelocity (+0x20) and
    // mbUpdated (+0x39), and does NOT write mPos (+0x00). NOTHING ELSE IN CreatePart WRITES +0x20 --
    // measured: the only store to it in all 382 instructions is `stvx128 v0, r31, r24` @0x82627BE0
    // with v0 == vspltisw 0 and r24 == 0x20 -- so a Construct() that omits the mAngularVelocity zero
    // leaves stale garbage from the previously recycled part slot where the console guarantees zero.
    // That IS observable. mLinearVelocity is re-written by SetLinearVelocity below, mPos by
    // SetPosition, so those two are the forgiving ones; +0x20 and +0x39 are not.
    lpPropPart->Construct();
    lpPropPart->SetEntityId( lEntityId );
    lpPropPart->SetType( luPropTypeIndex );
    lpPropPart->SetPartId( static_cast<u8>( li16PartIndex ) );

    // 0x82627BEC `lvx128 v1, r0, r25` with r25 == &lTransform + 0x30 == the affine's translation
    // row, then `bl PropPartInstance::SetPosition @0x825DE798`.
    lpPropPart->SetPosition( lTransform.Pos() );
    // 0x82627BF4 `vmr128 v1, v127` then `bl PropPartInstance::SetLinearVelocity @0x825DE860`.
    // ⚠️ MEASURED ASYMMETRY, stated exactly (round-2 MUST_FIX -- the earlier wording said "the
    // ANGULAR velocity is NOT written to the instance", which contradicts the zero store above):
    // the lAngularVelocity PARAMETER is never stored into the instance. mAngularVelocity (+0x20) IS
    // written, with ZERO, by Construct() above; lAngularVelocity itself (v126) survives untouched to
    // the InAddRigidBody record below and nowhere else -- there is no second `bl` and no `stvx` of
    // v126 into lpPropPart anywhere in the body (the only two stvx of v126 are the prologue spill
    // @0x826278E0 and the event record @0x82627D04).
    lpPropPart->SetLinearVelocity( lLinearVelocity );

    // :874. `cmplwi cr6, r19, 0x1F4` == 500 == KU_MAX_PROP_TYPES.
    CGS_ASSERT( luPropTypeIndex < KU_MAX_PROP_TYPES, "luPropTypeIndex < KU_MAX_PROP_TYPES" );

    // :847. 0x82627C28 `addi r3, r16, 0x54` -> CgsResource::ResourcePtr<T>::operator-> (0x822868E0,
    // identified by its baked assert "Can not instance resource pointer..." at CgsResourcePtr.h:544,
    // the NON-const overload's line), then `bl PropPhysicsDataHeader::GetType @0x82277C50` with
    // r4 = luPropTypeIndex. mpPhysicsData is the ResourcePtr at console this+0x54.
    const PropTypeData* lpType = mpPhysicsData->GetType( luPropTypeIndex );

    // :876. `extsh r31, r15` (li16PartIndex SIGN-extended) vs `lbz r11, 0x5D(r29)`
    // (muNumberOfParts, zero-extended), compared with `cmpw` -- a SIGNED compare of the two.
    CGS_ASSERT( static_cast<s32>( li16PartIndex ) < static_cast<s32>( lpType->GetNumberOfParts() ),
                "li16PartIndex < lpType->muNumberOfParts" );

    // :880 / :881. The same two conditions again, on two further source lines. Both are in the
    // emission in full (0x82627C6C..0x82627CAC) and both re-load `lbz 0x5D(r29)`.
    CGS_ASSERT( luPropTypeIndex < KU_MAX_PROP_TYPES, "luPropTypeIndex < KU_MAX_PROP_TYPES" );
    CGS_ASSERT( static_cast<s32>( li16PartIndex ) < static_cast<s32>( lpType->GetNumberOfParts() ),
                "li16PartIndex < lpType->muNumberOfParts" );

    // :848. `lwz r10, 0x40(r29)` == PropTypeData::maParts (console +0x40, host +0x48) and
    // partIdx*48 == the CONSOLE PropPartTypeData stride; indexed by name here.
    const PropPartTypeData* lpPartType = &lpType->GetParts()[ li16PartIndex ];

    // 0x82627CD4..0x82627CE4 -- register this part's triangle-cache slot with the scene.
    // `addi r11, r30, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + liPropPartIndex, and
    // `lfs f0, 0x28(r31)` == PropPartTypeData::mfSphereRadius (console +0x28, host +0x30).
    // ⚠️ MEASURED: the radius is the RAW GetBoundingRadius() -- there is NO KF_TRIANGLE_CACHE_PADDING
    //    add here, unlike the two UpdateCachedObjectPosition producers
    //    (BrnDetachedWheelManager.cpp:260 / BrnDetachedPartManager.cpp:299). No `fadds` and no
    //    rodata float load between 0x82627CD8 and 0x82627CE4.
    // ⚠️ The DWARF names an `AddCachedObject` producer on InSceneUpdateInterface
    //    (CgsSceneManagerIO_SceneUpdate.h:472) that this tree does not declare; the console inlines
    //    it, so the emission is a bare `bl BaseEventQueue<InEventAddToCache>::AddEvent @0x825E4620`
    //    on the queue itself. Appending to the named queue member reproduces that call exactly and
    //    invents no API. The queue's own "Reached Max length" tripwire lives in AddEvent
    //    (CgsBaseEventQueue.h:313); the caller emits no separate producer assert -- measured, there
    //    is no length compare between 0x82627CD0 and the `bl`.
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddToCacheEvent;
        lAddToCacheEvent.miCacheSlot         = KI_PROP_PART_CACHE_START_INDEX + liPropPartIndex;
        lAddToCacheEvent.mfCacheSphereRadius = lpPartType->GetBoundingRadius();

        lpSceneInput->mAddToCacheQueue.AddEvent( lAddToCacheEvent );
    }

    // ---- :844, the InAddRigidBody record ---------------------------------------------------------
    // OFFSET MAP, from the stack slots the emission fills (record base == var_180, the pointer
    // handed to AddEvent). Each console offset is matched to the member a committed static_assert
    // already pins, so nothing here is an offset cast:
    //   +0x00 mID                                   `std` 0x82627D84
    //   +0x10..+0x40 mRigidBody.mTransform          four lvx/stvx pairs 0x82627E68..0x82627E9C
    //   +0x50 mRigidBody.mVelocity                  `stvx128 v127` 0x82627CF4
    //   +0x60 mRigidBody.mAngularVelocity           `stvx128 v126` 0x82627D04
    //   +0x70 mRigidBody.mInertia.mInvTens          `stvx128 v0`   0x82627E44
    //   +0x80 mRigidBody.mInertia.mInvMass          `stfs`         0x82627D7C
    //   +0x84 mRigidBody.mInertia.mSpherical        `stfs`         0x82627E70
    //   +0x88 mRigidBody.mInertia.mMaxVelocity      `stfs`         0x82627D28
    //   +0x8C mRigidBody.mInertia.mMaxOmega         `stfs`         0x82627D30
    //   +0x90 mRigidBody.mInertia.mLinearDrag       `stfs`         0x82627D20
    //   +0x94 mRigidBody.mInertia.mAngularDrag      `stfs`         0x82627D18
    //   +0xA0 mRigidBody.mbSpy                      `stb 0`        0x82627D0C
    //   +0xB0 meState                               `stw 4`        0x82627CFC
    // (NewRigidBody sits at InAddRigidBody+0x10 and Inertia at NewRigidBody+0x60, so
    //  Inertia+0x18 == record+0x88 and so on -- CgsPhysicsSimulationIO_Events.h:279-282/:102-106
    //  and rw/physics/inertia.h:_rw_physics_Inertia_AssertLayout pin every one of them.)
    // Nothing writes +0x98/+0x9C (Inertia's tail padding) or the +0xA1..+0xAF gap -- so the record
    // ships with those bytes as stack garbage, exactly as it does on the console.
    CgsPhysics::PhysicsSimulationIO::InAddRigidBody lSimulationAddBodyEvent;

    lSimulationAddBodyEvent.mRigidBody.mVelocity        = lLinearVelocity;
    lSimulationAddBodyEvent.mRigidBody.mAngularVelocity = lAngularVelocity;

    // ⚠️⚠️ MEASURED CROSS-WIRING -- transcribed as it is, NOT "corrected". Two independent facts
    // meet here and both are solid:
    //   (a) the ADDRESS -> NAME map: PropDebugComponent::OnActivate @0x825E3628 hands
    //       flt_82F2A390 to RegisterVariable with the label "Max Angular Velocity" and
    //       flt_82F2A394 with "Max Linear Velocity" (re-read this round at 0x825E391C..0x825E3970),
    //       which is where KF_PROP_MAX_ANGULAR_VEL == 27.0f / KF_PROP_MAX_LINEAR_VEL == 30.0f come
    //       from (BrnPropManager.cpp:184-185).
    //   (b) the OFFSET -> MEMBER map: Inertia+0x18 is mMaxVelocity and +0x1C is mMaxOmega, by DWARF
    //       declaration order and by rw::physics::RigidBody::DynamicUpdate @0x82BC2B78, which
    //       clamps the ANGULAR magnitude first from `lfs 0x1C` (0x82BC2DF8) and the linear one
    //       second from `lfs 0x18` (0x82BC2EA8) -- I re-read both loads this round.
    // The emission puts flt_82F2A390 (the ANGULAR constant) into +0x18 (the LINEAR clamp) and
    // flt_82F2A394 (the LINEAR constant) into +0x1C (the ANGULAR clamp). AddPropToSim @0x826274D8
    // does exactly the same (0x82627598/0x826275A0 into its record's +0x88/+0x8C), so it is not a
    // one-off. Whether the shipped SOURCE crossed the two arguments or one of the two name maps
    // above is mislabelled is NOT decidable from the asm, and this file does not decide it: what is
    // reproduced is the stores. If a later wave settles it, fix it in ONE place with the evidence.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(  KF_PROP_MAX_ANGULAR_VEL ); // -> +0x18
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity( KF_PROP_MAX_LINEAR_VEL );  // -> +0x1C
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetLinearDrag(  KF_PROP_LINEAR_DRAG );            // -> +0x20
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetAngularDrag( KF_PROP_ANGULAR_DRAG );           // -> +0x24
    // (KF_PROP_RESTITUTION @0x82F2A398 is the fifth of that rodata run and is NOT read anywhere in
    //  these 382 instructions -- stated because the sibling AddPropToSim banner says "the five
    //  KF_PROP_* scalars"; here it is four.)

    lSimulationAddBodyEvent.mRigidBody.mbSpy = false;                       // `stb 0` 0x82627D0C
    // `stw r11(=4)` 0x82627CFC. 4 == rw::physics::ACTIVE_BODY (rigidbody.h:79).
    lSimulationAddBodyEvent.meState = rw::physics::ACTIVE_BODY;

    // :842. `sldi r9, r17, 32 ; clrlwi r11, r30, 16 ; or ; std` == (entityWord << 32) | (idx & 0xFFFF)
    // -- the packed CgsPhysics::RigidBodyId (high dword = the EntityId, low 16 bits = the index;
    // CgsRigidBody.h's banner and GetIndex() both say so).
    // ⚠️ HEADER REQUEST (round-2 NIT; do NOT change the expressions -- they are bit-exact):
    // `CgsPhysics::RigidBodyId::Set(EntityId, u16)` is DWARF-attested as an inlined callee of ALL
    // THREE bodies in this file, but CgsRigidBody.h has SetEntityId and GetIndex and no index
    // setter, so it could not be used. Until it lands, the packing is open-coded three times here
    // (this site plus RemovePart's and RemoveProp's); when it lands, collapse all three onto it --
    // CgsRigidBody.h:58-63 states that convention explicitly ("so the bit arithmetic lives in ONE
    // place each"). The owner tripwire at 0x82627CEC..
    // 0x82627D54 (`srwi r10,r17,24 ; cmplwi 3`, fired at BrnPropEntityID.h:278) is
    // PropEntityID::AssertIsProp() inlined -- reached here by calling GetValue(), whose committed
    // body (BrnPropEntityID.cpp:50) is exactly "AssertIsProp() then return the raw word".
    const CgsPhysics::RigidBodyId lRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropPartIndex ) ) );

    lSimulationAddBodyEvent.mID = lRigidBodyId;
    lSimulationAddBodyEvent.mRigidBody.mTransform = lTransform;

    // 0x82627D5C/0x82627D78 `lfs f0, 0x20(r31)` == PropPartTypeData::mfMass (console +0x20) then
    // `fdivs f0, f31, f0` with f31 == flt_82001C98 == 1.0f (byte-verified in the tree at
    // rw/physics/inertia.h:63).
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / lpPartType->GetMass() );

    // :914 / :845. `bl PropManager::GetPartInertia @0x82612AC8` returns Vector3 through the hidden
    // pointer in r3 (r4 = this, r5 = lpPartType) -- a normal by-value return here.
    // ⛔ GetPartInertia HAS NO BODY IN THE TREE (BrnPropManager.h declares it; its round-1 body
    //    is parked on the rw::collision::AABBox include clash). Link-red, gate-green.
    const Vector3 lInertia = GetPartInertia( lpPartType );

    // 0x82627DA4..0x82627E30. Each axis is splatted, multiplied by the WHOLE KVF_INERTIA_SCALE
    // register (`lvx128 v0, flt_82FB9400` -> vmulfp128 x3), and only LANE 0 of each product is read
    // back (`stvx` then `lfs` at offset 0) before `fdivs f0, f31(1.0f), f0`. So the scalar taken
    // from the VecFloat is its lane 0, and the result is the per-axis reciprocal of the scaled
    // inertia. The vperm/vrlimi128 at 0x82627E28/0x82627E2C is just how the compiler reassembles
    // the three scalars into one register; lane 3 of the assembled vector is a copy of one of the
    // splats and is never read.
    // ⭐ KVF_INERTIA_SCALE == Splat(3.0f), RECOVERED 2026-08-18 round 3b (thunk 0x82C5E6C0,
    //    rodata flt_82004270 = 0x40400000; provenance on the declaration in BrnPropManager.h). The
    //    warning that stood here -- "reads all-zero on disk and has no recovered initialiser ...
    //    every division below is 1.0f/0.0f" -- rested on a claim that was measured false: the disk
    //    bytes are zero, but the initialiser is a dynamic-initialiser thunk outside every IDA
    //    function. The divisions are now by 3x the raw inertia.
    Vector3 lInverseInertia;
    lInverseInertia.x = 1.0f / ( lInertia.x * KVF_INERTIA_SCALE.x );
    lInverseInertia.y = 1.0f / ( lInertia.y * KVF_INERTIA_SCALE.x );
    lInverseInertia.z = 1.0f / ( lInertia.z * KVF_INERTIA_SCALE.x );

    // 0x82627E38..0x82627E70 -- mInvTens := lInverseInertia, then
    // mSpherical := 1.0f / min(x, min(y, z)), which is rw::physics::Inertia::SetInverseInertia
    // (inertia.h:117) exactly. GOTCHA 4: both selects are `fcmpu ; blt <skip> ; fmr`, i.e.
    // `(a < b) ? a : b` with the UNORDERED case taking the `fmr` -- the committed inline spells it
    // with the same ternaries, so the NaN behaviour matches lane for lane.
    // ⚠️ ONE MEASURED DIFFERENCE, harmless: the committed inline stores the intermediate min(x,y)
    //    to +0x14 before the second compare (the console does that at the site inertia.h's banner
    //    quotes); HERE the intermediate store is optimised away and only the final `stfs` at
    //    0x82627E70 lands. Same final value, one fewer dead store. Not a reason to hand-spell the
    //    two lines instead of calling the named setter.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseInertia( lInverseInertia );

    // 0x82627EA0 `bl CgsPhysics::PhysicsSimulationIO::InputBuffer::GetAddRigidBodyQueue @0x825BCE08`
    // (the write-lock-guarded producer overload -- its "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1059 and it returns this+0x10), then
    // `bl BaseEventQueue<InAddRigidBody>::AddEvent @0x825A3000`.
    lpSimModuleInputBuffer->GetAddRigidBodyQueue()->AddEvent( lSimulationAddBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::RemovePart @ 0x8260F988   (117 instructions)
// DWARF: class decl BrnPropManager.h:328, body scope BrnPropManager.cpp:803.
//
// The part half of the retire pair: drop the part's triangle-cache slot, free its slot bit, and
// post one InRemoveRigidBody. It does NOT touch the PropPartInstance record at all -- measured:
// there is no `lwz 0x8C(this)` and no store into mpaPartInstances anywhere in the 117 instructions.
//
// ---- REGISTER MAP, prologue 0x8260F994..0x8260F9B0 ---------------------------------------------
//   r3 = this (r26)   r4 = lEntityId (r25)   r5 = luPartIndex (r29)
//   r6 = lpSceneInput (used immediately, not saved)   r7 = lpSimModuleInputBuffer (r24)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)   IsBitSet's bound tripwire, StrStream "invalid index : " << i << " < " << 30
//   BrnPropManager.cpp:811 (0x32B)  "mUsedParts.IsBitSet( luPartIndex )"
//   CgsBitArray.h:241 (0xF1)   UnSetBit's bound tripwire, "luIndex < NUMBITS"
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
// Both container tripwires compare UNSIGNED against 0x1E == 30 == KU_MAX_PHYSICAL_PROP_PARTS,
// which is also mUsedParts' capacity.
//
// ---- THE ONLY DWARF LOCAL ----------------------------------------------------------------------
// The DecFIGS scope names exactly one: `InRemoveRigidBody lSimulationRemoveBodyEvent @815`. There
// is deliberately NO named RigidBodyId local here (unlike RemoveProp, whose scope does name one) --
// the emission composes the handle straight into the record's leading qword at 0x8260FB30..0x8260FB44.
// =================================================================================================
void PropManager::RemovePart( PropEntityID                                             lEntityId,
                              u32                                                      luPartIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // 0x8260F99C..0x8260F9B8 -- FIRST thing the body does, before any assert.
    // `addi r11, r29, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + luPartIndex, written into the
    // 4-byte stack record, then `bl BaseEventQueue<InEventRemoveFromCache>::AddEvent @0x825E48C0`
    // on the queue at scene + 0xC77E0 (== &mRemoveFromCacheQueue -- reached by name; see gotcha 1).
    // Same inlined-producer situation as CreatePart's add: the DWARF's RemoveCachedObject
    // (CgsSceneManagerIO_SceneUpdate.h:488) is not declared in this tree and the console emits the
    // queue append directly. No caller-side length tripwire (measured: no compare, no other `bl`).
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
        lRemoveFromCacheEvent.miCacheSlot =
            KI_PROP_PART_CACHE_START_INDEX + static_cast<s32>( luPartIndex );

        lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );
    }

    // IsBitSet's own bound tripwire (CgsBitArray.h:203) -- see the CreatePart note for why it is
    // spelled here rather than in the container header.
    CGS_ASSERT( luPartIndex < mUsedParts.GetCapacity(), "invalid index" );
    // :811. 0x8260FA74..0x8260FAC8.
    CGS_ASSERT( mUsedParts.IsBitSet( luPartIndex ), "mUsedParts.IsBitSet( luPartIndex )" );

    // UnSetBit's bound tripwire (CgsBitArray.h:241), then the ld/andc/std at
    // 0x8260FAF4..0x8260FB08. The message is the binary's own and matches the committed spelling in
    // BrnPhysicalBodyPartPool_Remove.cpp:58.
    CGS_ASSERT( luPartIndex < mUsedParts.GetCapacity(), "luIndex < NUMBITS" );
    mUsedParts.UnSetBit( luPartIndex );

    // :815. `sldi r10, r25, 32 ; clrlwi r11, r29, 16 ; or ; std` -- same packing as CreatePart, and
    // the owner tripwire at 0x8260FAF8..0x8260FB2C is again AssertIsProp() inlined, reached through
    // GetValue().
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    lSimulationRemoveBodyEvent.mID =
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( luPartIndex ) );
    // ⚠️ The console's 16-byte stack record only ever writes the leading 8 bytes -- the
    // mbFailIfRigidBodyNotFound byte at +8 is UNINITIALISED stack on the X360 (measured: the only
    // store into var_60 is the `std` at 0x8260FB44). The host sets it FALSE == the tolerant remove,
    // which is both what ProcessRemoveRigidBodyQueue's not-found-tolerant arm does with a clear byte
    // and the committed treatment of the identical console quirk in
    // BrnPhysicalBodyPartPool_Remove.cpp:51.
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // 0x8260FB48 `bl sub_825BCF58` == InputBuffer::GetRemoveRigidBodyQueue() (the write-lock
    // producer overload; its "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1080 and it returns this + 0x196A0), then
    // `bl BaseEventQueue<InRemoveRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::RemoveProp @ 0x8260F540   (273 instructions)
// DWARF: class decl BrnPropManager.h:320, body scope BrnPropManager.cpp:760.
//
// The whole-prop retire: the same cache-evict / bit-free / InRemoveRigidBody spine as RemovePart,
// plus two things a part does not have -- an "added this frame" diagnostic and the release of the
// prop's JOINT slot (the smash-gate / signpost hinge).
//
// ---- REGISTER MAP, prologue 0x8260F54C..0x8260F568 ---------------------------------------------
//   r3 = this (r16)   r4 = lEntityId (r25)   r5 = luPropIndex (r31)
//   r6 = lpSceneInput (used immediately)     r7 = lpSimModuleInputBuffer (spilled to arg_34)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)        IsBitSet bound, "invalid index : " << i << " < " << 15  (twice:
//                                   once on luPropIndex, once on the joint index)
//   BrnPropManager.cpp:767 (0x2FF)  "mUsedProps.IsBitSet( luPropIndex )"
//   CgsBitArray.h:241 (0xF1)        UnSetBit bound, "luIndex < NUMBITS"                     (twice)
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"             (twice)
//   BrnPropManager.cpp:778 (0x30A)  the StrStream "Trying to remove prop added this frame: " message
//   BrnPropManager.cpp:783 (0x30F)  "mUsedPropJoints.IsBitSet( lpProp->GetJointIndex() )"
//   BrnPropInstance.h:326 (0x146)   "IsJointed()"  -- GetJointIndex's own tripwire, inlined
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   InRemoveRigidBody lSimulationRemoveBodyEvent @771 - RigidBodyId lPropRigidBodyId @772 -
//   PropInstance* lpProp @776 - CgsDev::StrStream lStrStream @778.
// The declaration lines settle one ordering the emission scrambles: lPropRigidBodyId (:772) is
// declared BEFORE lpProp (:776), which is why the owner tripwire fires at 0x8260F6E8 -- ahead of
// the `lwz 0x7C(this)` that computes lpProp at 0x8260F704 -- even though the `or`/`std` that packs
// the handle is scheduled after it. Source order below follows the DWARF; the two statements have
// no interdependency, so this is a scheduling difference only.
// =================================================================================================
void PropManager::RemoveProp( PropEntityID                                             lEntityId,
                              u32                                                      luPropIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // 0x8260F558..0x8260F570 -- first, exactly as in RemovePart but with the PROP slot base.
    // `addi r11, r31, 0x1C` == KI_PROP_CACHE_START_INDEX + luPropIndex.
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
        lRemoveFromCacheEvent.miCacheSlot =
            KI_PROP_CACHE_START_INDEX + static_cast<s32>( luPropIndex );

        lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );
    }

    // IsBitSet's bound tripwire (CgsBitArray.h:203, bound 0xF == 15 == mUsedProps' capacity).
    CGS_ASSERT( luPropIndex < mUsedProps.GetCapacity(), "invalid index" );
    // :767. 0x8260F634..0x8260F690.
    CGS_ASSERT( mUsedProps.IsBitSet( luPropIndex ), "mUsedProps.IsBitSet( luPropIndex )" );

    // UnSetBit's bound tripwire (CgsBitArray.h:241), then ld/andc/std at 0x8260F6C0..0x8260F6D4.
    CGS_ASSERT( luPropIndex < mUsedProps.GetCapacity(), "luIndex < NUMBITS" );
    mUsedProps.UnSetBit( luPropIndex );

    // :771 / :772. Same packing and same inlined AssertIsProp() (fired at 0x8260F6EC) as the two
    // functions above.
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    const CgsPhysics::RigidBodyId lPropRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( luPropIndex ) ) );

    lSimulationRemoveBodyEvent.mID = lPropRigidBodyId;                 // `std` 0x8260F71C
    // Same console quirk / same committed treatment as in RemovePart above: the byte at +8 is never
    // written by the console (the only store into var_B0 is that one `std`).
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // :776. `lwz r10, 0x7C(r16)` + `mulli r11, r31, 0x70` == mpaPropInstances + idx*112 (CONSOLE
    // stride); indexed by name here.
    PropInstance* lpProp = &mpaPropInstances[ luPropIndex ];

    // :778. `lbz r11, 0x6F(r27) ; clrlwi r11,r11,31` == mu8Flags & KU_ADDED_THIS_FRAME_FLAG ==
    // WasAddedThisFrame(). A DIAGNOSTIC ONLY -- the console does not skip anything on this path;
    // it falls straight through into the joint block whether or not the assert fires.
    if ( lpProp->WasAddedThisFrame() )
    {
        // Stream order, read off 0x8260F780..0x8260F7F0:
        //   << "Trying to remove prop added this frame: "   (the virtual char* sink)
        //   `stw r11(=2), stream+4`  == mePrintMode = E_PRINTMODE_HEXONCE, i.e. the inlined
        //        StrStreamBase::operator<<(PrintMode). Emitted BETWEEN the string and the id.
        //   << lpProp->GetEntityId().GetValue()            (`bl 0x821F0EC8`, the "%u"/"0x%X"
        //        u32 formatter; the owner tripwire at 0x8260F75C..0x8260F77C is GetValue's own
        //        AssertIsProp)
        //   << " Position: "
        //   << lpProp->GetTransform().Pos()                (`lvx128 v1, r27, 0x30` == the affine's
        //        translation row, then `bl 0x82203F70`)
        //   << "\n"
        //   FireAssert(gpcMessageBuffer, BrnPropManager.cpp, 778)
        //
        // ⚠️ ONE UNAVOIDABLE SPELLING CHANGE, FLAGGED. 0x82203F70 is
        //    `operator<<(StrStreamBase&, Vector3)` -- format string "(%f, %f, %f)", read out of
        //    off_82F31964 in its own body this round, and independently identified under that name
        //    by a committed file (BrnBehaviourGameplayExternal.h:375). That free operator has NO
        //    home anywhere in this tree (grepped src and vendor), and this lander may not add one.
        //    So the three lanes are pushed through StrStreamBase::AppendFormat with the console's
        //    OWN format string, which is exactly what that callee does internally -- byte-identical
        //    output, one call frame fewer. Replace this line with `<< lpProp->GetTransform().Pos()`
        //    the moment the operator is homed (reported as a header request).
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

        lStrStream << "Trying to remove prop added this frame: ";
        lStrStream << CgsDev::E_PRINTMODE_HEXONCE;
        lStrStream << lpProp->GetEntityId().GetValue();
        lStrStream << " Position: ";
        lStrStream.AppendFormat( "(%f, %f, %f)",
                                 lpProp->GetTransform().Pos().x,
                                 lpProp->GetTransform().Pos().y,
                                 lpProp->GetTransform().Pos().z );
        lStrStream << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:778
        CgsDev::Assert::EndAssert();
    }

    // 0x8260F808 `lbz r28, 0x6C(r27) ; cmplwi 0xFF ; beq -> the tail` == mu8JointIndex !=
    // KU_NOT_JOINTED == IsJointed(). Everything below runs only for a jointed prop.
    if ( lpProp->IsJointed() )
    {
        // mUsedPropJoints is the BitArray<15> at console this+0x670 (the emission reaches it as
        // `(idx>>6 + 0xCE)*8 + this` at 0x8260F8A8 and as `addi r30, r16, 0x670` at 0x8260F924 --
        // 0xCE*8 == 0x670, an arithmetic self-check on the member map). Reached by name.
        //
        // ⚠️ MEASURED RE-READ, transcribed: the console re-loads `lbz 0x6C(r27)` THREE separate
        //    times in this block (0x8260F808 for the IsJointed test, 0x8260F8F4 for GetJointIndex's
        //    own IsJointed tripwire, 0x8260F920 for the value it un-sets with), and it emits the
        //    BrnPropInstance.h:326 "IsJointed()" tripwire exactly ONCE (0x8260F900). GetJointIndex()
        //    is called FOUR times below (round-2 NIT: the earlier comment said "twice" and the code
        //    beneath it always had four). Two of the four are this codebase's own CALLER-SIDE
        //    CgsBitArray bound tripwires (CgsBitArray.h:203/:241), which the console open-codes from
        //    the already-loaded value instead of re-calling; the DWARF attests one inlined
        //    PropInstance::GetJointIndex in this body. So the reconstruction can emit that assert up
        //    to four times where the console emits it once -- all provably unreachable (const getter,
        //    the whole block is guarded by IsJointed()), which is why the spelling stands.
        //    GetJointIndex()'s committed body (BrnPropInstance.cpp:66) carries that same assert.
        CGS_ASSERT( static_cast<u32>( lpProp->GetJointIndex() ) < mUsedPropJoints.GetCapacity(),
                    "invalid index" );                                    // CgsBitArray.h:203
        // :783.
        CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( lpProp->GetJointIndex() ) ),
                    "mUsedPropJoints.IsBitSet( lpProp->GetJointIndex() )" );

        CGS_ASSERT( static_cast<u32>( lpProp->GetJointIndex() ) < mUsedPropJoints.GetCapacity(),
                    "luIndex < NUMBITS" );                                // CgsBitArray.h:241
        // 0x8260F948..0x8260F960 -- `rlwinm r11, r31, 29,3,28` is (idx>>6)*8, the field index.
        mUsedPropJoints.UnSetBit( static_cast<u32>( lpProp->GetJointIndex() ) );

        // 0x8260F964 `stb r9(=0xFF), 0x6C(r27)` == mu8JointIndex = KU_NOT_JOINTED.
        lpProp->SetNotJointed();
    }

    // 0x8260F968..0x8260F978 -- same producer pair as RemovePart.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );
}

}
}
