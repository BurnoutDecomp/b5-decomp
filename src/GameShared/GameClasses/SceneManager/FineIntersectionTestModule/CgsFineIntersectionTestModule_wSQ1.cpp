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
#include "vendor/renderware/collision/VolumeQuery.hpp"              // rw::collision::VolumeVolumeQuery

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

    void FineIntersectionTestModule::ComputeLineTestNearest(const InEventLineTestNearest* /*lpQuery*/,
                                                            OutEventLineTestNearestResult* lpOutResult)
    {
        CGS_ASSERT(false, "FineIntersectionTestModule::ComputeLineTestNearest @0x828C8CC8 is not reconstructed "
                          "(rw::collision::VolumeLineQuery::GetIntersections is a link-stub on this host)");
        // Never a silent "hit" if execution continues past the trap: say so explicitly rather
        // than leaving the caller's stack record as it was.
        lpOutResult->mbIntersection = false;
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
