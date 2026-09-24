# FX-RCEM4 (crash parity 2026-09-24, G61-D4) -- the start-line boost flame. One Road Rage run
# (RivalOrganic: every car waits on the start line through the intro) with BRN_START_LINE_BOOST_DIAG
# set: ActiveRaceCar::Update's start-line block (ARTIST 0x822F7C58..0x822F7D14) must flip
# mbIsDoingStartLineBoost on and off, re-armed from the module's RNG to t * 0.85 + 0.25 s (so every
# printed countdown lies in [0.25 - dt, 1.1 - dt]).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4StartLineBoostLive.ps1 --run-name fxrcem4_start_line_boost
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_start_line_boost'
$case.Bug = 'The grid cars never pulsed their boost flames: ActiveRaceCar::Update had no start-line block and the module had no RNG.'
$case.DiagEnv += ',BRN_START_LINE_BOOST_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'a grid flame flips on'; Pattern = '\[start-line-boost\] slot \d+ flame 1 next'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'and flips off again'; Pattern = '\[start-line-boost\] slot \d+ flame 0 next'; Expect = $true }
)
$case
