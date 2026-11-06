$ErrorActionPreference='Stop'
$taskTemp='D:/Reverse/BP-X360-Experimental-Artifacts/wave17/network-rank-terra/temp5'
New-Item -ItemType Directory -Force $taskTemp | Out-Null
$env:TEMP=$taskTemp; $env:TMP=$taskTemp
$taskVendor='C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor'
$taskArgs=@('-X','utf8','-B','tools/build_x360.py','--sdk','C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK','--out','D:/Reverse/BP-X360-Experimental-Artifacts/wave17/network-rank-terra/gate5','--timeout','180','--jobs','1','--compiler-arg=/showIncludes')
@('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src') | ForEach-Object { $taskArgs += '--compiler-arg=/I' + (Join-Path $taskVendor $_) }
@('vendor/dirtysdk/src/x360/lobbyrank.cpp','src/GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.cpp') | ForEach-Object { $taskArgs += '--source'; $taskArgs += $_ }
Set-Location 'D:/Reverse/BP-X360-Experimental-Worktrees/triangle_cache_luna_3534'
& C:/Python312/python.exe @taskArgs
exit $LASTEXITCODE
