# FX-TRAFFIC2 (crash parity 2026-09-24, CHAIN-STOMPEES a) -- live witness for
# TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput @0x8271F298. ShowtimeContacts
# drives into Showtime; BRN_TRAFFIC_DIAG arms the capped [T5-stomp] line the producer prints on its
# first Showtime frames and on every frame that published a stompee or a Showtime slot. Before the
# fix the body did not exist, so neither the line nor a single stompee could appear.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTraffic2StompeesLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxtraffic2_stompees'
$case.Area = 'traffic'
$case.Bug = 'In Showtime the traffic module must predict the landing point and publish the on-screen traffic cars inside the landing ring as potential stompees (CHAIN-STOMPEES a).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES the producer ran in Showtime'
       Pattern = '\[T5-stomp\] frame=\d+ '; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES a Showtime frame published at least one stompee'
       Pattern = '\[T5-stomp\] .* stompees=[1-8] '; Expect = $true }
    # 33317822: the score leg (GetVehicleScoreData -> AddPotentialScoree) -- the HUD's Showtime
    # score targets. The diag prints the potential-scoree count the frame ended with.
    @{ Kind = 'LogMatch'; Name = 'CHAIN-STOMPEES score leg published a potential scoree'
       Pattern = '\[T5-stomp\] .* scorees=([1-9]|1[0-9]|20) '; Expect = $true }
    # a8f25c60 (G58-D1): the sympathetic-crasher producer runs every Showtime frame.
    @{ Kind = 'LogMatch'; Name = 'G58-D1 the sympathetic-crasher producer ran in Showtime'
       Pattern = '\[T5-sympcrasher\] frame=\d+ crashers=\d+'; Expect = $true }
)
$case
