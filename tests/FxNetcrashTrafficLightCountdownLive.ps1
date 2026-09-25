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
# The lights' renderer (RenderLightsForHull @0x8275DBF0, the reader of +0x12C0 / +0x12C4) is not bodied yet, so
# the countdown has no visible effect on this build; the witness reads the manager's state.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxNetcrashTrafficLightCountdownLive.ps1
@{
  Name    = 'fxnetcrash_light_countdown'
  Area    = 'traffic'
  Bug     = 'The traffic lights must receive the event countdown on every change, offline too (arm 47 -> TrafficLightManager::SetCountdownValue 0x8274BD98; Update 0x8274EB04).'
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
  )
}
