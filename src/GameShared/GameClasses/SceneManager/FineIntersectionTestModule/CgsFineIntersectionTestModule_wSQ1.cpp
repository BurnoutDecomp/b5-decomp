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
// UPDATE 2026-09-25 (crash parity FX-FOLLOWUPS, item 2): CgsFineIntersectionTestModule.cpp IS mounted now, so the
// module's Construct / Prepare are the console's -- both query objects are built in the module's buffers and the
// two manager pointers are set. Reason (b) below is gone. Reason (a) stands for the LINE queries: the vendor
// VolumeLineQuery has its construction entry points (b5 26802ef3) but no line walk (GetIntersections,
// GetAllIntersections @0x82BB3820, AddVolumeRef, AddPrimitiveRef, InitQuery), so ComputeLineTestFine /
// ComputeLineTestNearest stay LOUD traps.
// UPDATE 2026-09-25 (crash parity FX-FOLLOWUPS stage a): the line walk is bodied (VolumeQuery.cpp, InitQuery /
// Finished in VolumeQuery.hpp) and ComputeLineTestNearest is RECONSTRUCTED below. The walk's own gaps -- the
// primitive lineSegIntersect slots and aggregate LineIntersectionQuery bodies this host lacks -- are LOUD traps
// inside the walk. ComputeLineTestFine and ComputeVolumeTestFine stay traps.
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
    void FineIntersectionTestModule::ComputeLineTestFine(const InEventLineTestFine* lpQuery,
                                                         OutEventLineTestFineResult* lpOutResult,
                                                         void* /*lpResultsOut*/)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeLineTestFine @0x828C7D70 is not reconstructed "
                          "(rw::collision::VolumeLineQuery::GetIntersections is a link-stub on this host)");
        // Never a silent hit if execution continues past the trap (the ComputeLineTestNearest
        // precedent below): its caller, SceneManagerModule::ProcessLineTestFine @0x828CDF4C, reads
        // miNumResults / mpaResults straight back, so they are stated rather than left as whatever
        // the caller's stack record held. (2026-09-24, FX-SCENEMGR, with that caller's body.)
        lpOutResult->mQueryId     = lpQuery->mQueryId;
        lpOutResult->miNumResults = 0;
        lpOutResult->mpaResults   = 0;
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
