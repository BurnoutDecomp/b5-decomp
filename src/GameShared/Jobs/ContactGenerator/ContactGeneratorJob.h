#pragma once

// =================================================================================================
// GameShared/Jobs/ContactGenerator/ContactGeneratorJob.h
//
// ⭐⭐⭐ THE DRAIN. ContactGeneratorJob is the job context that runs the collision workers off a
// job thread, and its LineWithTriangleListStream arm is the thing that finally READS the triangles
// the cache holds. Every wheel's ground contact comes out of it.
//
// The console's own file path is baked into every assert in the family, which is why this home
// sits under GameShared/Jobs/ContactGenerator/ and not with the collision generator:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGeneratorJob.cpp
//   d:\p4\b5_main\burnout\main\code\gameshared\jobs\contactgenerator\ContactGeneratorJob.h
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGenerator.cpp (entry)
//
// X360 homes reconstructed (traction-line wave 2026-08-11; sphere arms walls leg 2 2026-08-14):
//   ContactGeneratorJob::Execute                             @0x829267E0   (77)
//   ContactGeneratorJob::ExecuteLineWithTriangleListStream    @0x82921968  (589)
//   ContactGeneratorJob::ExecuteSphereListWithTriangleList    @0x829226A8  (967)  ⭐ contacts
//   ContactGeneratorJob::ExecuteSphereListWithTriangleListStream @0x829235C8 (100)
//   ContactGeneratorJob::LoadPrimitives                       @0x829210F0   (61)
//   ContactGeneratorJob::LoadResultList                       @0x829211E8   (46)
//   ContactGeneratorJob::AllocateMemory                       @0x829212A0   (54)
//   ContactGeneratorJob::RestoreMemory                        @0x82921050   (39)
//   ExecuteSweptSphereListWithTriangleList                    @0x829238E8  ⭐ swept leg
//   ExecuteSweptSphereListWithTriangleListStream               @0x82925238  (100)
//   ExecutePrimitiveListWithTriangleListStream                 @0x82926650  (100) ⭐ PROP leg
//   ExecutePrimitiveListWithTriangleList                       @0x82925908  (849) ⭐⭐ PROP
//                                                       NARROW PHASE (wave Q6, cluster pvt)
// Declared-only (the other four Execute arms; each a named boot gate in the .cpp -- see there
// for why the switch keeps all twelve cases instead of only the ones landed so far):
//   ExecuteSphereListWithSphereList          @0x829215B0 · ...Stream          @0x82923758
//   ExecuteBoxListWithTriangleList           @0x829218B8
//   ExecutePrimitivePairList                 @0x82925798
// Declared here, BODIES IN THE SIBLING PARTFILE ContactGeneratorJob_wQ6_01.cpp (wave Q6,
// cluster gpi -- both are called by ExecutePrimitiveListWithTriangleList and by nothing else):
//   BuildGPInstance                          @0x829222A0  (258)
//   CollideGPInstances                       @0x829253C8  (244)
//
// ─── LAYOUT ──────────────────────────────────────────────────────────────────────────────────
// Every offset below is read out of a body, never guessed. Execute's own pseudocode gives the
// three it touches (`a1[4] = a2` / `a1[16544] = 0` / `a1[16545] = -1`, i.e. +0x10 / +0x10280 /
// +0x10284), AllocateMemory gives the arena base and bound (`return v10 + a1 + 32` and the
// `>= 0x10000` overflow assert), and ContactGeneratorEntry gives the object stride:
//   0x82921028  ori r9, r11, 0x300        -> 0x10300
//   0x82921030  mullw r10, r10, r9        -> spuId * 0x10300
//   0x82921034  addi r11, r11, -16512     -> 0x831BBF80, the context array base
//
//   +0x00      (16 bytes)   never touched by any body in this family      [UNATTESTED]
//   +0x10      mpJobDescription           Execute's `a1[4] = a2`
//   +0x14      (12 bytes)   never touched                                 [UNATTESTED]
//   +0x20      maArena[65536]             AllocateMemory returns `this + 32 + alignedCursor`
//   +0x10020   (608 bytes)  never touched                                 [UNATTESTED]
//   +0x10280   miAllocCursor              `*(a1 + 66176)`, the bump cursor
//   +0x10284   miMemoryRestorePoint       `*(a1 + 66180)`, -1 == "no scope open"
//   +0x10288   (120 bytes)  never touched                                 [UNATTESTED]
//   sizeof     0x10300 == 66304           == ContactGeneratorEntry's `spuId * 0x10300`
//
// ⚠️ mpJobDescription WIDENS 4 -> 8 on x64, and that is correct here: this object is a
// runtime-carved module static, not a serialized record, and nothing reaches its members through
// a console byte offset (AllocateMemory reaches the arena and the cursor BY NAME, where the
// console reaches them as `this + 32` and `this + 66176`). The widening pushes the arena and the
// cursor off their console seats and NOTHING CARES -- the only cross-processor contract in this
// chain is the 176/192 stream record, and that lives in the descriptor header, gated there.
// ⚠️ The three UNATTESTED spans are named as unattested, NOT as padding: they exist to keep the
// arena and the cursor at their console-relative seats so the 0x10300 stride still describes the
// object. They are not claimed to be free.
//
// ⚠️⚠️ THE ARENA IS ALIGNMENT-SENSITIVE AND NO GATE CAN SEE IT. Every allocation this family
// makes is `AllocateMemory(256, 256)`, and the result is handed straight to 16-byte vector loads
// and stores. AllocateMemory aligns the CURSOR, not the base -- it returns `this + 32 +
// alignedCursor` -- so the returned pointer is 16-aligned only if the object is. On the console
// it was: the array base is 0x831BBF80 and 0x831BBF80 + 32 == 0x831BBFA0, which is 16- but NOT
// 256-aligned, i.e. 16 is the real guarantee the console ever had. `alignas(16)` below reproduces
// exactly that and no more. (The standing hazard this answers: a buffer that was 16-aligned for
// free at a console +0x20 moved to +0x24 after a widening and the first movaps AV'd. Here the
// widening is absorbed by a host-relative pad and the seat is GATED, not assumed.)
// =================================================================================================

