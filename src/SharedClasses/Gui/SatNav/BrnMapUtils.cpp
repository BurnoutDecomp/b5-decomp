// BrnMapUtils.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::MapTransform's out-of-line
// 2D map-space functions. The X360 implements these as hand-vectorised VMX128 (dot
// products via vmsum3fp128, matrix-inverse reciprocals via vrefp + two Newton-Raphson
// refinement steps vnmsubfp/vmaddfp, lane shuffles via vperm/vpermwi128). Those
// intrinsics have no portable PC equivalent, so the bodies below are SEMANTIC
// reconstructions in named-member scalar/lane C++ that produce the same result, per
// the policy stated in rw/math/vpu/types.h. The 3x3 transforms are row-major affine:
// a 2D point p is transformed as (p.x, p.y, 1) . M, i.e.
//   out = p.x * M.xAxis + p.y * M.yAxis + M.zAxis.
//
// ⭐ H3b (2026-08-25): the static map-space VALUES are now the image's own (read off
// the cinit thunks 0x82C51F90..0x82C52480 -- scratch h3b_dump.txt); the old
// "numeric contents owned by an out-of-scope init" boundary is retired. Two committed
// bodies are also FIXED against their asm:
//   * SetZoomedWorldRect @0x824504E8 -- the old body built a rect coord space and
//     biased its translation by a "centre" argument. The real function receives THREE
//     CORNERS of the (possibly rotated) zoomed rect, stores {C.xy, B.xy} as the
//     zoomed world rect, and stores the INVERSE of the corner coord space (a full
//     adjugate/determinant 3x3 inverse in the asm) as the world -> zoomed-unit
//     transform. The old body could not represent rotation at all.
//   * SetZoomedViewportRect @0x82450608 -- the old body composed with MakeTransform
//     (an inverse). The real function stores the viewport coord space AND its
//     composition WITH the device space (plain row product, no inverse).

#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"

namespace BrnGui
{

namespace
{
    // Compose two row-major affine 3x3 transforms: result = lhs followed by rhs,
    // i.e. transforming a point by `result` equals transforming by lhs then rhs.
    Matrix33 Multiply33( const Matrix33& lm33Lhs, const Matrix33& lm33Rhs )
    {
        Matrix33 lm33Out;
        // Each output basis row is lhs's row run through rhs (homogeneous, w treated as 1
        // only for the translation row zAxis).
        lm33Out.xAxis = {
            lm33Lhs.xAxis.x * lm33Rhs.xAxis.x + lm33Lhs.xAxis.y * lm33Rhs.yAxis.x,
            lm33Lhs.xAxis.x * lm33Rhs.xAxis.y + lm33Lhs.xAxis.y * lm33Rhs.yAxis.y,
            0.0f, 0.0f };
        lm33Out.yAxis = {
            lm33Lhs.yAxis.x * lm33Rhs.xAxis.x + lm33Lhs.yAxis.y * lm33Rhs.yAxis.x,
            lm33Lhs.yAxis.x * lm33Rhs.xAxis.y + lm33Lhs.yAxis.y * lm33Rhs.yAxis.y,
            0.0f, 0.0f };
        lm33Out.zAxis = {
            lm33Lhs.zAxis.x * lm33Rhs.xAxis.x + lm33Lhs.zAxis.y * lm33Rhs.yAxis.x + lm33Rhs.zAxis.x,
            lm33Lhs.zAxis.x * lm33Rhs.xAxis.y + lm33Lhs.zAxis.y * lm33Rhs.yAxis.y + lm33Rhs.zAxis.y,
            1.0f, 0.0f };
        return lm33Out;
    }

