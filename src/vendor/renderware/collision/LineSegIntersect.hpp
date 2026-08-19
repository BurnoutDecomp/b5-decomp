#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4 / RwBool
#include "vendor/renderware/collision/VolRef.hpp"

// ===========================================================================
// rw::collision line-segment intersection vocabulary -- the reconstruction
// mirror of the vendor rwccore.h primitive line-test declarations.
//
// OWNING HOME (LineSegIntersect.cpp) for the functions the X360 binary
// defines that are homed here:
//     rw::collision::FatTriangleLineSegIntersect   @ 0x82BBAD98
//     rw::collision::SolveQuarticRoots             @ 0x82BACA30
//     rw::collision::rwcSphereLineSegIntersect     @ 0x82BA81D8   (wave 2)
//     rw::collision::rwcCylinderLineSegIntersect   @ 0x82BAF8A0   (wave 2)
//     rw::collision::rwcTorusLineSegIntersect      @ 0x82BADAB0   (wave 2)
//     rw::collision::ThinTriangleLineSegIntersect  @ 0x82BB9EB8   (wave 2)
//     rw::collision::TriangleLineSegIntersect      @ 0x82BBB7B8   (wave 2)
// One helper remains a PENDING declaration (see below).
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

// @ 0x82BA81D8 -- sphere vs line segment (canonical rwccore.h:2464): the
// deferred-division near-root sphere test. Returns 1 = hit (lpDist valid),
// 0 = miss, -1 = the segment does not approach the sphere (receding start
// outside) -- the walk-abort code consumed by FatTriangleLineSegIntersect's
// Voronoi loop. X360: r3=lpDist, r4/r5/r6 = the three vector pointers,
// f1 = radius.
s32 rwcSphereLineSegIntersect(Fraction* lpDist,
                              const Vec4* lpLineStart,
                              const Vec4* lpLineDelta,
                              const Vec4* lpCentre,
                              f32 afRadius);

// @ 0x82BAF8A0 -- INFINITE cylinder vs line segment (canonical
// rwccore.h:2536, parameter names `invert`/`ignoreInside`). abIgnoreInside
// suppresses the radially-inside immediate accept; abInvert selects the FAR
// root (exit surface) and drops the receding-line -1 abort. Returns 1 = hit,
// 0 = miss, -1 = segment does not approach the surface (only when !abInvert).
// X360: r3=lpDist, f1=axisLengthSq, f2=radius, r6/r7 = the two flags (the
// 0x82BBAD98 call site passes 0, 0), v1..v4 = the four vectors by value.
s32 rwcCylinderLineSegIntersect(Fraction* lpDist,
                                f32 afAxisLengthSq,
                                f32 afRadius,
                                s32 abInvert,
                                s32 abIgnoreInside,
                                Vec4 aLineStart,
                                Vec4 aLineDelta,
                                Vec4 aBase,
                                Vec4 aAxis);

// @ 0x82BADAB0 -- torus vs line segment (canonical rwccore.h:3813): builds
// the line-parameter quartic of a z-axis torus (major radius R, minor r) and
// reports whether the damped-Newton quartic solver converged. Returns 1 on an
// accepted root (arDist = the line parameter), else 0 (arDist still receives
// the solver's final iterate). Sole caller of SolveQuarticRoots.
s32 rwcTorusLineSegIntersect(f32& arDist,
                             Vec4 aLineStart,
                             Vec4 aLineDir,
                             f32 afMajorRadius,
                             f32 afMinorRadius);

// @ 0x82BB9EB8 -- zero-fatness (thin) triangle vs line segment: one-sided
// Moller-Trumbore with a relative interval tolerance. Returns 1 on hit
// (position/volParam/lineParam filled -- the normal is the CALLER's job:
// TriangleLineSegIntersect @ 0x82BBB7B8 writes it after a hit), 0 on miss.
// X360 register map: r3 = lpResult, v1..v5 = the five vectors by value.
s32 ThinTriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,
                                 Vec4 aLineStart,
                                 Vec4 aLineDelta,
                                 Vec4 aV0,
                                 Vec4 aV1,
                                 Vec4 aV2);

// @ 0x82BBB7B8 -- triangle vs line segment dispatcher: zero fatness runs the
// thin one-sided tester and derives the unit face normal on a hit; non-zero
// fatness seeds that normal into the result (the fat tester's +0x20 input
// contract) and pulls the reported position back onto the core triangle
// surface. X360 register map: r3 = lpResult, v1..v5 = the five vectors by
// value, f1 = the fatness.
s32 TriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,
                             Vec4 aLineStart,
                             Vec4 aLineDelta,
                             Vec4 aV0,
                             Vec4 aV1,
                             Vec4 aV2,
                             f32  afFatness);

// (The 8-parameter TriangleNearestPointRegion declaration that stood here -- with an
//  invented f3 'lpfPlaneDistance' side channel -- was RETIRED 2026-08-18 (wave Q5): the
//  real 7-parameter body @0x82BBA748 lives in GPTriangle.cpp, declared in GPInstance.hpp:297.
//  The callee writes f0/f4-f13 and never f3; the caller's f3 test at 0x82BBB1D4 is its OWN
//  |det| staged at 0x82BBAE30/0x82BBAE44 -- see FatTriangleLineSegIntersect.)

} // namespace collision
} // namespace rw
