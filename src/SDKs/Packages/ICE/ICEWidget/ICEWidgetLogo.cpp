// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.cpp
//
// ICE::ICEWidgetLogo::Render @0x825346E0 -- the dev-only ICE camera-take editor's
// animated "ICE" logo. It draws the literal text "ICE" as shadowed on-screen text
// whose drawn height pulses/wobbles each frame, driven by a per-frame timer fed
// through the fixed-point ICE Sin.
//
// RECONSTRUCTION (from the X360 asm; no Feb-2007 source, no ICEWidgetLogo DWARF):
//
//   * ANIMATION PHASE -- the asm reads the timer at this+8 and turns it into a
//     radians angle, then a u16 ICE::Angle:
//       lwz  r11, 8(r31)                        -> miTimer
//       v29  = miTimer * 0.016666668 * 6.2831855  (timer * (1/60) * 2*pi)
//       ICE::Angle::SetFromVecFloat(&v29)         (radians -> u16 Angle, high word)
//       v6   = ICE::ICEMath::Sin(HIWORD(v29))     (the oscillation source)
//     The 1/60 (flt_820139F8) and 2*pi (flt_82001C94) make a full Sin cycle every
//     60 timer steps -- one second at the ICE 60-step rate.
//
//   * PHASE ADVANCE / WRAP -- the timer steps each frame and wraps at 60:
//       *(this+8) = (miTimer + 1) % 60
//     (asm: addi r8,r11,1 ; magic-multiply divide by 60 ; mulli 0x3C ; subf ; stw).
//     De-strength-reduced back to `% 60`.
//
//   * PULSE MAGNITUDE -- the negated Sin is lower-clamped, then mapped into a
//     half-height that swings around the logo's mid-height:
//       _FP10 = -v6                                            (fneg)
//       _FP10 = (-v6 >= 0) ? KF_ICE_LOGO_PULSE_FLOOR : v6      (fsel f10,f10,f11,f1)
//       halfH = ((_FP10 * 0.2) + 0.80000001) * 32.0            (fmadds ; fmuls)
//     so the half-height oscillates in [0.8*32, 1.0*32] = [25.6, 32.0] px and the
//     drawn text spans mfY +/- halfH (the wobble). The X360 marshals these pulsed
//     spans into vector stack buffers (v33/v34/v35 -> v36..v39) as the variadic
//     slots its vararg ABI requires; "ICE" carries no `%` specifiers, so they are
//     inert in the formatted output -- the visible effect is purely the animated
//     position. We keep the pulse computation (semantic parity) and pass it through
//     as the trailing varargs the X360 builds.
//
//   * TEXT DRAW -- ICE::ICERender::ScrShadowPrintf at the widget position, size 64:
//       r3 = a2                 -> liX  (the caller-supplied base X)
//       r4 = (s32)mfX           -> liY  (fctiwz f13 ; stfiwx ; lwz r4)
//       64.0 (flt_82020A7C)     -> lfSize
//       aIce "ICE"              -> the format/text
//     (mfY is converted to int and marshaled as a trailing vararg alongside the
//     pulse spans; "ICE" consumes none of them.)
//
// FLAG: the member names/offsets (mfX@0/mfY@4/miTimer@8), the 60-step wrap, the
//       1/60 & 2*pi phase scale, the pulse map constants (0.2 / 0.8 / 32.0), the
//       pulse lower-clamp floor (KF_ICE_LOGO_PULSE_FLOOR, the fsel `flt_82001CC0`
//       operand -- modelled as 0.0, the natural lower clamp; value not in the
//       exports), the size 64.0, the "ICE" literal, and the (liX=a2, liY=(s32)mfX)
//       call layout are all DERIVED FROM ASM (DWARF-sparse).
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp"

namespace ICE
{
    // Animation period: the timer wraps here, one full Sin cycle (flt_820139F8 =
    // 1/60 -> 60 steps make `timer * (1/60) * 2*pi` sweep a full revolution).
    static const s32 KI_ICE_LOGO_PERIOD = 60;

