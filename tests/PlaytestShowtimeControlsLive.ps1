# Original both-bumper entry, then independent real left-stick X/Y channels.
# Root's established returning-profile launch supplies the same bounded route.
$case = & (Join-Path $PSScriptRoot 'PlaytestShowtimeCashLive.ps1')
$case.Name = 'playtest_showtime_controls'
$case.Bug = 'Verify delivered stick signs, camera-relative force values and bounce delivery in real Showtime.'
# Drive holds the throttle only. Its stable "accel/none" schedule writes steering
# once before entry; the later event-driven holds own those channels until release.
$case.Run.MenuScript = 'timeout:110; wait:\[showtime-control\] raw; hold:SteerLeft:2; sleep:3; hold:SteerRight:2; sleep:3; hold:StickUp:2; sleep:3; hold:StickDown:2; sleep:3; mark:controls_done'
$case.DiagEnv += ',BRN_SHOWTIME_CONTROL_DIAG=1,BRN_AT_GATE_DIAG=1'
$case.Checks = @(
    @{ Kind='Mark'; Name='reached DRIVING'; Phase='DRIVING' }
    @{ Kind='LogMatch'; Name='original Showtime launch pop'; Pattern='\[showtime\] launch pop:' }
    @{ Kind='LogCount'; Name='live changed-control observations'; Pattern='^\[showtime-control\] raw '; Min=6; Max=96 }
    @{ Kind='Script'; Name='all four signed left-stick inputs reach aftertouch'; Script={
        param($ctx)
        $seen=@{Left=$false;Right=$false;Up=$false;Down=$false}
        foreach($line in $ctx.LogLines) {
            if($line -notmatch '^\[showtime-control\] raw (?<x>\S+) (?<y>\S+) ') { continue }
            $x=[double]::Parse($Matches.x,[cultureinfo]::InvariantCulture)
            $y=[double]::Parse($Matches.y,[cultureinfo]::InvariantCulture)
            if($x -lt -0.9){$seen.Left=$true}; if($x -gt 0.9){$seen.Right=$true}
            if($y -gt 0.9){$seen.Up=$true}; if($y -lt -0.9){$seen.Down=$true}
        }
        @{Pass=(@($seen.Values | Where-Object {$_}).Count -eq 4);Detail=($seen | ConvertTo-Json -Compress)}
    } }
    @{ Kind='Script'; Name='decoded axes and actual camera-relative force agree with ARTIST'; Script={
        param($ctx)
        $count=0; $bad=0; $maxError=0.0
        foreach($line in $ctx.LogLines) {
            if($line -notmatch '^\[showtime-control\] raw (?<sx>\S+) (?<sy>\S+) (?<spin>\S+) requested (?<gas>\S+) brake (?<brake>\S+) wheel (?<wheel>[01]) button [01] decoded (?<yaw>\S+) (?<pitch>\S+) (?<scalar>\S+) enable (?<enable>\S+) dt \S+ air (?<air>[01]) height \S+ valid [01] cameraX (?<cx>\S+) (?<cy>\S+) (?<cz>\S+) cameraZ (?<zx>\S+) (?<zy>\S+) (?<zz>\S+) force (?<fx>\S+) (?<fy>\S+) (?<fz>\S+)$') {continue}
            ++$count
            $v=@{}
            foreach($key in @('sx','sy','spin','gas','brake','yaw','pitch','scalar','enable','cx','cy','cz','zx','zy','zz','fx','fy','fz')) {
                $v[$key]=[single]::Parse($Matches[$key],[cultureinfo]::InvariantCulture)
            }
            $yaw=[single]($v.sx*[single]0.25)
            if($Matches.air -eq '1' -and [math]::Abs($v.spin) -ge 0.2) {
                $yaw=[single]($yaw+[single]([math]::Sign($v.spin)*0.2))
            }
            $pitch=if($Matches.wheel -eq '1'){[single]([single]($v.gas-$v.brake)*[single]-0.25)}else{[single]($v.sy*[single]-0.25)}
            if([math]::Abs($yaw-$v.yaw) -gt 0.000001 -or [math]::Abs($pitch-$v.pitch) -gt 0.000001 -or $v.scalar -ne 0){++$bad}
            $ys=[single]([single]($v.yaw*[single]-12000)*$v.enable)
            $ps=[single]([single]($v.pitch*[single]$(if($v.pitch -ge 0){-8000}else{-20000}))*$v.enable)
            $x=@($v.cx,$v.cy,$v.cz); $z=@($v.zx,$v.zy,$v.zz); $actual=@($v.fx,$v.fy,$v.fz)
            $base=@(0.0,0.0,0.0)
            for($axis=0;$axis -lt 3;++$axis){$base[$axis]=[single]([single]($x[$axis]*$ys)+[single]($z[$axis]*$ps))}
            # Current witness does not expose the boost latch: test the two
            # attested force regimes (1 and 2.5), without guessing from a press.
            $errors=@()
            foreach($factor in @(1.0,2.5)) {
                $maximum=0.0
                for($axis=0;$axis -lt 3;++$axis) {$maximum=[math]::Max($maximum,[math]::Abs([single]($base[$axis]*$factor)-$actual[$axis]))}
                $errors+=$maximum
            }
            $error=($errors | Measure-Object -Minimum).Minimum
            $maxError=[math]::Max($maxError,$error)
            if([double]::IsNaN($error) -or [double]::IsInfinity($error) -or $error -gt 0.02){++$bad}
            if($v.cy -ne 0 -or $v.zy -ne 0){++$bad}
        }
        @{Pass=($count -ge 6 -and $bad -eq 0);Detail="observations=$count, mismatches=$bad, max force error=$maxError N; boost-latch choice remains separate"}
    } }
    @{ Kind='LogCount'; Name='actual roll lever impulses'; Pattern='^\[showtime-control-roll\]'; Min=1 }
    @{ Kind='LogCount'; Name='actual pitch lever impulses'; Pattern='^\[showtime-control-pitch\]'; Min=1 }
    @{ Kind='LogCount'; Name='bounce actions reach director with independent budget'; Pattern='^\[director-action\] 144 '; Min=1 }
    @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
    @{ Kind='LogCount'; Name='no assertions'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
)
$case
