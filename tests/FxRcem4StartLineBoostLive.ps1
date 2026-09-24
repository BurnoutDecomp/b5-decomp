# FX-RCEM4 (crash parity 2026-09-24, G61-D4) -- the start-line boost flame. One RACE run (junction
# 480886, the FxNanpolGuardsLive start: a standing start, `[ai-attach] ... playerRolls 0`, so
# SetupOpponents puts every car ON_START_LINE; the RivalOrganic Road Rage default is a rolling start,
# playerRolls 1, and never enters the block) with BRN_START_LINE_BOOST_DIAG set:
# ActiveRaceCar::Update's start-line block (ARTIST 0x822F7C58..0x822F7D14) must flip
# mbIsDoingStartLineBoost on and off, re-armed from the module's RNG to t * 0.85 + 0.25 s (so every
# printed countdown lies in [0.25 - dt, 1.1 - dt]).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4StartLineBoostLive.ps1 --run-name fxrcem4_start_line_boost
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_start_line_boost'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Bug = 'The grid cars never pulsed their boost flames: ActiveRaceCar::Update had no start-line block and the module had no RNG.'
$case.DiagEnv += ',BRN_START_LINE_BOOST_DIAG=1'
$case.Run.MaxSeconds = 90
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'a grid flame flips on'; Pattern = '\[start-line-boost\] slot \d+ flame 1 next'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'and flips off again'; Pattern = '\[start-line-boost\] slot \d+ flame 0 next'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'a standing start'; Pattern = '\[ai-attach\] mode armed: .* playerRolls 0'; Expect = $true }
)
$case
