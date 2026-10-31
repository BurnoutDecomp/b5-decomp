#include "GameShared/Jobs/FrustumTest/FrustumTest.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eajobs/job_types.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"

#include <cstddef>  // offsetof
#include <cstring>  // std::memcpy (models the Xbox XMemCpy / VMX block copies)
#include <vectorintrinsics.h>
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere

// Byte-offset pins: the operator= asm (0x82BDF950) attests these exact region boundaries.
namespace
{
    CGS_STATIC_ASSERT((sizeof(CgsGeometric::Frustum) == 0x80), "Frustum must be 128 bytes");
    CGS_STATIC_ASSERT((offsetof(CgsSceneManager::FrustumJobQueryInfo, maViewProjection) == 0x500),
                  "maViewProjection must be at +0x500");
    CGS_STATIC_ASSERT((offsetof(CgsSceneManager::FrustumJobQueryInfo, max32EntityTypeFlags) == 0x780),
                  "max32EntityTypeFlags must be at +0x780");
    CGS_STATIC_ASSERT((offsetof(CgsSceneManager::FrustumJobQueryInfo, muNumQueries) == 0x7A8),
                  "muNumQueries must be at +0x7A8");

    // FrustumTestJobData pins (operator= @0x82BDFC48 attests these exact region boundaries).
    CGS_STATIC_ASSERT((offsetof(CgsSceneManager::FrustumTestJobData, mQueryInfo) == 0x20),
                  "mQueryInfo must be at +0x20");
    CGS_STATIC_ASSERT((offsetof(CgsSceneManager::FrustumTestJobData, mpResultBuffer) == 0x7D0),
                  "mpResultBuffer must be at +0x7D0");
}

// CgsSceneManager::FrustumJobQueryInfo::operator= @ 0x82BDF950
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 build lowers this
// assignment as a full, contiguous copy of the struct's 0x7AC live bytes, in three regions:
//
//   maFrustum[10]            (0x000..0x500): ten memcpy(dst, src, 0x80) calls
//                                            (addi r3/r4 by 0x80; li r5,0x80; bl memcpy x10)
//   maViewProjection[10]     (0x500..0x780): VMX lvx128/stvx128 16-byte lane copies (40 lanes;
//                                            10 Matrix44 * 4 Vector4 rows)
//   max32EntityTypeFlags[10] + muNumQueries  (0x780..0x7AC): eleven lwz/stw 32-bit word copies
//
// No field is skipped and no byte outside [0,0x7AC) is written (the 4 trailing alignment pad
// bytes at 0x7AC..0x7B0 are left untouched), so this is exactly a complete memberwise copy.
// Reproduced here as the three per-region member copies (memcpy stands in for the Xbox block /
// VMX intrinsic copies), returning *this -- matching the X360 `return this`.
//
// (This assignment operator is invoked by CgsSceneManager::FrustumTestJobData::operator=,
// which copies its embedded mQueryInfo member.)

namespace CgsSceneManager
{
    FrustumJobQueryInfo& FrustumJobQueryInfo::operator=(const FrustumJobQueryInfo& lrSource)
    {
        // Region 1 (0x000..0x500): the 10 swizzled frustums.
        std::memcpy(maFrustum, lrSource.maFrustum, sizeof(maFrustum));
        // Region 2 (0x500..0x780): the 10 view-projection matrices.
        std::memcpy(maViewProjection, lrSource.maViewProjection, sizeof(maViewProjection));
        // Region 3 (0x780..0x7A8): the 10 entity-type filter flag words.
        std::memcpy(max32EntityTypeFlags, lrSource.max32EntityTypeFlags, sizeof(max32EntityTypeFlags));
        // (0x7A8): the live query count.
        muNumQueries = lrSource.muNumQueries;
        return *this;
    }

    // CgsSceneManager::FrustumTestJobData::operator= @ 0x82BDFC48
    //
    // ARTIST copies the six typed header members, the query info, and three trailer members.
    FrustumTestJobData& FrustumTestJobData::operator=(const FrustumTestJobData& lrSource)
    {
        // +0x00..+0x14: the six job-control header words (6 lwz/stw).
        mpNodes = lrSource.mpNodes;
        mpNodeTypeMasks = lrSource.mpNodeTypeMasks;
        mpEntityLinks = lrSource.mpEntityLinks;
        mpEntitySpheres = lrSource.mpEntitySpheres;
        muNumNodes = lrSource.muNumNodes;
        muMaxEntities = lrSource.muMaxEntities;
        mQueryInfo = lrSource.mQueryInfo;
        mpResultBuffer = lrSource.mpResultBuffer;
        muMaxResults = lrSource.muMaxResults;
        miDebugMode = lrSource.miDebugMode;
        return *this;
    }

