# Pixel probe: parked car, orbiting main camera, real resolved reflection faces.
$reflectionProfile = if ($env:BRN_TEST_REFLECTION_PROFILE) { $env:BRN_TEST_REFLECTION_PROFILE } else { 'rival_hunt_profile.sav' }
$case=@{
    Name='reflection_shadows_orbit'; Area='render'
    Bug='Camera-dependent shadow cascade selection and visible cube boundaries in reflections.'
    Frames=$true; ProfileFixture=$reflectionProfile
    Run=@{
        Drive=$true; DriveDelay=0; ThrottleScript='0:handbrake'; MotionProbe=$true
        SkipIntro=$true; AcceptGap=1.0; MaxSeconds=75; FrameEvery=240
        Teleport='762.2,0.7,-2235.5,259'
        MenuScript='timeout:65;wait:CarSelectManager: Exit state is finished;sleep:8;hold:CameraLeft:1.2;sleep:3;hold:CameraRight:2.4;sleep:3;hold:CameraLeft:1.2;sleep:4;hold:CameraRight:1.2;sleep:3;hold:CameraLeft:1.2;sleep:8'
    }
    DiagEnv=('BRN_FRAME_DUMP_START=3500,BRN_FRAME_DUMP_MAX=35,BRN_ENVMAP_DUMP_START=1000,BRN_ENVMAP_DUMP=1' +
        $(if($env:BRN_TEST_REFLECTION_CAR){',BRN_DEBUG_PLAYER_CAR='+$env:BRN_TEST_REFLECTION_CAR}else{''}))
    Setup={
        param($ctx)
        if($ctx.Slot -le 0){throw 'Reflection shadow probe requires a private slot.'}
        $cubeDir=Join-Path $ctx.RunDir 'cube'
        New-Item -ItemType Directory -Force -Path $cubeDir | Out-Null
        $config=@'
[Display]
Width=1280
Height=720
AdapterIndex=0
VSync=1
DecoupleSimulation=1
Fullscreen=0
[Settings]
AntiAliasing=2
EnvironmentMap=1
Coronas=1
SunCorona=1
[Graphics]
ShadowResolutionScale=2
ShadowDistance=120
ShadowSlopeBias=1
AnisotropicFiltering=16
[Debug]
World/LODs/Environment Map LOD=0
World/LODs/Prop Environment Map LOD=0
World/LODs/Environment Map Draw Distance=500
World Module/Graphics/Render environment map at 30hz=false
'@
        [IO.File]::WriteAllText((Join-Path $ctx.Root ('build/game_slots/'+$ctx.Slot+'/config.ini')),$config,[Text.Encoding]::ASCII)
        [IO.File]::WriteAllText((Join-Path $ctx.RunDir 'config.test.ini'),$config,[Text.Encoding]::ASCII)
    }
    Checks=@(
        @{Kind='NewAsserts';Name='no new assertions'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='Script';Name='real cube pixels captured over multiple camera views';Script={param($ctx)
            $files=@(Get-ChildItem -LiteralPath (Join-Path $ctx.RunDir 'cube') -Filter '*.bmp')
            $groups=@($files | Group-Object {$_.BaseName -replace '_[0-5]$',''} | Where-Object {$_.Count -eq 6})
            @{Pass=$groups.Count -ge 6;Detail=('complete cube captures='+$groups.Count+' files='+$files.Count)}
        }}
    )
}
if($env:BRN_TEST_REFLECTION_CAR) {
    $case.Checks+=@{Kind='LogMatch';Name='requested test car swap was accepted';Pattern='\[car\] \*+ HARNESS-ONLY PLAYER CAR SWAP'}
}
$case
