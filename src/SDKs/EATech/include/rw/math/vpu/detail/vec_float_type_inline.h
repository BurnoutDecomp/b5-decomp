#pragma once

// rwmath rw/math/vpu/detail/vec_float_type_inline.h: the inline members of VecFloat, VecFloatRef<INDEX>
// and VecFloatRefIndex declared in vec_float.h.
//
// Structure: LegacySrc EARenderWare rwmath 1.02.00 vpu/detail/vec_float_type_inline.h, in its order with
// its signatures and statement layout (the DecFIGS DWARF places the same members on the same lines, +2..+4
// from VecFloat::SetFloat :152 on). LegacySrc's copy is PS3 preprocessor output; each platform macro
// expansion here is the Xenon one an ARTIST site attests:
//   lane reads                 the lane read in place, no register copy (VecFloatRef<X>::GetFloat in
//                              ColourSelection::GetUint32ColourFromVector4 0x824E8568: lfs 0(r31)).
//   VecFloat(v, index)         __vperm(v, v, __vspltw(__lvsl(0, 4 * index), 0)), the VecFloatRefIndex
//                              broadcast of the operator set (Camera::GetFrustum 0x822772AC).
//   VecFloat::SetFloat,        { f } stored with three zero words, lvx, vspltw 0:
//   VecFloatRef<I>::SetFloat   FlaptTimerFieldComponent::CalculateColour 0x8241CA48..0x8241CA60 (PS3 :155);
//                              Serialise<1> 0x82213C7C..0x82213C9C (VecFloatRef<Y> = float).
//   constant-lane store        lane D of the destination from lane S of the source: the vrlimi128 lane
//   (_asmSwizzleStore<D,S>,    insert, mask 8 >> D, rotate (S - D) & 3: Serialise<1> 0x82213CA0 (D = Y,
//   VecFloatRef<I> =)          S = X: mask 4, rotate 3); BehaviourGameplayExternal::ModifyTargetAngles
//                              0x822255E4 (X() = VecFloat: 8, 0) and 0x822255F4 (Z() = VecFloat: 2, 2);
//                              AttachmentTruck::Update 0x8220F0D8 (Y() = v.Y(): 4, 0).
//   compound VecFloatRef<I>    the operation over the whole register, then the lane insert <I, I>:
//                              ModifyTargetAngles 0x82225600..0x82225624 (Z() *= const float&: zero-filled
//                              splat, vmulfp(mV, b), vrlimi 2, 0); PropEntityDebugComponent::
//                              ToCellGridScreenCoords 0x822DC4C4..0x822DC4F4 (Y() /= v.Y(): vspltw 1, vrefp
//                              and two refinements, vmulfp(reciprocal, mV), vrlimi 4, 0) and 0x822DC500
//                              (X() *= v.X()).
//   runtime-lane store         __vperm(d, s, gSwizzleStoreConstants[D * 4 + S]), the table at 0x8327F140:
//   (VecFloatRefIndex =,       ProgressBarRenderer::RenderComponent 0x82463454..0x824634E0 (= VecFloat,
//   _asmSwizzleStore(d,i,s,j)) entry 8); InterpedParam3::Construct 0x8259CD30 (= float x3, entries 0/4/8,
//                              each a four-lane fill: four stfs and lvx); MainMapComponent::SetZoom
//                              0x82469B90..0x82469BC8 (*= const float&: zero-filled splat, vmulfp(mV, b),
//                              entry 10).
//   + - * /                    __vaddfp / __vsubfp / __vmulfp(a, b) / vrefp + two vnmsubfp/vmaddfp
//                              refinements + __vmulfp(reciprocal, a), as the operator set
//                              (detail/vec_float_operation_inline.h).
// VecFloatRefIndex::SetFloat has no PS3 or ARTIST user; it keeps LegacySrc's text, which is
// operator=(float)'s (attested above).
// The SDK declares the members always_inline (LegacySrc, DecFIGS); __forceinline is its Xenon spelling, as
// for the operator set. VecFloatRefIndex::operator=(VecFloatRef<INDEX>) is a plain template in the SDK.
// VecFloat has no user copy constructor or copy assignment: DecFIGS vec_float.h:56..72 declares neither
// (LegacySrc 1.02.00 defines both).

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/vector_intrinsic_operation.h"