    // Worker assertions use PrintStringed followed by the native MSR break twice.
    static void FrustumJobAssert(const char* lpFile, int liLine, const char* lpMessage)
    {
        CgsDev::Assert::PrintStringed(lpFile, 0, liLine, lpMessage);
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(lpFile, 0, liLine, lpMessage);
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }

    // 0x82BDF7B8 and 0x82BDF848: resolve and enqueue entity indices.
    SpatialPartitionEntityLink& FrustumTestJob::GetEntityNode(u16 lu16Entity)
    {
        if (lu16Entity >= SpatialPartition::KI_MAX_NUM_ENTITIES)
            FrustumJobAssert("d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\jobs\\frustumtest\\FrustumTestJob.h",
                             317, "luIndex < KI_MAX_NUM_ENTITIES");
        return mpEntityArray[lu16Entity];
    }

    void FrustumTestJob::PushEntityForTesting(u16 lu16Entity)
    {
        if (lu16Entity >= SpatialPartition::KI_MAX_NUM_ENTITIES)
            FrustumJobAssert("d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\jobs\\frustumtest\\FrustumTestJob.h",
                             329, "lu16Index<KI_MAX_NUM_ENTITIES");
        if (muNumEntitiesToTest >= SpatialPartition::KI_MAX_NUM_ENTITIES)
            FrustumJobAssert("d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\jobs\\frustumtest\\FrustumTestJob.h",
                             330, "muNumEntitiesToTest<KI_MAX_NUM_ENTITIES");
        mau16EntitiesToTest[muNumEntitiesToTest] = lu16Entity;
        ++muNumEntitiesToTest;
    }

    // CgsSceneManager::FrustumTestJob::Execute @ 0x82BE0158
    //
    // Runs every live query in mJobData.mQueryInfo. For each query: snapshot the VP matrix +
    // swizzled frustum + entity-type mask into the working slots, reset the candidate list,
    // open a result-buffer slot, descend the octree, run the narrow sphere test, then advance
    // the buffer to the next query slot. The X360 caches mpRootNodeInfo(header[1]) and
    // mpEntityArray(header[2]) into +0x5680/+0x5684 once before the loop.
    void* FrustumTestJob::Execute(const FrustumTestJobData& lrJobData)
    {
        mJobData = lrJobData;
        mpNodeEntityInfoArray = mJobData.mpNodeTypeMasks;
        mpEntityArray = mJobData.mpEntityLinks;

        for (u32 luQuery = 0; luQuery < mJobData.mQueryInfo.muNumQueries; ++luQuery)
        {
            std::memcpy(&mCurrentViewProjection, &mJobData.mQueryInfo.maViewProjection[luQuery],
                        sizeof(Matrix44));
            std::memcpy(&mCurrentFrustum, &mJobData.mQueryInfo.maFrustum[luQuery], 0x80);

            muNumEntitiesToTest = 0;
            mxCurrentEntityTypeFlags = mJobData.mQueryInfo.max32EntityTypeFlags[luQuery];

            if (mJobData.muNumNodes > KU_MAX_OCTREE_NODES)
                FrustumJobAssert("D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\FrustumTest\\FrustumTestJob.cpp",
                                 212, "KU_MAX_OCTREE_NODES >= mFrustumTestJobData.muNumNodes");

            JobCoarseResultBuffer* lpResultBuffer = mJobData.mpResultBuffer;
            lpResultBuffer->maQueryOffsets[lpResultBuffer->muNumQueries] = lpResultBuffer->muCurrentWriteOffset;

            FrustumTestRecursive(0);
            TestEntitiesBulk();

            lpResultBuffer = mJobData.mpResultBuffer;
            if (lpResultBuffer->muNumQueries >= KU_JOB_BUFFER_MAX_NUM_QUERIES)
                FrustumJobAssert("..\\..\\GameClasses\\SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h",
                                 113, "muNumQueries < KU_JOB_BUFFER_MAX_NUM_QUERIES");

            ++lpResultBuffer->muNumQueries;
        }

        return mJobData.mpResultBuffer;
    }