    // Invert a row-major affine 3x3 (2x2 linear part + translation). The X360 path uses
    // vrefp + two Newton-Raphson steps to form the reciprocal determinant; here we use
    // the C++ reciprocal directly (same value).
    Matrix33 Invert33( const Matrix33& lm33In )
    {
        const f32 lfA = lm33In.xAxis.x, lfB = lm33In.xAxis.y;
        const f32 lfC = lm33In.yAxis.x, lfD = lm33In.yAxis.y;
        const f32 lfTx = lm33In.zAxis.x, lfTy = lm33In.zAxis.y;

        const f32 lfDet = lfA * lfD - lfB * lfC;
        const f32 lfInvDet = ( lfDet != 0.0f ) ? ( 1.0f / lfDet ) : 0.0f;

        Matrix33 lm33Out;
        lm33Out.xAxis = {  lfD * lfInvDet, -lfB * lfInvDet, 0.0f, 0.0f };
        lm33Out.yAxis = { -lfC * lfInvDet,  lfA * lfInvDet, 0.0f, 0.0f };
        // Inverse translation: -T . inverse(linear).
        lm33Out.zAxis = {
            -( lfTx * lm33Out.xAxis.x + lfTy * lm33Out.yAxis.x ),
            -( lfTx * lm33Out.xAxis.y + lfTy * lm33Out.yAxis.y ),
            1.0f, 0.0f };
        return lm33Out;
    }
}

// ---- static map-space state -------------------------------------------------
// Values are the image's own (cinit thunk region 0x82C51F90..0x82C52480; exact f32
// bit patterns quoted). The coord-space matrices are built from their rects exactly
// as the thunks do (MakeCoordSpaceFromRect); within this TU the definition order
// guarantees the rects initialise first.
const Vector4  MapTransform::smv4WorldRect      = { -4375.419921875f, -5842.419921875f, 5363.14990234375f, 3904.739990234375f }; // 0xC588BB5C/0xC5B6935C/0x45A79933/0x45740BD7
const Vector4  MapTransform::smv4NormalizedRect = { 0.0f, 0.0f, 1.0f, 1.0f };
const Vector4  MapTransform::smv4DeviceRect     = { 0.0f, 0.0f, 1280.0f, 720.0f };
const Matrix33 MapTransform::smm33WorldSpace      = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4WorldRect );
const Matrix33 MapTransform::smm33NormalisedSpace = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4NormalizedRect );
const Matrix33 MapTransform::smm33DeviceSpace     = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4DeviceRect );

// The live sat-nav viewport rect (@0x82FB36A0). Default = the HD rect @0x82FB30A0
// (the cinit copy thunk @0x82C52340); GuiModule::Construct re-installs the HD/SD pick
// through SetSatNavRect. (SD alt @0x82FB3130 = {0.750781238079071, y0, 0.9039062261581421, y1}.)
Vector4  MapTransform::smv4SatNavViewRect = { 0.778124988079071f, 0.6652777791023254f, 0.9312499761581421f, 0.8666666746139526f };

Vector4  MapTransform::smv4ZoomedWorldRect                = { 0.0f, 0.0f, 0.0f, 0.0f };
Matrix33 MapTransform::smm33ZoomedWorldTransform          = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
Matrix33 MapTransform::smm33ZoomedViewportTransform       = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
Matrix33 MapTransform::smm33ZoomedViewportScreenTransform = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };

// @ 0x82450460 — build the coord-space matrix that maps the unit square onto lv4Rect
// (lv4Rect = minX, minY, maxX, maxY). Scale = (maxX-minX, maxY-minY), translate = (minX, minY).
Matrix33 MapTransform::MakeCoordSpaceFromRect( Vector4 lv4Rect )
{
    const f32 lfWidth  = lv4Rect.z - lv4Rect.x;   // maxX - minX
    const f32 lfHeight = lv4Rect.w - lv4Rect.y;   // maxY - minY

    Matrix33 lm33Out;
    lm33Out.xAxis = { lfWidth,    0.0f,     0.0f, 0.0f };
    lm33Out.yAxis = { 0.0f,       lfHeight, 0.0f, 0.0f };
    lm33Out.zAxis = { lv4Rect.x,  lv4Rect.y, 1.0f, 0.0f };
    return lm33Out;
}

