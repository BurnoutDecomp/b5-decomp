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
// Compute* narrow-phase queries.
//
// These four functions are NOT reconstructed in this pass. Their X360 bodies are large
// VMX128-heavy loops (ComputeLineTestFine @ 0x828C7D70 alone is ~700 lines of pseudocode
// with raw vector intrinsics) that drive RenderWare collision queries against per-entity
// volume instances. They depend on several SceneManager/RenderWare types that have no
// reconstructed callable home yet:
//   - CgsSceneManager::EntityManager / VolumeManager / VolumeInstance / SceneManagerEntity
//   - rw::collision::VolumeVolumeQuery / VolumeLineQuery runtime API (InitQuery,
//     GetIntersectionResultsBuffer, GetAllIntersections, ...)
//   - rw::collision::ClusteredMesh / KdTreeLineQuery / ClusteredMeshCluster
//   - the InEvent*/OutEvent* query payload structs (mostly undefined in the tree)
// Reconstructing those would require fabricating multiple large shared headers, which the
// reconstruction rules forbid. They are left as empty bodies (declared faithfully in the
// header) and reported in stubs_needed so a follow-up pass can implement them once the
// dependency types land.
// ---------------------------------------------------------------------------
void FineIntersectionTestModule::ComputeLineTestFine(const InEventLineTestFine* /*lpQuery*/,
                                                     OutEventLineTestFineResult* /*lpOutResult*/,
                                                     void* /*lpResultsOut*/)
{
}

void FineIntersectionTestModule::ComputeLineTestNearest(const InEventLineTestNearest* /*lpQuery*/,
                                                        OutEventLineTestNearestResult* /*lpOutResult*/)
{
}

void FineIntersectionTestModule::ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* /*lpQuery*/,
                                                          OutEventVolumeTestDeepestResult* /*lpOutResult*/)
{
}

void FineIntersectionTestModule::ComputeVolumeTestFine(const InEventVolumeTestFine* /*lpQuery*/,
                                                       OutEventVolumeTestFineResult* /*lpOutResult*/,
                                                       void* /*lpEntityBuffer*/)
{
}

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
