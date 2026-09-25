# FX-TRAFFIC4 item 2 (crash parity wave 5, 2026-09-24) -- live witness for the three-point turn:
# DriveTowardsTarget @0x8273DFC0 starts E_MANOEUVRE_3_POINT_TURN when a driving car's target is more than
# 15 m behind it (0x8273E6B0) and GenerateDriverInputs' manoeuvre-2 arm then calls
# Update3PointTurnManoeuvre @0x827190B0 every frame: phase 0 reverses on brake 0.8 steering on the side
# dot until the target is 74 degrees or more to one side (|dot| > 0.96), phase 1 drives on gas 0.8 with
# opposite lock, and the turn ends once the car faces within 45 degrees of the target (dot > 0.707).
# BRN_TRAFFIC_DIAG witnesses (capped, printed INSIDE Update3PointTurnManoeuvre): [T-3pt-turn] vehicle=V
# phase=0 reversing ... / phase=0->1 ... / done dot=D ... -> NONE.
#
# FX-SCENARIOS (2026-09-25): THE DRIVE IS NOW FxScenariosTrafficCheckProbe.ps1's. The organic Road Rage pursuit
# (RivalOrganic.ps1, 160 s) started no turn in any of this case's runs (fxtraffic4_turn/20260924_184644 .. 212041)
# and a turn in 8 of the 96 banked RivalOrganic runs (fxtraffic4/20260924_184432 at t = 66 s: a car a RIVAL had
# checked), so the case passed on "NO TURN STARTED".
# A turn needs a physical car facing away from its target, and the check arm is what turns one round
# (SetTrafficVehicleChecked @0x8262D748: drive 1.0, steering side * 0.5 for 2.5 s). The check drive re-places
# the player 46 / 38 m/s behind the traffic of a divided road every 7 s, coasting, so it CHECKS cars and leaves
# them; its first run (fxscenarios_traffic_check_probe/20260925_121711) checked 5 cars and started 2 turns, one
# of them through phase 0 -> 1 -> done (vehicle 278). Here a last shot parks the player in the median 80 m from
# the junction (speed 0), out of every lane, so the tail of the run is left to the turning cars.
#
# THE CHECKS. The console turn has no time-out (no phase stores or reads mfManoeuvreTime; it ends on the dot
# alone), so a turning car that is boxed in stays in phase 0, and a turn that starts late is cut by the end of
# the run -- both are console behaviour, not a stall of the arm (the check probe's vehicle 286 started 12.4 s
# before its last track line, 13 m from the player's car, and had moved 5 m by then). So the verdict is: a turn
# STARTED (its phase-0 line is printed by Update3PointTurnManoeuvre itself, which the old gate never ran), and a
# started turn was DRIVEN -- it progressed (0 -> 1 or done, at the console thresholds) or its car moved at least
# 1 m after the start ([traffic-track], BRN_TRAFFIC_TRACK): the gated arm left the car standing with zero pedals.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic4TurnLive.ps1 -Label FX-SCENARIOS
$case = & (Join-Path $PSScriptRoot 'FxScenariosTrafficCheckProbe.ps1')
$case.Name = 'fxtraffic4_turn'
$case.Area = 'traffic'
$case.Bug = 'Update3PointTurnManoeuvre had no body and GenerateDriverInputs gated its arm: a turning car sat still until the no-driving latch gave it up (FX-TRAFFIC4 item 2).'
# 40 placements every 300 sim frames (5 s) instead of the probe's 20 every 420: the first case run on the probe's
# exact drive (fxtraffic4_turn/20260925_124312) checked 2 cars and started no turn, the probe run 5 and 2 -- a
# turn is a per-check chance, so the case takes twice the checks. Then the park shot and a 30 s tail.
# First GREEN on it: fxtraffic4_turn/20260925_125415 (exe 17a0fe4cb750) -- 8 checks, 3 turns started, two of them
# (293, 96) through `phase=0 reversing` -> `phase=0->1 side=-0.96...` -> `done dot=0.712... -> NONE`.
$lShotCycle = @('3045.0/-1.8/-1860.0/180:46', '3031.0/-4.8/-2010.0/0:46', '3045.0/-1.8/-1860.0/180:38', '3031.0/-4.8/-2010.0/0:38')
$case.Run.CrashSweepShots = ((@(0..39) | ForEach-Object { $lShotCycle[$_ % 4] }) + @('3038.0/-1.8/-1860.0/180:0')) -join ','
$case.Run.CrashSweepSettle = 300
$case.Run.CrashSweepMax = 900
$case.Run.MaxSeconds = 265
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the 3-point-turn gate is gone'; Pattern = 'Update3PointTurnManoeuvre @0x827190B0'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'a three-point turn STARTED: Update3PointTurnManoeuvre ran its phase 0 (reverse on brake 0.8)'
       Pattern = '\[T-3pt-turn\] vehicle=\d+ phase=0 reversing brake=0\.8 '; Min = 1 }
    @{ Kind = 'Script'; Name = 'a started turn was DRIVEN: phase 0 -> 1 (|side| > 0.96), done (dot > 0.707), or its car moved >= 1 m after the start'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $start = @{}; $flip = @{}; $done = @{}; $bad = @(); $pos = @{}; $startPos = @{}; $moved = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[traffic-track\] t=[\d.]+ id=(\d+) pos=\(([-\d.]+), [-\d.]+, ([-\d.]+)\)') {
                $v = [int]$Matches[1]; $p = @([double]::Parse($Matches[2], $inv), [double]::Parse($Matches[3], $inv))
                $pos[$v] = $p
                if ($startPos.ContainsKey($v) -and -not $moved.ContainsKey($v)) {
                    $d = [math]::Sqrt([math]::Pow($p[0] - $startPos[$v][0], 2) + [math]::Pow($p[1] - $startPos[$v][1], 2))
                    if ($d -ge 1.0) { $moved[$v] = $d.ToString('f1', $inv) + ' m' }
                }
            }
            elseif ($line -match '\[T-3pt-turn\] vehicle=(\d+) phase=0 reversing') {
                $v = [int]$Matches[1]; $start[$v] = $true
                if ($pos.ContainsKey($v) -and -not $startPos.ContainsKey($v)) { $startPos[$v] = $pos[$v] }
            }
            elseif ($line -match '\[T-3pt-turn\] vehicle=(\d+) phase=0->1 side=([^ ]+)') {
                $flip[[int]$Matches[1]] = $true
                if ([math]::Abs([double]::Parse($Matches[2], $inv)) -lt 0.9599995) { $bad += $line }   # > 0.96f, printed to 6 places
            }
            elseif ($line -match '\[T-3pt-turn\] vehicle=(\d+) done dot=([^ ]+)') {
                $done[[int]$Matches[1]] = $true
                if ([double]::Parse($Matches[2], $inv) -lt 0.7069995) { $bad += $line }   # > 0.707f, printed to 6 places
            }
        }
        $driven = @($start.Keys | Where-Object { $flip.ContainsKey($_) -or $done.ContainsKey($_) -or $moved.ContainsKey($_) })
        $movedText = ($moved.Keys | Sort-Object | ForEach-Object { "$_ $($moved[$_])" }) -join ', '
        @{ Pass = ($driven.Count -gt 0 -and $bad.Count -eq 0)
           Detail = "started [$(($start.Keys | Sort-Object) -join ',')]; phase 0->1 [$(($flip.Keys | Sort-Object) -join ',')]; done [$(($done.Keys | Sort-Object) -join ',')]; moved after the start [$movedText]; off the console thresholds: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- turns still in phase 0 when the run ended, or whose car left (never fails)'; Script = {
        param($ctx)
        $start = @{}; $prog = @{}; $gone = @{}; $lastT = 0.0; $startT = @{}
        $inv = [cultureinfo]::InvariantCulture
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[traffic-track\] t=([\d.]+) ') { $lastT = [double]::Parse($Matches[1], $inv) }
            if ($line -match '\[T-3pt-turn\] vehicle=(\d+) phase=0 reversing') { $v = [int]$Matches[1]; if (-not $start.ContainsKey($v)) { $start[$v] = $true; $startT[$v] = $lastT } }
            if ($line -match '\[T-3pt-turn\] vehicle=(\d+) (phase=0->1|done)') { $prog[[int]$Matches[1]] = $true }
            if ($line -match '\[traffic-track\] id=(\d+) REMOVED reason=([^ ]+)') {
                $v = [int]$Matches[1]
                if ($start.ContainsKey($v) -and -not $prog.ContainsKey($v) -and -not $gone.ContainsKey($v)) { $gone[$v] = $Matches[2] }
            }
        }
        $open = @($start.Keys | Where-Object { -not $prog.ContainsKey($_) -and -not $gone.ContainsKey($_) } | ForEach-Object { "$_ (started {0:f1} s before the last track line)" -f ($lastT - $startT[$_]) })
        $left = @($gone.Keys | ForEach-Object { "$_ ($($gone[$_]))" })
        @{ Pass = $true; Detail = "still in phase 0 at the end: [$($open -join '; ')]; left before progressing: [$($left -join ', ')]" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- the player''s checks and the first [T-3pt-turn] lines (never fails)'; Script = {
        param($ctx)
        $checked = @($ctx.LogLines | Where-Object { $_ -match '\[T4-hit\] outcome=CHECKED .*raceCarIdx=0 ' }).Count
        $turn = @($ctx.LogLines | Where-Object { $_ -match '\[T-3pt-turn\]' } | Select-Object -First 6)
        @{ Pass = $true; Detail = "$checked car(s) CHECKED by the player. $(($turn | ForEach-Object { $_.Trim() }) -join ' | ')" }
    } }
)
$case
