#include "GameSource/World/AI/BrnAIPortal.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnAI::Portal::GetBoundaryLine @0x82764480.
//
// The X360 build:
//   - reads mu8NumBoundaryLines (this+0x12, lbz) and asserts the requested index is
//     below it ("lu8BoundryIndex < mu8NumBoundaryLines"; the baked d:\p4 file/line is
//     dropped in favour of __FILE__/__LINE__ by CGS_ASSERT);
//   - reads mpaBoundaryLines (this+0xC, lwz) and returns the element at the index:
//     r3 = mpaBoundaryLines + 16 * index. BoundaryLine is a 16-byte line, so this is
//     plain &mpaBoundaryLines[lu8BoundryIndex] (no raw pointer math needed).
namespace BrnAI
{
    const BoundaryLine* Portal::GetBoundaryLine(u8 lu8BoundryIndex) const
    {
        CGS_ASSERT(lu8BoundryIndex < mu8NumBoundaryLines,
                   "lu8BoundryIndex < mu8NumBoundaryLines");
        return &mpaBoundaryLines[lu8BoundryIndex];
    }
}
