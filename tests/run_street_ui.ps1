# Drive the original street HUD using the named-input game harness.
param([string]$OutDir = '', [string]$Teleport = '', [int]$MaxSeconds = 100)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$OutDir) { $OutDir = Join-Path $root ('.scratch/streets-' + (Get-Date -Format yyyyMMdd-HHmmss)) }
$out = [IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force $out | Out-Null
$oldTrace = $env:BRN_STREET_UI_TRACE
try {
    $env:BRN_STREET_UI_TRACE = '1'
    $flowArgs = @('-NoProfile','-File',"$root/tools/diagnostics/flow_run.ps1",'-OutDir',$out,
        '-MaxSeconds',"$MaxSeconds",'-Frames','-FrameEvery','60','-Drive','-DriveSeconds','40')
    if ($Teleport) { $flowArgs += @('-Teleport',$Teleport) }
    & pwsh @flowArgs *> "$out.runner.log"
    if ($LASTEXITCODE -ne 0) { throw "Street driving harness failed; see $out.runner.log" }
    $log = Get-Content "$out/BrnGame.log" -Raw
    if ($log -match '\[ASSERT [0-9]+\]|EXCEPTION_ACCESS_VIOLATION') { throw 'Assertion or crash during street driving.' }
    $roads = @([regex]::Matches($log, '\[street-ui\] entered road (\d+)') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($roads.Count -lt 2) { throw "Expected road transitions, observed $($roads.Count) distinct roads." }
    Write-Output "Street tracking: PASS ($($roads -join ', ')). Review original HUD signs in $out/frames."
} finally {
    $env:BRN_STREET_UI_TRACE = $oldTrace
}
