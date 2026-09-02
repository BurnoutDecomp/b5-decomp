// =============================================================================
// GameShared/GameClasses/SceneManager/CgsSceneManagerModule_wSQ1.cpp
//
// THE SCENE-QUERY PIPELINE -- the two passes SceneManagerModule::ProcessSceneQueries
// @0x828D57D0 runs over a query input buffer, and the fine-query dispatchers under them.
// Scene-query wave 1 (2026-09-02). Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic
// parity, not byte match); every constant below is read out of the image, never guessed.
//
//   SceneManagerModule::ProcessCoarseQueries                  @ 0x828CE770   (73 insns)   REAL
//   SceneManagerModule::ProcessCoarseSpatialPartitionQueries  @ 0x828CDB80   (79 insns)   REAL (dispatcher)
//   SceneManagerModule::ProcessFineQueries                    @ 0x828D5608   (114 insns)  REAL
//   SceneManagerModule::ProcessFineQueriesDirectly            @ 0x828D4F80   (418 insns)  REAL
//   SceneManagerModule::ProcessLineTestNearest                @ 0x828D38C0   (315 insns)  REAL
//   SceneManagerModule::ProcessTriangleCollisionLineTestNearests @ 0x828D4880 (234 insns) REAL (direct arm), job arm LOUD
//   -- and, as LOUD TRAPS carrying their console address, the eleven sibling handlers this
//      build has no producer for yet (see the block at the end of the file).
//
// ⭐ WHY THIS TU EXISTS. Every race car posts one 10 m "nearest" down-ray per frame
// (VehicleManager::GenerateAboveGroundLineTests @0x82633990, entity-type flags == 2 == the
// WORLD bit). Its answer -- AboveGroundTestResult.mbValid -- is guard 8 of UpdateDriftState
// @0x8261F94C, and until this TU existed the ray had NO consumer on this build:
// WorldModule::BridgePhysicsSceneQueriesToScene was an inert gate and ProcessSceneQueries
// skipped both passes. Measured last wave: 134 drift entries, 134 exits, all guard 8;
// mbValid == 0 on every frame of the session. The chain this file closes is
//   GenerateAboveGroundLineTests -> BridgePhysicsSceneQueriesToScene (type 6 -> the query
//   buffer's nearest-line queue) -> ProcessSceneQueries -> ProcessFineQueries ->
//   ProcessFineQueriesDirectly -> ProcessLineTestNearest (world-only: park the test on the
//   TriCacheQueryBuffer) -> ProcessTriangleCollisionLineTestNearests (line vs the static
//   poly-soup world) -> OutSceneQueryResultsQueue::AddTriangleCollisionLineTestNearestResult
//   (record type 2) -> BridgeSceneQueryResultsToPhysics -> ProcessAboveGroundLineTestsResults.
//
// ⛔ WHAT THIS WAVE DOES NOT LAND (named, not hidden):
//   * (LANDED in wave 1b, same day, b5 c6cb403f) BaseCollisionGenerator::
//     CollideLineAgainstPolySoupListNearest @0x828131C0 -- the ray-vs-world kernel over
//     PolygonSoupListSpatialMap::RunQuery @0x82843A80 and CgsGeometric::
//     IntersectLinePolygonSoupNearestSingleSided @0x8283BC98 -- is now a BODY for lines under
//     20 m (CgsCollisionGenerator_wSQ1.cpp + CgsPolygonSoupTests_LineNearest.cpp). Measured:
//     2165 HIT / 0 MISS, mbValid 274/274, drift held 97 frames, exit guard 4 (run sq1_drift2).
//     Only its 20 m+ arm (sub_82843E98) is still a loud trap.
//   * the job arm of ProcessTriangleCollisionLineTestNearests (>= 100 tests a pass).
//   * the octree arm's callees (LooseOctree::LineTestOptimized, the fine module's Compute*)
//     -- both LOUD traps in their own TUs; the race car's world-only rays never reach them.
//
// Image constants (tools/re/x360rd.py, corroborated against each other and the asm):
//   dword_82F33E44 = 0x00000064 (100)       the job-arm threshold in ProcessTriangleCollisionLineTestNearests
//   dword_82F33F64 = 0xFFFFFFFF             EntityId::KU_INVALID_ENTITY_ID (the invalid exclude id)
//   qword_82F33F70 = 0xFFFFFFFFFFFFFFFF     VolumeInstanceId::KU_INVALID_ID
//   flt_82001CC0   = 0.0f                   the "no hit" line parameter
//   flt_82001C98   = 1.0f / flt_820F259C = FLT_MAX  (fine-module constants, not used here)
// =============================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                      // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                    // CgsDev::Log::gpDebugPrint ([DIAG])
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                                   // IOBufferStack::Create/DestroyIOBuffer<T>
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"                                     // CgsModule::Lock/UnlockBuffersForIO
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                            // InputBuffer_Query / TriCacheQueryBuffer / OutputBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"     // OutSceneQueryResultsQueue<32768>::AddTriangleCollisionLineTestNearestResult
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h" // SpatialPartitionIO::OutputBuffer
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h"   // CoarseQueryResultBuffer<16384>
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h" // SpatialPartition::LineTest
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"  // CgsCollision::CollisionGenerator / BaseCollisionGenerator
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"          // CgsCollision::CollisionResultList / CollisionResult
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                                  // CgsGeometric::Line

#include <stdlib.h>   // getenv ([DIAG] BRN_SCENE_QUERY_DIAG, host only)

namespace CgsSceneManager
{
    // The pass monitors, registered by SceneManagerModule::Construct (CgsSceneManagerModule.cpp,
    // X360 dword_82F33ED0..dword_82F33EF4 -- "       Coarse" / "       Fine" / the six
    // "           Line*/Sphere*/Vol*" sub-monitors / "       TriCollLTs" / "       TriCollLTNs").
    extern s32 siProcessCoarseQueriesPerfMon;                  // dword_82F33ED0
    extern s32 siProcessFineQueriesPerfMon;                    // dword_82F33ED4
    extern s32 siProcessFineQueriesLineFinePerfMon;            // dword_82F33ED8
    extern s32 siProcessFineQueriesLineNearPerfMon;            // dword_82F33EDC
    extern s32 siProcessFineQueriesLineFastDSPerfMon;          // dword_82F33EE0
    extern s32 siProcessFineQueriesSphereFastPerfMon;          // dword_82F33EE4
    extern s32 siProcessFineQueriesVolFinePerfMon;             // dword_82F33EE8  ("VolFine" == ProcessFineVolumeTest)
    extern s32 siProcessFineQueriesVolNearPerfMon;             // dword_82F33EEC  ("VolNear" == ProcessVolumeTestDeepest)
    extern s32 siProcessTriCollisionLineTestsPerfMon;          // dword_82F33EF0
    extern s32 siProcessTriCollisionLineTestNearestsPerfMon;   // dword_82F33EF4