    // Phase scale: timer -> radians = timer * (1/60) * 2*pi (flt_820139F8 *
    // flt_82001C94).
    static const f32 KF_ICE_LOGO_INV_PERIOD = 0.016666668f;   // 1/60
    static const f32 KF_ICE_LOGO_TWO_PI     = 6.2831855f;     // 2*pi (single precision)

    // Pulse map: half-height = (clampedSin * 0.2 + 0.8) * 32.0 (flt_82004744 *,
    // flt_820054C8 +, flt_8207ACE0 *).
    static const f32 KF_ICE_LOGO_PULSE_GAIN = 0.2f;
    static const f32 KF_ICE_LOGO_PULSE_BIAS = 0.80000001f;
    static const f32 KF_ICE_LOGO_PULSE_SIZE = 32.0f;

    // Lower clamp on the negated Sin (the fsel `flt_82001CC0` operand): when -sin
    // is non-negative the value is floored to this constant. Modelled as 0.0 (the
    // natural lower clamp; exact value not in the exports). FLAG: asm-derived.
    static const f32 KF_ICE_LOGO_PULSE_FLOOR = 0.0f;

    // Drawn text size (flt_82020A7C) and the logo string (aIce).
    static const f32   KF_ICE_LOGO_SIZE = 64.0f;
    static const char* KS_ICE_LOGO_TEXT = "ICE";

    // ------------------------------------------------------------------------
    // ICEWidgetLogo::Render @0x825346E0
    // ------------------------------------------------------------------------
    void ICEWidgetLogo::Render(s32 liScreenX)
    {
        // --- animation phase: timer -> radians -> u16 Angle -> Sin -------------
        const f32 lfRadians = (f32)miTimer * KF_ICE_LOGO_INV_PERIOD * KF_ICE_LOGO_TWO_PI;

        // The X360 carries the radians scalar broadcast through a SIMD vector
        // (vspltw lane 0); model it as the VecFloat with the value in every lane.
        VecFloat lvfRadians;
        lvfRadians.x = lvfRadians.y = lvfRadians.z = lvfRadians.w = lfRadians;

        Angle leAngle;
        leAngle.SetFromVecFloat(lvfRadians);         // radians (x lane) -> u16 unit
        const f32 lfSin = ICEMath::Sin(leAngle);     // the oscillation source

        // Advance + wrap the phase for next frame (`(timer + 1) % 60`).
        miTimer = (miTimer + 1) % KI_ICE_LOGO_PERIOD;

        // --- pulse magnitude: lower-clamp -(sin), then map to a half-height -----
        // fsel f10,f10,f11,f1: `(-sin >= 0) ? floor : sin`.
        const f32 lfNegSin = -lfSin;
        const f32 lfClamped = (lfNegSin >= 0.0f) ? KF_ICE_LOGO_PULSE_FLOOR : lfSin;
        const f32 lfHalfHeight =
            ((lfClamped * KF_ICE_LOGO_PULSE_GAIN) + KF_ICE_LOGO_PULSE_BIAS) * KF_ICE_LOGO_PULSE_SIZE;

        // The pulsed vertical span the X360 builds into its variadic vector slots
        // (mfY +/- the oscillating half-height). "ICE" consumes no format args, so
        // these only carry the animation through the call shape.
        const s32 liPulseTop    = (s32)(mfY - lfHalfHeight);
        const s32 liPulseBottom = (s32)(mfY + lfHalfHeight);

        // --- draw the shadowed "ICE" text -------------------------------------
        // liX = the caller-supplied base X (a2); liY = the widget's X-derived line;
        // size 64; the pulsed span + mfY ride along as the trailing varargs the
        // X360 marshals (inert for the specifier-free "ICE").
        ICERender::ScrShadowPrintf(liScreenX, (s32)mfX, KF_ICE_LOGO_SIZE, KS_ICE_LOGO_TEXT,
                                   (s32)mfX, (s32)mfY, liPulseTop, liPulseBottom);
    }
}
