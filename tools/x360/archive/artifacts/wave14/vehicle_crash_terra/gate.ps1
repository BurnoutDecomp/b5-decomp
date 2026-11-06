$ErrorActionPreference = 'Stop'
$artifact = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\vehicle_crash_terra'
$taskEntry = (Get-Content 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\R18_SOURCE_ASSIGNMENTS.json' -Raw | ConvertFrom-Json).tasks | Where-Object name -eq 'vehicle_crash_terra'
$canonical = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp'
$sdk = Join-Path $canonical 'build\x360\sdk-6534.4\portable\XDK'
$attempt = Join-Path $artifact 'attempt-01'
New-Item -ItemType Directory -Force $attempt, (Join-Path $attempt 'temp') | Out-Null
$env:TEMP = Join-Path $attempt 'temp'
$env:TMP = $env:TEMP
$taskArguments = @('tools/build_x360.py','--sdk',$sdk,'--out',(Join-Path $attempt 'output'),'--timeout','180','--jobs','1','--compiler-arg=/showIncludes')
foreach ($source in $taskEntry.sources) { $taskArguments += @('--source',$source) }
foreach ($root in @('vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src')) { $taskArguments += "--compiler-arg=/I$(Join-Path $canonical $root)" }
& python @taskArguments 2>&1 | Tee-Object -FilePath (Join-Path $attempt 'console.log')
exit $LASTEXITCODE
