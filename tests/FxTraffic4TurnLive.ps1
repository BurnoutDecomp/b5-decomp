# FX-TRAFFIC4 item 2 (crash parity wave 5, 2026-09-24) -- live witness for the three-point turn:
# DriveTowardsTarget @0x8273DFC0 starts E_MANOEUVRE_3_POINT_TURN when a driving car's target is more than
# 15 m behind it (0x8273E6B0) and GenerateDriverInputs' manoeuvre-2 arm then calls
# Update3PointTurnManoeuvre @0x827190B0 every frame: phase 0 reverses on brake 0.8 steering on the side
# dot until the target is 74 degrees or more to one side (|dot| > 0.96), phase 1 drives on gas 0.8 with
# opposite lock, and the turn ends once the car faces within 45 degrees of the target (dot > 0.707).
# BRN_TRAFFIC_DIAG witnesses (capped): [T-3pt-turn] vehicle=V phase=0 reversing ... / phase=0->1 ... /
# done dot=D ... -> NONE. The drive is RivalOrganic's organic Road Rage pursuit, run for 160 s: the cars the
# player slams and spins round drive back to their params, and one whose target ends up 15 m behind it
# turns (fxtraffic4/20260924_184432 started one at t = 66 s). The turn only completes if the camera
# stays within 150 m meanwhile (the offscreen valve TryClearupOffscreenTraffic clears it otherwise).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic4TurnLive.ps1 --run-name fxtraffic4_turn
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic4_turn'
$case.Area = 'traffic'
$case.Bug = 'Update3PointTurnManoeuvre had no body and GenerateDriverInputs gated its arm: a turning car sat still until the no-driving latch gave it up (FX-TRAFFIC4 item 2).'
$case.Run.MaxSeconds = 160
$case.DiagEnv = 'BRN_TRAFFIC_DIAG=1,BRN_TRAFFIC_TRACK=1,BRN_RIVAL_PURSUIT_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'the 3-point-turn gate is gone'; Pattern = 'Update3PointTurnManoeuvre @0x827190B0'; Max = 0 }
    @{ Kind = 'Script'; Name = 'every three-point turn that starts is DRIVEN: phase 0 -> 1 or done, unless its car leaves first (no start = no witness)'; Script = {
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
        $left = @($start.Keys | Where-Object { -not ($flip.ContainsKey($_) -or $done.ContainsKey($_)) -and $gone.ContainsKey($_) })
        $stalled = @($start.Keys | Where-Object { -not ($flip.ContainsKey($_) -or $done.ContainsKey($_) -or $gone.ContainsKey($_)) })
        $what = if ($start.Count -eq 0) { 'NO TURN STARTED -- no witness this run' } elseif ($finished.Count -gt 0) { 'WITNESS: a turn progressed' } else { 'started, car left before progressing' }
        $leftText = ($left | ForEach-Object { "$_ ($($gone[$_]))" }) -join ','
        @{ Pass = ($stalled.Count -eq 0); Detail = "$what. reversing [$(($start.Keys | Sort-Object) -join ',')]; phase 0->1 [$(($flip.Keys | Sort-Object) -join ',')]; done [$(($done.Keys | Sort-Object) -join ',')]; removed before progressing [$leftText]; stalled [$($stalled -join ',')]" }
    } }
    @{ Kind = 'Script'; Name = 'INFO -- the first [T-3pt-turn] and [T-avoid] lines (never fails)'; Script = {
        param($ctx)
        $turn = @($ctx.LogLines | Where-Object { $_ -match '\[T-3pt-turn\]' } | Select-Object -First 6)
        $avoid = @($ctx.LogLines | Where-Object { $_ -match '\[T-avoid' }).Count
        @{ Pass = $true; Detail = "$avoid [T-avoid*] lines. $(($turn | ForEach-Object { $_.Trim() }) -join ' | ')" }
    } }
)
$case
