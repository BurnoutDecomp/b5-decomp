# Native case: run with tools/tests/run_case.ps1 -Case <this file> -Slot 9.
# The private slot retains the user's display quality; profiles and main INI are untouched.
$lod = if ($env:BRN_TEST_PROP_REFLECTION_LOD -eq '2') { 2 } else { 0 }
$case = @{
    Name = ('generic_ini_lod' + $lod); Area = 'render'
    Bug = 'Registered debug INI overrides and selectable prop reflection LOD.'
    Frames = $true; ProfileFixture = 'rival_hunt_profile.sav'; ReflectionLod = $lod
    Run = @{ Drive=$true; DriveDelay=0; ThrottleScript='0:handbrake'; SkipIntro=$true; AcceptGap=1.0
             Teleport='762.2,0.7,-2235.5,259'; MaxSeconds=90; FrameEvery=12 }
    DiagEnv = 'BRN_GRAPHICS_DIAG=1,BRN_DEBUG_UI_TRACE=1,BRN_FRAME_DUMP_ARM=0,BRN_FRAME_DUMP_START=5000,BRN_FRAME_DUMP_MAX=12'
    Setup = {
        param($ctx)
        $config = @'
[Display]
Width=2560
Height=1440
AdapterIndex=0
VSync=1
DecoupleSimulation=1
Fullscreen=0
[Settings]
AntiAliasing=8
AlphaToCoverage=1
EnvironmentMap=1
Coronas=1
SunCorona=1
[Graphics]
AnisotropicFiltering=16
ShadowResolutionScale=2
ShadowDistance=120
ShadowSlopeBias=1
[Debug]
Environment/Bloom luminance scale=0.3
World/LODs/Environment Map LOD=0
World/ShadowMap/Cast shadows from traffic=true
World/LODs/OverrideDistances=true
World/LODs/LOD0Distance=3000
World/LODs/LOD1Distance=6000
World/LODs/LOD2Distance=9000
World/LODs/OverridePropDistances=true
World/LODs/PropLOD0Distance=3000
World/LODs/PropLOD1Distance=6000
World/LODs/PropLOD2Distance=9000
Graphics/Vehicles.../LODs.../Quality LOD 0=30
Graphics/Vehicles.../LODs.../Quality LOD 1=66
Graphics/Vehicles.../LODs.../Quality LOD 2=105
Graphics/Vehicles.../LODs.../Quality LOD 3=150
Graphics/Vehicles.../LODs.../Quality LOD 4=210
World Module/Graphics/Render environment map at 30hz=false
'@
        if ($ctx.Case.ReflectionLod -eq 0) { $config += "`r`nWorld/LODs/Prop Environment Map LOD=0`r`n" }
        else { $config = $config.Replace('Render environment map at 30hz=false','Render environment map at 30hz=true') }
        $slotConfig = Join-Path $ctx.Root ('build/game_slots/' + $ctx.Slot + '/config.ini')
        [IO.File]::WriteAllText($slotConfig,$config,[Text.Encoding]::ASCII)
        [IO.File]::WriteAllText((Join-Path $ctx.RunDir 'config.test.ini'),$config,[Text.Encoding]::ASCII)
        if ($ctx.Case.ReflectionLod -ne 0) { return }
        $steps = @(
            'wait:CarSelectManager: Exit state is finished', 'sleep:12',
            'key:192', 'cmd:set "World/LODs/Prop Environment Map LOD" 1', 'key:192',
            'sleep:6', 'note:LOD1 menu edit retained',
            'key:192', 'cmd:set "World/LODs/Prop Environment Map LOD" 0', 'key:192',
            'sleep:6', 'note:LOD0 restored'
        )
        $stepsPath = Join-Path $ctx.RunDir 'debug_ini_steps.txt'
        [IO.File]::WriteAllLines($stepsPath,$steps,[Text.Encoding]::ASCII)
        $helper = Join-Path $ctx.Root 'tools/tests/tools/VW_DEFORM_debugkeys.ps1'
        $args = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$helper`"",
                  '-GameLog',"`"$($ctx.GameLog)`"",'-OutDir',"`"$($ctx.RunDir)`"",
                  '-Slot',"$($ctx.Slot)",'-StepsFile',"`"$stepsPath`"")
        Start-Process powershell -ArgumentList $args -PassThru -WindowStyle Hidden
    }
    Checks = @(
        @{Kind='NewAsserts';Name='no new assertions'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Mark';Name='reached driving';Phase='DRIVING'}
        @{Kind='LogMatch';Name='bloom uses registered engine global';Pattern='\[debug-ini\] applied environment/bloom luminance scale=0\.3'}
        @{Kind='LogMatch';Name='world reflection LOD uses registered member';Pattern='\[debug-ini\] applied world/lods/environment map lod=0'}
        @{Kind='LogMatch';Name='traffic shadow switch uses registered member';Pattern='\[debug-ini\] applied world/shadowmap/cast shadows from traffic=true'}
        @{Kind='LogMatch';Name='vehicle quality table is registered';Pattern='\[debug-ini\] applied graphics/vehicles\.\.\./lods\.\.\./quality lod 4=210'}
        @{Kind='LogMatch';Name='lazy world component auto-activated';Pattern='\[debug-ini\] applied world module/graphics/render environment map at 30hz=false'}
        @{Kind='LogMatch';Name='world and prop distances reach submission';Pattern='\[graphics-effect\].*worldBase=3000 propBase=3000 envLOD=0'}
        @{Kind='LogMatch';Name='native high-resolution shadow atlas retained';Pattern='\[shadow-rt\] target 2560x3840 .*compare=HW'}
    )
}
if ($lod -eq 0) {
    $case.Checks += @(
        @{Kind='LogMatch';Name='LOD0 props selected for reflection capture';Pattern='\[graphics-prop-reflection\].*lods=[1-9][0-9]*/0/0 distinct=[1-9]'}
        @{Kind='LogMatch';Name='live registry edit changes prop capture to LOD1';Pattern='\[graphics-prop-reflection\].*lods=0/[1-9][0-9]*/0'}
        @{Kind='LogCount';Name='initial prop override is applied only once';Pattern='\[debug-ini\] applied world/lods/prop environment map lod=0';Min=1;Max=1}
        @{Kind='Script';Name='LOD0 to LOD1 to LOD0 reflects live edits';Script={param($ctx)
            $states = @($ctx.LogLines | ForEach-Object {
                if ($_ -match '\[graphics-prop-reflection\].*lods=(\d+)/(\d+)/(\d+)') {
                    if ([int]$Matches[1] -gt 0) { 0 } elseif ([int]$Matches[2] -gt 0) { 1 } elseif ([int]$Matches[3] -gt 0) { 2 }
                }
            })
            $firstOne = [Array]::IndexOf($states,1)
            $before = $firstOne -gt 0 -and $states[0] -eq 0
            $after = $firstOne -ge 0 -and $states[$states.Count-1] -eq 0
            @{Pass=$before -and $after;Detail=('selected states: '+($states -join ','))}
        }}
        @{Kind='Script';Name='console edits completed';Script={param($ctx)
            $path=Join-Path $ctx.RunDir 'vw_deform_keys.log'
            $text=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllText($path)}else{''}
            @{Pass=$text -match 'RESULT DONE';Detail=($text -split "`r?`n" | Where-Object {$_ -match 'note:|RESULT'}) -join '; '}
        }}
    )
} else {
    $case.DiagEnv += ',BRN_ENVMAP_STATS=1'
    foreach ($check in $case.Checks) {
        if ($check.Name -eq 'lazy world component auto-activated') { $check.Pattern = $check.Pattern.Replace('=false','=true') }
    }
    $case.Checks += @{Kind='LogMatch';Name='absent prop override preserves original LOD2';Pattern='\[graphics-prop-reflection\].*lods=0/0/[1-9][0-9]*'}
    $case.Checks += @{Kind='Script';Name='registered refresh control drives three actual faces';Script={param($ctx)
        $groups = @{}
        foreach ($line in $ctx.LogLines) {
            if ($line -match '\[envmap\] update (\d+) face (\d+) .*rendered=([01])') {
                $id=$Matches[1]
                if (!$groups.ContainsKey($id)) { $groups[$id]=@() }
                $groups[$id] += [int]$Matches[3]
            }
        }
        # The original first frame fills every face before alternating halves.
        $initial = $groups.ContainsKey('0') -and ($groups['0'] -join '') -eq '111111'
        $steady = @($groups.Keys | Where-Object { $_ -ne '0' } | ForEach-Object { $groups[$_] -join '' })
        $bad = @($steady | Where-Object { $_ -ne '111000' -and $_ -ne '000111' })
        $both = @($steady | Sort-Object -Unique).Count -eq 2
        @{Pass=$initial -and $steady.Count -ge 2 -and $bad.Count -eq 0 -and $both;
          Detail=('initialSix='+$initial+' steadySamples='+$steady.Count+' invalid='+$bad.Count+' bothHalves='+$both)}
    }}
}
$case
