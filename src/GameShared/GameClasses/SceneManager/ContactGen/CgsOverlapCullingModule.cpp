// CgsSceneManager::OverlapCullingModule -- contact-generation NARROW PHASE of the
// SceneManager. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x828C18E8   (EXECUTED in boot trace)
//   Prepare                       @ 0x828B52B8
//   Release                       @ 0x828AB448
//   Update                        @ 0x828D1D50
//   ProcessAddInternalVolumeQueue @ 0x828B5420
//   ProcessOverlapsQueue          @ 0x828D0330
//   ProcessOverlap                @ 0x828CAF38
//   DoPairQuery                   @ 0x828C1A18
//
// ===========================================================================
// ⭐ WAVE Q5 / CLUSTER E3a (2026-08-19) -- THE PAIR QUERY IS NOW REAL.
//
// Until today the four functions from ProcessAddInternalVolumeQueue down were
// documented SHELLS: ProcessOverlapsQueue reset its counters and never walked the
// queue, ProcessOverlap was empty, DoPairQuery was `return 0`. The file-level FLAG
// that justified them ("UNRECOVERED IO-BUFFER ACCESSORS ... naming them would require
// fabricating their signatures/types") is RETIRED -- every one of those accessors has
// a real, asm-attested home now:
//
//   sub_828B0698  -> OverlapCullingIO::InputBuffer::GetOverlappingPairQueue() const
//                    (read-lock bit 4, `this+4`, baked CgsOverlapCullingModuleIO.h:87)
//   sub_828B0740  -> OverlapCullingIO::InputBuffer::GetAddInternalVolumeQueue() const
//                    (read-lock bit 4, `this+0x40010`, baked ...ModuleIO.h:88)
//   0x828B0938    -> OverlapCullingIO::OutputBuffer::GetContactQueue()
//                    (WRITE-lock bit 3, `this+0x10`, baked ...ModuleIO.h:129)
//   "CgsSceneManager::Overlap" @0x828AE0D0
//                 -> BaseEventQueue<OverlappingPair>::GetEvent(s32)   (stride 0x10)
//   sub_828AE220  -> BaseEventQueue<AddInternalCollisionVolume>::GetEvent(s32)
//                    (stride 0x0C)
//   "CgsSceneManager::Contact_::AddEventSafe" @0x828B8D08
//                 -> BaseEventQueue<Contact>::AddEventSafe(const Contact&)
//   rw::collision::PrimitivePairIntersect / VolumeVolumeQuery::GetPrimitiveIntersections
//                 -> vendor/renderware/collision/{GPInstance,VolumeQuery}.hpp
// (The three truncated IDA names above are AGENTS gotcha 6 in action -- a truncated
// symbol, not a missing function.)
//
// TWO ELEMENT TYPES HAD TO BE CORRECTED FIRST, both outside this cluster's file grant
// and both reported in scratchpad/waveQ5/e3a.owner.md:
//   * CgsSceneManager::OverlappingPair was declared as two 64-bit VolumeInstanceIds.
//     It is {u32 muVolumeInstanceA, u32 muVolumeInstanceB, f32 mfPadding, bool mbCull}
//     -- same 16-byte stride, completely different field roles. See that header.
//   * CgsSceneManager::Contact's `u8 maReserved30[8]` is muVolumeInstanceA /
//     muVolumeInstanceB (DWARF CgsSceneManagerTypes.h:107/:108); DoPairQuery is their
//     only producer.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h"

#include <cstddef>  // offsetof
#include <cstring>  // memset
#include <stdlib.h> // getenv ([DIAG] BRN_PROP_DIAG, host only)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"           // EntityManager, VolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"           // VolumeManager::GetRwVolume
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"     // CgsSceneManager::Contact (the emitted event)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsContactGenerationIO.h"  // OverlapCullingIO::InputBuffer / OutputBuffer
#include "vendor/renderware/collision/CollisionVolume.hpp"                  // rw::collision::Volume + E_VOLUMETYPE_*
#include "vendor/renderware/collision/GPInstance.hpp"                       // PrimitivePairIntersect(+Result)
#include "vendor/renderware/collision/VolumeQuery.hpp"                      // rw::collision::VolumeVolumeQuery

namespace CgsSceneManager
{

namespace
{
    // -----------------------------------------------------------------------
    // Contact::SourceResult vs rw::collision::PrimitivePairIntersectResult.
    //
    // The DWARF declares `Contact::Construct(PrimitivePairIntersectResult*, uint32_t)`
    // (CgsSceneManagerTypes.h:102). This tree reached that record before the vendor
    // narrow-phase header existed and forked it as the nested Contact::SourceResult;
    // the vendor type now has a real home (GPInstance.hpp:320). Retyping
    // Contact::Construct's parameter changes its mangled symbol and drags the vendor
    // header into every includer of CgsSceneManagerContact.h, so it is left to that
    // file's owner and DoPairQuery casts between the two spellings instead -- the same
    // "two spellings of one type" move CgsVolumeManager.cpp:212 already makes for
    // VolRef::Volume / rw::collision::Volume.
    //
    // These pins are the safety net: if either spelling drifts, this TU stops
    // compiling instead of silently reading the wrong lane.
    typedef rw::collision::PrimitivePairIntersectResult RwPairResult;
    static_assert(sizeof(Contact::SourceResult) == sizeof(RwPairResult),
                  "Contact::SourceResult must be the 0x750 PrimitivePairIntersectResult record");
    static_assert(offsetof(Contact::SourceResult, muFieldA) == offsetof(RwPairResult, tag1),
                  "Contact::SourceResult::muFieldA is PrimitivePairIntersectResult::tag1");
    static_assert(offsetof(Contact::SourceResult, muFieldB) == offsetof(RwPairResult, tag2),
                  "Contact::SourceResult::muFieldB is PrimitivePairIntersectResult::tag2");
    static_assert(offsetof(Contact::SourceResult, mNormal) == offsetof(RwPairResult, normal),
                  "Contact::SourceResult::mNormal is PrimitivePairIntersectResult::normal");
    static_assert(offsetof(Contact::SourceResult, maPositions) == offsetof(RwPairResult, pointsOn1),
                  "Contact::SourceResult::maPositions is PrimitivePairIntersectResult::pointsOn1");
    static_assert(offsetof(Contact::SourceResult, maImpulses) == offsetof(RwPairResult, pointsOn2),
                  "Contact::SourceResult::maImpulses is PrimitivePairIntersectResult::pointsOn2");
    static_assert(offsetof(Contact::SourceResult, muNumPoints) == offsetof(RwPairResult, numPoints),
                  "Contact::SourceResult::muNumPoints is PrimitivePairIntersectResult::numPoints");

    // -----------------------------------------------------------------------
    // The per-result contact emission block.
    //
    // The X360 emits this block TWICE, inline and instruction-identical, once per
    // DoPairQuery arm -- 0x828C1B0C..0x828C1B80 (prim x prim) and
    // 0x828C1C68..0x828C1CFC (prim x aggregate). The ONLY difference between the two
    // copies is which statistics counter the tail increments (muNumPrimPrimContacts
    // @+0x6C290 vs muNumPrimAggContacts @+0x6C294), so it is de-inlined into one
    // helper per the AGENTS "inlining reversal" rule with that counter passed by
    // reference. Store for store, per contact point:
    //
    //   Contact::Construct(&lContact, lpResult, luPoint)   ; 0x828C1B18 / 0x828C1C74
    //   lContact.muVolumeInstanceA = <arg r6>              ; stw r17, +0x30
    //   lContact.muVolumeInstanceB = <arg r9>              ; stw r16, +0x34
    //   queue = lpOutputBuffer->GetContactQueue()          ; 0x828C1B2C / 0x828C1C88
    //   if (!queue->AddEventSafe(lContact) && (gxMessageFilterFlags & 1))
    //       *gpDebugPrint << "CgsSceneManager::OverlapCullingModule::DoPairQuery"
    //                     << ": Failed to add contact event - queue is full\n";
    //   ++<counter>
    //
    // NOTE the two stamps happen AFTER Construct (which does not touch +0x30/+0x34)
    // and BEFORE the push -- and that the counter is bumped whether or not the push
    // succeeded, which is the console's own behaviour, not an oversight.
    // NOTE the queue handle is re-fetched from the buffer on EVERY point (the console
    // re-calls the write-locked accessor inside the loop); reproduced, because that
    // accessor carries the "Not locked for writing" tripwire.
    // -----------------------------------------------------------------------
    void EmitContactPoints(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                           const RwPairResult&             lrResult,
                           u32                             luVolumeInstanceIndexA,
                           u32                             luVolumeInstanceIndexB,
                           u32&                            lruContactCounter,
                           u32&                            lruDiagContactCount)
    {
        for (u32 luPoint = 0; luPoint < lrResult.numPoints; ++luPoint)
        {
            Contact lContact;
            lContact.Construct(reinterpret_cast<const Contact::SourceResult*>(&lrResult), luPoint);

            lContact.muVolumeInstanceA = luVolumeInstanceIndexA;
            lContact.muVolumeInstanceB = luVolumeInstanceIndexB;

            const bool lbAdded = lpOutputBuffer->GetContactQueue()->AddEventSafe(lContact);
            if (!lbAdded)
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "CgsSceneManager::OverlapCullingModule::DoPairQuery"
                        << ": Failed to add contact event - queue is full\n";
                }
            }
            else
            {
                // [DIAG] NOT IN THE X360 BINARY. One-shot: the first contact the
                // narrow phase ever pushes is the single line that says the scene
                // volume-collision middle is alive. Opt in with BRN_PROP_DIAG.
                static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
                static bool       sbLoggedFirstContact = false;
                if (sbPropDiag && !sbLoggedFirstContact && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbLoggedFirstContact = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q5-cull] first CONTACT emitted: instances "
                        << static_cast<s32>(luVolumeInstanceIndexA) << "/"
                        << static_cast<s32>(luVolumeInstanceIndexB) << "\n";
                }
            }

