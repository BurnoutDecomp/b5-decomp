$ErrorActionPreference = 'Stop'
$taskRoot = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\world_racecar_terra'
$env:TEMP = Join-Path $taskRoot 'temp2'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$taskEntry = (Get-Content -Raw 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\R18_SOURCE_ASSIGNMENTS.json' | ConvertFrom-Json).tasks | Where-Object name -eq 'world_racecar_terra'
$taskArgs = @('tools/build_x360.py','--sdk','C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK','--out',(Join-Path $taskRoot 'gate2'),'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
foreach ($source in $taskEntry.sources) { $taskArgs += @('--source', $source) }
$vendorRoot='C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'
foreach ($rel in @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')) { $taskArgs += '--compiler-arg=/I' + (Join-Path $vendorRoot $rel) }
& python @taskArgs
exit $LASTEXITCODE

