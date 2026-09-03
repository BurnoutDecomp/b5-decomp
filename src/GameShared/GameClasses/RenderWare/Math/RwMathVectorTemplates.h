#pragma once

// The single-precision LANE (vpu) vocabulary, for Matrix44Template's widening constructor from
// rw::math::vpu::Matrix44 (DWARF matrix44.h:111). vendor/renderware/include is on every
// translation unit's include path and rw/math/vpu/types.h is a leaf header (no further includes).
#include "rw/math/vpu/types.h"

namespace rw::math::fpu
{
template <typename Type>
class Vector3Template;

template <typename Type>
bool IsValid(const Vector3Template<Type>& lrVector);

template <typename Type>
class Vector2Template
{
public:
    // Additive grow (RouteMapModule embed): a trivial default ctor so the type can be
    // a value member of a default-constructible aggregate (BrnAI::AStarNode embeds a
    // Vector2Template<float>, and BrnAI::RouteMapModule embeds an AStar by value). The
    // X360 builds these records without running any per-lane init (the nodes are
    // constructed lazily by AStarNodePool::NewNode), so leaving the lanes uninitialised
    // is faithful. Layout is unchanged (mX@+0, mY@+4; no vtable).
    Vector2Template() {}
    Vector2Template(Type lX, Type lY) : mX(lX), mY(lY) {}   // (defined inline 2026-09-03: the two-lane init the X360 emits as two stfs; was declared-only -> LNK2019)

    // Additive grow (World-AI group): the canonical rwmath component readers.
    // BrnAStar's distance heuristics read x/y from a Vector2Template<float> &;
    // on X360 these are direct loads of the two leading lanes (lfs 0(rN)/4(rN)).
    // Inline accessors keep member access by-name and do not change the layout
    // (mX at +0, mY at +4 preserved).
    Type X() const { return mX; }
    Type Y() const { return mY; }

private:
    Type mX;
    Type mY;
};

template <typename Type>
class Vector3Template
{
public:
    Vector3Template(Type lX, Type lY, Type lZ);

    // Additive grow (StreetManager keystone, wave B): a trivial default ctor so the
    // type can be a value member of a Construct()-initialised aggregate
    // (BrnGameState::StreetManager embeds four Vector3Template<float> at X360
    // +0x1D40..+0x1D6F; BrnGameState::SectionWalkData embeds two). The X360 runs no
    // per-lane init on those embeds (StreetManager::Construct calls SetZero
    // explicitly), so uninitialised lanes are faithful. Layout unchanged
    // (mX@+0, mY@+4, mZ@+8; no vtable).
    Vector3Template() {}

    // Additive grow (StreetManager keystone, wave B): component readers on the
    // Vector2Template precedent above, plus the SetZero the DecFIGS DWARF attests
    // (StreetManager::Construct hint calls rw::math::fpu::Vector3Template<float>::
    // SetZero four times) and a whole-vector setter for the X360's three-lane
    // stfs store idiom. Layout unchanged.
    Type X() const { return mX; }
    Type Y() const { return mY; }
    Type Z() const { return mZ; }
    void Set(Type lX, Type lY, Type lZ) { mX = lX; mY = lY; mZ = lZ; }
    void SetZero() { mX = static_cast<Type>(0); mY = static_cast<Type>(0); mZ = static_cast<Type>(0); }

private:
    Type mX;
    Type mY;
    Type mZ;

    // Additive grow (Im2dTransform keystone group): by-name access to the three
    // lanes for the @0x8231A5C8 self-equality validity test; layout unchanged.
    friend bool IsValid<Type>(const Vector3Template<Type>& lrVector);
};

template <typename Type>
class Vector4Template
{
public:
    // DWARF vector4.h:58 -- the four-lane constructor (defined here: the boost-bar TU's
    // KV4_FPU_BOOSTBAR_RECT static is built through it, and a declaration-only template
    // member has no other TU to live in).
    Vector4Template(Type lX, Type lY, Type lZ, Type lW) : mX(lX), mY(lY), mZ(lZ), mW(lW) {}

