# FX-RUMBLE (crash parity 2026-09-22, G10-D1/D2/D3/D5/D7) -- the force-feedback producers, live.
# Driven by tests/run_rival_organic.py (organic pad pursuit of a rival; nothing injected):
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/RumbleLive.ps1 --run-name rumble_live
# BRN_RUMBLE_DIAG arms RumbleManager::PlayJolt's witness: one `[rumble] jolt prio=<p> queued=<0|1>
# len=<n> ...` line per jolt the game state asks for (per-producer budget), where the priority names
# the producer -- 1000 UpdateImpacts (the hardest contact in the player's run of the race-car contact
# queue), 1001 ProcessGameEvents case 31 -> OnVehicleAggressor/VictimImpact, 1002 the landing jolt,
# 1010 the crash-start jolt. queued=0 once len=4 is the missing input bridge (G10-D4), not a fault.
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'rumble_live'
$case.Area = 'rumble'
$case.Bug = 'The GameState module must run RumbleManager every pre-world frame and turn player contacts / rival impacts into queued pad jolts, with no new asserts.'
$case.DiagEnv += ',BRN_RUMBLE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'context: a real race-car contact happened'; Pattern = '\[td-contact\] entry race car'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'UpdateImpacts dispatched a contact jolt (prio 1000)'; Pattern = '\[rumble\] jolt prio=1000 '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'case 31 dispatched a race-car impact jolt (prio 1001)'; Pattern = '\[rumble\] jolt prio=1001 '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the Constructed jolt queue accepted a jolt'; Pattern = '\[rumble\] jolt prio=\d+ queued=1 '; Expect = $true }
)
$case
