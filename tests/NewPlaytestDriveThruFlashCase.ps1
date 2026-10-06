param([ValidateSet('gas_station','body_shop')][string]$Shop)
$lWorkflowRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lCase=& (Join-Path $lWorkflowRoot "tools/tests/cases/drivethru_$Shop.ps1")
$lCase.Name="playtest_${Shop}_repair_flash"
$lCase.Area='director'
$lCase.Bug='Record the authored shop camera, handoff and white-flash chronology; a log pass alone does not prove visual timing.'
$lCase.ProfileFixture='rival_hunt_profile.sav'
$lCase.Frames=$true
$lCase.Run.MaxSeconds=70
$lCase.Run.FrameEvery=3
$lCase.Run.MaxLogMB=64
$lCase.DiagEnv+=',BRN_CAMERA_TRACE=1,BRN_CAMERA_RIG_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_PFX_DIAG=1,BRN_BLACKBARS_DIAG=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=480'
$lCase.Checks+=@(
    @{Kind='Script';Name='camera samples and director/GUI flash order are recorded';Script={
        param($ctx)
        $first=-1;$last=-1;$request=-1;$hook=-1;$samples=0;$bad=0
        $sim=-1;$firstSim=-1;$lastSim=-1;$requestSim=-1;$hookSim=-1
        for($i=0;$i-lt$ctx.LogLines.Count;$i++) {
            $line=$ctx.LogLines[$i]
            if($line-match '^\[cam\] f=(\d+)') {$sim=[int]$Matches[1]}
            if($line-match '^\[dt-cam\] n=\d+ cam=\((?<cam>[^)]+)\) car=\((?<car>[^)]+)\).*dist=(?<dist>[^ ]+) fov=(?<fov>[^ ]+)') {
                if($first-lt0) {$first=$i;$firstSim=$sim};$last=$i;$lastSim=$sim;$samples++
                foreach($v in @($Matches.cam.Split(','))+@($Matches.car.Split(','))+@($Matches.dist,$Matches.fov)) {
                    $n=[double]::Parse($v,[cultureinfo]::InvariantCulture)
                    if([double]::IsNaN($n)-or[double]::IsInfinity($n)) {$bad++}
                }
            }
            if($request-lt0 -and $line-match "\[pfx-dir\] (EnsureEffectIsPlaying requests|bridge posts 495 start) 'Car_Reset'") {$request=$i;$requestSim=$sim}
            if($hook-lt0 -and $line-match "\[pfx\] StartHook 'Car_Reset'") {$hook=$i;$hookSim=$sim}
        }
        @{Pass=($samples-ge2 -and $bad-eq0 -and $request-gt$first -and $hook-ge$request)
          Detail="shop samples=$samples, nonfinite=$bad; log rows first=$first last=$last request=$request hook=$hook; nearest camera frames first=$firstSim last=$lastSim request=$requestSim hook=$hookSim. Compare the saved white-flash/cut pixels before judging parity."}
    }}
    @{Kind='LogCount';Name='no assertions';Pattern='\[ASSERT(?:\s|\])';Max=0}
)
return $lCase
