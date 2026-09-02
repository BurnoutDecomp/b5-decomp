#ifndef BRN_TRAFFIC_SWERVE_WATCH_H
#define BRN_TRAFFIC_SWERVE_WATCH_H

#include "types.hpp"

// ============================================================================================
// [DIAG] NOT IN THE X360 BINARY. OFF UNLESS BRN_TRAFFIC_DIAG IS SET. DELETE-WHEN-STABLE.
//
// WHY THIS EXISTS. The decision-to-moved-car chain
//   UpdateParams_TryAvoidCrashing (miBehaviour = 2)
//     -> UpdateVehiclesJob::CalcSwerveAmount   (miBehaviour == 2 => a FULL 1.0 swerve)
//        -> CalcTargetPos                      (one whole lane width of lateral offset)
//           -> MoveToTarget                    (the car is moved)
// is complete and bodied, and the [T5-avoid] rung proves the DECISION fires. What no run had
// ever produced is a FRAME SHOWING IT, because every camera this build has follows the player:
// the chase camera is bolted to a car that is, by construction, either crashed or driving away
// from the traffic that is reacting to it.
//
// This is the two-line hook that fixes that. UpdateVehiclesJob publishes WHERE the swerving
// car is (below); BrnWorldModule's existing bring-up establishing camera reads it under
// BRN_WORLD_CAMTRAFFIC=1 and LATCHES a static wide shot on the first swerve, so the car then
// drives THROUGH a fixed frame and its path can be seen to bend. Nothing here is in the
// simulation's control flow -- it is a write of three floats and a counter, gated on the same
// BRN_TRAFFIC_DIAG the [T5-*] rungs already use, and the camera side changes nothing unless
// its own variable is set.
// ============================================================================================

namespace BrnTraffic
{
    struct SwerveWatch
    {
        f32  mfPosX;
        f32  mfPosY;
        f32  mfPosZ;
        s32  miVehicle;      // the traffic vehicle index that is swerving, -1 == none
        s32  miBehaviour;    // the param's Param::miBehaviour; 2 == DRIVE_AROUND_OBSTRUCTION
        u32  muPublishes;    // monotonic; a camera can tell "fresh" from "never"
        u32  muBehaviour2;   // publishes whose miBehaviour was 2 (the crash-avoidance swerve)
        // Set to 1 by BrnWorldModule when BRN_WORLD_CAMTRAFFIC latches its static shot. Read by
        // the back-buffer writer (pc/gcm/renderengine/device.cpp) under BRN_FRAME_DUMP_ARM, so a
        // capture can dump EVERY present for a few seconds around the swerve instead of dumping
        // the whole run.
        // ⭐ THIS IS NOT A CONVENIENCE. Dumping every 2nd present for a 130 s run writes ~16 GB,
        // and the simulation is FRAME-COUPLED: the measured cost of that I/O storm was a run with
        // ZERO swerves and zero junction-FUP action where the same build, unfilmed, produced
        // seven swerves and nine removals. The camera has to be able to film a short window, or
        // filming destroys what it is filming.
        u32  muCameraLatched;

        // ---- THE CHAIN-CRASH ARM (2026-08-30, traffic-crash wave 2) --------------------------
        // Same argument as muCameraLatched above, one behaviour over. The SYMPATHETIC crash --
        // a traffic car that sees a wreck, aims itself at it and crashes ITSELF -- is the
        // pile-up the game is named for, and no frame had ever shown one. It cannot be filmed
        // by driving longer and hoping: the previous wave entered the arm in three of four
        // drives and committed a crash in ONE, so an unarmed dump either misses it or writes
        // gigabytes of the 99% of the run that is not it (and the sim is frame-coupled, so the
        // gigabytes can destroy the event -- see above).
        //
        // ⭐ THE ARM IS THE DETECTOR, and that is the point. A pixel-difference scan over a
        // dump cannot reliably find a crash -- measured 2026-08-29, a 41 m/s player-into-traffic
        // impact the LOG recorded did not show up in a frame-difference scan of the same run's
        // 1,238 frames. Here the game's own state machine says when to film, and every dumped
        // frame carries the crasher's live position in frames.csv, so "which frames contain the
        // chain crash" is answered by construction rather than by a heuristic that has already
        // been seen to miss one.
        //
        // UpdateSympatheticCrashing publishes the live position every frame it runs and bumps
        // muSympCommits ONCE, when the car enters a commit-bound state (HANDBRAKE / LOCKUP) --
        // 0.1 s and 1.1 s before the impact respectively, which is the last instant a camera
        // can still be staged for it. BRN_WORLD_CAMTRAFFIC=3 latches the static wide shot on
        // THAT counter instead of on an ordinary swerve, and the back-buffer writer's existing
        // BRN_FRAME_DUMP_ARM follows it through muCameraLatched with no new variable.
        f32  mfSympPosX;
        f32  mfSympPosY;
        f32  mfSympPosZ;
        s32  miSympVehicle;    // the sympathetically-crashing traffic index, -1 == none yet
        s32  miSympState;      // Vehicle::SympatheticCrashState (1 HEADON 2 ACCEL 3 HB 4 LOCKUP)
        u32  muSympPublishes;  // monotonic; a CSV row whose count equals the previous row's is
                               // stale, so a stamped position can never be mistaken for fresh
        u32  muSympCommits;    // monotonic; bumped on entry to HANDBRAKE / LOCKUP
    };

    // Defined in BrnUpdateVehiclesJob.cpp (the publisher).
    extern SwerveWatch gSwerveWatch;
}

#endif
