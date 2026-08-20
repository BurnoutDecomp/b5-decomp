#pragma once

#include "types.hpp"
// The VENDOR rw math tree (rw/math/vpu/types.h), not the SDKs/EATech one: the two are
// parallel reconstructions of the same rwmath types and are mutually exclusive in a TU.
// This is the one BrnDirector::Camera::Camera and BrnWorldModule are compiled against, and
// they are this header's consumers.
#include "rw/math/vpu/types.h"   // Matrix44Affine / Vector4 (the pose blend)

// ============================================================================
// GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h
//
// ⚠️ FLAG PC QUALITY-OF-LIFE LAYER -- NOT AN X360 FUNCTION, NOT A CONSOLE FILE.
//
// The console runs its simulation LOCKED TO THE DISPLAY: BrnGameModule::Construct
// @0x823C9EA8 seeds the two CgsSystem::Timers with 1/<vsync refresh rate> and
// CgsSystem::FrameRateManager with a minimum of ONE simulation step per rendered
// frame (mi8FrameRateMinSteps = 1, mi8FrameRateMaxSteps = 3), so "one render frame"
// and "one 60 Hz simulation tick" are the same thing and nothing ever needs to be
// interpolated. That identity is exactly what breaks on a PC that renders faster
// than 60 Hz: with a step forced per frame, a 144 Hz panel with VSync off ran the
// whole simulation -- physics, timers, camera, streaming budget -- at 144 ticks a
// second, i.e. 2.4x speed.
//
// The fix (see CgsFrameRate.cpp) decouples the two: the simulation is paced off the
// real-time clock at a fixed 60 Hz and the renderer runs as fast as it likes. That
// leaves rendered frames which fall BETWEEN two simulation ticks, and this header is
// what those frames use to fill the gap:
//
//   * SetAlpha / GetAlpha -- the fraction of a simulation step that has elapsed since
//     the last tick, in [0,1]. The render leg blends the previous tick's pose with the
//     current tick's pose by it, so a 60 Hz simulation reads as continuous motion at
//     any render rate. 1.0 means "show the newest state" and is what console-mode
//     pacing publishes, so the console path is bit-identical to what it was.
//   * SetFrameSeconds / GetFrameSeconds -- the REAL seconds the last rendered frame
//     took. Only for the PC bring-up stand-ins that advance something per render frame
//     and have no simulation tick to hang off (the world module's tour camera). Never
//     use it for anything the simulation owns.
//   * BlendTransform -- the pose blend itself.
//
// The publisher is BrnGameModule (one call per rendered frame, from GameMain); the
// consumers are the render legs. It is a free-function pair over file-scope state
// rather than a game-module accessor so that render legs deep in the world module can
// read it without taking a dependency on the game module.
//
// DELETE-WHEN: never, unless the host goes back to a vsync-locked 60 Hz simulation.
// This is a deliberate, permanent PC deviation, not a stand-in for missing console code.
// ============================================================================
namespace CgsSystem
{
    namespace FrameInterpolation
    {
        // The blend factor for THIS rendered frame, in [0,1]. 0 = the previous
        // simulation tick's state, 1 = the current one.
        void SetAlpha(f32 lfAlpha);
        f32  GetAlpha();

        // True while the simulation is actually being paced independently of the
        // renderer (i.e. GetAlpha can be < 1 and blending is worth doing). False in
        // console-locked mode, where every consumer must read the current state
        // directly -- no blend, no stored previous pose, no cost.
        void SetEnabled(bool lbEnabled);
        bool IsEnabled();

        // Real wall-clock seconds taken by the previous rendered frame.
        void SetFrameSeconds(f32 lfSeconds);
        f32  GetFrameSeconds();

