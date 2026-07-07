// BrnAI::RacingLineGenerator -- SetupSectionExit (@0x8278F548) and
// DropHardNoGoLinesIntoMap (@0x8278F600), reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Both resolve a section-cache entry via GetSectionPointer
// and drive that section's embedded HardNoGoMap. The X360 retail build emits no
// assert code in either body (the DWARF StrStream/<< machinery is compiled out),
// so none is reproduced here.

#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"

#include "GameSource/World/AI/Route/BrnRacingLine.h"   // RacingLine cursor/index/flag triple

namespace BrnAI
{
namespace
{
// Per-frame budget of hard-no-go lines the map is asked to place (KI_MAX_HNG_
// LINES_TO_PLACE_PER_FRAME == 60 == 0x3C, the +0x3C step in both asm bodies).
const s32 KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME = 60;
}

// @0x8278F548
// GetSectionPointer -> zero two edge vectors -> FindMaximalEdges(left, right, 1.0)
// -> exit = (left + right) * 0.5 -> SectionData::SetSectionExit (inlined).
void RacingLineGenerator::SetupSectionExit(RacingLine* lpRacingLine, s32 liNodeIndex)
{
    SectionData* lpCurrentSectionData = GetSectionPointer(lpRacingLine, liNodeIndex);

    Vector2 lLeftEdge;
    Vector2 lRightEdge;
    lLeftEdge.SetZero();
    lRightEdge.SetZero();

    lpCurrentSectionData->mHardNoGoMap.FindMaximalEdges(lLeftEdge, lRightEdge, 1.0f);

    // Midpoint of the two maximal edges (flt_820C4168 == 0.5f). The vaddfp/vmulfp
    // combine all four SIMD lanes; SetSectionExit only consumes x and y.
    Vector2 lMidPoint;
    lMidPoint.x = (lLeftEdge.x + lRightEdge.x) * 0.5f;
    lMidPoint.y = (lLeftEdge.y + lRightEdge.y) * 0.5f;
    lMidPoint.z = (lLeftEdge.z + lRightEdge.z) * 0.5f;
    lMidPoint.w = (lLeftEdge.w + lRightEdge.w) * 0.5f;

    lpCurrentSectionData->SetSectionExit(lMidPoint);
}

// @0x8278F600
// GetSectionPointer(current cursor) -> HardNoGoMap::MakeMap over the next line
// budget window; on success bump the placed-section count, otherwise advance the
// per-frame line cursor by one budget.
void RacingLineGenerator::DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine)
{
    SectionData* lpCurrentSectionData =
        GetSectionPointer(lpRacingLine, lpRacingLine->miCursor);

    const s32 liStart = lpRacingLine->miFlags;
    const s32 liEnd   = liStart + KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;

    if (lpCurrentSectionData->mHardNoGoMap.MakeMap(lpCurrentSectionData->mpAISection, liStart, liEnd))
    {
        ++lpRacingLine->miActiveIndex;
    }
    else
    {
        lpRacingLine->miFlags += KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;
    }
}
}
