// CgsSceneManager::OverlapCullingModule -- contact-generation narrow phase of the
// SceneManager. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x828C18E8   (EXECUTED in boot trace)
//   Prepare                       @ 0x828B52B8
//   Release                       @ 0x828AB448
//   ProcessOverlapsQueue          @ 0x828D0330
//   ProcessAddInternalVolumeQueue @ 0x828B5420
//   DoPairQuery                   @ 0x828C1A18
//
// Construct partitions the in-class VolumeVolumeQuery scratch buffer into an
// rw::collision::VolumeVolumeQuery (asserting the budgeted size + alignment) -- the
// direct analogue of the sibling FineIntersectionTestModule::Construct. Prepare/Release
// drive the same two-step START -> MANAGER -> DONE handshake over the stage enums (the
// post-increment asserts the stage never overruns DONE). The queue processors reset the
// per-frame statistics counters and walk the OverlapCulling IO buffers.
//
// FLAG -- UNRECOVERED IO-BUFFER ACCESSORS: the X360 queue walks read their elements
// through helpers with no reconstructable home yet (the OverlapCulling IO buffer's
// overlap-pair / internal-volume-request queue accessors, sub_828B0698 / sub_828B0740 /
// sub_828AE220 / CgsSceneManager::Overlap, and the per-contact narrow-phase entry points
// rw::collision::PrimitivePairIntersect / VolumeVolumeQuery::GetPrimitiveIntersections /
// CgsSceneManager::Contact_::AddEventSafe). Those accessors index opaque, not-yet-homed
// IO-buffer event queues; naming them would require fabricating their signatures/types,
// which the reconstruction rules forbid. The grounded control flow (asserts, counter
// resets, loop structure) is reconstructed; the element-level body that depends on the
// unrecovered accessors is left documented + minimal and reported in stubs_needed.

#include "GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h"

#include <cstddef>  // offsetof
#include <cstring>  // memset

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"           // EntityManager / VolumeManager
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsContactGenerationIO.h"  // OverlapCullingIO::InputBuffer / OutputBuffer
#include "vendor/renderware/collision/VolumeQuery.hpp"                      // rw::collision::VolumeVolumeQuery

namespace CgsSceneManager
{

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

    // memset(a1 + 100496, 0, 632): zero the internal-collision bookkeeping block (the
    // BitArray + the per-instance index tables' header region the asm clears each prepare).
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
// Update @ 0x828C... (vtable Update slot) -- the per-frame narrow phase. The X360 body
// drives ProcessAddInternalVolumeQueue / ProcessRemoveInternalVolumeQueue then
// ProcessOverlapsQueue / ProcessInternalCollisions / ProcessAccumulatedQueries off the
// input/output buffers. Its full body is not in this dossier's 6 functions; declared
// faithfully and left as the documented dispatch shell driven by the reconstructed
// helpers. (Flagged in stubs_needed.)
// ---------------------------------------------------------------------------
void OverlapCullingModule::Update(OverlapCullingIO::InputBuffer* lpInputBuffer,
                                  OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    if (lpInputBuffer != nullptr)
    {
        ProcessAddInternalVolumeQueue(lpInputBuffer);
        ProcessRemoveInternalVolumeQueue(lpInputBuffer);
        ProcessOverlapsQueue(lpOutputBuffer, lpInputBuffer);
    }
    ProcessInternalCollisions(lpOutputBuffer);
    ProcessAccumulatedQueries(lpOutputBuffer);
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
// ProcessOverlapsQueue @ 0x828D0330
//
//   assert(lpInputBuffer != NULL, 239); assert(lpOutputBuffer != NULL, 240);
//   reset the six per-frame pair/contact counters (a1[110750..110753], [110756], [110757]);
//   queue = <overlap-pair queue of lpInputBuffer>;          ; sub_828B0698(a3)
//   for (i = 0; i < queue.count; ++i) {
//       pair = <queue element i>;                            ; CgsSceneManager::Overlap(queue, i)
//       copy 4 words of the pair into a local OverlappingPair;
//       if (!HIBYTE(pair.word3)) ProcessOverlap(localPair, lpOutputBuffer);
//   }
//
// The counter resets and the loop bound are grounded; the queue accessor
// (sub_828B0698 / CgsSceneManager::Overlap) reads an opaque, not-yet-homed IO event
// queue, so the element walk is documented but not driven (the accessor cannot be named
// without fabricating its signature). (Flagged in stubs_needed.)
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessOverlapsQueue(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                                                const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer != NULL");

    // Reset the per-frame pair/contact statistics (the six a1[...] = 0 stores).
    muNumPrimPrimPairs    = 0;  // a1[110750]
    muNumPrimAggPairs     = 0;  // a1[110751]
    muNumOtherPairs       = 0;  // a1[110752]
    muNumInstanceQueries  = 0;  // a1[110753]
    muNumPrimPrimContacts = 0;  // a1[110756]
    muNumPrimAggContacts  = 0;  // a1[110757]

    // The X360 then walks the input buffer's overlap-pair queue and dispatches each pair
    // (whose flag high-byte is clear) to ProcessOverlap. The queue accessor reads an
    // unrecovered IO event queue; see the file-level FLAG. Not driven here.
}

// ---------------------------------------------------------------------------
// ProcessAddInternalVolumeQueue @ 0x828B5420
//
//   assert(lpInputBuffer != NULL, 309);
//   queue = <add-internal-volume request queue of lpInputBuffer>;   ; sub_828B0740(a2)
//   for (i = 0; i < queue.count; ++i) {
//       ev = <queue element i>;                                      ; sub_828AE220(queue, i)
//       assert(ev.muVolumeInstanceIndex         < KU_MAX_NUM_VOLUME_INSTANCES, 321);
//       assert(ev.muInternalVolumeInstanceIndex < KU_MAX_NUM_VOLUME_INSTANCES, 322);
//       assert(ev.muEscapeVolumeInstanceIndex   < KU_MAX_NUM_VOLUME_INSTANCES, 323);
//       (the CgsBitArray.h:222 over-capacity assert path on the index);
//       mabIsUsingInternalCollision.SetBit(ev.muVolumeInstanceIndex);
//       mauInternalVolumeInstanceIndex[ev.muVolumeInstanceIndex] = ev.muInternalVolumeInstanceIndex;
//       mauEscapeVolumeInstanceIndex  [ev.muVolumeInstanceIndex] = ev.muEscapeVolumeInstanceIndex;
//   }
//
// The 0x13B8 (5048) bound and the three index fields match the OverlapCullingIO::
// AddInternalCollisionVolume event (muVolumeInstanceIndex / muInternalVolumeInstanceIndex /
// muEscapeVolumeInstanceIndex). The assert preamble is grounded; the queue accessor
// (sub_828B0740 / sub_828AE220) reads an unrecovered IO event queue, so the per-event
// body is documented but not driven. (Flagged in stubs_needed.)
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessAddInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");

    // The X360 then walks the add-internal-volume request queue and, per event, sets the
    // is-using-internal-collision bit and records the internal/escape volume-instance
    // indices (each asserted < KU_MAX_NUM_VOLUME_INSTANCES). The queue accessor reads an
    // unrecovered IO event queue; see the file-level FLAG. Not driven here.
}

// ---------------------------------------------------------------------------
// The remaining DWARF-attested helpers. Their X360 bodies depend on the unrecovered
// IO-buffer accessors and/or the not-yet-homed RenderWare narrow-phase API (see the
// file-level FLAG); declared faithfully and left as documented shells so the TU compiles
// and SceneManagerModule keeps building. (All flagged in stubs_needed.)
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessRemoveInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer != NULL");
}

