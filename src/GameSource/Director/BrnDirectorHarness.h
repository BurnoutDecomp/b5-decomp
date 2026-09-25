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

#include <cstdlib>   // std::getenv (the opt-in below)

namespace BrnDirector
{
namespace Harness
{
    extern bool gbArbitratorInRoaming;

    // [FX-DIRECTOR2 OPT-IN -- NOT X360] the camera scene-query closure (2026-09-25). DEFAULT OFF.
    // BRN_FXD2_SCENEQUERY set (and not "0") runs the director's queries end to end, as the console
    // does unconditionally:
    //   MainDirector::UpdateCameraBehavioursPreScene   BehaviourManager::GenerateSceneQueries (0x82255834)
    //   BrnGameModule::DoUpdate_Director               the external leg (Append -> WorldModule::
    //                                                  ExternalSceneQueriesUpdate -> results Append)
    //   DirectorModule::Update                         ProcessSceneQueryResults (@0x82239278)
    //   MainDirector::UpdateCameraBehavioursPostScene  BehaviourManager::ProcessSceneQueryResults (0x8224FF30)
    // All four read THIS switch, so the path is on or off as one piece: a query issued without its
    // answer would leave its post box WAITING and assert on the next AddPostBox.
    // DELETE-WHEN: one live run with it ON is clean (0 asserts, 0 AV) and the closure is un-gated.
    inline bool SceneQueryClosureEnabled()
    {
        static const int siEnabled = []() -> int
        {
            const char* lpcValue = std::getenv("BRN_FXD2_SCENEQUERY");
            return (lpcValue != 0 && lpcValue[0] != '\0' && lpcValue[0] != '0') ? 1 : 0;
        }();
        return siEnabled != 0;
    }
}
}
