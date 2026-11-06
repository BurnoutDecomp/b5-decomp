$ErrorActionPreference = 'Stop'
$artifactRoot = 'D:/Reverse/BP-X360-Experimental-Artifacts/wave15/physics_io_terra'
$gateDir = Join-Path $artifactRoot 'gate-01'
$tempDir = Join-Path $artifactRoot 'temp-01'
New-Item -ItemType Directory -Force $gateDir, $tempDir | Out-Null
$env:TEMP = $tempDir
$env:TMP = $tempDir
$canonical = 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp'
$buildArgs = @('tools/build_x360.py','--sdk',"$canonical/build/x360/sdk-6534.4/portable/XDK",'--out',$gateDir,'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
$sourcePaths = @('src/GameSource/Physics/VehicleManager/SharedIO/BrnPlayerDriverControls.cpp','src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.cpp','src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface_UpdateRaceCarState.cpp','src/GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.cpp','src/GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager_Construct.cpp')
foreach ($sourcePath in $sourcePaths) { $buildArgs += @('--source',$sourcePath) }
$vendorRoots = @('vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src')
foreach ($vendorRoot in $vendorRoots) { $buildArgs += "--compiler-arg=/I$canonical/$vendorRoot" }
& python @buildArgs 2>&1 | Tee-Object -FilePath (Join-Path $artifactRoot 'gate-01-console.log')
exit $LASTEXITCODE
