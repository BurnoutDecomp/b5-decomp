# The prior freeburn run through this ramp/landing route selected type2 and
# requested the native 0.285714 impact time scale (cinematic-bars-staged).
# Use the independently measured, streamed shot1 approach. The far staging
# placement is not a physics sample; all positive camera checks start at shot1.
$case = & (Join-Path $PSScriptRoot 'PlaytestCrashGyroClockLive.ps1')
$case.Name = 'playtest_crash_gyro_freeburn'
$case.Bug = 'Observe original tumbling/trucking on the slowed world clock during an airborne freeburn crash.'
$case.Run.MaxSeconds = 120
$case.Run.CrashSweep = '3013.2,-9.0,-839.3'
$case.Run.CrashSweepShots = '1.7:0,3026.55/-8.9/-294.9/1.45:55.8'
$case.Run.CrashSweepSettle = 900
$case.Run.Remove('CrashSweepArm')
$case.Run.ThrottleScript = '0:accel'
$case.DiagEnv = 'BRN_CAMERA_RIG_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_DIRECTOR_TRACE=1,BRN_SWEEP_WAIT_ROAMING=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=480'
$case.Checks = @(
    @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
    @{Kind='LogMatch';Name='streamed jump approach fired';Pattern='\[sweep\] shot 1/2'}
    @{Kind='LogCount';Name='actual approach is not a bad seat';Pattern='SEAT BAD shot 1';Max=0}
    @{Kind='LogMatch';Name='actual approach has a positive seat witness';Pattern='\[sweep\] seat ok shot 1'}
    @{Kind='Script';Name='Director remains in original offline freeburn event type';Script={
        param($ctx)
        $rows=0; $other=0
        foreach($line in $ctx.LogLines) {
            if($line -match '^\[director\] .* evtype (?<event>-?\d+) ') {
                $rows++
                if([int]$Matches.event -ne -1) {$other++}
            }
        }
        @{Pass=($rows -ge 2 -and $other -eq 0);Detail="Director event-type rows=$rows, values other than native freeburn -1=$other"}
    }}
    @{Kind='Script';Name='real crash and native tumbling occur after the valid approach';Script={
        param($ctx)
        $after=$false; $crashes=0; $tumbling=0
        foreach($line in $ctx.LogLines) {
            if($line -match '\[sweep\] seat ok shot 1') {$after=$true}
            if(!$after) {continue}
            if($line -match '\[crash-exit\] OPENED crash record') {$crashes++}
            if($line -match '\[crashcam\] crash camera: moment type=2 valid=1') {$tumbling++}
        }
        @{Pass=($crashes -ge 1 -and $tumbling -ge 1);Detail="after the measured approach: real crashes=$crashes, native valid type2 selections=$tumbling"}
    }}
    @{Kind='Script';Name='slowed truck or plant retains original tumble speed and finite framing';Script={
        param($ctx)
        $after=$false; $rows=0; $slow=0; $bad=0; $maxDist=0.0
        $peakSpeed=0.0; $firstDist=$null; $lastDist=$null
        $inv=[cultureinfo]::InvariantCulture
        foreach($line in $ctx.LogLines) {
            if($line -match '\[sweep\] seat ok shot 1') {$after=$true}
            if(!$after) {continue}
            if($line -notmatch '^\[camera-rig\] gyro car=-?\d+ eye=(?<eye>[^ ]+) target=(?<target>[^ ]+) fov=(?<fov>[^ ]+) dt=(?<dt>[^ ]+) car=(?<car>[^ ]+) worldDt=(?<world>[^ ]+) noSlomoDt=(?<normal>[^ ]+) truck=(?<truck>[01]) planted=(?<planted>[01]) speedSq=(?<speed>[^ ]+)') {continue}
            $eye=@($Matches.eye.Split(',')|ForEach-Object {[double]::Parse($_,$inv)})
            $car=@($Matches.car.Split(',')|ForEach-Object {[double]::Parse($_,$inv)})
            $world=[double]::Parse($Matches.world,$inv); $normal=[double]::Parse($Matches.normal,$inv)
            $speed=[double]::Parse($Matches.speed,$inv); $fov=[double]::Parse($Matches.fov,$inv)
            $rows++
            foreach($v in @($eye)+@($car)+@($world,$normal,$speed,$fov)) {
                if([double]::IsNaN($v)-or[double]::IsInfinity($v)) {$bad++}
            }
            if($world -le 0 -or $world -ge $normal -or ($Matches.truck -ne '1' -and $Matches.planted -ne '1') -or $speed -le 200.0) {continue}
            $slow++; $peakSpeed=[math]::Max($peakSpeed,$speed)
            $dist=[math]::Sqrt([math]::Pow($eye[0]-$car[0],2)+[math]::Pow($eye[1]-$car[1],2)+[math]::Pow($eye[2]-$car[2],2))
            if($null -eq $firstDist) {$firstDist=$dist}
            $lastDist=$dist; $maxDist=[math]::Max($maxDist,$dist)
        }
        @{Pass=($rows -ge 2 -and $slow -ge 1 -and $bad -eq 0);Detail="after measured approach: gyro rows=$rows, slowed truck/plant above200=$slow, nonfinite=$bad, peak speedSq=$peakSpeed; slowed eye-to-car first=$firstDist last=$lastDist max=$maxDist m; pixels require separate review"}
    }}
    @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
    @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
)
$case
