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

        // DWARF h:73/h:82 -- declaration-only (their own ledger functions).
        void Construct(f32 lfInitialValue);
        void SetCurrentValue(f32 lfValue);

        // ⭐ DWARF h:77 -- BODIED INLINE 2026-08-01 (orbit-camera wave). A single load off
        // the member this header already names at +0x08. The console inlines it at every
        // site: CameraSphericalRotationController::GetPitchRotationAngleDegs is nothing but
        // this fetch, and BehaviourRotateAboutVehicle::Update @0x82249518 emits it as a bare
        // `lfs f13, 0x2C(controller)` (controller +0x24 mPitchMover, +0x08 mfCurrentValue).
        // Leaving it declaration-only made it an unresolved external for every caller.
        f32  GetCurrentValue() const { return mfCurrentValue; }

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

    public:
        // FLAG (access level) -- WIDENED private -> public 2026-08-01 (orbit-camera wave), the
        //   same widening and for the same reason as Behaviour.h's six base fields. The DWARF
        //   marks these private with Construct/Get/SetCurrentValue as the intended path, but
        //   OWNERS OF AN EMBEDDED SmoothMover WRITE THEM DIRECTLY on the console -- and they do
        //   it as an INLINED reset, not through a setter, so there is no console function to
        //   route through. CameraSphericalRotationController::Construct's ten-store block
        //   (three witnesses) writes mfCurrentSpeed and mfCurrentValue while deliberately
        //   LEAVING mfCenteringRate alone; with these private that block could only have been
        //   written by offset, which the x64 rule forbids. Widening changes no layout and no
        //   semantics.
        //   DELETE-WHEN: a console setter for the speed field is found (SetCurrentValue only
        //   covers the value), and every direct writer is re-expressed through the pair.
        f32 mfCenteringRate;   // h:117 (+0x00)
        f32 mfCurrentSpeed;    // h:118 (+0x04)
        f32 mfCurrentValue;    // h:119 (+0x08)
    };
}
}
}
