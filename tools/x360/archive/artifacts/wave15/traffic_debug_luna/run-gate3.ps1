$ErrorActionPreference = 'Stop'
$trafficArtifactRoot = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\traffic_debug_luna'
$trafficGateOut = Join-Path $trafficArtifactRoot 'gate3'
$trafficTempDir = Join-Path $trafficArtifactRoot 'temp\gate3'
$trafficConsoleLog = Join-Path $trafficArtifactRoot 'gate3-console.log'
New-Item -ItemType Directory -Path $trafficGateOut -Force | Out-Null
New-Item -ItemType Directory -Path $trafficTempDir -Force | Out-Null
$env:TEMP = $trafficTempDir
$env:TMP = $trafficTempDir
$trafficIncludes = @(
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EABase\include\Common',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EASTL\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EAThread\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\renderware\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\PPMalloc\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\coreallocator\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\dirtysdk\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\zlib\include',
    'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\lua\src'
)
$trafficBuildArgs = @(
    'tools/build_x360.py',
    '--sdk', 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK',
    '--source', 'src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.cpp',
    '--source', 'src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule_wW_01.cpp',
    '--source', 'src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLogger.cpp',
    '--source', 'src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.cpp',
    '--source', 'src/GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleDebugComponent.cpp',
    '--out', $trafficGateOut,
    '--timeout', '180',
    '--jobs', '1'
)
foreach ($trafficInclude in $trafficIncludes) {
    $trafficIncludeArg = '/I' + $trafficInclude
    $trafficBuildArgs += @('--compiler-arg', $trafficIncludeArg)
}
$trafficBuildArgs += @('--compiler-arg', '/showIncludes')
$trafficStartedUtc = [DateTime]::UtcNow.ToString('o')
& python @trafficBuildArgs 2>&1 | Tee-Object -FilePath $trafficConsoleLog
$trafficExitCode = $LASTEXITCODE
Set-Content -LiteralPath (Join-Path $trafficArtifactRoot 'gate3-exit-code.txt') -Value $trafficExitCode -NoNewline
Set-Content -LiteralPath (Join-Path $trafficArtifactRoot 'gate3-started-utc.txt') -Value $trafficStartedUtc -NoNewline
exit $trafficExitCode

