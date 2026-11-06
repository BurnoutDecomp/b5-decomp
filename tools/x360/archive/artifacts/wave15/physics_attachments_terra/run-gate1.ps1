$ErrorActionPreference = 'Stop'
$taskArtifact = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave15/physics_attachments_terra'
$taskSdk = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK'
$taskVendor = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor'
$taskTemp = Join-Path $taskArtifact 'temp1'
New-Item -ItemType Directory -Force $taskTemp | Out-Null
$env:TEMP = $taskTemp
$env:TMP = $taskTemp
$taskSpec = (Get-Content -Raw D:/Reverse/BP-X360-Experimental-Artifacts/wave15/R19_SOURCE_ASSIGNMENTS.json | ConvertFrom-Json).tasks | Where-Object name -eq physics_attachments_terra
$taskArgs = @('tools/build_x360.py','--sdk',$taskSdk,'--out',(Join-Path $taskArtifact 'gate1'),'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
foreach ($taskPath in $taskSpec.sources) { $taskArgs += @('--source',$taskPath) }
foreach ($taskPath in @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')) { $taskArgs += "--compiler-arg=/I$taskVendor/$taskPath" }
& python @taskArgs
exit $LASTEXITCODE
