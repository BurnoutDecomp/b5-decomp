# ARTIST producer82493A88 and consumer82578510: pause/resume suspension
# events must survive GUI unwrapping without an Invalid suspension type assert.
# Retain the existing pause camera, paused-byte and first-resume checks.
$case = & (Join-Path $PSScriptRoot 'MenusPauseSimPausedLive.ps1')
$case.Name = 'fxgui_suspension'
$case.Bug = 'GUI suspension events must use wrapper channel40 and be read from their unwrapped four-byte payload.'
$case.Checks += @(
    @{ Kind='LogCount'; Name='no invalid suspension type assertion'; Pattern='Invalid suspension type'; Max=0 }
    @{ Kind='LogCount'; Name='zero assertions throughout pause and resume'; Pattern='\[ASSERT \d+\]'; Max=0 }
)
$case
