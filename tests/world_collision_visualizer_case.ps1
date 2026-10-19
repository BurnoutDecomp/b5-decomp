# Real registry/console smoke test; run with the workflow run_case.ps1.
# Keep readback off while sending short named-event key taps: synchronous D3D
# frame capture can stall the owner thread long enough to miss complete taps.
@{
    Name='world_collision_visualizer'
    Frames=$false
    ProfileFixture='DISCO_collect_base.sav'
    Run=@{ SkipIntro=$true; AcceptGap=1.0; MaxSeconds=90; MinFreeGB=22; FrameEvery=120 }
    DiagEnv='BRN_DEBUG_UI_TRACE=1'
    Setup={
        param($ctx)
        $lsSlotDir=if($ctx.Slot -gt 0){Join-Path $ctx.Root ('build/game_slots/'+$ctx.Slot)}else{Join-Path $ctx.Root 'build/game'}
        $lsExec=if($ctx.Slot -gt 0){'../game_slots/'+$ctx.Slot+'/'}else{''}
        $lsRelative='../../'+$ctx.RunDir.Substring(([string]$ctx.Root).TrimEnd('\').Length+1).Replace('\','/')
        $laPhases=@{
            'collision-config'=@('component "World/Collision"','component "World/Collision draw mode"',
                'set "World/Collision/Draw distance" 100',
                'set "World/Collision/Draw collision world" FALSE',
                'set "World/Collision draw mode/Draw mode" 0')
            'collision-on'=@('set "World/Collision/Draw collision world" TRUE')
            'collision-off'=@('set "World/Collision/Draw collision world" FALSE')
            'collision-normals'=@('set "World/Collision/Draw collision world" TRUE',
                'set "World/Collision/Draw soup AABBs" TRUE',
                'set "World/Collision draw mode/Draw normals" TRUE')
            'collision-zones'=@('set "World/Collision draw mode/Draw mode" 2',
                'set "World/Collision draw mode/Draw normals" FALSE')
        }
        foreach($lrBind in @(@('F5','collision-on'),@('F6','collision-off'),@('F7','collision-normals'),@('F8','collision-zones'))) {
            $laPhases['collision-config']+=('bind '+$lrBind[0]+' exec "'+$lsExec+$lrBind[1]+'.txt"')
        }
        foreach($lsPhase in @($laPhases.Keys)) {
            $laPhases[$lsPhase]+=('save "'+$lsRelative+'/'+$lsPhase+'-saved.txt"')
            $lsFile=Join-Path $ctx.RunDir ($lsPhase+'.txt')
            Set-Content -LiteralPath $lsFile -Encoding ascii -Value $laPhases[$lsPhase]
            Copy-Item -LiteralPath $lsFile -Destination (Join-Path $lsSlotDir ($lsPhase+'.txt'))
        }
        $lsSteps=Join-Path $ctx.RunDir 'steps.txt'
        Set-Content -LiteralPath $lsSteps -Encoding ascii -Value @(
            'wait:CarSelectManager: Exit state is finished','sleep:4','key:192',
            ('cmd:exec "'+$lsExec+'collision-config.txt"'),'key:192',
            'sleep:3','note:baseline','key:116','sleep:4','note:collision-on',
            'key:117','sleep:3','note:collision-off','key:118','sleep:4','note:collision-normals',
            'key:119','sleep:4','note:collision-zones')
        $lsHelper=Join-Path $ctx.Root 'tools/tests/tools/VW_DEFORM_debugkeys.ps1'
        # Span loading/render scheduling hitches while retaining the named-event
        # harness. Characters use key-down edges; 150ms stays below UI repeat.
        $lsLocalHelper=Join-Path $ctx.RunDir 'collision-debugkeys.ps1'
        (Get-Content -LiteralPath $lsHelper -Raw).Replace('Start-Sleep -Milliseconds 60','Start-Sleep -Milliseconds 150') |
            Set-Content -LiteralPath $lsLocalHelper -Encoding ascii
        $laArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$lsHelper`"",'-GameLog',"`"$($ctx.GameLog)`"",
            '-OutDir',"`"$($ctx.RunDir)`"",'-Slot',"$($ctx.Slot)",'-StepsFile',"`"$lsSteps`"",'-TimeoutSec','85')
        $laArgs[4]="`"$lsLocalHelper`""
        return Start-Process powershell -ArgumentList $laArgs -PassThru -WindowStyle Hidden
    }
    Checks=@(
        @{Kind='NewAsserts';Name='no new assert families'}
        @{Kind='LogCount';Name='no exceptions';Pattern='\[EXCEPTION\]';Max=0}
        @{Kind='Script';Name='toggle and colour controls reach the native registry';Script={
            param($ctx)
            $laMissing=@()
            foreach($lrExpected in @(@('collision-config','FALSE'),@('collision-on','TRUE'),@('collision-off','FALSE'),@('collision-normals','TRUE'),@('collision-zones','TRUE'))) {
                $lsFile=Join-Path $ctx.RunDir ($lrExpected[0]+'-saved.txt')
                $lsText=if(Test-Path $lsFile){Get-Content $lsFile -Raw}else{''}
                if($lsText -notmatch ('(?m)^SET "/World/Collision/Draw collision world" "'+$lrExpected[1]+'"')){$laMissing+=$lrExpected[0]}
            }
            return @{Pass=$laMissing.Count -eq 0;Detail='mismatched saved toggle states: '+($laMissing -join ', ')}
        }}
        @{Kind='Script';Name='complete off on normals and zone control sequence';Script={
            param($ctx)
            $lsHelper=Join-Path $ctx.RunDir 'vw_deform_keys.log'
            if(!(Test-Path $lsHelper)){return @{Pass=$false;Detail='missing key sequence'}}
            $laLines=Get-Content $lsHelper
            return @{Pass=[bool]($laLines -match 'RESULT DONE');Detail=($laLines | Where-Object {$_ -match 'note:|RESULT'}) -join ' | '}
        }}
    )
}
