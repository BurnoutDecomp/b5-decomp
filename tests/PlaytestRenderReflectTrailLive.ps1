# Original pad movement then a held chase view: one bounded capture for moving
# reflections/blur and the laid trail's subsequent original age/fade.
$case = & (Join-Path $PSScriptRoot 'PlaytestRenderSkidHoldLive.ps1')
$case.Name = 'playtest_render_reflect_trail_1005'
$case.Bug = 'Inspect actual moving reflections/blur, then a grounded world strip during a held camera and original fade.'
# Lock the rear wheels while braking, avoiding a prolonged brake-to-reverse input.
$case.Run.ThrottleScript = '0:accel,4:accel+handbrake,5.5:brake+handbrake,8:handbrake'
$case.DiagEnv += ',BRN_FRAME_DUMP_START=5000,BRN_POSTFX_SOURCE_DUMP=1,BRN_POSTFX_SOURCE_START=5000,BRN_POSTFX_SOURCE_EVERY=30'
$case.Checks += @(
    @{ Kind='LogMatch'; Name='native source sequence actually written'; Pattern='\[postfx-source\].*unit=0 .*written=1' }
)
$case
