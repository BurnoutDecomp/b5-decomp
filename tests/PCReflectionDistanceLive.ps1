# Native game evidence using a private slot and the production debug registry.
$case = @{
    Name = 'reflection_draw_distance'; Area = 'render'
    Bug = 'Extend reflection object cutoffs and capture frusta independently of reflection detail.'
    Frames = $true; ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{ Drive=$true; DriveDelay=0; ThrottleScript='0:handbrake'; SkipIntro=$true; AcceptGap=1.0
             Teleport='762.2,0.7,-2235.5,259'; MaxSeconds=90; FrameEvery=120 }
    DiagEnv = 'BRN_GRAPHICS_DIAG=1,BRN_DEBUG_UI_TRACE=1,BRN_FRAME_DUMP_MAX=30'
    Setup = {
        param($ctx)
        if ($ctx.Slot -le 0) { throw 'Reflection distance case requires a private slot.' }
        $config = @'
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
[Debug]
World/LODs/Environment Map LOD=0
World/LODs/Prop Environment Map LOD=0
World/LODs/Environment Map Draw Distance=0
World Module/Graphics/Render environment map at 30hz=false
'@
        $slotConfig = Join-Path $ctx.Root ('build/game_slots/' + $ctx.Slot + '/config.ini')
        [IO.File]::WriteAllText($slotConfig,$config,[Text.Encoding]::ASCII)
        [IO.File]::WriteAllText((Join-Path $ctx.RunDir 'config.test.ini'),$config,[Text.Encoding]::ASCII)
        $steps = @(
            'wait:CarSelectManager: Exit state is finished', 'sleep:12',
            'key:192', 'cmd:set "World/LODs/Environment Map Draw Distance" 500', 'key:192',
            'sleep:12', 'note:Extended reflection distance retained',
            'key:192', 'cmd:set "World/LODs/Environment Map Draw Distance" 0', 'key:192',
            'sleep:8', 'note:Original reflection distances restored'
        )
        $stepsPath = Join-Path $ctx.RunDir 'reflection_distance_steps.txt'
        [IO.File]::WriteAllLines($stepsPath,$steps,[Text.Encoding]::ASCII)
        $helper = Join-Path $ctx.Root 'tools/tests/tools/VW_DEFORM_debugkeys.ps1'
        $helperArgs = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$helper`"",
            '-GameLog',"`"$($ctx.GameLog)`"",'-OutDir',"`"$($ctx.RunDir)`"",
            '-Slot',"$($ctx.Slot)",'-StepsFile',"`"$stepsPath`"")
        Start-Process powershell -ArgumentList $helperArgs -PassThru -WindowStyle Hidden
    }
    Checks = @(
        @{Kind='NewAsserts';Name='no new assertions'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='LogCount';Name='registered startup distance applies once';Pattern='\[debug-ini\] applied world/lods/environment map draw distance=0';Min=1;Max=1}
        @{Kind='LogMatch';Name='extended range submits world objects beyond authored cutoffs';Pattern='\[graphics-reflection-distance\].*distance=500 worldExtended=[1-9][0-9]*'}
        @{Kind='LogMatch';Name='extended range submits props beyond authored cutoffs';Pattern='\[graphics-reflection-distance\].*distance=500 .*propExtended=[1-9][0-9]*'}
        @{Kind='Script';Name='live distance edits extend captures then restore original cutoffs';Script={param($ctx)
            $states = @()
            $invalidOriginal = 0
            foreach ($line in $ctx.LogLines) {
                if ($line -match '\[graphics-reflection-distance\].*distance=(\d+) worldExtended=(\d+) propExtended=(\d+)') {
                    $states += [int]$Matches[1]
                    if ([int]$Matches[1] -eq 0 -and ([int]$Matches[2] -ne 0 -or [int]$Matches[3] -ne 0)) { ++$invalidOriginal }
                }
            }
            $firstExtended = [Array]::IndexOf($states,500)
            $pass = $firstExtended -gt 0 -and $states[0] -eq 0 -and $states[-1] -eq 0 -and $invalidOriginal -eq 0
            @{Pass=$pass;Detail=('distance states: '+($states -join ',')+' invalidOriginal='+$invalidOriginal)}
        }}
        @{Kind='Script';Name='console edit sequence completed';Script={param($ctx)
            $path=Join-Path $ctx.RunDir 'vw_deform_keys.log'
            $text=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllText($path)}else{''}
            @{Pass=$text -match 'RESULT DONE';Detail=($text -split "`r?`n" | Where-Object {$_ -match 'note:|RESULT'}) -join '; '}
        }}
    )
}
$case
