# FX-TRAFFIC3 item 3 (crash parity wave 5, 2026-09-24) -- live witness for VehicleManager::CrashFatalRaceCars
# @0x826361C0 reading KAB_SURFACE_IS_WATER with the RAW wheel surface id (0x826363C4 `lbzx`, no bound), after
# the host `liSurfaceId < KI_MAX_NUM_SURFACES` guard was removed. BRN_FATAL_SURFACE_DIAG prints, capped, one
# `[fatal-surface] car=C wheel=W id=I water=B onGround=G` line whenever a wheel's id changes, which proves the
# read was dispatched on real roads and shows the ids they produce (the static proof, D1: 0..19 on WORLDCOL.BIN,
# is tests/run_fxtraffic3_race_surface.py).
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxTraffic3RaceSurfaceLive.ps1 --run-name fxtraffic3_race_surface
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxtraffic3_race_surface'
$case.Area = 'crash'
$case.Bug = 'CrashFatalRaceCars must read the water table with the raw wheel surface id (no host bound) and the ids real roads produce must stay inside the table.'
$case.DiagEnv += ',BRN_FATAL_SURFACE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogCount'; Name = 'item 3 dispatched: CrashFatalRaceCars read a wheel surface id'
       Pattern = '\[fatal-surface\] car=\d+ wheel=\d id=\d+'; Min = 1 }
    @{ Kind = 'Script'; Name = 'every live wheel surface id is inside the 20 used surfaces (and the 32-entry table)'; Script = {
        param($ctx)
        $ids = @{}; $bad = @()
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[fatal-surface\] car=(\d+) wheel=(\d) id=(\d+) water=(\d)') {
                $id = [int]$Matches[3]; $ids[$id] += 1
                if ($id -ge 20) { $bad += $line }
            }
        }
        $seen = ($ids.Keys | Sort-Object | ForEach-Object { "$_" }) -join ','
        @{ Pass = ($ids.Count -gt 0 -and $bad.Count -eq 0); Detail = "ids seen: $seen; >= 20: $($bad.Count) $(($bad | Select-Object -First 1))" }
    } }
)
$case
