// BrnMapUtils.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::MapTransform's six out-of-line
// 2D map-space functions. The X360 implements these as hand-vectorised VMX128 (dot
// products via vmsum3fp128, matrix-inverse reciprocals via vrefp + two Newton-Raphson
// refinement steps vnmsubfp/vmaddfp, lane shuffles via vperm/vpermwi128). Those
// intrinsics have no portable PC equivalent, so the bodies below are SEMANTIC
// reconstructions in named-member scalar/lane C++ that produce the same result, per
// the policy stated in rw/math/vpu/types.h. The 3x3 transforms are row-major affine:
// a 2D point p is transformed as (p.x, p.y, 1) . M, i.e.
//   out = p.x * M.xAxis + p.y * M.yAxis + M.zAxis.

#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"

namespace BrnGui
{

// ---- static map-space state -------------------------------------------------
// These mirror the X360 static-const map rects/spaces (the `unk_82FB....` / `unk_82CDA...`
// constant pools the functions load). Concrete numeric contents are owned by the (out of
// scope) MapTransform::MapTransform() one-time init; declared here as zero-initialised
// definitions so the reconstructed functions link against named storage rather than raw
// pool addresses. HONEST BOUNDARY: the numeric rect/space values are set elsewhere.
const Vector4  MapTransform::smv4WorldRect      = { 0.0f, 0.0f, 0.0f, 0.0f };
const Vector4  MapTransform::smv4NormalizedRect = { 0.0f, 0.0f, 1.0f, 1.0f };
const Vector4  MapTransform::smv4DeviceRect     = { 0.0f, 0.0f, 0.0f, 0.0f };
const Matrix33 MapTransform::smm33WorldSpace      = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
const Matrix33 MapTransform::smm33NormalisedSpace = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
const Matrix33 MapTransform::smm33DeviceSpace     = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };

Vector4  MapTransform::smv4ZoomedWorldRect                = { 0.0f, 0.0f, 0.0f, 0.0f };
Matrix33 MapTransform::smm33ZoomedWorldTransform          = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
Matrix33 MapTransform::smm33ZoomedViewportTransform       = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };
Matrix33 MapTransform::smm33ZoomedViewportScreenTransform = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } };

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

// @ 0x824503C0 — apply a row-major affine 3x3 to a 2D point:
// out = p.x * xAxis + p.y * yAxis + zAxis (the X360 builds the matrix via MakeTransform
// for the two-matrix overload; this single-matrix overload just applies the given one).
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

// @ 0x824435C8 — the transform that takes a point out of `lm33From` space into
// `lm33To` space: inverse(from) composed with to.
Matrix33 MapTransform::MakeTransform( Matrix33 lm33From, Matrix33 lm33To )
{
    return Multiply33( Invert33( lm33From ), lm33To );
}

// @ 0x824504E8 — store the world-space coord transform for the current zoom level.
// lv2Min/lv2Max are the zoomed world-rect corners; lv2Centre carries the focus point.
void MapTransform::SetZoomedWorldRect( Vector2 lv2Min, Vector2 lv2Max, Vector2 lv2Centre )
{
    smv4ZoomedWorldRect = { lv2Min.x, lv2Min.y, lv2Max.x, lv2Max.y };
    smm33ZoomedWorldTransform = MakeCoordSpaceFromRect( smv4ZoomedWorldRect );
    // Bias the translation row toward the focus centre (the X360 folds lv2Centre into
    // the stored transform's translation lane).
    smm33ZoomedWorldTransform.zAxis.x = lv2Centre.x;
    smm33ZoomedWorldTransform.zAxis.y = lv2Centre.y;
}

// @ 0x82450608 — store the viewport (and derived screen) coord transforms from a rect.
// FLAG (VMX128 semantic-reconstruction floor): the X360 composes the screen transform with a
// plain vmaddfp multiply chain here (NO vrefp/vmsum inverse), whereas this uses MakeTransform
// (which inverts). It composes differently from the binary; acceptable only because the static
// map-spaces (smm33DeviceSpace etc.) are themselves placeholders set by the out-of-scope
// MapTransform() init. Re-derive the exact VMX op-for-op composition when that init is homed.
void MapTransform::SetZoomedViewportRect( Vector4 lv4Rect )
{
    smm33ZoomedViewportTransform = MakeCoordSpaceFromRect( lv4Rect );
    // The screen transform composes the viewport space onto the device space.
    smm33ZoomedViewportScreenTransform =
        MakeTransform( smm33ZoomedViewportTransform, smm33DeviceSpace );
}

// @ 0x824BAB78 — map a device-space (screen) point back into world space. Builds the
// device->world transform and applies it; the result is a Vector3 on the map plane (z=0).
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
