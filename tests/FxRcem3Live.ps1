# FX-RCEM3 (crash parity 2026-09-23): one live Stunt Run (tools/tests/cases/stunt_run_lifecycle.ps1 --
# junkyard boot, drive to stunt junction 480897, start event 558269) over the world-side arms this
# lane landed, with the BRN_RCEM_ACTION_DIAG dispatch witness armed:
#   * G68-D11 case 73 (the junkyard transition-in: RemoveRivals + car-select flags) -- every boot
#     passes CarSelectManager's Start/EndTransitionIn, so `[rcem-action] id 73` must appear.
#   * G68-D11 case 170 / HandleSetBoost -- StuntAttackMode::PreWorldUpdate posts {player, 2, 1.0f}
#     at the event start; the `[rcem-action] 170 SetBoost -> player bar A / M` line must show A == M.
#   * G68-D9 case 34 -- the Stunt Run sets KU_FLAG_DONUT_START, so START_PLAYING_MODE re-places the
#     player at 15 mph: `[rcem-action] 34 donut start` (and the `[teleport] ... RE-RESET` line of the
#     placement, whose velocity is 6.7056 m/s).
#   * G67-D6 / G60-D1 / G60-D2 / G61-D5 / G62-D1 / G62-D2 / G68-D1 / G68-D2 -- no-regression only on
#     this path (their arms are unit-tested); the run must stay assert- and exception-free.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxRcem3Live.ps1 -Label fxrcem3_live
$case = & (Join-Path $PSScriptRoot '..\..\tools\tests\cases\stunt_run_lifecycle.ps1')
$case.Name = 'fxrcem3_live'
$case.Bug = 'The FX-RCEM3 HandleGameActions arms (73, 170, 34-donut) must be dispatched with no new asserts.'
$case.DiagEnv += ',BRN_RCEM_ACTION_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'event started (action 23 -> gui 93)'; Cue = 'e-start' }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 73 (junkyard transition-in) dispatched'; Pattern = '\[rcem-action\] id 73 '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G68-D11 case 170 (SetBoost) dispatched'; Pattern = '\[rcem-action\] 170 SetBoost -> player bar'; Expect = $true }
    @{ Kind = 'Script'; Name = 'G68-D11 170: the Stunt Run starts with a FULL bar'; Script = {
        param($ctx)
        $rows = @($ctx.LogLines | Where-Object { $_ -match '\[rcem-action\] 170 SetBoost -> player bar ([-\d.eE+]+) / ([-\d.eE+]+)' } |
                  ForEach-Object { if ($_ -match 'bar ([-\d.eE+]+) / ([-\d.eE+]+)') { ,@([double]$Matches[1], [double]$Matches[2]) } })
        if ($rows.Count -eq 0) { return @{ Pass = $false; Detail = 'no 170 line' } }
        $full = @($rows | Where-Object { $_[1] -gt 0 -and [math]::Abs($_[0] - $_[1]) -lt 1e-3 })
        @{ Pass = ($full.Count -gt 0); Detail = "170 bar samples: $(($rows | ForEach-Object { "$($_[0])/$($_[1])" }) -join ', ')" }
    } }
    @{ Kind = 'LogMatch'; Name = 'G68-D9 case 34 donut-start placement dispatched'; Pattern = '\[rcem-action\] 34 donut start -> player RequestPlaceOnTrack'; Expect = $true }
)
$case
