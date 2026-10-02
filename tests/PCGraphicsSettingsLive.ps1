# Native graphics settings smoke test, using a private flow-harness slot.
# The slot's INI is restored byte-for-byte; the user's main INI/save is untouched.
param([int]$Slot = 9, [int]$MaxSeconds = 135, [string]$OutDir = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
if ($Slot -le 0) { throw 'Use a private slot greater than zero.' }
if (!$OutDir) { $OutDir = Join-Path $root ('scratch/graphics_settings_live/' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
$OutDir = [IO.Path]::GetFullPath($OutDir)
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $root 'scratch')) + [IO.Path]::DirectorySeparatorChar
if (!$OutDir.StartsWith($scratchRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Live output must be inside workflow scratch.' }
[void](New-Item -ItemType Directory -Force -Path $OutDir)
. (Join-Path $root 'tools/diagnostics/_box_lock.ps1')
Enter-BoxLock -Slot $Slot -TimeoutSec 30 -Label 'graphics settings setup/run/restore'
$heldSlotMutex = $script:BrnBoxLock
try {
    # Intentionally seed a missing private profile for this returning-player smoke
    # test. The source profile is only read; no fresh-profile scenario is claimed.
    & (Join-Path $root 'tools/tests/slots.ps1') -Slot $Slot
    if ($LASTEXITCODE -ne 0) { throw "Private slot staging failed: $LASTEXITCODE" }
    $config = Join-Path $root "build/game_slots/$Slot/config.ini"
    $hadConfig = Test-Path -LiteralPath $config
    [byte[]]$before = $null
    if ($hadConfig) {
        $before = [IO.File]::ReadAllBytes($config)
        [IO.File]::WriteAllBytes((Join-Path $OutDir 'config.before.ini'), $before)
    }
    try {
        $settings = @'
[Display]
Width=1280
Height=720
AdapterIndex=0
VSync=1
DecoupleSimulation=1
Fullscreen=0
[Settings]
AntiAliasing=8
AlphaToCoverage=1
EnvironmentMap=1
EnvironmentMap30Hz=1
Coronas=1
SunCorona=1
[Graphics]
BloomLuminanceScale=0.3
EnvironmentMapLOD=0
TrafficShadows=1
WorldLODOverrideDistance=3000
PropLODOverrideDistance=3000
VehicleLODPreset=High
'@
        [IO.File]::WriteAllText($config, $settings, [Text.Encoding]::ASCII)
        [IO.File]::WriteAllText((Join-Path $OutDir 'config.test.ini'), $settings, [Text.Encoding]::ASCII)
        # flow_run recreates only this bounded child of the checked scratch directory.
        # The OUTER slot mutex remains held. Skip the inner lock acquisition so its
        # process-exit lifetime cannot outlive this runner's balanced release.
        & (Join-Path $root 'tools/diagnostics/flow_run.ps1') -Slot $Slot -MaxSeconds $MaxSeconds -NoLock `
            -SkipIntro -AcceptGap 1 -Frames -FrameEvery 480 -Drive -DriveSeconds 15 `
            -DiagEnv 'BRN_FRAME_DUMP_MAX=16' -OutDir (Join-Path $OutDir 'flow') -FrameDir (Join-Path $OutDir 'frames')
        if ($LASTEXITCODE -ne 0) { throw "Flow harness failed: $LASTEXITCODE" }
        $log = Get-Content -LiteralPath (Join-Path $OutDir 'flow/BrnGame.log') -Raw
        $marks = Get-Content -LiteralPath (Join-Path $OutDir 'flow/marks.txt') -Raw
        $checks = [ordered]@{
            Settings = $log -match '\[graphics\] bloom=0\.3\d* envmapLOD=0 trafficShadows=1 worldLOD=3000 propLOD=3000 vehicleLOD=High aaRequest=8'
            Native8x = $log -match 'colourTexture=MSAA msaa=8'
            Driving = $marks -match 'phase=DRIVING'
            NoFault = $log -notmatch '\[ASSERT|\[EXCEPTION\]|resize allocation failed'
            Frames = @(Get-ChildItem -LiteralPath (Join-Path $OutDir 'frames') -Filter '*.bmp').Count -gt 0
        }
        $passed = @($checks.Values | Where-Object { !$_ }).Count -eq 0
        [IO.File]::WriteAllText((Join-Path $OutDir 'result.json'),
            (@{ Pass=$passed; Checks=$checks; Slot=$Slot; Config=$settings;
                ExeSHA256=(Get-FileHash -LiteralPath (Join-Path $root "build/game_slots/$Slot/Burnout_PC.exe") -Algorithm SHA256).Hash } | ConvertTo-Json -Depth 5))
        $checks | Format-Table
        if (!$passed) { throw 'Graphics live checks failed. See result.json and flow logs.' }
        Write-Host "[graphics-live] PASS: $OutDir"
    } finally {
        if ($hadConfig) { [IO.File]::WriteAllBytes($config, $before) }
        elseif (Test-Path -LiteralPath $config) { Remove-Item -LiteralPath $config }
    }
} finally {
    $heldSlotMutex.ReleaseMutex()
    $heldSlotMutex.Dispose()
    $script:BrnBoxLock = $null
}
