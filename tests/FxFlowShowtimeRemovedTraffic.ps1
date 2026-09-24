# FX-FLOW (crash parity 2026-09-24, G10-D9 caller) -- live witness for
# BrnGameModule::BridgeWorldTrafficAndPropDataToGui @0x823E5560. ShowtimeContacts drives into
# Showtime; BRN_TRAFFICGUI_DIAG arms the capped [traffic-gui] lines the bridge prints on its first
# forwards of each id and on every removed-traffic record that names a car (with the Showtime
# scorer's recent-crash count around CrashModeScoring::DealWithRemovedTraffic). Before the fix the
# body did not exist, so no world GUI record reached the GUI and the scorer never heard of a
# removed car.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxFlowShowtimeRemovedTraffic.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxflow_showtime_removed_traffic'
$case.Area = 'gamestate'
$case.Bug = 'The world output GUI queue must reach the GUI through BridgeWorldTrafficAndPropDataToGui, and each removed-traffic record (209) must reach CrashModeScoring::DealWithRemovedTraffic (G10-D9 caller).'
$case.DiagEnv += ',BRN_TRAFFICGUI_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'G10-D9 the score-target record (208) is forwarded'
       Pattern = '\[traffic-gui\] 208 score targets \d+ \(mode in progress [01]\) -> GUI'; Expect = $true }
    @{ Kind = 'LogMatch'; Name = 'G10-D9 the removed-traffic record (209) reaches the Showtime scorer'
       Pattern = '\[traffic-gui\] 209 removed traffic \d+ car\(s\) -> GUI \+ CrashModeScoring::DealWithRemovedTraffic'; Expect = $true }
)
$case
