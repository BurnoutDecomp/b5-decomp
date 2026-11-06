$ErrorActionPreference = 'Stop'
$taskRoot = 'D:\Reverse\BP-X360-Experimental-Worktrees\external-body-native-root'
$taskArtifact = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\native-entry-terra\fix'
$taskSdk = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$vendorRoot = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'
$env:TEMP = "$taskArtifact\temp"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
$taskArguments = @('tools/build_x360.py','--sdk',$taskSdk,'--out',"$taskArtifact\objects",'--jobs','1','--timeout','180','--compiler-arg=/showIncludes','--source','src/GameSource/Main/BrnMainX360.cpp')
$vendorIncludes = @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')
foreach ($v in $vendorIncludes) { $taskArguments += "--compiler-arg=/I$vendorRoot\$v" }
Set-Location $taskRoot
& python @taskArguments 2>&1 | Tee-Object -FilePath "$taskArtifact\console.log"
exit $LASTEXITCODE
