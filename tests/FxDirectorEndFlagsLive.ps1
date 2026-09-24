# FX-DIRECTOR (crash parity 2026-09-24) -- the player's end-of-event pair reaches the director, live.
#
# BrnGameModule::BridgeGameStateToDirector @0x823CD170 publishes two bytes out of the game-state
# module's ScoringOutputInterface (live arm 0x823CD4A0..0x823CD510):
#     @0x7AD5 = mabPlayerEliminated[mePlayerRaceCarIndex]
#     @0x7AD6 = mfModeTimeRemaining > 0.0f ? 0 : mbTimerActive     (flt_82001CC0 == 0.0f)
# and MainDirector::ProcessInputQueue's prologue copies them into GameState +0x1D0 / +0x1D1
# (0x82237430..0x82237440). Their one reader, ArbStateRoaming::ProcessPossibleFX, plays
# "Damage_Crit" / "Wrecked" on them in the ONLINE modes 12 / 14 / 17 only (jump table 0x82234A68),
# so no offline drive can show the post-effect -- on the console either. What an offline drive CAN
# show is the value path: ModeManager::UpdateCurrentMode gives the Stunt Run (mode 7) a mode time
# limit, and when that clock runs out the bridge's @0x7AD6 rises, the director copies it, and the
# edge-triggered witness prints it with the event type it arrived in.
#
# The shot is tools/tests/cases/stunt_run_lifecycle.ps1's: stunt junction 480897 at
# (2641.5, 1.3, -1723.8) heading 169, event 558269 (STUNT RUN, 2:00), started through the harness's
# BRN_START_EVENT junction injection, with that case's alternating steer so the car keeps covering
# ground and the sim keeps pace (a car spinning on the spot let the 120 s mode clock outlast a 265 s
# budget there). MaxSeconds is generous for the same reason: the mode clock is SIM time.
# Witness (BRN_DIRECTOR_ACTION_DIAG, NOT X360):
#   [director-state] end-of-event pair -> +0x1D1 mbModeTimeExpired <0|1> +0x1D0 mbPlayerEliminated <0|1> (event type N, team T)
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxDirectorEndFlagsLive.ps1
@{
  Name    = 'fxdirector_end_flags'
  Area    = 'director'
  Bug     = 'The director must receive the player''s end-of-event pair: the Stunt Run clock running out raises GameState +0x1D1 (bridge @0x7AD6, copy 0x82237430).'
  Frames  = $false
  Run     = @{
    Drive           = $true
    MotionProbe     = $true
    Teleport        = '2641.5,1.3,-1723.8,169'
    StartEvent      = $true
    EventFsm        = $true
    SkipTrainingTip = $true
    MaxSeconds      = 360
    SkipIntro       = $true
    AcceptGap       = 1.0
    SteerScript     = '0:none,22:left,38:right,54:left,70:right,86:left,102:right,118:left,134:right,150:left,166:right,182:left,198:right,214:left,230:right'
  }
  DiagEnv = 'BRN_DIRECTOR_ACTION_DIAG=1,BRN_STUNT_DIAG=1'
  Checks  = @(
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount';   Name = 'the Stunt Run was started at the junction (BRN_START_EVENT injection)'
       Pattern = '\[start\] \*\*\*\*\* HARNESS-ONLY START INJECTION'; Min = 1 }
    @{ Kind = 'Mark';       Name = 'mode reached E_GMS_IN_PROGRESS'; Cue = 'e-inprog' }
    @{ Kind = 'LogCount';   Name = 'the Stunt Run clock ran out and reached GameState +0x1D1 (bridge @0x7AD6 -> copy 0x82237430), in event type 7'
       Pattern = '\[director-state\] end-of-event pair -> \+0x1D1 mbModeTimeExpired 1 \+0x1D0 mbPlayerEliminated 0 \(event type 7,'; Min = 1 }
    @{ Kind = 'LogCount';   Name = 'nobody is eliminated offline (@0x7AD5 stays 0)'; Pattern = 'mbPlayerEliminated 1'; Max = 0 }
    @{ Kind = 'Script';     Name = 'the flag rises while the mode is IN_PROGRESS (its clock ran out) and falls again once the mode leaves it'; Script = {
        param($ctx)
        $inProgress = -1; $rise = -1; $outro = -1; $fall = -1; $i = 0
        foreach ($line in $ctx.LogLines) {
            if ($inProgress -lt 0 -and $line -match '\[stunt\] mode state -> E_GMS_IN_PROGRESS') { $inProgress = $i }
            elseif ($inProgress -ge 0 -and $rise -lt 0 -and $line -match '\[director-state\] end-of-event pair -> \+0x1D1 mbModeTimeExpired 1') { $rise = $i }
            elseif ($rise -ge 0 -and $outro -lt 0 -and $line -match '\[stunt\] mode state -> E_GMS_OUTRO') { $outro = $i }
            elseif ($rise -ge 0 -and $fall -lt 0 -and $line -match '\[director-state\] end-of-event pair -> \+0x1D1 mbModeTimeExpired 0') { $fall = $i }
            $i++
        }
        $lines = @($ctx.LogLines | Where-Object { $_ -match '\[director-state\] end-of-event pair' })
        @{ Pass = ($inProgress -ge 0 -and $rise -gt $inProgress -and $outro -gt $rise -and $fall -gt $rise)
           Detail = ("log lines: IN_PROGRESS {0}, rise {1}, OUTRO {2}, fall {3}; {4} witness line(s): {5}" -f $inProgress, $rise, $outro, $fall, $lines.Count, (($lines | Select-Object -First 4) -join ' | ')) }
    } }
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'zero asserts'; Pattern = '\[ASSERT \d+\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
  )
}
