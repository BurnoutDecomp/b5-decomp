#include "GameSource/World/AI/BrnAIPortal.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstdint>                                   // uintptr_t (relocation arithmetic)

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

    // Trivial position accessors (:256 / :259 / :262). AISection::GetMiddle @0x826771D0 uses
    // GetPositionY to lift the corner mean to the portal's height; the X360 inlines the
    // `lfs f0, 4(portal)` read, so these have no standalone symbol -- de-inlined here to the
    // named members because Portal keeps them private.
    f32 Portal::GetPositionX() const { return mPositionX; }
    f32 Portal::GetPositionY() const { return mPositionY; }
    f32 Portal::GetPositionZ() const { return mPositionZ; }

    // ------------------------------------------------------------------------------
    // Portal::FixUp / FixDown -- load-time relocation of the one pointer slot.
    //
    // Inlined into AISection::FixUp @0x8267D8C8 / FixDown @0x8267D978 on the console:
    //
    //     if (portal->mpaBoundaryLines) {
    //         portal->mpaBoundaryLines += base;             // (-= on the FixDown leg)
    //         for (k = 0; k < portal->mu8NumBoundaryLines; ++k) { /* empty */ }
    //     }
    //
    // The inner loop body really is empty -- BoundaryLine::FixUp holds no pointers to
    // rebase (it is four packed floats), so it compiled away. The NULL GUARD is real and
    // is kept: unlike the Traffic graph, the AI graph does serialise null slots.
    // ------------------------------------------------------------------------------
    void Portal::FixUp(const void* lpBaseData)
    {
        if (mpaBoundaryLines != nullptr)
        {
            mpaBoundaryLines = reinterpret_cast<BoundaryLine*>(
                reinterpret_cast<uintptr_t>(mpaBoundaryLines)
                + reinterpret_cast<uintptr_t>(lpBaseData));
        }
    }

    void Portal::FixDown(const void* lpBaseData)
    {
        if (mpaBoundaryLines != nullptr)
        {
            mpaBoundaryLines = reinterpret_cast<BoundaryLine*>(
                reinterpret_cast<uintptr_t>(mpaBoundaryLines)
                - reinterpret_cast<uintptr_t>(lpBaseData));
        }
    }
    // GetLinkSectionIndex (DWARF BrnAIPortal.h:273): the console inlines the `lhz 0x10(portal)` read
    // (AStar::Compute / RouteRequestManager); no out-of-line export. Conductor, 2026-09-03.
    u16 Portal::GetLinkSectionIndex() const { return mu16LinkSection; }

}
