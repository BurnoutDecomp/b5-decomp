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
//   a5 = lbHasIntersection (r8, clrlwi 24 -> bool). When true, copy fields from a4 (after
//        asserting a4 != 0); when false, skip the copy and push the record UNINITIALISED (the
//        asm does NOT memset/zero the stack image -- only the four copied fields are written on
//        the true path; on the false path NOTHING is written, so a garbage record is queued).
//
// SOURCE reads (relative to a4):  +0x40 -> mPosition (rec+0x00), +0x30 -> mNormal (rec+0x10),
//   +0x50 -> mfLineParam (rec+0x30), +0x60 (packed u32): high16 -> mu16MaterialTag (rec+0x34),
//   low16 -> mu16GroupTag (rec+0x36). The dest field offsets match the producer-pinned
//   OutEventLineTestNearestResult layout (mbIntersection@+0x2C, mfLineParam@+0x30,
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
}
}