    // 0x828B19E8: classify a node's box in homogeneous view-projection space.
    int FrustumTestJob::NodeInsideFrustumVp(const LooseOctreeNode* lpNode, const Matrix44* lpViewProjection)
    {
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lvPosition = __vrlimi(__lvx(&lpNode->mPosition, 0), lvOne, 1, 0);
        const __vector4 lvHalf = __lvx(&lpNode->mHalfDimensions, 0);
        const __vector4 lvRow0 = __lvx(lpViewProjection, 0);
        const __vector4 lvRow1 = __lvx(lpViewProjection, 16);
        const __vector4 lvRow2 = __lvx(lpViewProjection, 32);
        const __vector4 lvRow3 = __lvx(lpViewProjection, 48);
        const __vector4 lvExtent0 = __vmulfp(lvRow0, __vspltw(lvHalf, 0));
        const __vector4 lvExtent1 = __vmulfp(lvRow1, __vspltw(lvHalf, 1));
        const __vector4 lvExtent2 = __vmulfp(lvRow2, __vspltw(lvHalf, 2));
        __vector4 lvCentre = __vmulfp(lvRow0, __vspltw(lvPosition, 0));
        lvCentre = __vmaddfp(lvRow1, __vspltw(lvPosition, 1), lvCentre);
        lvCentre = __vmaddfp(lvRow2, __vspltw(lvPosition, 2), lvCentre);
        lvCentre = __vmaddfp(lvRow3, __vspltw(lvPosition, 3), lvCentre);
        __vector4 lvSign = __vspltisw(-1);
        lvSign = __vslw(lvSign, lvSign);
        const __vector4 lvMin0 = __vandc(__vsubfp(lvExtent0, __vspltw(lvExtent0, 3)), lvSign);
        const __vector4 lvMin1 = __vandc(__vsubfp(lvExtent1, __vspltw(lvExtent1, 3)), lvSign);
        const __vector4 lvMin2 = __vandc(__vsubfp(lvExtent2, __vspltw(lvExtent2, 3)), lvSign);
        const __vector4 lvMax0 = __vandc(__vaddfp(lvExtent0, __vspltw(lvExtent0, 3)), lvSign);
        const __vector4 lvMax1 = __vandc(__vaddfp(lvExtent1, __vspltw(lvExtent1, 3)), lvSign);
        const __vector4 lvMax2 = __vandc(__vaddfp(lvExtent2, __vspltw(lvExtent2, 3)), lvSign);
        const __vector4 lvMinRadius = __vaddfp(__vaddfp(lvMin2, lvMin1), lvMin0);
        const __vector4 lvMaxRadius = __vaddfp(__vaddfp(lvMax0, lvMax1), lvMax2);
        const __vector4 lvLow = __vsubfp(lvCentre, __vspltw(lvCentre, 3));
        const __vector4 lvHigh = __vaddfp(lvCentre, __vspltw(lvCentre, 3));
        const __vector4 lvOutside = __vor(__vcmpgtfp(lvLow, lvMinRadius),
                                          __vnor(__vcmpgefp(lvHigh, __vxor(lvMaxRadius, lvSign)),
                                                 __vcmpgefp(lvHigh, __vxor(lvMaxRadius, lvSign))));
        const __vector4 lvPartial = __vor(__vcmpgtfp(lvLow, __vxor(lvMinRadius, lvSign)),
                                          __vnor(__vcmpgefp(lvHigh, lvMaxRadius),
                                                 __vcmpgefp(lvHigh, lvMaxRadius)));
        CGS_ALIGN(16) u32 lauOutside[4];
        CGS_ALIGN(16) u32 lauPartial[4];
        __stvx(lvOutside, lauOutside, 0);
        __stvx(lvPartial, lauPartial, 0);
        if ((lauOutside[0] | lauOutside[1] | lauOutside[2] | lauOutside[3]) != 0)
            return 0;
        return (lauPartial[0] | lauPartial[1] | lauPartial[2] | lauPartial[3]) == 0 ? 1 : 2;
    }

