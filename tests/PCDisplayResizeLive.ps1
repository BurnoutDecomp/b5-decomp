# Live display regression. Uses the existing flow harness and a private save slot.
# Screenshots are deliberately bounded; config.ini is restored byte-for-byte.
param([int]$Slot = 8, [string]$OutDir = '', [int]$PauseAt = 35, [int]$UnpauseAt = 48)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$OutDir) { $OutDir = Join-Path $repo ('scratch/display_resize/' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
[void](New-Item -ItemType Directory -Force -Path $OutDir)
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path
$config = Join-Path $repo 'build/game/config.ini'
$configBytes = [IO.File]::ReadAllBytes($config)
[IO.File]::WriteAllBytes((Join-Path $OutDir 'config.before.ini'), $configBytes)
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DisplayResizeLive {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] public static extern bool AdjustWindowRect(ref Rect rect,uint style,bool menu);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int w,int h,uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int mode);
}
'@
[void][DisplayResizeLive]::SetProcessDpiAwarenessContext([IntPtr](-4))
$runner = $null
try {
    & (Join-Path $repo 'tools/tests/slots.ps1') -Slot $Slot
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',
        (Join-Path $repo 'tools/diagnostics/flow_run.ps1'),'-Slot',"$Slot",'-MaxSeconds','135',
        '-SkipIntro','-AcceptGap','1','-Frames','-FrameEvery','240','-DiagEnv','BRN_FRAME_DUMP_MAX=35',
        '-PauseAt',"$PauseAt",'-UnpauseAt',"$UnpauseAt",'-OutDir',"$OutDir/flow",'-FrameDir',"$OutDir/frames")
    $arguments = $arguments | ForEach-Object { '"' + $_ + '"' }
    $runner = Start-Process powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$OutDir/runner.log" -RedirectStandardError "$OutDir/runner.err"
    $runnerHandle = $runner.Handle # Retain the handle so ExitCode remains available after exit.
    $deadline = (Get-Date).AddSeconds(80)
    $game = $null
    do {
        Start-Sleep -Milliseconds 300
        $game = Get-Process Burnout_PC -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$repo\build\game_slots\$Slot\Burnout_PC.exe" } | Select-Object -First 1
        $ready = (Test-Path "$OutDir/runner.log") -and ((Get-Content "$OutDir/runner.log" -Raw) -match '\[flow\] strfin\s+at')
    } until (($game -and $ready) -or (Get-Date) -gt $deadline -or $runner.HasExited)
    if (!$game -or !$ready) { throw 'Live run did not reach driving.' }
    $game.Refresh(); $window = $game.MainWindowHandle
    if (!$window) { throw 'No game window in the private slot.' }
    $events = [Collections.Generic.List[object]]::new()
    function Record([string]$label) {
        $r = New-Object DisplayResizeLive+Rect
        [void][DisplayResizeLive]::GetClientRect($window,[ref]$r)
        $events.Add([pscustomobject]@{Time=(Get-Date).ToString('o');Action=$label;Width=$r.R;Height=$r.B})
        Write-Host "[display-live] $label client=$($r.R)x$($r.B)"
        $events | ConvertTo-Json | Set-Content "$OutDir/events.json"
    }
    function Toggle {
        [void][DisplayResizeLive]::SendMessage($window,0x100,[IntPtr]0x7a,[IntPtr]1)
        Record 'F11'
    }
    function Resize([int]$w,[int]$h) {
        $r = New-Object DisplayResizeLive+Rect; $r.R=$w; $r.B=$h
        [void][DisplayResizeLive]::AdjustWindowRect([ref]$r,0xcf0000,$false)
        [void][DisplayResizeLive]::SetWindowPos($window,[IntPtr]::Zero,0,0,$r.R-$r.L,$r.B-$r.T,0x14)
        Record "window ${w}x${h}"
    }
    Record 'driving'
    Toggle
    Start-Sleep -Seconds 8
    Toggle
    Start-Sleep -Seconds 5
    foreach ($size in @(@(1920,1080),@(2560,1440),@(3840,2160),@(1024,768),@(3440,1440),@(1601,901))) {
        Resize $size[0] $size[1]
        Start-Sleep -Seconds 9
    }
    [void][DisplayResizeLive]::ShowWindow($window,6)
    Record 'minimize'
    Start-Sleep -Seconds 3
    [void][DisplayResizeLive]::ShowWindow($window,9)
    Toggle
    Start-Sleep -Seconds 8
    Toggle
    Resize 1280 720
    while (!$runner.HasExited) { Start-Sleep -Milliseconds 500 }
    $runner.WaitForExit()
    if ($runner.ExitCode -ne 0) { throw "Flow harness failed: $($runner.ExitCode)" }
    $log = Get-Content "$OutDir/flow/BrnGame.log" -Raw
    if ($log -match '\[ASSERT|\[EXCEPTION\]|resize allocation failed|uncovered region') {
        throw 'Display regression: game log contains an assertion, exception or resize failure.'
    }
    $renderSizes = @([regex]::Matches($log,'\[display\] rendering at (\d+x\d+)') | ForEach-Object { $_.Groups[1].Value })
    $expectedSizes = @($events | Where-Object { $_.Width -gt 0 -and $_.Height -gt 0 } | ForEach-Object {
        $w = $_.Width; $h = [Math]::Floor($w * 9 / 16)
        if ($h -gt $_.Height) { $h = $_.Height; $w = [Math]::Floor($h * 16 / 9) }
        "${w}x${h}"
    } | Sort-Object -Unique)
    foreach ($required in $expectedSizes) {
        if ($renderSizes -notcontains $required) { throw "Game never rendered at $required" }
    }
    $frameSizes = @(Get-ChildItem "$OutDir/frames" -Filter '*.bmp' | ForEach-Object {
        $stream = [IO.File]::OpenRead($_.FullName)
        try {
            $reader = [IO.BinaryReader]::new($stream)
            $stream.Position = 18
            $w = $reader.ReadInt32(); $h = [Math]::Abs($reader.ReadInt32())
            "${w}x${h}"
        } finally { $stream.Dispose() }
    } | Sort-Object -Unique)
    if ($frameSizes.Count -lt [Math]::Min(3,$expectedSizes.Count)) { throw 'Too few distinct captured render sizes.' }
    @{Pass=$true;RenderSizes=$renderSizes;FrameSizes=$frameSizes;FlowExit=$runner.ExitCode} |
        ConvertTo-Json | Set-Content "$OutDir/result.json"
    Write-Host "[display-live] evidence: $OutDir"
} finally {
    if ($runner -and !$runner.HasExited) {
        $ownGame = Get-Process Burnout_PC -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$repo\build\game_slots\$Slot\Burnout_PC.exe" }
        foreach ($p in $ownGame) { [void]$p.CloseMainWindow() }
        $runner.WaitForExit(10000) | Out-Null
    }
    [IO.File]::WriteAllBytes($config,$configBytes)
}
