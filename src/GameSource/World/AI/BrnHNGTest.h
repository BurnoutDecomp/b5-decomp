#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// BrnAI HNG ("Hard No-Go") line-of-sight tests. A section's "no-go lines" are the 2D
// boundary segments the racing-line / reset-on-track code is not allowed to cross; an HNG
// test answers "does the query segment (lStart->lEnd) cut across any of them?".
//
// This home owns the section-side test reconstructed in BrnHNGTest.cpp:
//   LineTestSectionHNG  @ 0x8277A650  (BrnHNGTest.cpp:46)
//
// The DWARF for this source path also declares the traffic-side overload
// LineTestTrafficHNG(const BrnAI::NearbyVehicles*, Vector2, Vector2) (BrnHNGTest.cpp:104);
// that is a separate TU and is intentionally NOT declared here so this header stays to its
// owned surface (matching the BrnAIUtils.h convention).
namespace BrnAI
{
    struct AISection;   // SharedClasses/AI/AISectionsResourceType.h (pointer-only use here)

    // 0x8277A650 -- true if the 2D segment lStart->lEnd crosses any of lpSection's no-go
    // (HNG) boundary lines. Asserts lpSection != NULL, then walks the section's
    // muNumNoGoLines boundary segments and returns true for the first one the query segment
    // properly intersects (both intersection parameters within [0,1]); false if none do.
    bool LineTestSectionHNG(const AISection* lpSection, Vector2 lStart, Vector2 lEnd);
}
