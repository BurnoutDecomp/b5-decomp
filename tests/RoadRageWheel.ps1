$case = & (Join-Path $PSScriptRoot 'TrafficCrashLifecycle.ps1')
$case.Name = 'road_rage_wheel'
$case.Bug = 'Road Rage crashes must execute the original one-front-wheel twist fallback.'
$case.DiagEnv += ',BRN_WHEEL_PROBE=1'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'Road Rage front-wheel fallback executes'; Pattern = '\[wheel-probe\] ROAD_RAGE_TWIST obj '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'old dropped wheel fallback removed'; Pattern = "showtime wheel-detach tail.*not reconstructed"; Max = 0 }
)
$case
