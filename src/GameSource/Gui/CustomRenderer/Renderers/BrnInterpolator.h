#ifndef BRN_INTERPOLATOR_H
#define BRN_INTERPOLATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// BrnGui::Interpolator<T> - a small GUI helper that maps an input value onto a
// saturated [0,1] fraction between two keys, then offsets a value pair by that
// fraction. The custom HUD renderers (e.g. BoostBarRenderer) embed Interpolator<f32>
// members to ease bar values.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::Interpolator<float>::GetCurrentValue @ 0x82448898
//
// LAYOUT (four floats, from the @0x82448898 asm offsets):
//   mtValue0 @+0, mtValue1 @+4, mtKey0 @+8, mtKey1 @+0xC.
// An unset key holds the -FLT_MAX sentinel (-3.4028235e38f). IsValid() is true only
// when BOTH KEYS are set (neither key is the sentinel) -- the X360 IsValid() reads the
// KEY pair @+8/+0xC (lfs 8 / lfs 0xC vs -FLT_MAX), NOT the value pair.
//
// The asm's lvlx/vspltw/vsubfp/vmaddfp block is a single-lane VMX expression (one float
// broadcast across a vector register, lane0 read back), self-contained with no external
// types or callees -- it lowers cleanly to the scalar form below. It is NOT the textbook
// lerp value0 + t*(value1-value0); the guest computes a non-standard
//   (value1 - value0)*value0 + clamp((lInput - key0)/(key1 - key0), 0, 1).

namespace BrnGui
{
template <typename T>
class Interpolator
{
public:
    // Sentinel value an unset key holds (-FLT_MAX). A key equal to this is "not set".
    static const f32 KF_UnsetSentinel; // = -3.4028235e38f

    // True only when BOTH keys have been set (neither key is the -FLT_MAX sentinel).
    // The X360 GetCurrentValue @0x82448898 reads the KEY pair @+8/+0xC and compares each
    // to the sentinel (the caller asserts IsValid() via the inlined BrnBoostBarRenderer.h:102
    // site).
    bool IsValid() const
    {
        return mtKey0 != static_cast<T>(KF_UnsetSentinel) &&
               mtKey1 != static_cast<T>(KF_UnsetSentinel);
    }

    // @0x82448898 -- map lInput onto a saturated [0,1] fraction between the two keys, then
    // return the non-standard (value1 - value0)*value0 + that fraction. See the definition
    // below for the exact faithful lowering of the VMX block.
    T GetCurrentValue(T lInput) const;

    // Four-float guest layout (from the @0x82448898 asm offsets).
    T mtValue0; // @+0
    T mtValue1; // @+4
    T mtKey0;   // @+8
    T mtKey1;   // @+0xC
};

template <typename T>
const f32 Interpolator<T>::KF_UnsetSentinel = -3.4028235e38f;

template <typename T>
T Interpolator<T>::GetCurrentValue(T lInput) const
{
    // Caller-side assert (inlined IsValid() check at BrnBoostBarRenderer.h:102 in the X360).
    CGS_ASSERT(IsValid(), "IsValid()");

    // t = (lInput - key0) / (key1 - key0)   (fsubs f31,f0 / fsubs f13,f0 / fdivs).
    T t = (lInput - mtKey0) / (mtKey1 - mtKey0);

    // Saturate to [0,1] with the guest's two fsel clamps:
    //   fneg f12,t ; fsel f0, f12, 0.0, t   -> t = (-t >= 0) ? 0.0 : t   (lower clamp)
    if (-t >= static_cast<T>(0))
        t = static_cast<T>(0);
    //   fsubs f12, 1.0, t ; fsel f0, f12, t, 1.0 -> t = (1.0 - t >= 0) ? t : 1.0 (upper clamp)
    if (static_cast<T>(1) - t < static_cast<T>(0))
        t = static_cast<T>(1);

    // vmaddfp v0 = splat(value1 - value0) * splat(value0) + splat(t), lane0 returned.
    return (mtValue1 - mtValue0) * mtValue0 + t;
}
}

#endif