#include "types.hpp"

#include <cstddef>   // offsetof (the layout gates at the foot of this header)

namespace CgsSceneManager { namespace CgsCollision {
    struct CollisionJobDescription;
    struct SphereListWithTriangleListJobDesc;
    struct SweptSphereListWithTriangleListJobDesc;
    struct PrimitiveListWithTriangleListJobDesc;
    struct TriangleList;
    struct CollisionResultList;
} }

struct alignas(16) ContactGeneratorJob
{
    // The scratch arena AllocateMemory bump-allocates out of. The console's overflow tripwire
    // compares against this exact number, so it is the arena's SIZE, not a count:
    //   0x829212E8..  `if ( ((a3 + cursor - 1) & mask) + alignedSize >= 0x10000 )` -> assert
    //                 "Trying to use too much memory"  (ContactGeneratorJob.cpp:1720)
    static const s32 KI_ARENA_BYTES = 0x10000;   // 65536

    // ---------------------------------------------------------------------------------------
    // Execute @0x829267E0 -- the job body. Latches the descriptor, resets the bump cursor and
    // invalidates the restore point, then switches on the descriptor's type byte.
    // lpvJobData is EA::Jobs::Job::SetData's payload, i.e. the CollisionBatch's 256-byte
    // descriptor slot.
    // ---------------------------------------------------------------------------------------
    void Execute(void* lpvJobData);

    // ---------------------------------------------------------------------------------------
    // ExecuteLineWithTriangleListStream @0x82921968 (589) -- ⭐ THE TRACTION LINE TEST.
    // Drains the descriptor's command stream: per 176-byte command, test miNumLines segments
    // against miNumTriangleBatches Triangle4 blocks and post one 192-byte result.
    // ---------------------------------------------------------------------------------------
    void ExecuteLineWithTriangleListStream();

