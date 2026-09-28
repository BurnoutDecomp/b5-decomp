// FineIntersectionTestModule scene-query implementations, split during wave SQ1.
// ARTIST: LineTestFine828C7D70, LineTestNearest828C8CC8,
// VolumeTestDeepest828C90D0, VolumeTestFine828C93C8.
// Construct/Prepare and the vendor query walkers are mounted. LineTestFine now
// implements the primitive-volume arm used by triggers. Its separate ClusteredMesh
// fast path828C804C..828C8A9C and VolumeTestFine retain explicit missing-body traps.

#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"        // EntityId, K_INVALID_ENTITY_ID
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"   // GetEntityIdByIndex / GetFirstEntityVolumeInstance / GetVolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"  // VolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"   // GetVolumeTypeFlags / GetRwVolume
#include "vendor/renderware/collision/CollisionVolume.hpp"          // rw::collision::Volume
#include "vendor/renderware/collision/GPInstance.hpp"               // PrimitivePairIntersectResult
#include "vendor/renderware/collision/LineSegIntersect.hpp"         // VolumeLineSegIntersectResult (the line walk's results)
#include "vendor/renderware/collision/VolumeQuery.hpp"              // rw::collision::VolumeVolumeQuery / VolumeLineQuery

namespace CgsSceneManager
{
    // ARTIST828C7D70: primitive-volume arm828C8AA0..828C8C3C. Results retain
    // internal entity/instance indices; ProcessLineTestFine resolves their IDs.
    void FineIntersectionTestModule::ComputeLineTestFine(const InEventLineTestFine* lpQuery,
                                                         OutEventLineTestFineResult* lpOutResult,
                                                         void* lpResultsOut)
    {
        auto* lpIntersections = static_cast<FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray*>(lpResultsOut);
        u32 luMask = ~0u;
        u32 luExclude = static_cast<u32>(K_INVALID_ENTITY_ID);
        if (lpQuery->mu16ExcludeEntityIndex != 0xffff)
        {
            const EntityId lExcludeId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            luMask = lpQuery->mbExcludeParts ? lExcludeId.GetPartComparisonMask() : ~0u;
            luExclude = luMask & static_cast<u32>(lExcludeId);
        }
        CGS_ASSERT(lpOutResult != 0, "lpOutResult != NULL");
        lpOutResult->mQueryId = lpQuery->mQueryId;
        s32 liCount = 0;
        const LineTestIntersection* lpFirst = 0;
        for (u16 luCandidate = 0; luCandidate < lpQuery->mu16NumEntities; ++luCandidate)
        {
            const u16 luEntityIndex = lpQuery->mpau16EntityIndices[luCandidate];
            if ((static_cast<u32>(mpEntityManager->GetEntityIdByIndex(luEntityIndex)) & luMask) == luExclude)
                continue;
            s32 liInstance = 0;
            const VolumeInstance* lpInstance = mpEntityManager->GetFirstEntityVolumeInstance(luEntityIndex, &liInstance);
            while (lpInstance)
            {
                const s32 liVolume = lpInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolume) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapVolumes[1] = {
                        reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolume)) };
                    const Matrix44Affine* lapTransforms[1] = { &lpInstance->mWorldSpaceTransform };
                    if (rw::collision::gVolumeVTable[lapVolumes[0]->muVTableSlot]->muTypeID == 6)
                    {
                        // Unreconstructed original ClusteredMesh fast path828C804C..828C8A9C.
                        // Trigger boxes take the other arm. Retain the existing explicit trap
                        // for this separate path instead of reporting a successful empty query.
                        CGS_ASSERT(false, "ComputeLineTestFine ClusteredMesh fast path @0x828C804C is not reconstructed");
                    }
                    else
                    {
                        const rw::collision::VolRef::Vec4 lStart = {
                            lpQuery->mLineStart.x, lpQuery->mLineStart.y, lpQuery->mLineStart.z, lpQuery->mLineStart.w };
                        const rw::collision::VolRef::Vec4 lEnd = {
                            lpQuery->mLineEnd.x, lpQuery->mLineEnd.y, lpQuery->mLineEnd.z, lpQuery->mLineEnd.w };
                        mpVolumeLineQuery->InitQuery(lapVolumes, lapTransforms, 1, lStart, lEnd, 0.0f);
                        while (!mpVolumeLineQuery->Finished())
                        {
                            const u32 luCount = mpVolumeLineQuery->GetAllIntersections();
                            const rw::collision::VolumeLineSegIntersectResult* lpHits = mpVolumeLineQuery->m_resBuffer;
                            liCount += luCount;
                            for (u32 luHit = 0; luHit < luCount; ++luHit)
                            {
                                const auto& lrHit = lpHits[luHit];
                                LineTestIntersection lHit;
                                lHit.mPosition.x = lrHit.position.x;
                                lHit.mPosition.y = lrHit.position.y;
                                lHit.mPosition.z = lrHit.position.z;
                                lHit.mPosition.w = lrHit.position.w;
                                lHit.mNormal.x = lrHit.normal.x;
                                lHit.mNormal.y = lrHit.normal.y;
                                lHit.mNormal.z = lrHit.normal.z;
                                lHit.mNormal.w = lrHit.normal.w;
                                lHit.mVolumeInstanceId.muId = static_cast<u64>(static_cast<s64>(liInstance));
                                lHit.mEntityId = EntityId(luEntityIndex);
                                lHit.mfLineParam = lrHit.lineParam;
                                const auto* lpHitVolume = reinterpret_cast<const rw::collision::Volume*>(lrHit.vRef.muVolumePtr);
                                lHit.mu16MaterialTag = lpHitVolume ? static_cast<u16>(lpHitVolume->muSurfaceID) : 0;
                                lHit.mu16GroupTag = lpHitVolume ? static_cast<u16>(lpHitVolume->muGroupID) : 0;
                                lpIntersections->Append(lHit);
                                if (!lpFirst)
                                    lpFirst = &(*lpIntersections)[lpIntersections->GetLength() - 1];
                            }
                        }
                    }
                }
                liInstance = lpInstance->miNextEntityVolumeInstance;
                lpInstance = static_cast<const EntityManager*>(mpEntityManager)->GetVolumeInstance(liInstance);
            }
        }
        lpOutResult->miNumResults = liCount;
        lpOutResult->mpaResults = lpFirst;
    }


    // =========================================================================================
    // ComputeLineTestNearest @ 0x828C8CC8 -- RECONSTRUCTED 2026-09-25 (crash parity FX-FOLLOWUPS stage a); it was a
    // LOUD trap here. Where does the segment first meet the candidate entities' collision volumes? One
    // VolumeLineQuery walk per volume instance whose volume-type flags meet the query's; the smallest line
    // parameter over every result of every batch, instance and entity wins.
    //   r3 = this (r17), r4 = lpQuery (r21), r5 = lpOutResult (r30).
    //   0x828C8CF4  f31 = flt_820F259C (0x7F7FFFFF, FLT_MAX) -- the nearest so far; it is NEVER reset.
    //   0x828C8D10  query+0x2A (exclude index) != 0xFFFF: mask = query+0x2D (mbExcludeParts) ? 0xFFFFFC00 :
    //               0xFFFFFFFF (`subfic 0 ; subfe ; rlwinm 0,31,21 ; addi -1`), exclude = mask & the excluded
    //               entity's id (the inlined GetEntityIdByIndex, the :301 assert, `lwz 0(r3)`);
    //               == 0xFFFF: exclude = dword_82F33F64 (0xFFFFFFFF, K_INVALID_ENTITY_ID), mask = -1.
    //   0x828C8D9C  out+0x3A (mbIntersection) = 0, out+0x36 / +0x38 (the two tags) = 0, out+0x00 = query+0x20
    //               (mQueryId). Nothing else is written unless a result wins.
    //   0x828C8E10  per candidate i < query+0x28 (u16), entity index query+0x24[i]: (mask & id) == exclude -> next
    //               candidate (0x828C8E60 `cmplw ; beq` -- an EQUALITY, unlike ComputeVolumeTestDeepest's superset
    //               test at 0x828C9220).
    //   0x828C8E74    GetFirstEntityVolumeInstance(index, &instance) (0x828C5DC0; the instance index is var_5C,
    //                 r22), then per instance GetVolumeInstance(+0x60) (0x828B9F28, the CONST overload; r22 = that
    //                 index) until null:
    //   0x828C8E90      GetVolumeTypeFlags(+0x5C volume index) & query+0x2C == 0 -> next instance (the inlined
    //                   h:203 / h:204 body)
    //   0x828C8F14      the VolumeLineQuery (this+0x5981C) primed as InitQuery does -- the one input volume
    //                   {GetRwVolume} (0x828C5E68, var_6C), the one input matrix {the instance's transform, +0x00}
    //                   (var_70), numInputs 1, the segment query+0x00 / query+0x10 (four lanes each), fatness
    //                   f29 = flt_82001CC0 (0.0f); m_endClipVal = f30 = flt_82001C98 (1.0f)
    //   0x828C8F90      until Finished(): r29 = -1; n = GetAllIntersections (0x82BB3820); over its n results
    //                   (m_resBuffer re-read, stride 0xD0, compared unsigned) lineParam (+0x40) < nearest (`fcmpu ;
    //                   bge` -- a NaN never wins) -> nearest = lineParam, best = k; best >= 0 ->
    //   0x828C9014        out+0x3A = 1, +0x34 = the entity index, +0x30 = lineParam, +0x10 = position (+0x10),
    //                     +0x04 = the instance index, +0x20 = normal (+0x20); the hit volume (vRef +0x50) non-null:
    //                     +0x36 = its surfaceID (+0x58), +0x38 = its groupID (+0x54) (sth, the low halves); else
    //                     both 0.
    // =========================================================================================
    void FineIntersectionTestModule::ComputeLineTestNearest(const InEventLineTestNearest* lpQuery,
                                                            OutEventLineTestNearestResult* lpOutResult)
    {
        // flt_82001CC0 (0.0f): the fatness InitQuery is handed (f29).
        static const f32 KF_LINE_TEST_NEAREST_FATNESS = 0.0f;
        // The exclude-index sentinel query+0x2A is compared with (`cmplwi cr6, r11, 0xFFFF` @0x828C8D10).
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;
        // flt_820F259C: 0x7F7FFFFF, the largest finite f32 -- the starting "nearest" (f31).
        static const f32 KF_LINE_TEST_NEAREST_START = 3.40282346638528859812e+38f;

        f32 lfNearest = KF_LINE_TEST_NEAREST_START;   // f31

        u32 lx32Mask;      // var_54
        u32 lx32Exclude;   // var_58
        if (lpQuery->mu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX)
        {
            const EntityId lExcludeEntityId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            lx32Mask    = lpQuery->mbExcludeParts ? lExcludeEntityId.GetPartComparisonMask() : ~0u;
            lx32Exclude = lx32Mask & static_cast<u32>(lExcludeEntityId);
        }
        else
        {
            lx32Exclude = static_cast<u32>(K_INVALID_ENTITY_ID);   // dword_82F33F64
            lx32Mask    = ~0u;                                     // li r11, -1
        }

        lpOutResult->mbIntersection  = false;              // stb 0, 0x3A(r30)
        lpOutResult->mu16MaterialTag = 0;                  // sth 0, 0x36(r30)
        lpOutResult->mu16GroupTag    = 0;                  // sth 0, 0x38(r30)
        lpOutResult->mQueryId        = lpQuery->mQueryId;  // lwz 0x20(r21) ; stw 0(r30)

        for (u16 lu16Candidate = 0; lu16Candidate < lpQuery->mu16NumEntities; ++lu16Candidate)
        {
            const u16 lu16EntityIndex = lpQuery->mpau16EntityIndices[lu16Candidate];
            const u32 lx32EntityId    = static_cast<u32>(mpEntityManager->GetEntityIdByIndex(lu16EntityIndex));
            if ((lx32EntityId & lx32Mask) == lx32Exclude)
            {
                continue;
            }

            s32 liVolumeInstance = 0;   // var_5C, then r22
            const VolumeInstance* lpVolumeInstance =
                mpEntityManager->GetFirstEntityVolumeInstance(lu16EntityIndex, &liVolumeInstance);
            while (lpVolumeInstance != 0)
            {
                const s32 liVolumeIndex = lpVolumeInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolumeIndex) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapInputVolumes[1] =                          // var_6C
                        { reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolumeIndex)) };
                    const Matrix44Affine* lapInputMatrices[1] =                                // var_70
                        { &lpVolumeInstance->mWorldSpaceTransform };
                    const rw::collision::VolRef::Vec4 lLineStart =
                        { lpQuery->mLineStart.x, lpQuery->mLineStart.y, lpQuery->mLineStart.z, lpQuery->mLineStart.w };
                    const rw::collision::VolRef::Vec4 lLineEnd =
                        { lpQuery->mLineEnd.x, lpQuery->mLineEnd.y, lpQuery->mLineEnd.z, lpQuery->mLineEnd.w };

                    mpVolumeLineQuery->InitQuery(lapInputVolumes, lapInputMatrices, 1, lLineStart, lLineEnd,
                                                 KF_LINE_TEST_NEAREST_FATNESS);
                    while (!mpVolumeLineQuery->Finished())
                    {
                        s32 liBest = -1;                                                         // r29
                        const u32 luNumResults = mpVolumeLineQuery->GetAllIntersections();
                        const rw::collision::VolumeLineSegIntersectResult* lpaResults =
                            mpVolumeLineQuery->m_resBuffer;                                      // lwz 0x10 once
                        for (u32 luResult = 0; luResult < luNumResults; ++luResult)
                        {
                            if (lpaResults[luResult].lineParam < lfNearest)
                            {
                                liBest    = static_cast<s32>(luResult);
                                lfNearest = lpaResults[luResult].lineParam;
                            }
                        }
                        if (liBest < 0)
                        {
                            continue;
                        }

                        const rw::collision::VolumeLineSegIntersectResult& lrBest = lpaResults[liBest];
                        lpOutResult->mbIntersection        = true;               // stb 1, 0x3A
                        lpOutResult->mu16EntityIndex       = lu16EntityIndex;    // sth r20, 0x34
                        lpOutResult->mfLineParam           = lrBest.lineParam;   // lfs 0x40 ; stfs 0x30
                        lpOutResult->mPosition.x           = lrBest.position.x;  // lvx128 +0x10 ; stvx128 +0x10
                        lpOutResult->mPosition.y           = lrBest.position.y;
                        lpOutResult->mPosition.z           = lrBest.position.z;
                        lpOutResult->mPosition.w           = lrBest.position.w;
                        lpOutResult->muVolumeInstanceIndex = static_cast<u32>(liVolumeInstance);   // stw r22, 4
                        lpOutResult->mNormal.x             = lrBest.normal.x;    // lvx128 +0x20 ; stvx128 +0x20
                        lpOutResult->mNormal.y             = lrBest.normal.y;
                        lpOutResult->mNormal.z             = lrBest.normal.z;
                        lpOutResult->mNormal.w             = lrBest.normal.w;
                        const rw::collision::Volume* lpHitVolume =
                            reinterpret_cast<const rw::collision::Volume*>(lrBest.vRef.muVolumePtr);
                        if (lpHitVolume != 0)
                        {
                            lpOutResult->mu16MaterialTag = static_cast<u16>(lpHitVolume->muSurfaceID);   // +0x58
                            lpOutResult->mu16GroupTag    = static_cast<u16>(lpHitVolume->muGroupID);     // +0x54
                        }
                        else
                        {
                            lpOutResult->mu16MaterialTag = 0;
                            lpOutResult->mu16GroupTag    = 0;
                        }
                    }
                }
                // 0x828C9074: the next instance through the CONST overload (0x828B9F28), its index kept (r22).
                liVolumeInstance = lpVolumeInstance->miNextEntityVolumeInstance;
                lpVolumeInstance = static_cast<const EntityManager*>(mpEntityManager)->GetVolumeInstance(liVolumeInstance);
            }
        }
    }

    // =========================================================================================
    // ComputeVolumeTestDeepest @ 0x828C90D0 -- RECONSTRUCTED 2026-09-25 (crash parity FX-FOLLOWUPS, item 2); it was a
    // LOUD trap here. How deep does the query volume sink into the candidate entities' collision volumes? One
    // VolumeVolumeQuery per volume instance whose volume-type flags meet the query's; the deepest penetration over
    // every contact result of every instance wins.
    //   r3 = this, r4 = lpQuery (r27), r5 = lpOutResult (r24, spilled to sp+0x134).
    //   0x828C90F8  f30 = f31 = flt_82001CC0 (0.0f) -- the deepest so far; it is NOT reset per instance or entity.
    //   0x828C9100  query+0xCA (exclude index) != 0xFFFF:
    //                 mask = query+0xCD (mbExcludeParts) ? 0xFFFFFC00 : 0xFFFFFFFF (`subfic 0 ; subfe ;
    //                 rlwinm 0,31,21 ; addi -1` -- the part-comparison mask or all ones), and
    //                 r14 = mask & the excluded entity's id (the inlined GetEntityIdByIndex, :301 assert)
    //               == 0xFFFF: r14 = dword_82F33F64 (0xFFFFFFFF, K_INVALID_ENTITY_ID)
    //   0x828C9194  out+8 (mbIntersection) = 0, out+0 (mQueryId) = query+0xC0. out+4 (mfDepth) is NOT written here.
    //   0x828C91E0  per candidate i < query+0xC8, entity index query+0xC4[i]:
    //                 id = GetEntityIdByIndex; (r14 & id) == r14 -> next candidate (0x828C9220: a bit-SUPERSET test
    //                 against the masked exclude id, as compiled -- not an equality)
    //   0x828C9238    GetFirstEntityVolumeInstance(index, &first) (0x828C5DC0), then GetVolumeInstance(+0x60)
    //                 (0x828B9F28) until null; per instance:
    //   0x828C924C      GetVolumeTypeFlags(+0x5C volume index) & query+0xCC == 0 -> next instance (the inlined
    //                   h:203 / h:204 body)
    //   0x828C92D8      prime the VolumeVolumeQuery (this+0x59818): +0x14 m_padding = f30 (0.0f), +0x00 m_inputVols
    //                   = {GetRwVolume}, +0x04 m_inputMats = {the instance's transform (+0x00)}, +0x08 m_numInputs =
    //                   1, +0x0C m_currInput = 0, +0x1C m_volRefPairCount = 0, +0x38 m_queryVol = query+0x40,
    //                   +0x3C m_queryMtx = query+0x00, +0x10 m_cullTable = 0; GetPrimitiveIntersections (0x82BB3FF0)
    //   0x828C9340      over the results (m_intersectionBuffer, stride 0x750, count compared unsigned):
    //                   -distance (+0x4F0) > deepest (`fneg ; fcmpu ; ble` -- a NaN never wins) and numPoints
    //                   (+0x740) != 0 (`cmplwi ; ble`) -> deepest = -distance, best = i
    //   0x828C9374      best >= 0 -> out+4 = deepest, out+8 = 1
    // =========================================================================================
    void FineIntersectionTestModule::ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* lpQuery,
                                                              OutEventVolumeTestDeepestResult* lpOutResult)
    {
        // flt_82001CC0 (0.0f): the starting "deepest" (only a penetration, -distance > 0, can beat it) and the
        // query padding -- one register, f30, feeds both.
        static const f32 KF_VOLUME_TEST_DEEPEST_ZERO = 0.0f;
        // The exclude-index sentinel query+0xCA is compared with (`cmplwi r11, 0xFFFF` @0x828C9120).
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;

        f32 lfDeepest = KF_VOLUME_TEST_DEEPEST_ZERO;   // f31

        u32 lx32Exclude;                               // r14
        if (lpQuery->mu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX)
        {
            const EntityId lExcludeEntityId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            const u32      lx32Mask         = lpQuery->mbExcludeParts ? lExcludeEntityId.GetPartComparisonMask()
                                                                      : ~0u;
            lx32Exclude = lx32Mask & static_cast<u32>(lExcludeEntityId);
        }
        else
        {
            lx32Exclude = static_cast<u32>(K_INVALID_ENTITY_ID);   // dword_82F33F64
        }

        lpOutResult->mbIntersection = false;              // stb 0, 8(r24)
        lpOutResult->mQueryId       = lpQuery->mQueryId;  // lwz 0xC0(r27) ; stw 0(r24)

        for (u16 lu16Candidate = 0; lu16Candidate < lpQuery->mu16NumEntities; ++lu16Candidate)
        {
            const u16 lu16EntityIndex = lpQuery->mpau16EntityIndices[lu16Candidate];
            const u32 lx32EntityId    = static_cast<u32>(mpEntityManager->GetEntityIdByIndex(lu16EntityIndex));
            if ((lx32Exclude & lx32EntityId) == lx32Exclude)
            {
                continue;
            }

            s32 liFirstVolumeInstance = 0;   // var_B0: written by the call, never read
            const VolumeInstance* lpVolumeInstance =
                mpEntityManager->GetFirstEntityVolumeInstance(lu16EntityIndex, &liFirstVolumeInstance);
            while (lpVolumeInstance != 0)
            {
                const s32 liVolumeIndex = lpVolumeInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolumeIndex) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapInputVolumes[1] =                          // var_BC
                        { reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolumeIndex)) };
                    const Matrix44Affine* lapInputMatrices[1] =                                // var_B8
                        { &lpVolumeInstance->mWorldSpaceTransform };

                    rw::collision::VolumeVolumeQuery* lpQueryObject = mpVolumeVolumeQuery;
                    lpQueryObject->m_padding         = KF_VOLUME_TEST_DEEPEST_ZERO;          // +0x14
                    lpQueryObject->m_inputVols       = lapInputVolumes;                      // +0x00
                    lpQueryObject->m_inputMats       = lapInputMatrices;                     // +0x04
                    lpQueryObject->m_numInputs       = 1;                                    // +0x08
                    lpQueryObject->m_currInput       = 0;                                    // +0x0C
                    lpQueryObject->m_volRefPairCount = 0;                                    // +0x1C
                    lpQueryObject->m_queryVol        =                                       // +0x38
                        reinterpret_cast<const rw::collision::Volume*>(&lpQuery->mVolumeBuffer);
                    lpQueryObject->m_queryMtx        = &lpQuery->mTransform;                 // +0x3C
                    lpQueryObject->m_cullTable       = 0;                                    // +0x10

                    const u32 luNumResults = static_cast<u32>(mpVolumeVolumeQuery->GetPrimitiveIntersections());
                    const rw::collision::PrimitivePairIntersectResult* lpaResults =
                        mpVolumeVolumeQuery->m_intersectionBuffer;                           // lwz 0x30 once
                    s32 liBest = -1;                                                         // r30
                    for (u32 luResult = 0; luResult < luNumResults; ++luResult)
                    {
                        const f32 lfDepth = -lpaResults[luResult].distance;                  // fneg
                        if (lfDepth > lfDeepest && lpaResults[luResult].numPoints != 0)
                        {
                            liBest    = static_cast<s32>(luResult);
                            lfDeepest = lfDepth;
                        }
                    }
                    if (liBest >= 0)
                    {
                        lpOutResult->mfDepth        = lfDeepest;   // stfs f31, 4
                        lpOutResult->mbIntersection = true;        // stb 1, 8
                    }
                }
                // 0x828C9390: the CONST overload (0x828B9F28), as GetFirstEntityVolumeInstance itself uses.
                lpVolumeInstance = static_cast<const EntityManager*>(mpEntityManager)
                                       ->GetVolumeInstance(lpVolumeInstance->miNextEntityVolumeInstance);
            }
        }
    }

    void FineIntersectionTestModule::ComputeVolumeTestFine(const InEventVolumeTestFine* /*lpQuery*/,
                                                           OutEventVolumeTestFineResult* /*lpOutResult*/,
                                                           void* /*lpEntityBuffer*/)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeVolumeTestFine @0x828C93C8 is not reconstructed "
                          "(rw::collision::VolumeVolumeQuery is unproven on this host)");
    }
}