// (X360 inlines this; the layout is attested by SetZoomedWorldRect @0x824504E8 and
// SatNavRenderer::RenderComponent @0x82465EC0, which both assemble exactly these rows.)
// The coord space with origin lv2Origin whose unit X lands on lv2XPoint and unit Y on
// lv2YPoint: xAxis = X - origin, yAxis = Y - origin.
Matrix33 MapTransform::MakeCoordSpaceFromPoints( Vector2 lv2Origin, Vector2 lv2XPoint, Vector2 lv2YPoint )
{
    Matrix33 lm33Out;
    lm33Out.xAxis = { lv2XPoint.x - lv2Origin.x, lv2XPoint.y - lv2Origin.y, 0.0f, 0.0f };
    lm33Out.yAxis = { lv2YPoint.x - lv2Origin.x, lv2YPoint.y - lv2Origin.y, 0.0f, 0.0f };
    lm33Out.zAxis = { lv2Origin.x,               lv2Origin.y,               1.0f, 0.0f };
    return lm33Out;
}

// (X360 inlines the one-matrix apply at every call site.) Apply a row-major affine 3x3
// to a 2D point: out = p.x * xAxis + p.y * yAxis + zAxis.
Vector2 MapTransform::Transform( Vector2 lv2Point, Matrix33 lm33Transform )
{
    Vector2 lv2Out;
    lv2Out.x = lv2Point.x * lm33Transform.xAxis.x
             + lv2Point.y * lm33Transform.yAxis.x
             + lm33Transform.zAxis.x;
    lv2Out.y = lv2Point.x * lm33Transform.xAxis.y
             + lv2Point.y * lm33Transform.yAxis.y
             + lm33Transform.zAxis.y;
    lv2Out.z = 0.0f;
    lv2Out.w = 0.0f;
    return lv2Out;
}

// @ 0x824503C0 — the two-matrix overload: take the point out of `lm33From` space into
// `lm33To` space (MakeTransform then apply -- the X360 body is exactly that pair:
// `bl MakeTransform` on the two matrix pointers in r3/r4, then the inline apply on v1).
// ⭐ H3b: this address was previously attributed to the one-matrix overload; the asm's
// two pointer args pin it here.
Vector2 MapTransform::Transform( Vector2 lv2Point, Matrix33 lm33From, Matrix33 lm33To )
{
    return Transform( lv2Point, MakeTransform( lm33From, lm33To ) );
}

// @ 0x8245A080 (IDA `sub_8245A080` -- unnamed in the export set, identified here) — the
// RECT-pair overload: take a point out of the coord space of `lv4From` into the coord space
// of `lv4To`. Attested end to end: the 86-instruction body is two inlined
// MakeCoordSpaceFromRect builds (per rect: `vspltw` of all four lanes, `vsubfp` of z-x and
// w-y for the two axis lengths, `vperm` through the mask at unk_82CDA350 to interleave the
// lanes, and `vrlimi128 v,v12,2,0` -- v12 == vcfsx(vspltisw 1) == 1.0f -- to set the
// translation row's homogeneous 1) followed by a TAIL CALL to
// `BrnGui::MapTransform::Transform` @0x824503C0, the two-matrix overload above, with the two
// stack matrices in r3/r4. The unnamed symbol is why this leg has read as missing: it is the
// declared-only DWARF overload at BrnMapUtils.h, and MainMapComponent::Update / ApplyZoom /
// SnapToLocation call it four times each (world rect <-> MapTransform::smv4NormalizedRect).
Vector2 MapTransform::Transform( Vector2 lv2Point, Vector4 lv4From, Vector4 lv4To )
{
    return Transform( lv2Point,
                      MakeCoordSpaceFromRect( lv4From ),
                      MakeCoordSpaceFromRect( lv4To ) );
}

// @ 0x824435C8 — the transform that takes a point out of `lm33From` space into
// `lm33To` space: inverse(from) composed with to.
Matrix33 MapTransform::MakeTransform( Matrix33 lm33From, Matrix33 lm33To )
{
    return Multiply33( Invert33( lm33From ), lm33To );
}