        // Blend two rigid poses. This is a thin wrapper over rw::math::vpu::SLerp
        // @0x82216858 -- THE ENGINE'S OWN transform blend, the one every director camera
        // behaviour uses. See the banner on the definition for why it is the console's
        // function and not local maths, and what a hand-rolled version got wrong.
        //
        // Discontinuities (a respawn, a place-on-track, a camera cut) are NOT detected
        // here by thresholding the values -- that was tried and it mis-classified a
        // spinning road wheel as a cut. They are handled at the producer, which knows:
        // call PoseTrack::Reset when the pose jumps for a reason.
        rw::math::vpu::Matrix44Affine BlendTransform(
            const rw::math::vpu::Matrix44Affine& lrPrevious,
            const rw::math::vpu::Matrix44Affine& lrCurrent,
            f32                                  lfAlpha);

        // Scalar blend, same clamp/short-circuit rules. For the camera FOV.
        f32 BlendScalar(f32 lfPrevious, f32 lfCurrent, f32 lfAlpha);

        // ====================================================================
        // PoseTrack -- the reusable "one rigid pose, interpolated" primitive.
        //
        // ANY entity whose visible transform is republished once per simulation tick and
        // consumed once per rendered frame needs this, or it steps at 60 Hz inside a
        // faster frame stream and visibly shudders against everything that does not. On
        // this build that is the race car's body and its six wheels; it will be every
        // physics-driven prop the moment the prop entity module starts receiving poses,
        // and anything else that moves under simulation after that.
        //
        // THE THREE CALLS, AND THE ORDER THEY MUST GO IN. The display transform (the one
        // the render leg reads) is REWRITTEN by Apply every frame, so the tick pipeline
        // has to bracket its producers:
        //
        //   per simulation tick, BEFORE the producers:  Restore(display)
        //   ... the producers write `display` for the entities they own ...
        //   per simulation tick, AFTER  the producers:  Latch(display)
        //   per rendered frame,  BEFORE the dispatch:   Apply(display, GetAlpha())
        //
        // ⛔ THE RESTORE IS THE ONE THAT LOOKS OPTIONAL. Apply rewrites the display
        // transform, so between ticks it holds a blend rather than a tick pose. Drop the
        // Restore and Latch will take that blend as the next tick on every tick where the
        // producer owning that entity did not run -- and the history drifts. Whether any
        // such tick exists depends on the module: the race car currently has none (every
        // active slot is written every tick by exactly one producer), a prop module where
        // only some props are simulated will have many. Cheap, and the failure it prevents
        // is the kind that looks like working interpolation until the numbers are plotted.
        //
        // ⛔ MEASURING IT: read the display transform AFTER Apply. Sampled before it, a
        // probe alternates between the sub-step's raw latch and the previous frame's blend
        // and plots as a sawtooth that reads exactly like a broken blend. That artifact cost
        // a diagnosis round on 2026-08-17.
        //
        // Apply is IDEMPOTENT -- it always blends the two stored ticks, never what the
        // display transform currently holds -- so a frame that dispatches several passes
        // (main view, shadow cascades, env-map faces) may call it per pass.
        //
        // Cuts are handled by BlendTransform: a jump too large for one tick (a spawn, a
        // place-on-track, a shot change) snaps instead of smearing.
        // ====================================================================
        class PoseTrack
        {
        public:
            // No history. The next Latch starts one.
            void Reset() { mbCurrentValid = false; mbPreviousValid = false; }

            void Restore(rw::math::vpu::Matrix44Affine& lrDisplayPose) const
            {
                if (mbCurrentValid)
                    lrDisplayPose = mCurrent;
            }

            void Latch(const rw::math::vpu::Matrix44Affine& lrTickPose)
            {
                mPrevious       = mCurrent;
                mbPreviousValid = mbCurrentValid;
                mCurrent        = lrTickPose;
                mbCurrentValid  = true;
            }

            void Apply(rw::math::vpu::Matrix44Affine& lrDisplayPose, f32 lfAlpha) const
            {
                if (!mbCurrentValid)
                    return;
                lrDisplayPose = (mbPreviousValid && lfAlpha < 1.0f)
                                    ? BlendTransform(mPrevious, mCurrent, lfAlpha)
                                    : mCurrent;
            }

        private:
            rw::math::vpu::Matrix44Affine mCurrent;
            rw::math::vpu::Matrix44Affine mPrevious;
            bool                          mbCurrentValid;
            bool                          mbPreviousValid;
        };
    }
}
