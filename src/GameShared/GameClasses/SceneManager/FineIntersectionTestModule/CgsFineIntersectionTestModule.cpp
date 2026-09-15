// CgsSceneManager::FineIntersectionTestModule — fine (narrow-phase) intersection test
// stage of the SceneManager. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @ 0x828B0BF0   (EXECUTED in boot trace)
//   Prepare   @ 0x828AA630
//   Release   @ 0x828AA730
//   ComputeLineTestFine @ 0x828C7D70   (giant VMX128 narrow-phase loop)
//
// Construct partitions the two embedded scratch buffers into a VolumeVolumeQuery and a
// VolumeLineQuery and asserts the budgeted buffer sizes are large enough. Prepare/Release
// drive a two-step START -> MANAGER -> DONE handshake over the stage enums (post-increment
// asserts the stage never overruns DONE).

#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"

#include <cstddef>  // offsetof

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "vendor/renderware/collision/VolumeQuery.hpp"  // rw::collision::VolumeVolumeQuery / VolumeLineQuery

namespace CgsSceneManager
{

// ---------------------------------------------------------------------------
// Construct @ 0x828B0BF0
//
// Sizes and constructs both query objects inside the in-class scratch buffers. The X360
// build budgets 0x49000 (299008) bytes for the volume-volume query and 0x10800 (67584)
// bytes for the volume-line query and asserts the actual GetResourceDescriptor size fits.
// Both queries are sized for 100 volumes / 100 results (the 0x64 immediates).
// ---------------------------------------------------------------------------
void FineIntersectionTestModule::Construct()
{
    mePrepareStage  = E_FINE_INTERSECTION_PREPARE_START;            // word 91648 <- 0
    meReleaseStage  = E_FINE_INTERSECTION_RELEASE_DONE;             // word 91649 <- 2
    mpVolumeManager = nullptr;                                      // word 91652 <- 0
    mpEntityManager = nullptr;                                      // word 91653 <- 0

    // --- volume-volume query ---
    // The X360 descriptor is a 5-entry rw::ResourceDescriptor block; only entry[0].size
    // is consulted here, so a small u32[12] scratch covers it exactly as the asm does.
    u32 laVolumeVolumeDesc[12];
    rw::collision::VolumeVolumeQuery::GetResourceDescriptor(laVolumeVolumeDesc, 100, 100);
    CGS_ASSERT(laVolumeVolumeDesc[0] <= KU_VOLUME_VOLUME_QUERY_MEM_SIZE,
               "VolumeVolumeQueryMem is too small");

    void* lpVolumeVolumeBuffer[5];
    lpVolumeVolumeBuffer[0] = macVolumeVolumeQueryBuffer;  // backing-store base at [0]
    lpVolumeVolumeBuffer[1] = nullptr;
    lpVolumeVolumeBuffer[2] = nullptr;
    lpVolumeVolumeBuffer[3] = nullptr;
    lpVolumeVolumeBuffer[4] = nullptr;
    mpVolumeVolumeQuery = static_cast<rw::collision::VolumeVolumeQuery*>(
        rw::collision::VolumeVolumeQuery::Initialize(lpVolumeVolumeBuffer, 100, 100));  // word 91654

    // --- volume-line query ---
    u32 luVolumeLineDescSize;
    rw::collision::VolumeLineQuery::GetResourceDescriptor(&luVolumeLineDescSize, 100, 100);
    CGS_ASSERT(luVolumeLineDescSize <= KU_VOLUME_LINE_QUERY_MEM_SIZE,
               "VolumeLineQueryMem is too small");

    void* lpVolumeLineBuffer[5];
    lpVolumeLineBuffer[0] = macVolumeLineQueryBuffer;  // backing-store base (= this + 299008)
    lpVolumeLineBuffer[1] = nullptr;
    lpVolumeLineBuffer[2] = nullptr;
    lpVolumeLineBuffer[3] = nullptr;
    lpVolumeLineBuffer[4] = nullptr;
    mpVolumeLineQuery = static_cast<rw::collision::VolumeLineQuery*>(
        rw::collision::VolumeLineQuery::Initialize(lpVolumeLineBuffer, 100, 100));  // word 91655
}

// ---------------------------------------------------------------------------
// Destruct @ 0x828B (DWARF: no body emitted — the X360 destruct is a trivial teardown;
// the query objects live in the in-class scratch buffers so there is nothing to free).
// ---------------------------------------------------------------------------
void FineIntersectionTestModule::Destruct()
{
}

// ---------------------------------------------------------------------------
// Prepare @ 0x828AA630
//
// Two-step handshake. Entry stage 0 (START): wire up the managers, advance START->MANAGER
// ->DONE. Entry stage 1 (MANAGER): advance MANAGER->DONE. Entry stage 2 (DONE): reset to
// START and run the full advance. Any other value asserts. On completion the release stage
// is reset to START and the function returns true. Each post-increment asserts the stage
// never exceeds E_FINE_INTERSECTION_PREPARE_DONE.
// ---------------------------------------------------------------------------
bool FineIntersectionTestModule::Prepare(EntityManager* lpEntityManager, VolumeManager* lpVolumeManager)
{
    if (mePrepareStage != E_FINE_INTERSECTION_PREPARE_START &&
        mePrepareStage != E_FINE_INTERSECTION_PREPARE_MANAGER)
    {
        if (mePrepareStage >= 3)
        {
            CGS_ASSERT(false, "Unrecognised release state");
            return false;
        }
        // mePrepareStage == E_FINE_INTERSECTION_PREPARE_DONE: restart the handshake.
        mePrepareStage = E_FINE_INTERSECTION_PREPARE_START;
    }

    if (mePrepareStage == E_FINE_INTERSECTION_PREPARE_START)
    {
        mpEntityManager = lpEntityManager;  // word 91653
        mpVolumeManager = lpVolumeManager;  // word 91652
        mePrepareStage++;                   // START -> MANAGER
        CGS_ASSERT(mePrepareStage <= E_FINE_INTERSECTION_PREPARE_DONE,
                   "leEnumIndex <= FineIntersectionTestModule::E_FINE_INTERSECTION_PREPARE_DONE");
    }

    mePrepareStage++;                       // MANAGER -> DONE
    CGS_ASSERT(mePrepareStage <= E_FINE_INTERSECTION_PREPARE_DONE,
               "leEnumIndex <= FineIntersectionTestModule::E_FINE_INTERSECTION_PREPARE_DONE");

    meReleaseStage = E_FINE_INTERSECTION_RELEASE_START;
    return true;
}

// ---------------------------------------------------------------------------
// Release @ 0x828AA730
//
// Mirror handshake over meReleaseStage. Entry stage 0 (START): run the manager-release
// teardown then advance START->MANAGER. Entry stage 1 (MANAGER): advance MANAGER->DONE.
// Entry stage 2 (DONE): drop straight through. Any other value asserts. On completion the
// prepare stage is reset to START and the function returns true.
//
// NOTE: the X360 START branch invokes helper sub_828AA338(&meReleaseStage, 0) — an
// external, not-yet-reconstructed teardown that operates on the release stage word. Its
// behavior is not grounded, so it is intentionally NOT modelled here beyond the stage
// transition the surrounding asm makes observable. (Flagged in stubs_needed.)
// ---------------------------------------------------------------------------
bool FineIntersectionTestModule::Release()
{
    if (meReleaseStage == E_FINE_INTERSECTION_RELEASE_START)
    {
        // sub_828AA338(&meReleaseStage, 0): unreconstructed manager-release teardown.
        meReleaseStage++;  // START -> MANAGER
        CGS_ASSERT(meReleaseStage <= E_FINE_INTERSECTION_RELEASE_DONE,
                   "leEnumIndex <= FineIntersectionTestModule::E_FINE_INTERSECTION_RELEASE_DONE");
    }
    else if (meReleaseStage == E_FINE_INTERSECTION_RELEASE_MANAGER)
    {
        meReleaseStage++;  // MANAGER -> DONE
        CGS_ASSERT(meReleaseStage <= E_FINE_INTERSECTION_RELEASE_DONE,
                   "leEnumIndex <= FineIntersectionTestModule::E_FINE_INTERSECTION_RELEASE_DONE");
    }
    else if (meReleaseStage >= 3)
    {
        CGS_ASSERT(false, "Unrecognised release state");
        return false;
    }

    mePrepareStage = E_FINE_INTERSECTION_PREPARE_START;  // word 91648 <- 0
    return true;
}

// ---------------------------------------------------------------------------
// Compute* narrow-phase queries -- ComputeLineTestFine @0x828C7D70, ComputeLineTestNearest
// @0x828C8CC8, ComputeVolumeTestDeepest @0x828C90D0, ComputeVolumeTestFine @0x828C93C8.
//
// MOVED 2026-09-02 (scene-query wave 1) to CgsFineIntersectionTestModule_wSQ1.cpp, which is
// MOUNTED (this TU is not: its Construct/Prepare are still WorldLinkStubs boot gates). They
// were EMPTY bodies here -- silent-drop stubs whose callers read an untouched result record as
// "no hit" -- and are LOUD traps there until the rw::collision query objects they drive are
// real on this host (VolumeLineQuery::GetIntersections is a link-stub). See that TU's banner.
// ---------------------------------------------------------------------------

// Never called at runtime; pins the asm-attested member byte offsets.
void FineIntersectionTestModule::_AssertLayout()
{
    static_assert(sizeof(u8) == 1, "u8 must be one byte");
    static_assert(offsetof(FineIntersectionTestModule, macVolumeVolumeQueryBuffer) == 0x00000,
                  "macVolumeVolumeQueryBuffer @ +0x00000");
    static_assert(offsetof(FineIntersectionTestModule, macVolumeLineQueryBuffer) == 0x49000,
                  "macVolumeLineQueryBuffer @ +0x49000");
    static_assert(offsetof(FineIntersectionTestModule, mePrepareStage) == 0x59800,
                  "mePrepareStage @ +0x59800");
    static_assert(offsetof(FineIntersectionTestModule, meReleaseStage) == 0x59804,
                  "meReleaseStage @ +0x59804");
    static_assert(offsetof(FineIntersectionTestModule, miTextX) == 0x59808, "miTextX @ +0x59808");
    static_assert(offsetof(FineIntersectionTestModule, miTextY) == 0x5980C, "miTextY @ +0x5980C");
    // The trailing pointer members (mpVolumeManager/mpEntityManager/mpVolumeVolumeQuery/
    // mpVolumeLineQuery) sit at X360 byte offsets +0x59810/+0x59814/+0x59818/+0x5981C with
    // 32-bit pointer widths. On the x64 PC build pointers are 8 bytes, so their absolute
    // offsets necessarily diverge; only their relative ORDER is pinned (asserted below).
    static_assert(offsetof(FineIntersectionTestModule, mpVolumeManager) <
                      offsetof(FineIntersectionTestModule, mpEntityManager),
                  "mpVolumeManager precedes mpEntityManager (X360 +0x59810 < +0x59814)");
    static_assert(offsetof(FineIntersectionTestModule, mpEntityManager) <
                      offsetof(FineIntersectionTestModule, mpVolumeVolumeQuery),
                  "mpEntityManager precedes mpVolumeVolumeQuery (X360 +0x59814 < +0x59818)");
    static_assert(offsetof(FineIntersectionTestModule, mpVolumeVolumeQuery) <
                      offsetof(FineIntersectionTestModule, mpVolumeLineQuery),
                  "mpVolumeVolumeQuery precedes mpVolumeLineQuery (X360 +0x59818 < +0x5981C)");
}

}  // namespace CgsSceneManager

