// ===========================================================================
// GameShared/GameClasses/SceneManager/CgsSceneManagerBridgeFunctions.cpp
//
// The scene manager's BRIDGE family -- the per-frame passes that carry one
// sub-module's output queue into the next sub-module's input queue. The console's own
// home for the family is this file: every assert in all three bodies below bakes
//     d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\scenemanager\
//         CgsSceneManagerBridgeFunctions.cpp
// and the DecFIGS DWARF agrees line for line (the declaration lines it records for the
// three functions are :2051 / :2092 / :2141, and the X360's baked assert lines are the
// next two / next-two-plus-seven inside each -- 2053/2054, 2094/2095 + 2101/2102,
// 2143/2144 + 2150/2151).
//
// Reconstructed 2026-08-19, wave Q5 round 4 / cluster F1 -- THE SCENE COLLISION MIDDLE.
// Bodied here, store for store from the X360 ARTIST asm:
//
//   SceneManagerModule::BridgeOverlapGenerationToOverlapCulling  @0x828BA538   (90 insns)
//   SceneManagerModule::BridgeOverlapGenerationToOutputBuffer    @0x828BA6A0  (138 insns)
//   SceneManagerModule::BridgeOverlapCullerToOutputBuffer        @0x828BA8C8  (159 insns)
//
// @0x828BA6A0 is an EXPORT HOLE (no per-address JSON, and no progress/identity.json row);
// its asm came from the scout's targeted headless-IDA dump on a private .i64 copy,
// scratchpad/waveQ5/q5_out.json. The other two are
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828BA538.json and 0x828BA8C8.json.
//
// WHY THESE THREE MATTER: BridgeOverlapCullerToOutputBuffer is the ONLY producer of
// SceneManagerIO::OutputBuffer::mPotentialContactQueue in the whole binary, i.e. it is
// the single seam between "the narrow phase resolved a car-vs-prop contact" and
// "PropEntityModule::ProcessPotentialContacts sees it". Until it existed, the
// already-real WorldModule::BridgeSceneContactsToPropModule_PrePhysics @0x827ABCB0
// drained a queue nothing ever filled.
//
// -------------------------------------------------------------------------------------
// THREE FACTS THAT ARE EASY TO GET WRONG AND ARE ALL MEASURED HERE
//
//  1. THE SELF-PAIR REJECT IS NOT A NULL/NONZERO TEST. scene_collision_scout.md (row 52)
//     describes @0x828BA538 as forwarding a pair when "both instances' +80 for-collision
//     words [are] nonzero", which is Hex-Rays' `if ( *(result + 80) )` taken literally.
//     The asm (0x828BA664..0x828BA67C) loads BOTH instances' +0x50 -- decimal 80, which is
//     VolumeInstance::mUserID, the packed VolumeInstanceId, NOT a flag word -- shifts each
//     right by 32 to take its embedded EntityId, compares them with `cmplw`, and SKIPS the
//     AddEvent when they are EQUAL. Implementing the scout's reading forwards EVERY pair
//     (mUserID is never 0 on a live instance) including every car-vs-its-own-volumes pair.
//     CgsVolumeInstance.h:67-82 records the same correction independently (cluster A2).
//
//  2. THE PAIR CARRIES POOL INDICES, NOT IDS. All three bodies range-check the queued
//     pair's two leading WORDS against KI_MAX_NUM_VOLUME_INSTANCES (`cmpwi r,0x13B8`) with
//     the CgsEntityManager.h:345 assert, then map each index -> VolumeInstanceId. The
//     index->id step is EntityManager::GetVolumeInstanceIdByIndex @0x828B9E10 folded inline:
//     the console emits that function out of line for other callers and its body is
//     byte-identical to what appears here (same assert at the same baked line, then
//     `mVolumeInstancePool[i]` via the const operator[] @0x828B71E8, then `ld r,0x50`).
//     The receiver the bridges pass, `this + 1874560`, is &mEntityManager.mVolumeInstancePool
//     (mEntityManager is at module +0x1A2480 per CgsSceneManagerModule.h:308, the pool at
//     EntityManager +0x27600 -- 0x1A2480 + 0x27600 == 0x1C9A80 == 1874560, exactly).
//     Those are console values quoted as provenance; nothing here is spelled as an offset.
//
//  3. THE "TOO MANY" ARM IS A LENGTH-vs-CAPACITY TEST ON THE **DESTINATION**, and both
//     output bridges print the SAME string, "WARNING: Too many contacts!\n", even the one
//     that is appending overlap PAIRS. That is the console's own copy/paste; it is
//     reproduced verbatim rather than "corrected" to say pairs.
//
// -------------------------------------------------------------------------------------
// LOCKING -- A PREREQUISITE THIS TU CANNOT SATISFY BY ITSELF (reported to cluster E1a).
// Every accessor these bodies call carries the console's own lock tripwire, and the
// console brackets the calls in UpdateContactGeneration @0x828D5CA0 with lock helpers
// that the tree's UpdateContactGeneration does not yet call:
//     0x828D5E3C  sub_823B6FE0(lpCullInput, lpGenOutput)   -> LockForWrite / LockForRead
//     0x828D5E4C  BridgeOverlapGenerationToOverlapCulling(lpCullInput, lpGenOutput)
//     0x828D5E58  sub_823B7060(lpCullInput, lpGenOutput)   -> UnlockForRead / UnlockForWrite
//     0x828D5F20  sub_823B70E0(lpSceneOutput, lpCullOutput, lpGenOutput)
//                                        -> LockForWrite / LockForRead / LockForRead
//     0x828D5F90  BridgeOverlapCullerToOutputBuffer(lpSceneOutput, lpCullOutput)
//     0x828D5FA0  BridgeOverlapGenerationToOutputBuffer(lpSceneOutput, lpGenOutput)
//     0x828D5FB0  sub_823B7190(lpSceneOutput, lpCullOutput, lpGenOutput)  -> the unlocks
// Without those brackets these bodies fire 2-4 "Not locked for reading/writing" asserts
// per call per frame. Exact insertion text is in scratchpad/waveQ5/f1.owner.md.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                           // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"                    // EntityManager::GetVolumeInstanceIdByIndex
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h"                  // OverlappingPair (broad-phase element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"              // Contact (narrow-phase element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                   // SceneManagerIO::OutputBuffer + its queue typedefs
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // SceneManagerIO::OutOverlapPair
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                 // VolumeInstanceId (+ its packed-field geometry)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"        // SceneManagerIO::PotentialContact
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapCullingModuleIO.h"    // OverlapCullingIO::{InputBuffer,OutputBuffer}
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h" // OverlapGenerationIO::OutputBuffer

