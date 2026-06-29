#pragma once

// CgsSceneManager::FineIntersectionTestModule — the SceneManager's fine (exact)
// intersection-test stage. It owns two RenderWare collision query objects backed by
// fixed in-class scratch buffers (a VolumeVolumeQuery and a VolumeLineQuery) and runs
// the per-entity narrow-phase line/volume tests requested by the SceneManagerModule.
//
// Layout / enum values recovered from the DecFIGS DWARF
// (CgsFineIntersectionTestModule.h:54-138) and pinned against the X360 asm:
//   Construct @ 0x828B0BF0 stores into byte offsets 0x59800/0x59804/0x59810/0x59814/
//   0x59818/0x5981C (= word indices 91648/91649/91652/91653/91654/91655). The two query
//   scratch buffers precede those members: macVolumeVolumeQueryBuffer at +0 (299008 bytes)
//   and macVolumeLineQueryBuffer at +299008 (67584 bytes); the VolumeLineQuery buffer base
//   is referenced as word index 74752 (= byte 299008) by Construct. So:
//     +0x00000 macVolumeVolumeQueryBuffer (299008 = KU_VOLUME_VOLUME_QUERY_MEM_SIZE)
//     +0x49000 macVolumeLineQueryBuffer   (67584  = KU_VOLUME_LINE_QUERY_MEM_SIZE)
//     +0x59800 mePrepareStage   (word 91648)
//     +0x59804 meReleaseStage   (word 91649)
//     +0x59808 miTextX          (word 91650)
//     +0x5980C miTextY          (word 91651)
//     +0x59810 mpVolumeManager  (word 91652)
//     +0x59814 mpEntityManager  (word 91653)
//     +0x59818 mpVolumeVolumeQuery (word 91654)
//     +0x5981C mpVolumeLineQuery   (word 91655)

#include "types.hpp"

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"  // CgsSceneManager::LineTestIntersection

namespace rw
{
namespace collision
{
    class VolumeVolumeQuery;
    class VolumeLineQuery;
}
}

namespace CgsSceneManager
{
    // Forward declarations for the SceneManager peer types this module references by
    // pointer (their full class homes are reconstructed in their own TUs).
    class EntityManager;
    class VolumeManager;

    // Forward declarations for the per-query event payloads (defined in the SceneManager
    // IO headers). The Compute* methods take them only by pointer.
    struct InEventLineTestFine;
    struct OutEventLineTestFineResult;
    struct InEventLineTestNearest;
    struct OutEventLineTestNearestResult;
    struct InEventVolumeTestDeepest;
    struct OutEventVolumeTestDeepestResult;
    struct InEventVolumeTestFine;
    struct OutEventVolumeTestFineResult;

    struct FineIntersectionTestModule
    {
        // Memory budget for the two embedded query scratch buffers (CgsFineIntersectionTestModule.h:55-56).
        static const u32 KU_VOLUME_VOLUME_QUERY_MEM_SIZE = 299008;
        static const u32 KU_VOLUME_LINE_QUERY_MEM_SIZE   = 67584;

        // Two-step prepare/release handshakes driven by the SceneManagerModule. The asm
        // post-increments the stage and asserts it stays <= the DONE terminal, so the
        // enumerators are sequential 0/1/2 (DecFIGS CgsFineIntersectionTestModule.h:60-74).
        enum EFineIntersectionTestPrepareStage
        {
            E_FINE_INTERSECTION_PREPARE_START   = 0,
            E_FINE_INTERSECTION_PREPARE_MANAGER = 1,
            E_FINE_INTERSECTION_PREPARE_DONE    = 2,
        };

        enum EFineIntersectionTestReleaseStage
        {
            E_FINE_INTERSECTION_RELEASE_START   = 0,
            E_FINE_INTERSECTION_RELEASE_MANAGER = 1,
            E_FINE_INTERSECTION_RELEASE_DONE    = 2,
        };

        void Construct();
        bool Prepare(EntityManager* lpEntityManager, VolumeManager* lpVolumeManager);
        bool Release();
        void Destruct();

        // Fine narrow-phase queries. Bodies live in CgsFineIntersectionTestModule.cpp;
        // the event/output buffer payload types are taken by pointer only.
        void ComputeLineTestFine(const InEventLineTestFine* lpQuery,
                                 OutEventLineTestFineResult* lpOutResult,
                                 void* lpResultsOut);
        void ComputeLineTestNearest(const InEventLineTestNearest* lpQuery,
                                    OutEventLineTestNearestResult* lpOutResult);
        void ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* lpQuery,
                                      OutEventVolumeTestDeepestResult* lpOutResult);
        void ComputeVolumeTestFine(const InEventVolumeTestFine* lpQuery,
                                   OutEventVolumeTestFineResult* lpOutResult,
                                   void* lpEntityBuffer);

        // Byte-offset pin (validated in a never-called assert in the .cpp).
        static void _AssertLayout();

        // --- members (offsets pinned above) ---
        u8 macVolumeVolumeQueryBuffer[KU_VOLUME_VOLUME_QUERY_MEM_SIZE];  // +0x00000
        u8 macVolumeLineQueryBuffer[KU_VOLUME_LINE_QUERY_MEM_SIZE];      // +0x49000

        EFineIntersectionTestPrepareStage mePrepareStage;  // +0x59800
        EFineIntersectionTestReleaseStage meReleaseStage;  // +0x59804

        s32 miTextX;  // +0x59808
        s32 miTextY;  // +0x5980C

        VolumeManager* mpVolumeManager;  // +0x59810
        EntityManager* mpEntityManager;  // +0x59814

        rw::collision::VolumeVolumeQuery* mpVolumeVolumeQuery;  // +0x59818
        rw::collision::VolumeLineQuery*   mpVolumeLineQuery;    // +0x5981C
    };

    // Post-increment over the prepare/release stage enums (used by Prepare/Release to
    // advance the handshake). DecFIGS CgsFineIntersectionTestModule.h:137-138.
    inline FineIntersectionTestModule::EFineIntersectionTestPrepareStage
    operator++(FineIntersectionTestModule::EFineIntersectionTestPrepareStage& leStage, int)
    {
        const FineIntersectionTestModule::EFineIntersectionTestPrepareStage leOld = leStage;
        leStage = static_cast<FineIntersectionTestModule::EFineIntersectionTestPrepareStage>(
            static_cast<int>(leStage) + 1);
        return leOld;
    }

    inline FineIntersectionTestModule::EFineIntersectionTestReleaseStage
    operator++(FineIntersectionTestModule::EFineIntersectionTestReleaseStage& leStage, int)
    {
        const FineIntersectionTestModule::EFineIntersectionTestReleaseStage leOld = leStage;
        leStage = static_cast<FineIntersectionTestModule::EFineIntersectionTestReleaseStage>(
            static_cast<int>(leStage) + 1);
        return leOld;
    }
}
