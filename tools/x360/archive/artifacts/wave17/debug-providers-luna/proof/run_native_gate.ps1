param(
    [Parameter(Mandatory=$true)][string]$AttemptName,
    [string]$FreezeFrom = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = 'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a'
$artifactRoot = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\debug-providers-luna'
$attemptRoot = Join-Path $artifactRoot $AttemptName
if (Test-Path -LiteralPath $attemptRoot) { throw "Attempt already exists; preserving it: $attemptRoot" }
$outRoot = Join-Path $attemptRoot 'output'
$tempRoot = Join-Path $attemptRoot 'temp'
$sdkRoot = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$python = 'C:\Python312\python.exe'
$vendorRoot = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'
$vendorIncludes = @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')
$assignmentPath = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\r19-strict-game-link-diagnostic\unresolved-disjoint-assignments.json'
$assignment = Get-Content -LiteralPath $assignmentPath -Raw | ConvertFrom-Json
$maybeRow = $assignment.lanes.'02_network_dirtysdk' | Where-Object { $_.symbol -like '*MaybeDrawText*' }
if (@($maybeRow).Count -ne 1) { throw 'Could not identify one MaybeDrawText lane record.' }
$sourceNames = @(
    'src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.cpp',
    'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.cpp',
    'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.cpp',
    'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/MenuItems/CgsMenuItemVariableLineGraph.cpp',
    'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp'
) + @($maybeRow.callers)
$sourceNames = @($sourceNames | Select-Object -Unique)
if ($sourceNames.Count -ne 28) { throw "Expected 28 changed/actual-consumer CPPs, got $($sourceNames.Count)." }
New-Item -ItemType Directory -Force -Path $outRoot,$tempRoot | Out-Null
$env:TEMP = $tempRoot
$env:TMP = $tempRoot
$fallbackState = foreach ($root in $vendorIncludes) {
    $path = Join-Path $vendorRoot $root
    $exists = Test-Path -LiteralPath $path
    $gitHead = $null
    if ($exists) {
        $gitHead = (& git -C $path rev-parse HEAD 2>$null)
        if ($LASTEXITCODE -ne 0) { $gitHead = $null } else { $gitHead = $gitHead.Trim() }
    }
    [pscustomobject]@{ path = $path; exists = $exists; git_head = $gitHead }
}
$sourceBefore = foreach ($name in $sourceNames) {
    $path = Join-Path $repoRoot $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing source: $path" }
    $blob = (& git -C $repoRoot rev-parse ('HEAD:' + $name) 2>$null)
    if ($LASTEXITCODE -ne 0) { $blob = $null } else { $blob = $blob.Trim() }
    [pscustomobject]@{ path = $name; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash; git_blob_at_head = $blob }
}
if ($FreezeFrom) {
    $priorRoot = Join-Path $artifactRoot $FreezeFrom
    $priorSnapshotPath = Join-Path $priorRoot 'source-snapshot.json'
    $priorIncludesPath = Join-Path $priorRoot 'resolved-includes.json'
    if (-not (Test-Path -LiteralPath $priorSnapshotPath) -or -not (Test-Path -LiteralPath $priorIncludesPath)) { throw "Freeze manifests missing under $priorRoot" }
    $priorSnapshot = Get-Content -LiteralPath $priorSnapshotPath -Raw | ConvertFrom-Json
    foreach ($row in $sourceBefore) {
        $old = $priorSnapshot.source_after | Where-Object { $_.path -eq $row.path }
        if (@($old).Count -ne 1 -or $old.sha256 -ne $row.sha256) { throw "Source changed since freeze: $($row.path)" }
    }
    $priorIncludes = Get-Content -LiteralPath $priorIncludesPath -Raw | ConvertFrom-Json
    foreach ($row in $priorIncludes.files) {
        if (-not $row.sha256) { throw "Prior include was missing: $($row.path)" }
        if (-not (Test-Path -LiteralPath $row.path -PathType Leaf)) { throw "Frozen include disappeared: $($row.path)" }
        $now = (Get-FileHash -Algorithm SHA256 -LiteralPath $row.path).Hash
        if ($now -ne $row.sha256) { throw "Frozen include changed: $($row.path)" }
    }
}
$buildArguments = @(
    (Join-Path $repoRoot 'tools/build_x360.py'),
    '--sdk', $sdkRoot,
    '--out', $outRoot,
    '--jobs', '1',
    '--timeout', '180',
    '--quiet',
    '--compiler-arg=/showIncludes'
)
foreach ($item in $vendorIncludes) { $buildArguments += ('--compiler-arg=/I' + (Join-Path $vendorRoot $item)) }
foreach ($source in $sourceNames) { $buildArguments += @('--source', $source) }
$started = (Get-Date).ToUniversalTime().ToString('o')
$launcherLog = Join-Path $attemptRoot 'launcher.log'
Push-Location $repoRoot
try {
    & $python @buildArguments *> $launcherLog
    $exitCode = $LASTEXITCODE
} finally { Pop-Location }
$ended = (Get-Date).ToUniversalTime().ToString('o')
$sourceAfter = foreach ($name in $sourceNames) {
    $path = Join-Path $repoRoot $name
    [pscustomobject]@{ path = $name; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
}
$reportPath = Join-Path $outRoot 'compile-results.json'
if (-not (Test-Path -LiteralPath $reportPath)) { throw "Compiler did not produce report: $reportPath" }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$perTu = @(); $uniquePaths = @{}
foreach ($row in $report.results) {
    $tuPaths = @()
    foreach ($line in (Get-Content -LiteralPath $row.log)) {
        if ($line -match '^\s*Note:\s+including file:\s*(?<path>.+?)\s*$') {
            $path = $Matches.path.Trim()
            $tuPaths += $path
            $key = $path.ToLowerInvariant()
            if (-not $uniquePaths.ContainsKey($key)) { $uniquePaths[$key] = $path }
        }
    }
    $perTu += [pscustomobject]@{ source = $row.source; log = $row.log; includes = $tuPaths }
}
$includeFiles = foreach ($path in ($uniquePaths.Values | Sort-Object)) {
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        [pscustomobject]@{ path = $path; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
    } else { [pscustomobject]@{ path = $path; sha256 = $null; missing = $true } }
}
$includeManifest = [pscustomobject]@{ per_tu = $perTu; files = @($includeFiles) }
$includePath = Join-Path $attemptRoot 'resolved-includes.json'
$includeManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $includePath -Encoding utf8
$sourceSnapshot = [ordered]@{ source_before = @($sourceBefore); source_after = @($sourceAfter) }
$sourceSnapshotPath = Join-Path $attemptRoot 'source-snapshot.json'
$sourceSnapshot | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $sourceSnapshotPath -Encoding utf8
$objectEvidence = foreach ($row in $report.results) {
    $machine = $row.machine
    $objExists = Test-Path -LiteralPath $row.object -PathType Leaf
    [pscustomobject]@{
        source = $row.source; passed = $row.passed; machine = $machine
        object = $row.object; object_sha256 = if ($objExists) { (Get-FileHash -Algorithm SHA256 -LiteralPath $row.object).Hash } else { $null }
        assembly = $row.assembly; assembly_sha256 = if (Test-Path -LiteralPath $row.assembly -PathType Leaf) { (Get-FileHash -Algorithm SHA256 -LiteralPath $row.assembly).Hash } else { $null }
        log = $row.log; log_sha256 = if (Test-Path -LiteralPath $row.log -PathType Leaf) { (Get-FileHash -Algorithm SHA256 -LiteralPath $row.log).Hash } else { $null }
        command = $row.command
    }
}
$coffPath = Join-Path $attemptRoot 'coff-evidence.json'
@($objectEvidence) | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $coffPath -Encoding utf8
$invocation = [ordered]@{
    attempt = $AttemptName; freeze_from = $FreezeFrom; started_utc = $started; ended_utc = $ended; exit_code = $exitCode
    repo = $repoRoot; branch = (& git -C $repoRoot branch --show-current).Trim(); head = (& git -C $repoRoot rev-parse HEAD).Trim()
    sdk = $sdkRoot; python = $python; build_arguments = $buildArguments; temp = $tempRoot
    fallback_roots = @($fallbackState); source_names = $sourceNames; source_snapshot = $sourceSnapshotPath
    resolved_includes = $includePath; coff_evidence = $coffPath; compile_results = $reportPath; launcher_log = $launcherLog
}
$invocation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $attemptRoot 'invocation.json') -Encoding utf8
Get-Content -LiteralPath $launcherLog
Get-Content -LiteralPath $reportPath -Raw
exit $exitCode
