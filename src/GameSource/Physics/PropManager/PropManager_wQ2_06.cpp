// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_06.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, lander 06, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// THREE FUNCTIONS WERE ASSIGNED. TWO LAND HERE; ONE IS PARKED ON A MISSING DECLARATION:
//
//   * PropManager::Prepare                       @0x8260EE18 (126 insns)  -- BODIED BELOW.
//   * PropManager::ProcessAddPropInstanceEvents  @0x82632108 (515 insns)  -- BODIED BELOW.
//   * PropManager::OutputUpdatedProps            @0x82627EC8 (14 insns)   -- PARKED, complete body at
//         scratchpad/waveQ2/parked/PropManager_06_OutputUpdatedProps.cpp
//         Blocker: PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface() still returns the
//         opaque placeholder `struct PropOutputInterfaceStorage { unsigned char maBytes[1]; }`
//         (BrnPhysicsModuleIO.h:76). RE-CHECKED THIS ROUND rather than trusted from the round-2
//         owner report -- the placeholder is still there, verbatim, at that line, and my own
//         negative probe (scratchpad/waveQ2/probe_wQ2_06/probe_outputupdatedprops.cpp) fails with
//         exactly one diagnostic: C2440 "PropOutputInterfaceStorage* cannot be converted to
//         BrnPhysics::Props::PropOutputInterface*" at the accessor's return. (The round-2 owner's
//         probe reported a C2039 instead because it called AppendUpdatedProps directly on the
//         storage type; same blocker, different spelling of the same missing promotion.)
//         ⚠️ The GetPropManagerOutputInterface note in BrnPropManager.h claims "Both callees are
//         HOMED ... so this one has no blocker
//         at all". That claim is STALE/WRONG: GetPropManagerOutputInterface is homed, but its
//         RETURN TYPE is not the real PropOutputInterface. Not corrected from here -- this lander
//         may not edit headers; filed as a header request.
//         ⚠⚠ ARITHMETIC THE PROMOTION COMMIT MUST GET RIGHT (round-2 NIT, restated here because the
//         parked banner's wording implies a surviving pad): the seat at BrnPhysicsModuleIO.h:138 is
//         followed by `unsigned char maDeformationPad[148656 - 71793]`, the 71793 being
//         `71792 + sizeof(the 1-byte placeholder)`. 148656 - 71792 == 76864 EXACTLY, and 76864 is
//         the host sizeof the parked banner computes for the promoted PropOutputInterface
//         (38416 + 3216 + 22416 + 12816). So the re-derived pad
//         `148656 - (71792 + sizeof(PropOutputInterface))` evaluates to ZERO -- a zero-length array,
//         which ISO C++ forbids and MSVC accepts only as an extension. The correct edit is to
//         DELETE maDeformationPad and let _AssertLayout re-pin mDeformationOutputInterface at
//         +148656 directly, not to recompute a pad that no longer exists.
//
// ⚠️ A PRIOR, INTERRUPTED RUN of this same lander wrote the banner that used to sit here. It
//    claimed ProcessAddPropInstanceEvents was blocked on
//    `InSceneUpdateInterface::AddCachedObject(s32, f32)` being undeclared, and pointed at two
//    parked files that were NEVER WRITTEN. Re-derived from the raw asm this round (AGENTS.md
//    gotcha 10): the blocker was misidentified. `AddCachedObject` is the SOURCE-level helper the
//    DecFIGS DWARF names, but the ARTIST build INLINES it -- the emission at 0x826325E0..0x82632604
//    is a two-word event fill followed by a direct
//    `bl CgsModule::BaseEventQueue<...InEventAddToCache>::AddEvent`, with the queue pointer formed
//    as `addis r11, r5, 0xC ; addi r11, r11, 0x4930` == lpSceneInput + 0xC4930 ==
//    InSceneUpdateInterface::mAddToCacheQueue (CgsSceneManagerIO_SceneUpdate.h pins that exact
//    console offset on that exact member). That inlined form is spelled below; both the queue
//    member and EventQueue::AddEvent are public and homed, so nothing is blocked. The undeclared
//    `AddCachedObject` helper is filed as a header request, not a blocker.
//
// ⚠️ LINK-LEVEL DUPLICATES -- `cl /c` cannot see either of these; REPORTED, NOT REMOVED (AGENTS.md
//    gotcha 7: a gate is retired only in the same commit that MOUNTS the real body in
//    tools/build/build_game_exe.bat, and that script lists neither this file nor its siblings):
//      * PropManager::Prepare has an inert named LINK STUB at
//        b5-decomp/src/GameSource/World/WorldLinkStubs.cpp:516 (`bool Prepare(struct
//        rw::IResourceAllocator*)`, logs "PropManager::Prepare: inert [FLAG PC boot gate]" and
//        returns true). The real body below is an LNK2005 against it the instant this file is
//        mounted. Retire the stub in the mounting commit.
//      * PropManager::OutputUpdatedProps has an inert one-shot gate at
//        b5-decomp/src/GameSource/Physics/BrnPhysicsConductorGates.cpp:507. Nothing to retire yet
//        (its body is parked, not landed) -- listed so the two land together.
//
// ✅ EVERY CALLEE REACHED FROM THIS FILE HAS A BODY IN THE TREE (round-2 MUST_FIX, applied;
//    re-grepped 2026-08-18). The earlier banner said `PropManager::AddPropToSim` @0x826274D8 was
//    "declared with NO body anywhere in b5-decomp/src or b5-decomp/vendor" -- that was FALSE the day
//    it was written: AddPropToSim is fully bodied by the sibling wave-Q round-2 lander at
//    b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_05.cpp, with a signature matching
//    this file's call site parameter for parameter. Do NOT add a trap stub for it beside that body
//    (an LNK2005 `cl /c` cannot see), and do not hold this file's mount for it.
//    The only LINK-level items here are the two listed above: the WorldLinkStubs.cpp:516 Prepare
//    stub (a real duplicate to retire at mount time) and the OutputUpdatedProps gate.
//    Every OTHER callee was checked and is real: PropInstance::SetTransform / ::SetLinearVelocity
//    (BrnPropInstance.cpp:24/:56), CgsDev::DebugComponent::Register (CgsDebugComponent.cpp:66),
//    rw::BaseResourceDescriptor::BaseResourceDescriptor (vendor .../BaseResourceDescriptor.cpp:35),
//    rw::IResourceAllocator::DoAllocate (vendor rwcore_alloc.cpp:12), and the rest are header
//    inlines (the BitArray<15> methods, the BaseEventQueue methods, every PropInstance accessor
//    used here, PropPhysicsDataHeader::GetType, the PropTypeData accessors,
//    CgsResource::ResourcePtr<T>::operator->, rw::math::vpu::TransformPoint / Vector3::SetZero).
//
// GROUNDING. Every statement below was re-derived THIS ROUND from the raw `assembly` arrays of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260EE18.json and /0x82632108.json. The Hex-Rays
// pseudocode in those exports was read only as a cross-check and is wrong in the usual ways (for
// Prepare it prints the two virtual allocator calls as `MEMORY[0x6A0]` / `MEMORY[0x790]` and
// mislabels the parameter list). Every instruction count stated in this file is a count I made of
// the 0xXXXXXXXX-prefixed lines in those listings: Prepare is 126 ((0x8260F00C - 0x8260EE18)/4 + 1)
// and ProcessAddPropInstanceEvents is 515 ((0x82632910 - 0x82632108)/4 + 1).
//
// ⚠️ EVERY ASSERT MESSAGE QUOTED IN THIS FILE WAS READ OUT OF THE IMAGE, not reconstructed from the
// IDA comment (which truncates at ~40 characters). Dumped with headless IDA 9.3 against
// "IDA Files/BURNOUT_X360_ARTIST.XEX.i64" -- e.g. aLipropinstance_0 @0x8209BB60 ==
// "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)", aLpaddpropevent @0x8209BBA8
// == "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)". Note the binary's
// own text has NO space inside the static_cast<> parentheses; that spelling is preserved verbatim.
//
// ⚠️ NO CONSOLE LITERAL IS USED AS A HOST VALUE (AGENTS.md gotcha 1). The two allocation sizes the
// console spells as the immediates 0x690 and 0x780 are `KU_MAX_PHYSICAL_PROPS * sizeof(PropInstance)`
// and `KU_MAX_PHYSICAL_PROP_PARTS * sizeof(PropPartInstance)` here; both structs are pointer-free
// so the host sizes happen to agree (112 and 64), and the static_asserts below PIN that agreement
// instead of leaving it as a coincidence a later member addition would silently break.
// The one bare immediate kept is the descriptor ALIGNMENT 0x10, which is an alignment and not a
// size -- it is 16 on both targets because both records lead with a 16-byte SIMD member.
//
// GOTCHA 3 / GOTCHA 4 SITES: NONE IN EITHER BODY, and that is a measurement, not an assumption.
//   * Prepare's emission contains no float or vector register at all (nothing touches f1..f13 or
//     v0..v127 anywhere in the 126 instructions) and no floating-point compare -- so there is no
//     PPC float-arg GPR-skip and no NaN-polarity decision.
//   * ProcessAddPropInstanceEvents takes no float or vector PARAMETER (r3..r6 carry this + the
//     three pointers; v127 is a locally-materialised zero, not an argument), and its only
//     floating-point traffic is one `lfs f0, 0x44(lpType)` / `stfs f0` pair -- a copy, with no
//     fcmpu, no fsel and no vcmp anywhere in the 515 instructions. Its `bge`/`ble` branches are
//     all on integer condition registers (cmpwi/cmplwi/cmpldi), where gotcha 4 does not apply.
// GOTCHA 2 SITE (a store through obj+0xNNNN that targets an embedded sub-object): checked and
// none. Every this-relative store in both bodies lands on a member PropManager declares directly
// (mUsedProps/mUsedParts/mpaPropInstances/mpaPartInstances/maPropJointPositions/
// maLastJointRotation/mauPropIndexForJoint/maCurrentJointTransforms/mUsedPropJoints/
// mBreakPropJoints); the only embedded sub-object, mDebugComponent, is at +0x00 and is reached
// exactly once, by name, in Prepare.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"      // PropInstance (sizeof + accessors)
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"  // PropPartInstance (sizeof)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"           // AddPhysicalPropEvent
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"   // KU_MAX_PHYSICAL_PROPS / _PROP_PARTS
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"            // PropPhysicsDataHeader::GetType
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"              // PropTypeData accessors
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"     // PhysicsSimulationIO::InputBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"          // InSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // InEventAddToCache
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // gpDebugPrint / gxMessageFilterFlags
#include "rw/rwcore_structs.h"                                               // rw::IResourceAllocator / Resource / BaseResourceDescriptors
#include "rw/math/vpu/matrix44affine_operation.h"                            // rw::math::vpu::TransformPoint

namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// BrnPhysics::Props::PropManager::Prepare @ 0x8260EE18  (126 instructions)
//
// Stage 5 (E_PREPARESTAGE_PROPMANAGER) of BrnPhysics::PhysicsModule::Prepare @0x825ADB68 -- its
// only caller (xrefs_to is exactly {0x825ADB68}). Clears the two slot bit-sets, registers the debug
// component, and carves the two instance arrays out of the physics resource allocator.
//
// ⚠️ THE ADDRESS. The class declaration used to carry "@0x82C08ED0" and the round-2 shared-header
//    owner corrected it; I re-verified rather than inherited the correction. 0x82C08ED0 is
//    `__savegprlr_22`, the PPC register-save runtime helper -- it appears in THIS function's
//    xrefs_from precisely because `bl __savegprlr_22` is its second instruction (0x8260EE1C).
//    0x8260EE18 is the real body: its own export is named
//    BrnPhysics::Props::PropManager::Prepare and its xrefs_to is exactly PhysicsModule::Prepare.
//
// ---- REGISTER MAP, read off the prologue (0x8260EE18..0x8260EE2C) --------------------------------
//     r3 = this            -> r30
//     r4 = lpPhysicsAllocator -> r28
//     r29 = 0 (the shared zero), r25 = 1, r24 = 0x10 (the descriptor alignment, hoisted)
// No third parameter; nothing reads r5 before it is written.
//
// ---- PARAMETER TYPE: a KNOWN, DELIBERATE DELTA --------------------------------------------------
// The DecFIGS DWARF declares `void PropManager::Prepare(rw::LinearResourceAllocator* lpPhysicsAllocator)`
// (dwarfdump .../BrnPropManager.cpp, source :174) and the one real call site,
// BrnPhysicsModule.cpp:369, already hands over a rw::LinearResourceAllocator*. The committed class
// declaration takes the base rw::IResourceAllocator*, because narrowing it is a PAIR edit -- the
// inert stub at WorldLinkStubs.cpp:516 spells `struct rw::IResourceAllocator *` and stops compiling
// the moment the declaration alone changes. That file is not this partfile's to edit, so the
// definition below matches the COMMITTED declaration and the narrowing stays conductor item C2.
// It is behaviour-neutral either way: the body only ever reaches the allocator through the virtual
// DoAllocate slot, which both types share.
//
// ---- WHAT THE BODY DOES, instruction by instruction ---------------------------------------------
//   0x8260EE30  std r29, 0x80(r30)      mUsedProps  = 0     (BitArray<15>, ONE 64-bit field)
//   0x8260EE34  std r29, 0x90(r30)      mUsedParts  = 0     (BitArray<30>, ONE 64-bit field)
//   0x8260EE38  bl DebugComponent::Register   with r3 STILL == this; mDebugComponent is at +0x00
//                                       (PropManager::Construct @0x82627390 calls
//                                        PropDebugComponent::Construct with r3 == r4 == this), so
//                                       the console really is calling mDebugComponent.Register().
//   0x8260EE44..0x8260EE98  the FIRST log line, gated on `gxMessageFilterFlags & 1`
//                                       (`ld` + `clrldi ...,63` == the 64-bit flag word & 1):
//                                       op<<(const char*) through the stream vtable slot +4, then
//                                       the non-virtual op<<(s32) with r4 = 0x690, then
//                                       op<<(const char*) " bytes\n".
//   0x8260EEA0..0x8260EEBC  the descriptor's identity fill: r10 counts 4,3,2,1,0 (`bge` on the
//                                       POST-decrement value, so FIVE iterations) writing the pair
//                                       {size = 0, alignment = 1} at an 8-byte stride == the
//                                       rw::BaseResourceDescriptor default constructor run over a
//                                       FIVE-entry descriptor. See the <5>-vs-<4> note below.
//   0x8260EED0..0x8260EEEC  entry[0] = { size = 0x690, alignment = 0x10 }  (assembled as two
//                                       stw into a scratch qword then one std into the descriptor)
//   0x8260EEF0..0x8260EEF8  bctrl through allocator vtable slot +0x10 with r3 = &<sret Resource>,
//                                       r4 = allocator, r5 = &descriptor, r6 = "PropInstances"
//                                       -- the (const ResourceDescriptor&, const char*) -> Resource
//                                       shape, i.e. DoAllocate.
//   0x8260EEFC..0x8260EF04  mpaPropInstances (+0x7C) = the Resource's FIRST pool pointer; the
//                                       null test is taken from the same register BEFORE the store
//                                       (`cmplwi` at 0x8260EF00, `stw` at 0x8260EF04), so the store
//                                       happens unconditionally and the assert follows it.
//   0x8260EF14..0x8260EF2C  assert "mpaPropInstances != NULL", BrnPropManager.cpp:205 (0xCD)
//   0x8260EF30..0x8260F000  the identical second half for the parts array: "PropManager: array of
//                                       prop parts required " / 0x780 / "PropPartInstances" /
//                                       mpaPartInstances (+0x8C) / assert :223 (0xDF)
//   0x8260F004  li r3, 1                return true, unconditionally -- there is no other write to
//                                       r3 after the last bctrl, so no failure path exists.
//
// ---- MEASURED NEGATIVES, written down so nothing gets invented later ----------------------------
//  * Prepare stores to EXACTLY FOUR `this` offsets and no others -- +0x80, +0x90, +0x7C, +0x8C. I
//    re-grepped all 126 instructions for `st*` with an r30 base: those four and nothing else. In
//    particular it does NOT write +0x88 muNumberOfPropInstances or +0x98 muNumberOfPartInstances,
//    which is why the round-1 note that Release()/Destruct() loop over an unwritten counter stands.
//    Nothing is invented here to "fix" that: the console does not set them, so neither do we.
//  * The DecFIGS scope for this function (dwarfdump .../BrnPropManager.cpp, the Prepare block) lists
//    BitArray<15u>::UnSetAll, BitArray<30u>::UnSetAll, StrStreamBase::operator<<,
//    rw::ResourceDescriptor::ResourceDescriptor x2, rw::IResourceAllocator::Allocate x2 and the two
//    ~ResourceDescriptor -- and does NOT list DebugComponent::Register. The ARTIST asm has the `bl`
//    explicitly at 0x8260EE38, so the call is real in the SHIPPED build; the DWARF gap is a
//    DecFIGS/ARTIST merge-window delta, not evidence against it. Stated, not smoothed over.
//  * The DWARF spells the allocator method `Allocate`; the tree's committed name for that vtable
//    slot -- the one every sibling Prepare in this engine calls (CgsTriangleCacheManager.cpp:77,
//    CgsSceneManagerModule.cpp:150, CgsCachedTriangleList.cpp:121) -- is `DoAllocate`. Same slot,
//    same (descriptor, name) -> Resource signature; the tree name is used, not a second one forked.
//
// ---- THE <5>-vs-<4> RESOURCE DESCRIPTOR ---------------------------------------------------------
// The console's rw::ResourceDescriptor is rw::BaseResourceDescriptors<5> (40 bytes -- hence the FIVE
// identity pairs the loop writes); the PC rwcore this tree is built against uses <4>
// (rwcore_structs.h:76, and the divergence is documented in that header's own banner at :30). The
// committed sibling CgsSceneManager::TriangleCacheManager::Prepare hit exactly this and resolved it
// by declaring the <5> form and reinterpret_cast-ing it down to DoAllocate's <4> parameter; the same
// resolution is used here so the two prop descriptors have the console's shape. The cast is safe in
// this direction (the callee reads four of the five entries) and only entry 0 is ever non-identity.
// =================================================================================================
bool PropManager::Prepare( rw::IResourceAllocator* lpPhysicsAllocator )
{
    // 0x8260EE30 / 0x8260EE34. Two 64-bit zero stores == one UnSetAll each (both arrays are a
    // single 64-bit field: ceil(15/64) == ceil(30/64) == 1). DWARF names both calls.
    mUsedProps.UnSetAll();
    mUsedParts.UnSetAll();

    // 0x8260EE38. r3 is untouched between the prologue and this `bl`, and mDebugComponent sits at
    // +0x00, so `this` and `&mDebugComponent` are the same address -- the console is calling the
    // component's own Register(). (See the "measured negatives" note above about the DWARF gap.)
    mDebugComponent.Register();

    // ---------------------------------------------------------------------------------------
    // The prop instance array. Console immediate 0x690 == 15 * 112; expressed here as the host
    // arithmetic so a future member addition moves the allocation with the struct.
    // ---------------------------------------------------------------------------------------
    static_assert( sizeof( PropInstance ) == 112,
                   "PropInstance host stride must stay 112 == the console's 0x690/15" );

    const s32 liPropBufferSize =
        static_cast<s32>( KU_MAX_PHYSICAL_PROPS * sizeof( PropInstance ) );   // console `li r4, 0x690`

    // 0x8260EE44..0x8260EE98. The whole three-term chain is inside the filter test; the console
    // does not even load gpDebugPrint when the bit is clear.
    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
    {
        *CgsDev::Log::gpDebugPrint << "PropManager: array of props required "
                                   << liPropBufferSize
                                   << " bytes\n";
    }

    // 0x8260EEA0..0x8260EEEC. Default construction gives every entry the identity {0, 1} pair the
    // console's five-iteration loop writes; only entry 0 is then filled in.
    rw::BaseResourceDescriptors<5> lPropsResDesc;
    lPropsResDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>( liPropBufferSize );
    lPropsResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;   // ALIGNMENT, not a size: 16 on both targets

    // 0x8260EEF0..0x8260EEF8. Virtual dispatch through the allocator; the Resource comes back by
    // value (the console passes the sret pointer in r3) and pool 0 is the array base.
    rw::Resource lPropsRes = lpPhysicsAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>( lPropsResDesc ), "PropInstances" );
    mpaPropInstances = static_cast<PropInstance*>( lPropsRes.m_baseResources[0] );

    CGS_ASSERT( mpaPropInstances != NULL, "mpaPropInstances != NULL" );   // BrnPropManager.cpp:205

    // ---------------------------------------------------------------------------------------
    // The prop-part instance array. Console immediate 0x780 == 30 * 64.
    // ---------------------------------------------------------------------------------------
    static_assert( sizeof( PropPartInstance ) == 64,
                   "PropPartInstance host stride must stay 64 == the console's 0x780/30" );

    const s32 liPartBufferSize =
        static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS * sizeof( PropPartInstance ) );  // console `li r4, 0x780`

    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
    {
        *CgsDev::Log::gpDebugPrint << "PropManager: array of prop parts required "
                                   << liPartBufferSize
                                   << " bytes\n";
    }

    rw::BaseResourceDescriptors<5> lPropPartsResDesc;
    lPropPartsResDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>( liPartBufferSize );
    lPropPartsResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;

    rw::Resource lPropPartsRes = lpPhysicsAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>( lPropPartsResDesc ), "PropPartInstances" );
    mpaPartInstances = static_cast<PropPartInstance*>( lPropPartsRes.m_baseResources[0] );

    CGS_ASSERT( mpaPartInstances != NULL, "mpaPartInstances != NULL" );   // BrnPropManager.cpp:223

    // 0x8260F004  li r3, 1 -- unconditional.
    return true;
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessAddPropInstanceEvents @ 0x82632108  (515 instructions)
//
// Drain PropInputInterface::mAddPropQueue: every AddPhysicalPropEvent promotes one prop entity to a
// live PropInstance, registers its collision sphere with the scene's triangle cache, allocates a
// joint slot when the prop is a LEANING (lamppost-class) prop, and finally hands the whole thing to
// AddPropToSim, which posts the InAddRigidBody. Its only caller is
// ProcessInputsPreScene @0x8263AF30 (xrefs_to is exactly that one address).
//
// ---- REGISTER MAP, read off the prologue (0x82632108..0x82632134) -------------------------------
//     r3 = this               -> r21
//     r4 = lpInput            -> spilled at arg_1C and used DIRECTLY as the queue pointer:
//                                `mr r3,r4 ; lwz r11,8(r3)` == GetLength() at queue+8, i.e.
//                                mAddPropQueue sits at PropInputInterface+0 (the header pins it).
//     r5 = lpSceneInput       -> `addis r11,r5,0xC ; addi r11,r11,0x4930` == +0xC4930 ==
//                                InSceneUpdateInterface::mAddToCacheQueue
//     r6 = lpSimModuleInputBuffer -> spilled at arg_2C, reloaded only to feed AddPropToSim's r10
// No fourth parameter; nothing reads r7+ before it is written.
//
// ---- THE DecFIGS SCOPE (dwarfdump .../BrnPropManager.cpp, source :470) --------------------------
// Locals, verbatim, with their source lines -- these names are used below rather than invented:
//   int32_t luIndex :472 - const PropInputInterface::AddPhysicalPropEventQueue* lpAddPropEventQueue
//   :473 - const AddPhysicalPropEvent* lpAddPropEvent :474 - int32_t liPropInstanceIndex :475 -
//   PropInstance* lpProp :476 - RigidBodyId lRigidBodyId :477 - const PropTypeData* lpType :478 -
//   bool lbStatic :514 - bool lbJointed :515 - int32_t liLampostJointIndex :521
//
// ⚠️ `RigidBodyId lRigidBodyId` (:477) IS DEAD IN THIS BUILD AND NOTHING IS SYNTHESISED FOR IT.
//    Negative evidence, measured: the export's xrefs_from lists exactly twelve targets --
//    __savegprlr_14, BaseEventQueue<AddPhysicalPropEvent>::GetEvent (sub_825BC520),
//    BeginAssert/FireAssert/EndAssert, BasePriorityQueue::Clear, StrStreamBase::AppendFormat,
//    PropInstance::SetTransform, PropInstance::SetLinearVelocity,
//    ResourcePtr<PropPhysicsDataHeader>::operator-> (0x822868E0, the truncated
//    "BrnPhysics::Props::Prop" symbol), BaseEventQueue<InEventAddToCache>::AddEvent, and
//    PropManager::AddPropToSim. There is no RigidBodyId::Set, no GetAddRigidBodyQueue, and no
//    register in the 515 instructions ever holds one -- the rigid body is built entirely inside
//    AddPropToSim. This is the same dead-local pattern round 1 recorded for the sibling
//    ProcessRemovePartInstanceEvents; written down so a later DWARF sweep does not "restore" it.
//
// ---- SOURCE LINES THE ASSERTS BAKE (file aDP4B5MainBurno_225 == this .cpp) ----------------------
//   0x1EE == 494  "liPropInstanceIndex != -1"
//   0x1EF == 495  "!mUsedProps.IsBitSet( liPropInstanceIndex )"
//   0x1F3 == 499  "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)"
//   0x1FC == 508  "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)"
//   0x20A == 522  "liLampostJointIndex != -1"
//
// ---- FOUR ASSERT BLOCKS ARE DELIBERATELY NOT REPRODUCED -----------------------------------------
// The emission also carries four complete StrStream assert blocks whose baked FILE is
// aDP4B5MainBurno_51 == ".../gameshared/gameclasses/containers/./CgsBitArray.h", at its lines
// 0xCB == 203 ("invalid index : " << i << " < " << 15), 0xDE == 222 ("Index: " << i <<
// ", Number of bits: " << 15) twice, and 0xF1 == 241 ("luIndex < NUMBITS"). Those are the INLINED
// bodies of BitArray<15>::IsBitSet / ::SetBit / ::UnSetBit -- the callee's tripwires, not this
// function's source statements -- and the tree's CgsBitArray.h states in its own banner (:15-17)
// that it deliberately carries no assert-system dependency. Their bound (15) is identical to the
// :499 assert this body does reproduce, so nothing is lost. Named here so the ~150-instruction
// gaps at 0x82632280..0x8263237C, 0x826323D4..0x826324D0 and 0x82632770..0x8263287C are not read
// as dropped branches.
//
// ---- WHAT THE BODY DOES, in emission order ------------------------------------------------------
//   0x8263212C  lwz r11,8(queue) ; cmpwi 0 ; ble -> exit         GetLength() <= 0, SIGNED
//   0x82632144  vspltisw128 v127,0                               ONE zero vector, hoisted OUT of
//                                                                the loop (after the early-out)
//   0x82632244  bl sub_825BC520                                  GetEvent(luIndex); stride-80
//                                                                (`slwi r11,r30,2 ; add ; slwi 4`)
//   0x8263224C  lhz 0x4A / extsh                                 liPropInstanceIndex = miSlot (s16)
//   0x82632254..0x826323CC   the :494 and :495 asserts
//   0x826324D0  ldx/or/stdx on this+0x80                         mUsedProps.SetBit(...)
//   0x826324D8..0x82632500   the :499 assert (cmpwi 0xF -- SIGNED)
//   0x82632504  lwz 0x7C(this) ; mulli r22,0x70                  &mpaPropInstances[index];
//                                                                0x70 == 112 is the CONSOLE stride
//                                                                and is NOT used here -- the host
//                                                                sizeof does the striding.
//   0x8263251C..0x82632560   the instance seed (see the block comment inside the body)
//   0x82632564..0x82632588   the :508 assert
//   0x8263258C..0x826325F8   ResourcePtr operator-> + the inlined PropPhysicsDataHeader::GetType
//                            (its own two asserts bake BrnPropPhysicsDataHeader.h:173/:174, which
//                            the tree's header-inline GetType reproduces for free)
//   0x826325E0..0x82632604   the inlined AddCachedObject -> BaseEventQueue<InEventAddToCache>::
//                            AddEvent. MEASURED: the radius is copied straight through
//                            (`lfs f0,0x44(lpType)` -> `stfs f0`), with NO cache padding added --
//                            unlike UpdateTriangleCache, which adds KF_TRIANGLE_CACHE_PADDING.
//                            There is no `fadds` anywhere between 0x826325FC and 0x82632604.
//   0x82632608..0x8263261C   meState -> lbJointed (== 2) / lbStatic (== 1)
//   0x82632620..0x82632894   the joint block (taken only when lbJointed)
//   0x82632898..0x826328E4   SetIsStatic / SetExtraComOffsetFlag / AddPropToSim
//   0x826328E8..0x82632900   ++luIndex ; RE-READ the length ; cmpw ; blt -> loop
//
// ⚠️ THE LOOP BOUND IS RE-READ EVERY ITERATION, and that is a real difference from the sibling
//    ProcessAddPartInstanceEvents (which hoists it). 0x826328EC/0x826328F4 reload the spilled
//    queue pointer and `lwz r10,8(r10)` fresh, then `cmpw` (SIGNED) against the incremented
//    counter. Written as `luIndex < lpAddPropEventQueue->GetLength()` in the for-condition so the
//    re-read survives; do not "optimise" it into a hoisted local.
//
// ---- CONSOLE LITERALS THAT ARE COMMENTS ONLY (AGENTS.md gotcha 1) --------------------------------
// 0xC4930 (the mAddToCacheQueue byte offset), 0x70 (the PropInstance stride), 0x40/0x44/0x48/0x4A/
// 0x4C (the AddPhysicalPropEvent field offsets), 0x60/0x64/0x68/0x6C..0x6F (the PropInstance field
// offsets), 0x2A0/0x2B0/0xC0/0x1B0/0x670/0x678 (the joint-block member offsets) and the *8/*16/*64
// strides all appear ONLY in comments. Every one of them is reached by member name below, so the
// x64 pointer widening cannot desync any of them. The only NUMBERS in the code are the two bounds
// constants the assert strings themselves name (KU_MAX_PHYSICAL_PROPS, KU_MAX_PROP_TYPES) and
// KI_PROP_CACHE_START_INDEX -- all three are named constants, not offsets.
// =================================================================================================

