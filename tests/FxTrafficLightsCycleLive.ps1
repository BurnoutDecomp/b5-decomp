# FX-TRAFFICLIGHTS (crash parity wave 5, 2026-09-25) -- live witness that the traffic lights CYCLE in free roam and
# that the traffic obeys them. TrafficEntityModule::UpdateJunctions @0x82723EA0 (UpdateDecisionFrame 0x8274E5EC, every
# decision frame) runs every junction's phase timer down; on a phase change it sets the junction's stop lines red or not
# (HullRuntime::SetStoplineRed) and changes its lights (TrafficLightManager::ChangeLightState: AMBER on the way to red,
# GREEN at once); TrafficLightManager::UpdateHull @0x827517F8 then runs the hull's AMBER lights down to RED
# (TrafficLightRuntimeState::Update @0x827515D8). The traffic reads the stop lines in DoesParamNeedToStopForStopline
# @0x827249F8 -> UpdateParams_PrecalcBehaviourParams case 3 (behaviour 4, STOP AT THE STOPLINE: target speed 0).
# Before this commit UpdateJunctions had no body: no phase ever changed and no stop line was red outside an event start,
# so free-roam traffic never stopped at a light.
#
# The player drives off the junkyard road for 10 s and then stands, so the run watches the lights and the traffic
# around it for about two minutes. BRN_TRAFFIC_LIGHT_DIAG arms the capped witnesses (FLAG PC, reads only):
#   [traffic-lights] watching hull=H junction=J states=N lights=L state=S holds=T t=C
#   [traffic-lights] phase hull=H junction=J state=S/N holds=T redLights=r/L stoplines H:I=R|G|- ... instances red=a
#     amber=b green=c t=C sinceLast=D      (D is the phase the console just ran: it must match the previous line's T)
#   [traffic-lights] amber hull=H junction=J ran down to red: ... afterPhase=A t=C   (KF_AMBER_TIME 2.0 s of it)
#   [traffic-lights] param P waiting at a red stop line: stopline=H:I=R behaviour=4 ... speed=<0.5
#   [traffic-lights] param P released: stopline=H:I=G ...           (the stop line turned green)
#   [traffic-lights] param P moved off: ... speed=>3
# BRN_TRAFFIC_DIAG adds the [T3-behaviour] 5 s histogram: its [4] (STOP AT THE STOPLINE) was 0 in every free-roam run
# before this commit.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTrafficLightsCycleLive.ps1
@{
  Name    = 'fxtrafficlights_cycle'
  Area    = 'traffic'
  Bug     = 'Free-roam traffic lights never cycled and traffic never stopped at a red light: UpdateJunctions @0x82723EA0 (and UpdateHull @0x827517F8) had no body.'
  Frames  = $false
  Run     = @{
    Drive          = $true
    MotionProbe    = $true
    SkipIntro      = $true
    AcceptGap      = 1.0
    Teleport       = '3040.7,-5.8,-1937.9,180'   # baseline_boot_drive's road outside the junkyard exit
    DriveDelay     = 2.0
    ThrottleScript = '0:accel,10:brake,14:none'
    MaxSeconds     = 170
  }
  DiagEnv = 'BRN_TRAFFIC_LIGHT_DIAG=1,BRN_TRAFFIC_DIAG=1'
  Checks  = @(
    @{ Kind = 'NewAsserts'; Name = 'no NEW assert families' }
    @{ Kind = 'LogCount';   Name = 'no exceptions (0 AV)'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'LogCount';   Name = 'no assert from the module, the junction box, the hull runtimes or the light manager'
       Pattern = '\[ASSERT \d+\].*(BrnTrafficEntityModule|BrnJunctionLogicBox|BrnTrafficHullRuntime|BrnTrafficLightManager)'; Max = 0 }
    @{ Kind = 'Mark';       Name = 'reached DRIVING'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch';   Name = 'the witness adopted a junction with lights in the active hulls'
       Pattern = '\[traffic-lights\] watching hull=\d+ junction=\d+ states=\d+ lights=[1-9]'; Expect = $true }
    @{ Kind = 'LogCount';   Name = 'the watched junctions changed phase again and again (UpdateJunctions, every decision frame)'
       Pattern = '\[traffic-lights\] phase hull=\d+ junction=\d+ state=\d+/\d+'; Min = 6 }
    @{ Kind = 'LogMatch';   Name = 'a phase turned a stop line RED'
       Pattern = '\[traffic-lights\] phase hull=.* stoplines.* \d+:\d+=R'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'a phase turned a stop line GREEN'
       Pattern = '\[traffic-lights\] phase hull=.* stoplines.* \d+:\d+=G'; Expect = $true }
    @{ Kind = 'LogMatch';   Name = 'an AMBER light ran down to RED (UpdateHull -> TrafficLightRuntimeState::Update)'
       Pattern = '\[traffic-lights\] amber hull=\d+ junction=\d+ ran down to red'; Expect = $true }
    @{ Kind = 'Script';     Name = 'the console cadence: each phase lasted the time the previous line said it held (+- 0.25 s)'; Script = {
        param($ctx)
        $last = @{}; $pairs = 0; $bad = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -notmatch '\[traffic-lights\] phase hull=(?<h>\d+) junction=(?<j>\d+) state=\S+ holds=(?<holds>\S+) .* t=\S+ sinceLast=(?<since>\S+)') { continue }
          $key = "$($Matches.h):$($Matches.j)"
          $holds = [double]($Matches.holds -replace ',', '.'); $since = [double]($Matches.since -replace ',', '.')
          if ($last.ContainsKey($key)) {
            $pairs++
            if ([math]::Abs($since - $last[$key]) -gt 0.25) { $bad += "$key held $($last[$key]) ran $since" }
          }
          $last[$key] = $holds
        }
        return @{ Pass = ($pairs -ge 3 -and $bad.Count -eq 0); Detail = "consecutive pairs=$pairs mismatches=$($bad.Count) $($bad -join '; ')" }
      } }
    @{ Kind = 'LogMatch';   Name = 'a param stopped at a RED stop line (behaviour 4, STOP AT THE STOPLINE)'
       Pattern = '\[traffic-lights\] param \d+ waiting at a red stop line: stopline=\d+:\d+=R behaviour=4'; Expect = $true }
    @{ Kind = 'Script';     Name = 'a param waited at a red stop line, was released when it turned GREEN, and moved off'; Script = {
        param($ctx)
        $state = @{}; $done = @()
        foreach ($l in $ctx.LogLines) {
          if ($l -match '\[traffic-lights\] param (?<p>\d+) waiting at a red stop line: stopline=(?<s>\d+:\d+)=R') { $state[$Matches.p] = "W$($Matches.s)"; continue }
          if ($l -match '\[traffic-lights\] param (?<p>\d+) released: stopline=(?<s>\d+:\d+)=G') {
            if ($state[$Matches.p] -eq "W$($Matches.s)") { $state[$Matches.p] = "R$($Matches.s)" } ; continue }
          if ($l -match '\[traffic-lights\] param (?<p>\d+) moved off: stopline=(?<s>\d+:\d+)=. .* speed=(?<v>[\d.,]+)') {
            if ($state[$Matches.p] -eq "R$($Matches.s)") { $done += "param $($Matches.p) at $($Matches.s) (speed $($Matches.v))" } ; continue }
        }
        return @{ Pass = ($done.Count -ge 1); Detail = "$($done.Count) full wait/green/move-off sequence(s): $($done | Select-Object -First 3)" }
      } }
    @{ Kind = 'LogMatch';   Name = 'the behaviour histogram counts STOP AT THE STOPLINE ([4] > 0)'
       Pattern = '\[T3-behaviour\] histogram .*\[4\]=[1-9]'; Expect = $true }
  )
}
