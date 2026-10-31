// BrnMapUtils.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::MapTransform's out-of-line
// 2D map-space functions. MakeTransform, MakeCoordSpaceFromRect and the matrix
// Transform overloads retain the original VMX operations and all four row lanes.
// Other scalar helpers below retain their existing reconstruction and need separate
// native verification. The 3x3 transforms are row-major affine:
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
        { const rw::math::vpu::VectorIntrinsic lvRow = {
            lm33Lhs.xAxis.mV.x * lm33Rhs.xAxis.mV.x + lm33Lhs.xAxis.mV.y * lm33Rhs.yAxis.mV.x,
            lm33Lhs.xAxis.mV.x * lm33Rhs.xAxis.mV.y + lm33Lhs.xAxis.mV.y * lm33Rhs.yAxis.mV.y,
            0.0f, 0.0f }; lm33Out.xAxis.mV = lvRow; }
        { const rw::math::vpu::VectorIntrinsic lvRow = {
            lm33Lhs.yAxis.mV.x * lm33Rhs.xAxis.mV.x + lm33Lhs.yAxis.mV.y * lm33Rhs.yAxis.mV.x,
            lm33Lhs.yAxis.mV.x * lm33Rhs.xAxis.mV.y + lm33Lhs.yAxis.mV.y * lm33Rhs.yAxis.mV.y,
            0.0f, 0.0f }; lm33Out.yAxis.mV = lvRow; }
        { const rw::math::vpu::VectorIntrinsic lvRow = {
            lm33Lhs.zAxis.mV.x * lm33Rhs.xAxis.mV.x + lm33Lhs.zAxis.mV.y * lm33Rhs.yAxis.mV.x + lm33Rhs.zAxis.mV.x,
            lm33Lhs.zAxis.mV.x * lm33Rhs.xAxis.mV.y + lm33Lhs.zAxis.mV.y * lm33Rhs.yAxis.mV.y + lm33Rhs.zAxis.mV.y,
            1.0f, 0.0f }; lm33Out.zAxis.mV = lvRow; }
        return lm33Out;
    }

    // Existing scalar 2x2 affine reconstruction. The original uses a full3x3 inverse
    // and VMX reciprocal refinements; this helper remains a separate parity debt.
    Matrix33 Invert33( const Matrix33& lm33In )
    {
        const f32 lfA = lm33In.xAxis.mV.x, lfB = lm33In.xAxis.mV.y;
        const f32 lfC = lm33In.yAxis.mV.x, lfD = lm33In.yAxis.mV.y;
        const f32 lfTx = lm33In.zAxis.mV.x, lfTy = lm33In.zAxis.mV.y;

        const f32 lfDet = lfA * lfD - lfB * lfC;
        const f32 lfInvDet = ( lfDet != 0.0f ) ? ( 1.0f / lfDet ) : 0.0f;

        Matrix33 lm33Out;
        { const rw::math::vpu::VectorIntrinsic lvRow = {  lfD * lfInvDet, -lfB * lfInvDet, 0.0f, 0.0f }; lm33Out.xAxis.mV = lvRow; }
        { const rw::math::vpu::VectorIntrinsic lvRow = { -lfC * lfInvDet,  lfA * lfInvDet, 0.0f, 0.0f }; lm33Out.yAxis.mV = lvRow; }
        // Inverse translation: -T . inverse(linear).
        { const rw::math::vpu::VectorIntrinsic lvRow = {
            -( lfTx * lm33Out.xAxis.mV.x + lfTy * lm33Out.yAxis.mV.x ),
            -( lfTx * lm33Out.xAxis.mV.y + lfTy * lm33Out.yAxis.mV.y ),
            1.0f, 0.0f }; lm33Out.zAxis.mV = lvRow; }
        return lm33Out;
    }
}

// ---- static map-space state -------------------------------------------------
// ARTIST registered callbacks82C52010..82C52530. Preserve the original definition
// order: each rectangle is initialized immediately before its coordinate space.
const Vector4  MapTransform::smv4WorldRect(-4375.419921875f, -5842.419921875f, 5363.14990234375f, 3904.739990234375f); // 0xC588BB5C/0xC5B6935C/0x45A79933/0x45740BD7
const Matrix33 MapTransform::smm33WorldSpace      = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4WorldRect );
const Vector4  MapTransform::smv4NormalizedRect(0.0f, 0.0f, 1.0f, 1.0f);
const Matrix33 MapTransform::smm33NormalisedSpace = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4NormalizedRect );
const Vector4  MapTransform::smv4DeviceRect(0.0f, 0.0f, 1280.0f, 720.0f);
const Matrix33 MapTransform::smm33DeviceSpace     = MapTransform::MakeCoordSpaceFromRect( MapTransform::smv4DeviceRect );

