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
        $laPhases=@(
            @{Name='collision-config';Values=@('FALSE','FALSE','FALSE','Surfaces','100')},
            @{Name='collision-on';Values=@('TRUE','FALSE','FALSE','Surfaces','100')},
            @{Name='collision-off';Values=@('FALSE','FALSE','FALSE','Surfaces','100')},
            @{Name='collision-normals';Values=@('TRUE','TRUE','TRUE','Surfaces','100')},
            @{Name='collision-zones';Values=@('TRUE','TRUE','FALSE','Collision zones','100')})
        $laPaths=@('World/Collision/Draw collision world','World/Collision/Draw soup AABBs',
            'World/Collision draw mode/Draw normals','World/Collision draw mode/Draw mode',
            'World/Collision/Draw distance')
        $laExpected=@()
        for($liPhase=0;$liPhase -lt $laPhases.Count;++$liPhase) {
            $lrPhase=$laPhases[$liPhase]
            $laCommands=@()
            if($liPhase -eq 0) {
                $laCommands+=@('component "World/Collision"','component "World/Collision draw mode"')
                foreach($lrBind in @(@('F5','collision-on'),@('F6','collision-off'),@('F7','collision-normals'),@('F8','collision-zones'))) {
                    $laCommands+=('bind '+$lrBind[0]+' exec "'+$lsExec+$lrBind[1]+'.txt"')
                }
            }
            for($liField=0;$liField -lt $laPaths.Count;++$liField) {
                # Enum text setters require the authored option names, not their
                # numeric indices. Avoid the unrelated, original global SAVE edge.
                $lsAlias='c'+$liPhase+'v'+$liField
                $laCommands+=('set "'+$laPaths[$liField]+'" "'+$lrPhase.Values[$liField]+'"')
                $laCommands+=('ALIAS '+$lsAlias+' "'+$laPaths[$liField]+'"')
                $laCommands+=('ALIAS '+$lsAlias)
                $laCommands+=$lsAlias
                $laExpected+=@{Phase=$lrPhase.Name;Alias=$lsAlias;Path=('/'+$laPaths[$liField]);
                    Value=$lrPhase.Values[$liField];Numeric=($liField -eq 4)}
            }
            $lsFile=Join-Path $ctx.RunDir ($lrPhase.Name+'.txt')
            Set-Content -LiteralPath $lsFile -Encoding ascii -Value $laCommands
            Copy-Item -LiteralPath $lsFile -Destination (Join-Path $lsSlotDir ($lrPhase.Name+'.txt'))
        }
        $laExpected | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ctx.RunDir 'expected-values.json') -Encoding ascii
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
            $laProblems=@();$laObserved=@()
            $laExpected=Get-Content -LiteralPath (Join-Path $ctx.RunDir 'expected-values.json') -Raw | ConvertFrom-Json
            $laLines=@($ctx.LogLines)
            if($laExpected.Count -ne 25) {$laProblems+='expected exactly 25 bounded registry probes'}
            foreach($lrExpected in $laExpected) {
                $laIndices=@(for($liLine=0;$liLine -lt $laLines.Count;++$liLine) {
                    if($laLines[$liLine].StartsWith(($lrExpected.Alias+': '),[StringComparison]::Ordinal)) {$liLine}
                })
                $lsPath=$null;$lsValue=$null;$lbMatch=$false
                if($laIndices.Count -eq 1 -and $laIndices[0]+1 -lt $laLines.Count) {
                    $lsPath=$laLines[$laIndices[0]].Substring($lrExpected.Alias.Length+2)
                    $lsValue=$laLines[$laIndices[0]+1]
                    if($lrExpected.Numeric) {
                        $lfValue=0.0
                        $lbMatch=[double]::TryParse($lsValue,[Globalization.NumberStyles]::Float,
                            [Globalization.CultureInfo]::InvariantCulture,[ref]$lfValue) -and $lfValue -eq 100.0
                    } else {$lbMatch=$lsValue -ceq $lrExpected.Value}
                    $lbMatch=$lbMatch -and $lsPath -ceq $lrExpected.Path
                }
                $laObserved+=@{phase=$lrExpected.Phase;alias=$lrExpected.Alias;path=$lsPath;value=$lsValue;matches=$lbMatch}
                if(!$lbMatch) {$laProblems+=($lrExpected.Phase+': '+$lrExpected.Path)}
            }
            $laObserved | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $ctx.RunDir 'native-readback.json') -Encoding ascii
            return @{Pass=$laProblems.Count -eq 0;Detail='25 native path/value probes; mismatches: '+($laProblems -join ', ')}
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
