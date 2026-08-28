#ifndef BRN_DIAG_FILM_LATCH_H
#define BRN_DIAG_FILM_LATCH_H

#include "types.hpp"

// ============================================================================================
// [DIAG] NOT IN THE X360 BINARY. Inert unless BRN_FRAME_DUMP_ARM names it. DELETE-WHEN-STABLE.
//
// WHY THIS EXISTS -- the same measured constraint BrnTrafficSwerveWatch.h documents, generalised
// off the traffic subsystem. Dumping every present for a whole run writes gigabytes AND this
// simulation is FRAME-COUPLED: a run that films itself that hard can stop producing the very
// event the film exists to show. So a capture has to be able to arm on the moment it cares
// about. The traffic wave's arm keys on BrnTraffic::gSwerveWatch.muCameraLatched, which is
// specific to a swerve camera; this is the equivalent latch for TIME DILATION, so a run can film
// a slow-motion window instead of the whole session.
//
// It is written in exactly one place -- BrnGameModule::UpdateTimers, the one function that
// applies a timestep multiplier -- and read in exactly one place, the back-buffer writer in
// pc/gcm/renderengine/device.cpp. Both sides are gated on their own environment variables, so an
// ordinary run neither sets it nor reads it.
// ============================================================================================

namespace BrnDiag
{
    struct FilmLatch
    {
        // Non-zero once the simulation timestep has been observed away from real time. STICKY:
        // it stays raised after the dilation ends, so a strip captures the dilated stretch AND
        // the recovery, and the per-frame world displacement can be compared across the edge.
        u32 muSlomoLatched;

        // The scale that raised it, and the frame-step it implies -- so the film and the log can
        // be tied to the same instant without trusting a timestamp.
        f32 mfLatchedSimScale;
        f32 mfLatchedSimStep;
    };

    // Defined in GameSource/Game/BrnGameModule.cpp (the only writer).
    extern FilmLatch gFilmLatch;
}

#endif
