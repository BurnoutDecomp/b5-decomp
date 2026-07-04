#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// BrnAI utility free-functions (GameSource/World/AI/BrnAIUtils.{h,cpp}). Decompiled from the
// X360 build. This home owns the two functions reconstructed here:
//   StepTo                          @ 0x82766BA8 (BrnAIUtils.cpp:172)
//   Calc2DIntersectionEquationData  @ 0x82771800 (BrnAIUtils.cpp:211)
// The DWARF for this source path also declares the 2D geometry helpers DistancePointToLine,
// GetInterpOnLine, IsPointOnLine, DistancePosVelToOrigin, Convert3DVectorTo2D / Convert2DVectorTo3D,
// Rotate2DVectorByAngle and Find{Signed,Unsigned}AngleBetween2DVectors; those are reconstructed by
// their own recon passes and are intentionally NOT declared here so this header stays to its owned
// surface.

namespace BrnAI
{
    // 0x82766BA8 - move lfCurrent toward lfTarget by at most lfStep. Asserts on a negative step.
    // If the step overshoots the gap, snaps to the target; otherwise steps up or down by lfStep.
    f32 StepTo(f32 lfCurrent, f32 lfTarget, f32 lfStep);

    // 0x82771800 - solve the 2D line-line intersection parameters for segments (lP1->lP2) and
    // (lQ1->lQ2). Writes the two scalar intersection parameters to *lpfParamA / *lpfParamB and
    // returns true; if the lines are parallel (zero determinant) writes 0 to both and returns false.
    bool Calc2DIntersectionEquationData(Vector2 lP1,
                                        Vector2 lP2,
                                        Vector2 lQ1,
                                        Vector2 lQ2,
                                        f32*    lpfParamA,
                                        f32*    lpfParamB);

    // 0x827651F0 - signed perpendicular distance of the line through lPosition with direction
    // lVelocity to the origin = cross(lVelocity, lPosition)/|lPosition.xy|; falls back to
    // |lVelocity.xy| when |lPosition.xy| is zero. Asserts the result is finite ('Bad maths!').
    f32 DistancePosVelToOrigin(Vector2 lPosition, Vector2 lVelocity);

    // 0x82768680 - SIMD 'fast' point-in-section test against a precomputed 4-edge convex
    // section (SoA edge coefficients). Returns true iff (lfX,lfY) is on the inside half-plane
    // of all four edges. lpSectionEdges points at four attested 4-lane vectors (edgeX0 @+0x00,
    // edgeY0 @+0x10, coefA @+0x20, coefB @+0x30).
    bool IsInsideSectionFast(const void* lpSectionEdges, f32 lfX, f32 lfY);
}
