// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ_03.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props keystone wave Q, group 3, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// GROUP 3 WAS THREE FUNCTIONS. ONE LANDS HERE; TWO ARE PARKED, each on a declaration that does
// not exist in the tree today (verified by grep, not by repeating the spec):
//
//   * PropManager::UpdateTriangleCache            @0x826119A0  -- BODIED BELOW.
//   * PropManager::BeginPropWorldContactGeneration @0x82628CB0 -- PARKED,
//         scratchpad/waveQ/parked/PropManager_03_BeginPropWorldContactGeneration.cpp
//         (needs BaseCollisionGenerator::{Create,Run}CollidePrimitiveListWithTriangleListStream)
//   * PropManager::EndPropWorldContactGeneration   @0x82628E18 -- PARKED,
//         scratchpad/waveQ/parked/PropManager_03_EndPropWorldContactGeneration.cpp
//         (needs BaseCollisionGenerator::GetNumUsedResultLists)
//
// ⚠️ ODR NOTE FOR THE CONDUCTOR -- all three of these functions ALREADY HAVE A ONE-SHOT
//    CONDUCTOR-GATE BODY, in b5-decomp/src/GameSource/Physics/BrnPhysicsConductorGates.cpp:
//        :485  BeginPropWorldContactGeneration      :492  EndPropWorldContactGeneration
//        :514  UpdateTriangleCache
//    `cl /c` cannot see the duplicate. The :514 gate MUST be deleted when this partfile mounts.
//    ⚠️ RE-CONFIRMED 2026-08-18 (round 2), AND THE GATE IS DELIBERATELY LEFT IN PLACE.
//    BrnPhysicsConductorGates.cpp is not this TU's to edit, and AGENTS.md's gate convention is
//    that a gate is retired only in the same commit that MOUNTS the real body in
//    tools/build/build_game_exe.bat. That script lists BrnPhysicsConductorGates.cpp (line 1161)
//    and does NOT list PropManager_wQ_03.cpp, so today there is no LNK2005 -- it fires the
//    instant this partfile is added to the source list. Reported, not "fixed": deleting the gate
//    unilaterally would leave UpdateTriangleCache with no definition in the mounted build at all.
//    Re-run coverage_check against the whole GameSource/Physics directory, not just
//    GameSource/Physics/PropManager, or this cross-directory duplicate stays invisible.
//
// Grounding: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x826119A0.json (RAW `assembly`, 116 insns);
// the Hex-Rays pseudocode in the same export was used only to confirm the stack-slot map.
// Nothing below is invented; every claim in a comment is either an asm line quoted verbatim or
// is labelled INFERENCE.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                       // InputBuffer_Update::GetInSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"           // InSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"  // InEventUpdateCachedPosition
#include "rw/math/vpu/vector3_operation.h"                                               // rw::math::vpu::IsValid(Vector3)

