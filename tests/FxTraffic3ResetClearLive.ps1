# FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-1) -- live witness for the post-wreck traffic
# clear: TrafficEntityModule::HandleResetRaceCarEvents @0x82742CE8, called from PostPhysicsUpdate's
# RUNNING arm at 0x8274EA5C. After the LOCAL player's reset after a wreck the console removes every
# traffic car, parked ones included, within 75 m (flt_820BA7C4; 12 m online, flt_820BA7CC) and
# 10 m half-height (flt_820BA5E4) of the reset position.
#
# The organic Road Rage pursuit of RivalOrganic.ps1 wrecks the player (the CC-2 run of the same
# drive: BrnGame.log:14560 `[crash-info] player slot 0 wrecked 1 ... hardstopVsWall 1`), and the
# reset puts it back on a busy road. Witnesses:
#   [T-reset] car=0 after-wreck clear at (x, y, z) r=R h=H online=O   (BRN_TRAFFIC_DIAG, capped)
#     -- printed only inside the console's filter (local player AND mbResettingAfterWreck)
#   [traffic-track] id=N REMOVED reason=reset-after-wreck-cylinder ... pos=(x, y, z)
#     -- RemoveVehicle's own line (BRN_TRAFFIC_TRACK) for every car the clear removed
# Before the fix neither line could print: the consumer had no body.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic3ResetClearLive.ps1 --run-name fxtraffic3_reset_clear
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_reset_clear'
$case.Area = 'traffic'
$case.Bug = 'After a player wreck the reset must clear all traffic, parked cars included, within 75 m (12 m online) of the reset point (HandleResetRaceCarEvents, CC-1).'
$case.DiagEnv += ',BRN_TRAFFIC_TRACK=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'context: the player wrecked'
       Pattern = '\[crash-info\] player slot 0 wrecked 1'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'CC-1 dispatched: the local player''s after-wreck reset reached HandleResetRaceCarEvents (offline r=75 h=10)'
       Pattern = '\[T-reset\] car=0 after-wreck clear at \([^)]*\) r=75(\.0+)? h=10(\.0+)? online=0'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'CC-1 effect: traffic near the reset point was removed by the clear'
       Pattern = '\[traffic-track\] id=\d+ REMOVED reason=reset-after-wreck-cylinder'; Min = 1 }
    @{ Kind = 'Script'; Name = 'every clear removal lies inside the console cylinder (d_xz <= r, |dy| < h) of its reset'; Script = {
        param($ctx)
        $inv = [cultureinfo]::InvariantCulture
        $cx = $null; $cy = $null; $cz = $null; $r = 0.0; $h = 0.0
        $n = 0; $static = 0; $bad = @(); $maxD = 0.0
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[T-reset\] car=\d+ after-wreck clear at \(([^,]+), ([^,]+), ([^)]+)\) r=([^ ]+) h=([^ ]+)') {
                $cx = [double]::Parse($Matches[1], $inv); $cy = [double]::Parse($Matches[2], $inv); $cz = [double]::Parse($Matches[3], $inv)
                $r = [double]::Parse($Matches[4], $inv); $h = [double]::Parse($Matches[5], $inv)
            } elseif ($line -match '\[traffic-track\] id=(\d+) REMOVED reason=reset-after-wreck-cylinder state=(\w+) pos=\(([^,]+), ([^,]+), ([^)]+)\)') {
                $n++
                if ($Matches[2] -eq 'static') { $static++ }
                $x = [double]::Parse($Matches[3], $inv); $y = [double]::Parse($Matches[4], $inv); $z = [double]::Parse($Matches[5], $inv)
                if ($null -eq $cx) { $bad += "no [T-reset] before: $line"; continue }
                $d = [math]::Sqrt(($x - $cx) * ($x - $cx) + ($z - $cz) * ($z - $cz))
                if ($d -gt $maxD) { $maxD = $d }
                if ($d -gt $r + 1e-3 -or [math]::Abs($y - $cy) -ge $h) { $bad += $line }
            }
        }
        @{ Pass = ($n -gt 0 -and $bad.Count -eq 0); Detail = "$n clear removals ($static parked), max d_xz $([math]::Round($maxD, 2)) m; outside the cylinder: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
)
$case
