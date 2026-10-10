# Observe default and opt-in reflection LODs through the production debug registry.
$relative = $env:BRN_TEST_REFLECTION_LOD_MODE -eq 'Relative'
$case = @{
    Name = $(if ($relative) {'reflection_lod_relative'} else {'reflection_lod_defaults'}); Area = 'render'
    Relative = $relative
    Bug = 'Optional world and prop reflection LOD transitions must preserve fixed LOD2 defaults.'
    Frames = $true; ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{ Drive=$true; DriveDelay=0; ThrottleScript='0:handbrake'; SkipIntro=$true; AcceptGap=1.0
             Teleport='762.2,0.7,-2235.5,259'; MaxSeconds=$(if ($relative) {110} else {45}); FrameEvery=180 }
    DiagEnv = 'BRN_GRAPHICS_DIAG=1,BRN_DEBUG_UI_TRACE=1,BRN_FRAME_DUMP_MAX=30'
    Setup = {
        param($ctx)
        if ($ctx.Slot -le 0) { throw 'Reflection LOD case requires a private slot.' }
        # No reflection entries: the first captures exercise the untouched defaults.
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
'@
        if ($ctx.Case.Relative) {
            $config += @'

[Debug]
World/LODs/OverrideDistances=true
World/LODs/LOD0Distance=40
World/LODs/LOD1Distance=80
World/LODs/OverridePropDistances=true
World/LODs/PropLOD0Distance=20
World/LODs/PropLOD1Distance=40
World/Reflections/World/LOD mode=Relative
World/Reflections/Props/LOD mode=Relative
World/Reflections/World/LOD distance scale=0.5
World/Reflections/Props/LOD distance scale=0.25
World/Reflections/World/LOD0 distance=500
World/Reflections/World/LOD1 distance=1000
World/Reflections/Props/LOD0 distance=500
World/Reflections/Props/LOD1 distance=1000
'@
        }
        $slotConfig = Join-Path $ctx.Root ('build/game_slots/' + $ctx.Slot + '/config.ini')
        [IO.File]::WriteAllText($slotConfig,$config,[Text.Encoding]::ASCII)
        [IO.File]::WriteAllText((Join-Path $ctx.RunDir 'config.test.ini'),$config,[Text.Encoding]::ASCII)
        if (-not $ctx.Case.Relative) { return }
        $steps = @(
            'wait:CarSelectManager: Exit state is finished', 'sleep:8',
            'note:Relative world and prop distances active',
            'key:192',
            'cmd:set "World/Reflections/World/LOD mode" Custom',
            'cmd:set "World/Reflections/Props/LOD mode" Custom', 'key:192',
            'sleep:12', 'note:Custom distances active',
            'key:192',
            'cmd:set "World/Reflections/World/LOD mode" Fixed',
            'cmd:set "World/Reflections/Props/LOD mode" Fixed', 'key:192',
            'sleep:8', 'note:Fixed LOD2 restored'
        )
        $stepsPath = Join-Path $ctx.RunDir 'reflection_lod_steps.txt'
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
        @{Kind='LogCount';Name='detail transitions never extend the capture range';Pattern='\[graphics-reflection-distance\].*distance=[1-9]';Max=0}
    )
}
if ($relative) {
    $case.Checks += @(
        @{Kind='LogCount';Name='named world mode applies through startup registry once';Pattern='\[debug-ini\] applied world/reflections/world/lod mode=Relative';Min=1;Max=1}
        @{Kind='LogCount';Name='named prop mode applies through startup registry once';Pattern='\[debug-ini\] applied world/reflections/props/lod mode=Relative';Min=1;Max=1}
        @{Kind='LogMatch';Name='relative mode produces mixed world capture LODs';Pattern='\[graphics-reflection-lod\].*worldMode=1 propMode=1 worldScale=0\.5 propScale=0\.25 world=[1-9][0-9]*/[0-9]+/[1-9][0-9]*'}
        @{Kind='LogMatch';Name='relative mode produces higher-detail prop capture meshes';Pattern='\[graphics-reflection-lod\].*worldMode=1 propMode=1 .*prop=([1-9][0-9]*/[0-9]+|[0-9]+/[1-9][0-9]*)/[0-9]+'}
        @{Kind='LogMatch';Name='custom world and prop settings reach higher-detail packets';Pattern='\[graphics-reflection-lod\].*worldMode=2 propMode=2 .*world=[1-9][0-9]*/[0-9]+/[0-9]+ prop=[1-9][0-9]*/[0-9]+/[0-9]+'}
        @{Kind='Script';Name='relative to custom to fixed is observed';Script={param($ctx)
            $states = @($ctx.LogLines | ForEach-Object {
                if ($_ -match '\[graphics-reflection-lod\].*worldMode=(\d+) propMode=(\d+)') {
                    if ($Matches[1] -eq $Matches[2]) { [int]$Matches[1] }
                }
            })
            $relative = [Array]::IndexOf($states,1)
            $custom = [Array]::IndexOf($states,2)
            $pass = $states.Count -gt 0 -and $states[0] -eq 1 -and $relative -ge 0 -and $custom -gt $relative -and $states[-1] -eq 0
            @{Pass=$pass;Detail=('modes: '+($states -join ','))}
        }}
        @{Kind='LogMatch';Name='returning to fixed restores exclusive LOD2 packets';Pattern='\[graphics-reflection-lod\].*worldMode=0 propMode=0 .*world=0/0/[1-9][0-9]* prop=0/0/[1-9][0-9]*'}
        @{Kind='Script';Name='console edit sequence completed';Script={param($ctx)
            $path=Join-Path $ctx.RunDir 'vw_deform_keys.log'
            $text=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllText($path)}else{''}
            @{Pass=$text -match 'RESULT DONE';Detail=($text -split "`r?`n" | Where-Object {$_ -match 'note:|RESULT'}) -join '; '}
        }}
    )
} else {
    $case.Checks += @(
        @{Kind='LogMatch';Name='absent settings retain fixed world and prop LOD2';Pattern='\[graphics-reflection-lod\].*worldMode=0 propMode=0 .*world=0/0/[1-9][0-9]* prop=0/0/[1-9][0-9]*'}
    )
}
$case
