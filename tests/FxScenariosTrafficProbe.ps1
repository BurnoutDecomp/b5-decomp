# FX-SCENARIOS (crash parity 2026-09-25) -- a traffic ROADBLOCK probe: the scenario base for the physical-traffic
# driving arms that the organic pursuits reach only by luck (FxTraffic3GiveUpLive, FxTraffic3StuckReverseLive,
# FxTraffic4TurnLive). It changes only the scenario.
#
# WHY A ROADBLOCK. DriveTowardsTarget @0x8273DFC0 (and with it CheckIfPhysicalVehicleIsStuck, the three-point-turn
# start and the slam give-up) runs only for a physical traffic car that is NOT crashing: a NormalPhysical car, a
# SWERVING promotion that UpdateExtremeSwerving hands to it (crash slider < 0.4 or the player > 40 m away), or a
# slammed car after 2.5 s (UpdateRecoveringFromSlam, flt_82005548). The organic pursuits crash most of what they
# touch (CRASH_TRAFFIC above ~33..47 m/s of impact, [T4-crash-test]) and every crashed car raises the crash slider,
# which turns the next promotions into sympathetic crashes. A stationary player car in a traffic lane is hit at the
# traffic's own speed: a SLAM (slam magnitude <= 30, DecideOutcomeOfRaceCarTrafficContact), the slider stays low,
# and the slammed car then has to drive around, back off from, or give up against the car in its way.
#
# THE DRIVE. Free roam (no event). The deterministic crash sweep (flow_run -CrashSweep, frame-locked) places the
# player car at RivalOrganic's road, turned against the lane's flow (heading 0, as FxTraffic3StuckReverseLive's
# wrong-way drive), at a walking 9 m/s, and puts it back there every 15 s (900 sim frames), so the camera never
# leaves the scene (the offscreen valve clears physical traffic past 150 m). The pad drives 3 s (the sweep arms on
# 8 m driven) and then holds the HANDBRAKE for the rest of the run: the car is the roadblock.
# INFO checks only; the per-case variants carry the real checks.
# RESULT: fxscenarios_traffic_probe/20260925_110304 (exe 1c59049819d0): [T4-hit] SLAMMED x4, 1 dispatch, 43 stuck
# responses, 17 [T-stuck-reverse] lines, 5 give-up hand-backs, no three-point turn. On this drive:
# FxTraffic3GiveUpLive GREEN (fxtraffic3_give_up/20260925_113624), FxTraffic3StuckReverseLive GREEN
# (fxtraffic3_stuck_reverse/20260925_120118).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxScenariosTrafficProbe.ps1
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxscenarios_traffic_probe'
$case.Area = 'traffic'
$case.Bug = 'Scenario probe: physical traffic must reach DriveTowardsTarget''s arms (stuck test, give-up, three-point turn) around a stationary player car.'
$case.Run.Remove('Teleport')
$case.Run.StartEvent = $false
$case.Run.EventFsm = $false
$case.Run.Boost = ''
$case.Run.ThrottleScript = '0:accel,3:handbrake'
$case.Run.CrashSweep = '3040.7,-5.8,-1937.9'
$case.Run.CrashSweepShots = (@(1..12) | ForEach-Object { '0:9' }) -join ','
$case.Run.CrashSweepSettle = 900
$case.Run.CrashSweepMax = 1800
$case.Run.MaxSeconds = 200
$case.DiagEnv = 'BRN_TRAFFIC_DIAG=1,BRN_TRAFFIC_TRACK=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'Script'; Name = 'INFO -- the sweep placed the roadblock (never fails)'; Script = {
        param($ctx)
        $shots = @($ctx.LogLines | Where-Object { $_ -match '\[sweep\] (shot|SHOT)' })
        @{ Pass = $true; Detail = "$($shots.Count) [sweep] shot line(s); first: $(($shots | Select-Object -First 1))" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- traffic outcomes against the roadblock (never fails)'; Script = {
        param($ctx)
        $counts = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[T4-hit\] outcome=([A-Z_]+)') { $k = $Matches[1]; if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 } }
        }
        @{ Pass = $true; Detail = (($counts.Keys | Sort-Object | ForEach-Object { "$_ x$($counts[$_])" }) -join ', ') }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- DriveTowardsTarget arms reached (never fails)'; Script = {
        param($ctx)
        $n = @{}
        foreach ($p in @('\[T-stuck-check\] dispatched', '\[T-stuck-check\] vehicle=\d+ front=.*below 3\.2', '\[T-stuck-check\] vehicle=\d+ front=.*-> (NONE|STUCK_REVERSE|GIVE_UP)',
                          '\[T-stuck-reverse\]', '\[T-give-up\] vehicle=\d+ phase=0->1', '\[T-give-up\] vehicle=\d+ phase=1 waited=', '\[T-3pt-turn\] vehicle=\d+ phase=0 reversing',
                          '\[T-3pt-turn\] vehicle=\d+ phase=0->1', '\[T-3pt-turn\] vehicle=\d+ done', '\[T-stuck\] vehicle=', '\[T5-slam\]', '\[T-avoid\]')) {
            $n[$p] = @($ctx.LogLines | Where-Object { $_ -match $p }).Count
        }
        @{ Pass = $true; Detail = (($n.Keys | ForEach-Object { "$($_ -replace '\\', '') x$($n[$_])" }) -join '; ') }
    } }
)
$case
