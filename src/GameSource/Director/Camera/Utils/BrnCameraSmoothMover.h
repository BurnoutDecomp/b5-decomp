#pragma once

#include "types.hpp"

// BrnDirector::Camera::Utils::SmoothMover - a one-dimensional smoothed value mover
// (speed + value integrator with optional limits, limit-approach dampening, dead-zone
// braking and optional centering). DWARF home BrnCameraSmoothMover.h:47. This TU bodies
// Update and the two with-limits variants; Construct/Get/SetCurrentValue are their own
// ledger functions (declaration-only here). The two no-limits variants are bodied as the
// X360's "Not implemented yet" assert stubs (their whole attested bodies).
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    struct SmoothMover
    {
        // DWARF BrnCameraSmoothMover.h:51 (X360 float offsets pinned by the Update asm:
        // +0x00 max, +0x04 min, +0x08 dampening, +0x0C maxSpeed, +0x10 deadZone,
        // +0x14 brakingLag, +0x18 normalLag, +0x1C centeringRate, +0x20 centeringBlend,
        // +0x24 useCentering, +0x25 useLimits).
        struct Parameters
        {
            f32  mfMaxValue;            // h:52
            f32  mfMinValue;            // h:53
            f32  mfDampeningRange;      // h:55
            f32  mfMaxSpeed;            // h:57
            f32  mfDeadZoneHalfSize;    // h:59
            f32  mfBrakingLag;          // h:61
            f32  mfNormalLag;           // h:62
            f32  mfCenteringRate;       // h:64
            f32  mfCenteringRateBlend;  // h:65
            bool mbUseCentering;        // h:67
            bool mbUseLimits;           // h:68
        };

        // DWARF h:73/h:77/h:82 -- declaration-only (their own ledger functions).
        void Construct(f32 lfInitialValue);
        f32  GetCurrentValue() const;
        void SetCurrentValue(f32 lfValue);

        // @0x82223800 (this TU, DWARF h:89) -- dispatch on the parameter flags.
        void Update(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                    const Parameters& lParameters);

    private:
        // DWARF h:97/h:103 -- the no-limits variants; their whole X360 bodies are the
        // "Not implemented yet" asserts (cpp:82 / cpp:93), inlined into Update.
        void UpdateNoLimitsWithCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                         const Parameters& lParameters);
        void UpdateNoLimitsNoCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                       const Parameters& lParameters);

        // @0x8220D608 / @0x8220D820 (this TU, DWARF h:109/h:115).
        void UpdateWithLimitsWithCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                           const Parameters& lParameters);
        void UpdateWithLimitsNoCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                         const Parameters& lParameters);

        f32 mfCenteringRate;   // h:117 (+0x00)
        f32 mfCurrentSpeed;    // h:118 (+0x04)
        f32 mfCurrentValue;    // h:119 (+0x08)
    };
}
}
}
