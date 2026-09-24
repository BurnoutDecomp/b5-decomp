#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // CgsModule::VariableEventQueue<BUFSIZE,ALIGN> + AddEvent<EventT>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"        // OutEventLineTestNearestResult (layout-pinned)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_LineTestFineResult.hpp" // OutEventLineTestFineResult + LineTestIntersection (type-1 records)
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"   // CgsGeometric::PolySoupLineNearestResult (DWARF LineTestResult, read by name)

// -------- CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<SizeBytes> --------
//
// OutSceneQueryResultsQueue<N> is VariableEventQueue<N,16> + typed enqueue helpers. The
// AddTriangleCollisionLineTestNearestResult helper @ X360 0x828D1E90 (SizeBytes == 32768)
// builds an OutEventLineTestNearestResult record from a triangle-collision intersection and
// pushes it with event-type id 2.
//
//   a1 = this (queue)                                  (r3 -> r27)
//   a2 = lQueryId          (r4)  -> STORED at rec+0x28 (`stw r4, var_48`)  == mQueryId
//   a3 = lVolumeInstanceId (r6, 64-bit) -> STORED at rec+0x20 (`std r6, var_50`) == mVolumeInstanceId
//   a2b= lEntityId         (r5)  -> STORED at rec+0x2C (`stw r5, var_44`)  == mEntityId
//   a4 = lpIntersection (r7): pointer to a CgsCollision::CollisionResult -- the 112-byte record a
//        poly-soup line test writes (CollisionResultList::GetResult stride 0x70). This asm reads it
//        at +0x30 / +0x40 / +0x50 / +0x60; the record's field-level DWARF is not in the corpus, so
//        those reads stay documented raw offsets (the standing rule for job-output blobs).
//   a5 = lbHasIntersection (r8, clrlwi 24 -> bool). Stored UNCONDITIONALLY into rec+0x38
//        (mbIntersection) before the branch (stb r8, var_38(r1) @ 0x828D1EB4). When true, ALSO
//        copy fields from a4 (after asserting a4 != 0); when false, skip the copy and push the
//        rest of the record with only the three ids + mbIntersection written (the asm does NOT
//        memset/zero the stack image -- mPosition/mNormal/mfLineParam/tags are stack garbage on
//        the miss path).
//
// ⛔ CORRECTED 2026-09-02 (scene-query wave 1). The previous reading of this body said a2 and a3
//    were "spilled and never read back -> no observable effect" and typed them (s32, s64). The
//    three spills at 0x828D1EA0 / 0x828D1EA8 / 0x828D1EB0 land INSIDE the record being built
//    (the record is the sp+0x60 image `addi r4, r1, 0xD0+var_70` that AddEvent copies): var_48
//    == rec+0x28 == mQueryId, var_50 == rec+0x20 == mVolumeInstanceId, var_44 == rec+0x2C ==
//    mEntityId. Dropping them shipped every triangle-collision nearest result with a GARBAGE
//    query id -- the consumer (VehicleManager::ProcessAboveGroundLineTestsResults) decodes the
//    request type and car index out of exactly that word. Four callers pass the ids:
//    ProcessLineTestNearest @0x828D3C90 (queryId, INVALID entity, INVALID volume instance) and
//    ProcessTriangleCollisionLineTestNearests @0x828D4A48 / @0x828D4B20 (queryId, 0, 0).
//
// SOURCE reads (relative to a4):  +0x40 -> mPosition (rec+0x00), +0x30 -> mNormal (rec+0x10),
//   +0x50 -> mfLineParam (rec+0x30), +0x60 (packed u32): high16 -> mu16MaterialTag (rec+0x34),
//   low16 -> mu16GroupTag (rec+0x36). The dest field offsets match the producer-pinned
//   OutEventLineTestNearestResult layout (mbIntersection@+0x38, mfLineParam@+0x30,
//   mu16MaterialTag@+0x34, mu16GroupTag@+0x36; see CgsSceneManagerModuleIO.h).

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    template <s32 SizeBytes>
    class OutSceneQueryResultsQueue : public CgsModule::VariableEventQueue<SizeBytes, 16>
    {
    public:
        // @ X360 0x828D1E90 (SizeBytes == 32768). a2/a3 are accepted (the X360 signature
        // spills them) but have no observable effect in this function body.
        bool AddTriangleCollisionLineTestNearestResult(SceneQueryId     lQueryId,
                                                       EntityId         lEntityId,
                                                       VolumeInstanceId lVolumeInstanceId,
                                                       const void*      lpIntersection,
                                                       bool             lbHasIntersection);

        // @ X360 0x828C4A08 (SizeBytes == 32768). DWARF CgsSceneManagerModuleIO.h:434
        //     LineTestIntersection * AddLineTestFineResult(SceneQueryId, int);
        // (RENAMED 2026-09-24, FX-SCENEMGR: this was spelled AllocateLineTestFineResult(s32, s32)
        // returning void*, a name inferred from the caller while the IDA symbol was truncated.)
        // Reserve a type-1 event of 16 + n * 64 bytes, write the header (query id, count) and
        // return the record area for the caller to fill in place.
        LineTestIntersection* AddLineTestFineResult(SceneQueryId lQueryId, s32 liNumIntersections);

        // @ X360 0x828C4A60 (SizeBytes == 32768). DWARF CgsSceneManagerModuleIO.h:446
        //     LineTestIntersection * AddTriangleCollisionLineTestResult(SceneQueryId, EntityId,
        //                             VolumeInstanceId, const LineTestResult *, int);
        // LineTestResult is `typedef IntersectLinePolygonSoupResult` (CgsCollisionResult.h:35) --
        // the 112-byte record this tree names CgsGeometric::PolySoupLineNearestResult. One type-1
        // event, one 64-byte record per line-test result, every record stamped with the SAME two
        // caller ids. Its one caller is ProcessTriangleCollisionLineTests @0x828C6FB0 (ids 0, 0).
        LineTestIntersection* AddTriangleCollisionLineTestResult(SceneQueryId                                   lQueryId,
                                                                 EntityId                                       lEntityId,
                                                                 VolumeInstanceId                               lVolumeInstanceId,
                                                                 const CgsGeometric::PolySoupLineNearestResult* lpaResults,
                                                                 s32                                            liNumResults);
    };

    template <s32 SizeBytes>
    bool OutSceneQueryResultsQueue<SizeBytes>::AddTriangleCollisionLineTestNearestResult(
        SceneQueryId     lQueryId,
        EntityId         lEntityId,
        VolumeInstanceId lVolumeInstanceId,
        const void*      lpIntersection,
        bool             lbHasIntersection)
    {
        // The asm builds the record on an UNINITIALISED stack image -- no memset.
        OutEventLineTestNearestResult lEvent;

        // The three argument spills ARE the record's id fields (0x828D1EA0 / 0x828D1EA8 / 0x828D1EB0).
        lEvent.mQueryId          = lQueryId;           // rec+0x28 <- r4
        lEvent.mEntityId         = lEntityId;          // rec+0x2C <- r5
        lEvent.mVolumeInstanceId = lVolumeInstanceId;  // rec+0x20 <- r6 (std)

        // stb r8, var_38(r1) at 0x828D1EB4 writes lbHasIntersection into rec+0x38
        // (mbIntersection) UNCONDITIONALLY, before the branch on lbHasIntersection.
        lEvent.mbIntersection = lbHasIntersection;

        if (lbHasIntersection)
        {
            CGS_ASSERT(lpIntersection != nullptr, "Expected intersection result\n");

            const u8* lpSrc = reinterpret_cast<const u8*>(lpIntersection);

            // rec+0x00 <- src+0x40 ; rec+0x10 <- src+0x30 (the two 16-byte SIMD lanes)
            lEvent.mPosition = *reinterpret_cast<const Vector3*>(lpSrc + 0x40);
            lEvent.mNormal   = *reinterpret_cast<const Vector3*>(lpSrc + 0x30);

            // rec+0x30 <- src+0x50
            lEvent.mfLineParam = *reinterpret_cast<const f32*>(lpSrc + 0x50);

            // rec+0x34/0x36 <- split of the packed u32 at src+0x60
            const u32 lxPacked     = *reinterpret_cast<const u32*>(lpSrc + 0x60);
            lEvent.mu16GroupTag    = static_cast<u16>(lxPacked);        // low 16  -> rec+0x36
            lEvent.mu16MaterialTag = static_cast<u16>(lxPacked >> 16);  // high 16 -> rec+0x34
        }

        // Typed convenience overload: liSize == sizeof(OutEventLineTestNearestResult),
        // event-type id == 2.
        return this->template AddEvent<OutEventLineTestNearestResult>(&lEvent, 2);
    }

    // -------- AddLineTestFineResult  @ X360 0x828C4A08 --------
    //   0x828C4A24  slwi r11, n, 6 ; addi r5, r11, 0x10 ; li r4, 1
    //   0x828C4A30  bl VariableEventQueue<32768,16>::AllocateEvent(this, 1, n * 64 + 16)
    //   0x828C4A38  addi r3, ev, 0x10              -- the returned record area
    //   0x828C4A3C  stw id, 0(ev) ; stw n, 4(ev)   -- mQueryId / miNumIntersections
    // mafPad is not written. (ProcessLineTestFine @0x828CDCD0 calls it for its empty answer and
    // inlines the identical four instructions for its non-empty one.)
    template <s32 SizeBytes>
    LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddLineTestFineResult(
        SceneQueryId lQueryId, s32 liNumIntersections)
    {
        OutEventLineTestFineResult* lpEvent = static_cast<OutEventLineTestFineResult*>(
            this->AllocateEvent(1, (liNumIntersections << 6) + static_cast<s32>(sizeof(OutEventLineTestFineResult))));
        lpEvent->mQueryId           = lQueryId;             // stw r30, 0(r11)
        lpEvent->miNumIntersections = liNumIntersections;   // stw r31, 4(r11)
        return lpEvent->GetIntersections();                 // addi r3, r11, 0x10
    }

    // -------- AddTriangleCollisionLineTestResult  @ X360 0x828C4A60 --------
    //   0x828C4A74..8C  AllocateEvent(this, 1, n * 64 + 16) ; stw id, 0(ev) ; stw n, 4(ev)
    //   0x828C4A98      r3 = ev + 0x10 (returned) ; nothing more when n <= 0 (`ble`)
    //   per record i (src r10 = results + 0x30 + 0x70*i, dst r11 = ev + 0x30 + 0x40*i):
    //     0x828C4AC0  stw lEntityId        -> dst.mEntityId         (+0x28)
    //     0x828C4AC8  std lVolumeInstanceId -> dst.mVolumeInstanceId (+0x20)
    //     0x828C4ACC  lvx src+0x40 -> stvx dst+0x00 : mPosition
    //     0x828C4AD8  lvx src+0x30 -> stvx dst+0x10 : mNormal (the triangle normal)
    //     0x828C4AE0  lfs src+0x50 -> stfs dst+0x2C : mfLineParam (lane 0 of the splatted t)
    //     0x828C4AE8  lhz src+0x60 -> sth dst+0x30  : mu16MaterialTag (the tag's HIGH half)
    //     0x828C4AF0  lwz src+0x60 -> sth dst+0x32  : mu16GroupTag    (the tag's LOW half)
    // The two halves are read as a big-endian halfword and the low half of the word; on this host
    // they are the tag's value bits [16..31] and [0..15].
    template <s32 SizeBytes>
    LineTestIntersection* OutSceneQueryResultsQueue<SizeBytes>::AddTriangleCollisionLineTestResult(
        SceneQueryId                                   lQueryId,
        EntityId                                       lEntityId,
        VolumeInstanceId                               lVolumeInstanceId,
        const CgsGeometric::PolySoupLineNearestResult* lpaResults,
        s32                                            liNumResults)
    {
        OutEventLineTestFineResult* lpEvent = static_cast<OutEventLineTestFineResult*>(
            this->AllocateEvent(1, (liNumResults << 6) + static_cast<s32>(sizeof(OutEventLineTestFineResult))));
        lpEvent->mQueryId           = lQueryId;
        lpEvent->miNumIntersections = liNumResults;

        LineTestIntersection* lpaIntersections = lpEvent->GetIntersections();
        for (s32 liResult = 0; liResult < liNumResults; ++liResult)
        {
            const CgsGeometric::PolySoupLineNearestResult& lrSource = lpaResults[liResult];
            LineTestIntersection&                          lrRecord = lpaIntersections[liResult];

            lrRecord.mEntityId         = lEntityId;
            lrRecord.mVolumeInstanceId = lVolumeInstanceId;
            lrRecord.mPosition         = lrSource.mPosition;
            lrRecord.mNormal           = lrSource.mNormal;
            lrRecord.mfLineParam       = lrSource.mLineParam.x;
            lrRecord.mu16MaterialTag   = static_cast<u16>(lrSource.mau32Tag[0] >> 16);
            lrRecord.mu16GroupTag      = static_cast<u16>(lrSource.mau32Tag[0]);
        }
        return lpaIntersections;
    }
}
}
