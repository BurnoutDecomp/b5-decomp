$repoRoot = 'D:/Reverse/BP-X360-Experimental-Worktrees/native_quaternion_terra_r10'
$artifactRoot = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/apt-providers-luna/native-free-callbacks'
$attemptRoot = Join-Path $artifactRoot 'gate-attempt01'
$taskTempRoot = Join-Path $attemptRoot 'task-temp'
$sdkRoot = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK'
$python = 'C:/Python312/python.exe'
$sourceNames = @('src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.cpp')
$fallbackRoots = @(
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/EABase/include/Common',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/EASTL/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/EAThread/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/renderware/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/PPMalloc/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/coreallocator/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/dirtysdk/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/zlib/include',
    'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/lua/src'
)
New-Item -ItemType Directory -Force -Path $taskTempRoot | Out-Null
$env:TEMP = $taskTempRoot
$env:TMP = $taskTempRoot
$fallbackState = foreach ($root in $fallbackRoots) {
    $exists = Test-Path -LiteralPath $root
    $head = if ($exists) { (& git -C $root rev-parse HEAD 2>$null).Trim() } else { $null }
    [pscustomobject]@{ path = $root; exists = $exists; git_head = $head }
}
$sourceBefore = foreach ($name in $sourceNames) {
    $path = Join-Path $repoRoot $name
    [pscustomobject]@{
        path = $name
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
        git_blob = (& git -C $repoRoot rev-parse ('HEAD:' + $name)).Trim()
    }
}
$buildArguments = @(
    (Join-Path $repoRoot 'tools/build_x360.py'),
    '--sdk', $sdkRoot,
    '--out', $attemptRoot,
    '--jobs', '1',
    '--timeout', '180',
    '--quiet'
)
foreach ($name in $sourceNames) { $buildArguments += @('--source', $name) }
$buildArguments += '--compiler-arg=/showIncludes'
foreach ($root in $fallbackRoots) { $buildArguments += ('--compiler-arg=/I' + $root) }
$start = (Get-Date).ToUniversalTime().ToString('o')
$launcherLog = Join-Path $attemptRoot 'launcher.log'
& $python @buildArguments *> $launcherLog
$exitCode = $LASTEXITCODE
$end = (Get-Date).ToUniversalTime().ToString('o')
$sourceAfter = foreach ($name in $sourceNames) {
    $path = Join-Path $repoRoot $name
    [pscustomobject]@{ path = $name; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
}
$invocation = [ordered]@{
    started_utc = $start
    ended_utc = $end
    exit_code = $exitCode
    repo = $repoRoot
    branch = (& git -C $repoRoot branch --show-current).Trim()
    head = (& git -C $repoRoot rev-parse HEAD).Trim()
    tree = (& git -C $repoRoot rev-parse 'HEAD^{tree}').Trim()
    sdk = $sdkRoot
    python = $python
    build_arguments = $buildArguments
    temp = $taskTempRoot
    fallback_roots = @($fallbackState)
    source_before = @($sourceBefore)
    source_after = @($sourceAfter)
    launcher_log = $launcherLog
}
$invocation | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $attemptRoot 'invocation.json') -Encoding utf8
$reportPath = Join-Path $attemptRoot 'compile-results.json'
$bindingsPath = Join-Path $attemptRoot 'showIncludes-bindings.json'
if (Test-Path -LiteralPath $reportPath) {
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    $tuBindings = @()
    $uniquePaths = @{}
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
        $tuBindings += [pscustomobject]@{ source = $row.source; log = $row.log; includes = $tuPaths }
    }
    $pathBindings = foreach ($path in ($uniquePaths.Values | Sort-Object)) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            [pscustomobject]@{ path = $path; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
        } else {
            [pscustomobject]@{ path = $path; sha256 = $null; missing = $true }
        }
    }
    [pscustomobject]@{ per_tu = $tuBindings; files = @($pathBindings) } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $bindingsPath -Encoding utf8
}
Get-Content -LiteralPath $launcherLog
Get-Content -LiteralPath (Join-Path $attemptRoot 'compile-results.json') -Raw
