#pragma once

// Minimal owning home for the scene-manager fine-line-test RESULT output event:
//   CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult
//   CgsSceneManager::SceneManagerIO::LineTestFineIntersection (its per-hit record)
//
// This is the event the scene manager publishes into the shared scene-result queue
// (VariableEventQueue<32768,16>) for each fine line test it ran. BrnWorld::TriggerEntityModule::
// ProcessLineTestFineResult (X360 0x822D9FF8) consumes it: it reads the query id and the
// intersection count, validates every hit is a trigger entity, then republishes the overlapped
// triggers' handles.
//
// LAYOUT (X360-authoritative, from ProcessLineTestFineResult 0x822D9FF8):
//   +0x00  SceneQueryId mQueryId         (result[0])   -- the originating query id
//   +0x04  s32          miNumIntersections (result[1]) -- number of hit records that follow
//   +0x08..+0x37 header tail (the asm walks the records starting at word index 14 == +0x38)
//   +0x38  LineTestFineIntersection maIntersections[]  -- 64-byte stride records
//
// Each LineTestFineIntersection record is 64 bytes (the asm strides `v10 += 64`); the only field
// this consumer reads is the leading EntityId at the record's +0 (the asm reads the owner byte
// then the packed entity index from the same word). FLAG: the remaining 60 bytes of the
// intersection record (position / normal / line param / material+group tags / volume-instance id,
// per the sibling OutEventLineTestNearestResult) are not modelled here -- only EntityId@+0 is
// load-bearing for the trigger module. Promote to the full record when the line-test-result TU
// lands.
//
// NOTE (EntityId split): this X360 build's ProcessLineTestFineResult extracts the entity index
// as `(id >> 10) & 0x3FFF` (a 14-bit field at bit 10 -- the DecFIGS DWARF EntityId split), whereas
// the committed CgsEntityId.h uses the asm-authoritative 8/12/12 split. The trigger consumer is
// nonetheless correct because the PRODUCER (TriggerEntityModule::ProcessAddTriggerEvents, via
// EntityId::Set) and the CONSUMER (here, via EntityId::GetEntityIndex) both use the committed
// 8/12/12 convention, so the slot round-trips self-consistently regardless of the X360 build's
// 14/10 bit positions. The committed-accessor approach is kept for ODR consistency; the bit-split
// difference is recorded, not relied upon.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"       // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"   // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (queue stores events by byte image). Distinctly named to avoid
    // ODR clash with the other SceneManagerIO leaf-element Event bases.
    struct EventBaseLineTestFineResult {};

    // One hit record of a fine line test. 64-byte stride (X360-attested). Only mEntityId is
    // load-bearing for the trigger consumer; the trailing payload is opaque (see FLAG above).
    struct LineTestFineIntersection
    {
        EntityId mEntityId;      // +0x00  hit entity (owner + packed index)
        u8       maOpaque[60];   // +0x04..+0x3F  position/normal/tags/etc. (not modelled)
    };

    // The fine-line-test result output event. alignas not required (no SIMD field is read by the
    // trigger consumer); kept default-aligned. sizeof is NOT asserted (header tail is partial).
    struct OutEventLineTestFineResult : public EventBaseLineTestFineResult
    {
        SceneQueryId             mQueryId;            // +0x00  (result[0])
        s32                      miNumIntersections;  // +0x04  (result[1])
        u8                       maHeaderTail[0x38 - 0x08]; // +0x08..+0x37  (records start at +0x38)
        LineTestFineIntersection maIntersections[1];  // +0x38  (variable count == miNumIntersections)

        s32             GetNumIntersections() const          { return miNumIntersections; }
        SceneQueryId    GetQueryId() const                   { return mQueryId; }
        const LineTestFineIntersection& GetIntersection(s32 liIndex) const
        {
            return maIntersections[liIndex];
        }
    };
}
}
