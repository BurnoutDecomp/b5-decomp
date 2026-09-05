#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// BrnAI utility free-functions (GameSource/World/AI/BrnAIUtils.{h,cpp}). Decompiled from the
// X360 build. This home owns the two functions reconstructed here:
//   StepTo                          @ 0x82766BA8 (BrnAIUtils.cpp:172)
//   Calc2DIntersectionEquationData  @ 0x82771800 (BrnAIUtils.cpp:211)
// The DWARF for this source path also declares the 2D geometry helpers DistancePointToLine,
// GetInterpOnLine, IsPointOnLine, DistancePosVelToOrigin, Convert3DVectorTo2D / Convert2DVectorTo3D,
// Rotate2DVectorByAngle and Find{Signed,Unsigned}AngleBetween2DVectors. The two angle helpers are
// bodied in the partfile BrnAIUtils_Angles.cpp (AIDriver steering wave 2026-09-03) and declared
// below; the rest are reconstructed by their own recon passes and intentionally NOT declared here.

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
    // lVelocity to the origin = cross(lPosition, lVelocity)/|lVelocity.xy|; falls back to
    // |lPosition.xy| when |lVelocity.xy| is zero. Asserts the result is finite ('Bad maths!').
    // (Roles corrected 2026-09-05 against the asm -- see the body.)
    f32 DistancePosVelToOrigin(Vector2 lPosition, Vector2 lVelocity);

    // 0x8276DDB8 - signed perpendicular distance from lPoint to the infinite line through
    // (lLineStart, lLineEnd) = cross(end - start, point - start) / |end - start|; a zero-length
    // segment falls back to |point - start|. Asserts the result is finite ('Bad maths!',
    // BrnAIUtils.cpp:117). Consumer: RacingLineGenerator::GetPerpendicularDistanceToCentreLine.
    f32 DistancePointToLine(Vector2 lPoint, Vector2 lLineStart, Vector2 lLineEnd);

    // 0x82768680 - SIMD 'fast' point-in-section test against a precomputed 4-edge convex
    // section (SoA edge coefficients). Returns true iff (lfX,lfY) is on the inside half-plane
    // of all four edges. lpSectionEdges points at four attested 4-lane vectors (edgeX0 @+0x00,
    // edgeY0 @+0x10, coefA @+0x20, coefB @+0x30).
    bool IsInsideSectionFast(const void* lpSectionEdges, f32 lfX, f32 lfY);

    // 0x82766B20 - unsigned planar angle between two 2D unit vectors: acos(a.x*b.x + a.y*b.y),
    // or 0.0 when |dot| >= 1.0 (the X360 returns ZERO there -- it does not clamp the acos).
    // Bodied in BrnAIUtils_Angles.cpp (partfile of this TU).
    f32 FindUnsignedAngleBetween2DVectors(Vector2 lA, Vector2 lB);

    // 0x827716A8 - signed planar angle from lA to lB: the unsigned angle carrying the sign of the
    // 2D cross product +(a.x*b.y - a.y*b.x) -- POSITIVE when that cross is positive -- asserting
    // the cross product and the result are finite ("NAN error in
    // AIDriver::FindSignedAngleBetween2DVectors"). No IDA export exists for this address; the sign
    // was re-read word-by-word from the image bytes 0x8277176C..0x827717B0 (aiwave R7; the earlier
    // banner had the vcmpgtfp/vcmpgefp operands transposed and documented the opposite sign).
    f32 FindSignedAngleBetween2DVectors(Vector2 lA, Vector2 lB);
}
