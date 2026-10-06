# A proven real wall stimulus, without the deformation fixture's pause interruption.
# Flow checks measure the original rig clocks/poses; screenshots still require framing review.
@{
    Name='playtest_crash_gyro_clock'
    Area='director'
    Bug='Crash trucking must use the slowed world clock; retain actual vehicle/camera geometry for comparison.'
    Frames=$true
    ProfileFixture='rival_hunt_profile.sav'
    Run=@{
        SkipIntro=$true; AcceptGap=1.0; MaxSeconds=75
        Drive=$true; MotionProbe=$true
        CrashSweep='3249.796,-3.7,-1925.404'
        CrashSweepShots='225:70'; CrashSweepArm=4
        FrameEvery=3; MaxLogMB=64
    }
    DiagEnv='BRN_CAMERA_RIG_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_DIRECTOR_TRACE=1,BRN_BLACKBARS_DIAG=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=360'
    Checks=@(
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='LogMatch';Name='real wall stimulus fired';Pattern='\[sweep\] shot 0/'}
        @{Kind='LogCount';Name='wall approach was not a bad seat';Pattern='\[sweep\].*SEAT BAD';Max=0}
        @{Kind='LogMatch';Name='real crash record opened';Pattern='\[crash-exit\] OPENED crash record'}
        @{Kind='Script';Name='actual gyro trucking is sampled in slow motion with finite subject/eye poses';Script={
            param($ctx)
            $inv=[cultureinfo]::InvariantCulture
            $rows=0; $slowRows=0; $bad=0; $maxDist=0.0; $firstDist=$null; $lastDist=$null
            foreach($line in $ctx.LogLines) {
                if($line -notmatch '^\[camera-rig\] gyro car=-?\d+ eye=(?<eye>[^ ]+) target=(?<target>[^ ]+) fov=(?<fov>[^ ]+) dt=(?<dt>[^ ]+) car=(?<car>[^ ]+) worldDt=(?<world>[^ ]+) noSlomoDt=(?<normal>[^ ]+) truck=(?<truck>[01]) planted=(?<planted>[01])') {continue}
                $eye=@($Matches.eye.Split(',')|ForEach-Object {[double]::Parse($_,$inv)})
                $car=@($Matches.car.Split(',')|ForEach-Object {[double]::Parse($_,$inv)})
                $world=[double]::Parse($Matches.world,$inv);$normal=[double]::Parse($Matches.normal,$inv)
                $fov=[double]::Parse($Matches.fov,$inv);$rows++
                foreach($v in @($eye)+@($car)+@($world,$normal,$fov)) {if([double]::IsNaN($v)-or[double]::IsInfinity($v)) {$bad++}}
                if($world -gt 0 -and $world -lt $normal -and ($Matches.truck -eq '1' -or $Matches.planted -eq '1')) {$slowRows++}
                $dist=[math]::Sqrt([math]::Pow($eye[0]-$car[0],2)+[math]::Pow($eye[1]-$car[1],2)+[math]::Pow($eye[2]-$car[2],2))
                if($null -eq $firstDist) {$firstDist=$dist};$lastDist=$dist;$maxDist=[math]::Max($maxDist,$dist)
            }
            @{Pass=($rows -ge 2 -and $slowRows -ge 1 -and $bad -eq 0)
              Detail="gyro rows=$rows, slowed truck rows=$slowRows, nonfinite=$bad, eye-to-car first=$firstDist last=$lastDist max=$maxDist m; visual framing remains a separate review"}
        }}
        @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
    )
}
