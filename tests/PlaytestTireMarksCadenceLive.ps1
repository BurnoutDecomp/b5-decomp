# Turn/handbrake through the real wheel producer, then hold the original strip
# until it fades. Native readbacks measure actual trail-only road pixels.
$case = & (Join-Path $PSScriptRoot 'PlaytestRenderTrailPixelsLive.ps1')
$case.Name = 'tire_marks_cadence_1009'
$case.Bug = 'Uncapped zero-step command frames must preserve drawable tyre marks and the original ten-second fade.'
$case.Run.MaxSeconds = 65
$case.Run.FrameEvery = 120
$case.DiagEnv += ',BRN_TRAIL_CADENCE=1,BRN_FRAME_DUMP_MAX=24'
$case.Checks += @(
    @{ Kind='LogMatch'; Name='the wheel strip receives multiple original segments'; Pattern='\[trailseg\].*APPEND n=[2-9]\d* ' }
    @{ Kind='LogCount'; Name='zero-step presentations do not time out consecutive wheel segments'; Pattern='\[trailseg\].*NEWEMIT prev=1 why=timeout,.*dt=0\.00000'; Max=0 }
    @{ Kind='LogCount'; Name='the emitter pool does not exhaust'; Pattern='\[trailseg\].*NEWEMIT .*new=0+ .*pool=0'; Max=0 }
)
$case