    // CgsSceneManager::FrustumTestJob::FrustumTestRecursive @ 0x82BDFFC8
    //
    // Descends the loose octree for the current query. NodeInsideFrustumVp -> 1 (fully inside)
    // hands the whole subtree to TrivialAcceptRecursive; -> 2 (partial) tests this node's own
    // entities then recurses into any child octant whose subtree flags intersect the mask;
    // -> 0 prunes. The +3 octant is folded into the outer loop.
    int FrustumTestJob::FrustumTestRecursive(u16 lu16NodeIndex)
    {
        const LooseOctreeNode* lpRootNode = mJobData.mpNodes;
        const LooseOctreeNode* lpNode = lpRootNode + lu16NodeIndex;

        int liResult = NodeInsideFrustumVp(lpNode, &mCurrentViewProjection);

        while (liResult != 1)
        {
            if (liResult != 2)
                return liResult;

            const u32 lxNodeEntityFlags = lpNode->mxNodeEntityFlags;
            const int liEntityCount     = static_cast<s32>(lpNode->muNumElements);
            if ((lxNodeEntityFlags & mxCurrentEntityTypeFlags) != 0 && liEntityCount > 0)
            {
                for (u16 lu16Entity = lpNode->muHeadIndex;
                     lu16Entity != 0xFFFF;)
                {
                    SpatialPartitionEntityLink& lrEntityNode = GetEntityNode(lu16Entity);
                    if ((lrEntityNode.mx32TypeFlags & mxCurrentEntityTypeFlags) != 0)
                        PushEntityForTesting(lu16Entity);
                    lu16Entity = lrEntityNode.mu16NextEntity;
                }
            }

            const u16 lu16FirstChild = lpNode->muFirstChildIndex;
            if (lu16FirstChild == 0xFFFF)
                return liResult;

            const LooseOctreeNodeEntityInfo* lpChildInfo = mpNodeEntityInfoArray + lu16FirstChild;
            if ((mxCurrentEntityTypeFlags & lpChildInfo[0].mxSubTreeEntityFlags) != 0)
                liResult = FrustumTestRecursive(lu16FirstChild);
            if ((lpChildInfo[1].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) != 0)
                liResult = FrustumTestRecursive(static_cast<u16>(lu16FirstChild + 1));
            if ((lpChildInfo[2].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) != 0)
                liResult = FrustumTestRecursive(static_cast<u16>(lu16FirstChild + 2));
            if ((lpChildInfo[3].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) == 0)
                return liResult;

            lpNode = lpRootNode + (lu16FirstChild + 3);
            liResult = NodeInsideFrustumVp(lpNode, &mCurrentViewProjection);
        }

        // liResult == 1 (fully inside): hand the subtree to the trivial-accept walk. The X360
        // tail-calls TrivialAcceptRecursive and returns its (pointer) value, which every caller
        // ignores; we run it for its result-buffer side effects and return the fully-inside
        // classification (1), which is behaviourally faithful and type-correct.
        TrivialAcceptRecursive(lpNode);
        return liResult;
    }

    // The two swizzled plane batches in 0x82BDFE70 use VMX multiply, fused
    // multiply-add, subtract and ordered greater-than; any set lane rejects.
    bool FrustumTestJob::SphereInsideCurrentFrustum(const CgsGeometric::Sphere* lpSphere) const
    {
        const __vector4 lvSphere = __lvx(&lpSphere->mData, 0);
        const __vector4 lvX = __vspltw(lvSphere, 0);
        const __vector4 lvY = __vspltw(lvSphere, 1);
        const __vector4 lvZ = __vspltw(lvSphere, 2);
        const __vector4 lvRadius = __vspltw(lvSphere, 3);
        const Vector4* lpPlanes = mCurrentFrustum.maSwizzledPlanes;
        __vector4 lv0 = __vmulfp(__lvx(&lpPlanes[1], 0), lvY);
        __vector4 lv1 = __vmulfp(__lvx(&lpPlanes[5], 0), lvY);
        lv0 = __vmaddfp(__lvx(&lpPlanes[0], 0), lvX, lv0);
        lv1 = __vmaddfp(__lvx(&lpPlanes[4], 0), lvX, lv1);
        lv0 = __vmaddfp(__lvx(&lpPlanes[2], 0), lvZ, lv0);
        lv1 = __vmaddfp(__lvx(&lpPlanes[6], 0), lvZ, lv1);
        lv0 = __vsubfp(lv0, __lvx(&lpPlanes[3], 0));
        lv1 = __vsubfp(lv1, __lvx(&lpPlanes[7], 0));
        const __vector4 lvOutside = __vor(__vcmpgtfp(lv0, lvRadius), __vcmpgtfp(lv1, lvRadius));
        CGS_ALIGN(16) u32 lauOutside[4];
        __stvx(lvOutside, lauOutside, 0);
        return (lauOutside[0] | lauOutside[1] | lauOutside[2] | lauOutside[3]) == 0;
    }

