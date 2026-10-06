# Actual both-bumper Showtime entry and traffic-score chain, with bounded frames
# for visible world cash labels and their banked-score animation.
$case = & (Join-Path $PSScriptRoot 'FxShowtime2Live.ps1')
$case.Name = 'playtest_showtime_cash'
$case.Bug = 'Above-car cash and banked scores must draw through the real 3D/2D buffers during Showtime.'
$case.Remove('FreshProfile')
$case.ProfileFixture = 'rival_hunt_profile.sav'
$case.Frames = $true
$case.Run.FrameEvery = 120
$case.Run.MaxSeconds = 160
$case.DiagEnv += ',BRN_FRAME_DUMP_MAX=220'
$case.Checks += @(
    @{ Kind='LogCount'; Name='no assertions while cash renderer is mounted'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
)
$case
