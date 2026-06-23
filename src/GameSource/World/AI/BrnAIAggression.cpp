#include "GameSource/World/AI/BrnAIAggression.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnAI::AIAggression::GetTargetPos @0x827656D0.
//
// Returns the cached world-space target position. The X360 build:
//   - asserts mbTargetPosValid (this+0x44); on failure fires the dev-assert path
//     (folded into CGS_ASSERT here -- the baked d:\p4 file/line is dropped in favour
//     of the compiler's __FILE__/__LINE__);
//   - copies mTargetPos (this+0x30) into the ABI-hidden 16-byte return slot via a single
//     lvx128/stvx128 vector move -- a plain whole-Vector3 copy, not a per-lane compute.
//
// The DWARF declares this returning Vector3 by value; the X360 sret pointer arrives in r3
// and `this` in r4, exactly matching that ABI.
namespace BrnAI
{
    Vector3 AIAggression::GetTargetPos()
    {
        CGS_ASSERT(mbTargetPosValid, "mbTargetPosValid");
        return mTargetPos;
    }
}
