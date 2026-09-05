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

        // ⭐ THE TYRE-MARK LATCH, on exactly the argument the slomo latch above is built on.
        // A tyre mark is laid by BrnEffects::EffectsModule::HandleWheels calling
        // TrailSystem::AddTrailSegment, and that is a handful of frames in a run that is
        // minutes long -- so filming the whole run to catch it writes tens of gigabytes AND,
        // per the note above, can suppress the very event being filmed. muSkidLatched is
        // STICKY and raised by the first segment ever laid, so BRN_FRAME_DUMP_ARM=skid starts
        // the capture at the instant the first mark exists.
        u32 muSkidLatched;

        // ⭐ THE RUNNING SEGMENT COUNT AND THE LAST SEGMENT'S CONTACT POINT, stamped into
        // frames.csv so each captured frame is SELF-LABELLING -- exactly the way the
        // sympathetic-crash columns are. Two things a bitmap cannot say on its own: WHICH
        // frame the mark started on (the row where the count first rises) and WHERE on the
        // road it was laid (so a marker can be projected into the frame from the game's own
        // coordinates rather than pointed at by eye).
        u32 muTrailSegments;
        f32 mfLastSegX;
        f32 mfLastSegY;
        f32 mfLastSegZ;

        // ⭐ WHERE ON THE SCREEN THAT SEGMENT LANDED. The world position above cannot be
        // pointed at in a bitmap without the frame's camera, and the log has no camera. The
        // trail renderer already runs the skid vertex program's own transform
        // (oPos = pos.x*c0 + pos.y*c1 + pos.z*c2 + c3) on the first vertex of every batch it
        // submits, so it stamps the NDC here and frames.csv carries it: a marker can then be
        // drawn at exactly the pixel a mark was submitted to, and "no mark visible" becomes a
        // claim about a named pixel rather than about a whole picture. w <= 0 means behind
        // the camera; |x| or |y| > 1 means off-screen -- both are legitimate reasons for a
        // frame to show nothing, and both are now visible instead of guessed at.
        f32 mfSegNdcX;
        f32 mfSegNdcY;
        f32 mfSegClipW;

        // ⭐ THE ONE-STEP VELOCITY LATCH, on the same argument as muSkidLatched. The kerb
        // investigation's whole subject is a car losing an enormous amount of speed in ONE
        // 16.67 ms physics step -- an event that has never been filmed, because it lasts a
        // single step in a run that is minutes long and nobody knew in advance which step.
        // ⛔ AND THE FILM IS NOT DECORATION HERE: four "kerb catches" across two earlier waves
        // turned out, on the frames, to be the player rear-ending traffic, and one convincing
        // 15.7 -> 4.0 mph event was a stationary car parked across the approach. The log could
        // not tell those apart; the picture could. muDvLatched is raised by
        // ExternalPhysicsBody's [dv] witness on the first step whose |dv| crosses
        // BRN_DV_PROBE's threshold, so BRN_FRAME_DUMP_ARM=dv starts the capture at that step
        // and the frames show WHAT THE CAR HIT. STICKY, like the other two.
        // ⚠️ It films from the event ONWARD -- the approach is not in the strip, because the
        // latch cannot know about a step before it happens. What a `dv` strip can settle is
        // what is in contact with the car at and after the drain, not what it looked like
        // coming in.
        u32 muDvLatched;

        // The step index, the frame counter and the |dv| that raised it, so a strip and a
        // "[dv] STEP" line can be tied together without trusting a timestamp -- the same
        // self-labelling rule the four fields above follow.
        u32 muDvLatchedStep;
        u32 muDvLatchedFrame;
        f32 mfDvLatchedMagnitude;

        // ⭐ THE SHOWTIME VICTIM-GAIN LATCH (BRN_FRAME_DUMP_ARM=x15), on exactly the argument
        // muDvLatched is built on. The x15 arm in DeformableObject::ApplySensorImpulse
        // (@0x82607D78..0x82607DD0) fires only when a car is struck BY a car that is in showtime
        // -- three conditions at once, none of them controllable from the harness: the player in
        // showtime, a traffic car in reach, and a contact between them. Measured across six runs
        // it happened in ONE, for 48 sensor rows spanning 15 presents. A `dv` strip cannot be
        // relied on to cover that: in the run where it did fire, the dv latch rose 520 presents
        // EARLIER, so a 48-frame strip from dv ended 500 presents before the event.
        // ⛔ AND FILMING THE WHOLE RUN IS NOT THE ALTERNATIVE -- see this header's banner: this
        // simulation is frame-coupled and a run that films itself that hard stops producing the
        // event. STICKY, like the other three, so the strip carries the gain AND its aftermath.
        u32 muVictimGainLatched;

        // Self-labelling, same rule as the fields above: WHICH car took the gain, at which
        // present, and the shaped magnitude on the row that raised the latch -- so a bitmap and
        // an [st-mag] line can be tied together without trusting a timestamp.
        u32 muVictimGainPresent;
        s32 miVictimGainGid;
        f32 mfVictimGainShaped;

        // ⭐ THE PLAYER CAR'S WORLD POSITION AND THE LIVE BOOST-PLUME MASK, on exactly the
        // argument every field above is built on -- and added for the one thing this strip could
        // NOT do before: SCENE-MATCH TWO RUNS. A before/after film of a particle effect is worth
        // nothing unless the two frames being differenced are the same shot, and this sim is
        // frame-coupled, so two runs of one recipe present a different number of frames over the
        // same drive (measured 6,630 vs 7,560 over 51 s). Matching by frame index therefore
        // compares different places on the road, and "the plume got brighter" cannot be told from
        // "the car is somewhere else". With the car's own position stamped into every row, a pair
        // can be chosen by WHERE THE CAR WAS, and the background difference outside the effect
        // then MEASURES the quality of that match instead of being assumed.
        // Written in ONE place -- BoostStateMachine::OnTick, the only function that positions a
        // boost effect (the [boostloc] witness beside it reads the same RaceCarState transform) --
        // and read in ONE place, the back-buffer writer in pc/gcm/renderengine/device.cpp.
        // muBoostEffectMask is that tick's active FXBOOSTPOINT mask, so a row also says whether a
        // plume was being positioned at all on the frame it labels. All zero on a run where no
        // boost machine ticked. DELETE-WHEN-STABLE with the rest of the [lionfx] family.
        f32 mfCarPosX;
        f32 mfCarPosY;
        f32 mfCarPosZ;
        u32 muBoostEffectMask;
    };

    // Defined in GameSource/Game/BrnGameModule.cpp (the only writer).
    extern FilmLatch gFilmLatch;
}

#endif