namespace rw
{
namespace math
{
namespace vpu
{

__forceinline VecFloatRefIndex::VecFloatRefIndex(VectorIntrinsic& v, int index)
:mV(v)
{


    mIndex = (VectorAxis)index;
}

__forceinline VecFloatRefIndex::VecFloatRefIndex(const VecFloatRefIndex &v)
:mV(v.mV)
{
    mIndex= v.mIndex;

}

__forceinline float VecFloatRefIndex::GetFloat() const
{
    return mV.v[mIndex];
}

__forceinline void VecFloatRefIndex::SetFloat(float f)
{
    VectorIntrinsic vf;
    { ::rw::math::vpu::detail::VectorIntrinsicUnion u; u.scalarFloat[0] = f; u.scalarFloat[1] = f; u.scalarFloat[2] = f; u.scalarFloat[3] = f; vf = u.vector; };
    { mV = __vperm(mV, vf, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((mIndex) * 4 ) + (VectorAxisX))]); }
}

__forceinline const VectorIntrinsic& VecFloatRefIndex::GetVector() const
{
    return mV;
}

__forceinline int VecFloatRefIndex::GetIndex() const
{
    return mIndex;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator=(VecFloatRefIndex rhs)
{

    { mV = __vperm(mV, rhs.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((mIndex) * 4 ) + (rhs.mIndex))]); };
    return *this;
}

template<VectorAxis INDEX>
VecFloatRefIndex& VecFloatRefIndex::operator=(VecFloatRef<INDEX> rhs)
{

    { mV = __vperm(mV, rhs.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((mIndex) * 4 ) + (INDEX))]); };
    return *this;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator=(VecFloat::InParam rhs)
{

    { mV = __vperm(mV, rhs.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((mIndex) * 4 ) + (VectorAxisX))]); };
    return *this;
}

__forceinline VecFloatRefIndex::operator float() const
{
    return mV.v[mIndex];
}

__forceinline VecFloatRefIndex &VecFloatRefIndex::operator =(float f)
{
    VectorIntrinsic vf;
    { ::rw::math::vpu::detail::VectorIntrinsicUnion u; u.scalarFloat[0] = f; u.scalarFloat[1] = f; u.scalarFloat[2] = f; u.scalarFloat[3] = f; vf = u.vector; };
    { mV = __vperm(mV, vf, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((mIndex) * 4 ) + (VectorAxisX))]); };
    return *this;
}

__forceinline VecFloat::VecFloat()
{
}

__forceinline VecFloat::VecFloat(const float &v)
{
    { mV = __vspltw(__lvlx(&v, 0), 0); };
}

__forceinline VecFloat::VecFloat( VectorIntrinsicInParam v, int index )
{
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(index))), 0); mV = __vperm(v, v, permConstant); };
}

__forceinline VecFloat::VecFloat(VecFloatRefIndex v)
{
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(v.mIndex))), 0); mV = __vperm(v.mV, v.mV, permConstant); };
}

template<VectorAxis INDEX>
__forceinline VecFloat::VecFloat(VecFloatRef<INDEX> v)
{
    { mV = __vspltw(v.mV, (int)(INDEX)); };
}

__forceinline void VecFloat::SetFloat(float v)
{
    { ::rw::math::vpu::VectorIntrinsic u = { v }; mV = __vspltw(u, 0); };
}

__forceinline float VecFloat::GetFloat() const
{
    return mV.v[VectorAxisX];
}

__forceinline VecFloat::operator float() const
{
    return mV.v[VectorAxisX];
}

