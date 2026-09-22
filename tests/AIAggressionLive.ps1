# FX-AGG live case (crash parity 2026-09-22): the organic Road Rage pursuit of RivalOrganic.ps1 with
# the aggression witness armed. BRN_MM_DIAG=1 prints, once per second per AI car, an [mm-ai] line
# (aggState, speed-match type sm) and, while a slam lineup point is valid, an [mm-ai] lineup line
# with that point's and the car's lateral offsets in the target's right-axis frame.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/AIAggressionLive.ps1 --run-name ai_aggression_live
# It proves the fixed AIAggression code is DISPATCHED in real play and that the run adds no assertion
# or exception. The side check is the live form of G00-D1: GetPositionNextToTarget @0x827714E8 puts
# ATTACK_SLAM's -8 on the far side of the target from the rival and the positive alignments
# (+4 OVERTAKE_TO_SLAM, +7.5 DROP_BACK_TO_SLAM, +6 VEER) on the rival's own side. A sample counts
# only when its state matches its alignment (so the point was computed by that state's handler on
# the previous frame) and the rival is off the target's centreline by at least 0.5 m.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'ai_aggression_live'
$case.Bug = 'Rival aggression must run its lineup and speed-match states in real play, line up on the console side, and add no assertions.'
$case.DiagEnv += ',BRN_MM_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'aggression witness armed'; Pattern = '\[mm-ai\] car \d+ style'; Expect = $true }
    @{ Kind = 'Script'; Name = 'aggression lineup states dispatched'; Script = {
        param($ctx)
        $states = @{}; $modes = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[mm-ai\] car \d+ .*aggState (\d+) .*sm (\d+)') { $states[$Matches[1]] += 1; $modes[$Matches[2]] += 1 }
        }
        $lineup = @($states.Keys | Where-Object { $_ -in @('1', '2', '3', '5') })
        $detail = 'aggState ' + (($states.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)x$($_.Value)" }) -join ' ') +
                  ' | sm ' + (($modes.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)x$($_.Value)" }) -join ' ')
        @{ Pass = $lineup.Count -gt 0; Detail = $detail }
    } }
    @{ Kind = 'Script'; Name = 'lineup point on the console side of the target (G00-D1)'; Script = {
        param($ctx)
        # aggState -> the alignment its handler passes (0x820C41C0, 0x820C42D4, 0x820C26C0, 0x820C4250)
        $alignment = @{ '1' = 4.0; '2' = 7.5; '3' = 8.0; '5' = 6.0 }
        $good = @{}; $bad = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[mm-ai\] lineup car (\d+) aggState (\d+) pointLat ([^ ]+) carLat ([^ ]+)') {
                $state = $Matches[2]
                if (-not $alignment.ContainsKey($state)) { continue }
                $point = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                $car = [double]::Parse($Matches[4], [cultureinfo]::InvariantCulture)
                # One frame of target motion separates the computation from the sample; 0.2 m
                # still tells the nearest pair of alignments (7.5 / 8) apart.
                if ([math]::Abs([math]::Abs($point) - $alignment[$state]) -gt 0.2) { continue }
                if ([math]::Abs($car) -lt 0.5) { continue }
                $opposite = ($point * $car) -lt 0
                if ($opposite -eq ($state -eq '3')) { $good[$state] += 1 } else { $bad[$state] += 1 }
            }
        }
        $nGood = ($good.Values | Measure-Object -Sum).Sum; $nBad = ($bad.Values | Measure-Object -Sum).Sum
        $detail = "console side: " + (($good.GetEnumerator() | Sort-Object Name | ForEach-Object { "state$($_.Name)x$($_.Value)" }) -join ' ') +
                  " | wrong side: " + (($bad.GetEnumerator() | Sort-Object Name | ForEach-Object { "state$($_.Name)x$($_.Value)" }) -join ' ')
        @{ Pass = ($nGood -gt 0 -and -not $nBad); Detail = $detail }
    } }
)
$case
