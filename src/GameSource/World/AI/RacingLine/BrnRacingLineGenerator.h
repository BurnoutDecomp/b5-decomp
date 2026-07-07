#ifndef BRN_RACING_LINE_GENERATOR_H
#define BRN_RACING_LINE_GENERATOR_H

// BrnAI::RacingLineGenerator -- builds and maintains a driver's racing line from
// the section cache held by BrnAI::RacingLine. This TU bodies two of its members
// store-for-store from BURNOUT_X360_ARTIST.XEX:
//   * SetupSectionExit        (@0x8278F548)
//   * DropHardNoGoLinesIntoMap (@0x8278F600)
// The remaining ~50 members (declared by DWARF) live in other TUs and are not
// modelled here; only the collaborators these two functions touch are declared.
//
// SectionData is the element GetSectionPointer() hands back -- one entry of the
// RacingLine section cache (guest stride 0xB0, matching BrnRacingLine.h's
// SectionCacheEntry). Only the three slots the bodied functions read/write are
// named; the rest is honest padding. Offsets pinned from the two asm bodies:
//   +0x40  exit-target vector  (SetSectionExit stores z=x, w=y of the midpoint)
//   +0x50  embedded HardNoGoMap (FindMaximalEdges/MakeMap run on section+0x50)
//   +0xA0  const AISection*     (passed straight to HardNoGoMap::MakeMap)

#include "types.hpp"
#include "BrnCommonTypes.h"                                 // Vector2
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"  // HardNoGoMap (embedded)

namespace BrnAI
{
class RacingLine;   // fwd (GameSource/World/AI/Route/BrnRacingLine.h)
struct AISection;   // fwd (SharedClasses/AI/AISectionsResourceType.h)

struct SectionData
{
    u8          maPadHead[0x40];    // +0x00  other cache-entry fields (other TUs)
    Vector2     mExitTarget;        // +0x40  section exit point (z/w lanes written)
    HardNoGoMap mHardNoGoMap;       // +0x50  per-section "don't drive here" bit grid
    // Remainder of the map object past the committed HardNoGoMap head (its own tail
    // is owned by the map's build/spread TUs); keeps mpAISection at guest +0xA0.
    u8          maHngMapTail[0xA0 - 0x50 - sizeof(HardNoGoMap)];
    const AISection* mpAISection;   // +0xA0  section this entry mirrors
    u8          maPadTail[0xB0 - 0xA0 - sizeof(const AISection*)];  // to 0xB0 stride

    // Inlined by SetupSectionExit (vrlimi128 SetZ<X>/SetW<Y>): drop the exit
    // point's x/y into the z/w lanes of the exit vector, leaving x/y untouched.
    void SetSectionExit(const Vector2& lExit)
    {
        mExitTarget.z = lExit.x;
        mExitTarget.w = lExit.y;
    }
};

class RacingLineGenerator
{
public:
    // @0x8278F548  Set the section's exit target to the midpoint of the hard-no-go
    // map's maximal edges.
    void SetupSectionExit(RacingLine* lpRacingLine, s32 liNodeIndex);

    // @0x8278F600  Place the current section's next budgeted batch of hard-no-go
    // lines; advance the per-frame success count or the line cursor.
    void DropHardNoGoLinesIntoMap(RacingLine* lpRacingLine);

private:
    // Resolve the cache entry for a section index (bodied in another TU).
    SectionData* GetSectionPointer(RacingLine* lpRacingLine, s32 liIndex);
};
}

#endif
