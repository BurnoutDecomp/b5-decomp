#ifndef SDKS_PACKAGES_ICE_ICEPOINT_HPP
#define SDKS_PACKAGES_ICE_ICEPOINT_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEPoint.hpp
//
// ICE::Cubic1D -- a 1-D cubic-spline / critically-damped follower used by the
// ICE camera system. A Cubic1D drives one scalar (Val) toward a target
// (ValDesired) over a fixed duration, fitting a Hermite-style cubic whose
// coefficients (Coeff[0..3]) are rebuilt whenever a new seek begins, with
// optional first- and second-derivative clamping.
//
// This file currently models a MINIMAL but FAITHFUL slice: the Cubic1D field
// layout plus the full Cubic1D method set. Only the three functions reconstructed
// in this TU -- ClampDerivative, ClampSecondDerivative, and Update -- have bodies
// (in ICEPoint.cpp); the remaining methods are declaration-only until their own
// TUs land (the per-TU cl /c gate does not link, so declarations suffice).
//
// The future full ICEPoint.hpp TU -- Cubic2D / Cubic3D (which embed Cubic1D by
// value and use rw::math Vector2 / Vector3) plus ICE::Point and the inline
// accessors -- EXTENDS this header in place. It must NOT re-declare Cubic1D in a
// parallel header.
//
// LAYOUT (32-bit; struct-relative offsets, confirmed by the float-index / byte
// accesses in the three reconstructed functions):
//   @0x00  f32 Val          current value
//   @0x04  f32 dVal         current first derivative (velocity)
//   @0x08  f32 ValDesired   target value
//   @0x0C  f32 dValDesired  target first derivative
//   @0x10  f32 Coeff[4]     cubic coefficients (C0 t^3 + C1 t^2 + C2 t + C3)
//   @0x20  f32 time         normalised progress 0..1
//   @0x24  f32 duration     seek duration (seconds)
//   @0x28  s16 state        state machine (1 = running, 2 = (re)start, 0 = arrived)
//   @0x2A  s16 flags
//
// (The scalars are 32-bit float -> f32, and the state/flags are short int -> s16,
// per the project type vocabulary in types.hpp.)
// ============================================================================

namespace ICE
{

// 1-D cubic-spline follower.
struct Cubic1D
{
    f32 Val;          // @0x00
    f32 dVal;         // @0x04
    f32 ValDesired;   // @0x08
    f32 dValDesired;  // @0x0C
    f32 Coeff[4];     // @0x10
    f32 time;         // @0x20
    f32 duration;     // @0x24
    s16 state;        // @0x28
    s16 flags;        // @0x2A

    // --- Full Cubic1D method set. Only the three reconstructed in this TU
    //     (Update / ClampDerivative / ClampSecondDerivative) have bodies; the
    //     rest are declaration-only until their own TUs land. ---

    Cubic1D(s16 state, f32 duration);

    // Default-init applied to every Cubic1D in a fresh ICETake: all scalars zero,
    // the duration set to 1.0, the flags set to 1, the state cleared.
    Cubic1D()
        : Val(0.0f), dVal(0.0f), ValDesired(0.0f), dValDesired(0.0f), Coeff{},
          time(0.0f), duration(1.0f), state(0), flags(1)
    {
    }

    void Update(f32 dt, f32 derivativeLimit, f32 secondDerivativeLimit);
    void Snap();

    void SetVal(f32 val);
    void SetdVal(f32 dval);
    void SetValDesired(f32 valDesired);
    void SetdValDesired(f32 dvalDesired);

    void SetDuration(f32 duration);
    void SetState(s16 state);
    void SetFlags(s16 flags);

    f32 GetVal(f32 t) const;
    f32 GetdVal(f32 t) const;
    f32 GetddVal(f32 t) const;
    f32 GetVal() const;
    f32 GetdVal() const;
    f32 GetddVal() const;

    f32 GetValDesired() const;
    f32 GetdValDesired() const;

    f32 GetDerivative(f32 t) const;
    f32 GetSecondDerivative(f32 t) const;

    void ClampDerivative(f32 limit);
    void ClampSecondDerivative(f32 limit);

    void MakeCoeffs();

    s32 HasArrived() const;
};

// ============================================================================
// ICE::Cubic3D -- three Cubic1D followers driving the X/Y/Z of a 3-D quantity
// (e.g. the ICE camera's accel-offset and forward followers).
//
// GROWN here (minimal slice) for ICECameraMover, which embeds two Cubic3D by value
// (its mAccelOffset and mForward members). The full ICEPoint.hpp TU -- Cubic2D, the
// rw::math Vector2/Vector3 convenience accessors, and ICE::Point -- EXTENDS this
// header in place; it must NOT re-declare Cubic3D in a parallel header.
//
// LAYOUT: three Cubic1D laid out back-to-back (44 bytes each = 132 bytes total).
// Attested by ICECameraMover::Construct (which builds two 132-byte blocks) and by
// ICECameraMover::UpdateForwardVector (which Updates the three component Cubic1D at
// +0x00 / +0x2C / +0x58 of the mForward block).
// ============================================================================
struct Cubic3D
{
    Cubic1D maComponents[3];   // @0x00 X, @0x2C Y, @0x58 Z   (3 * 44 = 132 bytes)
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEPOINT_HPP
