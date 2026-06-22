#ifndef CGS_LINE_H
#define CGS_LINE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3Plus alias (rw::math::vpu::Vector3Plus)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsLine.h
//
// CgsGeometric::Line -- a geometric line segment: two endpoints, each a
// Vector3Plus (xyz position, w lane spare). Minimal OWNING home reconstructed
// from the X360 ARTIST build (CgsGeometric::Line::IsValid @ 0x82812370).
//
// LAYOUT (from the IsValid asm, which `lvx128`-loads two 16-byte vectors at
// `this+0x00` and `this+0x10`):
//   +0x00  Vector3Plus mStart   (start point; only xyz lanes are validated)
//   +0x10  Vector3Plus mEnd     (end point;   only xyz lanes are validated)
// sizeof == 32. Member names INFERRED (the DWARF has no entry for this TU);
// the two-endpoint shape is proven by the two 16-byte loads at +0/+0x10.
// ============================================================================

namespace CgsGeometric
{
    struct Line
    {
        Vector3Plus mStart;   // +0x00  start endpoint (xyz validated)
        Vector3Plus mEnd;     // +0x10  end endpoint   (xyz validated)

        // True iff both endpoints have finite (non-NaN) xyz components.
        bool IsValid() const;
    };
}

#endif // CGS_LINE_H
