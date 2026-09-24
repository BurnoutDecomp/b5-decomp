# FX-NANPOL (crash parity 2026-09-24): live smoke for the removal of the three host-only null
# [GUARD]s (b5 023e833c). The race clock (RaceBalancingManager::Update, inlined at 0x8279B678) now
# dereferences GetAICar(mePlayerGlobalRaceCarIndex) unconditionally (row 12 @0x8279B674 has no
# INVALID mapping any more), and UpdateCarRoutes' event-117 gate dereferences GetAIDriver(active)
# and GetAICar(global) unconditionally (0x82795780 / 0x827957A0), as the console does. The unit
# runner tests/run_fxnanpol_guards.py pins why those pointers cannot be null; this case proves the
# dereferences run in a real race and fault nowhere:
#   [resetpump] ... mbPlayerDataSet is SET   -- AIModule::Update's gated body ran (rows 11/12/26)
#   [racebal] race clock running             -- BRN_RACEBAL_DIAG one-shot, the first frame the
#                                               clock runs (mbInRace && !mbOnStartLine)
# Race junction 480886 (the FxGs2Tailing start), driven by tests/run_rival_organic.py's pursuit.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxNanpolGuardsLive.ps1 --run-name fxnanpol_guards
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxnanpol_guards'
$case.Area = 'ai'
$case.Bug = 'The AI race clock and the event-117 gate dereference the player driver/car without host null tests, as the console does, and never fault.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 90
$case.DiagEnv += ',BRN_RACEBAL_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'AIModule::Update gated body ran (row 12 GetAICar, row 26 UpdateCarRoutes)'; Pattern = '\[resetpump\] AIModule::Update: mbPlayerDataSet is SET'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the race clock ran with its unguarded player-car dereference (0x8279B698)'; Pattern = '\[racebal\] race clock running'; Expect = $true }
)
$case
