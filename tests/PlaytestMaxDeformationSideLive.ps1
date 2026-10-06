# Same road/heading as the original maximum screenshot. Both sweep placements
# request zero speed through real PlaceOnTrack; second placement follows streaming.
# Short real camera-stick holds sample lateral views without forcing a camera.
$case=& (Join-Path $PSScriptRoot 'PlaytestMaxDeformationLive.ps1')
$case.Name='playtest_max_deformation_side'
$case.Bug='Compare maximum deformation at rest on the original grounded road and actual side camera.'
$case.Run.Remove('PauseAt')
$case.Run.Remove('PauseTarget')
$case.Run.Remove('UnpauseAt')
$case.Run.Drive=$true
$case.Run.DriveDelay=4.0
$case.Run.DriveSeconds=1.0
$case.Run.CrashSweep='3039.3,-3.8,-1995.3'
$case.Run.CrashSweepShots='5.277:0,5.277:0'
$case.Run.CrashSweepArm=0.1
$case.Run.CrashSweepSettle=900
$case.Run.CrashSweepMax=1200
$case.Run.MaxSeconds=90
$case.Run.FrameEvery=30
# This case hard-caps500 1280x720 frames (~1.8GB plus20 source pairs).
$case.Run.MinFreeGB=20
$steps=@('timeout:85','wait:\[sweep\] seat ok shot 1','hold:Brake:55','hold:HandBrake:55','wait:\[max-deform\] phase callback')
for($i=0;$i -lt 15;++$i){$steps+=@('hold:CameraLeft:0.3','sleep:0.6')}
$steps+=@('sleep:2','mark:side_views_done')
$case.Run.MenuScript=$steps -join ';'
$case.DiagEnv='BRN_PLAYTEST_MAX_DEFORM_AT=35,BRN_SWEEP_WAIT_ROAMING=1,BRN_WHEEL_SUS_PROBE=1,BRN_RESLOADED_DIAG=1,BRN_WHEEL_DIAG=1,BRN_CAM_INPUT_DIAG=1,BRN_POSTFX_SOURCE_DUMP=1,BRN_FRAME_DUMP_MAX=500'
$case.Checks=@($case.Checks | Where-Object {$_.Name -notin @('paused through original Driver Details','maximum-damage readback survives repeated paused frames')})
$case.Checks+=@(
    @{Kind='Script';Name='original road second placement seated before maximum';Script={
        param($ctx)
        $seat=-1;$callback=-1;$distance=[double]::PositiveInfinity
        for($i=0;$i -lt $ctx.LogLines.Count;++$i){
            $line=$ctx.LogLines[$i]
            if($line -match '^\[sweep\] seat ok shot 1 .* off (?<d>\S+) m'){$seat=$i;$distance=[double]::Parse($Matches.d,[cultureinfo]::InvariantCulture)}
            if($line -match '^\[max-deform\] phase callback'){$callback=$i;break}
        }
        @{Pass=($seat -ge 0 -and $callback -gt $seat -and $distance -lt 0.5);Detail="second seat line=$seat, callback line=$callback, actual placement offset=$distance m"}
    }}
    @{Kind='Script';Name='maximum applied to stationary grounded player on original road';Script={
        param($ctx)
        $sample=@($ctx.LogLines | Where-Object {$_ -match '^\[max-deform-pose\] phase callback '} | Select-Object -First 1)
        if($sample.Count -eq 0){return @{Pass=$false;Detail='missing fresh maximum-command pose; old suspension rows may precede freeze and cannot qualify current speed'}}
        $callback=@($ctx.LogLines | Where-Object {$_ -match '^\[max-deform\] phase callback '} | Select-Object -First 1)
        if($callback.Count -ne 1 -or $callback[0] -notmatch '^\[max-deform\] phase callback sample 0 present (?<p>\d+) rig (?<r>\d+) '){return @{Pass=$false;Detail='missing maximum callback frame/rig identity'}}
        $callbackPresent=$Matches.p;$callbackRig=$Matches.r
        if($sample[0] -notmatch '^\[max-deform-pose\] phase callback sample 0 present (?<present>\d+) rig (?<rig>\d+) pos (?<x>\S+) (?<y>\S+) (?<z>\S+) at .* linear (?<vx>\S+) (?<vy>\S+) (?<vz>\S+) angular (?<ax>\S+) (?<ay>\S+) (?<az>\S+) frozen (?<frozen>[01]) .* ground (?<g0>[01]) (?<g1>[01]) (?<g2>[01]) (?<g3>[01])$'){
            return @{Pass=$false;Detail='fresh maximum-pose format did not parse: '+$sample[0]}
        }
        if($Matches.present -ne $callbackPresent -or $Matches.rig -ne $callbackRig){return @{Pass=$false;Detail='pose frame/rig does not match the actual maximum callback'}}
        $x=[double]::Parse($Matches.x,[cultureinfo]::InvariantCulture);$z=[double]::Parse($Matches.z,[cultureinfo]::InvariantCulture)
        $speed2=0.0;$angular2=0.0
        foreach($key in @('vx','vy','vz')){$value=[double]::Parse($Matches[$key],[cultureinfo]::InvariantCulture);$speed2+=$value*$value}
        foreach($key in @('ax','ay','az')){$value=[double]::Parse($Matches[$key],[cultureinfo]::InvariantCulture);$angular2+=$value*$value}
        $ground=0;foreach($key in @('g0','g1','g2','g3')){if($Matches[$key] -eq '1'){++$ground}}
        @{Pass=($speed2 -lt 0.0001 -and $angular2 -lt 0.0001 -and $ground -eq 4 -and [math]::Abs($x-3039.3) -lt 0.5 -and [math]::Abs($z+1995.3) -lt 0.5);
          Detail="fresh present=$($Matches.present), speed=$([math]::Sqrt($speed2)) m/s, angular=$([math]::Sqrt($angular2)) rad/s, grounded=$ground/4, frozen=$($Matches.frozen), X/Z=$x/$z"}
    }}
    @{Kind='LogMatch';Name='actual right-stick camera channel delivered';Pattern='^\[harness-camera\] axes -1 0 held 1 '}
    @{Kind='LogMatch';Name='camera channel explicitly released';Pattern='^\[harness-camera\] axes 0 0 held 0 '}
    @{Kind='LogMatch';Name='actual gameplay camera accepts rotated input';Pattern='^\[cam-look\] GameplayExternal isLookback 0 isRotated 1 '}
    @{Kind='LogCount';Name='actual original-coordinate world sources saved';Pattern='^\[postfx-source\] .*unit=0 .*written=1 ';Min=1}
)
$case