namespace BrnPhysics
{
namespace Props
{

// The cache sphere is padded before it is published. MEASURED: the immediate is the rodata float
// flt_82004014, whose bytes are 3D CC CC CD == 0.1f big-endian (read out of the .i64 with headless
// IDA 9.3). NAMED rather than left as a bare literal in round 2 (2026-08-18) to match the two
// sibling producers of the same constant -- BrnDetachedWheelManager.cpp:76 and
// BrnDetachedPartManager.cpp:269 both spell it KF_TRIANGLE_CACHE_PADDING. ⚠️ Each of those is a
// FUNCTION-LOCAL / FILE-LOCAL definition in its own .cpp -- there is no shared header home for
// this constant anywhere in the tree, so it is redeclared here in the same local form rather than
// a new shared header being invented for it. If a real home ever appears, all three collapse.
static const f32 KF_TRIANGLE_CACHE_PADDING = 0.1f;   // flt_82004014, measured

// =================================================================================================
// BrnPhysics::Props::PropManager::UpdateTriangleCache @ 0x826119A0  (116 asm insns)
//
// Arm 2 of PhysicsModule::UpdateCachedPositions @0x8259C370: per live (non-frozen) prop, post one
// InEventUpdateCachedPosition so the scene's triangle cache follows that prop's collision sphere.
//
// ---- MEASURED, line by line (all offsets are CONSOLE offsets quoted in comments only) ----------
//
//   0x826119C4  cmplwi cr6, r24, 0 / bne             the null tripwire on the parameter is
//   0x826119DC  "lpSceneInputBuffer_Update != NULL"  NON-GATING: the asm falls straight through
//   0x826119D8  li r5, 0x8EB  == BrnPropManager.cpp:2283
//
//   0x826119EC  lwz r11, 0x688(r26)                  mUpdatedProps.GetLength() -- and it is
//   0x82611B4C  lwz r11, 0x688(r26)                  RE-READ at the bottom of every iteration,
//                                                    so the bound is live, not hoisted.
//   0x82611A28  bl BrnPhysics::Props::UpdatePro      (IDA-truncated) == BaseEventQueue<
//                                                    UpdatePropEvent>::GetEvent(i) on this+0x680.
//   0x82611A30  lbz r11, 0x68(r31) / bne             skip the event when mbFrozen.
//
//   0x82611A18  lfs f31, flt_82001CC0 (== 0.0f, the value this TU's committed Construct banner
//               already pins) -- hoisted out of the loop and re-stored EVERY iteration at
//   0x82611A50  stfs f31, lfRadius ; 0x82611A58 stw r30(0), liCacheSlot
//               i.e. both out-params are freshly zero-initialised before each query.
//
//   0x82611A5C  bl GetTriangleCacheSlotAndRadius with r3=this, r4=lwz 0x60 (mEntityId),
//               r5=extsh lhz 0x64 (miPhysicsSlot, SIGN-extended), r6=&liCacheSlot, r7=&lfRadius.
//   0x82611A60  clrlwi r3,r3,24 / beq                the bool return gates everything below.
//
//   0x82611A6C  addi r31, r31, 0x30                  &lEvent.mTransform.Pos() -- row 3 of the
//               Matrix44Affine at event+0x00, i.e. wAxis. (event+0x30 == offsetof row 3.)
//   0x82611A74..0x82611AC8  lvx128 + three vspltw(0,1,2) + vcmpeqfp. self-equality lanes ANDed
//               == rw::math::vpu::IsValid(Vector3) exactly (x, y, then z), then
//   0x82611AE0  li r5, 0x8FE == BrnPropManager.cpp:2302, assert
//               "RwMathVPU::IsValid( lEvent.mTransform.Pos() )". Also NON-GATING.
//
//   0x82611AFC  bl CgsSceneManager::SceneManagerIO::InputB (IDA-truncated) ==
//               InputBuffer_Update::GetInSceneUpdateInterface(), r3 = the parameter.
//   0x82611B18  add r3, r3, r29 where r29 = 0xC5290  == &mUpdateCachedPositionQueue.
//   0x82611B48  bl BaseEventQueue<InEventUpdateCachedPosition>::AddEvent.
//
// ---- THE PAYLOAD BUILD, exactly as emitted (frame: sp+0x60 a Vector3Plus temp, sp+0x70 the
//      32-byte event, whose mNewPositionAndRadius sits at sp+0x80) ---------------------------
//   0x82611B00  vspltisw v0, 0                       a zero vector
//   0x82611B1C  vrlimi128 v127, v0, 1, 0             ZERO THE W LANE of the loaded position
//   0x82611B20  stvx128 v0,  sp+0x80                 zero the event's Vector3Plus member
//   0x82611B24  lwz/stw  liCacheSlot -> sp+0x70      lEvent.miCacheSlot = liCacheSlot
//   0x82611B30  stvx128 v127, sp+0x60                temp = position (w == 0)
//   0x82611B08/0x82611B0C/0x82611B14  f0 = lfRadius + flt_82004014
//   0x82611B38  stfs f0, sp+0x6C                     temp.w = that sum  (the W LANE IS THE RADIUS)
//   0x82611B40  lvx128/stvx128 sp+0x60 -> sp+0x80    lEvent.mNewPositionAndRadius = temp
//
//   flt_82004014 == 0.1f. MEASURED, not rounded: Hex-Rays renders this same rodata slot as the
//   literal 0.1 in four other independent exports (0x82708D48, 0x82715A18, 0x828ABF48,
//   0x82623198), and the sibling DetachedWheelManager::UpdateTriangleCache @0x8260E9F8 pads its
//   own cache sphere by the same 0.1. It is a real cache-padding constant, NOT a rounding
//   artefact of the disassembly.
//
//   Two stores in that sequence are DEAD and are therefore not transcribed (each is overwritten
//   before anything can observe it, and no address is published in between): the w-lane zero at
//   0x82611B1C (overwritten by the radius at 0x82611B38) and the whole-vector zero at
//   0x82611B20 (overwritten by the copy at 0x82611B44). They are the register-level way the
//   compiler built a Vector3Plus from a Vector3 plus a scalar. Called out rather than smoothed.
//
// ---- ⚠️ ONE DIVERGENCE FROM THE SOURCE SHAPE, FLAGGED, NOT HIDDEN ------------------------------
//   The DecFIGS DWARF declares the producer this loop called:
//       CgsSceneManagerIO_SceneUpdate.h:478
//         void InSceneUpdateInterface::UpdateCachedObjectPosition(int32_t, Vector3, float32_t);
//       (and a VecFloat-tailed overload at :484)
//   That method DOES NOT EXIST in the tree yet -- the X360 inlined it here, which is why the
//   event build above is open-coded in this body. The queue it appends to is a PUBLIC member of
//   InSceneUpdateInterface (CgsSceneManagerIO_SceneUpdate.h:258 -- the struct has no `private:`
//   at all), so this body reaches it BY NAME, which is legal and is the same by-name discipline
//   CgsTriangleCacheManager_Events.cpp already uses on its two sibling queues.
//   ⚠️ NOTE FOR WHOEVER OWNS CgsSceneManagerIO_SceneUpdate.h: when the DWARF producer lands, the
//   four payload lines below collapse into one call and SHOULD be folded. The spec's claim that
//   "the queue is private" is FALSE as of this wave; the only thing missing is the producer.
//   (I do not own that header, so nothing there was touched.)
//   ⚠️ ROUND 2 (2026-08-18): REQUEST 4 IN scratchpad/waveQ/PropManager.spec.md STAYS OPEN even
//   though this body compiles. It is a SOURCE-SHAPE divergence, not a blocker, and the risk of
//   dropping the request is that a second, independent producer of InEventUpdateCachedPosition
//   then exists forever. When UpdateCachedObjectPosition(s32, Vector3, f32) lands on
//   InSceneUpdateInterface, collapse the four payload lines below to
//       lpSceneUpdateInterface->UpdateCachedObjectPosition(
//           liCacheSlot, lrEvent.mTransform.Pos(), lfRadius + KF_TRIANGLE_CACHE_PADDING);
//   and delete this divergence banner. The DWARF citations were also renumbered in round 2:
//   :478 / :484 are the real SOURCE lines (the round-1 spec cited :506 / :509, which are the
//   dumpFILE's own line numbers).
// =================================================================================================
void PropManager::UpdateTriangleCache(
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
{
    // BrnPropManager.cpp:2283 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT(lpSceneInputBuffer_Update != NULL, "lpSceneInputBuffer_Update != NULL");

    // The bound is re-read every iteration (lwz 0x688 at both 0x826119EC and 0x82611B4C).
    for (s32 liEvent = 0; liEvent < mUpdatedProps.GetLength(); ++liEvent)
    {
        const UpdatePropEvent& lrEvent = mUpdatedProps.GetEvent(liEvent);

        if (lrEvent.mbFrozen)
        {
            continue;                       // lbz 0x68 / bne
        }

        s32 liCacheSlot = 0;                // stw r30(0)  -> the s32& out-param
        f32 lfRadius    = 0.0f;             // stfs f31     -> the f32& out-param (flt_82001CC0)

        if (!GetTriangleCacheSlotAndRadius(lrEvent.mEntityId, lrEvent.miPhysicsSlot,
                                           liCacheSlot, lfRadius))
        {
            continue;                       // this prop owns no triangle-cache slot
        }

        // BrnPropManager.cpp:2302 -- non-gating tripwire.
        CGS_ASSERT(rw::math::vpu::IsValid(lrEvent.mTransform.Pos()),
                   "RwMathVPU::IsValid( lEvent.mTransform.Pos() )");

        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface =
            lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

        // --- the inlined InSceneUpdateInterface::UpdateCachedObjectPosition (see the banner) ---
        CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition lEvent;
        lEvent.miCacheSlot = liCacheSlot;
        lEvent.mNewPositionAndRadius.SetVector3(lrEvent.mTransform.Pos());
        lEvent.mNewPositionAndRadius.SetPlus(lfRadius + KF_TRIANGLE_CACHE_PADDING);

        lpSceneUpdateInterface->mUpdateCachedPositionQueue.AddEvent(lEvent);
    }
}

}
}