            ++lruContactCounter;
            ++lruDiagContactCount;
        }
    }
}  // namespace

// ---------------------------------------------------------------------------
// Default construction. The X360 RWMutex/vtable wiring lives in the embedded base;
// the module's own counters/pointers are set by Construct, so the ctor is the trivial
// member-wise default (the bulk scratch buffer is intentionally left uninitialised --
// Construct partitions it).
// ---------------------------------------------------------------------------
OverlapCullingModule::OverlapCullingModule()
    : mePrepareStage(PREPARESTAGE_START)
    , meReleaseStage(RELEASESTAGE_DONE)
    , mpVolumeManager(nullptr)
    , mpEntityManager(nullptr)
    , mpContactGenerator(nullptr)
    , mpVolVolQuery(nullptr)
    , muNumPrimPrimPairs(0)
    , muNumPrimAggPairs(0)
    , muNumOtherPairs(0)
    , muNumInstanceQueries(0)
    , muNumPPQs(0)
    , muNumIntersections(0)
    , muNumPrimPrimContacts(0)
    , muNumPrimAggContacts(0)
{
}

// ---------------------------------------------------------------------------
// Construct @ 0x828C18E8   (EXECUTED in boot trace)
//
//   ModuleSingleBuffered::Construct(this);          ; base chain
//   meReleaseStage = 2 (RELEASESTAGE_DONE);          ; *(this+556) = 2
//   mePrepareStage = 0 (PREPARESTAGE_START);         ; *(this+552) = 0
//   desc = VolumeVolumeQuery::GetResourceDescriptor(scratch, 100, 100);
//   copy 10 words of the descriptor out;             ; do { *v3++ = *desc++; } x10
//   assert(desc.size       <= 0x62000, "VolumeVolumeQueryMem is too small");      ; v8 > 0x62000
//   assert(desc.alignment  == 16,      "VolumeVolumeMem alignment ... GTALIGN");  ; v9 != 16
//   assert((this+576) % alignment == 0, "VolumeVolumeQueryMem isn't aligned properly");
//   buf[0] = this+576 (maVolumeVolumeQueryMem); buf[1..4] = 0;
//   mpVolVolQuery = VolumeVolumeQuery::Initialize(buf, 100, 100);   ; *(this+572)
//   mbIsNewModule = true;                            ; *(this+4) = 1
//
// The 0x62000 (401408) budget, the 16-byte GTALIGN, and the 100/100 volume/result
// counts are all asm immediates. The descriptor is the 5-entry rw::ResourceDescriptor
// block (10 words copied out); only its size word (v8) and alignment word (v9) are
// consulted, so a small scratch covers it exactly as the asm does.
//
// The 100-result count is what DoPairQuery's "luNumIntersections <= 100" assert
// (.cpp:547) is checking against -- the same immediate, from the other end.
// ---------------------------------------------------------------------------
void OverlapCullingModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();   // base chain

    meReleaseStage = RELEASESTAGE_DONE;   // *(this+556) = 2
    mePrepareStage = PREPARESTAGE_START;  // *(this+552) = 0

    // The descriptor is copied out as 10 words; the asm then reads copied word 0 as the
    // size and copied word 1 as the alignment. Mirror the asm's 10-word copy into a local
    // scratch.
    u32 laDesc[10];
    {
        const u32* lpDesc = static_cast<const u32*>(
            rw::collision::VolumeVolumeQuery::GetResourceDescriptor(laDesc, 100, 100));
        for (s32 li = 0; li < 10; ++li)
        {
            laDesc[li] = lpDesc[li];
        }
    }
    // The asm consults the FIRST two copied words: lwz r10,var_90 (copied word 0) is the
    // size compared against 0x62000, and lwz r27,var_8C (var_90+4, copied word 1) is the
    // alignment compared against 16. (var_90 is the destination of the 10-word copy.)
    const u32 luQuerySize      = laDesc[0];  // var_90: copied word 0
    const u32 luQueryAlignment = laDesc[1];  // var_8C: copied word 1

    CGS_ASSERT(luQuerySize <= 0x62000, "VolumeVolumeQueryMem is too small");
    CGS_ASSERT(luQueryAlignment == 16,
               "VolumeVolumeMem alignment has changed. Changed the GTALIGN size");
    CGS_ASSERT(luQueryAlignment != 0 &&
                   (reinterpret_cast<uintptr_t>(maVolumeVolumeQueryMem) % luQueryAlignment) == 0,
               "VolumeVolumeQueryMem isn't aligned properly");

    void* lpBuffer[5];
    lpBuffer[0] = maVolumeVolumeQueryMem;  // this+576: backing-store base
    lpBuffer[1] = nullptr;
    lpBuffer[2] = nullptr;
    lpBuffer[3] = nullptr;
    lpBuffer[4] = nullptr;
    mpVolVolQuery = static_cast<rw::collision::VolumeVolumeQuery*>(
        rw::collision::VolumeVolumeQuery::Initialize(lpBuffer, 100, 100));  // *(this+572)

    mbIsNewModule = true;  // *(this+4) = 1
}

// ---------------------------------------------------------------------------
// Destruct @ 0x828C... (trivial: the query object lives in the in-class scratch buffer,
// so there is nothing to free; the X360 destruct only runs the base teardown via the
// vtable. Modelled as the base chain.)
// ---------------------------------------------------------------------------
void OverlapCullingModule::Destruct()
{
    CgsModule::ModuleSingleBuffered::Destruct();
}

// ---------------------------------------------------------------------------
// Prepare @ 0x828B52B8
//
// Entry stage 0 (START): fall straight into the advance. Stage 1 (MANAGER): chain the
// base Prepare, wire up the managers, zero the per-frame state, advance MANAGER->DONE.
// Stage 2 (DONE): reset to START then run the advance. Any other value asserts
// "Unrecognised prepare stage" and returns false. The post-increment asserts the stage
// never exceeds PREPARESTAGE_DONE.
//
// The asm structure: if (mePrepareStage == START) goto advance; else if (!=MANAGER)
// { if (>=3) assert+return 0; else mePrepareStage = START; goto advance; }; advance:
// if (ModuleSingleBuffered::Prepare()) { assert managers; wire managers; memset 632
// bytes of internal-volume state; ++stage; meReleaseStage = START; return 1; } else
// return 0.
// ---------------------------------------------------------------------------
bool OverlapCullingModule::Prepare(EntityManager* lpEntityManager, VolumeManager* lpVolumeManager)
{
    if (mePrepareStage != PREPARESTAGE_START)
    {
        if (mePrepareStage != PREPARESTAGE_MANAGER)
        {
            if (mePrepareStage >= 3)
            {
                CGS_ASSERT(false, "Unrecognised prepare stage");
                return false;
            }
            // PREPARESTAGE_DONE: restart the handshake.
            mePrepareStage = PREPARESTAGE_START;
        }
    }

    if (mePrepareStage == PREPARESTAGE_START)
    {
        // Stage advance from START (the asm's LABEL_7): ++stage and bound-check.
        mePrepareStage++;
        CGS_ASSERT(mePrepareStage <= PREPARESTAGE_DONE,
                   "leEnumIndex <= OverlapCullingModule::PREPARESTAGE_DONE");
    }

    if (!CgsModule::ModuleSingleBuffered::Prepare())
    {
        return false;
    }

    CGS_ASSERT(lpVolumeManager != nullptr, "lpVolManager");
    CGS_ASSERT(lpEntityManager != nullptr, "lpEntityManager");

    mpVolumeManager = lpVolumeManager;  // a1[140]
    mpEntityManager = lpEntityManager;  // a1[141]

    // memset(a1 + 100496, 0, 632): zero the internal-collision registration BitArray
    // (632 bytes == BitArray<5048>'s 79 u64 words). `a1` is a _DWORD* in the
    // pseudocode, so 100496 dwords == byte offset 401984 -- the same base the
    // ProcessAddInternalVolumeQueue / ProcessInternalCollisions bodies reach with
    // `addis rN,rThis,6 ; addi rN,rN,0x2240`.
    std::memset(&mabIsUsingInternalCollision, 0, 632);

    mePrepareStage++;  // MANAGER -> DONE
    CGS_ASSERT(mePrepareStage <= PREPARESTAGE_DONE,
               "leEnumIndex <= OverlapCullingModule::PREPARESTAGE_DONE");

    meReleaseStage = RELEASESTAGE_START;  // a1[139] = 0
    return true;
}