    // Additive grow (post-fx reprojection wave). The DecFIGS DWARF for this type
    // (dwarfdump/SDKs/EATech/include/rw/math/fpu/vector4.h) declares a default constructor
    // (vector4.h:57), `void Set(Type,Type,Type,Type)` (vector4.h:103) and the per-lane readers;
    // Matrix44Template below embeds four of these BY VALUE, and BrnPostFxShader::Render's
    // reprojection block builds its two constant screen-space matrices with Set. The reader
    // spelling follows this file's own Vector2Template/Vector3Template precedent above.
    // Layout unchanged (mX@+0, mY@+8, mZ@+16, mW@+24 for Type == double; no vtable).
    Vector4Template() {}
    void Set(Type lX, Type lY, Type lZ, Type lW) { mX = lX; mY = lY; mZ = lZ; mW = lW; }
    Type X() const { return mX; }
    Type Y() const { return mY; }
    Type Z() const { return mZ; }
    Type W() const { return mW; }

private:
    Type mX;
    Type mY;
    Type mZ;
    Type mW;
};

// DWARF vector4.h:159/:160 -- the double-precision spellings the fpu matrix family returns.
typedef Vector4Template<double> Vector4_64;
typedef Vector4Template<double> Vector4U_64;

// Additive grow (Im2dTransform keystone group): the scalar (fpu) 3x3 matrix of the
// rwmath template family - nine Type lanes stored row-major as three Vector3-sized
// rows (mRow0/1/2 each {x,y,z}), matching the contiguous nine-float aggregate the X360
// `rw::math::fpu::Mult<float>` reads (a2[0..8]/a3[0..8] @0x823C24F0) and the per-row
// `IsValid<float>` validation walks at +0/+0xC/+0x18 (CgsIm2dTransform.cpp:61).
template <typename Type>
struct Matrix33Template
{
    Type mRow0X, mRow0Y, mRow0Z;
    Type mRow1X, mRow1Y, mRow1Z;
    Type mRow2X, mRow2Y, mRow2Z;
};

// X360 0x8231A5C8: rw::math::fpu::IsValid<float>(Vector3Template<float> const&). Per-lane
// self-equality NaN test: `lfs f0,N(r3); fcmpu f0,f0; bne` for N in {0,4,8} (mX/mY/mZ),
// short-circuiting to the false sink the moment a lane fails. Returns true iff every lane
// equals itself (x==x && y==y && z==z), i.e. none of the three lanes is NaN.
template <typename Type>
inline bool IsValid(const Vector3Template<Type>& lrVector)
{
    return lrVector.mX == lrVector.mX
        && lrVector.mY == lrVector.mY
        && lrVector.mZ == lrVector.mZ;
}

// X360 0x823C24F0: rw::math::fpu::Mult<float>(Matrix33Template<float> const& lLhs,
// Matrix33Template<float> const& lRhs) -> Matrix33Template<float>. Faithful row-major
// 3x3 multiply (result[i][j] = sum_k lLhs[i][k] * lRhs[k][j]); the operand-order of each
// row's three fused products matches the asm's vmaddfp accumulation sequence exactly.
template <typename Type>
inline Matrix33Template<Type> Mult(const Matrix33Template<Type>& lLhs,
                                   const Matrix33Template<Type>& lRhs)
{
    Matrix33Template<Type> lResult;
    lResult.mRow0X = (lLhs.mRow0X * lRhs.mRow0X) + ((lLhs.mRow0Y * lRhs.mRow1X) + (lLhs.mRow0Z * lRhs.mRow2X));
    lResult.mRow0Y = (lRhs.mRow1Y * lLhs.mRow0Y) + ((lLhs.mRow0Z * lRhs.mRow2Y) + (lRhs.mRow0Y * lLhs.mRow0X));
    lResult.mRow0Z = (lRhs.mRow1Z * lLhs.mRow0Y) + ((lLhs.mRow0Z * lRhs.mRow2Z) + (lRhs.mRow0Z * lLhs.mRow0X));
    lResult.mRow1X = (lRhs.mRow0X * lLhs.mRow1X) + ((lRhs.mRow2X * lLhs.mRow1Z) + (lLhs.mRow1Y * lRhs.mRow1X));
    lResult.mRow1Y = (lRhs.mRow2Y * lLhs.mRow1Z) + ((lLhs.mRow1Y * lRhs.mRow1Y) + (lRhs.mRow0Y * lLhs.mRow1X));
    lResult.mRow1Z = (lRhs.mRow2Z * lLhs.mRow1Z) + ((lLhs.mRow1Y * lRhs.mRow1Z) + (lRhs.mRow0Z * lLhs.mRow1X));
    lResult.mRow2X = (lLhs.mRow2X * lRhs.mRow0X) + ((lRhs.mRow1X * lLhs.mRow2Y) + (lRhs.mRow2X * lLhs.mRow2Z));
    lResult.mRow2Y = (lRhs.mRow2Y * lLhs.mRow2Z) + ((lRhs.mRow1Y * lLhs.mRow2Y) + (lLhs.mRow2X * lRhs.mRow0Y));
    lResult.mRow2Z = (lRhs.mRow2Z * lLhs.mRow2Z) + ((lRhs.mRow1Z * lLhs.mRow2Y) + (lLhs.mRow2X * lRhs.mRow0Z));
    return lResult;
}

// ==================================================================================================
// ADDITIVE GROW (post-fx step-6 "producers" wave): rw::math::fpu::Matrix44Template and its
// operation vocabulary -- the SCALAR, DOUBLE-PRECISION 4x4 family.
//
// WHY IT LANDS HERE AND NOT IN A NEW FILE. The DWARF homes this type in
// dwarfdump/SDKs/EATech/include/rw/math/fpu/matrix44.h (members `Vector4Template<double> xAxis /
// yAxis / zAxis / wAxis`, matrix44.h:331-334) and its free functions in
// .../fpu/matrix44_operation.h. This tree already homes the WHOLE rw::math::fpu template family in
// THIS header -- Vector2Template / Vector3Template / Vector4Template / Matrix33Template and
// fpu::Mult(Matrix33Template) are all above -- so adding a second home would be the fork
// AGENTS.md forbids. When an SDKs/EATech/include/rw/math/fpu/ mirror is created, this block moves
// there whole; nothing here is Burnout-specific.
//
// THE ONE ATTESTED CONSUMER is BrnPostFxShader::Render @0x82408F08, whose camera-reprojection
// block calls, out of line and by these exact mangled names:
//   rw::math::fpu::Inverse<double>(const Matrix44Template<double>&, double&)   @0x82405210
//   rw::math::fpu::Mult<double>(const Matrix44Template<double>&, const Matrix44Template<double>&)
//                                                                             @0x82405690
//   rw::math::fpu::Subtract<double>(const Matrix44Template<double>&, const Matrix44Template<double>&)
//                                                                             @0x82405780
//   rw::math::fpu::Mult<double>(const Matrix44Template<double>&, double)      @0x824058A0
// (the four `bl` targets at 0x82409468 / 0x82409478 / 0x8240956C / 0x824095BC / 0x824095CC).
//
// CONVENTION: row-major, ROW-VECTOR. `Mult(a, b)` is `a * b`, i.e. "apply a, then b" -- pinned by
// the call chain itself, which composes lScreenToProjection * lInverseCurrentWVP (screen -> clip ->
// world) and then * (prevWVP * lProjectionToScreen) (world -> clip -> screen).
// ==================================================================================================

// DWARF matrix44.h:17/331-334. Four Vector4Template rows.
template <typename Type>
struct Matrix44Template
{
    Matrix44Template() {}
    Matrix44Template(const Vector4Template<Type>& lX, const Vector4Template<Type>& lY,
                     const Vector4Template<Type>& lZ, const Vector4Template<Type>& lW)
        : xAxis(lX), yAxis(lY), zAxis(lZ), wAxis(lW) {}

