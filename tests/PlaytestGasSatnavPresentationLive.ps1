# Original gas-station trigger/refill/camera flow, with a bounded presentation capture.
$lWorkflowRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lCase = & (Join-Path $lWorkflowRoot 'tools/tests/cases/drivethru_gas_station.ps1')
$lCase.Name = 'playtest_gas_satnav_presentation'
$lCase.Bug = 'Gas-station camera/Car_Reset ordering and the driving Satnav reveal/draw path.'
$lCase.ProfileFixture = 'rival_hunt_profile.sav'
$lCase.Frames = $true
$lCase.Run.MaxSeconds = 65
$lCase.Run.FrameEvery = 3
$lCase.DiagEnv += ',BRN_CAMERA_TRACE=1,BRN_CAM_INPUT_DIAG=1,BRN_CRASHCAM_DIAG=1,BRN_PFX_DIAG=1,BRN_BLACKBARS_DIAG=1,BRN_SATNAV_DIAG=1,BRN_HUD_VIS=1,BRN_FRAME_DUMP_ARM=slomo,BRN_FRAME_DUMP_MAX=480'
$lCase.Checks += @(
    @{ Kind='LogMatch'; Name='both Satnav map and mask arrive';
       Pattern='\[satnav-diag\] LoadResources: map\(199\)nn=1 mask\(201\)nn=1' }
    @{ Kind='LogMatch'; Name='Satnav becomes visible during driving';
       Pattern='\[hud-vis\] satnav show' }
    @{ Kind='LogMatch'; Name='Satnav renderer draws the map';
       Pattern='\[satnav-diag\] RenderComponent DRAW:' }
    @{ Kind='LogMatch'; Name='Car_Reset flash is requested by the director';
       Pattern="\[pfx-dir\] (EnsureEffectIsPlaying requests|bridge posts 495 start) 'Car_Reset'" }
    @{ Kind='LogMatch'; Name='Car_Reset reaches the GUI hook arbitrator';
       Pattern="\[pfx\] StartHook 'Car_Reset'" }
)
return $lCase
