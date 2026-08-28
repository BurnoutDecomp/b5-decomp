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

} // namespace BrnGui