    // DWARF matrix44.h:111 -- the widening constructor from the 128-bit-lane single-precision
    // rw::math::vpu::Matrix44. The X360 open-codes it as sixteen `lfs`/`stfd` pairs at the one
    // call site (0x824092FC.. and 0x8240948C..), which is exactly this element-by-element widen.
    explicit Matrix44Template(const rw::math::vpu::Matrix44& lrMatrix)
    {
        xAxis.Set(static_cast<Type>(lrMatrix.xAxis.x), static_cast<Type>(lrMatrix.xAxis.y),
                  static_cast<Type>(lrMatrix.xAxis.z), static_cast<Type>(lrMatrix.xAxis.w));
        yAxis.Set(static_cast<Type>(lrMatrix.yAxis.x), static_cast<Type>(lrMatrix.yAxis.y),
                  static_cast<Type>(lrMatrix.yAxis.z), static_cast<Type>(lrMatrix.yAxis.w));
        zAxis.Set(static_cast<Type>(lrMatrix.zAxis.x), static_cast<Type>(lrMatrix.zAxis.y),
                  static_cast<Type>(lrMatrix.zAxis.z), static_cast<Type>(lrMatrix.zAxis.w));
        wAxis.Set(static_cast<Type>(lrMatrix.wAxis.x), static_cast<Type>(lrMatrix.wAxis.y),
                  static_cast<Type>(lrMatrix.wAxis.z), static_cast<Type>(lrMatrix.wAxis.w));
    }

