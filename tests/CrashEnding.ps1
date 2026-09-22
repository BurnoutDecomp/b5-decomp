$case = & (Join-Path $PSScriptRoot 'CrashActions.ps1')
$case.Name = 'crash_ending'
$case.Bug = 'The player crash-ending signal must leave the crash module and be relayed by game state as the original action17.'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'crash-ending event published'
       Pattern = '\[crash-ending\] event 42 posted'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'crash-ending event reaches game-state relay'
       Pattern = '\[crash-ending\] event 42 relayed as action 17'; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'old dropped crash-ending signal removed'
       Pattern = "crash ending.*PARK"; Max = 0 }
)
$case