// @ 0x8245A080 (main-map slice 2026-08-27) — the rect-to-rect overload, declared in the
// DWARF (BrnMapUtils.h family) and previously in this header's declared-only list. The
// X360 body is the un-NAMED sub_8245A080 that the MainMapComponent view pipeline calls
// from Update / SnapToLocation / ApplyZoom / CalculatePositionedWorldRect (~10 sites):
// it assembles TWO MakeCoordSpaceFromRect matrices on the stack — the vperm/vrlimi128
// block builds exactly xAxis = {w, 0, 0, ·}, yAxis = {0, h, 0, 0}, zAxis =
// {min.x, min.y, 1, ·} per rect, where `·` is uninitialised splat junk the consumer
// never reads — from the two rect args in v2 (from) / v3 (to), then TAIL-CALLS the
// two-matrix Transform @0x824503C0 with the point still untouched in v1 and the two
// matrix pointers in r3/r4 (from-matrix in r3).
Vector2 MapTransform::Transform( Vector2 lv2Point, Vector4 lv4From, Vector4 lv4To )
{
    return Transform( lv2Point,
                      MakeCoordSpaceFromRect( lv4From ),
                      MakeCoordSpaceFromRect( lv4To ) );
}

// @ 0x8244F318 — the pre-race map zoom: how many world units the (zoomed) map view must
// span to fit the event's bounding rect A..B into the on-screen display rect C.
// Decoded from the X360 asm (vminfp/vmaxfp corner fold, the vrefp + two-Newton-Raphson
// reciprocals == full-precision divides, the two fsel maxima at the tail):
//   min = min(A, B), max = max(A, B)  (per lane; the caller passes them pre-ordered)
//   tX  = C.x * 0.85 / smv4DeviceRect.z      (screen fraction the map view spans in X)
//   tY  = C.y * 0.85 / smv4DeviceRect.w      (and in Y)
//   zoom = max( ((max.x-min.x) / tX) / lfBase,  (max.y-min.y) / tY,  2500 )
// C is the fly-by's display rect in DEVICE PIXELS ({638, 349.8}); lfBase is the display
// aspect (KF_MAP_BASE_ASPECT_RATIO) folded into the X term only (1280/1.7778 == 720, so
// both terms measure world-units-per-0.85-screen-height). Constants read off the image:
// 0.85 == flt_820550C0 (the usable-view fraction), 2500 == flt_820550BC (the minimum
// zoom -- the map never zooms tighter than a 2500-world-unit window).
f32 MapTransform::CalculateZoomFactor( Vector2 lv2A, Vector2 lv2B, Vector2 lv2C, f32 lfBase )
{
    const f32 KF_USABLE_VIEW_FRACTION = 0.85000002f;   // flt_820550C0
    const f32 KF_MINIMUM_ZOOM         = 2500.0f;       // flt_820550BC

    // vminfp / vmaxfp: the per-lane corner fold.
    const f32 lfMinX = ( lv2A.x < lv2B.x ) ? lv2A.x : lv2B.x;
    const f32 lfMinY = ( lv2A.y < lv2B.y ) ? lv2A.y : lv2B.y;
    const f32 lfMaxX = ( lv2A.x > lv2B.x ) ? lv2A.x : lv2B.x;
    const f32 lfMaxY = ( lv2A.y > lv2B.y ) ? lv2A.y : lv2B.y;

    const f32 lfWidth  = lfMaxX - lfMinX;
    const f32 lfHeight = lfMaxY - lfMinY;

    // The screen fractions the display rect occupies (the vrefp+N-R reciprocal of the
    // device extents, times the usable fraction, times the rect's pixel extents).
    const f32 lfFractionX = lv2C.x * KF_USABLE_VIEW_FRACTION / smv4DeviceRect.z;
    const f32 lfFractionY = lv2C.y * KF_USABLE_VIEW_FRACTION / smv4DeviceRect.w;

    const f32 lfZoomX = ( lfWidth / lfFractionX ) * ( 1.0f / lfBase );
    const f32 lfZoomY = lfHeight / lfFractionY;

    // The two fsel maxima: the larger axis requirement, floored at the minimum zoom.
    f32 lfZoom = ( lfZoomX - lfZoomY >= 0.0f ) ? lfZoomX : lfZoomY;
    lfZoom = ( lfZoom - KF_MINIMUM_ZOOM >= 0.0f ) ? lfZoom : KF_MINIMUM_ZOOM;
    return lfZoom;
}

