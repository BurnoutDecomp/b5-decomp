# Reproduces the owner freeze after a junkyard car change. Slot profile is parked/restored.
# ARTIST ready/detach latch must release boost/crumple bundles before re-admission.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$case = & (Join-Path $PSScriptRoot 'JunkyardSwitchCameraLive.ps1')
$case.Name = 'fx_car_switch_audio'
$case.Area = 'audio'
$case.Bug = 'Changing cars must unload old sound banks before reloading them, without double relocation or an audio-thread crash.'
$case.ProfileFixture = 'scratch/OWNERLIST_0927/CODEX/FREEZE_0929/OwnerProfile.sav'
$case.Frames = $false
$case.Run.Remove('FrameEvery')
$case.Run.Drive = $true
$case.Run.MotionProbe = $true
$trace = 'swap_pcm_' + [Guid]::NewGuid().ToString('N') + '.txt'
$case.PcmTraceFile = Join-Path $root ('build/game/' + $trace)
$case.DiagEnv = 'BRN_CRASHCAM_DIAG=1,BRN_SOUND_PCM_TRACE=' + $trace
$case.Checks = @(
 @{Kind='Mark'; Name='reached selection'; Cue='carsel'}
 @{Kind='Script'; Name='real car round trip completed'; Script={
   param($ctx)
   $models=@()
   foreach($line in $ctx.LogLines) {
     if($line -match 'STRM: Adding racecar for streaming: car=0, model=([^ ,]+)') {
       if($models.Count -eq 0 -or $models[-1] -ne $Matches[1]) { $models += $Matches[1] }
     }
   }
   @{Pass=($models.Count -ge 3 -and $models[0] -eq $models[-1]);Detail=($models -join ' -> ')}
 }}
 @{Kind='Mark'; Name='returned to driving'; Phase='DRIVING'}
 @{Kind='Script'; Name='car continues moving after selection'; Script={
   param($ctx)
   $line=($ctx.MarksText -split "`n") | Where-Object {$_ -match '^DRIVE\s+'} | Select-Object -First 1
   $pass=($line -match 'path=(?<p>[\d.,]+)m') -and ([double]($Matches.p -replace ',','.') -gt 20)
   @{Pass=$pass;Detail=$line}
 }}
 @{Kind='LogCount'; Name='boost resource released during car change'; Pattern="UnloadBundle 'sound\\aems\\Boost_Patch_Bank.bundle'.*pool 6: 1 resources";Min=1}
 @{Kind='LogCount'; Name='crumple resource released during car change'; Pattern="UnloadBundle 'sound\\aems\\CRUMPLEPATCHBANK.BUNDLE'.*pool 6: 1 resources";Min=1}
 @{Kind='Script'; Name='both sound banks loaded for replacement car'; Script={
   param($ctx)
   $dest=Join-Path $ctx.RunDir 'swap_pcm_trace.txt'
   if(-not (Test-Path -LiteralPath $dest)) { Copy-Item -LiteralPath $ctx.Case.PcmTraceFile -Destination $dest }
   $lines=Get-Content -LiteralPath $dest
   $boost=@($lines | Where-Object {$_ -match '^aems-bank .*path=.*Boost_Patch_Bank\.abi'}).Count
   $crumple=@($lines | Where-Object {$_ -match '^aems-bank .*path=.*CrumplePatchBank\.abi'}).Count
   @{Pass=($boost -ge 2 -and $crumple -ge 2);Detail="boost admissions=$boost crumple admissions=$crumple"}
 }}
 @{Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT \d+\]';Max=0}
 @{Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]';Max=0}
)
$case
