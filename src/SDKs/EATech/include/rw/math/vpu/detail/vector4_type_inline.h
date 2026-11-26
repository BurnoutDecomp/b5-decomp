#pragma once

// VPU Vector4 construction and component access over the genuine VectorIntrinsic mV.
// Native scalar/raw constructors and implicit copy are object-bound separately.
// Existing lane/ref/pointer/template overloads below retain their source surface;
// broader swizzle/NaN behavior requires its own native caller evidence.

#include "SDKs/EATech/include/rw/math/vpu/vector4.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  Set(scalar / VecFloat / lane-ref) -- assemble four components into mV
// =====================================================================================

inline void Vector4::Set(float x, float y, float z, float w)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..3] = x,y,z,w; mV = u.vector;
    mV.v[0] = x;
    mV.v[1] = y;
    mV.v[2] = z;
    mV.v[3] = w;
}

inline void Vector4::Set(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z, VecFloat::InParam w)
{
    // 2x vperm + vsldoi assemble: lane0<-x.X, lane1<-y.Y, lane2<-z.Z, lane3<-w.W.
    mV.v[0] = x.mV.v[VectorAxisX];
    mV.v[1] = y.mV.v[VectorAxisY];
    mV.v[2] = z.mV.v[VectorAxisZ];
    mV.v[3] = w.mV.v[VectorAxisW];
}

inline void Vector4::Set(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y,
                         VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam w)
{
    // broadcast each referenced lane then the same assemble.
    mV.v[0] = x.mV.v[x.mIndex];
    mV.v[1] = y.mV.v[y.mIndex];
    mV.v[2] = z.mV.v[z.mIndex];
    mV.v[3] = w.mV.v[w.mIndex];
}

template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX, VectorAxis WSRC_INDEX>
inline void Vector4::Set(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y,
                         VecFloatRef<ZSRC_INDEX> z, VecFloatRef<WSRC_INDEX> w)
{
    mV.v[0] = x.mV.v[XSRC_INDEX];
    mV.v[1] = y.mV.v[YSRC_INDEX];
    mV.v[2] = z.mV.v[ZSRC_INDEX];
    mV.v[3] = w.mV.v[WSRC_INDEX];
}

// =====================================================================================
//  SetComponent / SetX / SetY / SetZ / SetW -- single-lane writes
// =====================================================================================
// rwmath 1.02.00 writes the single-lane setters as the swizzle-store macros (LegacySrc
// vector4_type_inline.h); their Xenon bodies are those of the VecFloatRef / VecFloatRefIndex stores
// (detail/vec_float_type_inline.h): a constant lane from a VecFloat or VecFloatRef<INDEX> is the vrlimi128 lane
// insert, mask 8 >> D, rotate (S - D) & 3, the source lane of a VecFloat being D itself (LegacySrc) (SetY(VecFloat) in AIDebugComponent::DrawAggressiveWarning
// 0x82773CF8: vrlimi 4, 0; Vector4::SetX(VecFloat) in VehicleAttribs::SetupAttribsForDonutAI 0x825F62E8: 8, 0),
// a runtime lane permutes with gSwizzleStoreConstants[D * 4 + S] (0x8327F140).

// DecFIGS vector4_type_inline.h:59 declares the local VectorIntrinsicUnion temp (h:62), as
// rwmath 1.02.00 writes it: copy the register out, replace one lane, copy it back. ARTIST
// keeps that round trip through a stack slot (FlaptTimerFieldComponent::Construct
// 0x8241C89C, RoadRuleComponent::Construct 0x8242AE78).
inline void Vector4::SetComponent(int i, float value)
{
    detail::VectorIntrinsicUnion temp;
    temp.vector = mV;
    temp.scalarFloat[i] = value;
    mV = temp.vector;
}

inline void Vector4::SetComponent(int i, VecFloat::InParam value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (VectorAxisX))]); };
}

inline void Vector4::SetComponent(int i, VecFloatRefIndex::InParam value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (value.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector4::SetComponent(int i, VecFloatRef<INDEX> value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (INDEX))]); };
}

inline void Vector4::SetX(float x)
{
    SetComponent(0, x);
}

inline void Vector4::SetX(VecFloat::InParam x)
{
    { mV = __vrlimi(mV, x.mV, (0x8 >> (VectorAxisX)), (((int)(VectorAxisX)) - (VectorAxisX)) & 3); };
}