    // ---------------------------------------------------------------------------------------
    // ⭐⭐ THE SPHERE CONTACT ARMS — REAL as of walls leg 2 (2026-08-14).
    //
    // ExecuteSphereListWithTriangleListStream @0x829235C8 (100): drain the descriptor's
    // command stream; per 32-byte (host 48) command, build a LOCAL non-stream descriptor
    // (SphereListWithTriangleListJobDesc::Prepare) and run the non-stream worker on it.
    // No AddResult — results land in the command's own CollisionResultList.
    //
    // ExecuteSphereListWithTriangleList @0x829226A8 (967): the real worker. Takes the
    // DESCRIPTOR as a parameter (the console's r4: Execute's case 5 passes lpvJobData
    // straight through, and the stream arm passes its stack-local desc). Loops
    // (Triangle4 batch x sensor sphere), runs the contact kernel
    // CgsGeometric::IntersectTriangle4Sphere_HackyBurnoutVersion @0x8283D2E0, and queues
    // one 80-byte PrimitiveTestResult per hit lane into the descriptor's result list.
    // ---------------------------------------------------------------------------------------
    void ExecuteSphereListWithTriangleList(
        const CgsSceneManager::CgsCollision::SphereListWithTriangleListJobDesc* lpDesc); // @0x829226A8
    void ExecuteSphereListWithTriangleListStream();    // @0x829235C8

    // ---------------------------------------------------------------------------------------
    // ⭐⭐⭐ THE SWEPT (CONTINUOUS) CONTACT ARMS — REAL as of the swept leg (2026-08-16).
    //
    // These are the arms `DoRaceCarWorldContactGeneration` selects ABOVE ~6 m/s
    // (DeformableObject::IsUsingSweptSpheres). Until they landed they were named boot gates,
    // which is why a car doing 30 mph had no body-shell collision anywhere in the map: the
    // in-place arms above were the ONLY implemented generator and the console never routes a
    // fast car through them.
    //
    // ExecuteSweptSphereListWithTriangleListStream @0x82925238 (100): the exact structural
    // twin of the sphere stream arm — drain the command stream, build a local non-stream
    // descriptor per command (SweptSphereListWithTriangleListJobDesc::Prepare), run the
    // worker. AllocateMemory(128,128), asserts :642/:643.
    //
    // ExecuteSweptSphereListWithTriangleList @0x829238E8: the worker. Loops (Triangle4 batch
    // x swept sphere — stride 0x20, `slwi r11, r22, 5`), runs
    // CgsGeometric::IntersectTriangle4SweptSphere @0x8283EF50 and queues one 80-byte
    // PrimitiveTestResult per hit lane. Asserts :497 (the count) and :513/:539/:565/:591
    // (`lResult<k>.IsValid()`, one per unrolled lane).
    // ---------------------------------------------------------------------------------------
    void ExecuteSweptSphereListWithTriangleList(
        const CgsSceneManager::CgsCollision::SweptSphereListWithTriangleListJobDesc* lpDesc); // @0x829238E8
    void ExecuteSweptSphereListWithTriangleListStream();// @0x82925238

    // ---------------------------------------------------------------------------------------
    // ⭐⭐⭐ THE PRIMITIVE-LIST ARMS — the ones BREAKABLE PROPS run on (job types 11 and 12).
    //
    // ExecutePrimitiveListWithTriangleListStream @0x82926650 (100): REAL as of 2026-08-19
    // (wave Q6, cluster pstream). The exact structural twin of the sphere/swept stream arms —
    // drain the descriptor's command stream, build a stack-local NON-stream descriptor per
    // command (PrimitiveListWithTriangleListJobDesc::Prepare), run the non-stream worker on it.
    // No AddResult: results travel through the command's own CollisionResultList, which the
    // poster (BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream @0x82811D40)
    // allocated with PrepareNewPrimitiveTestResultsList.
    //
    // ExecutePrimitiveListWithTriangleList @0x82925908 (849): still a LOUD NAMED GATE — the
    // primitive-vs-triangle narrow phase itself. ⚠️ ITS SIGNATURE IS NOT THE NO-ARG FORM THIS
    // HEADER USED TO DECLARE. Measured two ways: Execute's jump table calls it at 0x829268A8
    // with r4 == lpvJobData untouched (exactly as cases 5 and 13 do for the two workers that
    // already take a descriptor), and the stream arm at 0x829267AC passes r4 == its stack-local
    // descriptor. So it takes the descriptor, and the stream arm could not call it faithfully
    // until this declaration was corrected.
    // ---------------------------------------------------------------------------------------
    void ExecutePrimitiveListWithTriangleList(
        const CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListJobDesc* lpDesc); // @0x82925908
    void ExecutePrimitiveListWithTriangleListStream(); // @0x82926650

