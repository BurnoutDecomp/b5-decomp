# FX-RCEM4 (crash parity 2026-09-24, reviewer B on 87d1ad23 / G62) -- live witness for
# ActiveRaceCar::OnResourcesLoaded @0x822EB168's Def() legs on every car the Road Rage start loads
# (the RivalOrganic event: player + rivals). BRN_RESLOADED_DIAG arms the capped [res-loaded] line the
# body prints after its legs ran.
#   * 0x822EB20C: mCentreOfMassTransform = the spec's +1552 matrix, copied at load -- the line must
#     report a resolved spec and the authored translation (PUSMC01: (0, -0.740575, 0.170226)), and the
#     promote seam's [seat] line (which prints -M1552.w straight off the spec) must agree with it.
#     Before the fix the copy happened one step later, at the promote seam, through
#     SetCentreOfMassTransformBringUp, and the line did not exist.
#   * 0x822EB410: the four RenderParams::SetWheelScale calls -- the line reports each wheel's scale
#     matrix diagonal after the loop; wheel 1's y must be twice the radius1 the promote seam's [seat]
#     line reads off the same WheelSpec (0.5 * maWheelSpecs[1].mScale.y). Before the fix the scales
#     were set at the promote seam instead (the identity until then).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4ResourcesLoadedLive.ps1 --run-name fxrcem4_resloaded
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_resloaded'
$case.Bug = 'ActiveRaceCar::OnResourcesLoaded must copy the streamed spec''s car-model-space to handling-body-space matrix into mCentreOfMassTransform (0x822EB20C) and set the four render wheel scales (0x822EB410) at load.'
$case.DiagEnv += ',BRN_RESLOADED_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'OnResourcesLoaded resolved the spec and copied the +1552 matrix'
       Pattern = '\[res-loaded\] OnResourcesLoaded slot \d+ spec 1 mCentreOfMassTransform\.w \('; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'no load with an unresolved spec'
       Pattern = '\[res-loaded\] OnResourcesLoaded slot \d+ spec 0 '; Max = 0 }
    @{ Kind = 'Script'; Name = 'the copied matrix is the one the promote seam reads off the spec'; Script = {
        param($ctx)
        $loaded = @{}
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[res-loaded\] OnResourcesLoaded slot (\d+) spec 1 mCentreOfMassTransform\.w \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)') {
                $loaded[[int]$Matches[1]] = @([double]$Matches[2], [double]$Matches[3], [double]$Matches[4])
            }
        }
        $seats = @($ctx.LogLines | Where-Object { $_ -match '\[seat\] car (\d+) .* -M1552\.w \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)' })
        $bad = @(); $matched = 0
        foreach ($s in $seats) {
            if ($s -match '\[seat\] car (\d+) .* -M1552\.w \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)') {
                $slot = [int]$Matches[1]
                if (-not $loaded.ContainsKey($slot)) { continue }
                $w = $loaded[$slot]
                $d = [math]::Abs($w[0] + [double]$Matches[2]) + [math]::Abs($w[1] + [double]$Matches[3]) + [math]::Abs($w[2] + [double]$Matches[4])
                if ($d -gt 1e-5) { $bad += "slot $slot com.w ($($w -join ', ')) vs -M1552.w ($($Matches[2]), $($Matches[3]), $($Matches[4]))" } else { $matched++ }
            }
        }
        @{ Pass = ($matched -gt 0 -and $bad.Count -eq 0);
           Detail = "$($loaded.Count) slot(s) loaded, $matched promote(s) agree" + $(if ($bad.Count) { "; MISMATCH: " + ($bad -join ' | ') } else { '' }) }
    } }
    @{ Kind = 'Script'; Name = 'the wheel scales are set at load from the authored WheelSpecs'; Script = {
        param($ctx)
        $wheel1 = @{}; $unset = @()
        foreach ($l in $ctx.LogLines) {
            if ($l -match '\[res-loaded\] OnResourcesLoaded slot (\d+) spec 1 .* wheel0 scale \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\) wheel1 scale \(([-\d.e]+), ([-\d.e]+), ([-\d.e]+)\)') {
                $wheel1[[int]$Matches[1]] = [double]$Matches[6]
                if ([double]$Matches[3] -eq 1.0 -and [double]$Matches[6] -eq 1.0) { $unset += "slot $($Matches[1])" }
            }
        }
        $bad = @(); $matched = 0
        foreach ($s in @($ctx.LogLines | Where-Object { $_ -match '\[seat\] car \d+ .* radius1 ' })) {
            if ($s -match '\[seat\] car (\d+) .* radius1 ([-\d.e]+) ') {
                $slot = [int]$Matches[1]
                if (-not $wheel1.ContainsKey($slot)) { continue }
                if ([math]::Abs($wheel1[$slot] - 2.0 * [double]$Matches[2]) -gt 1e-5) { $bad += "slot $slot wheel1.y $($wheel1[$slot]) vs 2*radius1 $(2.0 * [double]$Matches[2])" } else { $matched++ }
            }
        }
        @{ Pass = ($wheel1.Count -gt 0 -and $matched -gt 0 -and $bad.Count -eq 0 -and $unset.Count -eq 0);
           Detail = "$($wheel1.Count) slot(s) with scales, $matched promote(s) agree" + $(if ($bad.Count) { "; MISMATCH: " + ($bad -join ' | ') } else { '' }) + $(if ($unset.Count) { "; IDENTITY: " + ($unset -join ', ') } else { '' }) }
    } }
    @{ Kind = 'Script'; Name = 'loaded matrices (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[res-loaded\]' })
        @{ Pass = $true; Detail = "$($l.Count) line(s); " + (($l | Select-Object -First 3 | ForEach-Object { $_.Trim() }) -join ' | ') }
    } }
)
$case
