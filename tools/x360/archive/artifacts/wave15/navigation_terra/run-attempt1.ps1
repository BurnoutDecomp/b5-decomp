$ErrorActionPreference = 'Stop'
$taskRoot = 'D:/Reverse/BP-X360-Experimental-Worktrees/triangle_cache_luna_3534'
$taskOut = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave15/navigation_terra/attempt1'
$taskTemp = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave15/navigation_terra/temp1'
New-Item -ItemType Directory -Force $taskTemp | Out-Null
$env:TEMP = $taskTemp
$env:TMP = $taskTemp
$taskSdk = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK'
$taskVendor = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor'
$taskArgs = @('tools/build_x360.py','--sdk',$taskSdk,'--out',$taskOut,'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
@('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src') | ForEach-Object { $taskArgs += '--compiler-arg=/I' + (Join-Path $taskVendor $_) }
@('src/GameSource/World/AI/RacingLine/BrnRacingLineGenerator_Curve.cpp','src/GameSource/World/AI/RacingLine/BrnRacingLineGenerator_Extrapolate.cpp','src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp','src/GameSource/World/AI/Route/BrnAStar.cpp','src/GameSource/Jobs/Traffic/BrnUpdateVehiclesJob_MoveToTarget.cpp') | ForEach-Object { $taskArgs += '--source'; $taskArgs += $_ }
Set-Location $taskRoot
& python @taskArgs
exit $LASTEXITCODE
