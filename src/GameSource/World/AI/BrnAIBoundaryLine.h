#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// BrnAI::BoundaryLine -- a 2D boundary line attached to an AI Portal (the gap between two
// AI-section edges the racing-line code interpolates across). Returned by Portal::GetBoundaryLine
// and consumed by ResetOnTrackManager::ConvertNodesToPositionAndDirection.
//
// LAYOUT (pinned from the X360 asm of GetInterp @ 0x82676C78): the line is a single 16-byte,
// 16-aligned SIMD register (loaded whole with `lvx128 v13, r0, r4`) holding the two 2D endpoints
// packed as four floats -- start (lane0,lane1) and end (lane2,lane3). Modelled as the two named
// Vector2 endpoints so the 16-byte footprint matches and members are reached by name.
namespace BrnAI
{
    // 16 bytes total: the two 2D endpoints packed into the four lanes of one SIMD register
    // (start at lanes 0,1; end at lanes 2,3). NOT two 16-byte Vector2 members -- the X360
    // loads the whole line as a single `lvx128`, so it is four contiguous floats.
    struct alignas(16) BoundaryLine
    {
        f32 mfStartX;   // +0  (lane 0)
        f32 mfStartY;   // +4  (lane 1)
        f32 mfEndX;     // +8  (lane 2)
        f32 mfEndY;     // +12 (lane 3)

        // 0x82676C78 -- interpolate a point along the boundary line by parameter lfT, writing
        // the result Vector2 to *lpv2Out. (X360 ABI: out in r3, this in r4, lfT in f1.)
        void GetInterp(Vector2* lpv2Out, f32 lfT) const;
    };
}