// Authored HD82FB30A0 and SD82FB3130 values: all128bits are written by the original
// registered callbacks82C522B0 and82C522F8, including the equal Y components.
const Vector4 MapTransform::smv4SatNavViewRectHD(0.778124988079071f, 0.6652777791023254f, 0.9312499761581421f, 0.8666666746139526f);
const Vector4 MapTransform::smv4SatNavViewRectSD(0.750781238079071f, 0.6652777791023254f, 0.9039062261581421f, 0.8666666746139526f);

// The live sat-nav viewport rect (@0x82FB36A0). Default = the HD rect @0x82FB30A0
// (the cinit copy thunk @0x82C52340); GuiModule::Construct re-installs the HD/SD pick
// through SetSatNavRect by its high-definition argument (issue #11).
Vector4 MapTransform::smv4SatNavViewRect = MapTransform::smv4SatNavViewRectHD;

Vector4 MapTransform::smv4ZoomedWorldRect; // Native default construction performs no store.
Matrix33 MapTransform::smm33ZoomedWorldTransform = MapTransform::MakeCoordSpaceFromRect(MapTransform::smv4NormalizedRect);
Matrix33 MapTransform::smm33ZoomedViewportTransform = MapTransform::MakeCoordSpaceFromRect(MapTransform::smv4NormalizedRect);
Matrix33 MapTransform::smm33ZoomedViewportScreenTransform = MapTransform::MakeCoordSpaceFromRect(MapTransform::smv4NormalizedRect);

// @ 0x82450460 — build the coord-space matrix that maps the unit square onto lv4Rect
// (lv4Rect = minX, minY, maxX, maxY). Scale = (maxX-minX, maxY-minY), translate = (minX, minY).
Matrix33 MapTransform::MakeCoordSpaceFromRect( Vector4 lv4Rect )
{
    // ARTIST82450460..E0 retains the otherwise-unused fourth row lanes.
    static const CGS_ALIGN(16) u32 lauXYXX[4] = {0x00010203, 0x14151617, 0x00010203, 0x00010203};
    const __vector4 lvGather = __lvx(lauXYXX, 0);
    const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lvZero = __vspltisw(0);
    const __vector4 lvWidth = __vsubfp(__vspltw(lv4Rect.mV, 2), __vspltw(lv4Rect.mV, 0));
    const __vector4 lvHeight = __vsubfp(__vspltw(lv4Rect.mV, 3), __vspltw(lv4Rect.mV, 1));
    __vector4 lvOrigin = __vperm(lv4Rect.mV, lv4Rect.mV, lvGather);
    lvOrigin = __vperm(__vspltw(lvOrigin, 0), __vspltw(lvOrigin, 1), lvGather);
    lvOrigin = __vrlimi(lvOrigin, lvOne, 2, 0);
    __vector4 lvX = __vperm(lvWidth, lvZero, lvGather);
    __vector4 lvY = __vperm(lvZero, lvHeight, lvGather);
    lvX = __vperm(__vspltw(lvX, 0), __vspltw(lvX, 1), lvGather);
    lvY = __vperm(__vspltw(lvY, 0), __vspltw(lvY, 1), lvGather);
    lvX = __vrlimi(lvX, lvZero, 2, 0);
    lvY = __vrlimi(lvY, lvZero, 2, 0);
    return Matrix33(lvX, lvY, lvOrigin);
}

// (X360 inlines this; the layout is attested by SetZoomedWorldRect @0x824504E8 and
// SatNavRenderer::RenderComponent @0x82465EC0, which both assemble exactly these rows.)
// The coord space with origin lv2Origin whose unit X lands on lv2XPoint and unit Y on
// lv2YPoint: xAxis = X - origin, yAxis = Y - origin.
Matrix33 MapTransform::MakeCoordSpaceFromPoints( Vector2 lv2Origin, Vector2 lv2XPoint, Vector2 lv2YPoint )
{
    Matrix33 lm33Out;
    { const rw::math::vpu::VectorIntrinsic lvRow = { lv2XPoint.mV.x - lv2Origin.mV.x, lv2XPoint.mV.y - lv2Origin.mV.y, 0.0f, 0.0f }; lm33Out.xAxis.mV = lvRow; }
    { const rw::math::vpu::VectorIntrinsic lvRow = { lv2YPoint.mV.x - lv2Origin.mV.x, lv2YPoint.mV.y - lv2Origin.mV.y, 0.0f, 0.0f }; lm33Out.yAxis.mV = lvRow; }
    { const rw::math::vpu::VectorIntrinsic lvRow = { lv2Origin.mV.x,               lv2Origin.mV.y,               1.0f, 0.0f }; lm33Out.zAxis.mV = lvRow; }
    return lm33Out;
}