// ============================================================================
// FOLDED FROM CgsFineIntersectionTestModule_wSQ1.cpp (wave SQ1) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =============================================================================
// GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule_wSQ1.cpp
//
// The FineIntersectionTestModule's four Compute* entry points (scene-query wave 1, 2026-09-02):
//
//   ComputeLineTestFine      @ 0x828C7D70   (~700 lines of VMX128 pseudocode)
//   ComputeLineTestNearest   @ 0x828C8CC8   (258 insns)
//   ComputeVolumeTestDeepest @ 0x828C90D0
//   ComputeVolumeTestFine    @ 0x828C93C8
//
// Split out of CgsFineIntersectionTestModule.cpp because THAT TU is not mounted (its
// Construct/Prepare are still WorldLinkStubs boot gates -- the module's rw::collision query
// objects are never brought up on this host), while the scene-query dispatchers in
// CgsSceneManagerModule_wSQ1.cpp DO reference these four symbols. This TU is mounted on its own.
//
// ⛔ ALL FOUR ARE LOUD TRAPS, NOT BODIES -- and NOT the empty `{}` silent-drop stubs that stood
// in the unmounted TU until this wave (an untouched OutEventLineTestNearestResult read as "no
// hit"). The reason an honest body is impossible today even with the asm in hand: every one of
// them drives rw::collision::VolumeLineQuery / VolumeVolumeQuery::GetAllIntersections over the
// module's query objects, and on this host (a) VolumeLineQuery::GetIntersections is the
// `return 0` link-stub in AptRenderLinkStubs.cpp and (b) the module's Prepare is inert, so
// mpVolumeLineQuery / mpEntityManager / mpVolumeManager are null. A faithful transcription
// would run, find nothing, and report "no intersection" for every entity -- the exact class of
// plausible-zero this project keeps getting burned by. Parked LOUDLY instead; the console
// address is in every message.
//
// Reachability: SceneManagerModule::ProcessLineTestNearest @0x828D38C0 calls
// ComputeLineTestNearest only when the query's entity-type flags include a NON-world bit and the
// octree returned candidates; the race car's above-ground rays (flags == 2, world only) never
// come here. (The octree LineTest that would precede it is itself a trap -- see
// CgsLooseOctree_wSQ1.cpp -- so this is a second fence, not the first.)
// =============================================================================


