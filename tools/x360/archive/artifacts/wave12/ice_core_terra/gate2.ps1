$ErrorActionPreference = 'Stop'
$env:TEMP = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave12\ice_core_terra\temp'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$root = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor'
$argsList = @('--sdk','C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK','--out','D:/Reverse/BP-X360-Experimental-Artifacts/wave12/ice_core_terra/gate2','--jobs','1','--timeout','180')
foreach ($name in @('ICEAuthorSpaceOps','ICECameraMover','ICEControllerBubble','ICEMath','ICERender')) { $argsList += @('--source',"src/SDKs/Packages/ICE/$name.cpp") }
foreach ($path in @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')) { $argsList += "--compiler-arg=/I$root/$path" }
$argsList += '--compiler-arg=/showIncludes'
python tools/build_x360.py @argsList