#include <stdlib.h>   // getenv -- BRN_PROP_DIAG only, host-side diagnostic (see the [DIAG] block)

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// The console's index -> id step, folded inline into all three bodies.
//
// It is EntityManager::GetVolumeInstanceIdByIndex @0x828B9E10 verbatim: the same
// range assert (baked CgsEntityManager.h:345, "liIndex < KI_MAX_NUM_VOLUME_INSTANCES
// && liIndex >= 0"), the same const pool operator[] (@0x828B71E8), the same `ld r,0x50`
// == VolumeInstance::mUserID. Spelled here as the ordinary named call rather than
// re-inlined, per the project's inlining-reversal rule.
// ---------------------------------------------------------------------------

// The self-pair reject's comparand: the 32-bit EntityId word embedded in the HIGH dword
// of a VolumeInstanceId. The console folds it as `srdi r,id,32 ; clrlwi r,r,0` (a 64->32
// truncation of the shifted value) at 0x828BA664..0x828BA674. VolumeInstanceId exposes
// GetEntityIDOwner()/GetEntityIDEntityIndex() but not the whole word, so the splice is
// spelled from the type's own DWARF-attested geometry constant.
//
// ⚠️ NOT the owner byte alone. Two DIFFERENT props (same owner E_ENTITYTYPE_PROP,
// different entity indices) must still collide; only two volumes of the SAME entity are
// rejected. Comparing GetEntityIDOwner() would silently disable prop-vs-prop and
// car-vs-car collision entirely.
//
// REPORTED, NOT ADDED (see scratchpad/waveQ5/f1.owner.md): DecFIGS DWARF
// CgsVolumeInstanceId.h:85 declares `EntityId GetEntityId() const` -- the named accessor
// this expression is. CgsVolumeInstanceId.h is not this cluster's file, so the accessor
// is requested rather than added, and the fold is spelled locally in the meantime.
static u32 GetEmbeddedEntityIdWord(VolumeInstanceId lVolumeInstanceId)
{
    return static_cast<u32>(lVolumeInstanceId.muId
                            >> VolumeInstanceId::KU_ENTITY_ID_START_INDEX);
}

