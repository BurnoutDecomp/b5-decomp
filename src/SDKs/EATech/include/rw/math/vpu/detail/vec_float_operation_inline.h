#pragma once

// rwmath rw/math/vpu/detail/vec_float_operation_inline.h: the free VecFloat operators.
//
// Structure: verbatim from LegacySrc EARenderWare rwmath 1.02.00 vpu/detail/vec_float_operation_inline.h,
// all 181 operators in its order with its overload set and argument forms (== != > < >= <= over
// VecFloatRef<I> / VecFloatRefIndex / VecFloat / const float&; unary -; += -= *= /= on VecFloat& and
// float&; + - * / with the const float& forms). The DecFIGS DWARF lists the same overloads at the same
// relative positions (operator+(VecFloat, VecFloat) :1246 = LegacySrc :950 + 296 for every operator from
// the unary minus on); only its comparison block is longer: there > < >= <= have the == body shape (e.g.
// operator>(VecFloat, VecFloat) :354 -> operator>(VecFloat, VecFloatRef<I>) :362, as == :38 -> :46), which
// ARTIST confirms below. LegacySrc's copy is PS3 preprocessor output, so each platform macro is expanded
// in place; here each expansion is the Xenon one an ARTIST site attests (the PS3 line tags are the
// DecFIGS lineinfo of the same function's inlined operator):
//   VecFloatRef<I> operand    __vspltw(x.mV, I): the VecFloat(VecFloatRef<I>) expansion (0x8270DCC4);
//                             VehiclePhysics::UpdateSuspension 0x8261F6C0 (PS3 :1249).
//   VecFloatRefIndex operand  __vperm(x.mV, x.mV, __vspltw(__lvsl(0, 4 * index), 0)): Camera::GetFrustum
//                             0x822772AC..0x822772E0 (`li r8,0xC; lvsl; vspltw 0; vperm`, PS3 :155).
//   const float& operand      { f } stored with three zero words, lvx, vspltw 0 (the Xenon form of the PS3
//                             union fill, which is VecFloat::SetFloat's text too): GetFrustum
//                             0x822772B4..0x822772E8, VehiclePhysics::EnterDrift 0x825FA2B8..0x825FA300
//                             (PS3 :1658), Engine::ComputeGear 0x825CF024..0x825CF044 (PS3 :798),
//                             SetFloat in FlaptTimerFieldComponent::CalculateColour 0x8241CA48..0x8241CA60.
//   + -                       __vaddfp / __vsubfp: UpdateSuspension 0x8261F6C8; CalculateColour 0x8241CA64.
//   *                         __vmulfp(a, b), the PS3 vmaddfp(a, b, 0): FrustrumCollisionResolver::
//                             GetHeightAboveTraffic 0x821F90EC (PS3 :1540), ComputeGear 0x825CF090,
//                             StuntOffencesManager::CheckForDrift 0x82613900 (VecFloat * const float&).
//   /                         vrefp and two vnmsubfp/vmaddfp refinements, then __vmulfp(reciprocal, a):
//                             AttachmentTruck::Update 0x8220EFB0..0x8220EFC8 (PS3 :1689); by const float&
//                             the scalar 1.0f / b splatted: MapTransform::CalculateZoomFactor
//                             0x8244F364..0x8244F418 (PS3 :1715).
//   unary -                   vspltisw -1, vslw, vxor(a, mask), one vslw per negation:
//                             BehaviourGameplayExternal::ModifyTargetAngles 0x8222558C..0x822255D8 (PS3 :968).
//   == !=                     the zeroed CR word, vcmpeqfp., mfocrf, CR6 all-true bit (extrwi 1,24; the PS3
//                             vcmpeqfp_p(2, ...)): GetFrustum 0x822772C0..0x822772F4; != returns its
//                             complement: LooseOctree::UpdateRecursive 0x828B160C..0x828B1644 (PS3 :323).
//   > <                       vcmpgtfp.(a, b) / vcmpgtfp.(b, a): Camera::Utils::PointsWillPassEachOther
//                             0x821F87B4..0x821F87CC (DWARF operator> after Dot, PS3 :481); EnterDrift
//                             0x825FA340 (PS3 :515) and the out-of-line copy 0x821F0E20.
//   >= <=                     vcmpgefp.(a, b) / vcmpgefp.(b, a): ComputeGear 0x825CF048 (PS3 :800);
//                             CameraInterpolationController::RotateAboutPivot 0x8223DA8C / 0x8223DACC (PS3 :957).
//   const float& + - / VecFloat, float& op= VecFloat: scalar on GetFloat(), as LegacySrc writes them:
//                             EnterDrift 0x825FA308..0x825FA324 (stvx/lfs, fadds, the VecFloat(const float&)
//                             lvlx splat; PS3 :1372), CheckForDrift 0x82613904..0x82613910 (float& +=, PS3 :1035).
// The SDK declares every operator always_inline (LegacySrc, DecFIGS); __forceinline is its Xenon spelling.
// ARTIST's optimised code inlines them; the out-of-line rw::math::vpu operator copies it holds (operator<
// 0x821F0E20, operator- 0x82276868, operator+ 0x825B2620) are called only from the /Od PolygonSoupTester job.

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
#include "SDKs/EATech/include/rw/math/vpu/detail/vec_float_operation_platform_inline.h"