// ---------------------------------------------------------------------------
// Release @ 0x828AB448
//
// Mirror handshake over meReleaseStage. Entry stage 0 (START): run the manager-release
// teardown (the X360 sub_828AA2D8(&meReleaseStage, 0) helper) then chain the base
// Release and run the post-base teardown. Stage 1 (MANAGER): chain the base Release + the
// post-base teardown (no pre-base teardown). Stage 2 (DONE): drop straight to the reset.
// Any other value asserts "Bad prepare state" and returns false. On completion
// mePrepareStage resets to START and the function returns true.
//
// NOTE: the START branch invokes the unreconstructed teardown helper
// sub_828AA2D8(&meReleaseStage) twice (before and after the base Release). Its behaviour
// is not grounded, so it is intentionally NOT modelled beyond the surrounding stage
// transitions the asm makes observable. (Flagged in stubs_needed.)
// ---------------------------------------------------------------------------
bool OverlapCullingModule::Release()
{
    const EReleaseStage leStage = meReleaseStage;  // v3 = *(a1+556)

    if (leStage != RELEASESTAGE_START)
    {
        if (leStage != RELEASESTAGE_MANAGER)
        {
            if (leStage >= 3)
            {
                CGS_ASSERT(false, "Bad prepare state");
                return false;
            }
            // RELEASESTAGE_DONE: drop straight to the reset (LABEL_9).
        }
    }
    else
    {
        // RELEASESTAGE_START: sub_828AA2D8(&meReleaseStage, 0) -- unreconstructed manager
        // teardown (pre-base-release).
    }

    // The asm runs the base Release for BOTH START(0) and MANAGER(1): cmplwi r11,1 then
    // blt loc_828AB4A4 (START) falls through into loc_828AB4B0, and beq loc_828AB4B0
    // (MANAGER) lands there directly -- both reach the base-Release + post-base teardown.
    // Only DONE(2) takes blt loc_828AB4D8 and skips straight to the reset.
    if (leStage != RELEASESTAGE_DONE)
    {
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
        // sub_828AA2D8(&meReleaseStage, 0) -- unreconstructed manager teardown (post-base-release).
    }

    mePrepareStage = PREPARESTAGE_START;  // *(a1+552) = 0
    return true;
}

// ---------------------------------------------------------------------------
// Update @ 0x828D1D50  (the vtable Update slot; DWARF CgsOverlapCullingModule.cpp:207)
//
//   assert( lpInputBuffer  != NULL )                 ; li r5,0xD1  -> .cpp:209
//   assert( lpOutputBuffer != NULL )                 ; li r5,0xD2  -> .cpp:210
//   lpInputBuffer->LockForRead();                    ; 0x828D1DBC
//   lpOutputBuffer->LockForWrite();                  ; 0x828D1DC4
//   ProcessAddInternalVolumeQueue( lpInputBuffer );  ; 0x828D1DD0
//   ProcessInternalCollisions( lpOutputBuffer );     ; 0x828D1DDC
//   ProcessOverlapsQueue( lpOutputBuffer, lpInputBuffer ); ; 0x828D1DEC
//   lpInputBuffer->UnlockForRead();                  ; 0x828D1DF4
//   lpOutputBuffer->UnlockForWrite();                ; 0x828D1DFC
//
// ⚠️ CORRECTED 2026-08-19 (E3b reported it; re-measured here off the asm above). The
// previous body had FOUR divergences, and three of them were live:
//   (a) IT DID NOT LOCK EITHER BUFFER. Every queue accessor this module reaches for
//       carries the console's own lock tripwire ("Not locked for reading\n" /
//       "Not locked for writing\n", baked CgsOverlapCullingModuleIO.h:87/:88/:129), so
//       the moment the queue walks below became real, an unlocked Update would have
//       fired an assert per pair per frame. The locks are the console's, in the
//       console's order, and they are load-bearing.
//   (b) it called ProcessRemoveInternalVolumeQueue, which Update @0x828D1D50 does NOT
//       call -- there is no `bl` to it anywhere in the body.
//   (c) it called ProcessAccumulatedQueries, likewise absent.
//   (d) it guarded the input legs with `if (lpInputBuffer != nullptr)`; the console
//       asserts and carries on (a non-gating tripwire), it does not branch.
// (b) and (c) leave those two methods with no caller in this build; both keep their
// declarations and their documented bodies -- see the note at each.
// ---------------------------------------------------------------------------
void OverlapCullingModule::Update(OverlapCullingIO::InputBuffer* lpInputBuffer,
                                  OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");    // .cpp:209
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");  // .cpp:210

    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();

    ProcessAddInternalVolumeQueue(lpInputBuffer);
    ProcessInternalCollisions(lpOutputBuffer);
    ProcessOverlapsQueue(lpOutputBuffer, lpInputBuffer);

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
}

// SceneManagerModule-facing entry (UpdateContactGeneration): forwards to the typed Update.
void OverlapCullingModule::CullOverlaps(void* lpInputBuffer, void* lpOutputBuffer)
{
    Update(static_cast<OverlapCullingIO::InputBuffer*>(lpInputBuffer),
           static_cast<OverlapCullingIO::OutputBuffer*>(lpOutputBuffer));
}

void OverlapCullingModule::SetContactGenerator(void* lpContactGenerator)
{
    mpContactGenerator = lpContactGenerator;  // X360 ContactGenerator* (opaque here)
}

// ---------------------------------------------------------------------------
// ProcessAddInternalVolumeQueue @ 0x828B5420  (DWARF CgsOverlapCullingModule.cpp:307)
//
//   assert( lpInputBuffer != NULL )                             ; li r5,0x135 -> :309
//   queue = lpInputBuffer->GetAddInternalVolumeQueue();          ; bl sub_828B0740
//   for ( i = 0 ; i < queue->GetLength() ; ++i )                 ; lwz r11,8(r18)
//   {
//       lEvent = queue->GetEvent(i);                             ; bl sub_828AE220 (stride 0xC)
//       assert( lEvent.muVolumeInstanceIndex         < 0x13B8 )  ; li r5,0x141 -> :321
//       assert( lEvent.muInternalVolumeInstanceIndex < 0x13B8 )  ; li r5,0x142 -> :322
//       assert( lEvent.muEscapeVolumeInstanceIndex   < 0x13B8 )  ; li r5,0x143 -> :323
//       mabIsUsingInternalCollision.SetBit( lEvent.muVolumeInstanceIndex );
//       mauInternalVolumeInstanceIndex[ ... ] = lEvent.muInternalVolumeInstanceIndex;
//       mauEscapeVolumeInstanceIndex  [ ... ] = lEvent.muEscapeVolumeInstanceIndex;
//   }
//
// LANDED 2026-08-19 (was a bodyless shell with only the first assert). The two tables
// are reached BY NAME; the console immediates that prove the header's member order are
//   SetBit base   this + 6*65536 + 0x2240        == 401984  (mabIsUsingInternalCollision)
//   internal[i]   (i + 0x1892E)*4  == i*4 + 402616
//   escape[i]     (i + 0x19CE6)*4  == i*4 + 422808  ( == 402616 + 5048*4 )
// -- i.e. exactly BitArray<5048>, u32[5048], u32[5048] back to back, which is what the
// header declares and what E3b's ProcessInternalCollisions independently measured.
//
// The X360 also carries the BitArray's own over-capacity tripwire inlined into SetBit
// (CgsBitArray.h:222, message streamed as "Index: " << i << ", Number of bits: " <<
// 5048). CgsBitArray.h keeps its methods assert-free BY DESIGN -- see its banner at
// CgsBitArray.h:15-17, which states that the bounds asserts the DWARF shows guarding
// these methods are emitted AT THE CALL SITES by callers that own the CgsDev::Assert
// API, so the header stays free of the assert-system dependency. The :222 tripwire is
// therefore the CALLER's to write, per the convention CgsSceneSweeper_wQ5_01.cpp and
// BrnCameraValidityAccount.cpp already follow; it is emitted before the SetBit below.
//
// FAITHFUL: all three asserts are NON-GATING -- the console fires them and then does the
// three stores anyway (there is no branch around loc_828B568C). An out-of-range index
// would corrupt memory on the console too; no guard is invented here.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessAddInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");   // :309

    const OverlapCullingIO::InAddInternalVolumeQueue* lpQueue =
        lpInputBuffer->GetAddInternalVolumeQueue();

    for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
    {
        const OverlapCullingIO::AddInternalCollisionVolume& lrEvent = lpQueue->GetEvent(liEvent);

        CGS_ASSERT(lrEvent.muVolumeInstanceIndex < KU_MAX_NUM_VOLUME_INSTANCES,
                   "lEvent.muVolumeInstanceIndex < static_cast<uint32_t>( KI_MAX_NUM_VOLUME_INSTANCES )");          // :321
        CGS_ASSERT(lrEvent.muInternalVolumeInstanceIndex < KU_MAX_NUM_VOLUME_INSTANCES,
                   "lEvent.muInternalVolumeInstanceIndex < static_cast<uint32_t>( KI_MAX_NUM_VOLUME_INSTANCES )");  // :322
        CGS_ASSERT(lrEvent.muEscapeVolumeInstanceIndex < KU_MAX_NUM_VOLUME_INSTANCES,
                   "lEvent.muEscapeVolumeInstanceIndex < static_cast<uint32_t>( KI_MAX_NUM_VOLUME_INSTANCES )");    // :323

        CGS_ASSERT(lrEvent.muVolumeInstanceIndex < KU_MAX_NUM_VOLUME_INSTANCES,
                   "Index < Number of bits");   // CgsBitArray.h:222
        mabIsUsingInternalCollision.SetBit(lrEvent.muVolumeInstanceIndex);
        mauInternalVolumeInstanceIndex[lrEvent.muVolumeInstanceIndex] =
            lrEvent.muInternalVolumeInstanceIndex;
        mauEscapeVolumeInstanceIndex[lrEvent.muVolumeInstanceIndex] =
            lrEvent.muEscapeVolumeInstanceIndex;
    }
}