// ===========================================================================
// SceneManagerModule::BridgeOverlapGenerationToOverlapCulling  @ 0x828BA538  (90 insns)
//   console home CgsSceneManagerBridgeFunctions.cpp:2051
//
// Broad phase -> narrow phase. Walks the overlap generator's output pair queue and
// re-posts every pair whose two volume instances belong to DIFFERENT entities onto the
// overlap culler's input queue.
//
// Store for store:
//   assert(lpOverlapCullingInputBuffer     != NULL)                              // :2053
//   assert(lpOverlapGenerationOutputBuffer != NULL)                              // :2054
//   lpPairs  = lpOverlapGenerationOutputBuffer->GetOverlappingPairQueue();       // 0x828B0428 READ
//   lpCuller = lpOverlapCullingInputBuffer  ->GetOverlappingPairQueue();         // 0x828B07E8 WRITE
//   for (i = 0; i < lpPairs->GetLength(); ++i)                                   // `lwz r23,8(r26)`
//   {
//       pair = lpPairs->GetEvent(i);                                             // 0x828AE0D0
//       idA  = GetVolumeInstanceIdByIndex(pair.muVolumeInstanceA);               // assert + pool + ld 0x50
//       idB  = GetVolumeInstanceIdByIndex(pair.muVolumeInstanceB);
//       if (entityWord(idA) != entityWord(idB)) lpCuller->AddEvent(pair);        // 0x828B8B08
//   }
//
// Two shapes the asm settles that a rewrite would get wrong:
//   * The ORDER is A-assert, A-resolve, B-assert, B-resolve (0x828BA5F0 -> 0x828BA660):
//     the second index is not even loaded until the first instance has been fetched.
//     Reproduced, because an out-of-range A must trip its assert BEFORE B's is evaluated.
//   * The queue length is read ONCE before the loop into r23 and never re-loaded, even
//     though nothing inside can change it. Reproduced as a loop-invariant local.
//
// NO over-capacity guard here (unlike the two output bridges): the culler's input queue is
// 16384 deep, the same depth as the source, so the console does not test it.
// ===========================================================================
void SceneManagerModule::BridgeOverlapGenerationToOverlapCulling(
    OverlapCullingIO::InputBuffer* lpOverlapCullingInputBuffer,
    const OverlapGenerationIO::OutputBuffer* lpOverlapGenerationOutputBuffer)
{
    CGS_ASSERT(lpOverlapCullingInputBuffer != NULL,
               "lpOverlapCullingInputBuffer != NULL");                              // :2053
    CGS_ASSERT(lpOverlapGenerationOutputBuffer != NULL,
               "lpOverlapGenerationOutputBuffer != NULL");                          // :2054

    const OverlapGenerationIO::OutOverlappingPairQueue* const lpPairsQueue =
        lpOverlapGenerationOutputBuffer->GetOverlappingPairQueue();                 // 0x828B0428

    OverlapCullingIO::InputBuffer::InOverlappingPairQueue* const lpCullerQueue =
        lpOverlapCullingInputBuffer->GetOverlappingPairQueue();                     // 0x828B07E8

    const s32 liNumPairs = lpPairsQueue->GetLength();                               // 0x828BA5B4

    for (s32 liPair = 0; liPair < liNumPairs; ++liPair)
    {
        const OverlappingPair& lrPair = lpPairsQueue->GetEvent(liPair);             // 0x828AE0D0

        const VolumeInstanceId lVolumeInstanceIdA =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lrPair.muVolumeInstanceA));
        const VolumeInstanceId lVolumeInstanceIdB =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lrPair.muVolumeInstanceB));

        // SELF-PAIR REJECT (0x828BA664..0x828BA67C): two volume instances of the SAME
        // entity never reach the narrow phase -- an entity's internal volume pairs are the
        // separate OverlapCullingModule::ProcessInternalCollisions path.
        if (GetEmbeddedEntityIdWord(lVolumeInstanceIdA)
            != GetEmbeddedEntityIdWord(lVolumeInstanceIdB))
        {
            lpCullerQueue->AddEvent(lrPair);                                        // 0x828B8B08
        }
    }
}

