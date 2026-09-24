# FX-TRAFFIC4 item 1 (crash parity wave 5, 2026-09-24) -- live witness for GenerateDriverInputs' static
# (parked) section @0x82749B48..0x8274A4FC: a parked car the player hits becomes physical and moves, and
# from then on the section runs the offscreen clear-up valve TryClearupOffscreenTraffic @0x8273C4C8 on
# it every frame (0x8274A1E8), result ignored, no driver record.
#
# The drive: free burn, teleported onto the street where the parked car at (1752.1, -1.69, -2315.5) was
# hit in four organic runs (fxtraffic3_parked_traffic/20260924_135053 global 451, fxscenemgr_power_parking,
# fxrcem4_diag_latch, fxtraffic3_reset_clear) -- the FX-SCENEMGR power-parking teleport, heading 284 deg,
# ~68 m short of it. Nudge (trips the teleport's 8 m arm), hold still while the street streams in, then
# full throttle for the rest of the run: the car reaches the parked row at ~20-25 m/s (below the 38 m/s
# player-crash speed, so the player drives on) and keeps going west, taking the parked car out of view
# and past 150 m (KF_CLEARUP_FAR_FROM_CAMERA_DIST_SQ 22500, flt_8200D50C) for the valve.
# The hit (physics side) is [T5-arm]/[T5-ram], the world side [T5-apply] (BRN_TRAFFIC_DIAG); the static
# section is [T-static-clearup] (BRN_TRAFFIC_DIAG); a removal by the valve is [traffic-track] CLEARUP /
# REMOVED reason=clearup-offscreen (BRN_TRAFFIC_TRACK).
# RED on a pre-fix exe: the parked car is still hit and still moves (promotion was never the gap), but no
# [T-static-clearup] line exists and "[T3-drive] ... the static/parked pool section" is logged instead.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic4ParkedLive.ps1
@{
    Name = 'fxtraffic4_parked'
    Area = 'traffic'
    Bug  = 'GenerateDriverInputs skipped every physical parked (static-pool) car: the console ends the standard loop at the first index >= 400 and runs TryClearupOffscreenTraffic on each remaining physical car (FX-TRAFFIC4 item 1).'
    Frames = $false
    Run = @{
        Drive           = $true
        MotionProbe     = $true
        SkipIntro       = $true
        AcceptGap       = 1.0
        SkipTrainingTip = $true
        MaxSeconds      = 80
        Teleport        = '1817.8,-3.2,-2333.7,284'
        ThrottleScript  = '0:accel,3:handbrake,9:accel'
    }
    DiagEnv = 'BRN_TRAFFIC_DIAG=1,BRN_TRAFFIC_TRACK=1'
    Checks = @(
        @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
        @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
        @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
        @{ Kind = 'Script'; Name = 'a PARKED car (static pool, 400..598) was hit and the world side moved it'; Script = {
            param($ctx)
            $inv = [cultureinfo]::InvariantCulture
            $parked = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T5-arm\] slot=\d+ global=(\d+) crashing=(\d)') {
                    $g = [int]$Matches[1]
                    if ($g -ge 400 -and $g -lt 599) { $parked[$g] = $Matches[2] }
                }
            }
            $best = 0.0; $bestId = -1; $first = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T5-apply\] f=\d+ vehicle=(\d+) \|delta\|=[^ ]+ worldPos=\(([^,]+),([^,]+),([^)]+)\)') {
                    $v = [int]$Matches[1]
                    if (-not $parked.ContainsKey($v)) { continue }
                    $p = @([double]::Parse($Matches[2], $inv), [double]::Parse($Matches[3], $inv), [double]::Parse($Matches[4], $inv))
                    if (-not $first.ContainsKey($v)) { $first[$v] = $p; continue }
                    $d = [math]::Sqrt([math]::Pow($p[0] - $first[$v][0], 2) + [math]::Pow($p[2] - $first[$v][2], 2))
                    if ($d -gt $best) { $best = $d; $bestId = $v }
                }
            }
            $ids = ($parked.Keys | Sort-Object) -join ','
            @{ Pass = ($parked.Count -gt 0 -and $best -ge 0.5); Detail = "parked cars hit: [$ids]; largest world displacement $([math]::Round($best, 2)) m (vehicle $bestId)" }
        } }
        @{ Kind = 'Script'; Name = 'the static section ran the clear-up valve on the hit parked car (0x8274A1E8)'; Script = {
            param($ctx)
            $parked = @{}
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T5-arm\] slot=\d+ global=(\d+)') {
                    $g = [int]$Matches[1]
                    if ($g -ge 400 -and $g -lt 599) { $parked[$g] = $true }
                }
            }
            $seen = @{}; $n = 0; $cleared = @()
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[T-static-clearup\] vehicle=(\d+) species=(\d+).* far=(\d) cleared=(\d)') {
                    $n++
                    $seen[[int]$Matches[1]] = $true
                    if ($Matches[4] -eq '1') { $cleared += [int]$Matches[1] }
                }
            }
            $hit = @($parked.Keys | Where-Object { $seen.ContainsKey($_) })
            @{ Pass = ($hit.Count -gt 0); Detail = "$n [T-static-clearup] lines; hit parked cars visited: [$($hit -join ',')]; cleared by the valve: [$($cleared -join ',')]" }
        } }
        @{ Kind = 'LogCount'; Name = 'the static-pool gate is gone (no "[T3-drive] ... static/parked pool section")'; Pattern = 'static/parked pool section'; Max = 0 }
        @{ Kind = 'Script'; Name = 'INFO -- how each hit parked car left the physical set (never fails)'; Script = {
            param($ctx)
            $out = @()
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[traffic-track\] id=(\d+) (CLEARUP|REMOVED reason=[^ ]+)') {
                    $v = [int]$Matches[1]
                    if ($v -ge 400 -and $v -lt 599) { $out += "$v $($Matches[2])" }
                }
                if ($line -match '\[T3-demote\] #\d+ vehicle (\d+) ') {
                    $v = [int]$Matches[1]
                    if ($v -ge 400 -and $v -lt 599) { $out += "$v demoted" }
                }
            }
            @{ Pass = $true; Detail = (($out | Select-Object -First 12) -join '; ') }
        } }
    )
}
