# FX-RCEM (crash-parity 2026-09-22): the Showtime arm of HandlePrepareForModeAction latches the
# player car's mbIsWrecked (+0x782, `stb r18, 0x782` @0x823099D0), and ActiveRaceCar::mbIsInShowtime
# (+0x788) is armed only by action 143 (HandleGameActions @0x8230D724).
# Same live Showtime entry as ShowtimeContacts.ps1; BRN_WRECK_LATCH_DIAG=1 arms the [wrecklatch]
# witness that names the console address of every store to the latch.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RcemShowtimeWreckLatch.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'rcem_showtime_wreck_latch'
$case.Bug = 'Entering Showtime must latch the player car wrecked (+0x782); +0x788 is armed by action 143.'
$case.DiagEnv += ',BRN_WRECK_LATCH_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'showtime prepare latches the player car wrecked';
       Pattern = '\[wrecklatch\] car=\d+ <- 1 site=HandlePrepareForModeAction\.showtime@0x823099D0'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'action 143 still arms ActiveRaceCar::mbIsInShowtime';
       Pattern = '\[showtime-switch\] ActiveRaceCar \d+ mbIsInShowtime <- 1'; Expect = $true }
)
$case
