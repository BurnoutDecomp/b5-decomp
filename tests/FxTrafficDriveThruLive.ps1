# FX-TRAFFIC (crash parity 2026-09-23, G59-D1) -- live witness for the traffic module's drive-thru
# clean-up. RivalOrganic's boot crosses the junkyard drive-thru (action 99, in and out), which
# WorldModule::BridgeActionsToTrafficModule forwards into the traffic game-action queue; the
# console's HandleExternalRequests arm 0x8274BFA8 then runs ClearupCrashedTraffic and
# KillAllTrafficInCylinder(player, 250 m, 1000 m, false) offline. Before G59-D1 the four actions
# fell into `default: break`. BRN_TRAFFIC_TRACK arms the dispatch line and the per-removal
# reasons (drivethru-clearup-crashed / drivethru-cylinder).
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTrafficDriveThruLive.ps1
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic_drive_thru'
$case.Bug = 'A drive-thru (actions 97..100) must run the traffic clean-up: ClearupCrashedTraffic + the 250 m KillAllTrafficInCylinder on the player (G59-D1).'
$case.DiagEnv += ',BRN_TRAFFIC_TRACK=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'G59-D1 the drive-thru arm is dispatched (0x8274BFA8)'
       Pattern = '\[traffic-track\] drive-thru action=(97|98|99|100) clear-up \+ cylinder r=250(\.0+)? h=1000(\.0+)?'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'G59-D1 the 250 m cylinder removes the cars around the player (RemoveVehicle reason tag)'
       Pattern = '\[traffic-track\] id=\d+ REMOVED reason=drivethru-(cylinder|clearup-crashed) '; Min = 1 }
)
$case