// ---------------------------------------------------------------------------
// ProcessOverlapsQueue @ 0x828D0330  (DWARF CgsOverlapCullingModule.cpp:237)
//
//   assert( lpInputBuffer  != NULL )      ; li r5,0xEF -> :239
//   assert( lpOutputBuffer != NULL )      ; li r5,0xF0 -> :240
//   muNumPrimPrimPairs = muNumPrimAggPairs = muNumOtherPairs = muNumInstanceQueries
//     = muNumPrimPrimContacts = muNumPrimAggContacts = 0;
//         ; six stwx at this+0x6C278/27C/280/284/290/294 -- note the two GAPS:
//         ; muNumPPQs (+0x6C288) and muNumIntersections (+0x6C28C) are NOT reset here.
//   queue = lpInputBuffer->GetOverlappingPairQueue();   ; bl sub_828B0698
//   for ( i = 0 ; i < queue->GetLength() ; ++i )        ; lwz r11,8(r29) each iteration
//   {
//       lPair = queue->GetEvent(i);                     ; bl 0x828AE0D0, then four
//                                                       ; lwz/stw of the 16-byte element
//                                                       ; into a stack local
//       if ( !lPair.mbCull )  ProcessOverlap( lPair, lpOutputBuffer );  ; lbz local+0xC
//   }
//
// THE ELEMENT IS COPIED BY VALUE, deliberately: the console stages the four words in a
// stack local and hands ProcessOverlap the address of THAT (`addi r4,r1,var_40`), never
// the queue slot. ProcessOverlap's DWARF signature takes a NON-const OverlappingPair&,
// so the copy is the difference between a callee that could scribble on the queue and
// one that cannot. Reproduced.
//
// LANDED 2026-08-19 -- the loop used to be absent entirely (only the six counter resets
// were reconstructed), which is why the culler has been silently producing nothing.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessOverlapsQueue(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                                                const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");    // :239
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");  // :240

    // Reset the per-frame pair/contact statistics (the six stwx, in the asm's order).
    muNumPrimPrimPairs    = 0;  // this+0x6C278
    muNumPrimAggPairs     = 0;  // this+0x6C27C
    muNumOtherPairs       = 0;  // this+0x6C280
    muNumInstanceQueries  = 0;  // this+0x6C284
    muNumPrimPrimContacts = 0;  // this+0x6C290
    muNumPrimAggContacts  = 0;  // this+0x6C294

    const OverlapCullingIO::InputBuffer::InOverlappingPairQueue* lpQueue =
        lpInputBuffer->GetOverlappingPairQueue();

    for (s32 liPair = 0; liPair < lpQueue->GetLength(); ++liPair)
    {
        OverlappingPair lOverlappingPair = lpQueue->GetEvent(liPair);   // by value: see above

        // mbCull is the broad phase's culling-table verdict. TRUE == do not resolve.
        if (!lOverlappingPair.mbCull)
        {
            ProcessOverlap(lOverlappingPair, lpOutputBuffer);
        }
    }
}

// ---------------------------------------------------------------------------
// ProcessRemoveInternalVolumeQueue -- DWARF CgsOverlapCullingModule.cpp:346.
//
// DECLARED, NOT LANDED, AND NOT CALLED. The DWARF attests the declaration, but this
// build's Update @0x828D1D50 does not call it (measured: the body has `bl`s to
// ProcessAddInternalVolumeQueue, ProcessInternalCollisions and ProcessOverlapsQueue and
// to nothing else), and no X360 address is attributed to it in progress/identity.json
// or in the wave-Q5 scene-collision scout. Writing a body from the shape of its sibling
// would be fabrication (AGENTS gotcha 8), so it keeps the assert the caller-less
// declaration justifies and nothing more. Whoever finds its X360 address owns it.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessRemoveInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");
}

// ---------------------------------------------------------------------------
// ProcessOverlap @ 0x828CAF38  (DWARF CgsOverlapCullingModule.cpp:380)
//
//   assert( lpOutputBuffer != NULL )                            ; li r5,0x17E -> :382
//   A = lrOverlappingPair.muVolumeInstanceA;  B = ...B;          ; lwz 0(r23) / lwz 4(r23)
//   if ( A == -1 || B == -1 ) return;                            ; cmpwi -1 x2
//   lpVolInstA = mpEntityManager->GetVolumeInstance(A);          ; lwz r3,0x234 ; sub_828B9FD8
//   assert( lpVolInstA )                                         ; li r5,0x190 -> :400
//   lpVolInstB = mpEntityManager->GetVolumeInstance(B);
//   assert( lpVolInstB )                                         ; li r5,0x193 -> :403
//   lpVolA = mpVolumeManager->GetRwVolume(lpVolInstA->miVolumeIndex);  ; lwz r4,0x5C
//   lpVolB = mpVolumeManager->GetRwVolume(lpVolInstB->miVolumeIndex);
//   assert( lpVolA )                                             ; li r5,0x198 -> :408
//   assert( lpVolB )                                             ; li r5,0x199 -> :409
//   if ( lpVolInstA->mu16EntityIndex == lpVolInstB->mu16EntityIndex ) return;  ; lhz 0x64 x2
//   DoPairQuery( lpOutputBuffer, lpVolInstA, A, lpVolA,
//                lpVolInstB, B, lpVolB, lrOverlappingPair.mfPadding );  ; lfs f1,8(r23)
//
// ⚠️ THREE INDEX SPACES MEET HERE and mixing them is the trap this function is built
// around:
//   * A / B                      -- EntityManager VOLUME-INSTANCE pool indices, what the
//                                   broad phase queued and what the emitted Contact
//                                   carries;
//   * lpVolInst->miVolumeIndex   -- a VolumeManager VOLUME pool index (a different pool,
//                                   a different bound), the only thing GetRwVolume takes;
//   * lpVolInst->mu16EntityIndex -- an EntityManager ENTITY pool index, used only for the
//                                   self-pair reject below.
//
// ⚠️ THE SELF-PAIR REJECT IS A SECOND ONE. SceneManagerModule::BridgeOverlapGeneration-
// ToOverlapCulling @0x828BA538 already drops pairs whose two instances share an
// EntityId (`ld 0x50 ; srdi 32 ; cmplw ; beq skip`). This one compares
// mu16EntityIndex (+0x64) instead -- the entity POOL SLOT rather than the packed id --
// and it is the one that stops a car's own body/wheel volumes narrow-phasing against
// each other. Both exist in the console; neither replaces the other.
//
// FAITHFUL: all four asserts are NON-GATING tripwires. The console fires them and then
// dereferences/uses the value four instructions later with no branch around it (e.g.
// 0x828CAFFC `lwz r4,0x5C(r27)` immediately after the lpVolInstA assert block). No
// early-out is invented.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessOverlap(OverlappingPair& lrOverlappingPair,
                                          OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");   // :382

    const s32 liVolumeInstanceIndexA = static_cast<s32>(lrOverlappingPair.muVolumeInstanceA);
    const s32 liVolumeInstanceIndexB = static_cast<s32>(lrOverlappingPair.muVolumeInstanceB);

    // -1 is the "no volume instance" sentinel (KI_INVALID_VOLUME_INDEX's bit pattern).
    if (liVolumeInstanceIndexA == -1 || liVolumeInstanceIndexB == -1)
    {
        return;
    }

    VolumeInstance* lpVolInstA = mpEntityManager->GetVolumeInstance(liVolumeInstanceIndexA);
    CGS_ASSERT(lpVolInstA != nullptr, "lpVolInstA");   // :400

    VolumeInstance* lpVolInstB = mpEntityManager->GetVolumeInstance(liVolumeInstanceIndexB);
    CGS_ASSERT(lpVolInstB != nullptr, "lpVolInstB");   // :403

    const VolRef::Volume* lpVolA = mpVolumeManager->GetRwVolume(lpVolInstA->miVolumeIndex);
    const VolRef::Volume* lpVolB = mpVolumeManager->GetRwVolume(lpVolInstB->miVolumeIndex);

    CGS_ASSERT(lpVolA != nullptr, "lpVolA");   // :408
    CGS_ASSERT(lpVolB != nullptr, "lpVolB");   // :409

    // Two volume instances of the SAME entity never reach the narrow phase.
    if (lpVolInstA->mu16EntityIndex == lpVolInstB->mu16EntityIndex)
    {
        return;
    }

    DoPairQuery(lpOutputBuffer,
                lpVolInstA, lrOverlappingPair.muVolumeInstanceA, lpVolA,
                lpVolInstB, lrOverlappingPair.muVolumeInstanceB, lpVolB,
                lrOverlappingPair.mfPadding);
}

