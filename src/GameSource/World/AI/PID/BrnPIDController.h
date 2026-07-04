#ifndef BRN_AI_PID_CONTROLLER_H
#define BRN_AI_PID_CONTROLLER_H

#include "types.hpp"

// =============================================================================
// BrnAI::PIDController
//   DWARF home: GameSource/World/AI/PID/BrnPIDController.{h,cpp}.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. A classic proportional / integral /
// derivative controller over a fixed-length ring buffer of recent (error, timeStep)
// samples. The AI driver embeds two of these (normal + drift, BrnAIDriver.cpp
// @this+0x1B34 / +0x1B98) and steps them each frame: Record(error, dt) then
// GetOutput().
//
// LAYOUT (X360, DWARF-confirmed offsets -- all members are POD, no embedded
// pointers, so the layout is host-stable and pinned below):
//   mafError[10]        @0x00   ring buffer of recorded errors
//   mafTimeStep[10]     @0x28   ring buffer of recorded time steps
//   mfPCoefficient      @0x50   proportional gain
//   mfICoefficient      @0x54   integral gain
//   mfDCoefficient      @0x58   derivative gain
//   mfCurrentIntegral   @0x5C   running (sliding-window) rectangular integral
//   mn8CurrentIndex     @0x60   ring head (-1 == empty)
//   mn8PreviousIndex    @0x61   previous ring slot (-1 == none)
//   mn8NumErrorsRecorded@0x62   clamp-saturating fill count
//   mn8Pad              @0x63   layout padding
// =============================================================================

namespace BrnAI
{
    // Number of ring-buffer slots (mafError / mafTimeStep length).
    static const s32 KI_PIDCONTROLLER_NUM_SLOTS = 10;

    // Coefficient index into the {P, I, D} array passed to Prepare().
    enum EPIDCoefficient
    {
        E_COEFFICIENT_P = 0,
        E_COEFFICIENT_I = 1,
        E_COEFFICIENT_D = 2,
        E_COEFFICIENT_COUNT = 3
    };

    struct PIDController
    {
        PIDController() {}

        // Initialise the controller with the {P, I, D} coefficients and clear history.
        void Prepare(const f32* lafCoefficientValues);

        // Push one (error, timeStep) sample; maintain the sliding-window integral.
        void Record(f32 lfError, f32 lfTimeStep);

        // The most-recent recorded error (0 if nothing recorded yet).
        f32 GetError();

        // (currentError - previousError) / currentTimeStep (guarded).
        f32 GetErrorDerivative();

        // The PID law: P*error + I*integral + D*derivative.
        f32 GetOutput();

        // ---- console-faithful member layout ----
        f32 mafError[KI_PIDCONTROLLER_NUM_SLOTS];      // +0x00
        f32 mafTimeStep[KI_PIDCONTROLLER_NUM_SLOTS];   // +0x28
        f32 mfPCoefficient;                            // +0x50
        f32 mfICoefficient;                            // +0x54
        f32 mfDCoefficient;                            // +0x58
        f32 mfCurrentIntegral;                         // +0x5C
        s8  mn8CurrentIndex;                           // +0x60
        s8  mn8PreviousIndex;                          // +0x61
        s8  mn8NumErrorsRecorded;                      // +0x62
        s8  mn8Pad;                                    // +0x63
    };
} // namespace BrnAI

#endif // BRN_AI_PID_CONTROLLER_H
