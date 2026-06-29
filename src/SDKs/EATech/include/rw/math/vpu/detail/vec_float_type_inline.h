#pragma once

// Portable PC reconstruction of EARenderWare rwmath (1.02.00)
// rw/math/vpu/detail/vec_float_type_inline.h  -- the out-of-class inline bodies for the
// VecFloat / VecFloatRef<INDEX> / VecFloatRefIndex lane wrappers declared in vec_float.h.
//
// On the X360 every body here is one or more AltiVec/VMX128 intrinsics operating on a
// `vector float` register (`VectorIntrinsic mV`). Those intrinsics do not exist on PC,
// so -- per the project's vendor-SIMD rule -- each is lowered to the equivalent portable
// scalar lane math with semantic parity (x64 widths, member access by name, no raw
// offset hacks). The console intrinsic each line replaces is named in a comment.
//
// Lane semantics of the originals (all confirmed against the Feb-2007 source):
//   * VectorIntrinsicUnion read     temp.scalarFloat[i]            -> mafLane[i]
//   * vspltw(v, i)  (broadcast)      every lane = v lane i
//   * vaddfp/vsubfp(a,b)             per-lane a +/- b
//   * vmaddfp(a,b,0)                 per-lane a * b   (fused multiply, addend = 0)
//   * vrefp + 2x Newton refine       per-lane reciprocal (1/b); used to build a / b
//   * _asmSwizzleStore<D,S>(d, s)    copy s lane S into d lane D, keep d's other lanes
//   * gSwizzleStoreConstants[D*4+S]  the vperm control that performs that single-lane copy
//   * RW_MATH_VPU_CreateVectorFromScalar_R(f)  broadcast scalar f to all four lanes
//
// Because a lane-write only ever stores one lane and a lane-read only ever reads one lane,
// the heavy full-register arithmetic in the console code collapses to scalar float math on
// the single lane involved -- exactly what these de-optimised bodies do.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"

