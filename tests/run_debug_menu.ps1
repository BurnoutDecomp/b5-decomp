# Drive the real debug controller through named keys, with the existing game harness.
# Requires this repo inside BP-Decomp_Workflow and a built executable + game data.
param([string]$OutDir = '', [int]$MaxSeconds = 180)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$OutDir) { $OutDir = Join-Path $root ('.scratch/debug-menu-' + (Get-Date -Format yyyyMMdd-HHmmss)) }
$out = [IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force $out | Out-Null
. "$root/tools/diagnostics/_box_lock.ps1"
Enter-BoxLock -Label 'debug-menu regression'
$keys = @{}
$runner = $null
$gamePid = $null
$log = Join-Path $root 'build/game/BrnGame.log'
$prefix = 'debug-menu-' + [guid]::NewGuid().ToString('N')
$savedFiles = @{}
$oldTrace = $env:BRN_DEBUG_UI_TRACE
$env:BRN_DEBUG_UI_TRACE = '1'

function Check-Game {
    $runner.Refresh()
    if ($runner.HasExited) { throw 'Game harness exited before the scenario completed.' }
    if ((Get-Content $log -Raw) -match '\[ASSERT [0-9]+\]') { throw 'Game assertion during debug-menu scenario.' }
}
function Tap-Key([int]$Key, [bool]$Shift = $false, [bool]$Control = $false) {
    if ($Shift) { $keys[16].Set() | Out-Null }
    if ($Control) { $keys[17].Set() | Out-Null }
    Start-Sleep -Milliseconds 20
    $keys[$Key].Set() | Out-Null
    Start-Sleep -Milliseconds 60
    $keys[$Key].Reset() | Out-Null
    $keys[16].Reset() | Out-Null
    $keys[17].Reset() | Out-Null
    Start-Sleep -Milliseconds 60
}
function Type-Text([string]$Text) {
    foreach ($c in $Text.ToCharArray()) {
        $n = [int]$c
        if ($n -ge 97 -and $n -le 122) { Tap-Key ($n - 32) }
        elseif ($n -ge 65 -and $n -le 90) { Tap-Key $n $true }
        elseif ($n -ge 48 -and $n -le 57) { Tap-Key $n }
        else {
            switch ($c) {
                ' ' { Tap-Key 32 }
                '/' { Tap-Key 191 }
                '"' { Tap-Key 222 $true }
                '.' { Tap-Key 190 }
                '-' { Tap-Key 189 }
                default { throw "Unsupported test character: $c" }
            }
        }
    }
}
function Command([string]$Text) {
    Check-Game
    Type-Text $Text
    Tap-Key 13
    Start-Sleep -Milliseconds 300
    Write-Host "debug-menu: $Text"
}
function Snapshot([string]$Name) {
    Start-Sleep -Milliseconds 1400
    Check-Game
    $frame = Get-ChildItem "$out/frames" -Filter '*.bmp' | Sort-Object Name | Select-Object -Last 1
    if (!$frame) { throw 'No harness frame was captured.' }
    Copy-Item -LiteralPath $frame.FullName -Destination "$out/$Name.bmp"
}
function Save-State([string]$Name) {
    $file = "$prefix-$Name.txt"
    $path = Join-Path $root "build/game/$file"
    $savedFiles[$Name] = $path
    Command "save `"$file`""
    if (!(Test-Path -LiteralPath $path)) { throw "SAVE did not create $file" }
    Copy-Item -LiteralPath $path -Destination "$out/$Name.txt"
    return Get-Content -LiteralPath $path -Raw
}

try {
    for ($key = 0; $key -lt 256; $key++) {
        $keys[$key] = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset,
            ('Local\BurnoutPC_DebugKey_{0:X2}' -f $key))
        $keys[$key].Reset() | Out-Null
    }
    # This process owns the box lock for the entire child lifetime, including key cleanup.
    $arguments = @('-NoProfile', '-File', "`"$root/tools/diagnostics/flow_run.ps1`"",
        '-OutDir', "`"$out`"", '-MaxSeconds', "$MaxSeconds", '-Frames', '-FrameEvery', '60', '-HoldCarSelect', '-NoLock')
    $launch = Get-Date
    $runner = Start-Process pwsh -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput "$out.runner.log" -RedirectStandardError "$out.runner.err"
    $ready = $false
    while (!$runner.HasExited) {
        if (Test-Path "$out.runner.log") {
            $runnerLog = Get-Content "$out.runner.log" -Raw
            if ($runnerLog -match '\[flow\] pid=(\d+)') { $gamePid = [int]$Matches[1] }
        }
        if ((Test-Path $log) -and (Get-Item $log).LastWriteTime -ge $launch) {
            Check-Game
            if ((Get-Content $log -Raw) -match 'CSV : Entering Car Select') { $ready = $true; break }
        }
        Start-Sleep -Milliseconds 250
        $runner.Refresh()
    }
    if (!$ready) { throw 'The default junkyard flow never reached car selection.' }
    Tap-Key 32 $false $true
    Snapshot 'root'
    # Root -> World -> activate Scene sweeper, then edit its boolean and numeric rows.
    foreach ($key in @(40, 13, 13, 39, 40, 40, 39)) { Tap-Key $key }
    Snapshot 'boolean'
    Tap-Key 192
    $state = Save-State 'menu-edit'
    if ($state -notmatch 'SET "/World/Scene sweeper/Render scene sweeper boxes for dynamic objects" "TRUE"') { throw 'Menu boolean edit failed.' }
    if ($state -notmatch 'Scene sweeper render draw distance" "51.000"') { throw 'Menu numeric edit failed.' }
    Command 'set "World/Scene sweeper/Render scene sweeper boxes for dynamic objects" FALSE'
    Command 'help'
    Command 'set "Core/Debug/Settings/Text Size" 18'
    $state = Save-State 'before'
    if ($state -notmatch 'Text Size" "18.000"') { throw 'SET did not change the numeric value.' }
    Command 'set "Core/Debug/Settings/Text Size" 12'
    Command "exec `"$prefix-before.txt`""
    $state = Save-State 'restored'
    if ($state -notmatch 'Text Size" "18.000"') { throw 'EXEC did not restore the saved value.' }
    Command 'alias smoke "Core/Debug/Settings/Text Size"'
    Command 'smoke 20'
    Command 'bind F12 smoke 22'
    Tap-Key 123
    $state = Save-State 'bound'
    if ($state -notmatch 'Text Size" "22.000"' -or $state -notmatch 'ALIAS smoke' -or $state -notmatch 'BIND F12 smoke 22') {
        throw 'Alias or F12 binding failed.'
    }
    Command 'call "Debug/Sim/Step"'
    Command 'call "Debug/Sim/Play"'
    Command 'print "debug menu harness passed"'
    Snapshot 'console'
    Command 'set "Core/Debug/Settings/Text Size" 16'
    Tap-Key 192
    Tap-Key 191
    Snapshot 'pinned'
    Tap-Key 27
    $beforeHiddenKeys = [regex]::Matches((Get-Content $log -Raw), '\[debug-ui\]').Count
    foreach ($key in @(40, 13, 32, 8, 27)) { Tap-Key $key }
    if ([regex]::Matches((Get-Content $log -Raw), '\[debug-ui\]').Count -ne $beforeHiddenKeys) {
        throw 'Ordinary keys reached the hidden debug menu.'
    }
    Snapshot 'closed'
    Check-Game
    Write-Output "Debug menu regression: PASS ($out)"
}
finally {
    foreach ($event in $keys.Values) { $event.Reset() | Out-Null; $event.Dispose() }
    if ($gamePid) { Stop-Process -Id $gamePid -ErrorAction SilentlyContinue }
    if ($runner -and !$runner.HasExited) { $runner.WaitForExit(10000) | Out-Null }
    foreach ($path in $savedFiles.Values) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    $env:BRN_DEBUG_UI_TRACE = $oldTrace
}
