#include "GameShared/Jobs/FrustumTest/FrustumTest.h"

#include <cstddef>  // offsetof
#include <cstring>  // std::memcpy (models the Xbox XMemCpy / VMX block copies)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere

// Byte-offset pins: the operator= asm (0x82BDF950) attests these exact region boundaries.
namespace
{
    static_assert(sizeof(CgsGeometric::Frustum) == 0x80, "Frustum must be 128 bytes");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, maViewProjection) == 0x500,
                  "maViewProjection must be at +0x500");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, max32EntityTypeFlags) == 0x780,
                  "max32EntityTypeFlags must be at +0x780");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, muNumQueries) == 0x7A8,
                  "muNumQueries must be at +0x7A8");

    // FrustumTestJobData pins (operator= @0x82BDFC48 attests these exact region boundaries).
    static_assert(offsetof(CgsSceneManager::FrustumTestJobData, mQueryInfo) == 0x20,
                  "mQueryInfo must be at +0x20");
    static_assert(offsetof(CgsSceneManager::FrustumTestJobData, mau32Trailer) == 0x7D0,
                  "mau32Trailer must be at +0x7D0");
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
    // Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 body copies, in
    // order and store-for-store:
    //   +0x00..+0x14  six lwz/stw word copies                     -> mau32Header[0..5]
    //   +0x20         bl FrustumJobQueryInfo::operator=(this+0x20) -> mQueryInfo (delegated)
    //   +0x7D0..+0x7D8 three lwz/stw word copies                  -> mau32Trailer[0..2]
    // then `mr r3, r31` / blr (return *this). The +0x18 alignment gap is NOT touched.
    FrustumTestJobData& FrustumTestJobData::operator=(const FrustumTestJobData& lrSource)
    {
        // +0x00..+0x14: the six job-control header words (6 lwz/stw).
        mau32Header[0] = lrSource.mau32Header[0];
        mau32Header[1] = lrSource.mau32Header[1];
        mau32Header[2] = lrSource.mau32Header[2];
        mau32Header[3] = lrSource.mau32Header[3];
        mau32Header[4] = lrSource.mau32Header[4];
        mau32Header[5] = lrSource.mau32Header[5];
        // +0x20: the embedded per-frame query batch (delegated to its own operator=).
        mQueryInfo = lrSource.mQueryInfo;
        // +0x7D0..+0x7D8: the three trailing result/state words (3 lwz/stw).
        mau32Trailer[0] = lrSource.mau32Trailer[0];
        mau32Trailer[1] = lrSource.mau32Trailer[1];
        mau32Trailer[2] = lrSource.mau32Trailer[2];
        return *this;
    }

    // ------------------------------------------------------------------------
    // File-local entity-node accessor (X360 sub_82BDF7B8): resolves an entity index to its
    // entity-node record, whose word[0] is the entity type-flags and whose u16 at byte +4 is
    // the intrusive entity-list "next" index. Its own reconstruction is a sibling ledger
    // function; declared-not-defined here (the compile gate does not link).
    // ------------------------------------------------------------------------
    u32* FrustumTestJob_EntityNode(FrustumTestJob* lpJob, u16 lu16Entity);

    // CgsSceneManager::FrustumTestJob::Execute @ 0x82BE0158
    //
    // Runs every live query in mJobData.mQueryInfo. For each query: snapshot the VP matrix +
    // swizzled frustum + entity-type mask into the working slots, reset the candidate list,
    // open a result-buffer slot, descend the octree, run the narrow sphere test, then advance
    // the buffer to the next query slot. The X360 caches mpRootNodeInfo(header[1]) and
    // mpEntityArray(header[2]) into +0x5680/+0x5684 once before the loop.
    void* FrustumTestJob::Execute()
    {
        // X360 FrustumTestJobData::operator=(&mJobData) @0x82BE016C -- the job snapshots its
        // own payload (self-assignment; the operator copies the live regions in place).
        mJobData = mJobData;  // NOLINT: models the in-place FrustumTestJobData::operator= call

        mpNodeEntityInfoArray = reinterpret_cast<void*>(mJobData.mau32Header[1]); // 0x4E84 -> 0x5680
        mpEntityArray         = reinterpret_cast<void*>(mJobData.mau32Header[2]); // 0x4E88 -> 0x5684

        for (u32 luQuery = 0; luQuery < mJobData.mQueryInfo.muNumQueries; ++luQuery)
        {
            std::memcpy(&mCurrentViewProjection, &mJobData.mQueryInfo.maViewProjection[luQuery],
                        sizeof(Matrix44));
            std::memcpy(macCurrentFrustum, &mJobData.mQueryInfo.maFrustum[luQuery], 0x80);

            muNumEntitiesToTest = 0;
            mxCurrentEntityTypeFlags = mJobData.mQueryInfo.max32EntityTypeFlags[luQuery];

            CGS_ASSERT(KU_MAX_OCTREE_NODES >= mJobData.mau32Header[4],
                       "KU_MAX_OCTREE_NODES >= mFrustumTestJobData.muNumNodes");

            u32* lpResultBuffer = reinterpret_cast<u32*>(mJobData.mau32Trailer[0]);
            lpResultBuffer[lpResultBuffer[0] + 2] = lpResultBuffer[1]; // (numQueries+2)*4

            FrustumTestRecursive(0);
            TestEntitiesBulk();

            lpResultBuffer = reinterpret_cast<u32*>(mJobData.mau32Trailer[0]);
            CGS_ASSERT(lpResultBuffer[0] < 0x10u,
                       "muNumQueries < KU_JOB_BUFFER_MAX_NUM_QUERIES");

            ++lpResultBuffer[0]; // ++muNumQueries
        }

        return reinterpret_cast<void*>(mJobData.mau32Trailer[0]);
    }

    // CgsSceneManager::FrustumTestJob::FrustumTestRecursive @ 0x82BDFFC8
    //
    // Descends the loose octree for the current query. NodeInsideFrustumVp -> 1 (fully inside)
    // hands the whole subtree to TrivialAcceptRecursive; -> 2 (partial) tests this node's own
    // entities then recurses into any child octant whose subtree flags intersect the mask;
    // -> 0 prunes. The +3 octant is folded into the outer loop.
    int FrustumTestJob::FrustumTestRecursive(u16 lu16NodeIndex)
    {
        const u8* lpRootNode = reinterpret_cast<const u8*>(mJobData.mau32Header[0]);
        const u8* lpNode = lpRootNode + 96 * lu16NodeIndex;

        int liResult = NodeInsideFrustumVp(lpNode, &mCurrentViewProjection);

        while (liResult != 1)
        {
            if (liResult != 2)
                return liResult;

            const u32 lxNodeEntityFlags = *reinterpret_cast<const u32*>(lpNode + 0x54);
            const int liEntityCount     = *reinterpret_cast<const int*>(lpNode + 0x44);
            if ((lxNodeEntityFlags & mxCurrentEntityTypeFlags) != 0 && liEntityCount > 0)
            {
                for (u16 lu16Entity = *reinterpret_cast<const u16*>(lpNode + 0x4C);
                     lu16Entity != 0xFFFF;)
                {
                    u32* lpEntityNode = FrustumTestJob_EntityNode(this, lu16Entity);
                    if ((lpEntityNode[0] & mxCurrentEntityTypeFlags) != 0)
                        PushEntityForTesting(lu16Entity);
                    lu16Entity = *reinterpret_cast<const u16*>(
                        reinterpret_cast<const u8*>(lpEntityNode) + 4);
                }
            }

            const u16 lu16FirstChild = *reinterpret_cast<const u16*>(lpNode + 0x42);
            if (lu16FirstChild == 0xFFFF)
                return liResult;

            const u32* lpChildInfo =
                reinterpret_cast<const u32*>(mpNodeEntityInfoArray) + lu16FirstChild;
            if ((mxCurrentEntityTypeFlags & lpChildInfo[0]) != 0)
                liResult = FrustumTestRecursive(lu16FirstChild);
            if ((lpChildInfo[1] & mxCurrentEntityTypeFlags) != 0)
                liResult = FrustumTestRecursive(static_cast<u16>(lu16FirstChild + 1));
            if ((lpChildInfo[2] & mxCurrentEntityTypeFlags) != 0)
                liResult = FrustumTestRecursive(static_cast<u16>(lu16FirstChild + 2));
            if ((lpChildInfo[3] & mxCurrentEntityTypeFlags) == 0)
                return liResult;

            lpNode = lpRootNode + 96 * (lu16FirstChild + 3);
            liResult = NodeInsideFrustumVp(lpNode, &mCurrentViewProjection);
        }

        // liResult == 1 (fully inside): hand the subtree to the trivial-accept walk. The X360
        // tail-calls TrivialAcceptRecursive and returns its (pointer) value, which every caller
        // ignores; we run it for its result-buffer side effects and return the fully-inside
        // classification (1), which is behaviourally faithful and type-correct.
        TrivialAcceptRecursive(lpNode);
        return liResult;
    }

    // CgsSceneManager::FrustumTestJob::TestEntitiesBulk @ 0x82BDFE70
    //
    // Narrow sphere-vs-frustum test over the candidate list gathered by the recursive descent.
    // The VMX plane math is reproduced by the portable SphereInsideCurrentFrustum() stand-in
    // (semantic parity, NOT byte-exact -- FLAGGED). The result-buffer bookkeeping is byte-exact.
    // A debug bit (mJobData trailer[2] & 1) suppresses the whole pass.
    void* FrustumTestJob::TestEntitiesBulk()
    {
        if ((mJobData.mau32Trailer[2] & 1) == 0) // miDebugMode @ 0x5658
        {
            const CgsGeometric::Sphere* lpSpheres =
                reinterpret_cast<const CgsGeometric::Sphere*>(mJobData.mau32Header[3]); // 0x4E8C
            for (u32 lu = 0; lu < muNumEntitiesToTest; ++lu)
            {
                const u16 lu16Entity = mau16EntitiesToTest[lu];
                const CgsGeometric::Sphere* lpSphere = &lpSpheres[lu16Entity]; // stride 16

                if (SphereInsideCurrentFrustum(lpSphere))
                {
                    u32* lpBuf = reinterpret_cast<u32*>(mJobData.mau32Trailer[0]); // 0x5650
                    u16* lpu16Buffer = reinterpret_cast<u16*>(lpBuf[34]);          // +0x88
                    lpu16Buffer[lpBuf[1]] = lu16Entity;                            // [muCurrentWriteOffset]
                    ++lpBuf[lpBuf[0] + 18];                                        // maQueryNumResults[muNumQueries]
                    ++lpBuf[1];                                                    // ++muCurrentWriteOffset
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
    void* FrustumTestJob::TrivialAcceptRecursive(const u8* lpNodeIn)
    {
        void* lpResult = this;
        const u8* lpRootNode = reinterpret_cast<const u8*>(mJobData.mau32Header[0]);
        const u8* lpNode = lpNodeIn;

        for (;;)
        {
            const u32 lxNodeEntityFlags = *reinterpret_cast<const u32*>(lpNode + 0x54);
            const int liEntityCount     = *reinterpret_cast<const int*>(lpNode + 0x44);
            if ((lxNodeEntityFlags & mxCurrentEntityTypeFlags) != 0 && liEntityCount > 0)
            {
                for (u16 lu16Entity = *reinterpret_cast<const u16*>(lpNode + 0x4C);
                     lu16Entity != 0xFFFF;)
                {
                    u32* lpEntityNode = FrustumTestJob_EntityNode(this, lu16Entity);
                    lpResult = lpEntityNode;
                    if ((lpEntityNode[0] & mxCurrentEntityTypeFlags) != 0)
                    {
                        u32* lpBuf = reinterpret_cast<u32*>(mJobData.mau32Trailer[0]);
                        u16* lpu16Buffer = reinterpret_cast<u16*>(lpBuf[34]); // +0x88
                        lpu16Buffer[lpBuf[1]] = lu16Entity;
                        ++lpBuf[lpBuf[0] + 18];
                        ++lpBuf[1];
                    }
                    lu16Entity = *reinterpret_cast<const u16*>(
                        reinterpret_cast<const u8*>(lpEntityNode) + 4);
                }
            }

            const u16 lu16FirstChild = *reinterpret_cast<const u16*>(lpNode + 0x42);
            if (lu16FirstChild == 0xFFFF)
                return lpResult;

            const u32* lpChildInfo =
                reinterpret_cast<const u32*>(mpNodeEntityInfoArray) + lu16FirstChild;
            if ((lpChildInfo[0] & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + 96 * lu16FirstChild);
            if ((lpChildInfo[1] & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + 96 * (lu16FirstChild + 1));
            if ((lpChildInfo[2] & mxCurrentEntityTypeFlags) != 0)
                lpResult = TrivialAcceptRecursive(lpRootNode + 96 * (lu16FirstChild + 2));
            if ((lpChildInfo[3] & mxCurrentEntityTypeFlags) == 0)
                return lpResult;

            lpNode = lpRootNode + 96 * (lu16FirstChild + 3);
        }
    }
}
