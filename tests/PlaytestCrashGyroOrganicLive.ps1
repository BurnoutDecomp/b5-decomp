# This actual AI-pad pursuit previously selected valid tumbling at 69.86 m/s
# (PLAYTEST_1005/render-band-consumer-batch16). It changes no camera parameter.
$case = & (Join-Path $PSScriptRoot 'PlaytestBoostMultiplierLive.ps1')
$gyro = & (Join-Path $PSScriptRoot 'PlaytestCrashGyroClockLive.ps1')
$glancing = & (Join-Path $PSScriptRoot 'PlaytestCrashGyroGlancingLive.ps1')
$case.Name = 'playtest_crash_gyro_organic'
$case.Area = 'director'
$case.Bug = 'Actual pursuit collisions must reach the original tumbling selector and plant/truck the camera on the slowed world clock.'
$case.Run.MaxSeconds = 120
$case.Run.FrameEvery = 3
$case.DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_CAMERA_RIG_DIAG=1,BRN_DIRECTOR_TRACE=1,BRN_PROP_DIAG=1,BRN_DEBUG_PLAYER_CAR=PUSCLT02,BRN_AI_PAD_PLAYER=pursuit,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=480'
$case.Checks = @(
    @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
    @{Kind='LogMatch';Name='original AI seat drives through pad input';Pattern='\[ai-pad\] \*\*\*\*\* HARNESS-ONLY \(BRN_AI_PAD_PLAYER=pursuit\): armed'}
    @{Kind='LogCount';Name='no injected takedown';Pattern='\[td\] HARNESS';Max=0}
    @{Kind='LogMatch';Name='real crash record opened';Pattern='\[crash-exit\] OPENED crash record'}
    @{Kind='LogMatch';Name='original tumbling moment selected valid';Pattern='\[crashcam\] crash camera: moment type=2 valid=1'}
    $gyro.Checks[4]
    $glancing.Checks[9]
    @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
    @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
)
$case
