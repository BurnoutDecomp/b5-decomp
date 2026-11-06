$ErrorActionPreference = 'Stop'
$repo = 'D:\Reverse\BP-X360-Experimental-Worktrees\ai_accessors_terra'
$artifact = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave13\world_racing_terra'
$sdk = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$fallback = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp'
$tag = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
$out = Join-Path $artifact ("native-gate-$tag")
$tempDir = Join-Path $artifact ("temp-$tag")
New-Item -ItemType Directory -Force $tempDir | Out-Null
$env:TEMP = $tempDir
$env:TMP = $tempDir
$names = @(
 'src/GameSource/World/AI/RacingLine/BrnAISteeringFan_Aggression.cpp',
 'src/GameSource/World/AI/RacingLine/BrnAISteeringFan_Traffic.cpp',
 'src/GameSource/World/AI/RacingLine/BrnAISteeringFan_Weightings.cpp',
 'src/GameSource/World/AI/RacingLine/BrnHardNoGoMap.cpp',
 'src/GameSource/World/AI/RacingLine/BrnRacingLineGenerator.cpp'
)
$includes = @('vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src')
$argsList = @('tools/build_x360.py','--sdk',$sdk,'--out',$out,'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
foreach ($name in $names) { $argsList += @('--source', $name) }
foreach ($include in $includes) { $argsList += ('--compiler-arg=/I' + (Join-Path $fallback $include)) }
Set-Location $repo
& python @argsList
exit $LASTEXITCODE
