# FX-RCEM4 (crash parity 2026-09-24, CHAIN-STOMPEES c) -- live witness for
# RaceCarEntityModule::ProcessLeapedAndStompedCars @0x822BD5B8, the consumer of the stompee list
# FX-TRAFFIC2 publishes (FxTraffic2StompeesLive.ps1). ShowtimeContacts drives into Showtime;
# BRN_TRAFFIC_DIAG arms the producer's [T5-stomp] line and BRN_STOMP_DIAG the consumer's capped
# [stomp] line, printed only when the leg ran (Showtime + player airborne) and stored a non-empty
# list. Before the fix the function had no body and PostSceneUpdate skipped its slot, so the line
# could not appear and miStoredStompeeCount stayed 0.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxRcem4StompeesLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxrcem4_stompees'
$case.Area = 'physics'
$case.Bug = 'In Showtime, while the player car is airborne, RaceCarEntityModule must store the traffic module''s potential stompees for the leap target assist (CHAIN-STOMPEES c).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_STOMP_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES the producer published a stompee'
       Pattern = '\[T5-stomp\] .* stompees=[1-8] '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES c the module stored the stompees (Showtime + airborne)'
       Pattern = '\[stomp\] ProcessLeapedAndStompedCars stored [1-8] stompee'; Expect = $true }
)
$case
