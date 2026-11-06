$ErrorActionPreference = 'Stop'
$env:TEMP = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\vehicle_core_terra\temp'
$env:TMP = $env:TEMP
$sdk = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$vendor = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp'
$taskArguments = @('--sdk', $sdk, '--out', 'D:\Reverse\BP-X360-Experimental-Artifacts\wave14\vehicle_core_terra\attempt2', '--jobs', '1', '--timeout', '180', '--compiler-arg=/showIncludes')
@('vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src') | ForEach-Object { $taskArguments += "--compiler-arg=/I$(Join-Path $vendor $_)" }
@('BrnVehicleManager.cpp','BrnVehicleManager_Construct.cpp','BrnVehicleManager_Prepare.cpp','BrnVehicleManager_PerFrameLeaves.cpp','BrnVehicleManager_UpdateVehiclePhysics.cpp') | ForEach-Object { $taskArguments += '--source'; $taskArguments += "src/GameSource/Physics/VehicleManager/$_" }
python tools/build_x360.py @taskArguments
exit $LASTEXITCODE

