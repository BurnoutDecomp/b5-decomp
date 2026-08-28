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
    };

    // Defined in BrnUpdateVehiclesJob.cpp (the publisher).
    extern SwerveWatch gSwerveWatch;
}

#endif
