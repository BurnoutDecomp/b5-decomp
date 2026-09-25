# FX-NETCRASH (crash parity wave 5, 2026-09-25; b5 30d481f4) -- live witness that the traffic lights receive
# the event countdown, offline. An offline race started at the lights (Teleport onto the start line, StartEvent
# + EventFsm) runs its 3-2-1-GO countdown: ModeManager::CheckCountdownDisplay posts E_ACTION_SET_COUNTDOWN on
# each change of the display, TrafficEntityModule::HandleExternalRequests arm 47 hands it to
# TrafficLightManager::SetCountdownValue on EVERY change, offline included (0x8274BD98..0x8274BDA0, before the
# online test at 0x8274BDA4), and PostPhysicsUpdate's TrafficLightManager::Update (0x8274EAF0..0x8274EB04, with
# mfSimTimeStep) runs the GO's GREEN down: flt_82004270 = 3.0f of sim time, then the countdown ends (state 3).
# Before 30d481f4 all three calls were named gates and the manager had no countdown members.
# BRN_TRAFFIC_LIGHT_DIAG arms the manager's capped [traffic-lights] witness (reads only):
#   [traffic-lights] SetCountdownValue display=3 -> countdown=1 state=0 remaining=0.000000   (RED at 3 and 2)
#   [traffic-lights] SetCountdownValue display=1 -> countdown=1 state=1 ...                  (AMBER at 1)
#   [traffic-lights] SetCountdownValue display=0 -> countdown=1 state=2 remaining=3.000000   (GREEN at the GO)
#   [traffic-lights] countdown over -> countdown=0 state=3 ...                               (Update, 3 s on)
# Reference run: scratch/bugtest/runs/fxnetcrash_light_countdown/20260925_151017 (GREEN 8/8, 0 asserts, 0 AV).
#
# THE EVENT START'S LIGHTS (UpdateEventStarts @0x82743B80 + arm 34, b5 FX-NETCRASH 2026-09-25). A race is a mode
# that clears the traffic: HandlePrepareForModeAction stores mbNeedToSetUpLightsForEventStart =
# mbGameModeClearsTraffic and the mode's mTrafficLightTriggerId (0x82748550). PostPhysicsUpdate's
# UpdateEventStarts (0x8274EE90, every frame) consumes the flag once the trigger's hull is active: every stop
# line of the start junction's lights goes red (HullRuntime::SetStoplineRed) and every light instance is
# changed to red (TrafficLightManager::ChangeLightState @0x827518E0: GREEN -> AMBER for KF_AMBER_TIME = 2.0f,
# a RED light stays RED). Arm 34 (E_ACTION_START_PLAYING_MODE, 0x8274BDF0) then calls SetCountdownValue(0) and
# trips the .cpp 5995 assert if the flag is still up. Before this commit the flag was never consumed, the stop
# lines and lights never changed, and arm 34 was unwired. The module's capped witness (same switch, reads only):
#   [traffic-lights] event start pending: trigger=<id> hull=<h> hullActive=<0|1> clearsTraffic=1
#   [traffic-lights] UpdateEventStarts set up the event start: trigger=<id> hull=<h> gridClears=0 junctions=<n>
#     lights=<n> stoplinesRed=<red>/<all> lightInstances=<n> amber=<n> red=<n> green=0
# The lights' renderer (RenderLightsForHull @0x8275DBF0, RenderAllLightsToBeInStateForHull @0x8275DE50) is not
# bodied yet, so neither the countdown nor the event start's lights are visible on this build; the witnesses
# read the manager's and the hull runtimes' state.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxNetcrashTrafficLightCountdownLive.ps1
@{
  Name    = 'fxnetcrash_light_countdown'
  Area    = 'traffic'
  Bug     = 'The traffic lights must receive the event countdown on every change, offline too (arm 47 -> TrafficLightManager::SetCountdownValue 0x8274BD98; Update 0x8274EB04), and a race start must set up the start line''s lights (UpdateEventStarts 0x82743B80: stop lines red, lights to red via ChangeLightState 0x827518E0) before arm 34''s .cpp 5995 tripwire.'
  Frames  = $false
  Run     = @{
    Drive           = $true
    SkipIntro       = $true
    AcceptGap       = 1.0
    Teleport        = '3003.9,6.6,-1675.6,0'
    StartEvent      = $true
    EventFsm        = $true
    SkipTrainingTip = $true
    MaxSeconds      = 100
  }
  DiagEnv = 'BRN_TRAFFIC_LIGHT_DIAG=1,BRN_MODEMGR_DIAG=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no assert at all'; Pattern = '^\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions (0 AV)'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no assert from the traffic light manager'; Pattern = '\[ASSERT \d+\].*BrnTrafficLightManager'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch';   Name = 'arm 47 -> SetCountdownValue: the countdown turned the lights RED (>= 2)'
       Pattern = '\[traffic-lights\] SetCountdownValue display=[2-9] -> countdown=1 state=0'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'then AMBER at 1'
       Pattern = '\[traffic-lights\] SetCountdownValue display=1 -> countdown=1 state=1'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'then GREEN for flt_82004270 = 3 s at the GO'
       Pattern = '\[traffic-lights\] SetCountdownValue display=0 -> countdown=1 state=2 remaining=3'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'and PostPhysicsUpdate''s Update ended the countdown'
       Pattern = '\[traffic-lights\] countdown over -> countdown=0 state=3'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'the race clears the traffic: HandlePrepareForModeAction raised the event start''s lights flag'
       Pattern = '\[traffic-lights\] event start pending: trigger=\d+ hull=\d+ hullActive=[01] clearsTraffic=1'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'UpdateEventStarts set up the start line: stop lines red, every light instance on its way to red (none GREEN)'
       Pattern = '\[traffic-lights\] UpdateEventStarts set up the event start: trigger=\d+ hull=\d+ gridClears=0 junctions=[1-9]\d* lights=[1-9]\d* stoplinesRed=[1-9]\d*/\d+ lightInstances=[1-9]\d* amber=\d+ red=\d+ green=0'; Expect = $true }
    @{ Kind = 'LogCount';   Name = 'arm 34 adds its SetCountdownValue(0) to the GO''s (two display=0 calls)'
       Pattern = '\[traffic-lights\] SetCountdownValue display=0 '; Min = 2 }
    @{ Kind = 'LogCount';   Name = 'arm 34''s .cpp 5995 tripwire never fires: the lights were set up before the mode started playing'
       Pattern = '!mbNeedToSetUpLightsForEventStart \|\| mbDEBUGTurnTrafficOff'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no assert from the module, the junction lights or the hull runtimes'
       Pattern = '\[ASSERT \d+\].*(BrnTrafficEntityModule|BrnJunctionLogicBox|BrnTrafficHullRuntime)'; Max = 0 }
  )
}
