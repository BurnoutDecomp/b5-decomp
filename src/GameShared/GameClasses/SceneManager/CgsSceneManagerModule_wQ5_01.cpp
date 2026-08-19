// ===========================================================================
// CgsSceneManager::SceneManagerModule -- wave Q5 cluster E1b (2026-08-19)
//   THE PADDING / COLLISION-BODY RE-POST LEG of the scene-update drain.
//
// Partfile of GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp
// (declarations live in CgsSceneManagerModule.h, which is owned by cluster E1a;
// this TU only DEFINES four of its already-declared members so the two owners of
// the drain can land in parallel without touching one another's file).
//
// X360 ARTIST functions reconstructed here, store-for-store:
//   SceneManagerModule::ProcessSetPaddingEvent          @ 0x828CF9A8  (46 insns)
//   SceneManagerModule::ProcessClearEntityPaddingEvent  @ 0x828CFA60 (136 insns)
//   SceneManagerModule::ProcessForceNoPaddingEvent      @ 0x828CFC80  (46 insns)
//   SceneManagerModule::UpdateCollisionBody             @ 0x828C7528  (63 insns)
//
// ⚠️ UpdateCollisionBody @0x828C7528 is an EXPORT HOLE: it has no
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828C7528.json and no progress/identity.json
// row. Its disassembly + pseudocode were recovered by the wave-Q5 scout's headless-IDA
// run on a PRIVATE copy of the .i64 and live in scratchpad/waveQ5/q5_out.json (entry
// req == "0x828c7528"); every instruction quoted below is from that dump. The other
// three DO have per-address JSON.
//
// WHY THIS LEG EXISTS AT ALL (the wave's point). The broad-phase sweeper only knows a
// body's world AABB + its sweep padding from what the SceneManagerModule pushes onto
// OverlapGenerationIO::InputBuffer. UpdateCollisionBody is the one producer of
// mUpdateBodyQueue: it re-measures a volume instance's world box through the rwcollision
// per-type GetBBox and re-posts it together with the instance's padding lane. The three
// padding events are its only callers besides the drain
// (BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 idx 1429 / 1473 / 1514 / 1555).
//
// SOURCE LADDER USED
//   rung 1  ARTIST asm  -- every store, branch, early-out and call target below.
//   rung 2  DecFIGS DWARF -- signatures (CgsSceneManagerModule.h:337/340/343/346) and
//           the local NAMES/TYPES, transcribed from
//           references/DecFIGS/dwarfdump/_compile/CgsSceneManagerUnity.cpp:17821 /
//           :17907 / :17942 / :18017 (lAabb, lpVolInst, lpVolume, lPadding, lZero,
//           lu16EntityIdx, liVolumeInstIndex, lVolId, liVolumeInstIndex).
//   rung 3  Feb-2007 -- not consulted; this file has no Feb-2007 counterpart.
//
// ASSERT MESSAGES + BAKED LINES (all from CgsSceneManagerModule.cpp on the console;
// the string constant is aDP4B5MainBurno_415 == ".../scenemanager/CgsSceneManagerModule.cpp"):
//   0x45B == 1115  "lpOverlapGenerationInputBuffer != NULL"   ProcessSetPaddingEvent
//   0x469 == 1129  "Volume instance not found"                ProcessSetPaddingEvent
//   0x478 == 1144  "lpOverlapGenerationInputBuffer != NULL"   ProcessClearEntityPaddingEvent
//   0x47E == 1150  "Entity not found (ClearPadding): "        ProcessClearEntityPaddingEvent
//   0x48D == 1165  "Volume instance not found"                ProcessClearEntityPaddingEvent
//   0x4A5 == 1189  "lpOverlapGenerationInputBuffer != NULL"   ProcessForceNoPaddingEvent
//   0x4B3 == 1203  "Volume instance not found"                ProcessForceNoPaddingEvent
//   0x4C9 == 1225  "lpOverlapGenerationInputBuffer != NULL"   UpdateCollisionBody
//   0x4E7 == 1255  "Volume instance not found"                UpdateCollisionBody
// The console streams the offending id onto the two "... : " messages through
// CgsDev::StrStream; CGS_ASSERT in this tree takes a plain string (CgsAssert.h), so the
// message stops at the string -- the same treatment every other reconstructed TU gives
// (see CgsVolumeManager.cpp's banner).
//
// CONSOLE OFFSETS, FOR PROVENANCE ONLY -- nothing below is spelled as an offset:
//   this + 0x1A2480 (`addis 0x1A ; addi 0x2480`)  == mEntityManager
//   this + 0x2C7800 (`addis 0x2C ; addi 0x7800`)  == mVolumeManager
//   this + 0x1C9A80 (`lis 0x1C ; ori 0x9A80`)     == mEntityManager.mVolumeInstancePool
//                                                    (0x1A2480 + 0x27600), reached by the
//                                                    inlined GetVolumeInstanceIdByIndex
// Both match the member map in CgsSceneManagerModule.h:308/309.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"   // VolumeInstanceId (by-value param)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"     // VolumeInstance (mPadding / miVolumeIndex / IsCollidable)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetPadding.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearEntityPadding.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventForceNoPadding.h"
// The REAL OverlapGenerationIO::InputBuffer (UpdateBody / ForceNoPadding + AABBoxRows).
// CgsSceneManagerModule.h only forward-declares InputBuffer for its own declarations.
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"
// rw::collision::Volume -- for the GetBBox descriptor dispatch. Same include the sibling
// CgsVolumeManager.cpp uses for the same call; rw::collision::AABBox stays incomplete
// there and here (see the AABBoxRows note at the call site).
#include "SDKs/EATech/rwcollision/volume_debug_access.h"

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// SceneManagerModule::UpdateCollisionBody @ 0x828C7528  (63 insns; export hole --
//   dump: scratchpad/waveQ5/q5_out.json, DWARF CgsSceneManagerModule.h:346 /
//   body CgsSceneManagerModule.cpp:1223)
//
// SIGNATURE FROM THE ASM (not from Hex-Rays, which renders it `(int,int,int,int)`):
//   r3 = this
//   r4 = liVolumeInstanceIndex   -- `cmpwi cr6, r30, -1` => a SIGNED 32-bit index
//   r5 = lVolumeInstanceID       -- moved whole into r6 for UpdateBody's `std`, and every
//                                   caller loads it with a 64-bit `ld`, so it is the whole
//                                   packed 8-byte VolumeInstanceId BY VALUE (DWARF h:346
//                                   spells exactly `VolumeInstanceId`, not a reference)
//   r6 = lpOverlapGenerationInputBuffer
//
// BODY, instruction by instruction:
//   0x828C7548  cmplwi r27,0 / bne         -- assert lpOverlapGenerationInputBuffer != NULL (:1225)
//   0x828C7570  cmpwi  r30,-1 / beq 0x828C7600
//                                          -- the -1 arm only fires the "Volume instance
//                                             not found" assert (:1255) and returns; there
//                                             is no work on that path
//   0x828C7588  bl sub_828B9FD8            -- mEntityManager.GetVolumeInstance(index).
//                                             sub_828B9FD8 is the NON-const overload (it
//                                             calls the non-const ObjectPool operator[]
//                                             @0x828B7348); a non-const member reaching a
//                                             non-const member => plain member access here.
//   0x828C7590  lbz 0x6A / clrlwi ,31 / beq -> return
//                                          -- `if (!lpVolInst->IsCollidable()) return;`
//                                             (VolumeInstance::mx8Flags bit 0, pinned in
//                                             CgsVolumeInstance.h)
//   0x828C75A4  lwz r4,0x5C(r31)           -- lpVolInst->miVolumeIndex
//   0x828C75AC  bl VolumeManager::GetRwVolume
//   0x828C75B0  lwz r11,0x40(r3) ; lwz r11,4(r11) ; bctrl  with r3=volume, r4=r31,
//               r5=1, r6=&var_60
//                                          -- lpVolume->GetBBox(&lpVolInst->
//                                             mWorldSpaceTransform, /*tight*/1, lAabb).
//                                             ⚠️ r4 is the VolumeInstance POINTER: the
//                                             transform is that record's FIRST member
//                                             (CgsVolumeInstance.h +0x00), so the console's
//                                             `&instance` IS `&instance.mWorldSpaceTransform`.
//                                             This is NOT C++ virtual dispatch -- +0x40 is
//                                             the rwcollision per-TYPE descriptor and +0x04
//                                             inside it is getBBox (volume_debug_access.h).
//   0x828C75D4  bl sub_828B9FD8            -- GetVolumeInstance(index) a SECOND time. Kept:
//                                             the console really does call it twice (the
//                                             DWARF call list names EntityManager::
//                                             GetVolumeInstance twice for this function), and
//                                             it is an out-of-line, non-pure call the
//                                             compiler could not have CSE'd away.
//   0x828C75F0  lvx128 v1, r11, 0x40       -- lPadding = thatInstance->mPadding
//   0x828C75F4  bl OverlapGenerationIO::InputBuffer::UpdateBody
//                                             r3=buffer, r4=index, r5=&lAabb, r6=id, v1=padding
//
// ARGUMENT ORDER OF UpdateBody -- WHY (index, aabb, padding, id) AND NOT (…, id, padding).
// The Xenon ABI gives vector parameters their own register file (v1..) and they do NOT
// consume a GPR argument slot. So the GPR slots r4/r5/r6 are arguments 1, 2 and 4 while
// v1 is argument 3 -- which is exactly the DWARF declaration
// `UpdateBody(uint32_t, AABBox*, Vector3, VolumeInstanceId)` already landed at
// CgsOverlapGenerationModuleIO.h:223. Cross-check on the same convention in this very
// file: EntityManager::SetVolumePadding(int32_t, Vector3) is called from
// ProcessSetPaddingEvent with r4 = index and v1 = padding and NO r5 -- one GPR slot for
// one GPR argument. Had a vector consumed a slot, UpdateBody's id would have landed in
// r7, not r6.
//
// ⚠️ NO NULL GUARD ON lpVolInst, DELIBERATELY. EntityManager::GetVolumeInstance returns
// NULL for an unallocated slot, and the console dereferences the result unconditionally
// (`lbz r11,0x6A(r31)` with no test). A guard the binary does not have is new behaviour
// and would silently swallow a real breakage; reproduced as-is.
// ---------------------------------------------------------------------------
void SceneManagerModule::UpdateCollisionBody(s32 liVolumeInstanceIndex,
                                             VolumeInstanceId lVolumeInstanceID,
                                             OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");

    if (liVolumeInstanceIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    // DWARF CgsSceneManagerModule.cpp:1230 -- `const VolumeInstance * lpVolInst`.
    const VolumeInstance* lpVolInst = mEntityManager.GetVolumeInstance(liVolumeInstanceIndex);

    if (!lpVolInst->IsCollidable())
    {
        return;
    }

    // DWARF :1231 -- `const VolRef::Volume * lpVolume`.
    const VolRef::Volume* lpVolume = mVolumeManager.GetRwVolume(lpVolInst->miVolumeIndex);

    // DWARF :1229 -- `AABBox lAabb`. rw::collision::AABBox is only NAMED in this TU: its
    // real home (vendor/renderware/collision/AABBox.hpp) pulls the SDK
    // rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD via
    // BrnCommonTypes.h, and the two cannot coexist -- a PRE-EXISTING Vector3 fork reported
    // by three earlier readers (CgsVolumeManager.cpp:54-77, CgsSceneSweeper_wQ5_01.cpp:26-53,
    // PropManager_wQ4_03.cpp:289-301). OverlapGenerationIO::AABBoxRows is the byte image of
    // AABBox already declared for exactly this purpose in the header this TU includes
    // (two 16-byte corner rows, pinned by the mounted layout oracle
    // PropManager_wQ4_03_embed_check.cpp), so no new local fork is minted here.
    OverlapGenerationIO::AABBoxRows lAabb;

    // The scene manager's opaque VolRef::Volume IS rw::collision::Volume (DecFIGS
    // volume.h:39 typedefs exactly that); the cast is between two spellings of one type,
    // not a layout reinterpretation. Same one-liner as CgsVolumeManager.cpp:218.
    reinterpret_cast<const rw::collision::Volume*>(lpVolume)->GetBBox(
        &lpVolInst->mWorldSpaceTransform,
        1,
        *reinterpret_cast<rw::collision::AABBox*>(&lAabb));

    {
        // DWARF opens an inner block at :1237 for `Vector3 lPadding` -- and the asm's
        // second GetVolumeInstance call is inside it.
        const Vector3 lPadding =
            mEntityManager.GetVolumeInstance(liVolumeInstanceIndex)->mPadding;

        lpOverlapGenerationInput->UpdateBody(static_cast<u32>(liVolumeInstanceIndex),
                                             reinterpret_cast<const rw::collision::AABBox*>(&lAabb),
                                             lPadding,
                                             lVolumeInstanceID);
    }
}

// ---------------------------------------------------------------------------
// SceneManagerModule::ProcessSetPaddingEvent @ 0x828CF9A8  (46 insns)
//   DWARF CgsSceneManagerModule.h:337, body CgsSceneManagerModule.cpp:1113,
//   local :1117 `int32_t liVolumeInstIndex`.
//
// Drain leg for InSceneUpdateInterface::mSetPaddingQueue (idx 1473 of
// BridgeInputSceneUpdateInterfaceToSubModules).
//
//   0x828CF9C4  cmplwi r27,0 / bne          -- assert the input buffer (:1115)
//   0x828CF9F0  ld r4, 0(r30)               -- lrEvent.mVolInstanceId. A 64-bit `ld` off
//                                              the event's FIRST byte: the whole packed
//                                              VolumeInstanceId (CgsSceneManagerIO_EventSetPadding.h)
//   0x828CF9FC  bl EntityManager::GetVolumeInstanceIndexByID
//   0x828CFA04  cmpwi r31,-1 / beq 0x828CFA3C
//                                           -- miss arm: assert (:1129) and nothing else
//   0x828CFA18  lvx128 v1, r30, 0x10        -- lrEvent.mPadding (the event's +0x10 lane)
//   0x828CFA1C  bl EntityManager::SetVolumePadding(index, padding)
//   0x828CFA24  ld r5, 0(r30)               -- re-read the id for the call below
//   0x828CFA30  bl SceneManagerModule::UpdateCollisionBody(index, id, buffer)
// ---------------------------------------------------------------------------
void SceneManagerModule::ProcessSetPaddingEvent(const SceneManagerIO::InEventSetPadding& lrEvent,
                                                OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");

    const s32 liVolumeInstIndex =
        mEntityManager.GetVolumeInstanceIndexByID(lrEvent.mVolInstanceId);

    if (liVolumeInstIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    mEntityManager.SetVolumePadding(liVolumeInstIndex, lrEvent.mPadding);

    UpdateCollisionBody(liVolumeInstIndex, lrEvent.mVolInstanceId, lpOverlapGenerationInput);
}

// ---------------------------------------------------------------------------
// SceneManagerModule::ProcessForceNoPaddingEvent @ 0x828CFC80  (46 insns)
//   DWARF CgsSceneManagerModule.h:343, body CgsSceneManagerModule.cpp:1187,
//   local :1191 `int32_t liVolumeInstIndex`.
//
// Drain leg for InSceneUpdateInterface::mForceNoPaddingQueue (idx 1555).
//
//   0x828CFC9C  cmplwi r30,0 / bne          -- assert the input buffer (:1189)
//   0x828CFCC8  ld r4, 0(r28)               -- lrEvent.mVolInstanceId (8-byte id, the
//                                              event's only field)
//   0x828CFCD0  bl EntityManager::GetVolumeInstanceIndexByID
//   0x828CFCD8  cmpwi r31,-1 / beq 0x828CFD14  -- miss arm: assert (:1203) only
//   0x828CFCE4  stw r31, var_40(r1)         -- the console builds the one-word
//   0x828CFCEC  bl sub_828B0380             --   InForceNoPadding on the stack, fetches
//   0x828CFCF4  bl InForceNoPadding::AddEvent --  the write-locked queue and appends:
//                                              i.e. InputBuffer::ForceNoPadding(index),
//                                              inlined. De-inlined back to the member the
//                                              DWARF declares (CgsOverlapGenerationModuleIO.h:216,
//                                              body already landed by cluster D1) -- the
//                                              DWARF call list for THIS function names
//                                              `OverlapGenerationIO::InputBuffer::ForceNoPadding`
//                                              explicitly.
//   0x828CFD00  ld r5, 0(r28)               -- re-read the id
//   0x828CFD08  bl SceneManagerModule::UpdateCollisionBody(index, id, buffer)
// ---------------------------------------------------------------------------
void SceneManagerModule::ProcessForceNoPaddingEvent(const SceneManagerIO::InEventForceNoPadding& lrEvent,
                                                    OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");

    const s32 liVolumeInstIndex =
        mEntityManager.GetVolumeInstanceIndexByID(lrEvent.mVolInstanceId);

    if (liVolumeInstIndex == KI_INVALID_VOLUME_INSTANCE_INDEX)
    {
        CGS_ASSERT(false, "Volume instance not found");
        return;
    }

    lpOverlapGenerationInput->ForceNoPadding(static_cast<u32>(liVolumeInstIndex));

    UpdateCollisionBody(liVolumeInstIndex, lrEvent.mVolInstanceId, lpOverlapGenerationInput);
}

// ---------------------------------------------------------------------------
// SceneManagerModule::ProcessClearEntityPaddingEvent @ 0x828CFA60  (136 insns)
//   DWARF CgsSceneManagerModule.h:340, body CgsSceneManagerModule.cpp:1142.
//   Locals (DWARF): :1145 `Vector3 lZero`, :1146 `const VolumeInstance * lpVolInst`,
//   :1147 `uint16_t lu16EntityIdx`, :1148 `int32_t liVolumeInstIndex`, and inside the
//   loop body :1158 `VolumeInstanceId lVolId`.
//
// Drain leg for InSceneUpdateInterface::mClearEntityPaddingQueue (idx 1514). Walks an
// ENTITY's whole volume-instance chain and zeroes every instance's sweep padding.
//
//   0x828CFA84  cmplwi r21,0 / bne              -- assert the input buffer (:1144)
//   0x828CFAC0  lfs f0, flt_82001CC0 (== 0.0f)
//   0x828CFAC8  stfs x3 + stw 0                 -- lZero = {0,0,0,0}; loaded back into
//                                                  v127 at 0x828CFBCC and re-used for
//                                                  every iteration
//   0x828CFAB0  lwz r4, 0(r28)                  -- lrEvent.mEntity (a 4-byte EntityId --
//                                                  note the `lwz`, NOT the `ld` the two
//                                                  VolumeInstanceId events use)
//   0x828CFAD8  bl <IndexedHashTable<EntityId,u16,541>::Get>
//   0x828CFAE0..0x828CFAE8  r26 = element ? *(u16*)element : -1
//                                               -- i.e. EntityManager::GetEntityIndexByID,
//                                                  inlined. De-inlined back to the member
//                                                  (the DWARF call list names it).
//   0x828CFAEC  clrlwi/cmplwi 0xFFFF / bne      -- assert "Entity not found (ClearPadding): "
//                                                  (:1150). ⚠️ NOT A GATE: the miss path
//                                                  falls straight through to the walk with
//                                                  lu16EntityIdx == 0xFFFF. Faithful.
//   0x828CFB94  bl EntityManager::GetFirstEntityVolumeInstance(lu16EntityIdx,
//                                                              &liVolumeInstIndex)
//   0x828CFB9C  cmplwi r27,0 / beq -> return    -- the walk is a plain `while (inst)`
//   loop body @0x828CFBD0:
//     cmpwi r31,-1 / beq 0x828CFC3C             -- the -1 arm fires assert (:1165) only
//     0x828CFBD8..0x828CFC1C                    -- the inlined EntityManager::
//                                                  GetVolumeInstanceIdByIndex(liVolumeInstIndex):
//                                                  the SAME bounds assert (baked file
//                                                  aDP4B5MainBurno_439 == CgsEntityManager.h,
//                                                  line 0x159 == 345, message "liIndex <
//                                                  KI_MAX_NUM_VOLUME_INSTANCES &..."), the
//                                                  CONST pool operator[] @0x828B71E8 and
//                                                  `ld 0x50` == VolumeInstance::mUserID.
//                                                  De-inlined to the member @0x828B9E10.
//     0x828CFC20  bl EntityManager::SetVolumePadding(liVolumeInstIndex, lZero)
//     0x828CFC34  bl SceneManagerModule::UpdateCollisionBody(liVolumeInstIndex, lVolId, buffer)
//   loop tail @0x828CFC54:
//     lwz r4,0x60(r27)                          -- lpVolInst->miNextEntityVolumeInstance
//     bl EntityManager::GetVolumeInstance (0x828B9F28, the CONST overload)
//     bne -> loop body                          -- i.e. EntityManager::
//                                                  GetNextEntityVolumeInstance, inlined --
//                                                  see the note below.
//
// ⚠️ MISSING DECLARATION, REPORTED NOT EDITED. The loop tail is DWARF's
// `EntityManager::GetNextEntityVolumeInstance(const VolumeInstance*, int32_t*) const`
// (declared CgsEntityManager.h:120, body CgsEntityManager.h:465 -- a header inline with
// NO out-of-line X360 symbol, which is why the asm shows its two steps in place). This
// tree's CgsEntityManager.h does not declare it (the wave-Q5 A2 owner listed it as
// deliberately out of scope, entmgr.owner.md section on omissions), and CgsEntityManager.*
// is read-only for this cluster -- so the console's own inlined form is what is written
// here, and the exact declaration to add is reported in scratchpad/waveQ5/e1b.owner.md.
// Re-de-inline this to the member the moment that declaration lands.
//
// ⚠️ WHY THE `const EntityManager&` ALIAS. `GetVolumeInstance` is OVERLOADED on constness
// (const @0x828B9F28 / non-const @0x828B9FD8) and the two X360 symbols say which one each
// caller used: this function's xrefs list 0x828B9F28 (const) and the const pool
// operator[] 0x828B71E8, because the console reaches both through the CONST helpers
// GetNextEntityVolumeInstance / GetVolumeInstanceIdByIndex. Plain `mEntityManager.` from a
// non-const member would silently pick the NON-const overload -- a different X360 symbol
// than the binary calls. The alias keeps the call targets honest; delete it when
// GetNextEntityVolumeInstance is declared.
// ---------------------------------------------------------------------------
void SceneManagerModule::ProcessClearEntityPaddingEvent(const SceneManagerIO::InEventClearEntityPadding& lrEvent,
                                                        OverlapGenerationIO::InputBuffer* lpOverlapGenerationInput)
{
    CGS_ASSERT(lpOverlapGenerationInput != NULL, "lpOverlapGenerationInputBuffer != NULL");

    // DWARF :1145 -- the zero padding lane, built once and reused for the whole chain
    // (the console loads it into v127 outside the loop).
    Vector3 lZero;
    lZero.SetZero();

    // See the "WHY THE const EntityManager& ALIAS" note above.
    const EntityManager& lrConstEntityManager = mEntityManager;

    // DWARF :1147 -- `uint16_t lu16EntityIdx`. This tree's GetEntityIndexByID keeps the
    // s32/-1 shape on purpose (CgsEntityManager.h:121-129 documents why re-typing it to
    // the DWARF's uint16_t would turn "absent" into the valid-looking index 65535 in three
    // live per-frame paths), so the narrowing the console gets for free is explicit here.
    // The console's own inlined form yields exactly 0xFFFF on a miss (`li r26,-1` then
    // `clrlwi r11,r26,16`), which is KU16_INVALID_ENTITY_INDEX.
    const s32 liEntityIndex = lrConstEntityManager.GetEntityIndexByID(lrEvent.mEntity);
    const u16 lu16EntityIdx = (liEntityIndex < 0)
                                  ? KU16_INVALID_ENTITY_INDEX
                                  : static_cast<u16>(liEntityIndex);

    // NOT a gate on the console -- the miss path walks on with 0xFFFF. Reproduced.
    CGS_ASSERT(lu16EntityIdx != KU16_INVALID_ENTITY_INDEX, "Entity not found (ClearPadding): ");

    // DWARF :1148 / :1146.
    s32 liVolumeInstIndex = KI_INVALID_VOLUME_INSTANCE_INDEX;
    const VolumeInstance* lpVolInst =
        lrConstEntityManager.GetFirstEntityVolumeInstance(lu16EntityIdx, &liVolumeInstIndex);

    while (lpVolInst != NULL)
    {
        if (liVolumeInstIndex != KI_INVALID_VOLUME_INSTANCE_INDEX)
        {
            // DWARF :1158.
            const VolumeInstanceId lVolId =
                lrConstEntityManager.GetVolumeInstanceIdByIndex(liVolumeInstIndex);

            mEntityManager.SetVolumePadding(liVolumeInstIndex, lZero);

            UpdateCollisionBody(liVolumeInstIndex, lVolId, lpOverlapGenerationInput);
        }
        else
        {
            CGS_ASSERT(false, "Volume instance not found");
        }

        // EntityManager::GetNextEntityVolumeInstance(lpVolInst, &liVolumeInstIndex),
        // inlined by the console and not declarable from this cluster -- see the banner.
        liVolumeInstIndex = lpVolInst->miNextEntityVolumeInstance;
        lpVolInst = lrConstEntityManager.GetVolumeInstance(liVolumeInstIndex);
    }
}

}
