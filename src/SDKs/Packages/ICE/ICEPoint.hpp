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
// layout (matched field-for-field against the DecFIGS DWARF ICEPoint.hpp:28-40)
// plus the full Cubic1D method set the DWARF declares. Only the three functions
// reconstructed in this TU -- ClampDerivative @0x8252CD30,
// ClampSecondDerivative @0x8252CD98, and Update @0x8252CE70 -- have bodies
// (in ICEPoint.cpp); the remaining methods are declaration-only until their own
// TUs land (the per-TU cl /c gate does not link, so declarations suffice).
//
// The future full ICEPoint.hpp TU -- Cubic2D / Cubic3D (which embed Cubic1D by
// value and use rw::math Vector2 / Vector3) plus ICE::Point and the inline
// accessors -- EXTENDS this header in place. It must NOT re-declare Cubic1D in a
// parallel header.
//
// LAYOUT (32-bit, DWARF ICEPoint.hpp:28-40; offsets X360-attested via the
// result[N] float-index / *(result+byte) accesses in the three reconstructed
// functions' pseudocode/asm):
//   @0x00  f32 Val          result[0]   current value
//   @0x04  f32 dVal         result[1]   current first derivative (velocity)
//   @0x08  f32 ValDesired   result[2]   target value
//   @0x0C  f32 dValDesired  result[3]   target first derivative
//   @0x10  f32 Coeff[4]     result[4..7] cubic coefficients (C0 t^3 + C1 t^2 + C2 t + C3)
//   @0x20  f32 time         result[8]   normalised progress 0..1
//   @0x24  f32 duration     result[9]   seek duration (seconds)
//   @0x28  s16 state        *(result+40) state machine (1 = running, 2 = (re)start, 0 = arrived)
//   @0x2A  s16 flags        *(result+42)
//
// (DWARF spells the scalars float32_t; float32_t is NOT a project type -> f32,
// and short int -> s16, per the project type vocabulary in types.hpp.)
// ============================================================================

namespace ICE
{

// ICEPoint.hpp:28 (DWARF). 1-D cubic-spline follower.
struct Cubic1D
{
    f32 Val;          // @0x00  result[0]
    f32 dVal;         // @0x04  result[1]
    f32 ValDesired;   // @0x08  result[2]
    f32 dValDesired;  // @0x0C  result[3]
    f32 Coeff[4];     // @0x10  result[4..7]
    f32 time;         // @0x20  result[8]
    f32 duration;     // @0x24  result[9]
    s16 state;        // @0x28  *(result+40)
    s16 flags;        // @0x2A  *(result+42)

    // --- Full Cubic1D method set (DWARF ICEPoint.hpp:42-87). Only the three
    //     below with a comment-marked address are reconstructed in this TU; the
    //     rest are declaration-only until their own TUs land. ---

    Cubic1D(s16 state, f32 duration);

    // Default-init applied to every Cubic1D in a fresh ICETake: all scalars zero,
    // the duration set to 1.0, the flags set to 1, the state cleared.
    Cubic1D()
        : Val(0.0f), dVal(0.0f), ValDesired(0.0f), dValDesired(0.0f), Coeff{},
          time(0.0f), duration(1.0f), state(0), flags(1)
    {
    }

    void Update(f32 dt, f32 derivativeLimit, f32 secondDerivativeLimit);   // @0x8252CE70
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

    void ClampDerivative(f32 limit);          // @0x8252CD30
    void ClampSecondDerivative(f32 limit);    // @0x8252CD98

    void MakeCoeffs();

    s32 HasArrived() const;
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEPOINT_HPP