// (X360 inlines the one-matrix apply at every call site.) Apply a row-major affine 3x3
// to a 2D point: out = p.x * xAxis + p.y * yAxis + zAxis.
Vector2 MapTransform::Transform( Vector2 lv2Point, Matrix33 lm33Transform )
{
    static const CGS_ALIGN(16) u32 lauXYXX[4] = {0x00010203, 0x14151617, 0x00010203, 0x00010203};
    const __vector4 lvGather = __lvx(lauXYXX, 0);
    __vector4 lvPoint = __vperm(__vspltw(lv2Point.mV, 0), __vspltw(lv2Point.mV, 1), lvGather);
    lvPoint = __vrlimi(lvPoint, __vcfsx(__vspltisw(1), 0), 2, 0);
    __vector4 lvValue = __vmulfp(lm33Transform.xAxis.mV, __vspltw(lvPoint, 0));
    lvValue = __vmaddfp(lm33Transform.yAxis.mV, __vspltw(lvPoint, 1), lvValue);
    lvValue = __vmaddfp(lm33Transform.zAxis.mV, __vspltw(lvPoint, 2), lvValue);
    Vector2 lvResult;
    lvResult.mV = __vperm(lvValue, lvValue, lvGather);
    return lvResult;
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
    // ARTIST824435C8..6CC: full3x3 cofactors, reciprocal refinements and fused rows.
    const __vector4 lvX = lm33From.xAxis.mV;
    const __vector4 lvY = lm33From.yAxis.mV;
    const __vector4 lvZ = lm33From.zAxis.mV;
    const __vector4 lvXyz = __vpermwi(lvX, 0x63);
    const __vector4 lvYyz = __vpermwi(lvY, 0x63);
    const __vector4 lvZyz = __vpermwi(lvZ, 0x63);
    const __vector4 lvCofY = __vpermwi(__vnmsubfp(lvZyz, lvX, __vmulfp(lvZ, lvXyz)), 0x63);
    const __vector4 lvCofX = __vpermwi(__vnmsubfp(lvYyz, lvZ, __vmulfp(lvY, lvZyz)), 0x63);
    const __vector4 lvCofZ = __vpermwi(__vnmsubfp(lvXyz, lvY, __vmulfp(lvX, lvYyz)), 0x63);
    const __vector4 lvYHi = __vmrghw(lvCofY, lvCofY);
    const __vector4 lvXZHi = __vmrghw(lvCofX, lvCofZ);
    const __vector4 lvXZLo = __vmrglw(lvCofX, lvCofZ);
    const __vector4 lvYLo = __vmrglw(lvCofY, lvCofY);
    const __vector4 lvRow0 = __vmrghw(lvXZHi, lvYHi);
    const __vector4 lvRow1 = __vmrglw(lvXZHi, lvYHi);
    const __vector4 lvRow2 = __vmrghw(lvXZLo, lvYLo);
    const __vector4 lvDet = __vspltw(__vmsum3fp(lvX, lvCofX), 0);
    const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
    __vector4 lvReciprocal = __vrefp(lvDet);
    lvReciprocal = __vmaddfp(lvReciprocal, __vnmsubfp(lvReciprocal, lvDet, lvOne), lvReciprocal);
    lvReciprocal = __vmaddfp(lvReciprocal, __vnmsubfp(lvReciprocal, lvDet, lvOne), lvReciprocal);
    const __vector4 lvInv0 = __vmulfp(lvRow0, lvReciprocal);
    const __vector4 lvInv1 = __vmulfp(lvRow1, lvReciprocal);
    const __vector4 lvInv2 = __vmulfp(lvRow2, lvReciprocal);
    __vector4 lvOut0 = __vmulfp(lm33To.xAxis.mV, __vspltw(lvInv0, 0));
    lvOut0 = __vmaddfp(lm33To.yAxis.mV, __vspltw(lvInv0, 1), lvOut0);
    lvOut0 = __vmaddfp(lm33To.zAxis.mV, __vspltw(lvInv0, 2), lvOut0);
    __vector4 lvOut1 = __vmulfp(lm33To.xAxis.mV, __vspltw(lvInv1, 0));
    lvOut1 = __vmaddfp(lm33To.yAxis.mV, __vspltw(lvInv1, 1), lvOut1);
    lvOut1 = __vmaddfp(lm33To.zAxis.mV, __vspltw(lvInv1, 2), lvOut1);
    __vector4 lvOut2 = __vmulfp(lm33To.xAxis.mV, __vspltw(lvInv2, 0));
    lvOut2 = __vmaddfp(lm33To.yAxis.mV, __vspltw(lvInv2, 1), lvOut2);
    lvOut2 = __vmaddfp(lm33To.zAxis.mV, __vspltw(lvInv2, 2), lvOut2);
    return Matrix33(lvOut0, lvOut1, lvOut2);
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
    static const CGS_ALIGN(16) u32 lauXYXX[4] = {0x00010203, 0x14151617, 0x00010203, 0x00010203};
    static const CGS_ALIGN(16) u32 lauXXXY[4] = {0x00010203, 0x00010203, 0x00010203, 0x14151617};
    smv4ZoomedWorldRect.mV = __vsldoi(
        __vperm(lv2CornerC.mV, lv2CornerC.mV, __lvx(lauXXXY, 0)),
        __vperm(lv2CornerB.mV, lv2CornerB.mV, __lvx(lauXYXX, 0)), 8);
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
    lv2World.mV.x = lv3World.mV.x;
    lv2World.mV.y = lv3World.mV.z;
    lv2World.mV.z = 0.0f;
    lv2World.mV.w = 0.0f;

    Vector2 lv2Unit = Transform( lv2World, smm33ZoomedWorldTransform );

    if ( lbClamp )
    {
        f32 lfX = lv2Unit.mV.x;
        f32 lfY = lv2Unit.mV.y;

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
        lv2Unit.mV.x = ( lfX < 0.0f ) ? 0.0f : ( lfX > 1.0f ? 1.0f : lfX );
        lv2Unit.mV.y = ( lfY < 0.0f ) ? 0.0f : ( lfY > 1.0f ? 1.0f : lfY );
    }

    Vector2 lv2Out = Transform( lv2Unit, smm33ZoomedViewportScreenTransform );
    lv2Out.mV.z = 0.0f;
    lv2Out.mV.w = 0.0f;
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
    lv2Out.mV.x = lv3In.mV.x;
    lv2Out.mV.y = lv3In.mV.z;
    lv2Out.mV.z = 0.0f;
    lv2Out.mV.w = 0.0f;
    return lv2Out;
}

