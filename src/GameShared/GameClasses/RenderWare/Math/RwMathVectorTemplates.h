#pragma once

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
    Vector2Template(Type lX, Type lY);

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
    Vector4Template(Type lX, Type lY, Type lZ, Type lW);

private:
    Type mX;
    Type mY;
    Type mZ;
    Type mW;
};

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
}
