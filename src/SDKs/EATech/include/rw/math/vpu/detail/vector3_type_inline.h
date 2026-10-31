#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/detail/vector3_type_inline.h -- the out-of-class inline bodies for the
// Vector3 construction / component-access surface declared in vector3.h.
//
// On the X360 every body here is one or more AltiVec/VMX128 swizzle/permute intrinsics
// on the `vector float` register `VectorIntrinsic mV`. Those intrinsics do not exist on
// PC, so -- per the project's vendor-SIMD rule and exactly as the committed VecFloat lane
// wrappers (vec_float_type_inline.h) do -- each is lowered to the equivalent portable lane
// math with semantic parity (members by name, no raw offset hacks). The console intrinsic
// each body replaces is named in a comment.
//
// Lane facts of the originals (confirmed against the Feb-2007 source):
//   * VectorIntrinsicUnion u; u.scalarFloat[i] = f; mV = u.vector  -> write lane i.
//   * The Set(VecFloat x,y,z) / ctor "_asmCreateVectorAxis<Sx,Sy,Sz>(xV,yV,zV)" assemble
//     is a vperm fold that takes lane Sx of xV into lane 0, Sy of yV into lane 1,
//     Sz of zV into lane 2 (lane 3 untouched). A VecFloat argument is a broadcast, so
//     every lane already equals its X lane; the assemble collapses to three lane reads.
//   * SetComponent / SetX/Y/Z(VecFloat) go through gSwizzleStoreConstants / a
//     VectorPermuteConstant whose only net effect is to copy the source's X lane into one
//     destination lane while preserving the other three -- a single lane write.
//   * SetZero() = vspltisw(0): all four lanes 0.
//   * The X()/Y()/Z() accessors return a VecFloatRef<AXIS> bound to mV (no math).

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  Set(scalar / VecFloat / lane-ref) -- assemble three components into mV
// =====================================================================================

inline void Vector3::Set(float x, float y, float z)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..2] = x,y,z; mV = u.vector;
    mV.v[0] = x;
    mV.v[1] = y;
    mV.v[2] = z;
}

inline void Vector3::Set(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z)
{
    // vperm assemble: lane0 <- x lane X, lane1 <- y lane Y, lane2 <- z lane Z.
    // x/y/z are broadcasts so lane Y/Z each equal lane X.
    mV.v[0] = x.mV.v[VectorAxisX];
    mV.v[1] = y.mV.v[VectorAxisY];
    mV.v[2] = z.mV.v[VectorAxisZ];
}

inline void Vector3::Set(VecFloatRefIndex::InParam x,
                         VecFloatRefIndex::InParam y,
                         VecFloatRefIndex::InParam z)
{
    // Each arg: vspltw(lvsl) broadcasts its referenced lane, then the same vperm assemble.
    mV.v[0] = x.mV.v[x.mIndex];
    mV.v[1] = y.mV.v[y.mIndex];
    mV.v[2] = z.mV.v[z.mIndex];
}

template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX>
inline void Vector3::Set(VecFloatRef<XSRC_INDEX> x,
                         VecFloatRef<YSRC_INDEX> y,
                         VecFloatRef<ZSRC_INDEX> z)
{
    // vperm assemble: lane0 <- x lane XSRC, lane1 <- y lane YSRC, lane2 <- z lane ZSRC.
    mV.v[0] = x.mV.v[XSRC_INDEX];
    mV.v[1] = y.mV.v[YSRC_INDEX];
    mV.v[2] = z.mV.v[ZSRC_INDEX];
}

// =====================================================================================
//  SetComponent / SetX / SetY / SetZ -- single-lane writes
// =====================================================================================

inline void Vector3::SetComponent(int i, float value)
{
    // VectorIntrinsicUnion temp; temp.vector = mV; temp.scalarFloat[i] = value; mV = temp.vector;
    mV.v[i] = value;
}

inline void Vector3::SetComponent(int i, VecFloat::InParam value)
{
    // vperm gSwizzleStoreConstants[i*4 + X]: copy value X lane into mV lane i.
    mV.v[i] = value.mV.v[VectorAxisX];
}

