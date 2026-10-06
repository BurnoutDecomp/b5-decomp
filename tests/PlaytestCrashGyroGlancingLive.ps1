# Real glancing wall trials. The original tumble speed gate is |velocity|^2>200.
# Success requires both a selected native tumbling moment and slowed trucking poses.
$case = & (Join-Path $PSScriptRoot 'PlaytestCrashGyroClockLive.ps1')
$case.Name = 'playtest_crash_gyro_glancing'
$case.Bug = 'Observe original crash trucking during a real glancing collision; preserve native geometry and clocks.'
$case.Run.MaxSeconds = 110
$case.Run.CrashSweepShots = '220:53,230:70,240:70'
$case.Run.CrashSweepSettle = 600
$case.DiagEnv = 'BRN_CAMERA_RIG_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_DIRECTOR_TRACE=1,BRN_BLACKBARS_DIAG=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=540'
$case.Checks += @(
    @{Kind='LogMatch';Name='all three real wall approaches fired';Pattern='\[sweep\] shot 2/3'}
    @{Kind='LogMatch';Name='original tumbling moment selected valid';Pattern='\[crashcam\] crash camera: moment type=2 valid=1'}
    @{Kind='Script';Name='slowed trucking retains positive post-impact tumble speed';Script={
        param($ctx)
        $count=0; $peak=0.0; $inv=[cultureinfo]::InvariantCulture
        foreach($line in $ctx.LogLines) {
            if($line -notmatch '^\[camera-rig\] gyro .* worldDt=(?<world>[^ ]+) noSlomoDt=(?<normal>[^ ]+) truck=(?<truck>[01]) planted=(?<planted>[01]) speedSq=(?<speed>[^ ]+)') {continue}
            $world=[double]::Parse($Matches.world,$inv);$normal=[double]::Parse($Matches.normal,$inv)
            $speed=[double]::Parse($Matches.speed,$inv)
            if($world -gt 0 -and $world -lt $normal -and ($Matches.truck -eq '1' -or $Matches.planted -eq '1') -and $speed -gt 200.0) {$count++;$peak=[math]::Max($peak,$speed)}
        }
        @{Pass=($count -ge 1);Detail="slowed truck/plant rows above original200 speed-squared gate=$count; peak=$peak (m/s)^2"}
    }}
)
$case
