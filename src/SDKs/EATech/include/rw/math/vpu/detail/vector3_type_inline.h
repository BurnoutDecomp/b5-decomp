#pragma once

// VPU Vector3 construction and component access over the genuine VectorIntrinsic mV.
// Native scalar/raw constructors and implicit copy are object-bound separately.
// Existing lane/ref/pointer/template overloads below retain their source surface;
// their presence is not a claim that every swizzle/NaN/W-lane case is recovered.

#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"

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
    // rwmath vector3_type_inline.h:17: the constructor's three-lane fill (the aggregate initialiser with its zeroed
    // w word, ARTIST ExternallySimulatedBody::Construct 0x8259CF38..0x8259CF44 forms each w address apart).
    const VectorIntrinsic lValue = { x, y, z };
    mV = lValue;
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
// rwmath 1.02.00 writes the single-lane setters as the swizzle-store macros (LegacySrc
// vector3_type_inline.h); their Xenon bodies are those of the VecFloatRef / VecFloatRefIndex stores
// (detail/vec_float_type_inline.h): a constant lane from a VecFloat or VecFloatRef<INDEX> is the vrlimi128 lane
// insert, mask 8 >> D, rotate (S - D) & 3, the source lane of a VecFloat being D itself (LegacySrc) (SetY(VecFloat) in AIDebugComponent::DrawAggressiveWarning
// 0x82773CF8: vrlimi 4, 0; Vector4::SetX(VecFloat) in VehicleAttribs::SetupAttribsForDonutAI 0x825F62E8: 8, 0),
// a runtime lane permutes with gSwizzleStoreConstants[D * 4 + S] (0x8327F140).

inline void Vector3::SetComponent(int i, float value)
{
    detail::VectorIntrinsicUnion temp;
    temp.vector = mV;
    temp.scalarFloat[i] = value;
    mV = temp.vector;
}

inline void Vector3::SetComponent(int i, VecFloat::InParam value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (VectorAxisX))]); };
}

inline void Vector3::SetComponent(int i, VecFloatRefIndex::InParam value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (value.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector3::SetComponent(int i, VecFloatRef<INDEX> value)
{
    { mV = __vperm(mV, value.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((i) * 4 ) + (INDEX))]); };
}

inline void Vector3::SetX(float x)
{
    SetComponent(0, x);
}

inline void Vector3::SetX(VecFloat::InParam x)
{
    { mV = __vrlimi(mV, x.mV, (0x8 >> (VectorAxisX)), (((int)(VectorAxisX)) - (VectorAxisX)) & 3); };
}

inline void Vector3::SetX(VecFloatRefIndex::InParam x)
{
    { mV = __vperm(mV, x.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisX) * 4 ) + (x.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector3::SetX(VecFloatRef<INDEX> x)
{
    { mV = __vrlimi(mV, x.mV, (0x8 >> (VectorAxisX)), (((int)(INDEX)) - (VectorAxisX)) & 3); };
}

inline void Vector3::SetY(float y)
{
    SetComponent(1, y);
}

inline void Vector3::SetY(VecFloat::InParam y)
{
    { mV = __vrlimi(mV, y.mV, (0x8 >> (VectorAxisY)), (((int)(VectorAxisY)) - (VectorAxisY)) & 3); };
}

inline void Vector3::SetY(VecFloatRefIndex::InParam y)
{
    { mV = __vperm(mV, y.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisY) * 4 ) + (y.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector3::SetY(VecFloatRef<INDEX> y)
{
    { mV = __vrlimi(mV, y.mV, (0x8 >> (VectorAxisY)), (((int)(INDEX)) - (VectorAxisY)) & 3); };
}

inline void Vector3::SetZ(float z)
{
    SetComponent(2, z);
}

inline void Vector3::SetZ(VecFloat::InParam z)
{
    { mV = __vrlimi(mV, z.mV, (0x8 >> (VectorAxisZ)), (((int)(VectorAxisZ)) - (VectorAxisZ)) & 3); };
}

inline void Vector3::SetZ(VecFloatRefIndex::InParam z)
{
    { mV = __vperm(mV, z.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((VectorAxisZ) * 4 ) + (z.mIndex))]); };
}

template<VectorAxis INDEX>
inline void Vector3::SetZ(VecFloatRef<INDEX> z)
{
    { mV = __vrlimi(mV, z.mV, (0x8 >> (VectorAxisZ)), (((int)(INDEX)) - (VectorAxisZ)) & 3); };
}

inline void Vector3::SetZero()
{
    // One splatted zero register for all four lanes: ARTIST materialises it once with
    // vspltisw and stores it whole (BehaviourDebugFlyWorld::Prepare 0x821FB73C / 0x821FB784).
    mV = __vspltisw(0);
}

// =====================================================================================
//  Constructors
// =====================================================================================

inline Vector3::Vector3(VecFloat::InParam x, VecFloat::InParam y, VecFloat::InParam z)
    : mV(_asmCreateVectorAxis<VectorAxisX, VectorAxisY, VectorAxisZ>(x.mV, y.mV, z.mV))
{
}

inline Vector3::Vector3(VecFloatRefIndex::InParam x,
                        VecFloatRefIndex::InParam y,
                        VecFloatRefIndex::InParam z)
{
    const VecFloat xValue(x);
    const VecFloat yValue(y);
    const VecFloat zValue(z);
    mV = _asmCreateVectorAxis<VectorAxisX, VectorAxisY, VectorAxisZ>(xValue.mV, yValue.mV, zValue.mV);
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


inline Vector3::Vector3(float x, float y, float z)
{
    // rwmath vector3_type_inline.h:232 fills x, y, z of a vector and assigns it; the Xenon fill is the aggregate
    // initialiser, whose zeroed w word is stored through its own address (ARTIST's out-of-line copy 0x821F0DD8:
    // addi r11,r1,-4 ... stw r10,0(r11); AIDebugComponent::DrawResetOnTrackAISection 0x82767224/0x82767248,
    // DrawCarControlData 0x8277398C/0x827739B4, ResetOnTrackManager 0x82783E78/0x82783E98).
    const VectorIntrinsic lValue = { x, y, z };
    mV = lValue;
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