    namespace
    {
        // The console brackets each pass with `if (id > -1) StartMonitor(id)` ... `if (id > -1)
        // StopMonitor(id)` (a literal `cmpwi cr6, r3, -1 ; ble` before every call).
        inline void StartPassMonitor(s32 liMonitor) { if (liMonitor > -1) CgsDev::PerfMonCpu::StartMonitor(liMonitor); }
        inline void StopPassMonitor(s32 liMonitor)  { if (liMonitor > -1) CgsDev::PerfMonCpu::StopMonitor(liMonitor); }

        // Per-pass query counters. X360 .bss dword_83084860 .. dword_8308488C: each fine pass
        // adds its queue length (`lwz r10 ; add r10, r10, count ; stw r10`), and
        // ProcessTriangleCollisionLineTestNearests keeps a high-water mark. They are the scene
        // manager's per-frame statistics (the debug component's readers are not mounted). The
        // DWARF's static list for CgsSceneManagerModule.cpp does not name them, so they carry
        // descriptive names here; nothing keys on the console addresses.
        s32 siStat_NumLineTestFine          = 0;   // dword_83084860
        s32 siStat_NumLineTestNearest       = 0;   // dword_83084864
        s32 siStat_NumLineTestFastDS        = 0;   // dword_83084868
        s32 siStat_NumSphereTestFast        = 0;   // dword_8308486C
        s32 siStat_NumFineVolumeTest        = 0;   // dword_83084870
        s32 siStat_NumVolumeTestDeepest     = 0;   // dword_83084874
        s32 siStat_NumTriColLineTestsIn     = 0;   // dword_83084878  (input buffer's own, before the merge)
        s32 siStat_NumTriColLineTestNearIn  = 0;   // dword_8308487C
        s32 siStat_NumTriColLineTests       = 0;   // dword_83084880  (after the merge)
        s32 siStat_NumTriColLineTestNear    = 0;   // dword_83084884
        s32 siStat_MaxTriColLineTestNear    = 0;   // dword_8308488C  (high-water mark)

        // The two invalid-id sentinels the dispatchers stamp on a MISS result, read from the
        // image (see the file banner). Spelled through the id types' own constants so the two
        // homes cannot drift apart.
        inline EntityId InvalidEntityId()                 { EntityId l; l.SetInvalid(); return l; }           // dword_82F33F64
        inline VolumeInstanceId InvalidVolumeInstanceId() { VolumeInstanceId l; l.SetInvalid(); return l; }   // qword_82F33F70

        // The job-arm threshold of ProcessTriangleCollisionLineTestNearests (dword_82F33E44,
        // .data, image value 0x64). Below it the tests run synchronously on the calling thread.
        const s32 KI_MIN_TRI_COL_LINE_TEST_NEARESTS_FOR_JOB = 100;

        // The console's empty-result line parameter (flt_82001CC0).
        const f32 KF_NO_HIT_LINE_PARAM = 0.0f;

        // A CgsGeometric::Line is two 16-byte lanes; the console fills them with whole-lane
        // `lvx128/stvx128` copies of the query's mLineStart/mLineEnd (all four floats, w included).
        inline Vector3Plus LaneCopy(const Vector3& lrLane)
        {
            Vector3Plus lOut;
            lOut.x = lrLane.x; lOut.y = lrLane.y; lOut.z = lrLane.z; lOut.w = lrLane.w;
            return lOut;
        }
        inline Vector3 ZeroLane() { Vector3 lZero; lZero.SetZero(); return lZero; }   // vspltisw v0, 0
        // The literal zero ids the tri-collision pass stamps (`li r5, 0 ; li r6, 0`).
        inline VolumeInstanceId ZeroVolumeInstanceId() { VolumeInstanceId l; l.muId = 0; return l; }

        // [DIAG] host-only, opt-in (BRN_SCENE_QUERY_DIAG=1): print both sides of every
        // decision on the nearest-line path. Never on by default; nothing behavioural.
        bool SceneQueryDiagEnabled()
        {
            static const bool sbEnabled = []() {
                const char* lpcValue = getenv("BRN_SCENE_QUERY_DIAG");
                return lpcValue != 0 && lpcValue[0] == '1';
            }();
            return sbEnabled;
        }
    }

    // =========================================================================================
    // ProcessCoarseQueries @ 0x828CE770  (CgsSceneManagerModule.cpp:847..)
    //
    //   four null tripwires (:847/:848/:849/:850, non-gating)
    //   0x828CE83C  CreateIOBuffer<SpatialPartitionIO::OutputBuffer>(lpOutputBufferStack, "SpacialPartition")
    //   0x828CE848  LockBuffersForIO(lpSceneOutput /*write*/, lpSceneInput /*read*/)   (sub_823B6FE0)
    //   0x828CE854  LockForWrite(spatialPartitionOut)
    //   0x828CE868  ProcessCoarseSpatialPartitionQueries(this, lpSceneInput, spatialPartitionOut, lpSceneOutput)
    //   0x828CE870  UnlockForWrite(spatialPartitionOut)
    //   0x828CE87C  UnlockBuffersForIO(lpSceneOutput, lpSceneInput)                  (sub_823B7060)
    //   0x828CE888  DestroyIOBuffer<SpatialPartitionIO::OutputBuffer>(lpOutputBufferStack, ...)
    // =========================================================================================
    void SceneManagerModule::ProcessCoarseQueries(CgsModule::IOBufferStack* lpInputBufferStack,
                                                  CgsModule::IOBufferStack* lpOutputBufferStack,
                                                  SceneManagerIO::InputBuffer_Query* lpSceneInputBuffer,
                                                  SceneManagerIO::OutputBuffer* lpSceneOutputBuffer)
    {
        CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");    // :847
        CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");   // :848
        CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");    // :849
        CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");   // :850

        SpatialPartitionIO::OutputBuffer* lpSpatialPartitionOutputBuffer = 0;
        lpOutputBufferStack->CreateIOBuffer(&lpSpatialPartitionOutputBuffer, "SpacialPartition");

        CgsModule::LockBuffersForIO(lpSceneOutputBuffer, lpSceneInputBuffer);
        lpSpatialPartitionOutputBuffer->LockForWrite();

        ProcessCoarseSpatialPartitionQueries(lpSceneInputBuffer, lpSpatialPartitionOutputBuffer,
                                             lpSceneOutputBuffer);

        lpSpatialPartitionOutputBuffer->UnlockForWrite();
        CgsModule::UnlockBuffersForIO(lpSceneOutputBuffer, lpSceneInputBuffer);

        lpOutputBufferStack->DestroyIOBuffer(&lpSpatialPartitionOutputBuffer);
    }

