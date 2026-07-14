#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // CgsModule::VariableEventQueue<BUFSIZE,ALIGN> + AddEvent<EventT>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"        // OutEventLineTestNearestResult (layout-pinned)

// -------- CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<SizeBytes> --------
//
// OutSceneQueryResultsQueue<N> is VariableEventQueue<N,16> + typed enqueue helpers. The
// AddTriangleCollisionLineTestNearestResult helper @ X360 0x828D1E90 (SizeBytes == 32768)
// builds an OutEventLineTestNearestResult record from a triangle-collision intersection and
// pushes it with event-type id 2.
//
//   a1 = this (queue)                                  (r3 -> r27)
//   a2 = (s32) spilled to var_48; never read back      -> no observable effect
//   a3 = (s64) spilled to var_50; never read back      -> no observable effect
//   a4 = lpIntersection: pointer to a triangle-collision intersection record. This is NOT an
//        OutEventLineTestNearestResult (that type is only 0x40 bytes; this asm reads it at +0x60).
//        It is a larger narrow-phase intersection record whose concrete type is not yet
//        identified in the corpus -- modelled here as an opaque source read via raw byte offsets.
//   a5 = lbHasIntersection (r8, clrlwi 24 -> bool). Stored UNCONDITIONALLY into rec+0x38
//        (mbIntersection) before the branch (stb r8, var_38(r1) @ 0x828D1EB4). When true, ALSO
//        copy fields from a4 (after asserting a4 != 0); when false, skip the copy and push the
//        rest of the record UNINITIALISED (the asm does NOT memset/zero the stack image --
//        mbIntersection plus, on the true path, the four copied fields are the only bytes
//        written; on the false path only mbIntersection is written, the rest is garbage).
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
        bool AddTriangleCollisionLineTestNearestResult(s32 /*lUnused2*/,
                                                       s64 /*lUnused3*/,
                                                       const void* lpIntersection,
                                                       bool lbHasIntersection);

        // @ X360 0x828C4A08 (SizeBytes == 32768). Reserve a variable-length RESULT event sized
        // for a 16-byte header + liNumIntersections 64-byte intersection records, write the two
        // header words (query id, intersection count) and return a pointer to the record area for
        // the caller (SceneManagerModule::ProcessLineTestFine) to fill the records in place. Uses
        // event-type id 1 (the sibling AddTriangleCollisionLineTestNearestResult uses id 2). NAME
        // is inferred from the caller (ProcessLineTestFine) + the type-1 tag + the 64-byte record
        // stride; the X360 IDA symbol is truncated ("...OutSceneQueryResultsQueu"). Returns the
        // record-area pointer (void*), matching the X360 `return Event + 4` (past the 16-byte header).
        void* AllocateLineTestFineResult(s32 liQueryId, s32 liNumIntersections);
    };

    template <s32 SizeBytes>
    bool OutSceneQueryResultsQueue<SizeBytes>::AddTriangleCollisionLineTestNearestResult(
        s32 /*lUnused2*/,
        s64 /*lUnused3*/,
        const void* lpIntersection,
        bool lbHasIntersection)
    {
        // The asm builds the record on an UNINITIALISED stack image -- no memset.
        OutEventLineTestNearestResult lEvent;

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

    // -------- AllocateLineTestFineResult  @ X360 0x828C4A08 --------
    // Thin wrapper over the base VariableEventQueue<SizeBytes,16>::AllocateEvent:
    //   Event = AllocateEvent(1, (liNumIntersections << 6) + 16);   // 16B header + N*64B records
    //   Event[0] = liQueryId;  Event[1] = liNumIntersections;       // *Event = a2 ; Event[1] = a3
    //   return Event + 4;                                           // record area, past 16B header
    // The X360 stores the two header words through a _DWORD view of the returned payload and
    // returns the payload advanced by 4 words (16 bytes). Modelled with the same s32 header view.
    template <s32 SizeBytes>
    void* OutSceneQueryResultsQueue<SizeBytes>::AllocateLineTestFineResult(
        s32 liQueryId, s32 liNumIntersections)
    {
        const s32 liSize = (liNumIntersections << 6) + 16; // 16-byte header + N * 64-byte records
        s32* lpPayload = reinterpret_cast<s32*>(this->AllocateEvent(1, liSize));
        lpPayload[0] = liQueryId;          // *Event = a2
        lpPayload[1] = liNumIntersections; // Event[1] = a3
        return lpPayload + 4;              // Event + 4 (dwords) == the 64-byte-record area
    }
}
}
