# FX-AIDRV live case (crash parity 2026-09-22): the organic Road Rage pursuit of RivalOrganic.ps1 with
# the steering-fan witness armed. BRN_AI_FAN_DIAG=1 prints:
#   [aidrv] centre-row fan F ahead A recip R minRow M live L/C   (SteeringFan::IncludeCentreLineTracking)
#   [aidrv] furthest traffic|race-car index I evicted E/N         (the two avoidance feeders, full list)
# plus the pre-existing [aiprep]/[aiaccum] fan lines.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/AIDrvLive.ps1 --run-name aidrv_live
# G03-D1: AIDriver::Prepare @0x82792CA8 draws each driver's mfCentreLineAhead from AIModule::mRandom
# (RandomFloat(0.9975, 0.985)) and stores 1/(1-v). Before the fix every rival kept 0/0 and the
# eFan_SteerToCentre row (0x82786BC8) was all zero. The checks below need the row computed with a
# threshold in [0.985, 0.9975] and a matching reciprocal, and at least one sample where the row is
# non-zero -- i.e. the draw reached the rivals' racing line in real play -- and no assertion or
# exception anywhere in the run (G01-D1 GetUsefulDirection runs every AI tick on the same path).
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'aidrv_live'
$case.Bug = 'Every rival must steer with a drawn centre-line-ahead threshold (live SteerToCentre row) and the run must add no assertions.'
$case.DiagEnv += ',BRN_AI_FAN_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'centre-row witness armed'; Pattern = '\[aidrv\] centre-row fan'; Expect = $true }
    @{ Kind = 'Script'; Name = 'centre-line threshold drawn in [0.985, 0.9975] with recip 1/(1-v) (G03-D1)'; Script = {
        param($ctx)
        $good = 0; $bad = 0; $zero = 0; $values = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[aidrv\] centre-row fan (\d+) ahead ([^ ]+) recip ([^ ]+)') {
                $ahead = [double]::Parse($Matches[2], [cultureinfo]::InvariantCulture)
                $recip = [double]::Parse($Matches[3], [cultureinfo]::InvariantCulture)
                if ($ahead -eq 0.0) { $zero += 1; continue }
                $expect = 1.0 / (1.0 - $ahead)
                if ($ahead -ge 0.98499 -and $ahead -le 0.99751 -and [math]::Abs($recip - $expect) -le 0.01 * $expect) {
                    $good += 1; $values["$($Matches[1])"] = $ahead
                } else { $bad += 1 }
            }
        }
        $detail = "in-range samples $good, out-of-range $bad, zero-threshold $zero; per fan: " +
                  (($values.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ' ')
        @{ Pass = ($good -gt 0 -and $bad -eq 0); Detail = $detail }
    } }
    @{ Kind = 'Script'; Name = 'SteerToCentre row is live (non-zero) in real play (G03-D1)'; Script = {
        param($ctx)
        $live = 0; $min = 0.0; $last = ''
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[aidrv\] centre-row fan \d+ .*minRow ([^ ]+) live (\d+)/(\d+)') {
                $row = [double]::Parse($Matches[1], [cultureinfo]::InvariantCulture)
                if ($row -lt 0.0) { $live += 1; if ($row -lt $min) { $min = $row } }
                $last = "$($Matches[2])/$($Matches[3])"
            }
        }
        @{ Pass = ($live -gt 0); Detail = "samples with a negative row $live, most negative $min, last live/computed $last" }
    } }
    @{ Kind = 'Script'; Name = 'full-list eviction path (G03-D2) -- informational'; Script = {
        param($ctx)
        $asked = 0; $evicted = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[aidrv\] furthest (\S+) index (-?\d+) evicted (\d+)/(\d+)') { $asked += 1; if ([int]$Matches[2] -ge 0) { $evicted += 1 } }
        }
        @{ Pass = $true; Detail = "furthest-vehicle lines $asked, of which evictions $evicted (0 = the list never filled this run)" }
    } }
)
$case