    // DWARF matrix44.h:161 / :210 / :252-267 / :183.
    void SetIdentity()
    {
        const Type lfZero = static_cast<Type>(0);
        const Type lfOne  = static_cast<Type>(1);
        xAxis.Set(lfOne,  lfZero, lfZero, lfZero);
        yAxis.Set(lfZero, lfOne,  lfZero, lfZero);
        zAxis.Set(lfZero, lfZero, lfOne,  lfZero);
        wAxis.Set(lfZero, lfZero, lfZero, lfOne);
    }

    const Vector4Template<Type>& GetRow(int liRow) const
    {
        return (liRow == 0) ? xAxis : (liRow == 1) ? yAxis : (liRow == 2) ? zAxis : wAxis;
    }
    Type GetElem(int liRow, int liColumn) const
    {
        const Vector4Template<Type>& lrRow = GetRow(liRow);
        return (liColumn == 0) ? lrRow.X() : (liColumn == 1) ? lrRow.Y()
             : (liColumn == 2) ? lrRow.Z() : lrRow.W();
    }

    // The four COLUMN readers (matrix44.h:252/257/262/267). BrnPostFxShader::Render narrows
    // columns 0, 1 and 3 of the refined velocity matrix into BlurMatrixX / BlurMatrixY /
    // BlurMatrixW -- the shader's per-pixel dot is `dot((u, v, depth, 1), column)`, which is why
    // the CPU hands it COLUMNS of a row-vector matrix and not rows.
    Vector4Template<Type> GetXColumn() const
    { Vector4Template<Type> lR; lR.Set(xAxis.X(), yAxis.X(), zAxis.X(), wAxis.X()); return lR; }
    Vector4Template<Type> GetYColumn() const
    { Vector4Template<Type> lR; lR.Set(xAxis.Y(), yAxis.Y(), zAxis.Y(), wAxis.Y()); return lR; }
    Vector4Template<Type> GetZColumn() const
    { Vector4Template<Type> lR; lR.Set(xAxis.Z(), yAxis.Z(), zAxis.Z(), wAxis.Z()); return lR; }
    Vector4Template<Type> GetWColumn() const
    { Vector4Template<Type> lR; lR.Set(xAxis.W(), yAxis.W(), zAxis.W(), wAxis.W()); return lR; }