namespace CgsSceneManager
{
    void FineIntersectionTestModule::ComputeLineTestFine(const InEventLineTestFine* /*lpQuery*/,
                                                         OutEventLineTestFineResult* /*lpOutResult*/,
                                                         void* /*lpResultsOut*/)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeLineTestFine @0x828C7D70 is not reconstructed "
                          "(rw::collision::VolumeLineQuery::GetIntersections is a link-stub on this host)");
    }

    void FineIntersectionTestModule::ComputeLineTestNearest(const InEventLineTestNearest* /*lpQuery*/,
                                                            OutEventLineTestNearestResult* lpOutResult)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeLineTestNearest @0x828C8CC8 is not reconstructed "
                          "(rw::collision::VolumeLineQuery::GetIntersections is a link-stub on this host)");
        // Never a silent "hit" if execution continues past the trap: say so explicitly rather
        // than leaving the caller's stack record as it was.
        lpOutResult->mbIntersection = false;
    }

    void FineIntersectionTestModule::ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* /*lpQuery*/,
                                                              OutEventVolumeTestDeepestResult* /*lpOutResult*/)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeVolumeTestDeepest @0x828C90D0 is not reconstructed "
                          "(rw::collision::VolumeVolumeQuery is unproven on this host)");
    }

    void FineIntersectionTestModule::ComputeVolumeTestFine(const InEventVolumeTestFine* /*lpQuery*/,
                                                           OutEventVolumeTestFineResult* /*lpOutResult*/,
                                                           void* /*lpEntityBuffer*/)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeVolumeTestFine @0x828C93C8 is not reconstructed "
                          "(rw::collision::VolumeVolumeQuery is unproven on this host)");
    }
}
