#pragma once

// Canonical home for BrnWorld::WorldEntityIO::RequestInterface
// (BrnWorldEntityRequestInterface.h). This is the small request-flag object the world-entity
// module's GUI->world bridge uses to ask the entity modules to (re)validate or invalidate the
// collision world before the next scene. It carries exactly two boolean request flags and a
// small set of guarded mutators; the X360 ARTIST spine is authoritative for the layout and the
// bodies.
//
// LAYOUT (X360 getter/mutator byte stores, authoritative):
//   +0  bool mbInvalidateCollisionWorld   (InvalidateCollisionWorld sets *this+0 = 1)
//   +1  bool mbValidateCollisionWorld     (ValidateCollisionWorld   sets *this+1 = 1)
//
// Proven by the out-of-line bodies:
//   InvalidateCollisionWorld @ 0x823A7DC8  tests byte +1 (validate); on set fires the tripwire
//                                           "Can not invalidate collision world when already
//                                           validating!" (BrnWorldEntityRequestInterface.h:85)
//                                           then `stb 1, 0(this)`  -> mbInvalidateCollisionWorld = true
//   ValidateCollisionWorld   @ 0x823A7E70  tests byte +0 (invalidate); same tripwire text
//                                           (line 92) then `stb 1, 1(this)` -> mbValidateCollisionWorld = true
//   Append                   @ 0x8279BA10  ORs the two request flags of `lrOther` into `this`
//                                           (byte +1 |= other +1, byte +0 |= other +0) then,
//                                           when BOTH end up set, fires the tripwire
//                                           "Append resulted in requests to validate and
//                                           invalidate the collision world" (line 114)
//
// The asserts are non-gating tripwires (house CGS_ASSERT). The single-byte stores in the asm
// (`stb`) pin both flags to byte width and their +0 / +1 placement.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace WorldEntityIO
{
    // The collision-world (in)validation request the GUI->world bridge raises. Exactly two
    // mutually-exclusive request flags (you may not ask to validate and invalidate at once).
    struct RequestInterface
    {
        RequestInterface()
            : mbInvalidateCollisionWorld(false)
            , mbValidateCollisionWorld(false)
        {
        }

        // 0x823A7DC8 -- request the collision world be invalidated. Asserts a validate request
        // is not already pending, then raises the invalidate flag.
        void InvalidateCollisionWorld();

        // 0x823A7E70 -- request the collision world be (re)validated. Asserts an invalidate
        // request is not already pending, then raises the validate flag.
        void ValidateCollisionWorld();

        // 0x8279BA10 -- merge lrOther's pending requests into this one (logical OR of each flag).
        // Asserts the merged result does not request both validate and invalidate at once.
        void Append(const RequestInterface& lrOther);

        bool mbInvalidateCollisionWorld;   // +0
        bool mbValidateCollisionWorld;     // +1
    };
}
}
