#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h"

// rw::math::vpu::SLerp -- the ENGINE'S OWN transform blend (X360 0x82216858). See the
// banner on BlendTransform below for why this file uses it rather than any local maths.
#include "rw/math/vpu/matrix44affine_operation.h"

// ⚠️ FLAG PC QUALITY-OF-LIFE LAYER -- see the banner in CgsFrameInterpolation.h.
// No X360 counterpart exists for the alpha bookkeeping in this file. The one piece of
// MATHS in it is not ours at all -- it is the console's SLerp, called directly.
namespace CgsSystem
{
    namespace FrameInterpolation
    {
        namespace
        {
            f32  sfAlpha        = 1.0f;
            f32  sfFrameSeconds = 1.0f / 60.0f;
            bool sbEnabled      = false;

            // Below this the blend is a no-op either way and the caller gets the endpoint
            // back untouched (bit-identical to not blending at all).
            const f32 KF_ALPHA_EPSILON = 1.0f / 512.0f;
        }

        void SetAlpha(f32 lfAlpha)
        {
            if (lfAlpha < 0.0f)
                lfAlpha = 0.0f;
            if (lfAlpha > 1.0f)
                lfAlpha = 1.0f;
            sfAlpha = lfAlpha;
        }

        f32 GetAlpha()
        {
            return sfAlpha;
        }

        void SetEnabled(bool lbEnabled)
        {
            sbEnabled = lbEnabled;
        }

        bool IsEnabled()
        {
            return sbEnabled;
        }

        void SetFrameSeconds(f32 lfSeconds)
        {
            // Clamped so a breakpoint, a stalled load or a window drag cannot hand a
            // consumer a multi-second step. One tenth of a second is six 60 Hz ticks.
            if (lfSeconds < 0.0f)
                lfSeconds = 0.0f;
            if (lfSeconds > 0.1f)
                lfSeconds = 0.1f;
            sfFrameSeconds = lfSeconds;
        }

        f32 GetFrameSeconds()
        {
            return sfFrameSeconds;
        }

        f32 BlendScalar(f32 lfPrevious, f32 lfCurrent, f32 lfAlpha)
        {
            if (lfAlpha >= 1.0f - KF_ALPHA_EPSILON)
                return lfCurrent;
            if (lfAlpha <= KF_ALPHA_EPSILON)
                return lfPrevious;
            return lfPrevious + (lfCurrent - lfPrevious) * lfAlpha;
        }

        // ====================================================================
        // ⭐ THE BLEND IS THE CONSOLE'S OWN, AND DELIBERATELY SO.
        //
        // rw::math::vpu::SLerp @0x82216858 is what this engine blends transforms with --
        // BrnDirector::Camera::BehaviourGameplayExternal, BehaviourIceAnim, BehaviourRoadRunner
        // and BehaviourInterpolate all go through it. It blends the three axis rows along the
        // arc between them (falling back to a plain lerp inside the console's own 2-degree
        // threshold), re-normalises each, and lerps the translation row.
        //
        // ⛔ IT REPLACED A HAND-ROLLED BLEND, AND THAT BLEND WAS A BUG. The first version of
        // this function nlerp'd the rows and re-orthonormalised them by Gram-Schmidt --
        // rebuilding the x and y rows from z, with a measured handedness sign -- and rejected
        // the blend outright when the two poses were more than 30 degrees or 25 metres apart,
        // on the theory that such a jump had to be a cut rather than motion.
        //
        // A ROAD WHEEL BREAKS THAT ASSUMPTION IMMEDIATELY. A 0.331 m wheel at 20 m/s turns
        // about 57 degrees per 60 Hz tick -- past the invented threshold -- so every wheel on
        // a moving car took the "this is a cut" arm and was handed back UNBLENDED. MEASURED
        // on the boot run: the body changed on 13359 of 18162 frames with essentially every
        // hold-run one frame long, while wheel rows held for two and three frames (1520 and
        // 644 runs) -- the wheels stepping at 60 Hz inside a 130 fps stream, which is exactly
        // what they looked like.
        //
        // SLerp has no such threshold, because it does not need one: a 57-degree arc is what
        // spherical interpolation is FOR. It also makes no handedness assumption and rebuilds
        // no row from the others.
        //
        // WHAT IS GIVEN UP, STATED PLAINLY:
        //   * A TELEPORT will smear over one frame instead of snapping -- SLerp cannot tell a
        //     respawn from a fast movement, and neither could the thresholds it replaced (they
        //     just guessed, and guessed wrong for wheels). Discontinuities are handled where
        //     they are actually KNOWN: the producer calls PoseTrack::Reset. That is the honest
        //     seam, and it needs no magic number.
        //   * A rotation of more than half a turn per tick aliases the short way round -- the
        //     wagon-wheel effect, inherent to interpolating a sampled rotation and not
        //     something a different blend would fix. For this engine that is a wheel above
        //     ~224 km/h, by which point the console's own spinning-blur wheel technique
        //     (RenderRaceCar's technique 0) is what is on screen anyway.
        //
        // Scale: SLerp normalises the three axis rows, so it is only lossless on an
        // orthonormal 3x3. MEASURED before adopting it -- the race car's published body and
        // wheel transforms are exactly orthonormal (row magnitudes 1.000000, row dots
        // 0.000000); the wheels' scale lives in the separate GetWheelScaleMatrix, which is
        // composed after this and never blended.
        // ====================================================================
        rw::math::vpu::Matrix44Affine BlendTransform(
            const rw::math::vpu::Matrix44Affine& lrPrevious,
            const rw::math::vpu::Matrix44Affine& lrCurrent,
            f32                                  lfAlpha)
        {
            if (lfAlpha >= 1.0f - KF_ALPHA_EPSILON)
                return lrCurrent;
            if (lfAlpha <= KF_ALPHA_EPSILON)
                return lrPrevious;

            // The fourth argument is SLerp's OUT parameter (the rotation remaining after the
            // blend). The console's own callers pass a stack slot they never read -- DecFIGS
            // names one of them `lUnusedAngle` -- and SLerp null-checks it, so nothing is
            // dropped by passing none.
            return rw::math::vpu::SLerp(lrPrevious, lrCurrent, lfAlpha, 0);
        }
    }
}