// @ 0x824504E8 — install the zoomed world window from THREE CORNERS of the (possibly
// rotated) rect: A is the shared origin corner, B and C its two adjacent corners
// (SatNavComponent::SetViewParamsFromPlayerCar / SatNavRenderer::RenderComponent pass
// corners[0], corners[2], corners[1] of GetZoomedCarWorldRect in that order). Stores
//   smv4ZoomedWorldRect       = {C.x, C.y, B.x, B.y}   (the two adjacent corners)
//   smm33ZoomedWorldTransform = INVERSE(coordSpaceFromPoints(A, B-A axisX, C-A axisY))
// i.e. the world -> zoomed-unit transform (the asm's full adjugate + vrefp/N-R
// reciprocal-determinant 3x3 inverse).
//
// ⭐ [satnav rotation 2026-08-25] AXIS ORDER FIXED (was (A, C, B) -- a TRANSPOSED map:
// the whole minimap rendered reflected about its diagonal, i.e. "the wrong rotation").
// The asm's VMX adjugate lane order is opaque, but the (A, B, C) assignment is pinned
// three independent ways by the surrounding X360 code:
//   1. rotate-map mode must put the car's forward vector screen-UP. The heading basis is
//      RotationY(theta).(0,0,1) == {sin, 0, cos} (attested in-tree by the rival FOV cone,
//      MapIconManager::GetSatNavIconStateForRival @0x824FA320), and screen-down == the
//      unit-y axis: only yAxis = C-A == corners[1]-corners[0] == -2*Tz == -(sin,cos)*2d
//      satisfies it. xAxis = B-A follows.
//   2. the north indicator is driven with the map's effective rotation (theta in rotate
//      mode, the constant pi for the fixed map -- SatNavComponent::Update). The fixed
//      map's screen-up under (A, B, C) is exactly {sin pi, cos pi}; under (A, C, B) it
//      is 90 degrees off the angle the indicator is told.
//   3. the fixed map then shows the map texture in its authored orientation (screen
//      right == world +X == texture U, screen down == world +Z == texture V, per the
//      asm-pinned smm33WorldSpace rect above); (A, C, B) drew it transposed.
void MapTransform::SetZoomedWorldRect( Vector2 lv2CornerA, Vector2 lv2CornerB, Vector2 lv2CornerC )
{
    smv4ZoomedWorldRect = { lv2CornerC.x, lv2CornerC.y, lv2CornerB.x, lv2CornerB.y };
    smm33ZoomedWorldTransform =
        Invert33( MakeCoordSpaceFromPoints( lv2CornerA, lv2CornerB, lv2CornerC ) );
}

// @ 0x82450608 — install the zoomed viewport from the on-screen rect: the viewport
// coord space itself, and its composition with the DEVICE space (a plain row product,
// no inverse -- zoomed-unit -> viewport-normalised -> device).
void MapTransform::SetZoomedViewportRect( Vector4 lv4Rect )
{
    smm33ZoomedViewportTransform = MakeCoordSpaceFromRect( lv4Rect );

    // The asm composes each viewport row through the device-space rows directly
    // (out_row = r.x*D0 + r.y*D1 + r.z*D2) == Multiply33(viewport, device).
    smm33ZoomedViewportScreenTransform =
        Multiply33( smm33ZoomedViewportTransform, smm33DeviceSpace );
}

