// ===========================================================================
// CgsSceneManagerModule_wBT_01.cpp  (GameShared/GameClasses/SceneManager)
//
// SceneManagerModule::ProcessSphereTestFast, the fast sphere query of the fine pass.
//
// It lives beside CgsSceneManagerModule.cpp rather than in it because it builds a real
// rw::collision::SphereVolume (SphereVolume::Initialize), whose home is
// vendor/renderware/collision/CollisionVolume.hpp. The parent TU reads volumes through
// SDKs/EATech/rwcollision/volume_debug_access.h, and the two headers each define
// rw::collision::Volume, so they cannot share a translation unit.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

#include <cstring>   // std::memcpy (the fine query's volume copy)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"                                                                   // rw::Resource
#include "vendor/renderware/collision/CollisionVolume.hpp"                                       // rw::collision::SphereVolume
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                               // SceneManagerIO::OutputBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"                         // SceneManagerIO::OutEventSphereTestFastResult
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSphereTest.h"               // SceneManagerIO::InEventSphereTestFast
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"        // OutSceneQueryResultsQueue<32768>
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h"   // SpatialPartitionIO::OutputBuffer
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h"     // CoarseQueryResultBuffer<16384>
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h"   // SpatialPartition::VolumeTest
#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModuleIO.h"   // FineIntersectionTestIO::InEventVolumeTestDeepest
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"   // BaseCollisionGenerator::TestSphereAgainstPolySoupList
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"            // CgsCollision::CollisionResultList
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                                  // CgsGeometric::Sphere