// DWARF BrnTriangleCacheConstants.h:36. Same local re-declaration (and same reason) as the
// committed siblings PropManager_wQ2_02.cpp and PropManager_wQ2_04.cpp (each in its own
// triangle-cache-constants banner): that header has no
// committed home in the tree yet, so the constant is spelled locally rather than a new shared
// header being invented for it. Attested here by `addi r10, r22, 0x1C` at 0x826325E8.
// ⚠️ FOLD-TIME REDEFINITION HAZARD (round-2 NIT): this is the THIRD file-local copy of the same
// name at the same scope in this directory, and all three part-file banners say they fold back into
// BrnPropManager.cpp -- after that fold one TU would carry three definitions of it (a C2374 that
// `cl /c` cannot see today, since internal linkage keeps one per TU legal). Whoever folds second
// drops its copy, or all three switch to BrnTriangleCacheConstants.h in the same commit; the
// standing header request for that file must therefore list all three sites, not two.
static const s32 KI_PROP_CACHE_START_INDEX = 28;

void PropManager::ProcessAddPropInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::AddPhysicalPropEventQueue* lpAddPropEventQueue =
        &lpInput->GetAddPhysicalPropQueue();

    // ONE zero vector, materialised once: `vspltisw128 v127, 0` at 0x82632144 sits OUTSIDE the
    // loop (and after the early-out). It is consumed FIVE times per non-jointed iteration and SIX
    // when the joint block is taken (round-2 NIT: the count used to say five flat) -- twice as a
    // direct `stvx128` into the instance's two velocity members (0x82632520 / 0x8263253C, which is
    // the folded Vector3::SetZero pair), three times as a `vmr128` argument (v1 for
    // SetLinearVelocity at 0x82632558, v1/v2 for AddPropToSim at 0x826328BC/0x826328C4), and once
    // more in the JOINTED path as `stvx128 v127, r6, r21` @0x8263273C, the maLastJointRotation
    // clear (spelled below as SetZero).
    // vspltisw zeroes ALL FOUR lanes, w included, which is exactly Vector3::SetZero's semantics.
    // ⚠️ AUTHORED NAME: unlike the sibling ProcessAddPartInstanceEvents (whose DWARF scope names a
    // `Vector3 lVelocity`), THIS function's DecFIGS scope names no Vector3 local at all, so the
    // identifier below is this reconstruction's spelling of the hoisted register, not a recovered
    // name. The hoist itself is measured.
    Vector3 lZeroVelocity;
    lZeroVelocity.SetZero();

    for ( s32 luIndex = 0; luIndex < lpAddPropEventQueue->GetLength(); ++luIndex )
    {
        const AddPhysicalPropEvent* lpAddPropEvent = &lpAddPropEventQueue->GetEvent( luIndex );

        // 0x8263224C `lhz 0x4A(event)` + `extsh` -- the s16 slot sign-extended, which is why the
        // -1 test below is the SIGNED cmpwi and not a cmplwi.
        const s32 liPropInstanceIndex = lpAddPropEvent->miSlot;

        CGS_ASSERT( liPropInstanceIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropInstanceIndex != -1" );                                    // :494
        CGS_ASSERT( !mUsedProps.IsBitSet( static_cast<u32>( liPropInstanceIndex ) ),
                    "!mUsedProps.IsBitSet( liPropInstanceIndex )" );                   // :495

        mUsedProps.SetBit( static_cast<u32>( liPropInstanceIndex ) );

        CGS_ASSERT( liPropInstanceIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                    "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)" );  // :499

        PropInstance* lpProp = &mpaPropInstances[liPropInstanceIndex];

        // -----------------------------------------------------------------------------------
        // The instance seed, 0x8263251C..0x82632560. Ten stores plus two out-of-line calls.
        //
        // ⚠️ SOURCE SHAPE, STATED HONESTLY. The DecFIGS callee list for this function names
        //    `PropInstance::Construct()` -- declared with NO parameters (dwarfdump
        //    .../PropPhysics/BrnPropInstance.h:69) -- immediately before the two
        //    `rw::math::vpu::Vector3::SetZero` entries, so the original source almost certainly
        //    wrote `lpProp->Construct();` here and the compiler inlined it. TWO things stop that
        //    call being written: (a) the tree's PropInstance::Construct is DECLARATION-ONLY with
        //    no body anywhere (BrnPropInstance.h says so in its own banner), so calling it would
        //    open a link hole against a body this site cannot recover; and (b) the split is not
        //    recoverable from here anyway -- the DWARF callee list contains SetTypeId but NOT
        //    SetEntityId, even though the `stw` to mEntityId (+0x60) is plainly in the emission,
        //    so at least one of these stores is attributed to a callee the list does not name.
        //    The MEASURED stores are therefore written out by name. Observable behaviour is
        //    identical either way; only the source-level factoring is lost, and it is flagged
        //    rather than guessed.
        // -----------------------------------------------------------------------------------
        lpProp->SetInstanceID( 0 );                                       // 0x8263251C stw 0,+0x68
        lpProp->mLinearVelocity.SetZero();                                // 0x82632520 stvx +0x40
        lpProp->SetNotJointed();                                          // 0x82632534 stb 0xFF,+0x6C
        lpProp->SetIsStatic( false );                                     // 0x82632528 stb 0,+0x6D
        lpProp->SetMovementState( E_PROP_MOVESTATE_STATIONARY );          // 0x8263252C stb 0,+0x6E
        // 0x82632530 `stb r15(==1), 0x6F`. An assignment, not an OR: the flag word starts as
        // exactly KU_ADDED_THIS_FRAME_FLAG, and the extra-COM bit is written further down by
        // SetExtraComOffsetFlag. There is no set-side accessor for this bit (the header declares
        // only WasAddedThisFrame/ClearAddedThisFrameFlag), so the member is written by name.
        lpProp->mu8Flags = PropInstance::KU_ADDED_THIS_FRAME_FLAG;
        lpProp->mAngularVelocity.SetZero();                               // 0x8263253C stvx +0x50
        lpProp->SetEntityId( lpAddPropEvent->mEntityId );                 // 0x82632544 stw +0x60
        lpProp->SetTypeId(
            static_cast<u32>( static_cast<s32>( lpAddPropEvent->miPropTypeId ) ) );  // 0x82632550 lhz/extsh/stw +0x64

        // 0x82632554 / 0x82632560 -- the only two out-of-line calls in the seed. r4 == the event
        // pointer itself, i.e. &lpAddPropEvent->mTransform (mTransform is at event+0).
        lpProp->SetTransform( lpAddPropEvent->mTransform );
        lpProp->SetLinearVelocity( lZeroVelocity );

        CGS_ASSERT( static_cast<s32>( lpAddPropEvent->miPropTypeId )
                        < static_cast<s32>( KU_MAX_PROP_TYPES ),
                    "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)" );  // :508

        // 0x8263258C `addi r3, this, 0x54` == &mpPhysicsData, into ResourcePtr<T>::operator->
        // (0x822868E0 -- IDA truncates that symbol to "BrnPhysics::Props::Prop"; I dumped its body
        // and it is the ResourcePtr tripwire, baked CgsResourcePtr.h:544, which pins the identity).
        // ⚠️ The string is "Can not instance resource pointer - it has no main memory resource\n"
        //    (aCanNotInstance @0x820072A0, read out of the image with headless IDA this round). The
        //    committed tree spells it at CgsResourcePtr.h:201/:209 and CgsResourceHandle.h:90/:99.
        //    ⚠️ The bad "...it is NULL" quote lived in PropManager_wQ2_01.cpp's callee census, NOT
        //    in PropManager_wQ2_02.cpp as this note used to say (round-2 MUST_FIX: wQ2_02:180 is an
        //    unrelated cache-slot comment and wQ2_02's own mention of the string is a correctly
        //    truncated prefix). wQ2_01's census has since been corrected.
        // GetType is then INLINED, with its own two asserts (BrnPropPhysicsDataHeader.h:173/:174,
        // baked as `li r5,0xAD`/`li r5,0xAE` against the "...SharedClasses\Physics\Props\
        // BrnPropPhysicsDataHeader.h" filename) -- the tree's header-inline GetType reproduces
        // both. muTypeId is loaded at 0x82632590, BEFORE the call, because the call clobbers r3.
        const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

        // ---------------------------------------------------------------------------------------
        // 0x826325E0..0x82632604 -- the INLINED InSceneUpdateInterface::AddCachedObject. The DWARF
        // names the helper (`void AddCachedObject(int32_t, float32_t)`, dumpfile
        // .../CgsSceneManagerIO_SceneUpdate.h:503 -> source :472); the tree does not declare it,
        // and the ARTIST build emits no call to it -- the two-word event fill and the direct
        // `bl BaseEventQueue<InEventAddToCache>::AddEvent` ARE the whole helper, with the queue
        // pointer formed as lpSceneInput + 0xC4930 == mAddToCacheQueue. The inlined form is spelled
        // here; when AddCachedObject is declared (header request), collapse these four lines onto
        // it. The event's own bounds tripwire lives inside BaseEventQueue::AddEvent, exactly as on
        // the console.
        // ---------------------------------------------------------------------------------------
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddToCacheEvent;
        lAddToCacheEvent.miCacheSlot         = KI_PROP_CACHE_START_INDEX + liPropInstanceIndex;
        lAddToCacheEvent.mfCacheSphereRadius = lpType->GetBoundingRadius();   // NO padding added
        lpSceneInput->mAddToCacheQueue.AddEvent( lAddToCacheEvent );

        // 0x82632608..0x8263261C. ONE load of meState feeds both flags: `cmpwi r11,2` selects the
        // jointed arm, and `addi r10,r11,-1 ; cntlzw ; extrwi r8,r11,1,26` is the branchless
        // `(meState == 1)` (the extracted bit is set only when cntlzw(meState-1) == 32, i.e. only
        // when meState-1 == 0). E_STATIC == 1 and E_LEANING == 2 in BrnWorld::EPropState.
        bool       lbStatic  = ( lpAddPropEvent->meState == BrnWorld::E_STATIC );
        const bool lbJointed = ( lpAddPropEvent->meState == BrnWorld::E_LEANING );

        if ( lbJointed )
        {
            // 0x82632624..0x8263268C -- the inlined BitArray<15>::GetFirstZeroBit (the DWARF's
            // name; the tree spells the same method GetFirstClearBit, and its body is the same
            // algorithm: skip an all-ones field, else `x - ((x-1)&x)` to isolate the lowest set
            // bit of the complement, `cntlzd`, `wordIndex*64 + 63 - clz`, and -1 when the result
            // reaches tuNumBits).
            const s32 liLampostJointIndex = mUsedPropJoints.GetFirstClearBit();

            CGS_ASSERT( liLampostJointIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liLampostJointIndex != -1" );                                  // :522

            lpProp->SetJointIndex( liLampostJointIndex );                      // 0x826326AC stb +0x6C
            mBreakPropJoints.UnSetBit( static_cast<u32>( liLampostJointIndex ) );  // 0x826326D0 andc on +0x678
            mauPropIndexForJoint[liLampostJointIndex] =
                static_cast<u8>( liPropInstanceIndex );                        // 0x82632714 stb +0x2A0

            // 0x82632718..0x82632748. The joint anchor in world space.
            // ⚠️ vmaddfp OPERAND ORDER, re-derived rather than assumed (this exact rule shipped a
            //    defect in round 1). IDA prints the RAW FIELD order vD, vA, vB, vC while the PPC
            //    semantics are vD = vA*vC + vB -- so the printed form `vmaddfp vD, vA, vB, vC`
            //    means vD = <2nd operand> * <4th operand> + <3rd operand>. Reading it that way,
            //    the cascade at 0x82632738/0x82632740/0x82632744 is
            //        v13 = xAxis*splat_x + wAxis ; v13 = yAxis*splat_y + v13 ; v0 = zAxis*splat_z + v13
            //    == TransformPoint(propTransform, jointLocator). The other reading would give
            //    row0*row3, which is not any transform at all -- that asymmetry is what settles it.
            //    v9/v8/v0 are `vspltw` of lanes 0/1/2 of *(lpType+0) == PropTypeData::mJointLocator,
            //    and v12/v11/v10/v13 are lpProp's four transform rows at +0x00/+0x10/+0x20/+0x30.
            // ⚠️ ONE LANE DIVERGES, STATED: the console's FMA cascade is full 4-lane, so its w is
            //    jointLocator.w-weighted; the committed helper forces `lvResult.w = 0.0f`
            //    (matrix44affine_operation.h:69 -- :66 is the x-lane expression, :69 is the w
            //    store). maPropJointPositions is a Vector3 array whose
            //    consumers (UpdateJointedProps) read xyz, and the tree helper is the standing
            //    precedent, so it is used -- but do not "restore" the w lane by hand-spelling the
            //    FMA without checking those consumers.
            maPropJointPositions[liLampostJointIndex] =
                rw::math::vpu::TransformPoint( lpProp->GetTransform(), lpType->GetJointLocator() );

            maLastJointRotation[liLampostJointIndex].SetZero();                // 0x8263273C stvx +0x1B0
            maCurrentJointTransforms[liLampostJointIndex] =
                lpAddPropEvent->mTransform;                                    // 0x8263274C..0x82632768
            mUsedPropJoints.SetBit( static_cast<u32>( liLampostJointIndex ) ); // 0x82632880 or on +0x670

            // 0x82632884 `mr r8, r15` (r15 == 1) -- a jointed prop is always static.
            lbStatic = true;
        }

        lpProp->SetIsStatic( lbStatic );                                        // 0x82632898 stb +0x6D
        lpProp->SetExtraComOffsetFlag( lpAddPropEvent->mbAddExtraComOffset );   // 0x8263289C..0x826328D0

        // 0x826328E4. r4/r5/r6/r7/r8/r9/r10 + v1/v2 exactly as the register map above; the
        // Matrix44Affine rides r6 as a hidden reference (the event pointer itself) and the two
        // Vector3s ride v1/v2, consuming NO GPR slot -- which is why lpSimModuleInputBuffer lands
        // in r10 and not r8.
        AddPropToSim( lpAddPropEvent->mEntityId,
                      liPropInstanceIndex,
                      lpAddPropEvent->mTransform,
                      lpType,
                      lbStatic,
                      lpAddPropEvent->mbAddExtraComOffset,
                      lpSimModuleInputBuffer,
                      lZeroVelocity,
                      lZeroVelocity );
    }
}

}
}
