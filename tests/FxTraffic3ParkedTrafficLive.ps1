# FX-TRAFFIC3 item 4 (crash parity wave 5, 2026-09-24) -- live witness for the Power Parking producer
# TrafficEntityModule::GenerateNearbyParkedTrafficOutput @0x8271FA18, called by PreSceneUpdate at 0x8274AB14.
#
# The console runs its loop only while the player power parks (+0x717E5, republished each frame from
# RaceCarToTrafficInterface bit 0). That bit's only producer, RaceCarEntityModule::ProcessPowerParking
# @0x822CDF10, has no PC body yet, so on PC the gate stays closed and the producer returns at once
# (`[T-parked] dispatched, mbPlayerIsPowerParking=0` once). BRN_PARKED_DRYRUN (NOT IN THE X360 BINARY)
# lets the same loop run on a closed gate WITHOUT the publish, on the frames the player car is active,
# and prints one capped line each time the count changes:
#   [T-parked] dry-run (not published) count=N closestSq=D1 secondSq=D2 angle=A perp=P
# so this drive shows the body measuring the real parked cars that come within 15 m of the player.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic3ParkedTrafficLive.ps1 --run-name fxtraffic3_parked_traffic
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_parked_traffic'
$case.Area = 'traffic'
$case.Bug = 'PreSceneUpdate must dispatch GenerateNearbyParkedTrafficOutput, and its loop must measure the parked cars within 15 m of the player (item 4; dry run while the ProcessPowerParking gate has no PC producer).'
$case.DiagEnv += ',BRN_PARKED_DRYRUN=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'item 4 dispatched: PreSceneUpdate reached GenerateNearbyParkedTrafficOutput'
       Pattern = '\[T-parked\] (dispatched|published|dry-run)'; Min = 1 }
    @{ Kind = 'Script'; Name = 'a parked car within 15 m of the player was measured, and every line obeys CheckVehicleForPowerPark'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $n = 0; $near = 0; $bad = @(); $best = [double]::MaxValue; $maxCount = 0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-parked\] (?:dry-run \(not published\)|published) count=(\d+) closestSq=([^ ]+) secondSq=([^ ]+) angle=([^ ]+) perp=([^ ]+)') {
                $n++
                $count = [int]$Matches[1]
                $c = [double]::Parse($Matches[2], $inv); $s = [double]::Parse($Matches[3], $inv)
                $a = [double]::Parse($Matches[4], $inv); $p = [double]::Parse($Matches[5], $inv)
                if ($count -gt $maxCount) { $maxCount = $count }
                if ($count -ge 1) {
                    $near++
                    if ($c -lt $best) { $best = $c }
                    # counted => inside 225 (flt_82018E3C); the closest refreshed angle in [0, pi/2] and perp >= 0
                    if ($c -gt 225.0 -or $a -lt 0.0 -or $a -gt [math]::PI / 2 + 1e-4 -or $p -lt 0.0) { $bad += $line }
                    if ($count -ge 2 -and ($s -gt 225.0 -or $s -lt $c)) { $bad += $line }
                } elseif ($c -lt 1e38 -or $s -lt 1e38 -or $a -lt 1e38 -or $p -lt 1e38) {
                    $bad += $line   # nothing counted => the four FLT_MAX seeds (flt_820BA23C)
                }
            }
        }
        $closest = if ($near -gt 0) { [math]::Round([math]::Sqrt($best), 2) } else { 'n/a' }
        @{ Pass = ($near -gt 0 -and $bad.Count -eq 0); Detail = "$n lines, $near with a parked car inside 15 m (max count $maxCount, nearest $closest m); broken invariants: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
    @{ Kind = 'Script'; Name = 'context (info): the console gate stays closed on PC -- ProcessPowerParking has no body'; Script = {
        param($ctx)
        $gate = @($ctx.LogLines | Where-Object { $_ -match '\[T-parked\] dispatched, mbPlayerIsPowerParking=0' }).Count
        $pub = @($ctx.LogLines | Where-Object { $_ -match '\[T-parked\] published' }).Count
        @{ Pass = $true; Detail = "closed-gate line: $gate; published lines: $pub" }
    } }
)
$case
