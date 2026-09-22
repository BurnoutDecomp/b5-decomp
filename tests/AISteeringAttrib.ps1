$case = & (Join-Path $PSScriptRoot 'RoadRageWheel.ps1')
$case.Name = 'ai_steering_attrib'
$case.Bug = 'Rival driver setup must read each car steering record before the live collision scenario.'
$case.DiagEnv += ',BRN_AI_ATTRIB_DIAG=1'
$case.Checks += @(
    @{ Kind = 'LogCount'; Name = 'driver steering attributes loaded'; Pattern = '\[ai-steer-attrib\] distance='; Min = 1 }
)
$case
