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

#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

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
