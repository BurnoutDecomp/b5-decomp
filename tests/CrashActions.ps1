$case = & (Join-Path $PSScriptRoot 'PartMotion.ps1')
$case.Name = 'crash_actions'
$case.Bug = 'The real game-action queue must configure crash policy and pause cleanup during a rival takedown camera.'
$case.DiagEnv += ',BRN_CRASH_ACTION_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'mode preparation reaches crash module'
       Pattern = '\[crash-action\] id=23 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'takedown camera pauses crash cleanup'
       Pattern = '\[crash-action\] id=6 cleanup=0 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'camera exit resumes crash cleanup'
       Pattern = '\[crash-action\] id=6 cleanup=1 '; Min = 1 }
    @{ Kind = 'LogCount'; Name = 'old parked action path removed'
       Pattern = 'HandleGameActions PARK'; Max = 0 }
)
$case
