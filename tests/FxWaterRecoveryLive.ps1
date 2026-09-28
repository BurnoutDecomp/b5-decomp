# Drive on the harbour approach before entering water. A direct junkyard-to-water
# teleport has no AI reset section or recorded road transform and is not a gameplay
# recovery test. The first shot records real wheel/ray contacts; it does not write
# the reset history. The second shot is the established water sound fixture.
$case = & (Join-Path $PSScriptRoot 'FxDirectorWaterFade.ps1')
$case.Name = 'fx_water_recovery'
$case.Bug = 'A water entry after driving on a road must recover to dry ground and resume driving.'
$case.ProfileFixture = 'scratch/OWNERLIST_0927/L2/slot2_profile_drifted_21db22a8.sav'
$case.Run.MaxSeconds = 80
$case.Run.CrashSweep = '1789.888,-2.465,-2338.579'
$case.Run.CrashSweepShots = '1789.888/-2.465/-2338.579/292:12,1790.5/-3.2/-2393.0/260:44'
$case.Run.CrashSweepSettle = 600
$case.Checks += @(
    @{ Kind='Script'; Name='real road history exists before the water shot'; Script={
        param($ctx)
        $ready=$false
        foreach ($line in $ctx.LogLines) {
            if ($line -match '^\[rot-ring\] player depth=[1-4] .*inSystem=1 ') { $ready=$true }
            if ($line -match '^\[sweep\] shot 1/') { break }
        }
        @{Pass=$ready; Detail="road history recorded before water: $ready"}
    } }
    @{ Kind='Script'; Name='water recovery reaches dry ground and remains out of the respawn loop'; Script={
        param($ctx)
        $entered=$false; $dry=0; $resets=0; $lastWater=-1; $i=0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '^\[wrecklatch\] .*water') { $entered=$true; $lastWater=$i; $dry=0 }
            if ($entered -and $line -match '^\[rot\] request resolved: car 0 type 1 ') { $resets++ }
            if ($entered -and $line -match '^\[motion\] n \d+ pos [-\d.]+ (?<y>[-\d.]+) [-\d.]+ .* mph (?<mph>[-\d.]+) ') {
                $y=[double]::Parse($Matches.y,[Globalization.CultureInfo]::InvariantCulture)
                $mph=[double]::Parse($Matches.mph,[Globalization.CultureInfo]::InvariantCulture)
                if ($y -gt -6 -and $mph -gt 5) { $dry++ }
            }
            $i++
        }
        @{Pass=($entered -and $dry -ge 20 -and $resets -le 3); Detail="water=$entered; moving dry samples after last water=$dry; player resets=$resets"}
    } }
)
$case