    // The other five arms of the switch. Bodies NOT reconstructed -- each is a named
    // one-shot boot gate in the .cpp. Declared so the switch can name them and so the closure
    // is enforced at link time rather than discovered at runtime.
    void ExecuteSphereListWithSphereList();            // @0x829215B0
    void ExecuteSphereListWithSphereListStream();      // @0x82923758
    void ExecuteBoxListWithTriangleList();             // @0x829218B8
    void ExecutePrimitivePairList();                   // @0x82925798

    // LoadPrimitives @0x829210F0 (61) / LoadResultList @0x829211E8 (46) -- the worker's
    // "DMA down" pair (a plain copy on X360; the names are the SPU build's). LoadPrimitives
    // copies a TriangleList {base, count}; LoadResultList copies a CollisionResultList
    // header and asserts its results memory is non-null (:1552).
    void LoadPrimitives(const CgsSceneManager::CgsCollision::TriangleList* lpSourceTriangleList,
                        CgsSceneManager::CgsCollision::TriangleList* lpDestinationTriangleList);
    void LoadResultList(const CgsSceneManager::CgsCollision::CollisionResultList* lpSourceResultList,
                        CgsSceneManager::CgsCollision::CollisionResultList* lpDestinationResultList);

    // AllocateMemory @0x829212A0 -- align the cursor up, hand back the arena slice, advance the
    // cursor by the aligned size. No real allocation happens; the name is the console's.
    void* AllocateMemory(s32 liNumBytes, s32 liAlignment);

    // RestoreMemory @0x82921050 -- pop the bump cursor back to the open scope's restore point.
    void RestoreMemory();

    // --- members -----------------------------------------------------------------------------
    u8    mauUnattested00[0x10];                        // +0x00     [UNATTESTED]
    const CgsSceneManager::CgsCollision::CollisionJobDescription* mpJobDescription; // +0x10
    // The console's gap here is 12 bytes because its pointer above is 4. Sized HOST-relative so
    // the arena keeps its +0x20 seat after the widening, and gated below.
    u8    mauUnattested18[0x20 - 0x10 - sizeof(void*)]; // +0x18 on the host [UNATTESTED]
    u8    maArena[KI_ARENA_BYTES];                      // +0x20
    u8    mauUnattested10020[0x260];                    // +0x10020  [UNATTESTED]
    s32   miAllocCursor;                                // +0x10280
    s32   miMemoryRestorePoint;                         // +0x10284
    u8    mauUnattested10288[0x78];                     // +0x10288  [UNATTESTED]
};

// GATES. The arena seat and the two cursor seats are what AllocateMemory/RestoreMemory/Execute
// were read at; the 0x10300 is the object stride ContactGeneratorEntry multiplies by. The stride
// is NOT load-bearing on the host (the entry indexes a C array, so the compiler's own stride is
// used either way) -- it is kept because it costs nothing and because a change to it means
// somebody edited a member without reading this banner.
static_assert(offsetof(ContactGeneratorJob, maArena)             == 0x20,    "arena @ +0x20");
static_assert(offsetof(ContactGeneratorJob, miAllocCursor)       == 0x10280, "cursor @ +0x10280");
static_assert(offsetof(ContactGeneratorJob, miMemoryRestorePoint)== 0x10284, "restore @ +0x10284");
static_assert(sizeof(ContactGeneratorJob)                        == 0x10300, "sizeof == 0x10300");

// The console's context array: ContactGeneratorEntry indexes `0x831BBF80 + spuId * 0x10300` and
// asserts spuId < 6 (ContactGenerator.cpp:55). Six contexts, kept at the console's count -- the
// same shape and the same reason as gaPolygonSoupTesterJobs.
const s32 KI_NUM_CONTACT_GENERATOR_JOBS = 6;
extern ContactGeneratorJob gaContactGeneratorJobs[KI_NUM_CONTACT_GENERATOR_JOBS];
