#pragma once

// CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult -- the scene manager's fine-line-test
// RESULT event (type id 1 on the shared scene-result queue, VariableEventQueue<32768,16>).
//
// LAYOUT -- DWARF CgsSceneManagerModuleIO.h:217..227 (CORRECTED 2026-09-24, crash parity FX-SCENEMGR):
//   +0x00  SceneQueryId mQueryId             (:219)
//   +0x04  int32_t      miNumIntersections   (:220)
//   +0x08  float32_t    mafPad[2]            (:222)
//   +0x10  LineTestIntersection[miNumIntersections]   -- GetIntersections() (:227), 64-byte stride
// Its producers write exactly that: OutSceneQueryResultsQueue::AddLineTestFineResult @0x828C4A08
// (`stw id, 0(ev) ; stw n, 4(ev) ; addi r3, ev, 0x10`, event size n * 64 + 16),
// AddTriangleCollisionLineTestResult @0x828C4A60 and SceneManagerModule::ProcessLineTestFine
// @0x828CDCD0 (records at event + 0x10, EntityId at record + 0x28).
//
// ⛔ WHAT THIS FILE USED TO SAY: records at +0x38 with the EntityId at the record's +0, "from
// TriggerEntityModule::ProcessLineTestFineResult @0x822D9FF8 alone". That consumer reads word
// 14 (+0x38) and strides 64 -- which is record 0's mEntityId (+0x10 + 0x28), NOT a record base.
// Both models address the same bytes for that one read, so the trigger consumer's behaviour is
// unchanged by this correction; PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 (records from
// word 4, stride 16 words) is the reader that pins the base at +0x10.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"      // CgsSceneManager::SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"  // CgsSceneManager::LineTestIntersection

#include <cstdint>   // uintptr_t (the record area follows the header in the event's byte image)

namespace CgsSceneManager
{
    // CgsSceneManagerTypes.h:67 -- the 64-byte record (two Vector3 lanes force the 16-byte padding).
    static_assert(sizeof(LineTestIntersection) == 0x40, "LineTestIntersection is the 64-byte record");

namespace SceneManagerIO
{
    // Empty per-module event base (queue stores events by byte image). Distinctly named to avoid
    // ODR clash with the other SceneManagerIO leaf-element Event bases.
    struct EventBaseLineTestFineResult {};

    struct OutEventLineTestFineResult : public EventBaseLineTestFineResult
    {
        SceneQueryId mQueryId;             // +0x00  (:219)
        s32          miNumIntersections;   // +0x04  (:220)
        f32          mafPad[2];            // +0x08  (:222) -- never written by any producer

        // :227 -- the records follow the 16-byte header in the queue's own byte buffer (the
        // sanctioned external-byte-stream case: a variable-length event, not a C++ array member).
        LineTestIntersection* GetIntersections() const
        {
            return reinterpret_cast<LineTestIntersection*>(reinterpret_cast<uintptr_t>(this) + sizeof(*this));
        }

        // Tree conveniences over the two header words and one record (the trigger consumer's
        // spelling; not in the DWARF).
        s32          GetNumIntersections() const { return miNumIntersections; }
        SceneQueryId GetQueryId() const          { return mQueryId; }
        const LineTestIntersection& GetIntersection(s32 liIndex) const { return GetIntersections()[liIndex]; }
    };
    static_assert(sizeof(OutEventLineTestFineResult) == 0x10, "the header is 16 bytes; records start at +0x10");
}
}