// @ 0x82428878 — map a world point onto the device through the zoomed window:
//   u = (world.x, world.z, 1) . smm33ZoomedWorldTransform        (zoomed-unit 0..1)
//   [lbClamp] direction-preserving clamp of u toward the window centre (0.5, 0.5):
//     walk the line from (0.5,0.5) through u back until it enters the unit square
//     (slope/intercept in the asm, with a 1-x mirror for negative overshoot), then a
//     plain clamp01 of both lanes.
//   out = (u.x, u.y, 1) . smm33ZoomedViewportScreenTransform     (device space)
Vector2 MapTransform::WorldToDevice( Vector3 lv3World, bool lbClamp )
{
    // The map plane is world (x, z) (the asm perms lanes 0/2 of the input vector).
    Vector2 lv2World;
    lv2World.x = lv3World.x;
    lv2World.y = lv3World.z;
    lv2World.z = 0.0f;
    lv2World.w = 0.0f;

    Vector2 lv2Unit = Transform( lv2World, smm33ZoomedWorldTransform );

    if ( lbClamp )
    {
        f32 lfX = lv2Unit.x;
        f32 lfY = lv2Unit.y;

        // The asm guards the slope on x == 0.5 exactly (vcmpeqfp against the 0.5 splat).
        if ( lfX != 0.5f )
        {
            const f32 lfSlope     = ( lfY - 0.5f ) / ( lfX - 0.5f );
            const f32 lfIntercept = lfY - lfSlope * lfX;

            // Overshoot measure per axis: the raw value, mirrored (1 - v) when negative.
            const f32 lfOverX = ( lfX < 0.0f ) ? ( 1.0f - lfX ) : lfX;
            const f32 lfOverY = ( lfY < 0.0f ) ? ( 1.0f - lfY ) : lfY;

            if ( lfOverX > 1.0f && lfOverX > lfOverY )
            {
                // x dominates: clamp x, keep the point on the centre line.
                lfX = ( lfX < 0.0f ) ? 0.0f : ( lfX > 1.0f ? 1.0f : lfX );
                lfY = lfSlope * lfX + lfIntercept;
            }
            else if ( lfOverY > 1.0f && lfOverY > lfOverX )
            {
                // y dominates: clamp y, solve the line for x (the asm's vrefp(slope)).
                lfY = ( lfY < 0.0f ) ? 0.0f : ( lfY > 1.0f ? 1.0f : lfY );
                lfX = ( lfY - lfIntercept ) * ( 1.0f / lfSlope );
            }
        }

        // Final plain clamp01 of both lanes (always applied on the clamp path).
        lv2Unit.x = ( lfX < 0.0f ) ? 0.0f : ( lfX > 1.0f ? 1.0f : lfX );
        lv2Unit.y = ( lfY < 0.0f ) ? 0.0f : ( lfY > 1.0f ? 1.0f : lfY );
    }

    Vector2 lv2Out = Transform( lv2Unit, smm33ZoomedViewportScreenTransform );
    lv2Out.z = 0.0f;
    lv2Out.w = 0.0f;
    return lv2Out;
}

