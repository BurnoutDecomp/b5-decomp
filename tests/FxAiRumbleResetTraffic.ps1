# FX-AI-RUMBLE live case (crash parity 2026-09-23, G07-D1): the organic Road Rage pursuit of
# RivalOrganic.ps1 with the reset-on-track traffic witness armed.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxAiRumbleResetTraffic.ps1 --run-name fxairumble_hng
# BRN_ROT_TRAFFIC_DIAG=1 prints, from ResetOnTrackManager::TestCarHNG @0x82790BD8 whenever it reaches
# its two TRAFFIC legs (a nearby set was passed and neither hard-no-go section leg hit):
#   [rot-hng] traffic legs call N nearby C speed S -> AHEAD hit | ACROSS hit | clear hits H/N
# Before the fix those legs were a logged `return false` ("[rot] PARKED: ... traffic legs need
# LineTestTrafficHNG"), so a reset pose was never swept off a vehicle standing on it. The checks
# need the legs dispatched in real play with a non-empty avoidance list, the old PARKED line gone,
# and no assertion (NearbyVehicles::GetCount now carries the console's two asserts) or exception.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxairumble_hng'
$case.Area = 'reset-on-track'
$case.Bug = 'Reset-on-track must test the avoidance list with LineTestTrafficHNG (both TestCarHNG traffic legs) and the run must add no assertions.'
$case.DiagEnv += ',BRN_ROT_TRAFFIC_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'context: reset-on-track requests resolved'; Pattern = '\[rot\] request resolved: car'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'the traffic-legs PARKED line is gone'; Pattern = 'traffic legs need LineTestTrafficHNG'; Max = 0 }
    @{ Kind = 'LogMatch'; Name = 'TestCarHNG reached its traffic legs (G07-D1 dispatched)'; Pattern = '\[rot-hng\] traffic legs call \d+ nearby [1-9]'; Expect = $true }
    @{ Kind = 'Script'; Name = 'traffic-leg answers -- informational'; Script = {
        param($ctx)
        $calls = 0; $ahead = 0; $across = 0; $maxNearby = 0; $last = ''
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[rot-hng\] traffic legs call (\d+) nearby (\d+) speed ([^ ]+) -> (AHEAD hit|ACROSS hit|clear) hits (\d+)/(\d+)') {
                $calls += 1
                if ($Matches[4] -eq 'AHEAD hit') { $ahead += 1 } elseif ($Matches[4] -eq 'ACROSS hit') { $across += 1 }
                if ([int]$Matches[2] -gt $maxNearby) { $maxNearby = [int]$Matches[2] }
                $last = "$($Matches[5])/$($Matches[6])"
            }
        }
        @{ Pass = $true; Detail = "printed lines $calls (ahead hits $ahead, across hits $across), max nearby $maxNearby, running total hits/calls $last" }
    } }
)
$case
