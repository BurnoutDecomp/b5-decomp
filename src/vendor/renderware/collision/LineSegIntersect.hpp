#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4 / RwBool
#include "vendor/renderware/collision/VolRef.hpp"

// ===========================================================================
// rw::collision line-segment intersection vocabulary -- the reconstruction
// mirror of the vendor rwccore.h primitive line-test declarations.
//
// OWNING HOME (LineSegIntersect.cpp) for the two functions the X360 binary
// defines that are homed here:
//     rw::collision::FatTriangleLineSegIntersect  @ 0x82BBAD98
//     rw::collision::SolveQuarticRoots            @ 0x82BACA30
// The remaining family members are PENDING declarations (see below).
// ===========================================================================

namespace rw
{
namespace collision
{

// ---------------------------------------------------------------------------
// rw::collision::Fraction (canonical rwccore.h:1448) -- a deferred-division
// line parameter: the primitive tests and the Voronoi-cap migrations trade
// {numerator, denominator} pairs and only divide when a step is actually
// taken (t = num / den). Copied as an 8-byte unit (ld/std).
// ---------------------------------------------------------------------------
struct Fraction
{
    f32 num;   // +0x00
    f32 den;   // +0x04
};

// ---------------------------------------------------------------------------
// rw::collision::VolumeLineSegIntersectResult (canonical rwccore.h:1439):
//     const Volume *v;                 // +0x00 (console pointer word)
//     Vector3 position, normal, volParam;   // +0x10 / +0x20 / +0x30
//     float32_t lineParam;             // +0x40
//     VolRef vRef;                     // +0x50 (the committed 0x80 record)
// Offsets attested by the r30-relative stores of FatTriangleLineSegIntersect
// @ 0x82BBAD98 (position/normal/volParam/lineParam; normal @+0x20 is an INPUT
// -- the caller-seeded unit plane normal -- before it is overwritten) and by
// rw::collision::AddQueryResult @ 0x82BB1588, which fills the whole record
// including the embedded VolRef (volume @+0x50, transform pointer @+0x54
// aimed at the inline rows @+0x60, tag @+0xC0, tag bit count @+0xC4). The
// 0xD0 stride matches the `208 * results` result-list partition of the
// committed VolumeLineQuery::Initialize (SDKs/EATech/rwcollision/
// volumelinequery.cpp). The console pointer word `v` is kept as u32 so the
// stride holds on x64 (same convention as VolRef / PrimitivePairIntersectResult).
// ---------------------------------------------------------------------------
struct VolumeLineSegIntersectResult
{
    u32    v;           // +0x00  the input volume the query is walking (pointer word)
    u32    mPad04[3];   // +0x04  unwritten (16-byte alignment of position)
    Vec4   position;    // +0x10  world-space hit position (out)
    Vec4   normal;      // +0x20  unit plane normal in / hit normal out
    Vec4   volParam;    // +0x30  volume-space hit parameter (u, v, w)
    f32    lineParam;   // +0x40  hit parameter along the segment
    u32    mPad44[3];   // +0x44  unwritten
    VolRef vRef;        // +0x50  the hit volume reference (0x80 stride)
};

static_assert(offsetof(VolumeLineSegIntersectResult, position)  == 0x10, "VLSIR layout");
static_assert(offsetof(VolumeLineSegIntersectResult, normal)    == 0x20, "VLSIR layout");
static_assert(offsetof(VolumeLineSegIntersectResult, volParam)  == 0x30, "VLSIR layout");
static_assert(offsetof(VolumeLineSegIntersectResult, lineParam) == 0x40, "VLSIR layout");
static_assert(offsetof(VolumeLineSegIntersectResult, vRef)      == 0x50, "VLSIR layout");
static_assert(sizeof(VolumeLineSegIntersectResult) == 0xD0,
              "result record must match the X360 208-byte result-list stride");

// ---------------------------------------------------------------------------
// Delivered entry points.
// ---------------------------------------------------------------------------

// @ 0x82BBAD98 -- fat (rounded) triangle vs line segment intersection.
// Returns 1 on hit (result block filled), 0 on miss. The vector parameters
// ride in VMX v1..v5 on the console (by value); f1 = the fatness.
s32 FatTriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,   // r3
                                Vec4 aLineStart,                          // v1
                                Vec4 aLineDelta,                          // v2
                                Vec4 aV0,                                 // v3
                                Vec4 aV1,                                 // v4
                                Vec4 aV2,                                 // v5
                                f32  afFatness);                          // f1

// @ 0x82BACA30 -- damped Newton-Raphson search for one real root of the
// quartic sum(coefficients[k] * x^k). The final iterate is always written to
// arRoot; returns 1 when the last evaluated residual |f(x)| is below 0.001,
// else 0. (Canonical rwccore.h:3812; sole caller rwcTorusLineSegIntersect.)
RwBool SolveQuarticRoots(f32 lafCoefficients[5], f32& arRoot);

// ---------------------------------------------------------------------------
// PENDING family members (declaration-only; bodies not delivered this wave).
// ---------------------------------------------------------------------------

// PENDING @ 0x82BA81D8 -- sphere vs line segment (canonical rwccore.h:2464).
// Call shape attested at the 0x82BBAD98 call site (r3=lpDist out Fraction,
// r4/r5/r6 = the three vector pointers, f1 = radius). Returns <0 on
// error/abort, 0 = no direct hit, >0 = hit codes.
s32 rwcSphereLineSegIntersect(Fraction* lpDist,
                              const Vec4* lpLineStart,
                              const Vec4* lpLineDelta,
                              const Vec4* lpCentre,
                              f32 afRadius);

// PENDING @ 0x82BAF8A0 -- infinite cylinder vs line segment (canonical
// rwccore.h:2537). Call shape attested at the 0x82BBAD98 call site
// (r3=lpDist, f1=axisLengthSq, f2=radius, r6/r7 = two integer flags -- both
// zero there -- and the four vectors in v1..v4). Returns <0 on error, 0 = no
// direct hit, >0 = hit.
s32 rwcCylinderLineSegIntersect(Fraction* lpDist,
                                f32 afAxisLengthSq,
                                f32 afRadius,
                                s32 aiFlagA,
                                s32 aiFlagB,
                                Vec4 aLineStart,
                                Vec4 aLineDelta,
                                Vec4 aBase,
                                Vec4 aAxis);

// PENDING X360 loc_82BBA748 -- the triangle nearest-point / feature-region
// classifier FatTriangleLineSegIntersect calls. The ledger attributes the
// address to rw::collision::GPTriangle::GetIntervals; the observed semantics
// here are a point-vs-triangle nearest-feature query, so it is declared under
// a semantic name pending that TU's own decompile (FLAGGED). X360 internal
// convention: r3 = out nearest point, r4/r5 = out u/v parameters, v1 = query
// point, v2/v3/v4 = V0/V1/V2. Returns the feature region in r3 (0..2 =
// vertex, 3..5 = edge V0V1/V0V2/V1V2, 6 = face interior) AND the signed
// point-to-plane distance in f3 (a same-TU register side channel, consumed by
// the face-region normal flip) -- modelled as lpfPlaneDistance.
s32 TriangleNearestPointRegion(Vec4* lpNearest,
                               f32* lpfParamU,
                               f32* lpfParamV,
                               f32* lpfPlaneDistance,
                               Vec4 aPoint,
                               Vec4 aV0,
                               Vec4 aV1,
                               Vec4 aV2);

// ALSO PENDING in this family (no declarations needed -- nothing delivered
// this wave calls them):
//   rw::collision::TriangleLineSegIntersect      @ 0x82BBB7B8 (rwccore.h:3483)
//   rw::collision::ThinTriangleLineSegIntersect  @ 0x82BB9EB8 (rwccore.h:3583)
//   rw::collision::rwcTorusLineSegIntersect      @ 0x82BADAB0 (rwccore.h:3814;
//       the sole caller of SolveQuarticRoots)

} // namespace collision
} // namespace rw
