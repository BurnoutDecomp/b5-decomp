# Reproduce traffic interpolation through real checks/slams using a private save.
# The display config is restored byte-for-byte; no frame dumps are needed.
param([string]$Name='traffic_render_interpolation',[int]$Seconds=85,[int]$Slot=8)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$out=Join-Path $repo "scratch/traffic_interpolation/$Name"
[void](New-Item -ItemType Directory -Force -Path $out)
$config=Join-Path $repo 'build/game/config.ini'
$saved=[IO.File]::ReadAllBytes($config)
[IO.File]::WriteAllBytes((Join-Path $out 'config.before.ini'),$saved)
try {
    & "$repo/tools/tests/slots.ps1" -Slot $Slot
    $text=[IO.File]::ReadAllText($config)
    $text=$text -replace '(?m)^Width=\d+','Width=1280' -replace '(?m)^Height=\d+','Height=720' -replace '(?m)^VSync=\d+','VSync=0'
    [IO.File]::WriteAllText($config,$text)
    Get-CimInstance Win32_Processor | Select-Object Name,LoadPercentage | ConvertTo-Json | Set-Content "$out/host.json"
    $cycle=@('3045.0/-1.8/-1860.0/180:46','3031.0/-4.8/-2010.0/0:46','3045.0/-1.8/-1860.0/180:38','3031.0/-4.8/-2010.0/0:38')
    $shots=(@(0..11) | ForEach-Object {$cycle[$_ % 4]}) -join ','
    & "$repo/tools/diagnostics/flow_run.ps1" -Slot $Slot -MaxSeconds $Seconds -SkipIntro -AcceptGap 1 `
        -Drive -ThrottleScript '0:accel,3:none' -CrashSweep '3040.7,-5.8,-1937.9' `
        -CrashSweepShots $shots -CrashSweepSettle 420 -CrashSweepMax 900 -SkipTrainingTip `
        -DiagEnv 'BRN_TRAFFIC_INTERP_DIAG=1,BRN_TRAFFIC_DIAG=1,BRN_CRASH_ACTION_DIAG=1' -OutDir "$out/flow"
    if ($LASTEXITCODE -ne 0) {throw "Flow failed: $LASTEXITCODE"}
    & python "$PSScriptRoot/analyze_traffic_interpolation.py" "$out/flow/BrnGame.log" --output "$out/pose_summary.json" --check
    if ($LASTEXITCODE -ne 0) {throw 'Traffic render interpolation check failed; inspect pose_summary.json.'}
} finally {[IO.File]::WriteAllBytes($config,$saved)}
