# FX-TRAFFIC (crash parity 2026-09-23, G58-X1) -- live witness for the Showtime sympathetic-crash
# cone. ShowtimeContacts drives into Showtime; BRN_TRAFFIC_DIAG arms the capped [T5-symp] line
# UpdateParams_TryStartSympatheticCrashing prints when a param latches a sympathetic-crash target,
# which now carries the cone's recip-Y lane. In Showtime that lane is +0x726F0's z, seeded from
# flt_820BA544 == 0.25 (Construct 0x82740658..0x82740680); before G58-X1 it was 0.0.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxTrafficShowtimeConeLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxtraffic_showtime_cone'
$case.Area = 'traffic'
$case.Bug = 'The Showtime sympathetic-crash cone must squash the height offset by 0.25 (+0x726F0 lane z), not zero it (G58-X1).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'G58-X1 a Showtime sympathetic crash latched with recipY 0.25'
       Pattern = '\[T5-symp\] .* showtime=1 recipY=0\.25'; Expect = $true }
    @{ Kind = 'LogCount'; Name = 'G58-X1 no Showtime cone test with a zero Y scale'
       Pattern = '\[T5-symp\] .* showtime=1 recipY=0 '; Max = 0 }
)
$case
