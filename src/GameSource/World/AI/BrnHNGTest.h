#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// BrnAI HNG ("Hard No-Go") line-of-sight tests. A section's "no-go lines" are the 2D
// boundary segments the racing-line / reset-on-track code is not allowed to cross; an HNG
// test answers "does the query segment (lStart->lEnd) cut across any of them?".
//
// This home owns both HNG tests the DWARF lists for BrnHNGTest.cpp, reconstructed there:
//   LineTestSectionHNG  @ 0x8277A650  (BrnHNGTest.cpp:46)
//   LineTestTrafficHNG  @ 0x8277A878  (BrnHNGTest.cpp:104; crash parity G07-D1, 2026-09-23)
namespace BrnAI
{
    struct AISection;        // SharedClasses/AI/AISectionsResourceType.h (pointer-only use here)
    struct NearbyVehicles;   // GameSource/World/AI/BrnAIDriver.h -- pointer-only use; the full type
                             // would drag the whole AIDriver header cascade into every includer.

    // 0x8277A650 -- true if the 2D segment lStart->lEnd crosses any of lpSection's no-go
    // (HNG) boundary lines. Asserts lpSection != NULL, then walks the section's
    // muNumNoGoLines boundary segments and returns true for the first one the query segment
    // properly intersects (both intersection parameters within [0,1]); false if none do.
    bool LineTestSectionHNG(const AISection* lpSection, Vector2 lStart, Vector2 lEnd);

    // 0x8277A878 -- true if the 2D segment lAttemptStartPos->lAttemptEndPos crosses one of the
    // four HNG lines boxing a vehicle of the avoidance list. A vehicle is only tested when its
    // SIGNED distance to the segment's line is < 5.0 (KF_TOO_CLOSE_TOO_TRAFFIC) and the foot of
    // its perpendicular falls strictly inside the segment. Callers: ResetOnTrackManager::TestCarHNG
    // (its two traffic legs) and ResetOnTrackDebugComponent::RenderWorld.
    bool LineTestTrafficHNG(const NearbyVehicles* lpNearbyTraffic,
                            Vector2 lAttemptStartPos, Vector2 lAttemptEndPos);
}