// The inverse of the pick above: the map's y goes back into the world Z lane and the world
// height is zero. Pinned by its consumers -- CrashNavMap::UpdateMapCentre
// (BrnCrashNavMap_wJ_07.cpp:204) feeds Unflatten's result straight into WorldToDevice, whose
// own body reads .x and .z, so anything but (x, 0, y) would lose the map's second axis.
Vector3 MapTransform::Unflatten( Vector2 lv2In )
{
    Vector3 lv3Out;
    lv3Out.mV.x = lv2In.mV.x;
    lv3Out.mV.y = 0.0f;
    lv3Out.mV.z = lv2In.mV.y;
    lv3Out.mV.w = 0.0f;
    return lv3Out;
}

// (X360 inlines the 16-byte store to @0x82FB36A0 -- GuiModule::Construct's HD/SD pick
// and the debug component's sliders.) Install the live sat-nav viewport rect.
void MapTransform::SetSatNavRect( Vector4 lv4Rect )
{
    smv4SatNavViewRect = lv4Rect;
}

// @ 0x824BAB78 — map a device-space (screen) point back into world space: the exact
// inverse of WorldToDevice's zoomed chain. The X360 body loads the two ZOOMED matrices --
// smm33ZoomedViewportScreenTransform @0x82FB3140 first, smm33ZoomedWorldTransform
// @0x82FB32E0 second -- and inverts each in place (the adjugate rows through the
// vpermwi128 0x63 / vnmsubfp pairs, the determinant through vmsum3fp128, its reciprocal
// through vrefp plus two Newton steps), then applies device -> zoomed unit -> world. The
// map's second axis is returned in the world Z lane, which is what Flatten / WorldToDevice
// read back (and what the vperm through unk_82CDA350 at the tail lays out).
//
// RE-DERIVED 2026-09-10: the previous body mapped through the STATIC device/world spaces
// (the whole-map rect), so CrashNavMap::UpdateMainMap scrolled the map toward a bogus
// cursor world position and the view drifted off the player to the map's own edge.
Vector3 MapTransform::DeviceToWorld( Vector2 lv2Device )
{
    const Vector2 lv2Unit  = Transform( lv2Device, Invert33( smm33ZoomedViewportScreenTransform ) );
    const Vector2 lv2World = Transform( lv2Unit,   Invert33( smm33ZoomedWorldTransform ) );

    Vector3 lv3Out;
    lv3Out.mV.x = lv2World.mV.x;
    lv3Out.mV.y = 0.0f;
    lv3Out.mV.z = lv2World.mV.y;
    lv3Out.mV.w = 0.0f;
    return lv3Out;
}

