# FX-SCENARIOS (crash parity 2026-09-25) -- a traffic CHECK probe: the scenario for the three-point turn
# (FxTraffic4TurnLive.ps1). It changes only the scenario.
#
# WHAT STARTS A TURN. DriveTowardsTarget @0x8273DFC0 sets E_MANOEUVRE_3_POINT_TURN (0x8273E6B0) when the target
# is more than 15 m behind the car (forward distance < KF_DRIVER_REVERSE_TURN_DIST) AND the car faces against
# its param's direction (dot < 0) AND no manoeuvre runs. So the car must be turned round while physical. The
# banked runs hold 10 turn starts in 9 runs (of 164 with BRN_TRAFFIC_DIAG; 8 in the 96 RivalOrganic Road Rage
# runs); 6 of them followed a CHECK (fxtraffic4/20260924_184432 vehicle 266 by a rival, fxcrashvfx_hinge/
# 20260925_070742 180, fxcrashvfx_hinge/20260925_072037 386, fxrcem4_cars_ready/20260924_223335 29,
# fxtraffic3_give_up/20260924_205051 383, fxxlane_sweeteners_aipad/20260925_110716 29) and one a sympathetic
# crash's give-up (fxrcem4_collision_post/20260924_200007 143). SetTrafficVehicleChecked @0x8262D748 records steering
# side * 0.5 (flt_82001DA0) and drive sign(checker speed) * 1.0 (flt_82001C98), and UpdateRecoveringFromSlam
# @0x8273E778 drives the car on those for 2.5 s (flt_82005548) -- full gas on half lock turns it round. A SLAM
# records drive * 0.5 and steering side * magnitude / 1000 (unk_82FB8B50), which cannot turn a car. The checks
# that did NOT turn their car were pushed from behind by the pursuing player the whole 2.5 s ([T-stuck] back
# timer past 2 s -> the wedge cut zeroes the gas; fxwitness_hinge_1/20260925_065018 vehicles 199, 16, 339).
#
# WHAT MAKES A CHECK. DecideOutcomeOfRaceCarTrafficContact: CRASH when impactSpeed * trafficMass / raceCarMass
# exceeds the player's crash speed (37.998 m/s * scale, [T4-crash-test]); else CHECK when impactSpeed *
# raceCarMass / trafficMass > 30 (flt_82004F5C). The player car is 1589 kg, a sedan 1450 kg: a CHECK closes at
# 27.4 .. 41.6 m/s -- a 46 m/s car rear-ending traffic at ~14 m/s, or a 38 m/s car meeting a stopped one.
#
# THE DRIVE. Free roam. The launch point of FxScenariosTrafficProbe.ps1 is a junction of a divided road (lanes
# x~3044.5 / 3049.5 flowing -z, x~3026.5 / 3031 flowing +z) and a road from the east flowing -x (fxscenarios_
# traffic_probe/20260925_110304 [traffic-track]). The deterministic crash sweep puts the car 78 m up the busy -z
# lane heading 180, or 70 m down the +z lane heading 0, at 46 or 38 m/s, every 420 sim frames (7 s); the pad
# drives 3 s (the sweep arms on 8 m) and then COASTS, so a checked car is not pushed on, and the next placement
# takes the player off it. Every placement is in the same 150 m, so the checked car stays inside the offscreen
# valve's radius while it turns. INFO checks, plus FxTraffic4TurnLive's own verdict on any turn that starts.
# RESULT: fxscenarios_traffic_check_probe/20260925_121711 (exe b745b4a968f9): 5 cars CHECKED by the player, TWO turns
# started -- vehicle 278 `phase=0 reversing` -> `phase=0->1 side=-0.962008` -> `done dot=0.707162 -> NONE`, vehicle
# 286 still in phase 0 at the end. The same drive in fxtraffic4_turn/20260925_124312 checked 2 and started none,
# so FxTraffic4TurnLive runs it with twice the placements.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenariosTrafficCheckProbe.ps1
$case = & (Join-Path $PSScriptRoot 'FxScenariosTrafficProbe.ps1')
$case.Name = 'fxscenarios_traffic_check_probe'
$case.Bug = 'Scenario probe: a traffic car the player CHECKS and leaves must be free to turn round and start the three-point turn.'
$case.Run.ThrottleScript = '0:accel,3:none'
$lShotCycle = @('3045.0/-1.8/-1860.0/180:46', '3031.0/-4.8/-2010.0/0:46', '3045.0/-1.8/-1860.0/180:38', '3031.0/-4.8/-2010.0/0:38')
$case.Run.CrashSweepShots = (@(0..19) | ForEach-Object { $lShotCycle[$_ % 4] }) -join ','
$case.Run.CrashSweepSettle = 420
$case.Run.CrashSweepMax = 900
$case.Run.MaxSeconds = 195
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'Script'; Name = 'INFO -- the sweep placements (never fails)'; Script = {
        param($ctx)
        $shots = @($ctx.LogLines | Where-Object { $_ -match '\[sweep\] (shot|SHOT)' })
        @{ Pass = $true; Detail = "$($shots.Count) [sweep] shot line(s); last: $(($shots | Select-Object -Last 1))" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- the player''s contacts with traffic (decided outcomes; never fails)'; Script = {
        param($ctx)
        $counts = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[T4-hit\] decided outcome=([A-Z_]+) .*raceCar=0 ') { $k = $Matches[1]; if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 } }
        }
        $checked = @($ctx.LogLines | Where-Object { $_ -match '\[T4-hit\] outcome=CHECKED .*raceCarIdx=0 ' }).Count
        @{ Pass = $true; Detail = "decided: $((($counts.Keys | Sort-Object | ForEach-Object { "$_ x$($counts[$_])" }) -join ', ')); CHECKED by the player: $checked" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- checked cars on full gas, and the turn witnesses (never fails)'; Script = {
        param($ctx)
        $drive = @($ctx.LogLines | Where-Object { $_ -match '\[T5-slam\] veh=\d+ physTime=0\.016667 drv=1\.000000 steer=-?0\.500000' }).Count
        $turn = @($ctx.LogLines | Where-Object { $_ -match '\[T-3pt-turn\]' } | Select-Object -First 6)
        @{ Pass = $true; Detail = "$drive checked car(s) seen starting on drv 1.0 / steer 0.5 ([T5-slam] is capped at 200 lines). $(($turn | ForEach-Object { $_.Trim() }) -join ' | ')" }
    } }
    @{ Kind = 'Script'; Name = 'FxTraffic4TurnLive''s verdict: every three-point turn that starts is DRIVEN (no start = no witness)'; Script = {
        param($ctx)
        $start = @{}; $flip = @{}; $done = @{}; $gone = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-3pt-turn\] vehicle=(\d+) phase=0 reversing') { $start[[int]$Matches[1]] = $true }
            if ($line -match '\[T-3pt-turn\] vehicle=(\d+) phase=0->1') { $flip[[int]$Matches[1]] = $true }
            if ($line -match '\[T-3pt-turn\] vehicle=(\d+) done') { $done[[int]$Matches[1]] = $true }
            if ($line -match '\[traffic-track\] id=(\d+) REMOVED reason=([^ ]+)') {
                $v = [int]$Matches[1]
                if ($start.ContainsKey($v) -and -not $gone.ContainsKey($v)) { $gone[$v] = $Matches[2] }
            }
        }
        $finished = @($start.Keys | Where-Object { $flip.ContainsKey($_) -or $done.ContainsKey($_) })
        $stalled = @($start.Keys | Where-Object { -not ($flip.ContainsKey($_) -or $done.ContainsKey($_) -or $gone.ContainsKey($_)) })
        $what = if ($start.Count -eq 0) { 'NO TURN STARTED' } elseif ($finished.Count -gt 0) { 'WITNESS: a turn progressed' } else { 'started, car left before progressing' }
        @{ Pass = $true; Detail = "$what. reversing [$(($start.Keys | Sort-Object) -join ',')]; phase 0->1 [$(($flip.Keys | Sort-Object) -join ',')]; done [$(($done.Keys | Sort-Object) -join ',')]; stalled [$($stalled -join ',')]" }
    } }
)
$case