// (X360 inlines both of these; DWARF BrnMapUtils.h:135 / :140.) The map plane is world XZ:
// Flatten drops the world Y and packs (x, z) into a Vector2, Unflatten lifts a map point
// back onto the y == 0 plane.
//
// ⭐ THE XZ LANE PICK IS ATTESTED TWICE, INDEPENDENTLY.
//  1. DWARF. The dump gives Flatten exactly one child, and it is
//     `rw::math::vpu::_asmCreateVectorAxis<VectorAxisX, VectorAxisZ>`
//     (references/DecFIGS/dwarfdump/_compile/BrnGuiUnity.cpp:1493-1497) -- the SDK's
//     "build a 2-lane register from these two axes of a Vector3" primitive, with the axes
//     spelled X and Z in the template arguments. That names the lanes outright.
//  2. THE IMAGE. The permute mask the family uses, unk_82CDA450, reads
//     {0,1,2,3, 24,25,26,27, 0,1,2,3, 0,1,2,3} == (x, z, x, x) -- decoded off
//     scratch/postfx_step9_final/envfix/work/image.bin by the CrashNavMap slice and
//     recorded at BrnCrashNavMap.cpp:430.
//     (⚠️ SIDE NOTE for whoever next touches GameSource/Math/BrnMathUtils.cpp: its
//     "FLAG (Flatten lane mask): unk_82CDA450 is an un-valued .rdata blob" is now STALE --
//     the mask has been read, and it confirms the XZ choice that file inferred. Not edited
//     here because BrnMath::Flatten is not this wave's TU.)
//
// The z/w lanes are written as 0, following this tree's Vector2 convention (WorldToDevice
// above, BrnMath::Flatten, EventIconManager::GetEventIconPositions all do the same). The
// console's mask duplicates .x into those two padding lanes instead; nothing in the
// recovered set reads them, and the SDK Vector2 vocabulary (Dot / Magnitude) is x/y only.
Vector2 MapTransform::Flatten( Vector3 lv3In )
{
    Vector2 lv2Out;
    lv2Out.x = lv3In.x;
    lv2Out.y = lv3In.z;
    lv2Out.z = 0.0f;
    lv2Out.w = 0.0f;
    return lv2Out;
}

// The inverse of the pick above: the map's y goes back into the world Z lane and the world
// height is zero. Pinned by its consumers -- CrashNavMap::UpdateMapCentre
// (BrnCrashNavMap_wJ_07.cpp:204) feeds Unflatten's result straight into WorldToDevice, whose
// own body reads .x and .z, so anything but (x, 0, y) would lose the map's second axis.
Vector3 MapTransform::Unflatten( Vector2 lv2In )
{
    Vector3 lv3Out;
    lv3Out.x = lv2In.x;
    lv3Out.y = 0.0f;
    lv3Out.z = lv2In.y;
    lv3Out.w = 0.0f;
    return lv3Out;
}

// (X360 inlines the 16-byte store to @0x82FB36A0 -- GuiModule::Construct's HD/SD pick
// and the debug component's sliders.) Install the live sat-nav viewport rect.
void MapTransform::SetSatNavRect( Vector4 lv4Rect )
{
    smv4SatNavViewRect = lv4Rect;
}

// @ 0x824BAB78 — map a device-space (screen) point back into world space.
// FLAG (pre-existing semantic reconstruction, unchanged this slice): the X360 body
// reads the ZOOMED matrices (@0x82FB3140 + @0x82FB32E0 per the image xrefs), i.e. it
// undoes the zoomed chain; this body still routes through the static device/world
// spaces. No mounted TU calls it yet -- re-derive against the asm when the crash-nav
// cursor slice (its real consumer) lands.
Vector3 MapTransform::DeviceToWorld( Vector2 lv2Device )
{
    const Matrix33 lm33DeviceToWorld = MakeTransform( smm33DeviceSpace, smm33WorldSpace );
    const Vector2  lv2World = Transform( lv2Device, lm33DeviceToWorld );

    Vector3 lv3Out;
    lv3Out.x = lv2World.x;
    lv3Out.y = lv2World.y;
    lv3Out.z = 0.0f;
    lv3Out.w = 0.0f;
    return lv3Out;
}

