$ErrorActionPreference = 'Stop'
$taskRoot = 'D:\Reverse\BP-X360-Experimental-Worktrees\director_controls_terra_r10'
$taskArtifact = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\trigger_gui_terra'
$taskSdk = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$vendorRoot = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'
$env:TEMP = "$taskArtifact\temp"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
$taskArguments = @('tools/build_x360.py','--sdk',$taskSdk,'--out',"$taskArtifact\gate1",'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
$vendorIncludes = @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')
foreach ($v in $vendorIncludes) { $taskArguments += "--compiler-arg=/I$vendorRoot\$v" }
$j = Get-Content 'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\R19_SOURCE_ASSIGNMENTS.json' -Raw | ConvertFrom-Json
$t = $j.tasks | Where-Object name -eq trigger_gui_terra
foreach($p in $t.sources) { $taskArguments += @('--source',$p) }
Set-Location $taskRoot
& python @taskArguments 2>&1 | Tee-Object -FilePath "$taskArtifact\gate1-console.log"
exit $LASTEXITCODE