    Vector4Template<Type> xAxis;   // DWARF matrix44.h:331
    Vector4Template<Type> yAxis;   // DWARF matrix44.h:332
    Vector4Template<Type> zAxis;   // DWARF matrix44.h:333
    Vector4Template<Type> wAxis;   // DWARF matrix44.h:334
};

// DWARF matrix44.h:339.
typedef Matrix44Template<double> Matrix44_64;

// FLAG (home): GetMatrix44_IdentityTemplate / GetMatrix44_64_Identity are DWARF-homed in
// rw/math/fpu/constants_operation.h (:336 / :1086), which has no mirror in this tree. They sit
// here with the type they return rather than in a third file; move them with the block above the
// day a constants_operation.h mirror lands. BrnPostFxShader::Render calls
// GetMatrix44_64_Identity twice (the DWARF hint listing for that function names it twice), which
// is why the two screen-space constant matrices below start from the identity and overwrite only
// three rows each -- their zero/one off-diagonals are the identity's, not separate stores.
template <typename Type>
inline Matrix44Template<Type> GetMatrix44_IdentityTemplate()
{
    Matrix44Template<Type> lResult;
    lResult.SetIdentity();
    return lResult;
}
inline Matrix44_64 GetMatrix44_64_Identity() { return GetMatrix44_IdentityTemplate<double>(); }

// Mult(a, b) == a * b, row-vector convention: out.row_i = sum_k a[i][k] * b.row_k.
template <typename Type>
inline Matrix44Template<Type> Mult(const Matrix44Template<Type>& lLhs,
                                   const Matrix44Template<Type>& lRhs)
{
    Matrix44Template<Type> lResult;
    for (int liRow = 0; liRow < 4; ++liRow)
    {
        const Vector4Template<Type>& lrSource = lLhs.GetRow(liRow);
        Vector4Template<Type> lRow;
        lRow.Set(lrSource.X() * lRhs.xAxis.X() + lrSource.Y() * lRhs.yAxis.X()
               + lrSource.Z() * lRhs.zAxis.X() + lrSource.W() * lRhs.wAxis.X(),
                 lrSource.X() * lRhs.xAxis.Y() + lrSource.Y() * lRhs.yAxis.Y()
               + lrSource.Z() * lRhs.zAxis.Y() + lrSource.W() * lRhs.wAxis.Y(),
                 lrSource.X() * lRhs.xAxis.Z() + lrSource.Y() * lRhs.yAxis.Z()
               + lrSource.Z() * lRhs.zAxis.Z() + lrSource.W() * lRhs.wAxis.Z(),
                 lrSource.X() * lRhs.xAxis.W() + lrSource.Y() * lRhs.yAxis.W()
               + lrSource.Z() * lRhs.zAxis.W() + lrSource.W() * lRhs.wAxis.W());
        switch (liRow)
        {
        case 0:  lResult.xAxis = lRow; break;
        case 1:  lResult.yAxis = lRow; break;
        case 2:  lResult.zAxis = lRow; break;
        default: lResult.wAxis = lRow; break;
        }
    }
    return lResult;
}
template <typename Type>
inline Matrix44Template<Type> operator*(const Matrix44Template<Type>& lLhs,
                                        const Matrix44Template<Type>& lRhs)
{
    return Mult(lLhs, lRhs);
}

// Mult(m, scalar) -- every element scaled. BrnPostFxShader::Render uses it on the identity to
// build `I * lScreenToVelocity.wAxis.W()`.
template <typename Type>
inline Matrix44Template<Type> Mult(const Matrix44Template<Type>& lrMatrix, Type lfScalar)
{
    Matrix44Template<Type> lResult;
    lResult.xAxis.Set(lrMatrix.xAxis.X() * lfScalar, lrMatrix.xAxis.Y() * lfScalar,
                      lrMatrix.xAxis.Z() * lfScalar, lrMatrix.xAxis.W() * lfScalar);
    lResult.yAxis.Set(lrMatrix.yAxis.X() * lfScalar, lrMatrix.yAxis.Y() * lfScalar,
                      lrMatrix.yAxis.Z() * lfScalar, lrMatrix.yAxis.W() * lfScalar);
    lResult.zAxis.Set(lrMatrix.zAxis.X() * lfScalar, lrMatrix.zAxis.Y() * lfScalar,
                      lrMatrix.zAxis.Z() * lfScalar, lrMatrix.zAxis.W() * lfScalar);
    lResult.wAxis.Set(lrMatrix.wAxis.X() * lfScalar, lrMatrix.wAxis.Y() * lfScalar,
                      lrMatrix.wAxis.Z() * lfScalar, lrMatrix.wAxis.W() * lfScalar);
    return lResult;
}
template <typename Type>
inline Matrix44Template<Type> operator*(const Matrix44Template<Type>& lrMatrix, Type lfScalar)
{
    return Mult(lrMatrix, lfScalar);
}

// Subtract(a, b) -- element-wise a - b.
template <typename Type>
inline Matrix44Template<Type> Subtract(const Matrix44Template<Type>& lLhs,
                                       const Matrix44Template<Type>& lRhs)
{
    Matrix44Template<Type> lResult;
    lResult.xAxis.Set(lLhs.xAxis.X() - lRhs.xAxis.X(), lLhs.xAxis.Y() - lRhs.xAxis.Y(),
                      lLhs.xAxis.Z() - lRhs.xAxis.Z(), lLhs.xAxis.W() - lRhs.xAxis.W());
    lResult.yAxis.Set(lLhs.yAxis.X() - lRhs.yAxis.X(), lLhs.yAxis.Y() - lRhs.yAxis.Y(),
                      lLhs.yAxis.Z() - lRhs.yAxis.Z(), lLhs.yAxis.W() - lRhs.yAxis.W());
    lResult.zAxis.Set(lLhs.zAxis.X() - lRhs.zAxis.X(), lLhs.zAxis.Y() - lRhs.zAxis.Y(),
                      lLhs.zAxis.Z() - lRhs.zAxis.Z(), lLhs.zAxis.W() - lRhs.zAxis.W());
    lResult.wAxis.Set(lLhs.wAxis.X() - lRhs.wAxis.X(), lLhs.wAxis.Y() - lRhs.wAxis.Y(),
                      lLhs.wAxis.Z() - lRhs.wAxis.Z(), lLhs.wAxis.W() - lRhs.wAxis.W());
    return lResult;
}
template <typename Type>
inline Matrix44Template<Type> operator-(const Matrix44Template<Type>& lLhs,
                                        const Matrix44Template<Type>& lRhs)
{
    return Subtract(lLhs, lRhs);
}

// Determinant / Inverse -- the GENERAL 4x4 (no affine or orthonormal fast path: the matrices this
// family inverts are full world-view-PROJECTION matrices, whose fourth column is not (0,0,0,1)).
// Inverse returns adj(m)/det and publishes the determinant through lrDeterminant, which is the
// same shape the single-precision sibling rw::math::vpu::Inverse @0x825B2628 already carries in
// vendor/renderware/include/rw/math/vpu/matrix44_operation.h.
//
// ZERO-DETERMINANT GUARD: the X360 fpu::Inverse<double> @0x82405210 compares the determinant against
// dbl_82001CA8 (0.0) and returns the ZERO matrix on a singular input (rung-7 verifier corrected the
// earlier "divides unconditionally" reading here); reproduced below. The vpu single-precision Inverse
// (MotionBlurState::Update's four inverses) has no such test on the console and keeps none.
template <typename Type>
inline Matrix44Template<Type> Inverse(const Matrix44Template<Type>& lrMatrix, Type& lrDeterminant)
{
    Type laE[16];
    for (int liRow = 0; liRow < 4; ++liRow)
    {
        for (int liColumn = 0; liColumn < 4; ++liColumn)
        {
            laE[liRow * 4 + liColumn] = lrMatrix.GetElem(liRow, liColumn);
        }
    }

    Type laAdj[16];
    laAdj[0]  =  laE[5]*laE[10]*laE[15] - laE[5]*laE[11]*laE[14] - laE[9]*laE[6]*laE[15]
               + laE[9]*laE[7]*laE[14]  + laE[13]*laE[6]*laE[11] - laE[13]*laE[7]*laE[10];
    laAdj[4]  = -laE[4]*laE[10]*laE[15] + laE[4]*laE[11]*laE[14] + laE[8]*laE[6]*laE[15]
               - laE[8]*laE[7]*laE[14]  - laE[12]*laE[6]*laE[11] + laE[12]*laE[7]*laE[10];
    laAdj[8]  =  laE[4]*laE[9]*laE[15]  - laE[4]*laE[11]*laE[13] - laE[8]*laE[5]*laE[15]
               + laE[8]*laE[7]*laE[13]  + laE[12]*laE[5]*laE[11] - laE[12]*laE[7]*laE[9];
    laAdj[12] = -laE[4]*laE[9]*laE[14]  + laE[4]*laE[10]*laE[13] + laE[8]*laE[5]*laE[14]
               - laE[8]*laE[6]*laE[13]  - laE[12]*laE[5]*laE[10] + laE[12]*laE[6]*laE[9];
    laAdj[1]  = -laE[1]*laE[10]*laE[15] + laE[1]*laE[11]*laE[14] + laE[9]*laE[2]*laE[15]
               - laE[9]*laE[3]*laE[14]  - laE[13]*laE[2]*laE[11] + laE[13]*laE[3]*laE[10];
    laAdj[5]  =  laE[0]*laE[10]*laE[15] - laE[0]*laE[11]*laE[14] - laE[8]*laE[2]*laE[15]
               + laE[8]*laE[3]*laE[14]  + laE[12]*laE[2]*laE[11] - laE[12]*laE[3]*laE[10];
    laAdj[9]  = -laE[0]*laE[9]*laE[15]  + laE[0]*laE[11]*laE[13] + laE[8]*laE[1]*laE[15]
               - laE[8]*laE[3]*laE[13]  - laE[12]*laE[1]*laE[11] + laE[12]*laE[3]*laE[9];
    laAdj[13] =  laE[0]*laE[9]*laE[14]  - laE[0]*laE[10]*laE[13] - laE[8]*laE[1]*laE[14]
               + laE[8]*laE[2]*laE[13]  + laE[12]*laE[1]*laE[10] - laE[12]*laE[2]*laE[9];
    laAdj[2]  =  laE[1]*laE[6]*laE[15]  - laE[1]*laE[7]*laE[14]  - laE[5]*laE[2]*laE[15]
               + laE[5]*laE[3]*laE[14]  + laE[13]*laE[2]*laE[7]  - laE[13]*laE[3]*laE[6];
    laAdj[6]  = -laE[0]*laE[6]*laE[15]  + laE[0]*laE[7]*laE[14]  + laE[4]*laE[2]*laE[15]
               - laE[4]*laE[3]*laE[14]  - laE[12]*laE[2]*laE[7]  + laE[12]*laE[3]*laE[6];
    laAdj[10] =  laE[0]*laE[5]*laE[15]  - laE[0]*laE[7]*laE[13]  - laE[4]*laE[1]*laE[15]
               + laE[4]*laE[3]*laE[13]  + laE[12]*laE[1]*laE[7]  - laE[12]*laE[3]*laE[5];
    laAdj[14] = -laE[0]*laE[5]*laE[14]  + laE[0]*laE[6]*laE[13]  + laE[4]*laE[1]*laE[14]
               - laE[4]*laE[2]*laE[13]  - laE[12]*laE[1]*laE[6]  + laE[12]*laE[2]*laE[5];
    laAdj[3]  = -laE[1]*laE[6]*laE[11]  + laE[1]*laE[7]*laE[10]  + laE[5]*laE[2]*laE[11]
               - laE[5]*laE[3]*laE[10]  - laE[9]*laE[2]*laE[7]   + laE[9]*laE[3]*laE[6];
    laAdj[7]  =  laE[0]*laE[6]*laE[11]  - laE[0]*laE[7]*laE[10]  - laE[4]*laE[2]*laE[11]
               + laE[4]*laE[3]*laE[10]  + laE[8]*laE[2]*laE[7]   - laE[8]*laE[3]*laE[6];
    laAdj[11] = -laE[0]*laE[5]*laE[11]  + laE[0]*laE[7]*laE[9]   + laE[4]*laE[1]*laE[11]
               - laE[4]*laE[3]*laE[9]   - laE[8]*laE[1]*laE[7]   + laE[8]*laE[3]*laE[5];
    laAdj[15] =  laE[0]*laE[5]*laE[10]  - laE[0]*laE[6]*laE[9]   - laE[4]*laE[1]*laE[10]
               + laE[4]*laE[2]*laE[9]   + laE[8]*laE[1]*laE[6]   - laE[8]*laE[2]*laE[5];

    const Type lfDeterminant = laE[0]*laAdj[0] + laE[1]*laAdj[4] + laE[2]*laAdj[8] + laE[3]*laAdj[12];
    lrDeterminant = lfDeterminant;

    // THE CONSOLE'S ZERO-DETERMINANT GUARD (rung-7 verifier): rw::math::fpu::Inverse<double> @0x82405210 does
    // `fcmpu f1, dbl_82001CA8 (0.0)` / `bne 0x824052A0` and on a SINGULAR input returns the ZERO matrix
    // (sixteen `stfd` of 0.0) instead of dividing -- the earlier note here that "the console has no guard"
    // was wrong. Without it a singular WVP (a garbage or degenerate view) would put NaN into
    // BlurMatrixX/Y/W and from there into a tex2Dgrad gradient.
    Matrix44Template<Type> lResult;
    if (lfDeterminant == static_cast<Type>(0))
    {
        lResult.xAxis.Set(0, 0, 0, 0);
        lResult.yAxis.Set(0, 0, 0, 0);
        lResult.zAxis.Set(0, 0, 0, 0);
        lResult.wAxis.Set(0, 0, 0, 0);
        return lResult;
    }

    const Type lfInverseDeterminant = static_cast<Type>(1) / lfDeterminant;
    lResult.xAxis.Set(laAdj[0]  * lfInverseDeterminant, laAdj[1]  * lfInverseDeterminant,
                      laAdj[2]  * lfInverseDeterminant, laAdj[3]  * lfInverseDeterminant);
    lResult.yAxis.Set(laAdj[4]  * lfInverseDeterminant, laAdj[5]  * lfInverseDeterminant,
                      laAdj[6]  * lfInverseDeterminant, laAdj[7]  * lfInverseDeterminant);
    lResult.zAxis.Set(laAdj[8]  * lfInverseDeterminant, laAdj[9]  * lfInverseDeterminant,
                      laAdj[10] * lfInverseDeterminant, laAdj[11] * lfInverseDeterminant);
    lResult.wAxis.Set(laAdj[12] * lfInverseDeterminant, laAdj[13] * lfInverseDeterminant,
                      laAdj[14] * lfInverseDeterminant, laAdj[15] * lfInverseDeterminant);
    return lResult;
}

// DWARF matrix44_operation.h:35 -- the determinant-discarding overload. No X360 call site in the
// post-fx path uses it; declared because the DWARF does and because it is the natural spelling.
template <typename Type>
inline Matrix44Template<Type> Inverse(const Matrix44Template<Type>& lrMatrix)
{
    Type lfDeterminant = static_cast<Type>(0);
    return Inverse(lrMatrix, lfDeterminant);
}

// DWARF matrix44_operation.h:8.
template <typename Type>
inline Type Determinant(const Matrix44Template<Type>& lrMatrix)
{
    Type lfDeterminant = static_cast<Type>(0);
    Inverse(lrMatrix, lfDeterminant);
    return lfDeterminant;
}
}
