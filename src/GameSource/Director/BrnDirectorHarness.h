#pragma once

// ============================================================================
// GameSource/Director/BrnDirectorHarness.h
//
// [HARNESS -- NOT X360] State the director publishes for the PC test harness ONLY. Nothing in the
// game reads these; they exist so a harness can wait on a director condition without reaching
// into the director's objects.
//
//   gbArbitratorInRoaming  true while the arbitrator's state container runs ArbStateRoaming.
//                          Written by ArbitratorStateContainer::SetCurrentState (the single writer
//                          of the container's current state). Read by the crash sweep's opt-in
//                          BRN_SWEEP_WAIT_ROAMING gate (World/BrnPlaceOnTrackManager.cpp), which
//                          holds the sweep's first shot until the junkyard car-select state has
//                          played out its OUTRO take and handed the camera back -- otherwise the
//                          shot's crash lands while ArbStateCarSelect still owns the camera and the
//                          crash camera (and its time scale) can only start once the outro ends
//                          (measured: fxd2trace_h225_s80_r1, 82 frames late).
// ============================================================================

namespace BrnDirector
{
namespace Harness
{
    extern bool gbArbitratorInRoaming;
}
}
