# FX-TRAFFIC3 item 1b (crash parity wave 5, 2026-09-24) -- live witness for CC-2's back-off:
# TrafficEntityModule::CheckIfPhysicalVehicleIsStuck @0x8272C010 (called by DriveTowardsTarget at
# 0x8273E150) and UpdateStuckReverseManoeuvre @0x82719430 (GenerateDriverInputs' STUCK_REVERSE arm,
# 0x827493C8). A physical traffic car whose front or back stuck timer passes 3.2 s (flt_820BA868)
# stops driving: at both ends it gives up; at the nose it rolls a percent against 0.4
# (flt_8200473C) each test and, on a hit, reverses (brake 0.75) until 30 degrees off its target, a
# back contact or 3 s, then stops on the handbrake. BRN_TRAFFIC_DIAG witnesses (capped):
#   [T-stuck-check] vehicle=V front=F back=B -> below 3.2, drives on        (a live timer, not stuck yet)
#   [T-stuck-check] vehicle=V front=F back=B [roll=R] -> NONE | STUCK_REVERSE | GIVE_UP phase=1
#   [T-stuck-reverse] vehicle=V phase=0 reversing ... / phase=0->1 ... / phase=1 end ...
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic3StuckReverseLive.ps1 -Label FX-TRAFFIC3
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_stuck_reverse'
$case.Area = 'traffic'
$case.Bug = 'A traffic car stuck past 3.2 s must reach CheckIfPhysicalVehicleIsStuck, take the console response, and on STUCK_REVERSE back off through UpdateStuckReverseManoeuvre (CC-2 back-off, item 1b).'
# Free roam, wrong way: RivalOrganic's own teleport point turned round (heading 0 instead of 180),
# no event, no boost, the throttle held -- the car drives against the lane's flow and keeps its
# nose pressed into the first oncoming car, which is the pin CheckIfPhysicalVehicleIsStuck tests.
$case.Run.Teleport = '3040.7,-5.8,-1937.9,0'
$case.Run.StartEvent = $false
$case.Run.EventFsm = $false
$case.Run.Boost = ''
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'item 1b dispatched: DriveTowardsTarget reached CheckIfPhysicalVehicleIsStuck'
       Pattern = '\[T-stuck-check\] dispatched vehicle='; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'the test saw a live CC-2 stuck timer'
       Pattern = '\[T-stuck-check\] vehicle=\d+ front='; Min = 1 }
    @{ Kind = 'Script'; Name = 'every stuck test follows the console rule (<= 3.2 drives on; both -> GIVE_UP; nose -> roll vs 0.4; tail -> NONE, no roll)'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $n = 0; $below = 0; $rolls = 0; $bad = @(); $counts = @{ NONE = 0; STUCK_REVERSE = 0; GIVE_UP = 0 }
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-stuck-check\] vehicle=\d+ front=([^ ]+) back=([^ ]+) -> below 3\.2') {
                $below++
                $f = [double]::Parse($Matches[1], $inv); $b = [double]::Parse($Matches[2], $inv)
                if ($f -gt 3.2000000476837 -or $b -gt 3.2000000476837) { $bad += $line }
                continue
            }
            if ($line -match '\[T-stuck-check\] vehicle=\d+ front=([^ ]+) back=([^ ]+)(?: roll=([^ ]+))? -> (NONE|STUCK_REVERSE|GIVE_UP)') {
                $n++
                if ($line -match '\(rolls (\d+)\)' -and [int]$Matches[1] -gt $rolls) { $rolls = [int]$Matches[1] }
                $null = $line -match '\[T-stuck-check\] vehicle=\d+ front=([^ ]+) back=([^ ]+)(?: roll=([^ ]+))? -> (NONE|STUCK_REVERSE|GIVE_UP)'
                $f = [double]::Parse($Matches[1], $inv); $b = [double]::Parse($Matches[2], $inv)
                $resp = $Matches[4]; $counts[$resp] += 1
                $fs = $f -gt 3.2000000476837; $bs = $b -gt 3.2000000476837
                if ($fs -and $bs) { if ($resp -ne 'GIVE_UP') { $bad += $line } }
                elseif ($fs) {
                    if (-not $Matches[3]) { $bad += $line; continue }
                    $r = [double]::Parse($Matches[3], $inv)
                    $want = if ($r -gt 0.4000000059604645) { 'NONE' } else { 'STUCK_REVERSE' }
                    if ($r -lt 0 -or $r -ge 100 -or $resp -ne $want) { $bad += $line }
                }
                elseif ($bs) { if ($resp -ne 'NONE' -or ($Matches[3] -and [double]::Parse($Matches[3], $inv) -ne -1)) { $bad += $line } }
                else { $bad += $line }
            }
        }
        @{ Pass = ($n + $below -gt 0 -and $bad.Count -eq 0); Detail = "$below live timers below 3.2 (drive on); $n stuck responses printed (NONE $($counts.NONE), STUCK_REVERSE $($counts.STUCK_REVERSE), GIVE_UP $($counts.GIVE_UP)), $rolls reverse rolls in all; off-rule: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
    @{ Kind = 'Script'; Name = 'a STUCK_REVERSE car backs off: reverses on the brake, then hands back (or the 0.4 % roll never hit)'; Script = {
        param($ctx)
        $started = @{}; $reversing = @{}; $phase1 = @{}; $ended = @{}; $speeds = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-stuck-check\] vehicle=(\d+) .*-> STUCK_REVERSE') { $started[$Matches[1]] = 1 }
            elseif ($line -match '\[T-stuck-reverse\] vehicle=(\d+) phase=0 reversing brake=0\.75') { $reversing[$Matches[1]] = 1 }
            elseif ($line -match '\[T-stuck-reverse\] vehicle=(\d+) phase=0->1 .* speed=([^ ]+)') { $phase1[$Matches[1]] = 1; $speeds += $Matches[2] }
            elseif ($line -match '\[T-stuck-reverse\] vehicle=(\d+) phase=1 end') { $ended[$Matches[1]] = 1 }
        }
        $pass = ($started.Count -eq 0) -or ($reversing.Count -gt 0)
        @{ Pass = $pass; Detail = "STUCK_REVERSE starts: $($started.Count); reversing: $($reversing.Count); phase 0->1: $($phase1.Count) (speeds $($speeds -join ', ')); ended: $($ended.Count)" }
    } }
)
$case
