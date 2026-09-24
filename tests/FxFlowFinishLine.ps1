# FX-FLOW (crash parity 2026-09-24, NEW-FINISHLINE): an offline race finish reaches the director's
# post-event camera with a real finish-line id, live.
# Same race as FxGs2Tailing.ps1 (junction 480886, -Teleport "3003.9,6.6,-1675.6,0" -StartEvent,
# driven by tests/run_rival_organic.py's pad pursuit), which measurably reaches FinishCurrentMode
# inside 120 s (scratch/bugtest/runs/fxgs2_tailing/20260923_135945: finish 6th, then ONE
# "Unknown finish line" assert, BrnDirectorResourceManager.cpp:325).
# BRN_FINISHLINE_DIAG=1 arms two witnesses:
#   [finish-line] action 24 -> ...        MainDirector::ProcessInputQueue case 24 @0x82238738 stored
#                                         the PrepareForMode broadcast (the ONLY writer of
#                                         GameState::mFinishLineID / mFinishLineNorthmostDir)
#   [finish-line] post-event shots: ...   ArbStatePostEvent::Prepare's GetEventCompletionShots lookup
#                                         ran with that id (the assert fires just before it on a miss)
#   python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/FxFlowFinishLine.ps1 --run-name fxflow_finish_line
$case = & (Join-Path $PSScriptRoot 'RivalOrganic.ps1')
$case.Name = 'fxflow_finish_line'
$case.Area = 'director'
$case.Bug = 'The director GameState.mFinishLineID must be written by ProcessInputQueue case 24 (the PrepareForMode finish-line broadcast), so the post-event camera finds the finish-line shot group instead of asserting "Unknown finish line" at every offline race finish.'
$case.Run.Teleport = '3003.9,6.6,-1675.6,0'
$case.Run.MaxSeconds = 120
$case.DiagEnv += ',BRN_MODEMGR_DIAG=1,BRN_FINISHLINE_DIAG=1'
$case.Checks = @(
    @{ Kind = 'NewAsserts'; Name = 'no new assertions' }
    @{ Kind = 'LogCount'; Name = 'no "Unknown finish line" assert'; Pattern = 'Unknown finish line'; Max = 0 }
    @{ Kind = 'LogCount'; Name = 'no exceptions'; Pattern = '\[EXCEPTION\]'; Max = 0 }
    @{ Kind = 'Mark'; Name = 'reached driving'; Phase = 'DRIVING' }
    @{ Kind = 'LogMatch'; Name = 'case 24 dispatched (the finish-line broadcast reached the director)'; Pattern = '\[finish-line\] action 24 -> GameState\.mFinishLineID [1-9]\d*'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'the race finished (FinishCurrentMode)'; Pattern = '\[evt-finish\] FinishCurrentMode ENTERED mode 0'; Expect = $true }
    @{ Kind = 'Script'; Name = 'the post-event lookup keyed on the broadcast id'; Script = {
        param($ctx)
        $stored = $null
        $looked = $null
        $lookedId = $null
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[finish-line\] action 24 -> GameState\.mFinishLineID (\d+)') { $stored = $Matches[1] }
            if ($line -match '\[finish-line\] post-event shots: mode (\-?\d+) finish line (\d+) -> (\d+) shot') { $looked = "mode $($Matches[1]) id $($Matches[2]) shots $($Matches[3])"; $lookedId = $Matches[2] }
        }
        $pass = ($null -ne $stored) -and ($null -ne $looked) -and ($lookedId -eq $stored)
        @{ Pass = $pass; Detail = "stored id: $stored; post-event lookup: $looked" }
    } }
)
$case