__forceinline VecFloat &VecFloat::operator =(const float& f)
{
    SetFloat(f);
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloat& VecFloat::operator=(const VecFloatRef<INDEX>& v)
{
    *this = VecFloat(v);
    return *this;
}

__forceinline VecFloat& VecFloat::operator=(const VecFloatRefIndex& v)
{
    *this = VecFloat(v);
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>::VecFloatRef(VectorIntrinsic& v)
:mV(v)
{
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>::VecFloatRef(const VecFloatRef<INDEX> & v)
:mV(v.mV)
{
}

template<VectorAxis INDEX>
__forceinline float VecFloatRef<INDEX>::GetFloat() const
{
    return mV.v[INDEX];
}

template<VectorAxis INDEX>
__forceinline void VecFloatRef<INDEX>::SetFloat(float f)
{
    VectorIntrinsic vec;
    { ::rw::math::vpu::VectorIntrinsic u = { f }; vec = __vspltw(u, 0); };
    { mV = __vrlimi(mV, vec, (0x8 >> (INDEX)), (((int)(VectorAxisX)) - (INDEX)) & 3); };
}

template<VectorAxis INDEX>
__forceinline const VectorIntrinsic& VecFloatRef<INDEX>::GetVector() const
{
    return mV;
}

template<VectorAxis INDEX>
__forceinline int VecFloatRef<INDEX>::GetIndex() const
{
    return INDEX;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefX rhs)
{
    { mV = __vrlimi(mV, rhs.mV, (0x8 >> (INDEX)), (((int)(VectorAxisX)) - (INDEX)) & 3); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefY rhs)
{
    { mV = __vrlimi(mV, rhs.mV, (0x8 >> (INDEX)), (((int)(VectorAxisY)) - (INDEX)) & 3); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefZ rhs)
{
    { mV = __vrlimi(mV, rhs.mV, (0x8 >> (INDEX)), (((int)(VectorAxisZ)) - (INDEX)) & 3); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefW rhs)
{
    { mV = __vrlimi(mV, rhs.mV, (0x8 >> (INDEX)), (((int)(VectorAxisW)) - (INDEX)) & 3); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloatRefIndex rhs)
{
    { mV = __vperm(mV, rhs.mV, ::rw::math::vpu::detail::gSwizzleStoreConstants[(((INDEX) * 4 ) + (rhs.mIndex))]); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(VecFloat::InParam rhs)
{
    { mV = __vrlimi(mV, rhs.mV, (0x8 >> (INDEX)), (((int)(VectorAxisX)) - (INDEX)) & 3); };
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>::operator float() const
{
    return GetFloat();
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator =(float f)
{
    SetFloat(f);
    return *this;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (VecFloat::InParam b)
{
    VectorIntrinsic r; r = __vaddfp((*this).mV, b.mV);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (VecFloatRef<INDEX_OTHER> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX_OTHER)); }; r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator += (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloat::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic aVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((*this).mIndex))), 0); aVI = __vperm((*this).mV, (*this).mV, permConstant); }; r = __vaddfp(aVI, (b).mV);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloatRef<INDEX> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX)); }; r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator += (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator += (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vaddfp((*this).mV, bVI);; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (VecFloat::InParam b)
{
    VectorIntrinsic r; r = __vsubfp( (*this).mV, b.mV ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (VecFloatRef<INDEX_OTHER> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX_OTHER)); }; r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator -= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloat::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic aVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((*this).mIndex))), 0); aVI = __vperm((*this).mV, (*this).mV, permConstant); }; r = __vsubfp( aVI, (b).mV ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloatRef<INDEX> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX)); }; r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator -= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator -= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vsubfp( (*this).mV, bVI ); const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (VecFloat::InParam b)
{
    VectorIntrinsic r; r = __vmulfp(((*this).mV), (b.mV));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (VecFloatRef<INDEX_OTHER> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX_OTHER)); }; r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator *= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloat::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic aVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((*this).mIndex))), 0); aVI = __vperm((*this).mV, (*this).mV, permConstant); }; r = __vmulfp((aVI), ((b).mV));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloatRef<INDEX> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX)); }; r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator *= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator *= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); r = __vmulfp(((*this).mV), (bVI));; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (VecFloat::InParam b)
{
    VectorIntrinsic r; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
template<VectorAxis INDEX_OTHER>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (VecFloatRef<INDEX_OTHER> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX_OTHER)); }; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRef<INDEX>& VecFloatRef<INDEX>::operator /= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore<INDEX, INDEX>((*this).mV, r); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloat::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic aVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((*this).mIndex))), 0); aVI = __vperm((*this).mV, (*this).mV, permConstant); }; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp((b).mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), ((b).mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), ((b).mV), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), (aVI)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

template<VectorAxis INDEX>
__forceinline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloatRef<INDEX> b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { bVI = __vspltw((b).mV, (int)(INDEX)); }; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator /= (VecFloatRefIndex::InParam b)
{
    VectorIntrinsic r; VectorIntrinsic bVI; { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)((b).mIndex))), 0); bVI = __vperm((b).mV, (b).mV, permConstant); }; { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}

__forceinline VecFloatRefIndex& VecFloatRefIndex::operator /= (const float& b)
{
    VectorIntrinsic r; VectorIntrinsic bVI = RW_MATH_VPU_CreateVectorFromScalar_R(b); { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVI);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVI), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVI), (vOne))), (secondEstimate)); }; r = __vmulfp((reciprocal), ((*this).mV)); }; const_cast<VectorIntrinsic&>((*this).mV) = _asmSwizzleStore((*this).mV, (*this).mIndex, r, (*this).mIndex); return *this;;
}
}
}
}