namespace rw
{
namespace math
{
namespace vpu
{

// =====================================================================================
//  VecFloatRefIndex
// =====================================================================================

inline VecFloatRefIndex::VecFloatRefIndex(VectorIntrinsic& v, int index)
    : mV(v)
{
    mIndex = (VectorAxis)index;
}

inline VecFloatRefIndex::VecFloatRefIndex(const VecFloatRefIndex& v)
    : mV(v.mV)
{
    mIndex = v.mIndex;
}

inline float VecFloatRefIndex::GetFloat() const
{
    // VectorIntrinsicUnion temp; temp.vector = mV; return temp.scalarFloat[mIndex];
    return mV.mafLane[mIndex];
}

inline void VecFloatRefIndex::SetFloat(float f)
{
    // build broadcast(f), then vperm gSwizzleStoreConstants[mIndex*4 + X]:
    //   store vf's X lane into mV's mIndex lane.  vf.x == f.
    mV.mafLane[mIndex] = f;
}

inline const VectorIntrinsic& VecFloatRefIndex::GetVector() const
{
    return mV;
}

inline int VecFloatRefIndex::GetIndex() const
{
    return mIndex;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator =(VecFloatRefIndex rhs)
{
    // vperm gSwizzleStoreConstants[mIndex*4 + rhs.mIndex]:
    //   mV.lane[mIndex] = rhs.mV.lane[rhs.mIndex]
    mV.mafLane[mIndex] = rhs.mV.mafLane[rhs.mIndex];
    return *this;
}

template<VectorAxis INDEX>
VecFloatRefIndex& VecFloatRefIndex::operator =(VecFloatRef<INDEX> rhs)
{
    // vperm gSwizzleStoreConstants[mIndex*4 + INDEX]
    mV.mafLane[mIndex] = rhs.mV.mafLane[INDEX];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator =(VecFloat::InParam rhs)
{
    // vperm gSwizzleStoreConstants[mIndex*4 + X]:  mV.lane[mIndex] = rhs X lane
    mV.mafLane[mIndex] = rhs.mV.mafLane[VectorAxisX];
    return *this;
}

inline VecFloatRefIndex::operator float() const
{
    // VectorIntrinsicUnion temp; temp.vector = mV; return temp.scalarFloat[mIndex];
    return mV.mafLane[mIndex];
}

inline VecFloatRefIndex& VecFloatRefIndex::operator =(float f)
{
    // broadcast(f) then store X lane into mV.lane[mIndex]
    mV.mafLane[mIndex] = f;
    return *this;
}

// =====================================================================================
//  VecFloat
// =====================================================================================

inline VecFloat::VecFloat()
{
    // uninitialised on the console (no body); leave mV uninitialised.
}

inline VecFloat::VecFloat(const float& v)
{
    // lvlx(&v) then vspltw lane0 -> broadcast scalar v to all four lanes
    mV.mafLane[0] = v;
    mV.mafLane[1] = v;
    mV.mafLane[2] = v;
    mV.mafLane[3] = v;
}

inline VecFloat::VecFloat(VectorIntrinsicInParam v, int index)
{
    // vspltw(v, index): broadcast lane `index` of v to all four lanes.
    const float lfValue = v.mafLane[index];
    mV.mafLane[0] = lfValue;
    mV.mafLane[1] = lfValue;
    mV.mafLane[2] = lfValue;
    mV.mafLane[3] = lfValue;
}

inline VecFloat::VecFloat(VecFloatRefIndex v)
{
    // vspltw(v.mV, v.mIndex): broadcast the referenced lane to all four lanes.
    const float lfValue = v.mV.mafLane[v.mIndex];
    mV.mafLane[0] = lfValue;
    mV.mafLane[1] = lfValue;
    mV.mafLane[2] = lfValue;
    mV.mafLane[3] = lfValue;
}

template<VectorAxis INDEX>
inline VecFloat::VecFloat(VecFloatRef<INDEX> v)
{
    // vspltw(v.mV, INDEX): broadcast the referenced lane to all four lanes.
    const float lfValue = v.mV.mafLane[INDEX];
    mV.mafLane[0] = lfValue;
    mV.mafLane[1] = lfValue;
    mV.mafLane[2] = lfValue;
    mV.mafLane[3] = lfValue;
}

inline VecFloat::VecFloat(const VecFloat& v)
{
    mV = v.mV;
}

inline void VecFloat::SetFloat(float v)
{
    // VectorIntrinsicUnion u; u.scalarFloat[0..3] = v; mV = u.vector  -> broadcast.
    mV.mafLane[0] = v;
    mV.mafLane[1] = v;
    mV.mafLane[2] = v;
    mV.mafLane[3] = v;
}

inline float VecFloat::GetFloat() const
{
    // VectorIntrinsicUnion vecUnion; vecUnion.vector = mV; return scalarFloat[X];
    return mV.mafLane[VectorAxisX];
}

inline VecFloat& VecFloat::operator =(const VecFloat& f)
{
    mV = f.mV;
    return *this;
}

inline VecFloat::operator float() const
{
    return mV.mafLane[VectorAxisX];
}

inline VecFloat& VecFloat::operator =(const float& f)
{
    SetFloat(f);
    return *this;
}

template<VectorAxis INDEX>
inline VecFloat& VecFloat::operator =(const VecFloatRef<INDEX>& v)
{
    *this = VecFloat(v);
    return *this;
}

inline VecFloat& VecFloat::operator =(const VecFloatRefIndex& v)
{
    *this = VecFloat(v);
    return *this;
}

// =====================================================================================
//  VecFloatRef<INDEX>
// =====================================================================================

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>::VecFloatRef(VectorIntrinsic& v)
    : mV(v)
{
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>::VecFloatRef(const VecFloatRef<INDEX>& v)
    : mV(v.mV)
{
}

template<VectorAxis INDEX>
inline float VecFloatRef<INDEX>::GetFloat() const
{
    // VectorIntrinsicUnion vecUnion; vecUnion.vector = mV; return scalarFloat[INDEX];
    return mV.mafLane[INDEX];
}

template<VectorAxis INDEX>
inline void VecFloatRef<INDEX>::SetFloat(float f)
{
    // build broadcast(f); VectorPermuteConstant copies that vec's X lane into lane INDEX.
    mV.mafLane[INDEX] = f;
}

template<VectorAxis INDEX>
inline const VectorIntrinsic& VecFloatRef<INDEX>::GetVector() const
{
    return mV;
}

template<VectorAxis INDEX>
inline int VecFloatRef<INDEX>::GetIndex() const
{
    return INDEX;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefX rhs)
{
    // VectorPermuteConstant: lane INDEX <- rhs lane X
    mV.mafLane[INDEX] = rhs.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefY rhs)
{
    mV.mafLane[INDEX] = rhs.mV.mafLane[VectorAxisY];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefZ rhs)
{
    mV.mafLane[INDEX] = rhs.mV.mafLane[VectorAxisZ];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefW rhs)
{
    mV.mafLane[INDEX] = rhs.mV.mafLane[VectorAxisW];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefIndex rhs)
{
    // vperm gSwizzleStoreConstants[INDEX*4 + rhs.mIndex]
    mV.mafLane[INDEX] = rhs.mV.mafLane[rhs.mIndex];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloat::InParam rhs)
{
    // VectorPermuteConstant: lane INDEX <- rhs lane X
    mV.mafLane[INDEX] = rhs.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>::operator float() const
{
    return GetFloat();
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(float f)
{
    SetFloat(f);
    return *this;
}

// ---- VecFloatRef<INDEX> += --------------------------------------------------------

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (VecFloat::InParam b)
{
    // r = vaddfp(mV, b.mV); store r lane INDEX back into mV lane INDEX.
    // b broadcast => b.lane[INDEX] == b.x.
    mV.mafLane[INDEX] = mV.mafLane[INDEX] + b.mV.mafLane[INDEX];
    return *this;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (VecFloatRef<INDEX_OTHER> b)
{
    // bVI = vspltw(b.mV, INDEX_OTHER) (broadcast b's lane); r = vaddfp(mV, bVI);
    // store r lane INDEX.
    mV.mafLane[INDEX] = mV.mafLane[INDEX] + b.mV.mafLane[INDEX_OTHER];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (const VecFloatRefIndex& b)
{
    // bVI = broadcast(b.mV lane b.mIndex); r = vaddfp(mV, bVI); store r lane INDEX.
    mV.mafLane[INDEX] = mV.mafLane[INDEX] + b.mV.mafLane[b.mIndex];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (const float& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] + b;
    return *this;
}

// ---- VecFloatRef<INDEX> -= --------------------------------------------------------

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (VecFloat::InParam b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] - b.mV.mafLane[INDEX];
    return *this;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (VecFloatRef<INDEX_OTHER> b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] - b.mV.mafLane[INDEX_OTHER];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (const VecFloatRefIndex& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] - b.mV.mafLane[b.mIndex];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (const float& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] - b;
    return *this;
}

// ---- VecFloatRef<INDEX> *= --------------------------------------------------------

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (VecFloat::InParam b)
{
    // r = vmaddfp(mV, b.mV, 0): per-lane multiply.
    mV.mafLane[INDEX] = mV.mafLane[INDEX] * b.mV.mafLane[INDEX];
    return *this;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (VecFloatRef<INDEX_OTHER> b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] * b.mV.mafLane[INDEX_OTHER];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (const VecFloatRefIndex& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] * b.mV.mafLane[b.mIndex];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (const float& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] * b;
    return *this;
}

// ---- VecFloatRef<INDEX> /= --------------------------------------------------------
// The console computes 1/b via vrefp + two Newton-Raphson refinement steps, then
// multiplies. That is just a hardware-accurate way to evaluate the reciprocal; the
// portable form is the exact scalar division it converges to.

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (VecFloat::InParam b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] / b.mV.mafLane[INDEX];
    return *this;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (VecFloatRef<INDEX_OTHER> b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] / b.mV.mafLane[INDEX_OTHER];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (const VecFloatRefIndex& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] / b.mV.mafLane[b.mIndex];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (const float& b)
{
    mV.mafLane[INDEX] = mV.mafLane[INDEX] / b;
    return *this;
}

// =====================================================================================
//  VecFloatRefIndex compound assignment
//  (runtime lane; the console permutes the referenced lane down to lane 0, does the fp
//   op, then swizzle-stores the result lane back into mIndex)
// =====================================================================================

// ---- += ---------------------------------------------------------------------------

inline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloat::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] + b.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloatRef<INDEX> b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] + b.mV.mafLane[INDEX];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloatRefIndex::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] + b.mV.mafLane[b.mIndex];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator += (const float& b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] + b;
    return *this;
}

// ---- -= ---------------------------------------------------------------------------

inline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloat::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] - b.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloatRef<INDEX> b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] - b.mV.mafLane[INDEX];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloatRefIndex::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] - b.mV.mafLane[b.mIndex];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator -= (const float& b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] - b;
    return *this;
}

// ---- *= ---------------------------------------------------------------------------

inline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloat::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] * b.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloatRef<INDEX> b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] * b.mV.mafLane[INDEX];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloatRefIndex::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] * b.mV.mafLane[b.mIndex];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator *= (const float& b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] * b;
    return *this;
}

// ---- /= ---------------------------------------------------------------------------

inline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloat::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] / b.mV.mafLane[VectorAxisX];
    return *this;
}

template<VectorAxis INDEX>
inline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloatRef<INDEX> b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] / b.mV.mafLane[INDEX];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloatRefIndex::InParam b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] / b.mV.mafLane[b.mIndex];
    return *this;
}

inline VecFloatRefIndex& VecFloatRefIndex::operator /= (const float& b)
{
    mV.mafLane[mIndex] = mV.mafLane[mIndex] / b;
    return *this;
}

}
}
}
