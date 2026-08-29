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

        // ⭐ THE LIVE scale, refreshed every sub-step. The frame writer stamps it into a sidecar
        // CSV beside the dump, so every captured frame CARRIES the timestep it was rendered
        // under. Without it a strip cannot be told apart from an ordinary one: the drive-thru
        // dilates the SIM timer only, so the camera and the HUD keep running at full rate and
        // "the picture looks slow" is not something the eye can adjudicate. A frame that names
        // its own timestep can be.
        f32 mfLiveSimScale;
        f32 mfLiveSimStep;

        // ⭐ THE LIVE SHOWTIME/BOOST METER FRACTION, on exactly the same argument as the two
        // above and for exactly the same reason. The boost bar IS the showtime meter (the
        // showtime arm of RaceCarEntityModule::UpdateBoost overwrites the strategy tank from
        // CrashPlayManager::mfBoostPercentage every frame, and the bridge publishes that tank
        // as GuiEventBoostInfo id 206), so "does the bar move with the value" is the whole
        // question -- and a bitmap alone cannot answer it. A LOG line cannot either: the log
        // ticks on SIM frames and the dump ticks on PRESENTS, so pairing them means guessing a
        // frame rate. Stamping the published fraction into the same CSV row as the bitmap makes
        // every frame SELF-LABELLING, exactly as mfLiveSimStep made the slow-motion strip
        // self-labelling, and the fill measured off the pixels can then be regressed against it
        // with no time base at all.
        //
        // Written in ONE place -- BrnGameModule::BridgeWorldVehicleDataToGui, the sole producer
        // of event 206 -- as mfBoostAmount / mfMaxBoost, i.e. the exact fraction
        // BoostBarRenderer::RenderComponent draws. Read in ONE place, the back-buffer writer in
        // pc/gcm/renderengine/device.cpp. -1.0f means "no boost record has been published yet".
        f32 mfLiveBoostFraction;
    };

    // Defined in GameSource/Game/BrnGameModule.cpp (the only writer).
    extern FilmLatch gFilmLatch;
}

#endif