inline void Vector3::SetComponent(int i, VecFloatRefIndex::InParam value)
{
    // vperm gSwizzleStoreConstants[i*4 + value.mIndex]: copy value lane mIndex into mV lane i.
    mV.v[i] = value.mV.v[value.mIndex];
}

template<VectorAxis INDEX>
inline void Vector3::SetComponent(int i, VecFloatRef<INDEX> value)
{
    // vperm gSwizzleStoreConstants[i*4 + INDEX]: copy value lane INDEX into mV lane i.
    mV.v[i] = value.mV.v[INDEX];
}

inline void Vector3::SetX(float x)
{
    SetComponent(0, x);
}

inline void Vector3::SetX(VecFloat::InParam x)
{
    // VectorPermuteConstant: mV lane X <- x lane X (other lanes preserved).
    mV.v[VectorAxisX] = x.mV.v[VectorAxisX];
}

inline void Vector3::SetX(VecFloatRefIndex::InParam x)
{
    // vperm gSwizzleStoreConstants[X*4 + x.mIndex].
    mV.v[VectorAxisX] = x.mV.v[x.mIndex];
}

template<VectorAxis INDEX>
inline void Vector3::SetX(VecFloatRef<INDEX> x)
{
    mV.v[VectorAxisX] = x.mV.v[INDEX];
}

inline void Vector3::SetY(float y)
{
    SetComponent(1, y);
}

inline void Vector3::SetY(VecFloat::InParam y)
{
    mV.v[VectorAxisY] = y.mV.v[VectorAxisX];
}

inline void Vector3::SetY(VecFloatRefIndex::InParam y)
{
    mV.v[VectorAxisY] = y.mV.v[y.mIndex];
}

template<VectorAxis INDEX>
inline void Vector3::SetY(VecFloatRef<INDEX> y)
{
    mV.v[VectorAxisY] = y.mV.v[INDEX];
}

inline void Vector3::SetZ(float z)
{
    SetComponent(2, z);
}

inline void Vector3::SetZ(VecFloat::InParam z)
{
    mV.v[VectorAxisZ] = z.mV.v[VectorAxisX];
}

inline void Vector3::SetZ(VecFloatRefIndex::InParam z)
{
    mV.v[VectorAxisZ] = z.mV.v[z.mIndex];
}

template<VectorAxis INDEX>
inline void Vector3::SetZ(VecFloatRef<INDEX> z)
{
    mV.v[VectorAxisZ] = z.mV.v[INDEX];
}

inline void Vector3::SetZero()
{
    // vspltisw(0): all four lanes 0.
    mV.v[0] = 0.0f;
    mV.v[1] = 0.0f;
    mV.v[2] = 0.0f;
    mV.v[3] = 0.0f;
}

// =====================================================================================
//  Constructors
// =====================================================================================

inline Vector3::Vector3(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z)
{
    // mV(_asmCreateVectorAxis<X,Y,Z>(x.mV, y.mV, z.mV)): lane0<-x.X, lane1<-y.Y, lane2<-z.Z.
    mV.v[0] = x.mV.v[VectorAxisX];
    mV.v[1] = y.mV.v[VectorAxisY];
    mV.v[2] = z.mV.v[VectorAxisZ];
}

inline Vector3::Vector3(VecFloatRefIndex::InParam x,
                        VecFloatRefIndex::InParam y,
                        VecFloatRefIndex::InParam z)
{
    // broadcast each referenced lane then assemble lane0/1/2.
    mV.v[0] = x.mV.v[x.mIndex];
    mV.v[1] = y.mV.v[y.mIndex];
    mV.v[2] = z.mV.v[z.mIndex];
}

template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX>
inline Vector3::Vector3(VecFloatRef<XSRC_INDEX> x,
                        VecFloatRef<YSRC_INDEX> y,
                        VecFloatRef<ZSRC_INDEX> z)
{
    // mV(_asmCreateVectorAxis<XSRC,YSRC,ZSRC>(x.mV, y.mV, z.mV)).
    mV.v[0] = x.mV.v[XSRC_INDEX];
    mV.v[1] = y.mV.v[YSRC_INDEX];
    mV.v[2] = z.mV.v[ZSRC_INDEX];
}