void OverlapCullingModule::ProcessOverlap(OverlappingPair& /*lrOverlappingPair*/,
                                          OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/)
{
}

// DoPairQuery @ 0x828C1A18 -- the narrow-phase pair query (NOT executed in boot trace).
// The X360 body splits primitive-vs-primitive (rw::collision::PrimitivePairIntersect)
// from primitive-vs-aggregate (VolumeVolumeQuery::GetPrimitiveIntersections), constructs
// a CgsSceneManager::Contact per resulting contact point and AddEventSafe's it to the
// output. It depends on the not-yet-homed RenderWare query API and the unrecovered
// OverlapCullingIO output accessor; declaration is faithful, body is left undriven.
u32 OverlapCullingModule::DoPairQuery(OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/,
                                      const VolumeInstance* /*lpVolumeInstanceA*/, u32 /*luVolumeIndexA*/,
                                      const VolRef::Volume* /*lpVolumeA*/,
                                      const VolumeInstance* /*lpVolumeInstanceB*/, u32 /*luVolumeIndexB*/,
                                      const VolRef::Volume* /*lpVolumeB*/, f32 /*lfPadding*/)
{
    return 0;
}

bool OverlapCullingModule::IsInsideEscapeVolume(s32 /*liVolumeInstanceIndex*/)
{
    return false;
}

void OverlapCullingModule::DoInternalCollision(s32 /*liVolumeInstanceIndex*/,
                                               OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/)
{
}

void OverlapCullingModule::ProcessInternalCollisions(OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/)
{
}

void OverlapCullingModule::ProcessAccumulatedQueries(OverlapCullingIO::OutputBuffer* /*lpOutputBuffer*/)
{
}

// Never called at runtime; pins the asm-attested member ORDER (absolute X360 byte
// offsets diverge on the x64 PC build because of the 8-byte pointer width and the
// ModuleSingleBuffered base, so only the relative order is asserted -- exactly as the
// FineIntersectionTestModule sibling).
void OverlapCullingModule::_AssertLayout()
{
    static_assert(sizeof(u8) == 1, "u8 must be one byte");
    static_assert(OverlapCullingModule::KU_VOL_QUERY_MEM_SIZE == 401408,
                  "VolumeVolumeQueryMem budget is 0x62000");
    static_assert(OverlapCullingModule::KU_MAX_NUM_VOLUME_INSTANCES == 5048,
                  "KI_MAX_NUM_VOLUME_INSTANCES == 0x13B8");
    static_assert(offsetof(OverlapCullingModule, mePrepareStage) <
                      offsetof(OverlapCullingModule, meReleaseStage),
                  "mePrepareStage precedes meReleaseStage");
    static_assert(offsetof(OverlapCullingModule, mpVolumeManager) <
                      offsetof(OverlapCullingModule, mpEntityManager),
                  "mpVolumeManager precedes mpEntityManager (X360 word 140 < 141)");
    static_assert(offsetof(OverlapCullingModule, maVolumeVolumeQueryMem) <
                      offsetof(OverlapCullingModule, mabIsUsingInternalCollision),
                  "maVolumeVolumeQueryMem precedes the internal-collision tables");
    static_assert(offsetof(OverlapCullingModule, muNumPrimPrimPairs) <
                      offsetof(OverlapCullingModule, muNumPrimAggContacts),
                  "the per-frame counter block is contiguous and ordered");
}

}  // namespace CgsSceneManager