// @ 0x8244F318 — BrnGui::MapTransform::CalculateZoomFactor.
//
// The world-space zoom scale that makes the axis-aligned box spanned by lv2A/lv2B fit a
// viewport whose normalised width/height are lv2C.mV.x / lv2C.mV.y, at aspect lfBase. Callers:
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
    const f32 lfMinX = ( lv2A.mV.x < lv2B.mV.x ) ? lv2A.mV.x : lv2B.mV.x;
    const f32 lfMinY = ( lv2A.mV.y < lv2B.mV.y ) ? lv2A.mV.y : lv2B.mV.y;
    const f32 lfMaxX = ( lv2A.mV.x > lv2B.mV.x ) ? lv2A.mV.x : lv2B.mV.x;
    const f32 lfMaxY = ( lv2A.mV.y > lv2B.mV.y ) ? lv2A.mV.y : lv2B.mV.y;

    const f32 lfWidth  = lfMaxX - lfMinX;
    const f32 lfHeight = lfMaxY - lfMinY;

    // flt_820550C0 / flt_820550BC / flt_82001C98 -- see the banner.
    const f32 KF_VIEWPORT_USABLE_FRACTION = 0.85f;
    const f32 KF_MIN_ZOOM_FACTOR          = 2500.0f;
    const f32 KF_ONE                      = 1.0f;

    // smv4DeviceRect lanes 2/3 (1280 / 720); the console forms both reciprocals with
    // vrefp + two Newton-Raphson refinements, which is the C++ divide's value.
    const f32 lfDeviceWidth  = smv4DeviceRect.mV.z;
    const f32 lfDeviceHeight = smv4DeviceRect.mV.w;

    // The usable viewport extents, back in device units.
    const f32 lfViewportX = ( lv2C.mV.x * KF_VIEWPORT_USABLE_FRACTION ) / lfDeviceWidth;
    const f32 lfViewportY = ( lv2C.mV.y * KF_VIEWPORT_USABLE_FRACTION ) / lfDeviceHeight;

    const f32 lfAspectScale = KF_ONE / lfBase;                 // X axis only (see banner)
    const f32 lfZoomX       = lfAspectScale * ( lfWidth / lfViewportX );
    const f32 lfZoomY       = lfHeight / lfViewportY;

    // `fsubs f12,f0,f13 ; fsel f0,f12,f0,f13` twice -- max(x, y) then max(that, 2500).
    const f32 lfLarger = ( lfZoomX - lfZoomY >= 0.0f ) ? lfZoomX : lfZoomY;
    return ( lfLarger - KF_MIN_ZOOM_FACTOR >= 0.0f ) ? lfLarger : KF_MIN_ZOOM_FACTOR;
}

} // namespace BrnGui

namespace BrnGui
{
// ARTIST 0x824581F0. Each failed all-lanes comparison branches to true (0x82458364).
// Preserve those early-outs, including their disjunction, rather than correcting the game.
bool MapTransform::IsWithinViewport(Vector3 lv3World, f32 lfMarginX, f32 lfMarginY)
{
    const Vector2 lv2World(lv3World.mV.x, lv3World.mV.z);
    const Vector2 lv2Unit = Transform(lv2World, smm33ZoomedWorldTransform);
    return !(-lfMarginX > lv2Unit.mV.x) || !(lv2Unit.mV.x > lfMarginX + 1.0f) ||
        !(-lfMarginY > lv2Unit.mV.y) || !(lv2Unit.mV.y > lfMarginY + 1.0f);
}
}