// ---------------------------------------------------------------------------
// DoPairQuery @ 0x828C1A18  (DWARF CgsOverlapCullingModule.cpp:469)
//
// THE CULLER'S CONTACT PRODUCER. Two arms, chosen by whether either volume is an
// AGGREGATE (a clustered mesh / compound); everything else is a primitive.
//
// THE TYPE TEST, twice over: `lwz r11,0x40(vol) ; lwz r11,0(r11) ; cmpwi r11,6`.
// On the console +0x40 holds the per-type DESCRIPTOR POINTER (gVolumeVTable[type],
// installed by BrnPhysics::Props::FixableVolume::FixUp) and the descriptor's leading
// word is its own type id -- so the two loads are "read this volume's type". On THIS
// host the +0x40 slot is modelled as the 4-byte type enum itself (see the width note in
// vendor/renderware/collision/CollisionVolume.hpp), so the same read is one member
// access: `lpVolume->muVTableSlot`. 6 == E_VOLUMETYPE_AGGREGATE, and the tree's enum is
// grounded on the shipped descriptors' own leading words, not guessed.
//
// STATISTICS (0x828C1A3C..0x828C1A9C), reproduced with its retail-binary quirk intact:
//   if      ( !aggA && !aggB )  ++muNumPrimPrimPairs;    ; this+0x6C278
//   else if (  aggA && !aggB )  ++muNumPrimAggPairs;     ; this+0x6C27C
//   else                        ++muNumOtherPairs;       ; this+0x6C280
// ⚠️ NOTE THE ASYMMETRY -- a PRIMITIVE-A x AGGREGATE-B pair counts as "other", not as
// "primAgg". That is not a transcription slip: at loc_828C1A68 the compiler re-tests
// `typeA == 6` on a path it has already proved `typeA != 6`, so the `bne` there is
// unconditionally taken. Writing the second condition as the source did
// (`aggA && !aggB`) reproduces it exactly; "de-optimising" it into a symmetric XOR
// would change the shipped counts. The DISPATCH below is symmetric even though the
// counter is not, so no contact is lost either way.
//
// ARM SELECT (0x828C1AA0..0x828C1ABC): `if (aggA || aggB)` -> aggregate arm.
//
// PRIM x PRIM (0x828C1AC0..0x828C1B84):
//   RwBool ok = PrimitivePairIntersect( lResult, volA, &instA->mWorldSpaceTransform,
//                                       volB, &instB->mWorldSpaceTransform,
//                                       lfPadding, NULL );
//   if ( !ok ) return;  if ( lResult.numPoints == 0 ) return;
//   <emit every point, counter = muNumPrimPrimContacts>
//   ⚠️ THE PPC FLOAT-ARG ABI IS VISIBLE HERE (AGENTS gotcha 3): the call sets
//   r3..r7 and r9 but NEVER r8 -- afPadding is the 6th argument, rides in f1, and
//   SKIPS its GPR slot; r9 = 0 is the 7th argument (apSepDir = NULL). Reading r8 as an
//   argument would have mis-numbered everything after it.
//   ⚠️ The two "transform" arguments are the VolumeInstance pointers in the asm,
//   because VolumeInstance::mWorldSpaceTransform sits at offset 0. Spelled by name.
//
// PRIM x AGGREGATE (0x828C1B8C..0x828C1D0C): fill the module's own persistent
// VolumeVolumeQuery in place as a 1xN query -- side A is the single "input" volume,
// side B is the query volume -- then run it:
//   q = mpVolVolQuery;                                   ; lwz r11,0x23C(r27)
//   q->m_padding          = lfPadding;                   ; stfs f1,0x14
//   q->m_currInput        = 0;                           ; stw 0,0x0C
//   q->m_inputVols        = &lapVolumesA[0];             ; stw r9,0x00   (&arg_34)
//   q->m_inputMats        = &lapMatricesA[0];            ; stw r7,0x04   (&var_830)
//   q->m_numInputs        = 1;                           ; stw r6,0x08
//   q->m_volRefPairCount  = 0;                           ; stw 0,0x1C
//   q->m_queryVol         = lpVolumeB;                   ; stw r10,0x38
//   q->m_queryMtx         = &instB->mWorldSpaceTransform;; stw r8,0x3C
//   q->m_cullTable        = NULL;                        ; stw 0,0x10
//   n = q->GetPrimitiveIntersections();
//   assert( n <= 100 );                                  ; li r5,0x223 -> :547
//   if ( n == 0 ) return;
//   ++muNumInstanceQueries;                              ; this+0x6C284
//   for ( each of the n results, stride 0x750 )
//       <emit every point, counter = muNumPrimAggContacts>
//   ⚠️ the intersection buffer base is re-read from the query INSIDE the outer loop
//   (0x828C1C48 `lwz r11,0x23C(r27)` then `lwz r11,0x30(r11)`), not cached -- kept.
//   ⚠️ the `n == 0` test is emitted TWICE (0x828C1C00 and 0x828C1C1C) with the
//   muNumInstanceQueries increment between them; the second is unreachable, so it is
//   folded here and the increment sits after the single early-out. Same behaviour.
//   ⚠️ 100 is the same immediate Construct passes as the query's result count.
//
// RETURN: void. The asm sets no return value on any path (it falls into
// __restgprlr_16 from three places); Hex-Rays' `int` is the leftover r3 of whatever it
// called last. DWARF agrees.
// ---------------------------------------------------------------------------
void OverlapCullingModule::DoPairQuery(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                                       const VolumeInstance* lpVolumeInstanceA, u32 luVolumeInstanceIndexA,
                                       const VolRef::Volume* lpVolumeA,
                                       const VolumeInstance* lpVolumeInstanceB, u32 luVolumeInstanceIndexB,
                                       const VolRef::Volume* lpVolumeB, f32 lfPadding)
{
    // The scene manager's opaque VolRef::Volume IS rw::collision::Volume (DecFIGS
    // volume.h:39 typedefs exactly that); the cast is between two spellings of one
    // type, not a layout reinterpretation. Same move, same reason, as
    // CgsVolumeManager.cpp:212.
    const rw::collision::Volume* lpRwVolumeA =
        reinterpret_cast<const rw::collision::Volume*>(lpVolumeA);
    const rw::collision::Volume* lpRwVolumeB =
        reinterpret_cast<const rw::collision::Volume*>(lpVolumeB);

    const bool lbIsAggregateA =
        (lpRwVolumeA->muVTableSlot == static_cast<u32>(rw::collision::E_VOLUMETYPE_AGGREGATE));
    const bool lbIsAggregateB =
        (lpRwVolumeB->muVTableSlot == static_cast<u32>(rw::collision::E_VOLUMETYPE_AGGREGATE));

    if (!lbIsAggregateA && !lbIsAggregateB)
    {
        ++muNumPrimPrimPairs;
    }
    else if (lbIsAggregateA && !lbIsAggregateB)
    {
        ++muNumPrimAggPairs;
    }
    else
    {
        ++muNumOtherPairs;
    }

    // [DIAG] NOT IN THE X360 BINARY. One-shot pair-query probe (BRN_PROP_DIAG): the
    // first time the narrow phase is asked to resolve anything, report which two volume
    // types it got and how many contacts came out. luDiagContacts is the only cost when
    // the probe is off -- one register increment per emitted contact.
    u32 luDiagContacts = 0;

    if (!lbIsAggregateA && !lbIsAggregateB)
    {
        // ---- primitive x primitive -------------------------------------------------
        RwPairResult lResult;

        const rw::collision::RwBool lbIntersects = rw::collision::PrimitivePairIntersect(
            lResult,
            lpRwVolumeA, &lpVolumeInstanceA->mWorldSpaceTransform,
            lpRwVolumeB, &lpVolumeInstanceB->mWorldSpaceTransform,
            lfPadding, nullptr);

        if (lbIntersects != 0 && lResult.numPoints != 0)
        {
            EmitContactPoints(lpOutputBuffer, lResult,
                              luVolumeInstanceIndexA, luVolumeInstanceIndexB,
                              muNumPrimPrimContacts, luDiagContacts);
        }
    }
    else
    {
        // ---- primitive x aggregate (the 1xN VolumeVolumeQuery) ---------------------
        const rw::collision::Volume*  lapInputVolumes[1]  = { lpRwVolumeA };
        const Matrix44Affine*         lapInputMatrices[1] =
            { &lpVolumeInstanceA->mWorldSpaceTransform };

        rw::collision::VolumeVolumeQuery* lpQuery = mpVolVolQuery;

        lpQuery->m_padding         = lfPadding;                                  // +0x14
        lpQuery->m_currInput       = 0;                                          // +0x0C
        lpQuery->m_inputVols       = lapInputVolumes;                            // +0x00
        lpQuery->m_inputMats       = lapInputMatrices;                           // +0x04
        lpQuery->m_numInputs       = 1;                                          // +0x08
        lpQuery->m_volRefPairCount = 0;                                          // +0x1C
        lpQuery->m_queryVol        = lpRwVolumeB;                                // +0x38
        lpQuery->m_queryMtx        = &lpVolumeInstanceB->mWorldSpaceTransform;   // +0x3C
        lpQuery->m_cullTable       = nullptr;                                    // +0x10

        const u32 luNumIntersections =
            static_cast<u32>(mpVolVolQuery->GetPrimitiveIntersections());

        CGS_ASSERT(luNumIntersections <= 100, "luNumIntersections <= 100");   // :547

        if (luNumIntersections == 0)
        {
            return;
        }

        ++muNumInstanceQueries;

        for (u32 luResult = 0; luResult < luNumIntersections; ++luResult)
        {
            // Re-read through the query handle every iteration, as the console does.
            const RwPairResult& lrResult = mpVolVolQuery->m_intersectionBuffer[luResult];

            if (lrResult.numPoints != 0)
            {
                EmitContactPoints(lpOutputBuffer, lrResult,
                                  luVolumeInstanceIndexA, luVolumeInstanceIndexB,
                                  muNumPrimAggContacts, luDiagContacts);
            }
        }
    }

    // [DIAG] see above -- one shot, after both arms.
    {
        static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
        static bool       sbLoggedFirstPairQuery = false;
        if (sbPropDiag && !sbLoggedFirstPairQuery && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedFirstPairQuery = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q5-cull] first pair query: types "
                << lpRwVolumeA->muVTableSlot << "/" << lpRwVolumeB->muVTableSlot
                << " -> contacts " << luDiagContacts << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// ProcessAccumulatedQueries -- DWARF CgsOverlapCullingModule.cpp:428.
//
// DECLARED, NOT LANDED, AND NOT CALLED. Same state as
// ProcessRemoveInternalVolumeQueue above: attested by the DWARF, absent from
// Update @0x828D1D50's call list, and with no X360 address attributed anywhere. It is
// the counterpart of the ContactGenerator::QueryAccumulator IO buffer
// SceneManagerModule::UpdateContactGeneration allocates and never fills. Left empty
// rather than invented.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessAccumulatedQueries(OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/)
{
}

// ---------------------------------------------------------------------------
// IsInsideEscapeVolume @0x828CB0A8 / DoInternalCollision @0x828CB1D8 /
// ProcessInternalCollisions @0x828CB308 -- landed in the sibling partfile
// ContactGen/CgsOverlapCullingModule_wQ5_01.cpp (wave Q5, cluster E3b). Their shells
// were deleted from this file in the same step; re-adding a body here is LNK2005.
// ---------------------------------------------------------------------------

// Never called at runtime; pins the asm-attested member ORDER (absolute X360 byte
// offsets diverge on the x64 PC build because of the 8-byte pointer width and the
// ModuleSingleBuffered base, so only the relative order is asserted -- exactly as the
// FineIntersectionTestModule sibling).
void OverlapCullingModule::_AssertLayout()
{
    static_assert(sizeof(u8) == 1, "u8 must be one byte");
    static_assert(OverlapCullingModule::KU_VOL_QUERY_MEM_SIZE == 401408,
                  "VolumeVolumeQueryMem budget is 0x62000");
    // The host VolumeVolumeQuery(100, 100) -- staging and instancing at the host widths
    // (VolumeQueryHostLayout.hpp) -- still fits the console budget (0x495D8 <= 0x62000).
    static_assert(rw::collision::KU_VOLUME_VOLUME_QUERY_HOST_SIZE_R100 <= OverlapCullingModule::KU_VOL_QUERY_MEM_SIZE,
                  "the host VolumeVolumeQuery for 100/100 fits the 0x62000 VolumeVolumeQueryMem budget");
    static_assert(OverlapCullingModule::KU_MAX_NUM_VOLUME_INSTANCES == 5048,
                  "KI_MAX_NUM_VOLUME_INSTANCES == 0x13B8");
    static_assert(offsetof(OverlapCullingModule, mePrepareStage) <
                      offsetof(OverlapCullingModule, meReleaseStage),
                  "mePrepareStage precedes meReleaseStage");
    static_assert(offsetof(OverlapCullingModule, mpVolumeManager) <
                      offsetof(OverlapCullingModule, mpEntityManager),
                  "mpVolumeManager precedes mpEntityManager (X360 word 140 < 141)");
    static_assert(offsetof(OverlapCullingModule, mpContactGenerator) <
                      offsetof(OverlapCullingModule, mpVolVolQuery),
                  "mpContactGenerator precedes mpVolVolQuery (X360 word 142 < 143)");
    static_assert(offsetof(OverlapCullingModule, maVolumeVolumeQueryMem) <
                      offsetof(OverlapCullingModule, mabIsUsingInternalCollision),
                  "maVolumeVolumeQueryMem precedes the internal-collision tables");
    static_assert(offsetof(OverlapCullingModule, mabIsUsingInternalCollision) <
                      offsetof(OverlapCullingModule, mauInternalVolumeInstanceIndex),
                  "the registration BitArray precedes the internal-volume table");
    static_assert(offsetof(OverlapCullingModule, mauInternalVolumeInstanceIndex) <
                      offsetof(OverlapCullingModule, mauEscapeVolumeInstanceIndex),
                  "the internal-volume table precedes the escape-volume table");
    static_assert(offsetof(OverlapCullingModule, muNumPrimPrimPairs) <
                      offsetof(OverlapCullingModule, muNumPrimAggContacts),
                  "the per-frame counter block is contiguous and ordered");
}

}  // namespace CgsSceneManager

// ============================================================================
// FOLDED FROM CgsOverlapCullingModule_wQ5_01.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// CgsSceneManager::OverlapCullingModule -- the culler's INTERNAL-COLLISION half.
//   Wave Q5, cluster E3b (2026-08-19). Companion partfile to
//   ContactGen/CgsOverlapCullingModule.cpp (cluster E3a owns that file and the
//   shared header GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h).
//
// Three X360 ARTIST bodies, landed store-for-store from the per-address IDA
// export (.ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json, `assembly` array --
// the pseudocode is a hint and is WRONG about IsInsideEscapeVolume's arity, see
// below):
//
//   OverlapCullingModule::ProcessInternalCollisions @ 0x828CB308  (206 insns)
//   OverlapCullingModule::IsInsideEscapeVolume      @ 0x828CB0A8  ( 75 insns)
//   OverlapCullingModule::DoInternalCollision       @ 0x828CB1D8  ( 76 insns)
//
// WHAT THIS HALF DOES. Separate from the broad-phase overlap pairs the sweeper
// produces, the culler keeps a per-volume-instance "internal collision"
// registration: a volume instance may name a second INTERNAL volume instance it
// must additionally be tested against every frame, plus a third ESCAPE volume
// instance that bounds how long the registration lives. Every frame
// ProcessInternalCollisions walks the registered set (a BitArray<5048> keyed by
// volume-instance index), and per registered instance either
//   * runs the internal pair query (DoInternalCollision -> DoPairQuery, which is
//     the SAME contact producer the overlap path uses), while the instance is
//     still inside its escape volume; or
//   * de-registers it (clears the bit) the first frame it is NOT.
// The queue that populates the three tables is
// OverlapCullingModule::ProcessAddInternalVolumeQueue (E3a's).
//
// ---------------------------------------------------------------------------
// MEMBER LAYOUT -- RE-DERIVED FROM THIS CLUSTER'S OWN ASM, NOT ASSUMED.
// Every table this file touches is reached BY NAME; the console byte offsets
// below are documentation only (gotcha 1 -- an X360 immediate is never a host
// value). They are quoted because they are what proves the header's member set
// is the right one, and all three bodies agree:
//
//   this + 0x230 (560)  mpVolumeManager        lwz r3,0x230(r31)  x2 per body
//   this + 0x234 (564)  mpEntityManager        lwz r3,0x234(r31)  x2 per body
//   this + 401984       mabIsUsingInternalCollision
//                         ProcessInternalCollisions: `addis r22,r14,6 ;
//                         addi r22,r22,0x2240` == 6*65536 + 0x2240 == 401984,
//                         and Prepare's own `memset(this+401984, 0, 632)`
//                         (632 == 79 u64 fields == BitArray<5048>).
//   this + 402616       mauInternalVolumeInstanceIndex[5048]
//                         DoInternalCollision: `addis r11,r29,2 ;
//                         addi r11,r11,-0x76D2 ; slwi r11,r11,2` ==
//                         (i + 131072 - 30418)*4 == i*4 + 402616.
//   this + 422808       mauEscapeVolumeInstanceIndex[5048]
//                         IsInsideEscapeVolume: `addis r11,r4,2 ;
//                         addi r11,r11,-0x631A ; slwi r11,r11,2` ==
//                         (i + 131072 - 25370)*4 == i*4 + 422808
//                         (== 402616 + 5048*4, i.e. the table straight after).
//   this + 443000       muNumPrimPrimPairs ... (the counter block; cross-checked
//                         from DoPairQuery @0x828C1A5C `addis r11,r27,7 ;
//                         addi r11,r11,-0x3D88` == 443000).
// So the header's order  mabIsUsingInternalCollision, mauInternalVolumeInstance-
// Index, mauEscapeVolumeInstanceIndex, <counters>  is attested end to end.
//
// ---------------------------------------------------------------------------
// THE BIT ARRAY IS DE-INLINED, DELIBERATELY. The X360 folds
// BitArray<5048>::GetFirstNonZeroBit / GetNextNonZeroBit / UnSetBit into
// ProcessInternalCollisions (that is most of its 206 instructions: the u64
// word scan, the `x & -x` lowest-set-bit isolate expressed as
// `cntlzd(w - ((w-1)&w))`, the in-word linear probe, and the `andc` clear).
// AGENTS "UNDO COMPILER OPTIMIZATIONS / inlining reversal" says to restore the
// calls, and CgsBitArray.h already carries all three methods with exactly these
// semantics. The two bounds ASSERTS the console emits inside those inlines stay
// at the call site, which is the convention CgsBitArray.h's own banner states
// ("emitted at the call sites by callers that own the CgsDev::Assert API"):
//   * CgsBitArray.h:241 ("luIndex < NUMBITS") -- reproduced below, it guards the
//     UnSetBit arm (X360 0x828CB42C `cmplwi r31,0x13B8 ; blt`).
//   * CgsBitArray.h:203 (the StrStream'd "invalid index : <i> < <N>") -- guards
//     the IsBitSet probe INSIDE the next-set-bit search (X360 0x828CB484..
//     0x828CB584). It has no expressible call site once the search is a single
//     GetNextNonZeroBit() call, and it is unreachable anyway (the search is
//     bounded by tuNumBits). NOT reproduced; recorded here instead of invented
//     somewhere it does not belong.
// PERFORMANCE DELTA, stated because it is real and not a defect: the committed
// GetNextNonZeroBit is a bit-at-a-time probe where the console skips whole
// all-zero u64 words. Same result, more loop trips when the registered set is
// sparse; fixing that belongs in CgsBitArray.h, not here.
//
// ---------------------------------------------------------------------------
// HEX-RAYS IS WRONG ABOUT IsInsideEscapeVolume'S SIGNATURE. The pseudocode
// renders it `IsInsideEscapeVolume(int a1)` -- one argument -- and then indexes
// `sub_828B9FD8(*(a1 + 564))` with no index at all. The asm has `addis r11,r4,2`
// in the third instruction: r4 is a live SECOND parameter (the volume-instance
// index), exactly as the DecFIGS DWARF declares it
// (CgsOverlapCullingModule.cpp:600 -> `bool IsInsideEscapeVolume(int32_t)`).
// The header's declaration is already right; the pseudocode is not.
// ===========================================================================


// NOT included, on purpose:
//   * ContactGen/CgsContactGenerationIO.h and ContactGen/CgsOverlapCullingModuleIO.h --
//     the OverlapCullingIO::OutputBuffer is only ever passed THROUGH this file
//     (to DoPairQuery), never dereferenced, so the forward declaration the
//     shared header already carries is enough. Including either would also make
//     this TU a second front in the still-open blob-vs-real OutputBuffer swap
//     that cluster D2 reported.
//   * vendor/renderware/collision/CollisionVolume.hpp -- only `const Volume*` is
//     needed and GPInstance.hpp forward-declares it. Pulling the full record in
//     would collide with the SECOND rw::collision::Volume that
//     SDKs/EATech/rwcollision/volume_debug_access.h defines (the fork
//     CollisionVolume.hpp's own banner reports).

namespace CgsSceneManager
{
namespace
{
    // ---------------------------------------------------------------------
    // The one type seam in this file, isolated so it is a single documented
    // place rather than four casts scattered through two bodies.
    //
    // VolumeManager::GetRwVolume returns `const CgsSceneManager::VolRef::Volume*`
    // (CgsVolumeStore.h:82 -- an opaque forward declaration) while the narrow
    // phase takes `const rw::collision::Volume*`. DecFIGS volume.h:39 typedefs
    // `VolRef::Volume` TO `rw::collision::Volume`, so these are one console
    // type described by two forward declarations in this tree; the conversion
    // reinterprets nothing.
    //
    // ⚠️ REPORTED, NOT UNILATERALLY FIXED (gotcha 7): unifying the two spellings
    // means editing CgsVolumeStore.h / CgsVolumeManager.h / CgsCollisionMeshData.h
    // / CgsOverlapCullingModule.h, none of which this cluster owns. The
    // VolumeManager owner already filed the same fork from the other side
    // (CgsVolumeManager.cpp's GetRwVolume banner) and casts identically there.
    // When the fork is collapsed, delete this helper and the calls become direct.
    inline const rw::collision::Volume* AsRwVolume(const VolRef::Volume* lpVolume)
    {
        return reinterpret_cast<const rw::collision::Volume*>(lpVolume);
    }
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::IsInsideEscapeVolume @ 0x828CB0A8  (75 insns)
// DWARF home: CgsOverlapCullingModule.cpp:600
//
//   lpVolInst       = mpEntityManager->GetVolumeInstance( liVolumeInstanceIndex );
//   lpEscapeVolInst = mpEntityManager->GetVolumeInstance(
//                         mauEscapeVolumeInstanceIndex[ liVolumeInstanceIndex ] );
//   assert( lpVolInst,       "lpVolInst"       )   // .cpp:614   (li r5,0x266)
//   assert( lpEscapeVolInst, "lpEscapeVolInst" )   // .cpp:615   (li r5,0x267)
//   lpVol       = mpVolumeManager->GetRwVolume( lpVolInst->miVolumeIndex );
//   lpEscapeVol = mpVolumeManager->GetRwVolume( lpEscapeVolInst->miVolumeIndex );
//   assert( lpVol,       "lpVol"       )           // .cpp:619   (li r5,0x26B)
//   assert( lpEscapeVol, "lpEscapeVol" )           // .cpp:620   (li r5,0x26C)
//   return PrimitivePairIntersect( <stack result>, lpVol, lpVolInst->transform,
//                                  lpEscapeVol, lpEscapeVolInst->transform,
//                                  0.0f, NULL ) != 0;
//
// ORDER IS LOAD-BEARING AND PRESERVED: both GetVolumeInstance calls happen
// BEFORE either assert (r29/r27 are filled at 0x828CB0CC/0x828CB0DC, the asserts
// start at 0x828CB0F0), and `lwz r4,0x5C(r29)` -- the miVolumeIndex read -- is
// only reached AFTER them at 0x828CB134. CGS_ASSERT does not abort, so a NULL
// instance faults on the member read here exactly as it does on the console; no
// guard is added (adding one would be behaviour the binary does not have).
//
// THE `rw::collision::PrimitivePairIntersect` CALL, register by register
// (0x828CB19C-0x828CB1BC), against the committed declaration in
// vendor/renderware/collision/GPInstance.hpp:637:
//   r3  = sp + 0x50, an UNINITIALISED 1872-byte stack block -- exactly
//         sizeof(PrimitivePairIntersectResult) (0x750); the 0x7D0 frame is
//         0x50 linkage + this block + the __savegprlr_27 spill. No memset:
//         the callee writes every field it publishes.
//   r4  = lpVol                    -> apVolume1
//   r5  = lpVolInst                -> apMtx1     (see the transform note below)
//   r6  = lpEscapeVol              -> apVolume2
//   r7  = lpEscapeVolInst          -> apMtx2
//   f1  = flt_82001CC0             -> afPadding  (== 0.0f: an internal-collision
//         query runs with NO extra padding. Not guessed -- the value is pinned
//         three independent ways: the decrypted XEX image bytes are 0x00000000
//         (recorded at CgsModel.cpp:123), PrimitiveIntersect.cpp's own rodata
//         note for this exact symbol says 0.0f, and Hex-Rays renders the
//         argument `0.0` in both this body and DoInternalCollision.)
//   r9  = 0                        -> apSepDir   (NULL: no caller-supplied
//         separating direction, so the callee runs its own 6x6 SAT dispatch)
//   NOTE (gotcha 3): f1 consumes the r8 GPR slot, which is why apSepDir lands in
//   r9 and not r8. Hex-Rays drops the r9 argument entirely.
//   Return: `cntlzw ; extrwi 1,26 ; xori 1` == (result != 0) as a bool -- and the
//   caller reads it back with `clrlwi r11,r3,24`, i.e. one byte. bool is right.
//
// TRANSFORM ARGUMENT: the console passes the VolumeInstance pointer itself where
// the canonical prototype (rwccore.h:3001) takes `const Matrix44Affine*`. That is
// not a pun -- VolumeInstance::mWorldSpaceTransform is the record's FIRST member
// (CgsVolumeInstance.h, X360 +0x00), so the two addresses are identical. Written
// here as `&lpVolInst->mWorldSpaceTransform`: same address, and it says what the
// callee actually consumes instead of leaning on the offset being zero.
// ---------------------------------------------------------------------------
bool OverlapCullingModule::IsInsideEscapeVolume(s32 liVolumeInstanceIndex)
{
    // `lwzx r30, r11, r31` @0x828CB0C8 -- the escape registration is read one
    // instruction before the first GetVolumeInstance call, i.e. from the table
    // as it stands at entry, not after either lookup.
    const u32 luEscapeVolumeInstanceIndex =
        mauEscapeVolumeInstanceIndex[liVolumeInstanceIndex];

    VolumeInstance* lpVolInst =
        mpEntityManager->GetVolumeInstance(liVolumeInstanceIndex);
    VolumeInstance* lpEscapeVolInst =
        mpEntityManager->GetVolumeInstance(static_cast<s32>(luEscapeVolumeInstanceIndex));

    CGS_ASSERT(lpVolInst != NULL, "lpVolInst");              // .cpp:614
    CGS_ASSERT(lpEscapeVolInst != NULL, "lpEscapeVolInst");  // .cpp:615

    const VolRef::Volume* lpVol =
        mpVolumeManager->GetRwVolume(lpVolInst->miVolumeIndex);
    const VolRef::Volume* lpEscapeVol =
        mpVolumeManager->GetRwVolume(lpEscapeVolInst->miVolumeIndex);

    CGS_ASSERT(lpVol != NULL, "lpVol");              // .cpp:619
    CGS_ASSERT(lpEscapeVol != NULL, "lpEscapeVol");  // .cpp:620

    rw::collision::PrimitivePairIntersectResult lIntersectResult;

    return rw::collision::PrimitivePairIntersect(lIntersectResult,
                                                 AsRwVolume(lpVol),
                                                 &lpVolInst->mWorldSpaceTransform,
                                                 AsRwVolume(lpEscapeVol),
                                                 &lpEscapeVolInst->mWorldSpaceTransform,
                                                 0.0f,
                                                 NULL) != 0;
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::DoInternalCollision @ 0x828CB1D8  (76 insns)
// DWARF home: CgsOverlapCullingModule.cpp:644
//
// The exact same four-step resolve as IsInsideEscapeVolume, against the INTERNAL
// table instead of the ESCAPE table, and ending in the shared contact producer
// DoPairQuery instead of the bare intersection test:
//
//   assert( lpVolInst,    "lpVolInst"    )   // .cpp:657  (li r5,0x291)
//   assert( lpIntVolInst, "lpIntVolInst" )   // .cpp:658  (li r5,0x292)
//   assert( lpVol,        "lpVol"        )   // .cpp:662  (li r5,0x296)
//   assert( lpIntVol,     "lpIntVol"     )   // .cpp:663  (li r5,0x297)
//
// DoPairQuery ARGUMENT ORDER, register by register (0x828CB2D4-0x828CB2FC):
//   r3  = this
//   r4  = r23 = the OutputBuffer this function was handed
//   r5  = r30 = lpVolInst
//   r6  = r29 = liVolumeInstanceIndex          <-- the VOLUME-INSTANCE index
//   r7  = r27 = lpVol
//   r8  = r26 = lpIntVolInst
//   r9  = r24 = mauInternalVolumeInstanceIndex[liVolumeInstanceIndex]
//                                              <-- also a VOLUME-INSTANCE index
//   r10 = r25 = lpIntVol
//   f1  = flt_82001CC0 == 0.0f (no padding, same constant as above)
//
// ⚠️ REPORTED TO THE HEADER OWNER (E3a): the declaration at
// CgsOverlapCullingModule.h:127-131 names r6/r9 `luVolumeIndexA`/`luVolumeIndexB`.
// They are NOT volume indices (those would be `lpVolInst->miVolumeIndex`, which
// is what GetRwVolume is fed two lines earlier and is a DIFFERENT index space) --
// they are volume-INSTANCE indices, and DoPairQuery's own body proves it: it
// stashes r6/r9 in r17/r16 and stores them straight into the
// CgsSceneManager::Contact it builds (0x828C1B20 `stw r17,var_7F0` /
// 0x828C1B28 `stw r16,var_7EC`), which is the pair of instance ids the world
// bridge later resolves. Suggested rename: luVolumeInstanceIndexA / ...B. A
// wrong name here is a live trap for whoever bodies DoPairQuery.
//
// RETURN TYPE: the asm tail-calls DoPairQuery and leaves its r3 in place, which
// is why Hex-Rays types this `int`. The DWARF declares
// `void DoInternalCollision(int32_t, OutputBuffer *)` (CgsOverlapCullingModule.h,
// .cpp:644) and the only caller (ProcessInternalCollisions @0x828CB424) discards
// r3 -- so void it is, and the result of DoPairQuery is deliberately not used.
// ---------------------------------------------------------------------------
void OverlapCullingModule::DoInternalCollision(s32 liVolumeInstanceIndex,
                                               OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    // lwzx r24, r11, r31 -- the internal registration, read before the calls.
    const u32 luInternalVolumeInstanceIndex =
        mauInternalVolumeInstanceIndex[liVolumeInstanceIndex];

    VolumeInstance* lpVolInst =
        mpEntityManager->GetVolumeInstance(liVolumeInstanceIndex);
    VolumeInstance* lpIntVolInst =
        mpEntityManager->GetVolumeInstance(static_cast<s32>(luInternalVolumeInstanceIndex));

    CGS_ASSERT(lpVolInst != NULL, "lpVolInst");        // .cpp:657
    CGS_ASSERT(lpIntVolInst != NULL, "lpIntVolInst");  // .cpp:658

    const VolRef::Volume* lpVol =
        mpVolumeManager->GetRwVolume(lpVolInst->miVolumeIndex);
    const VolRef::Volume* lpIntVol =
        mpVolumeManager->GetRwVolume(lpIntVolInst->miVolumeIndex);

    CGS_ASSERT(lpVol != NULL, "lpVol");        // .cpp:662
    CGS_ASSERT(lpIntVol != NULL, "lpIntVol");  // .cpp:663

    DoPairQuery(lpOutputBuffer,
                lpVolInst, static_cast<u32>(liVolumeInstanceIndex), lpVol,
                lpIntVolInst, luInternalVolumeInstanceIndex, lpIntVol,
                0.0f);
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::ProcessInternalCollisions @ 0x828CB308  (206 insns)
// DWARF home: CgsOverlapCullingModule.cpp:682
//
//   assert( lpOutputBuffer != NULL )   // .cpp:684  (li r5,0x2AC)
//   for ( i = mabIsUsingInternalCollision.GetFirstNonZeroBit();
//         i >= 0;
//         i = mabIsUsingInternalCollision.GetNextNonZeroBit( i ) )
//   {
//       if ( IsInsideEscapeVolume( i ) )  DoInternalCollision( i, lpOutputBuffer );
//       else                              mabIsUsingInternalCollision.UnSetBit( i );
//   }
//
// TERMINATION, as the asm actually writes it: the loop's back edge tests ONLY
// `i >= 0` (0x828CB630 `cmpwi cr6,r31,0 ; bge loc_828CB3FC`). The `>= 5048`
// bound is enforced inside the inlined searches, which `return` outright
// (0x828CB378, 0x828CB3A8, 0x828CB5C8, 0x828CB5D4, 0x828CB5FC). The committed
// GetFirstNonZeroBit / GetNextNonZeroBit hand back KI_INVALID_BITINDEX (-1) on
// every one of those paths, so `i >= 0` is the same condition -- and the
// entry `cmpwi r31,0x13B8 ; bge` / `cmpwi r31,0 ; blt` pair at 0x828CB3A4/A8/AC
// is the same -1-or-out-of-range early exit before the first iteration.
//
// THE ELSE ARM IS A DE-REGISTRATION, NOT AN ERROR PATH: the console clears the
// bit with `sld/andc/stdx` at 0x828CB44C-0x828CB460, i.e. once a volume instance
// has left its escape volume it stops being internally collided, permanently,
// until something re-posts it through the add-internal-volume queue. Clearing
// bit i does not disturb the search, which resumes at i+1.
//
// ⚠️ COUNTER NOTE for whoever wires the boot diagnostics (scene_collision_scout.md
// §6) -- DO NOT expect muNumPrimPrimPairs to show internal-collision work. The
// console's Update @0x828D1D50 runs, in this order:
//     LockForRead(in) ; LockForWrite(out)
//     ProcessAddInternalVolumeQueue(in)
//     ProcessInternalCollisions(out)          <-- this function
//     ProcessOverlapsQueue(out, in)
//     UnlockForRead(in) ; UnlockForWrite(out)
// and ProcessOverlapsQueue OPENS by zeroing six of the eight counters
// (0x828D0398-0x828D03E0 stores 0 to this+443000/443004/443008/443012/443024/
// 443028 == muNumPrimPrimPairs / muNumPrimAggPairs / muNumOtherPairs /
// muNumInstanceQueries / muNumPrimPrimContacts / muNumPrimAggContacts). So
// whatever DoPairQuery increments on this pass is WIPED before the frame ends.
// That is the console's own behaviour, preserved; the internal-collision path
// has no counter of its own and needs its own instrumentation if it needs any.
//
// ⚠️ AND THE MOUNTED Update() DOES NOT MATCH THAT SEQUENCE -- reported to the
// E3a owner rather than fixed here (CgsOverlapCullingModule.cpp is not this
// cluster's file). See scratchpad/waveQ5/e3b.owner.md, defects_found.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessInternalCollisions(OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != NULL, "lpOutputBuffer != NULL");  // .cpp:684

    for (s32 liVolumeInstanceIndex = mabIsUsingInternalCollision.GetFirstNonZeroBit();
         liVolumeInstanceIndex >= 0;
         liVolumeInstanceIndex = mabIsUsingInternalCollision.GetNextNonZeroBit(liVolumeInstanceIndex))
    {
        if (IsInsideEscapeVolume(liVolumeInstanceIndex))
        {
            DoInternalCollision(liVolumeInstanceIndex, lpOutputBuffer);
        }
        else
        {
            // The bounds check the console emits from inside the inlined
            // UnSetBit (0x828CB42C); CgsBitArray.h keeps its asserts at the
            // call site by design. Unreachable in practice -- the search is
            // bounded by tuNumBits -- and preserved because the binary has it.
            CGS_ASSERT(static_cast<u32>(liVolumeInstanceIndex) < KU_MAX_NUM_VOLUME_INSTANCES,
                       "luIndex < NUMBITS");
            mabIsUsingInternalCollision.UnSetBit(static_cast<u32>(liVolumeInstanceIndex));
        }
    }
}

}  // namespace CgsSceneManager