// @ 0x8244F318 — BrnGui::MapTransform::CalculateZoomFactor.
//
// The world-space zoom scale that makes the axis-aligned box spanned by lv2A/lv2B fit a
// viewport whose normalised width/height are lv2C.x / lv2C.y, at aspect lfBase. Callers:
// PreRaceFlyByState::CalculateZoomFactor @0x824BE8F0, CrashNavMap::CalculateEventZoomFactor
// @0x824BF4B0, OnlineSelectRoute::CalculateZoomFactor @0x8248F398,
// GuiNetworkRouteInfo::CalculateZoomFactor @0x8242C6C0.
//
// The asm is pure VMX + four scalar fdivs; every operand is decoded:
//   vminfp/vmaxfp v1,v2                 -> the axis-aligned box of the two points
//   splat lanes 0/1, vsubfp             -> lfWidth / lfHeight
//   lvx unk_82FB30C0                    -> smv4DeviceRect {0, 0, 1280, 720}; lane 2 (z) and
//                                          lane 3 (w) are the two vrefp+2xNewton reciprocals
//   flt_820550C0 = 0.85f                -> the usable fraction of the viewport rect
//   flt_82001C98 = 1.0f, fdivs by f1    -> 1.0f / lfBase, applied to the X axis ONLY
//   flt_820550BC = 2500.0f              -> the fsel floor on the result
//   two fsel pairs                      -> max(x, y) then max(that, 2500)
// Every constant is read from the image (scratch/postfx_step9_final/envfix/work/image.bin,
// file offset = VA - 0x82000000, big-endian): 451C4000 = 2500.0f, 3F59999A = 0.85f,
// 3F800000 = 1.0f. None is invented.
//
// ⭐ THE X/Y ASYMMETRY IS THE CONSOLE'S, NOT A TRANSCRIPTION SLIP. The 1/lfBase factor
// multiplies the X ratio and NOT the Y ratio -- `lfs f13, flt_82001C98 ; fdivs f13,f13,f1 ;
// stfs f13, var_30` feeds only the v7 that the X path multiplies by at 0x8244F418, while the
// Y path at 0x8244F46C divides the raw height and goes straight into the fsel. lfBase is the
// aspect ratio (MainMapComponent::SetZoom passes 16/9 == flt_82F25AD4 into the same family
// of maths), so this is the aspect correction on the horizontal axis alone.
f32 MapTransform::CalculateZoomFactor( Vector2 lv2A, Vector2 lv2B, Vector2 lv2C, f32 lfBase )
{
    // vminfp / vmaxfp, then the lane-0 / lane-1 splats and subtracts.
    const f32 lfMinX = ( lv2A.x < lv2B.x ) ? lv2A.x : lv2B.x;
    const f32 lfMinY = ( lv2A.y < lv2B.y ) ? lv2A.y : lv2B.y;
    const f32 lfMaxX = ( lv2A.x > lv2B.x ) ? lv2A.x : lv2B.x;
    const f32 lfMaxY = ( lv2A.y > lv2B.y ) ? lv2A.y : lv2B.y;

    const f32 lfWidth  = lfMaxX - lfMinX;
    const f32 lfHeight = lfMaxY - lfMinY;

    // flt_820550C0 / flt_820550BC / flt_82001C98 -- see the banner.
    const f32 KF_VIEWPORT_USABLE_FRACTION = 0.85f;
    const f32 KF_MIN_ZOOM_FACTOR          = 2500.0f;
    const f32 KF_ONE                      = 1.0f;

    // smv4DeviceRect lanes 2/3 (1280 / 720); the console forms both reciprocals with
    // vrefp + two Newton-Raphson refinements, which is the C++ divide's value.
    const f32 lfDeviceWidth  = smv4DeviceRect.z;
    const f32 lfDeviceHeight = smv4DeviceRect.w;

    // The usable viewport extents, back in device units.
    const f32 lfViewportX = ( lv2C.x * KF_VIEWPORT_USABLE_FRACTION ) / lfDeviceWidth;
    const f32 lfViewportY = ( lv2C.y * KF_VIEWPORT_USABLE_FRACTION ) / lfDeviceHeight;

    const f32 lfAspectScale = KF_ONE / lfBase;                 // X axis only (see banner)
    const f32 lfZoomX       = lfAspectScale * ( lfWidth / lfViewportX );
    const f32 lfZoomY       = lfHeight / lfViewportY;

    // `fsubs f12,f0,f13 ; fsel f0,f12,f0,f13` twice -- max(x, y) then max(that, 2500).
    const f32 lfLarger = ( lfZoomX - lfZoomY >= 0.0f ) ? lfZoomX : lfZoomY;
    return ( lfLarger - KF_MIN_ZOOM_FACTOR >= 0.0f ) ? lfLarger : KF_MIN_ZOOM_FACTOR;
}

} // namespace BrnGui