    // =========================================================================================
    // ProcessCoarseSpatialPartitionQueries @ 0x828CDB80  (CgsSceneManagerBridgeFunctions.cpp)
    //
    // Walk the query input's COARSE queue (VariableEventQueue, read-locked getter @0x828AF270)
    // and dispatch each record on its type id:
    //   0 -> ProcessCoarseLineTest(this, spOut, event, sceneOut)          @0x828C6D78
    //   1 -> ProcessCoarseSphereTest(this, spOut, event, sceneOut)        @0x828C6B48
    //   2 -> ProcessCoarseVolumeTest(this, event, spOut, sceneOut)        @0x828C62C0
    //   3 -> ProcessCoarseFrustumTest(this, spOut, event, sceneOut)       @0x828C6918
    //   4 -> ProcessCoarseFrustumTestVp(this, spOut, event, &v15, &v14, sceneOut) @0x828C6518
    //        -- v15 is a 16-byte stack block memset to 0xFF and v14 a 16-byte block memset to 0,
    //           both carved once before the loop (the Vp pass's per-call scratch: a "previous
    //           result" seed of all-ones and a zeroed accumulator); their types belong to that
    //           handler's TU and are carried as opaque 16-byte blocks here.
    //   default -> ignored.
    // The coarse getter is re-fetched for GetNextEvent every iteration (the console calls
    // SceneManagerIO::InputBuffer_Query::GetCoarseQueryQueue again at 0x828CDC98).
    //
    // ⚠️ The case numbers are the console's raw jump-table indices. The tree's
    // SceneManagerIO::ECoarseQueryEvent (CgsSceneManagerIO_CoarseQueryQueue.h) spells only
    // E_IN_EVENT_FRUSTUM_TEST_VP == 4 from an attested `li r5, 4`; its SPHERE == 1 agrees with
    // case 1, but its FRUSTUM == 2 and VOLUME == 8 CONTRADICT this switch (2 dispatches to the
    // VOLUME test, 3 to the frustum test). The switch is the authority; the literals stay
    // literals until that enum is re-attested from its producers.
    // =========================================================================================
    void SceneManagerModule::ProcessCoarseSpatialPartitionQueries(
        const SceneManagerIO::InputBuffer_Query*  lpSceneInputBuffer,
        SpatialPartitionIO::OutputBuffer*         lpSpatialPartitionOutputBuffer,
        SceneManagerIO::OutputBuffer*             lpSceneOutputBuffer)
    {
        alignas(16) u8 laFrustumTestVpScratchA[16];   // memset(v15, 0xFF, 16) @0x828CDBB0
        alignas(16) u8 laFrustumTestVpScratchB[16];   // memset(v14, 0x00, 16) @0x828CDBA8
        for (s32 li = 0; li < 16; ++li) { laFrustumTestVpScratchA[li] = 0xFF; laFrustumTestVpScratchB[li] = 0; }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        for (s32 liId = lpSceneInputBuffer->GetCoarseQueryQueue()->GetFirstEvent(&lpEvent, &liSize);
             liId >= 0;
             liId = lpSceneInputBuffer->GetCoarseQueryQueue()->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liId)
            {
            case 0:
                ProcessCoarseLineTest(lpSpatialPartitionOutputBuffer, lpEvent, lpSceneOutputBuffer);
                break;
            case 1:
                ProcessCoarseSphereTest(lpSpatialPartitionOutputBuffer, lpEvent, lpSceneOutputBuffer);
                break;
            case 2:
                ProcessCoarseVolumeTest(lpEvent, lpSpatialPartitionOutputBuffer, lpSceneOutputBuffer);
                break;
            case 3:
                ProcessCoarseFrustumTest(lpSpatialPartitionOutputBuffer, lpEvent, lpSceneOutputBuffer);
                break;
            case 4:
                ProcessCoarseFrustumTestVp(lpSpatialPartitionOutputBuffer, lpEvent,
                                           laFrustumTestVpScratchA, laFrustumTestVpScratchB,
                                           lpSceneOutputBuffer);
                break;
            default:
                break;
            }
        }
    }

    // =========================================================================================
    // ProcessFineQueries @ 0x828D5608  (CgsSceneManagerModule.cpp:880..)
    //
    //   four null tripwires (:880/:881/:882/:883)
    //   0x828D56E0  CreateIOBuffer<SpatialPartitionIO::OutputBuffer>(outStack, "SpacialPartition")
    //   0x828D56F4  CreateIOBuffer<FineIntersectionTestIO::OutputBuffer>(outStack, "FineTest")
    //   0x828D5708  CreateIOBuffer<SceneManagerIO::TriCacheQueryBuffer>(outStack, "TriCacheQuery")
    //   0x828D5714  LockBuffersForIO(sceneOut /*write*/, sceneIn /*read*/)
    //   0x828D5720  LockForWrite(spatialPartitionOut)
    //   0x828D572C  LockForWrite(fineTestOut)
    //   0x828D5740  CreateIOBuffer<CgsCollision::CollisionGenerator>(INPUT stack, "Contact Generator")
    //   0x828D5758  BaseCollisionGenerator::Prepare(gen, gen + 0x12400, 0x200000)
    //               == CollisionGenerator::Prepare() (the embedded 2 MB results arena; the
    //               same inlined pair VehicleManager::StartVehicleContactGeneration emits)
    //   0x828D5778  ProcessFineQueriesDirectly(this, sceneIn, gen, triCacheQuery, spOut, fineOut, sceneOut)
    //   0x828D5784  DestroyIOBuffer<CollisionGenerator>(INPUT stack, ...)
    //   0x828D578C  UnlockForWrite(spatialPartitionOut)
    //   0x828D5794  UnlockForWrite(fineTestOut)
    //   0x828D57A0  UnlockBuffersForIO(sceneOut, sceneIn)
    //   0x828D57AC  DestroyIOBuffer<TriCacheQueryBuffer>(outStack)
    //   0x828D57B8  DestroyIOBuffer<FineIntersectionTestIO::OutputBuffer>(outStack)
    //   0x828D57C4  DestroyIOBuffer<SpatialPartitionIO::OutputBuffer>(outStack)
    //   (note: the TriCacheQuery buffer is NOT lock-bracketed here -- ProcessFineQueriesDirectly
    //    write-locks it itself.)
    // =========================================================================================
    void SceneManagerModule::ProcessFineQueries(CgsModule::IOBufferStack* lpInputBufferStack,
                                                CgsModule::IOBufferStack* lpOutputBufferStack,
                                                SceneManagerIO::InputBuffer_Query* lpSceneInputBuffer,
                                                SceneManagerIO::OutputBuffer* lpSceneOutputBuffer)
    {
        CGS_ASSERT(lpInputBufferStack != NULL,  "lpInputBufferStack != NULL");    // :880
        CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");   // :881
        CGS_ASSERT(lpSceneInputBuffer != NULL,  "lpSceneInputBuffer != NULL");    // :882
        CGS_ASSERT(lpSceneOutputBuffer != NULL, "lpSceneOutputBuffer != NULL");   // :883

        SpatialPartitionIO::OutputBuffer*      lpSpatialPartitionOutputBuffer = 0;
        FineIntersectionTestIO::OutputBuffer*  lpFineTestOutputBuffer         = 0;
        SceneManagerIO::TriCacheQueryBuffer*   lpTriCacheQueryBuffer          = 0;
        lpOutputBufferStack->CreateIOBuffer(&lpSpatialPartitionOutputBuffer, "SpacialPartition");
        lpOutputBufferStack->CreateIOBuffer(&lpFineTestOutputBuffer, "FineTest");
        lpOutputBufferStack->CreateIOBuffer(&lpTriCacheQueryBuffer, "TriCacheQuery");

        CgsModule::LockBuffersForIO(lpSceneOutputBuffer, lpSceneInputBuffer);
        lpSpatialPartitionOutputBuffer->LockForWrite();
        lpFineTestOutputBuffer->LockForWrite();

        CgsCollision::CollisionGenerator* lpCollisionGenerator = 0;
        lpInputBufferStack->CreateIOBuffer(&lpCollisionGenerator, "Contact Generator");
        lpCollisionGenerator->Prepare();

        ProcessFineQueriesDirectly(lpSceneInputBuffer, lpCollisionGenerator, lpTriCacheQueryBuffer,
                                   lpSpatialPartitionOutputBuffer, lpFineTestOutputBuffer,
                                   lpSceneOutputBuffer);

        lpInputBufferStack->DestroyIOBuffer(&lpCollisionGenerator);

        lpSpatialPartitionOutputBuffer->UnlockForWrite();
        lpFineTestOutputBuffer->UnlockForWrite();
        CgsModule::UnlockBuffersForIO(lpSceneOutputBuffer, lpSceneInputBuffer);

        lpOutputBufferStack->DestroyIOBuffer(&lpTriCacheQueryBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpFineTestOutputBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpSpatialPartitionOutputBuffer);
    }

    // =========================================================================================
    // ProcessFineQueriesDirectly @ 0x828D4F80  (CgsSceneManagerBridgeFunctions.cpp:579..)
    //
    //   four null tripwires (:579 sceneIn / :580 spatialPartitionOut / :581 fineTestOut / :582 sceneOut)
    //   0x828D5044  LockForWrite(triCacheQuery)
    //   six passes, each `StartMonitor ; n = queue.GetLength() ; stat += n ; for i < n:
    //   handler(queue.GetEvent(i)) ; StopMonitor`, in this order:
    //     LineFine   (0x828AF318 / GetEvent 0x828ACED0 stride 64)  -> ProcessLineTestFine
    //     LineNear   (0x828AF3C0 / GetEvent 0x828ACF78 stride 64)  -> ProcessLineTestNearest
    //     LineFastDS (0x828AF468 / inline GetEvent stride 64)      -> ProcessLineTestFastDoubleSided
    //     SphereFast (0x828AF510 / GetEvent 0x828AD020)            -> ProcessSphereTestFast
    //     VolNear    (0x828AF5B8 / inline GetEvent stride 224)     -> ProcessVolumeTestDeepest
    //     VolFine    (0x828AF660 / inline GetEvent stride 224)     -> ProcessFineVolumeTest
    //   (each inline GetEvent carries the three CgsBaseEventQueue.h:272/:274/:275 tripwires
    //    -- the queue's own GetEvent does that on the host)
    //   0x828D54D4..0x828D5560  stats += the input buffer's own tri-collision queue lengths, then
    //                           MERGE the input buffer's three tri-collision queues onto the
    //                           TriCacheQueryBuffer's (BaseEventQueue<T>::Append x3:
    //                           sub_828B9668 LineTest, sub_828B9758 LineTestNearest, and the
    //                           InEventTriangleCollisionSphereTest Append @0x828B9848)
    //   0x828D5568..0x828D55A8  stats += the merged lengths
    //   0x828D55BC  ProcessTriangleCollisionLineTests(this, gen, triCacheQuery.LineTestQueue, sceneOut)
    //   0x828D55D8  ProcessTriangleCollisionLineTestNearests(this, gen, triCacheQuery.NearestQueue, sceneOut)
    //   0x828D55F4  ProcessTriangleCollisionSphereTests(this, gen, triCacheQuery.SphereQueue, sceneOut)
    //   0x828D55FC  UnlockForWrite(triCacheQuery)
    //
    // The per-pass queue length is read ONCE before each loop (`lwz r28, 8(r3)`), so handlers
    // that push more work onto the SAME queue are not re-walked -- reproduced with a hoisted
    // count. (None of the six handlers appends to its own input queue; the nearest handler
    // pushes onto the TriCacheQueryBuffer, which is drained after the merge below.)
    // =========================================================================================
    void SceneManagerModule::ProcessFineQueriesDirectly(
        const SceneManagerIO::InputBuffer_Query*   lpSceneInputBuffer,
        CgsCollision::BaseCollisionGenerator*      lpCollisionGenerator,
        SceneManagerIO::TriCacheQueryBuffer*       lpTriCacheQueryBuffer,
        SpatialPartitionIO::OutputBuffer*          lpSpatialPartitionOutputBuffer,
        FineIntersectionTestIO::OutputBuffer*      lpFineTestOutputBuffer,
        SceneManagerIO::OutputBuffer*              lpSceneOutputBuffer)
    {
        CGS_ASSERT(lpSceneInputBuffer != NULL,             "lpSceneInputBuffer != NULL");              // :579
        CGS_ASSERT(lpSpatialPartitionOutputBuffer != NULL, "lpSpatialPartitionOutputBuffer != NULL");  // :580
        CGS_ASSERT(lpFineTestOutputBuffer != NULL,         "lpFineTestOutputBuffer != NULL");          // :581
        CGS_ASSERT(lpSceneOutputBuffer != NULL,            "lpSceneOutputBuffer != NULL");             // :582

        lpTriCacheQueryBuffer->LockForWrite();

        // ---- LineFine ------------------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesLineFinePerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineLineTestQueue* lpQueue =
                lpSceneInputBuffer->GetFineLineTestQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumLineTestFine += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessLineTestFine(lpCollisionGenerator, lpTriCacheQueryBuffer,
                                    lpSpatialPartitionOutputBuffer,
                                    &lpSceneInputBuffer->GetFineLineTestQueue()->GetEvent(li),
                                    lpSceneOutputBuffer, lpFineTestOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesLineFinePerfMon);

        // ---- LineNear ------------------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesLineNearPerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineLineTestNearestQueue* lpQueue =
                lpSceneInputBuffer->GetFineLineTestNearestQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumLineTestNearest += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessLineTestNearest(lpCollisionGenerator, lpTriCacheQueryBuffer,
                                       lpSpatialPartitionOutputBuffer,
                                       &lpSceneInputBuffer->GetFineLineTestNearestQueue()->GetEvent(li),
                                       lpSceneOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesLineNearPerfMon);

        // ---- LineFastDS ----------------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesLineFastDSPerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineLineTestFastDoubleSidedQueue* lpQueue =
                lpSceneInputBuffer->GetFineLineTestFastDoubleSidedQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumLineTestFastDS += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessLineTestFastDoubleSided(lpCollisionGenerator, lpTriCacheQueryBuffer,
                                               lpSpatialPartitionOutputBuffer,
                                               &lpSceneInputBuffer->GetFineLineTestFastDoubleSidedQueue()->GetEvent(li),
                                               lpSceneOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesLineFastDSPerfMon);

        // ---- SphereFast ----------------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesSphereFastPerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineSphereTestFastQueue* lpQueue =
                lpSceneInputBuffer->GetFineSphereTestFastQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumSphereTestFast += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessSphereTestFast(lpCollisionGenerator, lpTriCacheQueryBuffer,
                                      &lpSceneInputBuffer->GetFineSphereTestFastQueue()->GetEvent(li),
                                      lpSpatialPartitionOutputBuffer, lpSceneOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesSphereFastPerfMon);

        // ---- VolNear (deepest) ---------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesVolNearPerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineVolumeTestDeepestQueue* lpQueue =
                lpSceneInputBuffer->GetFineVolumeTestDeepestQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumVolumeTestDeepest += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessVolumeTestDeepest(lpCollisionGenerator, lpTriCacheQueryBuffer,
                                         &lpSceneInputBuffer->GetFineVolumeTestDeepestQueue()->GetEvent(li),
                                         lpSpatialPartitionOutputBuffer, lpSceneOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesVolNearPerfMon);

        // ---- VolFine -------------------------------------------------------------------------
        StartPassMonitor(siProcessFineQueriesVolFinePerfMon);
        {
            const SceneManagerIO::InputBuffer_Query::InFineVolumeTestQueue* lpQueue =
                lpSceneInputBuffer->GetFineVolumeTestQueue();
            const s32 liCount = lpQueue->GetLength();
            siStat_NumFineVolumeTest += liCount;
            for (s32 li = 0; li < liCount; ++li)
            {
                ProcessFineVolumeTest(&lpSceneInputBuffer->GetFineVolumeTestQueue()->GetEvent(li),
                                      lpSpatialPartitionOutputBuffer, lpSceneOutputBuffer,
                                      lpFineTestOutputBuffer);
            }
        }
        StopPassMonitor(siProcessFineQueriesVolFinePerfMon);

        // ---- merge the input buffer's own triangle-collision tests onto the pass buffer ------
        siStat_NumTriColLineTestsIn    += lpTriCacheQueryBuffer->GetTriangleCollisionLineTestQueue()->GetLength();
        siStat_NumTriColLineTestNearIn += lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->GetLength();

        lpTriCacheQueryBuffer->GetTriangleCollisionLineTestQueue()->Append(
            *lpSceneInputBuffer->GetTriangleCollisionLineTestQueue());                     // sub_828B9668
        lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->Append(
            *lpSceneInputBuffer->GetTriangleCollisionLineTestNearestQueue());              // sub_828B9758
        lpTriCacheQueryBuffer->GetTriangleCollisionSphereTestQueue()->Append(
            *lpSceneInputBuffer->GetTriangleCollisionSphereTestQueue());                   // @0x828B9848

        siStat_NumTriColLineTests    += lpTriCacheQueryBuffer->GetTriangleCollisionLineTestQueue()->GetLength();
        siStat_NumTriColLineTestNear += lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->GetLength();

        // ---- the three triangle-collision passes ---------------------------------------------
        ProcessTriangleCollisionLineTests(lpCollisionGenerator,
                                          lpTriCacheQueryBuffer->GetTriangleCollisionLineTestQueue(),
                                          lpSceneOutputBuffer);
        ProcessTriangleCollisionLineTestNearests(lpCollisionGenerator,
                                                 lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue(),
                                                 lpSceneOutputBuffer);
        ProcessTriangleCollisionSphereTests(lpCollisionGenerator,
                                            lpTriCacheQueryBuffer->GetTriangleCollisionSphereTestQueue(),
                                            lpSceneOutputBuffer);

        lpTriCacheQueryBuffer->UnlockForWrite();
    }

    // =========================================================================================
    // ProcessLineTestNearest @ 0x828D38C0  (CgsSceneManagerBridgeFunctions.cpp:1198..)
    //
    // r3 = this, r4 = lpCollisionGenerator, r5 = lpTriCacheQueryBuffer, r6 = lpSpatialPartitionOut,
    // r7 = lpQuery (InEventLineTestNearest, 64 bytes), r8 = lpSceneOutputBuffer.
    //
    // 1. WORLD-ONLY SHORT-CUT (0x828D38F4): if mx32EntityTypeFlags == 2 (exactly the world bit)
    //    build an InEventTriangleCollisionLineTestNearest {start, end, queryId} and AddEvent it on
    //    the TriCacheQueryBuffer's nearest queue (write-locked getter @0x828AFC48); return. The
    //    race car's above-ground rays take this arm every frame.
    // 2. Otherwise run the COARSE octree line query into the spatial-partition output's coarse
    //    result buffer: BeginResultsBatch; mpSpatialPartition->LineTest(flags, start, end, buf)
    //    (vtbl+24); read written / attempted / batch pointer; EndResultsBatch.
    // 3. Resolve the exclude entity: if mExcludeEntityId != KU_INVALID_ENTITY_ID look it up
    //    (EntityManager id->index, `short_5(this+2661568, id)` is the table probe); a missing
    //    entity streams "Entity not found (LineTestNearest): <id>" (:1198). Then the two
    //    consistency tripwires (:1201 query-id echo, :1203 attempted == written).
    // 4. If the coarse pass produced candidates -- and they are not ONLY the excluded entity --
    //    build the fine-module query {start, end, queryId, candidate indices, count,
    //    excludeIndex, volumeTypeFlags, excludeParts = (meExclusionMode == 1)} and run
    //    FineIntersectionTestModule::ComputeLineTestNearest (this+0x148C40); keep the result
    //    when it reports an intersection.
    // 5. If the world bit is NOT set (0x828D3B34..): publish the fine result if any (record type 2
    //    via the typed AddEvent, VolumeInstanceId/EntityId resolved through the EntityManager's
    //    index->id lookups), else the EMPTY result (zero position/normal, invalid ids, param 0.0,
    //    mbIntersection 0).
    // 6. If the world bit IS set: with no fine result, park the test on the TriCacheQueryBuffer
    //    exactly as in step 1. With a fine result, ALSO test the line against the static world
    //    synchronously (CollideLineAgainstPolySoupListNearest on &mTriangleCollisionManager --
    //    this+0x3A82C0 IS the TriangleCollisionManager, whose first member is the
    //    PolygonSoupListSpatialMap -- then Finish); if the world produced a result AND the fine
    //    hit's line parameter is greater than the world hit's (vcmpgtfp on the +0x50 lane, ALL
    //    lanes), publish the world hit through AddTriangleCollisionLineTestNearestResult with
    //    the invalid entity / volume-instance ids; otherwise publish the fine result (step 5).
    // =========================================================================================
    void SceneManagerModule::ProcessLineTestNearest(
        CgsCollision::BaseCollisionGenerator*          lpCollisionGenerator,
        SceneManagerIO::TriCacheQueryBuffer*           lpTriCacheQueryBuffer,
        SpatialPartitionIO::OutputBuffer*              lpSpatialPartitionOutputBuffer,
        const SceneManagerIO::InEventLineTestNearest*  lpQuery,
        SceneManagerIO::OutputBuffer*                  lpSceneOutputBuffer)
    {
        // The world bit of the entity-type flags (the `rlwinm r11, r11, 0,30,30` mask @0x828D3B38
        // and the `cmplwi r11, 2` equality @0x828D38F4).
        static const u32 KU_ENTITY_TYPE_FLAG_WORLD = 2u;

        // ---- 1. world-only: hand the test straight to the triangle-collision pass ------------
        if (lpQuery->mx32EntityTypeFlags == KU_ENTITY_TYPE_FLAG_WORLD)
        {
            SceneManagerIO::InEventTriangleCollisionLineTestNearest lTriangleTest;
            lTriangleTest.mLineStart = lpQuery->mLineStart;   // stvx128 +0x00
            lTriangleTest.mLineEnd   = lpQuery->mLineEnd;     // stvx128 +0x10
            lTriangleTest.mQueryId   = lpQuery->mQueryId;     // stw     +0x20
            lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->AddEvent(lTriangleTest);

            if (SceneQueryDiagEnabled())
            {
                *CgsDev::Log::gpDebugPrint << "[scene-query] nearest world-only q="
                    << lpQuery->mQueryId.mId
                    << " start=(" << lpQuery->mLineStart.x << "," << lpQuery->mLineStart.y << "," << lpQuery->mLineStart.z
                    << ") end.y=" << lpQuery->mLineEnd.y
                    << " -> triCache nearest queue len="
                    << lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->GetLength() << "\n";
            }
            return;
        }

        // ---- 2. the coarse octree pass -------------------------------------------------------
        CoarseQueryResultBuffer<16384>* lpCoarseResults = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer();
        lpCoarseResults->BeginResultsBatch();
        mSpatialPartitionManager.GetSpatialPartition()->LineTest(lpQuery->mx32EntityTypeFlags,
                                                                 lpQuery->mLineStart, lpQuery->mLineEnd,
                                                                 lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer());
        const SceneQueryId lCoarseQueryId    = lpQuery->mQueryId;   // `lwz r24, 0x20(r31)` after the call
        const s32  liNumResultsWritten       = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetNumResultsWritten();
        const s32  liNumResultsAttempted     = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetNumResultsAttempted();
        const u16* lpau16ResultsBatch        = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetResultsBatch();
        lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->EndResultsBatch();

        // ---- 3. the exclude entity ------------------------------------------------------------
        const bool lbExcludeParts = (lpQuery->meExclusionMode == SceneManagerIO::E_NEAREST_EXCLUDE_ALL_CHILD_PARTS);
        u16 lu16ExcludeEntityIndex = 0xFFFF;
        if (lpQuery->mExcludeEntityId != InvalidEntityId())
        {
            const s32 liIndex = mEntityManager.GetEntityIndexByID(lpQuery->mExcludeEntityId);
            if (liIndex >= 0)
            {
                lu16ExcludeEntityIndex = static_cast<u16>(liIndex);
            }
            // :1198 -- "Entity not found (LineTestNearest): <id>" (the console streams the id).
            CGS_ASSERT(lu16ExcludeEntityIndex != 0xFFFF, "Entity not found (LineTestNearest)");
        }
        CGS_ASSERT(lpQuery->mQueryId.mId == lCoarseQueryId.mId,
                   "lpInputQuery->mQueryId == lpCoarseResult->mQueryId");                        // :1201
        CGS_ASSERT(liNumResultsAttempted == liNumResultsWritten,
                   "lpCoarseResult->miActualNumResults == lpCoarseResult->miNumResultsStored");   // :1203

        // ---- 4. the fine module over the candidates -------------------------------------------
        FineIntersectionTestIO::OutEventLineTestNearestResult  lFineResult;
        const FineIntersectionTestIO::OutEventLineTestNearestResult* lpFineResult = 0;   // r22
        if (liNumResultsWritten != 0 &&
            !(liNumResultsWritten == 1 && lu16ExcludeEntityIndex == lpau16ResultsBatch[0]))
        {
            FineIntersectionTestIO::InEventLineTestNearest lFineQuery;
            lFineQuery.mLineStart              = lpQuery->mLineStart;                  // +0x00
            lFineQuery.mLineEnd                = lpQuery->mLineEnd;                    // +0x10
            lFineQuery.mQueryId                = lpQuery->mQueryId;                    // +0x20
            lFineQuery.mpau16EntityIndices     = lpau16ResultsBatch;                   // +0x24
            lFineQuery.mu16NumEntities         = static_cast<u16>(liNumResultsWritten);// +0x28
            lFineQuery.mu16ExcludeEntityIndex  = lu16ExcludeEntityIndex;               // +0x2A
            lFineQuery.mxVolumeTypeFlags       = lpQuery->mxVolumeTypeFlags;           // +0x2C
            lFineQuery.mbExcludeParts          = lbExcludeParts;                       // +0x2D

            mFineIntersectionTestModule.ComputeLineTestNearest(&lFineQuery, &lFineResult);
            if (lFineResult.mbIntersection)
            {
                lpFineResult = &lFineResult;
            }
        }

        SceneManagerIO::OutSceneQueryResultsQueue<32768>* lpResultsQueue = lpSceneOutputBuffer->GetResultsQueue();

        // ---- 6. the world bit is set: the static world competes with the fine hit ------------
        if ((lpQuery->mx32EntityTypeFlags & KU_ENTITY_TYPE_FLAG_WORLD) != 0)
        {
            if (lpFineResult == 0)
            {
                SceneManagerIO::InEventTriangleCollisionLineTestNearest lTriangleTest;   // 0x828D3B54..
                lTriangleTest.mLineStart = lpQuery->mLineStart;
                lTriangleTest.mLineEnd   = lpQuery->mLineEnd;
                lTriangleTest.mQueryId   = lpQuery->mQueryId;
                lpTriCacheQueryBuffer->GetTriangleCollisionLineTestNearestQueue()->AddEvent(lTriangleTest);
                return;
            }

            CgsGeometric::Line lLine;                                     // the two lanes at sp+0xC0/0xD0
            lLine.mStart = LaneCopy(lpQuery->mLineStart);
            lLine.mEnd   = LaneCopy(lpQuery->mLineEnd);
            const u16 lu16ResultList = lpCollisionGenerator->CollideLineAgainstPolySoupListNearest(
                lLine, mTriangleCollisionManager.GetPolySoupListSpacialMap(), 0, 0);   // 0x828D3BB0
            lpCollisionGenerator->Finish();                                             // 0x828D3BBC

            CgsCollision::CollisionResultList lResultList = lpCollisionGenerator->GetResultList(lu16ResultList);
            if (lResultList.mu16NumResults != 0)                                        // lhz 0xC
            {
                const CgsCollision::CollisionResult* lpWorldResult = lResultList.GetResult(0);
                if (lpWorldResult != 0)
                {
                    // 0x828D3C00..0x828D3C3C: splat the fine hit's mfLineParam and vcmpgtfp it
                    // against the world result's +0x50 lane; the CR6 "all lanes true" bit decides.
                    // +0x50 is the line-parameter lane of the CollisionResult record (the same
                    // offset AddTriangleCollisionLineTestNearestResult reads as mfLineParam); the
                    // record has no field-level DWARF in the corpus, so this is the documented
                    // raw read the tree already uses for it.
                    const Vector4& lrWorldParamLane =
                        *reinterpret_cast<const Vector4*>(reinterpret_cast<const u8*>(lpWorldResult) + 0x50);
                    const f32 lfFineParam = lpFineResult->mfLineParam;
                    const bool lbWorldIsNearer = (lfFineParam > lrWorldParamLane.x) &&
                                                 (lfFineParam > lrWorldParamLane.y) &&
                                                 (lfFineParam > lrWorldParamLane.z) &&
                                                 (lfFineParam > lrWorldParamLane.w);
                    if (lbWorldIsNearer)
                    {
                        // 0x828D3C44..0x828D3C90: the world hit wins.
                        CgsCollision::CollisionResultList lWinningList = lpCollisionGenerator->GetResultList(lu16ResultList);
                        lpResultsQueue->AddTriangleCollisionLineTestNearestResult(
                            lpQuery->mQueryId, InvalidEntityId(), InvalidVolumeInstanceId(),
                            lWinningList.GetResult(0), true);
                        return;
                    }
                }
            }
            // fall through: the fine hit stands (LABEL_27 with lpFineResult != 0)
        }

        // ---- 5. publish: the fine hit, or the empty result ------------------------------------
        SceneManagerIO::OutEventLineTestNearestResult lResult;
        if (lpFineResult == 0)
        {
            // 0x828D3CA8..0x828D3D00: the EMPTY record.
            lResult.mPosition         = ZeroLane();                  // vspltisw v0, 0
            lResult.mNormal           = ZeroLane();
            lResult.mVolumeInstanceId = InvalidVolumeInstanceId();   // qword_82F33F70
            lResult.mQueryId          = lpQuery->mQueryId;
            lResult.mEntityId         = InvalidEntityId();           // dword_82F33F64
            lResult.mfLineParam       = KF_NO_HIT_LINE_PARAM;        // flt_82001CC0
            lResult.mu16MaterialTag   = 0;
            lResult.mu16GroupTag      = 0;
            lResult.mbIntersection    = false;
        }
        else
        {
            // 0x828D3D14..0x828D3DA0: the fine hit, its pool indices resolved back to ids.
            lResult.mVolumeInstanceId = mEntityManager.GetVolumeInstanceIdByIndex(
                                            static_cast<s32>(lpFineResult->muVolumeInstanceIndex));
            lResult.mEntityId         = mEntityManager.GetEntityIdByIndex(lpFineResult->mu16EntityIndex);
            lResult.mQueryId          = lpQuery->mQueryId;
            lResult.mPosition         = lpFineResult->mPosition;
            lResult.mNormal           = lpFineResult->mNormal;
            lResult.mfLineParam       = lpFineResult->mfLineParam;
            lResult.mu16MaterialTag   = lpFineResult->mu16MaterialTag;
            lResult.mu16GroupTag      = lpFineResult->mu16GroupTag;
            lResult.mbIntersection    = true;
        }
        lpResultsQueue->AddEvent<SceneManagerIO::OutEventLineTestNearestResult>(&lResult, 2);
    }

    // =========================================================================================
    // ProcessTriangleCollisionLineTestNearests @ 0x828D4880  (the TriCollLTNs pass)
    //
    // r3 = this, r4 = lpCollisionGenerator, r5 = the merged nearest queue, r6 = lpSceneOutputBuffer.
    //
    //   StartMonitor(dword_82F33EF4)
    //   dword_8308488C = max(dword_8308488C, queue.length)                  (high-water mark)
    //   if (queue.length >= dword_82F33E44 /*100*/)  -> the JOB arm (0x828D48F0..0x828D4A70):
    //       stream = the generator's line-vs-poly-soup stream (sub_82810FE8 == the
    //       CgsCollisionGenerator.cpp:695/:715 "Failed to allocate stream producer/buffers"
    //       creator); one 112-byte command per test through DataStreamCommandPoster;
    //       RunCollideLineAgainstPolySoupList(gen, &mTriangleCollisionManager, stream, 1, 1, 0)
    //       fans the batches out to PolygonSoupTesterEntry jobs; then one
    //       AddTriangleCollisionLineTestNearestResult per test from the result iterator.
    //   else                                          -> the DIRECT arm (0x828D4A80..0x828D4B30):
    //       for each test: line = {event.mLineStart, event.mLineEnd};
    //           idx = CollideLineAgainstPolySoupListNearest(gen, &line, &mTriangleCollisionManager, 0, 0);
    //           Finish();
    //           list = GetResultList(idx)  (the :303 "luIndex < mu16NumUsedResultLists" tripwire,
    //                                       then the four-word copy)
    //           if (list.mu16NumResults != 0)   (`HIWORD(v21)` == the +0xC halfword)
    //               result = GetResultList(idx).GetResult(0)  (the :143 "lu16Index < mu16NumResults" tripwire)
    //               AddTriangleCollisionLineTestNearestResult(queue, event.mQueryId, 0, 0, result, true)
    //           else
    //               AddTriangleCollisionLineTestNearestResult(queue, event.mQueryId, 0, 0, NULL, false)
    //   StopMonitor(dword_82F33EF4)
    //
    // ⚠️ On the MISS path the console passes entity id 0 and volume-instance id 0 -- NOT the
    // invalid sentinels ProcessLineTestNearest uses (`li r5,0 ; li r6,0` at 0x828D4A38 /
    // 0x828D4B10). Reproduced literally: the consumer keys on mbIntersection, not on the ids.
    //
    // ⛔ The JOB arm is a LOUD trap this wave (its stream creator, the DataStreamCommandPoster
    // command layout and the RunCollideLineAgainstPolySoupList batch descriptor are not in the
    // tree). It is reached only with >= 100 world line tests in ONE query pass; this build's
    // producers post one per live race car.
    // =========================================================================================
    void SceneManagerModule::ProcessTriangleCollisionLineTestNearests(
        CgsCollision::BaseCollisionGenerator*                                  lpCollisionGenerator,
        CgsModule::EventQueue<SceneManagerIO::InEventTriangleCollisionLineTestNearest, 256>* lpQueue,
        SceneManagerIO::OutputBuffer*                                          lpSceneOutputBuffer)
    {
        StartPassMonitor(siProcessTriCollisionLineTestNearestsPerfMon);

        if (lpQueue->GetLength() > siStat_MaxTriColLineTestNear)
        {
            siStat_MaxTriColLineTestNear = lpQueue->GetLength();
        }

        if (lpQueue->GetLength() >= KI_MIN_TRI_COL_LINE_TEST_NEARESTS_FOR_JOB)
        {
            CGS_ASSERT(false, "ProcessTriangleCollisionLineTestNearests @0x828D4880: the JOB arm (>= 100 tests; "
                              "RunCollideLineAgainstPolySoupList @0x82811198 + its stream) is not reconstructed");
        }
        else
        {
            const s32 liCount = lpQueue->GetLength();
            for (s32 li = 0; li < liCount; ++li)
            {
                const SceneManagerIO::InEventTriangleCollisionLineTestNearest& lrTest = lpQueue->GetEvent(li);

                CgsGeometric::Line lLine;                                  // lvx/stvx at sp+0x90/0xA0
                lLine.mStart = LaneCopy(lrTest.mLineStart);
                lLine.mEnd   = LaneCopy(lrTest.mLineEnd);
                const u16 lu16ResultList = lpCollisionGenerator->CollideLineAgainstPolySoupListNearest(
                    lLine, mTriangleCollisionManager.GetPolySoupListSpacialMap(), 0, 0);
                lpCollisionGenerator->Finish();

                CgsCollision::CollisionResultList lResultList = lpCollisionGenerator->GetResultList(lu16ResultList);
                SceneManagerIO::OutSceneQueryResultsQueue<32768>* lpResultsQueue = lpSceneOutputBuffer->GetResultsQueue();
                if (lResultList.mu16NumResults != 0)
                {
                    CgsCollision::CollisionResultList lHitList = lpCollisionGenerator->GetResultList(lu16ResultList);
                    const CgsCollision::CollisionResult* lpHit = lHitList.GetResult(0);
                    if (SceneQueryDiagEnabled())
                    {
                        const f32 lfParam = *reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(lpHit) + 0x50);
                        const Vector3& lrPos = *reinterpret_cast<const Vector3*>(reinterpret_cast<const u8*>(lpHit) + 0x40);
                        *CgsDev::Log::gpDebugPrint << "[scene-query] world nearest HIT q=" << lrTest.mQueryId.mId
                            << " t=" << lfParam << " pos=(" << lrPos.x << "," << lrPos.y << "," << lrPos.z << ")\n";
                    }
                    lpResultsQueue->AddTriangleCollisionLineTestNearestResult(
                        lrTest.mQueryId, EntityId(0u), ZeroVolumeInstanceId(), lpHit, true);
                }
                else
                {
                    if (SceneQueryDiagEnabled())
                    {
                        *CgsDev::Log::gpDebugPrint << "[scene-query] world nearest MISS q="
                            << lrTest.mQueryId.mId << " (result list empty)\n";
                    }
                    lpResultsQueue->AddTriangleCollisionLineTestNearestResult(
                        lrTest.mQueryId, EntityId(0u), ZeroVolumeInstanceId(), 0, false);
                }
            }
        }

        StopPassMonitor(siProcessTriCollisionLineTestNearestsPerfMon);
    }

    // =========================================================================================
    // THE ELEVEN SIBLING HANDLERS -- LOUD TRAPS, each carrying its console address.
    //
    // None of them has a producer on this build: the coarse queue's five record kinds come from
    // the entity modules' SceneQueryInterface coarse tests and the AI (all still gated), and the
    // fine LineFine / FastDS / SphereFast / VolumeDeepest / VolumeFine queues are fed by the
    // race-car / traffic / trigger post-scene bridges, which are WorldLinkStubs gates. A trap
    // here is therefore unreachable today and becomes the FIRST thing a future producer hits --
    // which is the point: never a quiet "no result" for a query somebody asked.
    // =========================================================================================
    void SceneManagerModule::ProcessCoarseLineTest(SpatialPartitionIO::OutputBuffer*, const CgsModule::Event*, SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessCoarseLineTest @0x828C6D78 is not reconstructed");
    }
    void SceneManagerModule::ProcessCoarseSphereTest(SpatialPartitionIO::OutputBuffer*, const CgsModule::Event*, SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessCoarseSphereTest @0x828C6B48 is not reconstructed");
    }
    void SceneManagerModule::ProcessCoarseVolumeTest(const CgsModule::Event*, SpatialPartitionIO::OutputBuffer*, SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessCoarseVolumeTest @0x828C62C0 is not reconstructed");
    }
    void SceneManagerModule::ProcessCoarseFrustumTest(SpatialPartitionIO::OutputBuffer*, const CgsModule::Event*, SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessCoarseFrustumTest @0x828C6918 is not reconstructed");
    }
    void SceneManagerModule::ProcessCoarseFrustumTestVp(SpatialPartitionIO::OutputBuffer*, const CgsModule::Event*, void*, void*, SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessCoarseFrustumTestVp @0x828C6518 is not reconstructed "
                          "(the synchronous VP frustum test; the job path is ProcessFrustumTestJobRequests)");
    }
    void SceneManagerModule::ProcessLineTestFine(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                                                 SpatialPartitionIO::OutputBuffer*, const SceneManagerIO::InEventLineTestFine*,
                                                 SceneManagerIO::OutputBuffer*, FineIntersectionTestIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessLineTestFine @0x828CDCD0 is not reconstructed");
    }
    void SceneManagerModule::ProcessLineTestFastDoubleSided(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                                                            SpatialPartitionIO::OutputBuffer*, const SceneManagerIO::InEventLineTestFastDoubleSided*,
                                                            SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessLineTestFastDoubleSided @0x828D3DB0 is not reconstructed");
    }
    void SceneManagerModule::ProcessSphereTestFast(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                                                   const SceneManagerIO::InEventSphereTestFast*, SpatialPartitionIO::OutputBuffer*,
                                                   SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessSphereTestFast @0x828D4090 is not reconstructed");
    }
    void SceneManagerModule::ProcessVolumeTestDeepest(CgsCollision::BaseCollisionGenerator*, SceneManagerIO::TriCacheQueryBuffer*,
                                                      const SceneManagerIO::InEventVolumeTestDeepest*, SpatialPartitionIO::OutputBuffer*,
                                                      SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460 is not reconstructed");
    }
    void SceneManagerModule::ProcessFineVolumeTest(const SceneManagerIO::InEventVolumeTestFine*, SpatialPartitionIO::OutputBuffer*,
                                                   SceneManagerIO::OutputBuffer*, FineIntersectionTestIO::OutputBuffer*)
    {
        CGS_ASSERT(false, "SceneManagerModule::ProcessFineVolumeTest @0x828CE328 is not reconstructed");
    }
    void SceneManagerModule::ProcessTriangleCollisionLineTests(CgsCollision::BaseCollisionGenerator*,
                                                               CgsModule::EventQueue<SceneManagerIO::InEventTriangleCollisionLineTest, 256>* lpQueue,
                                                               SceneManagerIO::OutputBuffer*)
    {
        // The console runs the pass unconditionally; with an EMPTY queue it does nothing but
        // its monitor bracket, so an empty queue is not a reason to trap. Only actual work is.
        StartPassMonitor(siProcessTriCollisionLineTestsPerfMon);
        CGS_ASSERT(lpQueue->GetLength() == 0,
                   "SceneManagerModule::ProcessTriangleCollisionLineTests @0x828C6FB0 is not reconstructed (tests were queued)");
        StopPassMonitor(siProcessTriCollisionLineTestsPerfMon);
    }
    void SceneManagerModule::ProcessTriangleCollisionSphereTests(CgsCollision::BaseCollisionGenerator*,
                                                                 CgsModule::EventQueue<SceneManagerIO::InEventTriangleCollisionSphereTest, 256>* lpQueue,
                                                                 SceneManagerIO::OutputBuffer*)
    {
        CGS_ASSERT(lpQueue->GetLength() == 0,
                   "SceneManagerModule::ProcessTriangleCollisionSphereTests @0x828B0B30 is not reconstructed (tests were queued)");
    }
}
