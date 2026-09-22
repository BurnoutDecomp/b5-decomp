# FX-RCEM (crash-parity 2026-09-22): the player's slot must publish Showtime to the AI.
# Same live Showtime entry as ShowtimeContacts.ps1 (BRN_SHOWTIME_WATCH=1 arms the witness).
# RaceCarEntityModule::WriteUpdatedAIData @0x822D1FC8 publishes (i == player) && module +0x1823D,
# and +0x1823D is mCrashPlayManager.mbIsInShowtime (+0x180F0 + 0x14D). Before the fix it read a
# phantom byte nothing wrote, so the [showtime-ai] edge 0 -> 1 could never appear.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RcemShowtimeAI.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'rcem_showtime_ai'
$case.Bug = 'In Showtime the player car must publish mbIsInShowtime to the AI (WriteUpdatedAIData).'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'player slot publishes showtime to the AI';
       Pattern = '\[showtime-ai\] player slot \d+ publishes showtime 0 -> 1'; Expect = $true }
)
$case
