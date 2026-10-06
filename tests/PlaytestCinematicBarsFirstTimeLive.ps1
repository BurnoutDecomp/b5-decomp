$laCase = & (Join-Path $PSScriptRoot 'PlaytestCinematicBarsLive.ps1')
$laCase.Name = 'playtest_cinematic_bars_first_time'
$laCase.ProfileFixture = (Resolve-Path (Join-Path $PSScriptRoot '../../scratch/PLAYTEST_1005/first-time-jump-profile.sav')).Path
# The fixture differs only in the completed-jump Set/count row, with the existing
# B5SV codec/checksum. Stage the nearby streamed district before the known jump
# approach; use the actual trigger and stunt moment, never force a camera cut.
$laCase.Run.Remove('Teleport')
$laCase.Run.CrashSweep = '3013.2,-9.0,-839.3'
$laCase.Run.CrashSweepShots = '1.7:0,3026.55/-8.9/-294.9/1.45:55.8'
$laCase.Run.CrashSweepSettle = 900
$laCase.Run.ThrottleScript = '0:accel'
$laCase.Checks += @(
    @{Kind='LogCount'; Name='staged signature-jump approach actually fired'; Pattern='\[sweep\] shot 1/2'; Min=1}
    # The far staging shot is not a physics sample. Only shot1 is the measured
    # approach; require its positive seat witness as well as no bad-seat row.
    @{Kind='LogCount'; Name='jump approach seat remains valid'; Pattern='SEAT BAD shot 1'; Max=0}
    @{Kind='LogMatch'; Name='actual jump approach is seated on the road'; Pattern='\[sweep\] seat ok shot 1'}
)
$laCase
