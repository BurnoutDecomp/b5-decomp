// BrnAI::RacingLineGenerator -- SetupSectionExit (@0x8278F548) and
// DropHardNoGoLinesIntoMap (@0x8278F600), reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Both resolve a section-cache entry via GetSectionPointer
// and drive that section's embedded HardNoGoMap. The X360 retail build emits no
// assert code in either body (the DWARF StrStream/<< machinery is compiled out),
// so none is reproduced here.

#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"

#include "GameSource/World/AI/Route/BrnRacingLine.h"   // RacingLine spread-cursor triple + SectionData

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
// GetSectionPointer(miSectionToSpread) -> HardNoGoMap::MakeMap over the next line
// budget window starting at miHNGLineStart; on success step miBackwardsStep, otherwise
// advance miHNGLineStart by one budget. (Member names: DWARF BrnRacingLine.h:103/:106/:109,
// pinned to the 0xBC0/0xBC4/0xBC8 stores of ClearSectionCache @0x8276E090.)
void RacingLineGenerator::DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine)
{
    SectionData* lpCurrentSectionData =
        GetSectionPointer(lpRacingLine, lpRacingLine->miSectionToSpread);

    const s32 liStart = lpRacingLine->miHNGLineStart;
    const s32 liEnd   = liStart + KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;

    if (lpCurrentSectionData->mHardNoGoMap.MakeMap(lpCurrentSectionData->mpLineSection, liStart, liEnd))
    {
        ++lpRacingLine->miBackwardsStep;
    }
    else
    {
        lpRacingLine->miHNGLineStart += KI_MAX_HNG_LINES_TO_PLACE_PER_FRAME;
    }
}
}
