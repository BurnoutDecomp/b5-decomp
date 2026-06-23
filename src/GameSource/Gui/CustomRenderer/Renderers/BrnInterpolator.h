#ifndef BRN_INTERPOLATOR_H
#define BRN_INTERPOLATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// BrnGui::Interpolator<T> - a small GUI helper that maps an input value onto a
// saturated [0,1] fraction between a start key and an end key, then linearly
// interpolates the keyed range by that fraction. The custom HUD renderers
// (e.g. BoostBarRenderer) embed Interpolator<f32> members to ease bar values.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::Interpolator<float>::GetCurrentValue @ 0x82448898
//
// The X360 build holds two key floats; an unset key is the -FLT_MAX sentinel
// (-3.4028235e38f). IsValid() is true only when BOTH keys are set (neither is the
// sentinel). The asm's lvlx/vspltw/vmaddfp block is a single-lane VMX expression of
// the scalar lerp `mStart + t*(mEnd - mStart)` (one float in a vector register), not
// a multi-stage vectorised pipeline -- it lowers cleanly to the scalar form below.

namespace BrnGui
{
template <typename T>
class Interpolator
{
public:
    // Sentinel value an unset key holds (-FLT_MAX). A key equal to this is "not set".
    static const f32 KF_UnsetSentinel; // = -3.4028235e38f

    // True only when both the start and the end key have been set (neither is the
    // -FLT_MAX sentinel). The X360 IsValid() check at @0x82448898 (asserted by the
    // caller via the inlined BrnBoostBarRenderer.h:102 site).
    bool IsValid() const
    {
        return mtStart != static_cast<T>(KF_UnsetSentinel) &&
               mtEnd   != static_cast<T>(KF_UnsetSentinel);
    }

    // @0x82448898 -- KEYSTONE (deferred). See the definition below: the X360 body reads a
    // FOUR-float layout (values @+0/+4, keys @+8/+0xC) and returns a non-standard
    // (value1-value0)*value0 + clamp((lInput-key0)/(key1-key0),0,1), not a textbook lerp.
    // This 2-float model + its IsValid() are an incomplete slice pending the VMX pass.
    T GetCurrentValue(T lInput) const;

    // NOTE: the real X360 layout is four floats (value0@+0, value1@+4, key0@+8, key1@+0xC);
    // only the leading pair is modelled here for the deferred slice.
    T mtStart;
    T mtEnd;
};

template <typename T>
const f32 Interpolator<T>::KF_UnsetSentinel = -3.4028235e38f;

template <typename T>
T Interpolator<T>::GetCurrentValue(T lInput) const
{
    // KEYSTONE -- NOT faithfully reconstructed (deferred to a careful VMX-aware pass).
    // The X360 @0x82448898 reads FOUR floats: a value pair @+0/+4 AND a KEY pair @+8/+0xC.
    // IsValid() actually tests the KEY pair @+8/+0xC (lfs 8 / lfs 0xC vs the -FLT_MAX
    // sentinel), not the value pair this 2-float model exposes. It then computes a clamped
    // fraction  t = clamp((lInput - key0) / (key1 - key0), 0, 1)  (fdivs + two fsel) and
    // returns the NON-standard  (value1 - value0)*value0 + t  (vmaddfp v0 = v12*v0 + v13
    // with v12=splat(value1-value0), v0=splat(value0), v13=splat(t)) -- i.e. NOT the
    // textbook lerp mStart + t*(mEnd-mStart). Faithful reconstruction needs the 4-float
    // layout + the exact VMX lane roles verified; until then this is an honest floor.
    CGS_ASSERT(IsValid(), "IsValid()");
    (void)lInput;
    return mtStart;
}
}

#endif
