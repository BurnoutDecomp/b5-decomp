# Exercise the live EasyDrive UI through the existing named-input harness.
param([string]$OutDir = '', [int]$MaxSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$OutDir) { $OutDir = Join-Path $root ('.scratch/easydrive-' + (Get-Date -Format yyyyMMdd-HHmmss)) }
$out = [IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force $out | Out-Null
. "$root/tools/diagnostics/_box_lock.ps1"
Enter-BoxLock -Label 'EasyDrive UI regression'
$events = @{}
$runner = $null
$gamePid = $null
$log = Join-Path $root 'build/game/BrnGame.log'
$oldTrace = $env:BRN_EASYDRIVE_TRACE
$env:BRN_EASYDRIVE_TRACE = '1'
function Check-Game {
    $runner.Refresh()
    if ($runner.HasExited) { throw 'Game harness exited before the scenario completed.' }
    if ((Get-Content $log -Raw) -match '\[ASSERT [0-9]+\]') { throw 'Game assertion during EasyDrive scenario.' }
}
function Press([string]$Name) {
    Check-Game
    $events[$Name].Set() | Out-Null
    Start-Sleep -Milliseconds 250
}
function Snapshot([string]$Name) {
    Start-Sleep -Milliseconds 1500
    Check-Game
    $frame = Get-ChildItem "$out/frames" -Filter '*.bmp' | Sort-Object Name | Select-Object -Last 1
    if (!$frame) { throw 'No frame captured.' }
    Copy-Item -LiteralPath $frame.FullName -Destination "$out/$Name.bmp"
    Copy-Item -LiteralPath $log -Destination "$out/BrnGame.log"
    Write-Output "Captured $Name"
}
try {
    foreach ($name in @('DPadRight','DPadLeft','DPadDown','DPadUp','Next','Prev','OptionNext','OptionPrev','Stop')) {
        $events[$name] = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::AutoReset,
            "Local\BurnoutPC_Input_$name")
        $events[$name].Reset() | Out-Null
    }
    $args = @('-NoProfile','-File',"`"$root/tools/diagnostics/flow_run.ps1`"",
        '-OutDir',"`"$out`"",'-MaxSeconds',"$MaxSeconds",'-Frames','-FrameEvery','30','-NoLock')
    $launch = Get-Date
    $runner = Start-Process pwsh -ArgumentList $args -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput "$out.runner.log" -RedirectStandardError "$out.runner.err"
    $ready = $false
    while (!$runner.HasExited) {
        if (Test-Path "$out.runner.log") {
            $runnerLog = Get-Content "$out.runner.log" -Raw
            if ($runnerLog -match '\[flow\] pid=(\d+)') { $gamePid = [int]$Matches[1] }
            if ($runnerLog -match 'log confirmed fresh' -and (Test-Path $log) -and (Get-Item $log).LastWriteTime -ge $launch) {
                Check-Game
                if ((Get-Content $log -Raw) -match 'CarSelectManager: Exit state is finished') { $ready = $true; break }
            }
        }
        Start-Sleep -Milliseconds 250
        $runner.Refresh()
    }
    if (!$ready) { throw 'Default game flow did not reach driving.' }
    Start-Sleep -Seconds 3
    Snapshot 'driving'
    Press 'DPadRight'
    Snapshot 'opened'
    $opening = [regex]::Matches((Get-Content $log -Raw), '\[easydrive-view\] panel 1 list 2.*count (\d+)') | Select-Object -Last 1
    if (!$opening) { throw 'EasyDrive did not open.' }
    $count = [int]$opening.Groups[1].Value
    if ($count -notin @(8,9)) { throw "Unexpected offline shortcut count: $count" }
    Press 'DPadDown'
    Press 'DPadRight'
    Snapshot 'freeburn-branch'
    Press 'DPadDown'
    Snapshot 'branch-down'
    Press 'DPadLeft'
    Snapshot 'branch-back'
    1..($count - 2) | ForEach-Object { Press 'DPadDown' }
    Snapshot 'scrolled-bottom'
    Press 'DPadLeft'
    Snapshot 'closed'
    $text = Get-Content $log -Raw
    foreach ($pattern in @('\[easydrive-view\] panel 1 list 2', '\[easydrive-view\].*branch 5', ("\[easydrive-view\].*selected " + ($count - 1) + " row 4 count " + $count), '\[easydrive-view\] panel 5')) {
        if ($text -notmatch $pattern) { throw "Missing EasyDrive transition: $pattern" }
    }
    Press 'DPadRight'
    Snapshot 'reopened'
    1..($count - 5) | ForEach-Object { Press 'DPadDown' }
    Press 'DPadRight'
    Start-Sleep -Seconds 5
    Snapshot 'view-challenges'
    if ((Get-Content $log -Raw) -notmatch '\[challenge-browser\] ready') { throw 'Challenge browser never became ready.' }
    1..6 | ForEach-Object { Press 'Next' }
    Snapshot 'challenges-scrolled'
    Press 'OptionNext'
    Snapshot 'challenges-three-players'
    Press 'Stop'
    Start-Sleep -Seconds 3
    Snapshot 'returned-to-driving'

    if ((Get-Content $log -Raw) -notmatch '\[easydrive-command\] main option 4') { throw 'View Challenges did not reach InGame.' }
    if ((Get-Content $log -Raw) -notmatch "\[tut-ticker\] InGameMessageRenderer queued custom message added=1 training=0") { throw 'Challenge description did not reach the ticker renderer.' }
    Write-Output "EasyDrive and challenge browser: PASS ($out). Captured navigation, scrolling, player filter, ticker, and return to driving."
} finally {
    foreach ($event in $events.Values) { $event.Reset() | Out-Null; $event.Dispose() }
    if (Test-Path $log) { Copy-Item -LiteralPath $log -Destination "$out/BrnGame.log" }
    if ($gamePid) { Stop-Process -Id $gamePid -Force -ErrorAction SilentlyContinue }
    if ($runner) { $runner.Refresh(); if (!$runner.HasExited) { Stop-Process -Id $runner.Id -Force -ErrorAction SilentlyContinue } }
    $env:BRN_EASYDRIVE_TRACE = $oldTrace
}