namespace CgsSceneManager
{
    // =========================================================================================
    // ProcessSphereTestFast  (its asserts bake source lines)
    //
    // A yes/no sphere test against the static world and the entities (results-queue type 4).
    //   1. the WORLD bit: TestSphereAgainstPolySoupList on the event's sphere lane (tags 0, 0),
    //      Finish. Any result answers TRUE at once; a miss on a world-ONLY query (flags == 2)
    //      answers FALSE; a mixed query falls through to the entities.
    //   2. the query sphere becomes a real collision volume: SphereVolume::Initialize of the
    //      event radius in a 0x80-byte block through a zeroed rw::Resource whose lane 0 is the
    //      block ("lpVolume != NULL" ; the console also default-constructs an unread
    //      five-lane rw::ResourceDescriptor beside it), placed by a transform with identity
    //      axes whose translation row is the event's whole sphere lane (radius in w).
    //   3. the coarse octree VolumeTest of that volume; the query id re-read; written /
    //      attempted / batch.
    //   4. the exclude entity ("Entity not found (SphereTestFast): ", the console streams
    //      the id after it) and the two tripwires ( -- this handler's own "lRequest"
    //      spelling -- and).
    //   5. no candidate, or exactly the excluded one: FALSE. Otherwise the fine module's
    //      DEEPEST volume test of the sphere volume over the candidates; its hit byte is the
    //      answer. The record carries the event's query id either way.
    // =========================================================================================
    void SceneManagerModule::ProcessSphereTestFast(CgsCollision::BaseCollisionGenerator*         lpCollisionGenerator,
                                                   SceneManagerIO::TriCacheQueryBuffer*          /*lpTriCacheQueryBuffer*/,
                                                   const SceneManagerIO::InEventSphereTestFast*  lpQuery,
                                                   SpatialPartitionIO::OutputBuffer*             lpSpatialPartitionOutputBuffer,
                                                   SceneManagerIO::OutputBuffer*                 lpSceneOutputBuffer)
    {
        // The world bit of the entity-type flags (bit 1).
        static const u32 KU_ENTITY_TYPE_FLAG_WORLD = 2u;
        // The results-queue event type of the fast sphere result.
        static const s32 KI_SPHERE_TEST_FAST_RESULT_EVENT = 4;
        // The stack block the sphere volume is built in, and the slice the fine query copies.
        static const u32 KU_SPHERE_VOLUME_BLOCK_SIZE = 0x80;
        // The exclude-index sentinel.
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;
        static_assert(sizeof(FineIntersectionTestIO::InEventVolumeTestDeepest().mVolumeBuffer) == KU_SPHERE_VOLUME_BLOCK_SIZE,
                      "the fine query's volume slot is one 0x80-byte block");

        SceneManagerIO::OutEventSphereTestFastResult lResult;
        lResult.mQueryId = lpQuery->mQueryId;

        // ---- 1. the world bit: the static world answers first -------------------------------------
        if ((lpQuery->mx32EntityTypeFlags & KU_ENTITY_TYPE_FLAG_WORLD) != 0)
        {
            CgsGeometric::Sphere lSphere;                                   // the whole lane
            lSphere.mPositionRadius.x = lpQuery->mSpherePosPlusRadius.x;
            lSphere.mPositionRadius.y = lpQuery->mSpherePosPlusRadius.y;
            lSphere.mPositionRadius.z = lpQuery->mSpherePosPlusRadius.z;
            lSphere.mPositionRadius.w = lpQuery->mSpherePosPlusRadius.w;
            const u16 lu16ResultList = lpCollisionGenerator->TestSphereAgainstPolySoupList(
                &lSphere, mTriangleCollisionManager.GetPolySoupListSpacialMap(), 0, 0);
            lpCollisionGenerator->Finish();

            if (lpCollisionGenerator->GetResultList(lu16ResultList).mu16NumResults != 0)
            {
                lResult.mbIntersection = true;
                lpSceneOutputBuffer->GetResultsQueue()->AddEvent<SceneManagerIO::OutEventSphereTestFastResult>(
                    &lResult, KI_SPHERE_TEST_FAST_RESULT_EVENT);
                return;
            }
            if (lpQuery->mx32EntityTypeFlags == KU_ENTITY_TYPE_FLAG_WORLD)
            {
                lResult.mbIntersection = false;
                lpSceneOutputBuffer->GetResultsQueue()->AddEvent<SceneManagerIO::OutEventSphereTestFastResult>(
                    &lResult, KI_SPHERE_TEST_FAST_RESULT_EVENT);
                return;
            }
        }

        // ---- 2. the query sphere as a collision volume --------------------------------------------
        alignas(16) u8 laVolumeMemory[KU_SPHERE_VOLUME_BLOCK_SIZE];
        rw::Resource lVolumeResource;                                       // zeroed lanes
        lVolumeResource.m_baseResources[0] = laVolumeMemory;
        const rw::collision::SphereVolume* lpVolume =
            rw::collision::SphereVolume::Initialize(lVolumeResource, lpQuery->mSpherePosPlusRadius.w);

        Matrix44Affine lTransform;
        lTransform.xAxis.x = 1.0f; lTransform.xAxis.y = 0.0f; lTransform.xAxis.z = 0.0f; lTransform.xAxis.w = 0.0f;
        lTransform.yAxis.x = 0.0f; lTransform.yAxis.y = 1.0f; lTransform.yAxis.z = 0.0f; lTransform.yAxis.w = 0.0f;
        lTransform.zAxis.x = 0.0f; lTransform.zAxis.y = 0.0f; lTransform.zAxis.z = 1.0f; lTransform.zAxis.w = 0.0f;
        lTransform.wAxis.x = lpQuery->mSpherePosPlusRadius.x;
        lTransform.wAxis.y = lpQuery->mSpherePosPlusRadius.y;
        lTransform.wAxis.z = lpQuery->mSpherePosPlusRadius.z;
        lTransform.wAxis.w = lpQuery->mSpherePosPlusRadius.w;

        CGS_ASSERT(lpVolume != 0, "lpVolume != NULL");

        // ---- 3. the coarse octree pass ------------------------------------------------------------
        lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->BeginResultsBatch();
        mSpatialPartitionManager.GetSpatialPartition()->VolumeTest(lpQuery->mx32EntityTypeFlags,
                                                                   reinterpret_cast<const VolRef::Volume*>(lpVolume),
                                                                   &lTransform,
                                                                   lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer());
        const SceneQueryId lCoarseQueryId    = lpQuery->mQueryId;
        const s32  liNumResultsWritten       = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetNumResultsWritten();
        const s32  liNumResultsAttempted     = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetNumResultsAttempted();
        const u16* lpau16ResultsBatch        = lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->GetResultsBatch();
        lpSpatialPartitionOutputBuffer->GetCoarseResultBuffer()->EndResultsBatch();

        // ---- 4. the exclude entity ----------------------------------------------------------------
        const bool lbExcludeParts = (lpQuery->meExclusionMode == SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
        u16 lu16ExcludeEntityIndex = KU16_NO_EXCLUDE_ENTITY_INDEX;
        EntityId lInvalidEntityId;
        lInvalidEntityId.SetInvalid();
        if (lpQuery->mExcludeEntityId != lInvalidEntityId)
        {
            const s32 liIndex = mEntityManager.GetEntityIndexByID(lpQuery->mExcludeEntityId);
            if (liIndex >= 0)
            {
                lu16ExcludeEntityIndex = static_cast<u16>(liIndex);
            }
            CGS_ASSERT(lu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX,
                       "Entity not found (SphereTestFast): ");
        }
        CGS_ASSERT(lpQuery->mQueryId.mId == lCoarseQueryId.mId,
                   "lRequest.mQueryId == lpCoarseResult->mQueryId");
        CGS_ASSERT(liNumResultsAttempted == liNumResultsWritten,
                   "lpCoarseResult->miActualNumResults == lpCoarseResult->miNumResultsStored");

        // ---- 5. the fine module's deepest test over the candidates --------------------------------
        bool lbIntersection = false;
        if (liNumResultsWritten != 0 &&
            !(liNumResultsWritten == 1 && lu16ExcludeEntityIndex == lpau16ResultsBatch[0]))
        {
            FineIntersectionTestIO::InEventVolumeTestDeepest lFineQuery;
            lFineQuery.mTransform = lTransform;                                                         // +0x00
            std::memcpy(&lFineQuery.mVolumeBuffer, lpVolume, KU_SPHERE_VOLUME_BLOCK_SIZE);             // +0x40
            lFineQuery.mQueryId                = lpQuery->mQueryId;                                     // +0xC0
            lFineQuery.mpau16EntityIndices     = lpau16ResultsBatch;                                    // +0xC4
            lFineQuery.mu16NumEntities         = static_cast<u16>(liNumResultsWritten);                 // +0xC8
            lFineQuery.mu16ExcludeEntityIndex  = lu16ExcludeEntityIndex;                                // +0xCA
            lFineQuery.mxVolumeTypeFlags       = lpQuery->mxVolumeTypeFlags;                            // +0xCC
            lFineQuery.mbExcludeParts          = lbExcludeParts;                                        // +0xCD

            // As in ProcessVolumeTestDeepest, the host record starts at zero: the module writes the
            // depth only on a hit, and this handler reads the hit byte alone.
            FineIntersectionTestIO::OutEventVolumeTestDeepestResult lFineResult = {};
            mFineIntersectionTestModule.ComputeVolumeTestDeepest(&lFineQuery, &lFineResult);
            lbIntersection = lFineResult.mbIntersection;
        }

        lResult.mbIntersection = lbIntersection;
        lpSceneOutputBuffer->GetResultsQueue()->AddEvent<SceneManagerIO::OutEventSphereTestFastResult>(
            &lResult, KI_SPHERE_TEST_FAST_RESULT_EVENT);
    }
}
