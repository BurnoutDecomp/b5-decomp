$laCase = & (Join-Path $PSScriptRoot 'FxDirectorStuntJumpLive.ps1')
$laCase.Name = 'playtest_cinematic_bars'
$laCase.Bug = 'Original signature-jump camera publishes and visibly draws its 15-percent letterbox bars, then removes them.'
$laCase.ProfileFixture = 'rival_hunt_profile.sav'
$laCase.BarFrameScript = Join-Path $PSScriptRoot 'playtest_cinematic_bars_frames.py'
$laCase.Frames = $true
$laCase.Run.MaxSeconds = 90
$laCase.Run.Remove('CrashSweep')
$laCase.Run.Remove('CrashSweepShots')
$laCase.Run.Remove('CrashSweepSettle')
# Use the known road run-up and allow the destination to stream after placement,
# then drive the ramp normally. The previous immediate far sweep seated128m high
# and never entered the jump trigger, so it cannot count as a cinematic sample.
$laCase.Run.Teleport = '3013.2,-9.0,-839.3,1.7'
$laCase.Run.ThrottleScript = '0:accel,1:none,6:accel'
$laCase.Checks = @($laCase.Checks | Where-Object { $_.Pattern -ne '\[sweep\] shot 0/' })
$laCase.Run.FrameEvery = 3
$laCase.DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_BLACKBARS_DIAG=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=360'
$laCase.Checks += @(
    @{Kind='LogMatch'; Name='valid cinematic camera publishes original bar amount'; Pattern='\[blackbars\] director valid=1 .*authored=0\.150000 published=0\.150000'}
    @{Kind='Script'; Name='letterbox appears on captured world and leaves after the jump'; Script={
        param($ctx)
        $lsPy = $ctx.Case.BarFrameScript
        $laOut = & py -3 $lsPy (Join-Path $ctx.RunDir 'frames') 2>&1
        try { $lrResult = ("$laOut" | ConvertFrom-Json) }
        catch { return @{Pass=$false; Detail="frame measurement failed: $laOut"} }
        @{Pass=($lrResult.bar_frames -ge 2 -and $lrResult.uncovered_after -ge 1); Detail="bars on $($lrResult.bar_frames) captured frames; visible world after bars on $($lrResult.uncovered_after) frames; first=$($lrResult.first_bar), last=$($lrResult.last_bar)"}
    }}
)
$laCase