namespace rw {
namespace math {
namespace vpu {

template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator==(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator==(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator==(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}


template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator!=(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

template<VectorAxis INDEX>
__forceinline bool operator!=(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}

__forceinline bool operator!=(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpeqfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return !ret;
}


template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator>(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(a.mV, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}


template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator<(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(b.mV, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgtfpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}


template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator>=(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(a.mV, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, b.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator>=(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(aVF, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator>=(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(a.mV, bVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}


template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline bool operator<=(VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloat::InParam a, VecFloat::InParam b)
{
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(b.mV, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(VecFloat::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(VecFloatRef<INDEX> a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(VecFloatRefIndex::InParam a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(VecFloatRef<INDEX> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(const float& a, VecFloatRef<INDEX> b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX)); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(const float& a, VecFloat::InParam b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic u = { a }; aVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(b.mV, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

template<VectorAxis INDEX>
__forceinline bool operator<=(VecFloatRef<INDEX> a, const float& b)
{
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, aVF, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline bool operator<=(VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVF = __vspltw(u, 0); };
    bool ret;
    { unsigned int cr = 0; __vcmpgefpR(bVF, a.mV, &cr); ret = ((cr & 0x80) == 0x80); };
    return ret;
}

__forceinline VecFloat operator - (VecFloat::InParam a)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic signMask; signMask = __vspltisw(-1); signMask = __vslw(signMask, signMask);; ret = __vxor(a.mV, signMask); };
    return VecFloat(ret);
}

template<VectorAxis INDEX>
__forceinline VecFloat operator - (VecFloatRef<INDEX> a)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX)); };
    { ::rw::math::vpu::VectorIntrinsic signMask; signMask = __vspltisw(-1); signMask = __vslw(signMask, signMask);; ret = __vxor(aVF, signMask); };
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloatRefIndex::InParam a)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic signMask; signMask = __vspltisw(-1); signMask = __vslw(signMask, signMask);; ret = __vxor(aVF, signMask); };
    return VecFloat(ret);
}


__forceinline VecFloat& operator += (VecFloat& a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vaddfp(a.mV, b.mV);;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

template<VectorAxis INDEX1>
__forceinline VecFloat& operator += (VecFloat& a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vaddfp(a.mV, bVF);;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator += (VecFloat& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vaddfp(a.mV, bVF);;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator += (VecFloat& a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vaddfp(a.mV, bVec);;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline float& operator += (float& a, VecFloat::InParam b)
{
    a += b.GetFloat();
    return a;
}

template<VectorAxis INDEX1>
__forceinline float& operator += (float& a, VecFloatRef<INDEX1> b)
{
    a += b.GetFloat();
    return a;
}

__forceinline float& operator += (float& a, VecFloatRefIndex::InParam b)
{
    a += b.GetFloat();
    return a;
}


__forceinline VecFloat& operator -= (VecFloat& a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vsubfp( a.mV, b.mV );
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

template<VectorAxis INDEX1>
__forceinline VecFloat& operator -= (VecFloat& a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vsubfp( a.mV, bVF );
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator -= (VecFloat& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vsubfp( a.mV, bVF );
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator -= (VecFloat& a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vsubfp( a.mV, bVec );
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline float& operator -= (float& a, VecFloat::InParam b)
{
    a -= b.GetFloat();
    return a;
}

template<VectorAxis INDEX1>
__forceinline float& operator -= (float& a, VecFloatRef<INDEX1> b)
{
    a -= b.GetFloat();
    return a;
}

__forceinline float& operator -= (float& a, VecFloatRefIndex::InParam b)
{
    a -= b.GetFloat();
    return a;
}


__forceinline VecFloat& operator *= (VecFloat& a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vmulfp((a.mV), (b.mV));;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

template<VectorAxis INDEX1>
__forceinline VecFloat& operator *= (VecFloat& a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vmulfp((a.mV), (bVF));;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator *= (VecFloat& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vmulfp((a.mV), (bVF));;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator *= (VecFloat& a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vmulfp((a.mV), (bVec));;
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline float& operator *= (float& a, VecFloat::InParam b)
{
    a *= b.GetFloat();
    return a;
}

template<VectorAxis INDEX1>
__forceinline float& operator *= (float& a, VecFloatRef<INDEX1> b)
{
    a *= b.GetFloat();
    return a;
}

__forceinline float& operator *= (float& a, VecFloatRefIndex::InParam b)
{
    a *= b.GetFloat();
    return a;
}


__forceinline VecFloat& operator /= (VecFloat& a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

template<VectorAxis INDEX1>
__forceinline VecFloat& operator /= (VecFloat& a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator /= (VecFloat& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline VecFloat& operator /= (VecFloat& a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVec);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVec), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVec), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    const_cast<VectorIntrinsic&>(a.mV) = ret;
    return a;
}

__forceinline float& operator /= (float& a, VecFloat::InParam b)
{
    a /= b.GetFloat();
    return a;
}

template<VectorAxis INDEX1>
__forceinline float& operator /= (float& a, VecFloatRef<INDEX1> b)
{
    a /= b.GetFloat();
    return a;
}

__forceinline float& operator /= (float& a, VecFloatRefIndex::InParam b)
{
    a /= b.GetFloat();
    return a;
}


__forceinline VecFloat operator + (VecFloat::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vaddfp(a.mV, b.mV);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (VecFloat::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vaddfp(a.mV, bVF);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vaddfp(a.mV, bVF);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    VectorIntrinsic ret;
    ret = __vaddfp(a.mV, bVec);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (VecFloatRef<INDEX1> a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    ret = __vaddfp(aVF, b.mV);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline VecFloat operator + (VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    ret = __vaddfp(aVF, bVF);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (VecFloatRef<INDEX1> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vaddfp(aVF, bVF);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (VecFloatRef<INDEX1> a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vaddfp(aVF, bVec);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    ret = __vaddfp(aVF, b.mV);;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (VecFloatRefIndex::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vaddfp(aVF, bVF);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vaddfp(aVF, bVF);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vaddfp(aVF, bVec);;
    return VecFloat(ret);
}

__forceinline VecFloat operator + (const float& a, VecFloat::InParam b)
{
    return VecFloat(a + b.GetFloat());
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator + (const float& a, VecFloatRef<INDEX1> b)
{
    return VecFloat(a + b.GetFloat());
}

__forceinline VecFloat operator + (const float& a, VecFloatRefIndex::InParam b)
{
    return VecFloat(a + b.GetFloat());
}


__forceinline VecFloat operator - (VecFloat::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vsubfp( a.mV, b.mV );
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (VecFloat::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vsubfp( a.mV, bVF );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vsubfp( a.mV, bVF );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloat::InParam a, const float& b)
{
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    VectorIntrinsic ret;
    ret = __vsubfp( a.mV, bVec );
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (VecFloatRef<INDEX1> a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    ret = __vsubfp( aVF, b.mV );
    return VecFloat(ret);
}

template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline VecFloat operator - (VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    ret = __vsubfp( aVF, bVF );
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (VecFloatRef<INDEX1> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vsubfp( aVF, bVF );
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (VecFloatRef<INDEX1> a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vsubfp( aVF, bVec );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    ret = __vsubfp( aVF, b.mV );
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (VecFloatRefIndex::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vsubfp( aVF, bVF );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vsubfp( aVF, bVF );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    ret = __vsubfp( aVF, bVec );
    return VecFloat(ret);
}

__forceinline VecFloat operator - (const float& a, VecFloat::InParam b)
{
    return VecFloat(a - b.GetFloat());
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator - (const float& a, VecFloatRef<INDEX1> b)
{
    return VecFloat(a - b.GetFloat());
}

__forceinline VecFloat operator - (const float& a, VecFloatRefIndex::InParam b)
{
    return VecFloat(a - b.GetFloat());
}


__forceinline VecFloat operator * (VecFloat::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    ret = __vmulfp((a.mV), (b.mV));;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (VecFloat::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vmulfp((a.mV), (bVF));;
    return VecFloat(ret);
}

__forceinline VecFloat operator * (VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vmulfp((a.mV), (bVF));;
    return VecFloat(ret);
}

__forceinline VecFloat operator * (VecFloat::InParam a, const float& b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { b }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((a.mV), (floatVec));; };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (VecFloatRef<INDEX1> a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    ret = __vmulfp((aVF), (b.mV));;
    return VecFloat(ret);
}

template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline VecFloat operator * (VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    ret = __vmulfp((aVF), (bVF));;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (VecFloatRef<INDEX1> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vmulfp((aVF), (bVF));;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (VecFloatRef<INDEX1> a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { b }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((aVF), (floatVec));; };
    return VecFloat(ret);
}

__forceinline VecFloat operator * (VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    ret = __vmulfp((aVF), (b.mV));;
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (VecFloatRefIndex::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    ret = __vmulfp((aVF), (bVF));;
    return VecFloat(ret);
}

__forceinline VecFloat operator * (VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    ret = __vmulfp((aVF), (bVF));;
    return VecFloat(ret);
}

__forceinline VecFloat operator * (VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { b }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((aVF), (floatVec));; };
    return VecFloat(ret);
}

__forceinline VecFloat operator * (const float& a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { a }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((b.mV), (floatVec));; };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator * (const float& a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { a }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((bVF), (floatVec));; };
    return VecFloat(ret);
}

__forceinline VecFloat operator * (const float& a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic floatVec; { ::rw::math::vpu::VectorIntrinsic u = { a }; floatVec = __vspltw(u, 0); }; ret = __vmulfp((bVF), (floatVec));; };
    return VecFloat(ret);
}


__forceinline VecFloat operator / (VecFloat::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (VecFloat::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (VecFloat::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (a.mV)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (VecFloat::InParam a, const float& b)
{
    VectorIntrinsic ret;
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(b); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (a.mV)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (VecFloatRef<INDEX1> a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1, VectorAxis INDEX2>
__forceinline VecFloat operator / (VecFloatRef<INDEX1> a, VecFloatRef<INDEX2> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX2)); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (VecFloatRef<INDEX1> a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (VecFloatRef<INDEX1> a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { aVF = __vspltw(a.mV, (int)(INDEX1)); };
    VectorIntrinsic bVec;
    { ::rw::math::vpu::VectorIntrinsic u = { b }; bVec = __vspltw(u, 0); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(b); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (VecFloatRefIndex::InParam a, VecFloat::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(b.mV);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (b.mV), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (b.mV), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (VecFloatRefIndex::InParam a, VecFloatRef<INDEX1> b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { bVF = __vspltw(b.mV, (int)(INDEX1)); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (VecFloatRefIndex::InParam a, VecFloatRefIndex::InParam b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    VectorIntrinsic bVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(b.mIndex))), 0); bVF = __vperm(b.mV, b.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; { ::rw::math::vpu::VectorIntrinsic secondEstimate; { ::rw::math::vpu::VectorIntrinsic estimate, vOne; estimate = __vrefp(bVF);; vOne = __vcfsx(__vspltisw(1), 0); ; secondEstimate = __vmaddfp((estimate), (__vnmsubfp((estimate), (bVF), (vOne))), (estimate)); }; ::rw::math::vpu::VectorIntrinsic vOne; vOne = __vcfsx(__vspltisw(1), 0); ; reciprocal = __vmaddfp((secondEstimate), (__vnmsubfp((secondEstimate), (bVF), (vOne))), (secondEstimate)); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (VecFloatRefIndex::InParam a, const float& b)
{
    VectorIntrinsic ret;
    VectorIntrinsic aVF;
    { ::rw::math::vpu::VectorIntrinsic permConstant = __vspltw(__lvsl(0, (4*(int)(a.mIndex))), 0); aVF = __vperm(a.mV, a.mV, permConstant); };
    { ::rw::math::vpu::VectorIntrinsic reciprocal; float floatRecip = 1.0f/(b); { ::rw::math::vpu::VectorIntrinsic u = { floatRecip }; reciprocal = __vspltw(u, 0); }; ret = __vmulfp((reciprocal), (aVF)); };
    return VecFloat(ret);
}

__forceinline VecFloat operator / (const float& a, VecFloat::InParam b)
{
    return VecFloat(a / b.GetFloat());
}

template<VectorAxis INDEX1>
__forceinline VecFloat operator / (const float& a, VecFloatRef<INDEX1> b)
{
    return VecFloat(a / b.GetFloat());
}

__forceinline VecFloat operator / (const float& a, VecFloatRefIndex::InParam b)
{
    return VecFloat(a / b.GetFloat());
}


}}}
