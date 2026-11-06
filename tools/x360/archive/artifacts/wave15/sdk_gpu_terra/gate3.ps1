$ErrorActionPreference = 'Stop'
$taskRoot = 'D:\Reverse\BP-X360-Experimental-Worktrees\gui_events_cpp03_luna_r10'
$taskArtifact = 'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\sdk_gpu_terra'
$taskSdk = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
$vendorRoot = 'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'
$env:TEMP = "$taskArtifact\temp3"
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
$taskArguments = @('tools/build_x360.py','--sdk',$taskSdk,'--out',"$taskArtifact\gate3",'--jobs','1','--timeout','180','--compiler-arg=/showIncludes')
$vendorIncludes = @('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src')
foreach ($v in $vendorIncludes) { $taskArguments += "--compiler-arg=/I$vendorRoot\$v" }
$taskSources = @('src/SDKs/XCam/XCamVideoOutput.cpp','src/SDKs/XCam/XCamDecoderOutput.cpp','src/SDKs/XCam/XCamI420VideoOutput.cpp','src/SDKs/XCam/XCamYUY2VideoOutput.cpp','src/SDKs/XCam/XCamDecoder.cpp','src/SDKs/XCam/XCamEncoder.cpp','src/SDKs/XCam/XCamEncoder_wBT_01.cpp','src/SDKs/XCam/XCAM_wM_01.cpp','src/SDKs/XCam/XCamRemoteConsole.cpp','src/SDKs/XCam/XCamPacketizer.cpp','src/SDKs/XCam/XCamQOSController.cpp','src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.cpp','src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxvignette.cpp')
foreach($p in $taskSources) { $taskArguments += @('--source',$p) }
Set-Location $taskRoot
& python @taskArguments 2>&1 | Tee-Object -FilePath "$taskArtifact\gate3-console.log"
exit $LASTEXITCODE