// ===========================================================================
// SceneManagerModule::BridgeOverlapGenerationToOutputBuffer  @ 0x828BA6A0  (138 insns)
//   console home CgsSceneManagerBridgeFunctions.cpp:2092
//   (EXPORT HOLE -- asm from scratchpad/waveQ5/q5_out.json)
//
// Broad phase -> the scene manager's own output buffer. The RAW overlap pairs, with both
// pool indices resolved to packed ids, are published for consumers that want the pair
// list without the narrow phase: VehicleManager::StartVehicleContactGeneration
// @0x8262AEE8 and, through WorldModule::BridgeScenePotentialContactsToPhysics
// @0x827ABD80, the physics module.
//
// Store for store:
//   assert(lpSceneOutputBuffer != NULL)                                          // :2094
//   assert(lpOverlapGenerationOutputBuffer != NULL)                              // :2095
//   lpPairsQueue = lpOverlapGenerationOutputBuffer->GetOverlappingPairQueue();   // 0x828B0428 READ
//   assert(lpSceneOutputBuffer)                                                  // :2101
//   assert(lpPairsQueue)                                                         // :2102
//   for (i = 0; i < lpPairsQueue->GetLength(); ++i) {
//       pair = *lpPairsQueue->GetEvent(i);                       // BY VALUE: four `lwz`/`stw`
//                                                                //   into sp+var_90 (0x828BA7A0..)
//       out.muVolumeInstanceIdA = GetVolumeInstanceIdByIndex(pair.muVolumeInstanceA);
//       out.muVolumeInstanceIdB = GetVolumeInstanceIdByIndex(pair.muVolumeInstanceB);
//       out.mfPadding           = pair.mfPadding;                // `lfs f0` / `stfs f0`
//       if (dst->GetLength() >= dst->GetMaxLength()) print("WARNING: Too many contacts!\n");
//       else dst->AddEvent(out);                                 // 0x828AD390
//   }
//
// Deliberately reproduced oddities:
//   * The FOUR-word by-value copy of the source pair. The console materialises the whole
//     16-byte element on the stack before touching any field, including the mbCull word
//     it never reads. Modelled as a by-value local (the copy is what the console does; the
//     dead field is the compiler's, not an invention).
//   * mbCull is NOT consulted here. The cull verdict gates the NARROW phase
//     (OverlapCullingModule::ProcessOverlapsQueue tests it); the raw pair list is published
//     whole. An `if (!pair.mbCull)` guard here would withhold pairs the console publishes,
//     with no compile-time or run-time signal that it had.
//   * GetOverlapPairsQueue() is called THREE times per element (0x828BA854 / 0x828BA864 /
//     0x828BA87C) -- length, capacity, append. The console does not CSE it because each
//     call carries the write-lock tripwire as a side effect. Reproduced call for call.
//   * The over-capacity arm prints "WARNING: Too many contacts!\n" -- the CONTACT wording on
//     the PAIR queue is the console's own, kept verbatim (see the header note, fact 3).
// ===========================================================================
void SceneManagerModule::BridgeOverlapGenerationToOutputBuffer(
    SceneManagerIO::OutputBuffer* lpSceneOutputBuffer,
    const OverlapGenerationIO::OutputBuffer* lpOverlapGenerationOutputBuffer)
{
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");         // :2094
    CGS_ASSERT(lpOverlapGenerationOutputBuffer != NULL,
               "lpOverlapGenerationOutputBuffer != NULL");                          // :2095

    const OverlapGenerationIO::OutOverlappingPairQueue* const lpPairsQueue =
        lpOverlapGenerationOutputBuffer->GetOverlappingPairQueue();                 // 0x828B0428

    // The console re-asserts both handles AFTER the accessor -- the second pair of
    // tripwires is on the local it just produced, not on the parameters again.
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer");                 // :2101
    CGS_ASSERT(lpPairsQueue != NULL, "lpPairsQueue");                               // :2102

    const s32 liNumPairs = lpPairsQueue->GetLength();                               // 0x828BA75C

    for (s32 liPair = 0; liPair < liNumPairs; ++liPair)
    {
        const OverlappingPair lPair = lpPairsQueue->GetEvent(liPair);               // by-value, 4 words

        SceneManagerIO::OutOverlapPair lOutOverlapPair;
        lOutOverlapPair.muVolumeInstanceIdA =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lPair.muVolumeInstanceA));
        lOutOverlapPair.muVolumeInstanceIdB =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lPair.muVolumeInstanceB));
        lOutOverlapPair.mfPadding = lPair.mfPadding;                                // 0x828BA844/48

        if (lpSceneOutputBuffer->GetOverlapPairsQueue()->GetLength()
            >= lpSceneOutputBuffer->GetOverlapPairsQueue()->GetMaxLength())
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "WARNING: Too many contacts!\n";
            }
        }
        else
        {
            lpSceneOutputBuffer->GetOverlapPairsQueue()->AddEvent(lOutOverlapPair); // 0x828AD390
        }
    }
}

