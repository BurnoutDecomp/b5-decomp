# Live check of the PC hardware-init helpers against the real game window.
#
# 1. Seeds known accessibility values (all three hotkey bits on, screensaver active) for this
#    logon session, launches the game, and reads the system state once the window is up:
#    the hotkey-active and confirm-hotkey bits must be clear, every other field unchanged,
#    and the screensaver off.
# 2. Posts a lone Alt press and release to the window. The game thread must not enter menu
#    mode (a menu loop stalls the frame loop).
# 3. Closes the window with WM_CLOSE and, after the process exits, requires the seeded values
#    back exactly. The user's own values are restored at the end whatever happened.
#
#   powershell -ExecutionPolicy Bypass -File b5-decomp/tests/run_hardware_init_live.ps1 [-Exe <path>]
param(
    [string]$Exe,
    [int]$BootSeconds = 20
)
$ErrorActionPreference = 'Stop'
$workflow = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $Exe) { $Exe = Join-Path $workflow 'build/game/Burnout_PC.exe' }
$Exe = (Resolve-Path $Exe).Path

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HwInitLive
{
    [StructLayout(LayoutKind.Sequential)] public struct STICKYKEYS { public uint cbSize; public uint dwFlags; }
    [StructLayout(LayoutKind.Sequential)] public struct FILTERKEYS
    { public uint cbSize; public uint dwFlags; public uint iWaitMSec; public uint iDelayMSec; public uint iRepeatMSec; public uint iBounceMSec; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
    [StructLayout(LayoutKind.Sequential)] public struct GUITHREADINFO
    { public uint cbSize; public uint flags; public IntPtr hwndActive, hwndFocus, hwndCapture, hwndMenuOwner, hwndMoveSize, hwndCaret; public RECT rcCaret; }

    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW")] static extern bool SpiSticky(uint a, uint u, ref STICKYKEYS p, uint w);
    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW")] static extern bool SpiFilter(uint a, uint u, ref FILTERKEYS p, uint w);
    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW")] static extern bool SpiBool(uint a, uint u, ref int p, uint w);
    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW")] static extern bool SpiSet(uint a, uint u, IntPtr p, uint w);
    [DllImport("user32.dll")] public static extern IntPtr FindWindowA(string c, string n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessageA(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern bool GetGUIThreadInfo(uint tid, ref GUITHREADINFO g);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);

    public static bool Focus(IntPtr h)
    {
        uint unused;
        uint other = GetWindowThreadProcessId(GetForegroundWindow(), out unused);
        uint self = GetCurrentThreadId();
        AttachThreadInput(self, other, true);
        SetForegroundWindow(h);
        BringWindowToTop(h);
        AttachThreadInput(self, other, false);
        return GetForegroundWindow() == h;
    }
    public static bool IsForeground(IntPtr h) { return GetForegroundWindow() == h; }
    public static void Tap(byte vk)
    {
        keybd_event(vk, 0, 0, UIntPtr.Zero);
        keybd_event(vk, 0, 2, UIntPtr.Zero);   // KEYEVENTF_KEYUP
    }

    public static uint[] Read()
    {
        STICKYKEYS s = new STICKYKEYS(); s.cbSize = 8; SpiSticky(0x3A, 8, ref s, 0);
        STICKYKEYS t = new STICKYKEYS(); t.cbSize = 8; SpiSticky(0x34, 8, ref t, 0);
        FILTERKEYS f = new FILTERKEYS(); f.cbSize = 24; SpiFilter(0x32, 24, ref f, 0);
        int ss = 0; SpiBool(0x10, 0, ref ss, 0);
        return new uint[] { s.dwFlags, t.dwFlags, f.dwFlags, f.iWaitMSec, f.iDelayMSec, f.iRepeatMSec, f.iBounceMSec, (uint)ss };
    }
    public static void Write(uint[] v)
    {
        STICKYKEYS s = new STICKYKEYS(); s.cbSize = 8; s.dwFlags = v[0]; SpiSticky(0x3B, 8, ref s, 0);
        STICKYKEYS t = new STICKYKEYS(); t.cbSize = 8; t.dwFlags = v[1]; SpiSticky(0x35, 8, ref t, 0);
        FILTERKEYS f = new FILTERKEYS(); f.cbSize = 24; f.dwFlags = v[2]; f.iWaitMSec = v[3]; f.iDelayMSec = v[4]; f.iRepeatMSec = v[5]; f.iBounceMSec = v[6];
        SpiFilter(0x33, 24, ref f, 0);
        SpiSet(0x11, v[7], IntPtr.Zero, 0);
    }
    public static uint GuiFlags(uint tid)
    {
        GUITHREADINFO g = new GUITHREADINFO(); g.cbSize = (uint)Marshal.SizeOf(typeof(GUITHREADINFO));
        return GetGUIThreadInfo(tid, ref g) ? g.flags : 0xFFFFFFFF;
    }
}
'@

$names = 'sticky flags', 'toggle flags', 'filter flags', 'filter wait', 'filter delay', 'filter repeat', 'filter bounce', 'screensaver active'
$results = New-Object System.Collections.Generic.List[object]
function Check([bool]$pass, [string]$label) {
    $results.Add([pscustomobject]@{ Pass = $pass; Label = $label })
    Write-Output (($(if ($pass) { 'PASS  ' } else { 'FAIL  ' })) + $label)
}
function Show([uint32[]]$v) { (0..7 | ForEach-Object { '{0}=0x{1:X}' -f $names[$_], $v[$_] }) -join ', ' }

if (Get-Process Burnout_PC -ErrorAction SilentlyContinue) { throw 'Burnout_PC.exe is already running; the single-instance guard would refuse a second copy.' }

$user = [HwInitLive]::Read()
# Hotkey-active (0x4) and confirm-hotkey (0x8) on in all three, plus the available bit, and
# filter timings that differ from each other so a swapped field shows.
$seed = [uint32[]]@(($user[0] -bor 0x0E), ($user[1] -bor 0x0E), ($user[2] -bor 0x0E), 1000, 500, 300, 0, 1)
$game = $null
try {
    [HwInitLive]::Write($seed)
    $seeded = [HwInitLive]::Read()
    Write-Output ('seeded:  ' + (Show $seeded))

    $info = New-Object System.Diagnostics.ProcessStartInfo $Exe, '-skipvideos'
    $info.WorkingDirectory = Split-Path $Exe -Parent
    $info.UseShellExecute = $false
    $info.EnvironmentVariables['BRN_AUDIO_MUTE'] = '1'
    $game = [System.Diagnostics.Process]::Start($info)

    $window = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds($BootSeconds)
    while ((Get-Date) -lt $deadline -and -not $game.HasExited) {
        $window = [HwInitLive]::FindWindowA('BurnoutParadiseWindowClass', 'Burnout Paradise')
        if ($window -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 250
    }
    Check ($window -ne [IntPtr]::Zero) 'game window created (device notification registration succeeded)'
    if ($window -eq [IntPtr]::Zero) { throw 'no game window' }
    Start-Sleep -Seconds 8   # well past InitializeHardware

    $during = [HwInitLive]::Read()
    Write-Output ('running: ' + (Show $during))
    Check ($during[0] -eq ($seeded[0] -band 0xFFFFFFF3)) 'sticky keys: hotkey bits cleared, other flags kept'
    Check ($during[1] -eq ($seeded[1] -band 0xFFFFFFF3)) 'toggle keys: hotkey bits cleared, other flags kept'
    Check ($during[2] -eq ($seeded[2] -band 0xFFFFFFF3)) 'filter keys: hotkey bits cleared, other flags kept'
    Check (($during[3..6] -join ',') -eq ($seeded[3..6] -join ',')) 'filter keys: timings unchanged'
    if ($seeded[7] -ne 0) { Check ($during[7] -eq 0) 'screensaver disabled while running' }
    else { Write-Output 'SKIP  screensaver: the system kept it off when seeded, so the switch is not observable' }

    $gamePid = [uint32]0
    $thread = [HwInitLive]::GetWindowThreadProcessId($window, [ref]$gamePid)
    # Real key input only goes to the foreground window, so the probe runs only when the game
    # window holds the foreground; otherwise it would tap Alt into another application.
    if ([HwInitLive]::Focus($window)) {
        Start-Sleep -Milliseconds 500
        $before = [HwInitLive]::GuiFlags($thread)
        if ([HwInitLive]::IsForeground($window)) { [HwInitLive]::Tap(0x12) }   # VK_MENU
        # Menu mode can end on its own within a second, so sample through that window.
        $samples = @(); $menu = 0
        for ($i = 0; $i -lt 10; $i++) {
            Start-Sleep -Milliseconds 150
            $flags = [HwInitLive]::GuiFlags($thread)
            $samples += '{0:X}' -f $flags
            $menu = $menu -bor ($flags -band 0x1C)   # GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE
        }
        Write-Output ('gui thread flags before 0x{0:X}, after Alt: {1}' -f $before, ($samples -join ' '))
        Check ($menu -eq 0) 'lone Alt does not put the game thread into menu mode'
        if ($menu -ne 0 -and [HwInitLive]::IsForeground($window)) { [HwInitLive]::Tap(0x1B) }   # Esc leaves menu mode
        Start-Sleep -Milliseconds 500
    } else {
        Write-Output 'SKIP  lone Alt probe: could not bring the game window to the foreground'
    }

    [void][HwInitLive]::PostMessageA($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)                 # WM_CLOSE
    $exited = $game.WaitForExit(20000)
    Check $exited 'game exits after WM_CLOSE'
    $restored = [HwInitLive]::Read()
    Write-Output ('exited:  ' + (Show $restored))
    Check (($restored -join ',') -eq ($seeded -join ',')) 'every saved value restored on exit'
}
finally {
    if ($game -and -not $game.HasExited) { $game.Kill(); $game.WaitForExit(10000) | Out-Null }
    [HwInitLive]::Write($user)
}
$failed = @($results | Where-Object { -not $_.Pass }).Count
Write-Output ('run_hardware_init_live: {0}/{1} pass ({2} fail)' -f ($results.Count - $failed), $results.Count, $failed)
exit $(if ($failed) { 1 } else { 0 })