inline void Vector4::SetX(VecFloatRefIndex::InParam x)
{
    { mV = __vperm(mV, x.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisX) * 4 ) + (x.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector4::SetX(VecFloatRef<INDEX> x)
{
    { mV = __vrlimi(mV, x.mV, (0x8 >> (VectorAxisX)), (((int)(INDEX)) - (VectorAxisX)) & 3); };
}

inline void Vector4::SetY(float y)
{
    SetComponent(1, y);
}

inline void Vector4::SetY(VecFloat::InParam y)
{
    { mV = __vrlimi(mV, y.mV, (0x8 >> (VectorAxisY)), (((int)(VectorAxisY)) - (VectorAxisY)) & 3); };
}

inline void Vector4::SetY(VecFloatRefIndex::InParam y)
{
    { mV = __vperm(mV, y.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisY) * 4 ) + (y.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector4::SetY(VecFloatRef<INDEX> y)
{
    { mV = __vrlimi(mV, y.mV, (0x8 >> (VectorAxisY)), (((int)(INDEX)) - (VectorAxisY)) & 3); };
}

inline void Vector4::SetZ(float z)
{
    SetComponent(2, z);
}

inline void Vector4::SetZ(VecFloat::InParam z)
{
    { mV = __vrlimi(mV, z.mV, (0x8 >> (VectorAxisZ)), (((int)(VectorAxisZ)) - (VectorAxisZ)) & 3); };
}

inline void Vector4::SetZ(VecFloatRefIndex::InParam z)
{
    { mV = __vperm(mV, z.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisZ) * 4 ) + (z.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector4::SetZ(VecFloatRef<INDEX> z)
{
    { mV = __vrlimi(mV, z.mV, (0x8 >> (VectorAxisZ)), (((int)(INDEX)) - (VectorAxisZ)) & 3); };
}

inline void Vector4::SetW(float w)
{
    SetComponent(3, w);
}

inline void Vector4::SetW(VecFloat::InParam w)
{
    { mV = __vrlimi(mV, w.mV, (0x8 >> (VectorAxisW)), (((int)(VectorAxisW)) - (VectorAxisW)) & 3); };
}

inline void Vector4::SetW(VecFloatRefIndex::InParam w)
{
    { mV = __vperm(mV, w.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisW) * 4 ) + (w.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector4::SetW(VecFloatRef<INDEX> w)
{
    { mV = __vrlimi(mV, w.mV, (0x8 >> (VectorAxisW)), (((int)(INDEX)) - (VectorAxisW)) & 3); };
}

inline void Vector4::SetZero()
{
    // One splatted zero register for all four lanes: ARTIST materialises it once with
    // vspltisw and stores it whole (BehaviourDebugFlyWorld::Prepare 0x821FB73C / 0x821FB784).
    mV = __vspltisw(0);
}

// =====================================================================================
//  Constructors
// =====================================================================================

inline Vector4::Vector4(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z, VecFloat::InParam w)
{
    // mV(_asmCreateVectorAxis<X,Y,Z,W>(x.mV,y.mV,z.mV,w.mV)).
    mV.v[0] = x.mV.v[VectorAxisX];
    mV.v[1] = y.mV.v[VectorAxisY];
    mV.v[2] = z.mV.v[VectorAxisZ];
    mV.v[3] = w.mV.v[VectorAxisW];
}

inline Vector4::Vector4(VecFloatRefIndex::InParam x, VecFloatRefIndex::InParam y,
                        VecFloatRefIndex::InParam z, VecFloatRefIndex::InParam w)
{
    mV.v[0] = x.mV.v[x.mIndex];
    mV.v[1] = y.mV.v[y.mIndex];
    mV.v[2] = z.mV.v[z.mIndex];
    mV.v[3] = w.mV.v[w.mIndex];
}

template<VectorAxis XSRC_INDEX, VectorAxis YSRC_INDEX, VectorAxis ZSRC_INDEX, VectorAxis WSRC_INDEX>
inline Vector4::Vector4(VecFloatRef<XSRC_INDEX> x, VecFloatRef<YSRC_INDEX> y,
                        VecFloatRef<ZSRC_INDEX> z, VecFloatRef<WSRC_INDEX> w)
{
    mV.v[0] = x.mV.v[XSRC_INDEX];
    mV.v[1] = y.mV.v[YSRC_INDEX];
    mV.v[2] = z.mV.v[ZSRC_INDEX];
    mV.v[3] = w.mV.v[WSRC_INDEX];
}

inline Vector4::Vector4(Vector3::InParam v3, VecFloatRefW::InParam w)
{
    // mV(v3.mV); then VectorPermuteConstant copies w lane W into lane W.
    mV = v3.mV;
    mV.v[VectorAxisW] = w.mV.v[VectorAxisW];
}

inline Vector4::Vector4(Vector3::InParam v3, VecFloatRefIndex::InParam w)
{
    // mV(v3.mV); then vperm gSwizzleStoreConstants[W*4 + w.mIndex].
    mV = v3.mV;
    mV.v[VectorAxisW] = w.mV.v[w.mIndex];
}

inline Vector4::Vector4(Vector3::InParam v3, VecFloat::InParam w)
{
    // mV(v3.mV); then VectorPermuteConstant copies w X lane into lane W.
    mV = v3.mV;
    mV.v[VectorAxisW] = w.mV.v[VectorAxisX];
}

inline Vector4::Vector4(Vector3::InParam v3, const float w)
{
    // mV(v3.mV); SetComponent(3, w).
    mV = v3.mV;
    SetComponent(3, w);
}

template<class T>
inline Vector4::Vector4(const fpu::Vector4Template<T>& v)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..3] = (float)v.X(),Y(),Z(),W(); mV = u.vector;
    mV.mafLane[0] = static_cast<float>(v.X());
    mV.mafLane[1] = static_cast<float>(v.Y());
    mV.mafLane[2] = static_cast<float>(v.Z());
    mV.mafLane[3] = static_cast<float>(v.W());
}

inline Vector4::Vector4()
{
    // uninitialised on the console (no body).
}

inline Vector4::Vector4(float x, float y, float z, float w)
{
    // ARTIST821F0DD8/821F0E00: scalar inputs assembled, then one raw row copy.
    VectorIntrinsic lValue;
    lValue.v[0] = x;
    lValue.v[1] = y;
    lValue.v[2] = z;
    lValue.v[3] = w;
    mV = lValue;
}

inline Vector4::Vector4(const float xyzw[4])
{
    // mV(lvx(xyzw)): load 16 bytes.
    mV.v[0] = xyzw[0];
    mV.v[1] = xyzw[1];
    mV.v[2] = xyzw[2];
    mV.v[3] = xyzw[3];
}

inline Vector4::Vector4(VectorIntrinsicInParam v)
{
    mV = v;
}


// =====================================================================================
//  Assignment / register access
// =====================================================================================


inline Vector4& Vector4::operator =(VectorIntrinsicInParam v)
{
    mV = v;
    return *this;
}

inline Vector4::operator VectorIntrinsic&()
{
    return mV;
}

inline Vector4::operator const VectorIntrinsic&() const
{
    return mV;
}

inline void Vector4::SetVector(VectorIntrinsicInParam v)
{
    mV = v;
}

inline VectorIntrinsic& Vector4::GetVector()
{
    return mV;
}

inline const VectorIntrinsic& Vector4::GetVector() const
{
    return mV;
}

// =====================================================================================
//  Lane-reference accessors
// =====================================================================================

inline VecFloatRefX Vector4::X() { return VecFloatRefX(mV); }
inline const VecFloatRefX Vector4::X() const { return VecFloatRefX(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefY Vector4::Y() { return VecFloatRefY(mV); }
inline const VecFloatRefY Vector4::Y() const { return VecFloatRefY(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefZ Vector4::Z() { return VecFloatRefZ(mV); }
inline const VecFloatRefZ Vector4::Z() const { return VecFloatRefZ(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefW Vector4::W() { return VecFloatRefW(mV); }
inline const VecFloatRefW Vector4::W() const { return VecFloatRefW(const_cast<Vector4*>(this)->mV); }

inline VecFloatRefX Vector4::GetX() { return VecFloatRefX(mV); }
inline const VecFloatRefX Vector4::GetX() const { return VecFloatRefX(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefY Vector4::GetY() { return VecFloatRefY(mV); }
inline const VecFloatRefY Vector4::GetY() const { return VecFloatRefY(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefZ Vector4::GetZ() { return VecFloatRefZ(mV); }
inline const VecFloatRefZ Vector4::GetZ() const { return VecFloatRefZ(const_cast<Vector4*>(this)->mV); }
inline VecFloatRefW Vector4::GetW() { return VecFloatRefW(mV); }
inline const VecFloatRefW Vector4::GetW() const { return VecFloatRefW(const_cast<Vector4*>(this)->mV); }

inline VecFloatRefIndex Vector4::GetComponent(int i)
{
    return VecFloatRefIndex(mV, i);
}

inline const VecFloatRefIndex Vector4::GetComponent(int i) const
{
    return VecFloatRefIndex(const_cast<Vector4*>(this)->mV, i);
}

inline VecFloatRefIndex Vector4::operator [](int i)
{
    return VecFloatRefIndex(mV, i);
}

inline const VecFloatRefIndex Vector4::operator [](int i) const
{
    return VecFloatRefIndex(const_cast<Vector4*>(this)->mV, i);
}

}
}
}