// ===========================================================================
// SceneManagerModule::BridgeOverlapCullerToOutputBuffer  @ 0x828BA8C8  (159 insns)
//   console home CgsSceneManagerBridgeFunctions.cpp:2141
//
// ⭐ THE POTENTIALCONTACT PRODUCER -- the single seam between the scene's narrow phase and
// the world. Every resolved CgsSceneManager::Contact becomes a
// SceneManagerIO::PotentialContact on SceneManagerIO::OutputBuffer::mPotentialContactQueue
// (console +32800), which is exactly the queue
// WorldModule::BridgeSceneContactsToPropModule_PrePhysics @0x827ABCB0 drains into
// PropEntityModule::ProcessPotentialContacts. Nothing else in the binary writes it.
//
// Store for store (the staged record is the 80-byte block at sp+var_C0):
//   assert(lpSceneOutputBuffer != NULL)                                          // :2143
//   assert(lpOverlapCullingOutputBuffer != NULL)                                 // :2144
//   lpContactQueue = lpOverlapCullingOutputBuffer->GetContactQueue();            // 0x828B0890 READ
//   assert(lpSceneOutputBuffer)                                                  // :2150
//   assert(lpContactQueue)                                                       // :2151
//   for (i = 0; i < lpContactQueue->GetLength(); ++i) {
//       c = lpContactQueue->GetEvent(i);                                         // 0x828AE2D0
//       pc.mPointOnA = c.mPointOnA;                       // lvx/stvx c+0x00 -> event+0x00
//       pc.mPointOnB = c.mPointOnB;                       // lvx/stvx c+0x10 -> event+0x10
//       pc.mNormal   = c.mNormal;                         // lvx/stvx c+0x20 -> event+0x20
//       pc.muVolumeInstanceIdA = GetVolumeInstanceIdByIndex(c.muVolumeInstanceA); // event+0x30
//       pc.muVolumeInstanceIdB = GetVolumeInstanceIdByIndex(c.muVolumeInstanceB); // event+0x38
//       pc.muPolyTagA = c.muPolyTagA;                     // lwz c+0x38 -> event+0x40
//       pc.muPolyTagB = c.muPolyTagB;                     // lwz c+0x3C -> event+0x44
//       if (dst->GetLength() >= dst->GetMaxLength()) print("WARNING: Too many contacts!\n");
//       else dst->AddEvent(pc);                                                  // 0x828AD230
//   }
//
// ⚠️ FAITHFUL, AND A REAL HAZARD: pc.mu16PrimitiveIndexA / mu16PrimitiveIndexB
// (PotentialContact +0x48 / +0x4A) are NEVER WRITTEN. There is no store to sp+var_C0+0x48
// anywhere in the body and no PotentialContact::Construct() call, while
// BaseEventQueue<PotentialContact>::AddEvent @0x828AD230 copies the FULL 80 bytes
// (`li r9,0xA` + a 10x ld/std loop, stride (len*5)<<4 == len*80). So the console publishes
// two indeterminate stack bytes in every potential contact this bridge produces, and any
// consumer calling GetPrimitiveIndexA()/GetPrimitiveIndexB() on one reads garbage. That is
// what the binary does; it is reproduced, NOT "fixed" with a zero-init the console has no
// instruction for. Flagged in scratchpad/waveQ5/f1.owner.md as a console defect to watch if
// a prop-contact consumer ever starts using the primitive index.
//
// LANE-COPY NOTE (host type seam, not a divergence): the three 16-byte lanes are typed
// Vector4 on CgsSceneManager::Contact and Vector3 on SceneManagerIO::PotentialContact.
// Both are the same 16-byte 4-lane POD (rw::math::vpu::Vector3 / Vector4 in
// vendor/renderware/include/rw/math/vpu/types.h) and the console moves each with one
// lvx/stvx pair, but they are DISTINCT structs on the host, so the copy is spelled
// lane-for-lane. It collapses to a plain assignment the moment
// CgsSceneManagerContact.h's Vector4 members are retyped to the DWARF's Vector3 (a delta
// cluster E3a already recorded in that header, together with the mPosition -> mPointOnA /
// mImpulse -> mPointOnB / muFieldA -> muPolyTagA / muFieldB -> muPolyTagB renames the
// DWARF attests at CgsSceneManagerTypes.h:104-110). The old names are used below because
// that header is not this cluster's grant; each site names the DWARF truth beside it.
// ===========================================================================
void SceneManagerModule::BridgeOverlapCullerToOutputBuffer(
    SceneManagerIO::OutputBuffer* lpSceneOutputBuffer,
    const OverlapCullingIO::OutputBuffer* lpOverlapCullingOutputBuffer)
{
    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");         // :2143
    CGS_ASSERT(lpOverlapCullingOutputBuffer != NULL,
               "lpOverlapCullingOutputBuffer != NULL");                             // :2144

    const OverlapCullingIO::OutputBuffer::OutContactQueue* const lpContactQueue =
        lpOverlapCullingOutputBuffer->GetContactQueue();                            // 0x828B0890

    CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer");                 // :2150
    CGS_ASSERT(lpContactQueue != NULL, "lpContactQueue");                           // :2151

    const s32 liNumContacts = lpContactQueue->GetLength();                          // 0x828BA984

    // [DIAG] NOT IN THE X360 BINARY. Opt-in one-shot (set BRN_PROP_DIAG): the first frame
    // the narrow phase hands this bridge anything at all. A silent log means the culler is
    // still producing nothing -- a DIFFERENT failure from "the world dropped the contacts",
    // which WorldBridgePropModule.cpp's own "[Q5-world] first N potential contacts" line
    // reports. The getenv latch is evaluated once; a per-frame getenv would be a syscall.
    {
        static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
        static bool       sbLoggedFirstContacts = false;
        if (sbPropDiag && !sbLoggedFirstContacts && liNumContacts > 0
            && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedFirstContacts = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q5-bridge] first PotentialContact(s) " << liNumContacts
                << " offered by the culler\n";
        }
    }

    for (s32 liContact = 0; liContact < liNumContacts; ++liContact)
    {
        const Contact& lrContact = lpContactQueue->GetEvent(liContact);             // 0x828AE2D0

        SceneManagerIO::PotentialContact lPotentialContact;

        // mPosition / mImpulse are Contact::mPointOnA / ::mPointOnB (DWARF
        // CgsSceneManagerTypes.h:104/:105) -- see the LANE-COPY NOTE above.
        lPotentialContact.mPointOnA.x = lrContact.mPosition.x;
        lPotentialContact.mPointOnA.y = lrContact.mPosition.y;
        lPotentialContact.mPointOnA.z = lrContact.mPosition.z;
        lPotentialContact.mPointOnA.w = lrContact.mPosition.w;

        lPotentialContact.mPointOnB.x = lrContact.mImpulse.x;
        lPotentialContact.mPointOnB.y = lrContact.mImpulse.y;
        lPotentialContact.mPointOnB.z = lrContact.mImpulse.z;
        lPotentialContact.mPointOnB.w = lrContact.mImpulse.w;

        lPotentialContact.mNormal.x = lrContact.mNormal.x;
        lPotentialContact.mNormal.y = lrContact.mNormal.y;
        lPotentialContact.mNormal.z = lrContact.mNormal.z;
        lPotentialContact.mNormal.w = lrContact.mNormal.w;

        lPotentialContact.muVolumeInstanceIdA =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lrContact.muVolumeInstanceA));
        lPotentialContact.muVolumeInstanceIdB =
            mEntityManager.GetVolumeInstanceIdByIndex(static_cast<s32>(lrContact.muVolumeInstanceB));

        // muFieldA / muFieldB are Contact::muPolyTagA / ::muPolyTagB (DWARF :109/:110),
        // sourced from rw::collision::PrimitivePairIntersectResult::tag1 / tag2.
        lPotentialContact.muPolyTagA = lrContact.muFieldA;
        lPotentialContact.muPolyTagB = lrContact.muFieldB;

        // mu16PrimitiveIndexA / mu16PrimitiveIndexB deliberately left unset -- see the
        // "FAITHFUL, AND A REAL HAZARD" note above.

        if (lpSceneOutputBuffer->GetPotentialContactQueue()->GetLength()
            >= lpSceneOutputBuffer->GetPotentialContactQueue()->GetMaxLength())
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "WARNING: Too many contacts!\n";
            }
        }
        else
        {
            lpSceneOutputBuffer->GetPotentialContactQueue()->AddEvent(lPotentialContact); // 0x828AD230
        }
    }
}

}