template<class T>
inline Vector3::Vector3(const fpu::Vector3Template<T>& v)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..2] = (float)v.x,y,z; mV = u.vector;
    mV.mafLane[0] = static_cast<float>(v.x);
    mV.mafLane[1] = static_cast<float>(v.y);
    mV.mafLane[2] = static_cast<float>(v.z);
}

inline Vector3::Vector3()
{
    // uninitialised on the console (no body); leave mV uninitialised.
}

inline Vector3::Vector3(const Vector3& rhs)
{
    mV = rhs.mV;
}

inline Vector3::Vector3(float x, float y, float z)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..2] = x,y,z; mV = u.vector;
    mV.v[0] = x;
    mV.v[1] = y;
    mV.v[2] = z;
}

inline Vector3::Vector3(const float xyz[3])
{
    // mV(lvx(xyz)): load 16 bytes from xyz. Only [0..2] are defined input; lane 3 is the
    // 4th float at that address on the console -- read the three defined components.
    mV.v[0] = xyz[0];
    mV.v[1] = xyz[1];
    mV.v[2] = xyz[2];
}

inline Vector3::Vector3(VectorIntrinsicInParam v)
{
    mV = v;
}

// =====================================================================================
//  Assignment / register access
// =====================================================================================

inline Vector3& Vector3::operator =(Vector3::InParam v)
{
    mV = v.mV;
    return *this;
}

inline Vector3& Vector3::operator =(VectorIntrinsicInParam v)
{
    mV = v;
    return *this;
}

inline Vector3::operator VectorIntrinsic&()
{
    return mV;
}

inline Vector3::operator const VectorIntrinsic&() const
{
    return mV;
}

inline void Vector3::SetVector(VectorIntrinsicInParam v)
{
    mV = v;
}

inline VectorIntrinsic& Vector3::GetVector()
{
    return mV;
}

inline const VectorIntrinsic& Vector3::GetVector() const
{
    return mV;
}

// =====================================================================================
//  Lane-reference accessors (no math; bind a VecFloatRef to mV)
// =====================================================================================

inline VecFloatRefX Vector3::X() { return VecFloatRefX(mV); }
inline const VecFloatRefX Vector3::X() const { return VecFloatRefX(const_cast<Vector3*>(this)->mV); }
inline VecFloatRefY Vector3::Y() { return VecFloatRefY(mV); }
inline const VecFloatRefY Vector3::Y() const { return VecFloatRefY(const_cast<Vector3*>(this)->mV); }
inline VecFloatRefZ Vector3::Z() { return VecFloatRefZ(mV); }
inline const VecFloatRefZ Vector3::Z() const { return VecFloatRefZ(const_cast<Vector3*>(this)->mV); }

inline VecFloatRefX Vector3::GetX() { return VecFloatRefX(mV); }
inline const VecFloatRefX Vector3::GetX() const { return VecFloatRefX(const_cast<Vector3*>(this)->mV); }
inline VecFloatRefY Vector3::GetY() { return VecFloatRefY(mV); }
inline const VecFloatRefY Vector3::GetY() const { return VecFloatRefY(const_cast<Vector3*>(this)->mV); }
inline VecFloatRefZ Vector3::GetZ() { return VecFloatRefZ(mV); }
inline const VecFloatRefZ Vector3::GetZ() const { return VecFloatRefZ(const_cast<Vector3*>(this)->mV); }

inline VecFloatRefIndex Vector3::GetComponent(int i)
{
    return VecFloatRefIndex(mV, i);
}

inline const VecFloatRefIndex Vector3::GetComponent(int i) const
{
    return VecFloatRefIndex(const_cast<Vector3*>(this)->mV, i);
}

inline VecFloatRefIndex Vector3::operator [](int i)
{
    return VecFloatRefIndex(mV, i);
}

inline const VecFloatRefIndex Vector3::operator [](int i) const
{
    return VecFloatRefIndex(const_cast<Vector3*>(this)->mV, i);
}

}
}
}
