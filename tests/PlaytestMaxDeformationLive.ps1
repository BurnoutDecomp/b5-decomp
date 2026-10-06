# Original selected-rig/max-total callbacks on the actual returning-player car.
# ProfileFixture is copied into this private slot and restored by run_case finally.
@{
    Name = 'playtest_max_deformation'
    Area = 'physics'
    Bug = 'Trace the original maximum debug command through the live spec, parts, wheels and palette.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        SkipIntro = $true
        AcceptGap = 1.0
        MaxSeconds = 80
        PauseAt = '12'
        PauseTarget = 'driver'
        UnpauseAt = '19'
        FrameEvery = 60
    }
    DiagEnv = 'BRN_PLAYTEST_MAX_DEFORM_AT=6,BRN_PAUSE_DAMAGE_DIAG=1,BRN_SCREEN_DIAG=1,BRN_FRAME_DUMP_MAX=180'
    Checks = @(
        @{ Kind='Mark'; Name='reached DRIVING'; Phase='DRIVING' }
        @{ Kind='LogMatch'; Name='original maximum preset callback fired'; Pattern='\[max-deform\] phase callback .*preset 2 scales 1 1 1 1 1 1 spec' }
        @{ Kind='LogCount'; Name='eight complete palette snapshots'; Pattern='^\[max-deform-row\]'; Min=1024; Max=1024 }
        @{ Kind='LogCount'; Name='four wheels in every snapshot'; Pattern='^\[max-deform-wheel\]'; Min=32; Max=32 }
        @{ Kind='Script'; Name='every live sensor follows its streamed signed compression limits'; Script={
            param($ctx)
            $count=0; $bad=0; $maximum=0.0; $declared=0
            foreach($line in $ctx.LogLines) {
                if($line -match '^\[max-deform\] phase callback .* sensors (?<n>\d+) ') { $declared=[int]$Matches.n }
                if($line -notmatch '^\[max-deform-sensor\] index (?<i>\d+) rest (?<rx>\S+) (?<ry>\S+) (?<rz>\S+) limits (?<px>\S+) (?<nx>\S+) (?<py>\S+) (?<ny>\S+) (?<pz>\S+) (?<nz>\S+) local (?<x>\S+) (?<y>\S+) (?<z>\S+) radius (?<r>\S+)') { continue }
                ++$count
                $r=@([single]::Parse($Matches.rx,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.ry,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.rz,[cultureinfo]::InvariantCulture))
                $p=@([single]::Parse($Matches.px,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.py,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.pz,[cultureinfo]::InvariantCulture))
                $n=@([single]::Parse($Matches.nx,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.ny,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.nz,[cultureinfo]::InvariantCulture))
                $v=@([single]::Parse($Matches.x,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.y,[cultureinfo]::InvariantCulture),[single]::Parse($Matches.z,[cultureinfo]::InvariantCulture))
                for($axis=0;$axis -lt 3;++$axis) {
                    $expected=[single]([single]($r[$axis]+$p[$axis])-$n[$axis])
                    $error=[math]::Abs([double]$v[$axis]-[double]$expected)
                    if([double]::IsNaN($error) -or [double]::IsInfinity($error) -or $error -gt 0.000001) { ++$bad }
                    if($error -gt $maximum) { $maximum=$error }
                }
            }
            @{ Pass=($declared -gt 0 -and $count -eq $declared -and $bad -eq 0)
               Detail="sensors=$count/$declared, mismatches=$bad, max absolute error=$maximum" }
        } }
        @{ Kind='LogMatch'; Name='paused through original Driver Details'; Pattern="\[screen\] ENTER 'CN_D_DETAIL" }
        @{ Kind='Script'; Name='maximum-damage readback survives repeated paused frames'; Script={
            param($ctx)
            $paused=@(); $resumed=$false
            foreach($line in $ctx.LogLines) {
                if($line -notmatch '^\[pause-damage\] paused (?<p>[01]) parts (?<parts>\d+) detached (?<d>\d+) wheels (?<w>\d+) skin (?<s>\d+) poses (?<t>\d+)') { continue }
                if($Matches.p -eq '1') { $paused += "$($Matches.parts):$($Matches.d):$($Matches.w):$($Matches.s):$($Matches.t)" }
                elseif($paused.Count -gt 0) { $resumed=$true }
            }
            $states=@($paused | Select-Object -Unique)
            @{ Pass=($paused.Count -ge 3 -and $states.Count -eq 1 -and $resumed)
               Detail="paused samples=$($paused.Count), states=$($states.Count), resumed=$resumed" }
        } }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogCount'; Name='no asserts'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
    )
}
