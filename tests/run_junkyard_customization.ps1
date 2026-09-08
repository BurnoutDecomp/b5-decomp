# Exercise the default Hunter Cavalry junkyard through the existing flow harness.
# Run from any directory; game data and a built executable must already be installed.
param([string]$OutDir = '', [int]$MaxSeconds = 110)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$OutDir) { $OutDir = Join-Path $root ('.scratch/junkyard-' + (Get-Date -Format yyyyMMdd-HHmmss)) }
$out = [IO.Path]::GetFullPath($OutDir)
$events = @{}
foreach ($name in @('Accept','Next','Prev','OptionNext','OptionPrev','Stop')) {
    $events[$name] = [Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::AutoReset,"Local\BurnoutPC_Input_$name")
}
$launch = Get-Date
$runnerArgs = @('-NoProfile','-File',"$root\tools\diagnostics\flow_run.ps1",'-OutDir',$out,'-MaxSeconds',[string]$MaxSeconds,'-Frames','-FrameEvery','30','-HoldCarSelect')
$runner = Start-Process pwsh -ArgumentList $runnerArgs -PassThru -WindowStyle Hidden -RedirectStandardOutput "$out.runner.log" -RedirectStandardError "$out.runner.err"
$log = Join-Path $root 'build\game\BrnGame.log'
$sent = $false
while (!$runner.HasExited) {
    if (!$sent -and (Test-Path $log) -and (Get-Item $log).LastWriteTime -ge $launch) {
        $text = Get-Content $log -Raw
        if ($text -match 'CSV : Entering Car Select') {
            Start-Sleep -Seconds 4
            $events.Accept.Set() | Out-Null
            Write-Output 'harness: accepted car, entering paint selection'
            $sent = $true
            Start-Sleep -Seconds 5
            foreach ($step in @('OptionNext','OptionNext','OptionNext','Next','OptionNext','Next','OptionNext','OptionNext','Accept')) {
                $events[$step].Set() | Out-Null
                Write-Output "harness regression: $step"
                Start-Sleep -Seconds 6
            }
        }
    }
    Start-Sleep -Milliseconds 250
    $runner.Refresh()
}

foreach ($handle in $events.Values) { $handle.Dispose() }
if (!$sent) { throw 'The harness never reached car selection.' }
$runLog = Get-Content (Join-Path $out 'BrnGame.log') -Raw
if ($runLog -match '\[ASSERT') { throw "Game assertions occurred; inspect $out" }
foreach ($model in @('PUSMC1A2','PUSMC1A3')) {
    if ($runLog -notmatch "LoadBundle 'Vehicles\\VEH_${model}_GR.bin'") {
        throw "Finish model $model was not loaded."
    }
}
if ([regex]::Matches($runLog, 'CarSelectManager: StreamingFinished').Count -lt 3) {
    throw 'Not all three finish swaps completed.'
}
if ($runLog -notmatch 'ChangePlayerCarColour' -or $runLog -notmatch 'CarSelectManager: Exit state is finished') {
    throw 'Paint changes or the return to driving did not complete.'
}
# Initial paint indices vary with the fresh profile's randomized selection. The last
# three paint writes witness the current colour followed by our two OptionNext taps.
# Check that both taps change colour within the selected palette, rather than pinning
# this regression to one run's starting colour (formerly palette 1, colours 14/15).
$paintWrites = [regex]::Matches($runLog,
    'ChangePlayerCarColour: player slot 0 -> palette (\d+) colour (\d+)')
if ($paintWrites.Count -lt 3) { throw 'Missing paint-selection witnesses.' }
$lastPaint = @($paintWrites | Select-Object -Last 3)
for ($index = 1; $index -lt 3; $index++) {
    if ($lastPaint[$index].Groups[1].Value -ne $lastPaint[$index - 1].Groups[1].Value -or
        $lastPaint[$index].Groups[2].Value -eq $lastPaint[$index - 1].Groups[2].Value) {
        throw 'Each colour-selection tap must change colour within the selected palette.'
    }
}
Write-Output "Junkyard customization regression: PASS ($out)"