    // CgsSceneManager::FrustumTestJob::TestEntitiesBulk @ 0x82BDFE70
    //
    // Narrow sphere-vs-frustum test over the candidate list gathered by the recursive descent.
    // The sphere comparison uses the original swizzled VMX plane operations.
    // A debug bit (mJobData trailer[2] & 1) suppresses the whole pass.
    void* FrustumTestJob::TestEntitiesBulk()
    {
        if ((mJobData.miDebugMode & 1) == 0) // miDebugMode @ 0x5658
        {
            const CgsGeometric::Sphere* lpSpheres =
                mJobData.mpEntitySpheres; // 0x4E8C
            for (u32 lu = 0; lu < muNumEntitiesToTest; ++lu)
            {
                const u16 lu16Entity = mau16EntitiesToTest[lu];
                const CgsGeometric::Sphere* lpSphere = &lpSpheres[lu16Entity]; // stride 16

                if (SphereInsideCurrentFrustum(lpSphere))
                {
                    JobCoarseResultBuffer* lpBuf = mJobData.mpResultBuffer;
                    lpBuf->mpu16Buffer[lpBuf->muCurrentWriteOffset] = lu16Entity;
                    ++lpBuf->maQueryNumResults[lpBuf->muNumQueries];
                    ++lpBuf->muCurrentWriteOffset;
                }
            }
            muNumEntitiesToTest = 0;
        }
        return this;
    }

    // CgsSceneManager::FrustumTestJob::TrivialAcceptRecursive @ 0x82BDFCD8
    //
    // Reached when a node is fully inside the frustum: every entity in the subtree passes the
    // frustum test, so no per-sphere narrow test runs -- each entity that also passes the type
    // filter is written straight into the current query's result slot. Recurses into every
    // child octant whose subtree flags intersect the mask; the +3 octant is the tail loop.
    void* FrustumTestJob::TrivialAcceptRecursive(const LooseOctreeNode* lpNodeIn)
    {
        void* lpResult = this;
        const LooseOctreeNode* lpRootNode = mJobData.mpNodes;
        const LooseOctreeNode* lpNode = lpNodeIn;

        for (;;)
        {
            const u32 lxNodeEntityFlags = lpNode->mxNodeEntityFlags;
            const int liEntityCount     = static_cast<s32>(lpNode->muNumElements);
            if ((lxNodeEntityFlags & mxCurrentEntityTypeFlags) != 0 && liEntityCount > 0)
            {
                for (u16 lu16Entity = lpNode->muHeadIndex;
                     lu16Entity != 0xFFFF;)
                {
                    SpatialPartitionEntityLink& lrEntityNode = GetEntityNode(lu16Entity);
                    lpResult = &lrEntityNode;
                    if ((lrEntityNode.mx32TypeFlags & mxCurrentEntityTypeFlags) != 0)
                    {
                        JobCoarseResultBuffer* lpBuf = mJobData.mpResultBuffer;
                        lpBuf->mpu16Buffer[lpBuf->muCurrentWriteOffset] = lu16Entity;
                        ++lpBuf->maQueryNumResults[lpBuf->muNumQueries];
                        ++lpBuf->muCurrentWriteOffset;
                    }
                    lu16Entity = lrEntityNode.mu16NextEntity;
                }
            }

            const u16 lu16FirstChild = lpNode->muFirstChildIndex;
            if (lu16FirstChild == 0xFFFF)
                return lpResult;

            const LooseOctreeNodeEntityInfo* lpChildInfo = mpNodeEntityInfoArray + lu16FirstChild;
            if ((lpChildInfo[0].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + lu16FirstChild);
            if ((lpChildInfo[1].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + (lu16FirstChild + 1));
            if ((lpChildInfo[2].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + (lu16FirstChild + 2));
            if ((lpChildInfo[3].mxSubTreeEntityFlags & mxCurrentEntityTypeFlags) == 0)
                return lpResult;

            lpNode = lpRootNode + (lu16FirstChild + 3);
        }
    }
}

// ARTIST 0x82BDF680: the second local-job parameter is the persistent payload.
namespace EA { namespace Thread { void* GetThreadId(); } }
namespace CgsSceneManager
{
    // ARTIST 0x83299F80: six worker contexts, selected by the native job thread ID.
    FrustumTestJob gaFrustumTestJobs[6];
}

void FrustumTestEntry(EA::Jobs::Param, EA::Jobs::Param lData,
                      EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = static_cast<u64>(reinterpret_cast<u32>(EA::Thread::GetThreadId()));
    const u64 luIndex = (luThreadId - 0xF8000088ull) >> 2;
    if (luIndex >= 6)
    {
        CgsDev::StrStream lMessage(byte_83015F20, sizeof(byte_83015F20));
        const u64 luReportedThreadId = static_cast<u64>(reinterpret_cast<u32>(EA::Thread::GetThreadId()));
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
        CgsSceneManager::FrustumJobAssert(
            "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\FrustumTest\\FrustumTest.cpp",
            58, lMessage.GetBuffer());
    }
    CgsSceneManager::gaFrustumTestJobs[static_cast<u32>(luIndex)].Execute(
        *static_cast<const CgsSceneManager::FrustumTestJobData*>(lData.mpValue));
}
