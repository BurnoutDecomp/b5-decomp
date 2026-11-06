$ErrorActionPreference = 'Stop'
$artifact = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave14/physical_parts_terra'
$sdk = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK'
$fallback = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp'
$env:TEMP = "$artifact/temp"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$taskArguments = @('--sdk',$sdk,'--out',"$artifact/gate-01",'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
$roots = @('vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src')
foreach ($r in $roots) { $taskArguments += "--compiler-arg=/I$(Join-Path $fallback $r)" }
$assignment = Get-Content -Raw 'D:/Reverse/BP-X360-Experimental-Artifacts/wave14/R18_SOURCE_ASSIGNMENTS.json' | ConvertFrom-Json
foreach ($source in ($assignment.tasks | Where-Object name -eq physical_parts_terra).sources) { $taskArguments += @('--source',$source) }
& python tools/build_x360.py @taskArguments 2>&1 | Tee-Object "$artifact/gate-01-console.log"
exit $LASTEXITCODE
