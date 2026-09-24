# FX-RCEM4 (crash parity 2026-09-24, reviewer C) -- live witness for UpdateStreaming's readiness
# publish, 0x822FF204..0x822FF210: every update writes lbAllReady into the active-race-car output
# interface (GetActiveRaceCarOutputInterface()->SetAllActiveCarsReady, +0x2861), the flag
# HUDMessageLogic::GenerateLeaderMessages @0x82394110 waits on before the race-leader HUD messages.
# BRN_CARS_READY_DIAG arms a capped [cars-ready] line per change of the published value. Before the fix
# nothing wrote the member (it read 0 for ever) and the line could not appear. A Road Rage start
# (RivalOrganic) loads the player and the rivals, so the value must reach 1.
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxRcem4CarsReadyLive.ps1 --run-name fxrcem4_cars_ready
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxrcem4_cars_ready'
$case.Bug = 'RaceCarEntityModule::UpdateStreaming must publish the all-active-cars-ready flag to its output every update (0x822FF210).'
$case.DiagEnv += ',BRN_CARS_READY_DIAG=1'
$case.Run.MaxSeconds = 150
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'the publish ran and reached 1 (every active car ready)'
       Pattern = '\[cars-ready\] UpdateStreaming SetAllActiveCarsReady\(1\)'; Expect = $true }
    @{ Kind = 'Script'; Name = 'published transitions (report)'; Script = {
        param($ctx)
        $l = @($ctx.LogLines | Where-Object { $_ -match '\[cars-ready\]' })
        @{ Pass = $true; Detail = "$($l.Count) change(s); " + (($l | Select-Object -First 6 | ForEach-Object { ($_ -replace '.*SetAllActiveCarsReady', '').Trim() }) -join ' | ') }
    } }
)
$case
