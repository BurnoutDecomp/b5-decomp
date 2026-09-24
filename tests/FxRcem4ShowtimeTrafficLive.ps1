# FX-RCEM4 (crash parity 2026-09-24) -- live witness for RaceCarEntityModule::PostSceneUpdate's Showtime
# traffic publish (0x822FE4BC..0x822FE554): every frame the race-car -> traffic interface gets
# E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND = CrashPlayManager::IsPlayerInShowtimeOnGround() and the
# Showtime traffic density scale = CrashPlayManager::GetShowtimeTrafficDensityScale(); the traffic module
# latches both in its PostSceneUpdate (+464870 / +464932). ShowtimeContacts drives into Showtime;
# BRN_SHOWTIME_TRAFFIC_DIAG arms the capped [showtime-traffic] line (one per change of the published
# pair) and BRN_TRAFFIC_DIAG the traffic side's [T1-showtime] top-up line, which prints the scale the
# traffic module actually latched. Before the fix neither was ever written: the flag stayed 0 and the
# traffic scale sat at RaceCarToTrafficInterface::Construct's 1.0.
#   powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/FxRcem4ShowtimeTrafficLive.ps1
$case = & (Join-Path $PSScriptRoot 'ShowtimeContacts.ps1')
$case.Name = 'fxrcem4_showtime_traffic'
$case.Area = 'traffic'
$case.Bug = 'RaceCarEntityModule::PostSceneUpdate must publish the Showtime on-ground flag and traffic density scale to the traffic module every frame (0x822FE4BC..0x822FE554).'
$case.DiagEnv += ',BRN_TRAFFIC_DIAG=1,BRN_SHOWTIME_TRAFFIC_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogMatch'; Name = 'the publish ran in Showtime'
       Pattern = '\[showtime-traffic\] PostSceneUpdate publish onGround [01] densityScale [0-9.]+ \(in showtime 1\)'; Expect = $true }
    @{ Kind = 'Script'; Name = 'the traffic module latched the published scale (report)'; Script = {
        param($ctx)
        $pub = @($ctx.LogLines | Where-Object { $_ -match '\[showtime-traffic\]' })
        $top = @($ctx.LogLines | Where-Object { $_ -match '\[T1-showtime\] top-up .* densityScale=' })
        $ground = @($pub | Where-Object { $_ -match 'onGround 1' })
        @{ Pass = $true; Detail = "$($pub.Count) publish change(s) ($($ground.Count) on-ground), $($top.Count) traffic top-up(s); " +
            (($pub | Select-Object -First 3 | ForEach-Object { ($_ -replace '.*publish ', '').Trim() }) -join ' | ') +
            $(if ($top.Count) { ' || traffic: ' + (($top | Select-Object -Last 1) -replace '.*(densityScale=\S+).*', '$1') } else { '' }) }
    } }
)
$case
