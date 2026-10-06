# Real wall impact, then pause/unpause through the normal Driver Details flow.
# The diagnostic hashes render-owned damage before presentation interpolation.
@{
    Name = 'playtest_pause_deformation'
    Area = 'physics'
    Bug = 'Pausing a damaged car must preserve its detached part queue, skin and wheel visibility.'
    Frames = $true
    ProfileFixture = 'rival_hunt_profile.sav'
    Run = @{
        SkipIntro = $true
        AcceptGap = 1.0
        MaxSeconds = 70
        Drive = $true
        CrashSweep = '3249.796,-3.7,-1925.404'
        CrashSweepShots = '225:70'
        CrashSweepArm = 4
        PauseAt = '5.5'
        PauseTarget = 'driver'
        UnpauseAt = '14'
        FrameEvery = 30
    }
    DiagEnv = 'BRN_PAUSE_DAMAGE_DIAG=1,BRN_SCREEN_DIAG=1,BRN_DEFORM_BBOX_DIAG=1,BRN_FRAME_DUMP_MAX=180'
    Checks = @(
        @{ Kind='Mark'; Name='reached DRIVING'; Phase='DRIVING' }
        @{ Kind='LogMatch'; Name='real wall-impact stimulus fired'; Pattern='\[sweep\] shot 0/' }
        @{ Kind='LogMatch'; Name='real crash record opened'; Pattern='\[crash-exit\] OPENED crash record' }
        @{ Kind='LogMatch'; Name='entered Driver Details'; Pattern="\[screen\] ENTER 'CN_D_DETAIL" }
        @{ Kind='Script'; Name='detached geometry exists and stays frozen over pause, then resumes'; Script={
            param($ctx)
            $paused = @()
            $resumed = $false
            foreach ($line in $ctx.LogLines) {
                if ($line -notmatch '^\[pause-damage\] paused (?<pause>[01]) parts (?<parts>\d+) detached (?<detached>\d+) wheels (?<wheels>\d+) skin (?<skin>\d+) poses (?<poses>\d+)') { continue }
                if ($Matches.pause -eq '1') {
                    $paused += [pscustomobject]@{
                        Parts=[int]$Matches.parts; Detached=[int]$Matches.detached
                        Signature="$($Matches.parts):$($Matches.detached):$($Matches.wheels):$($Matches.skin):$($Matches.poses)"
                    }
                } elseif ($paused.Count -gt 0) { $resumed = $true }
            }
            $signatures = @($paused | Select-Object -ExpandProperty Signature -Unique)
            $detached = $paused.Count -gt 0 -and $paused[0].Detached -gt 0
            @{ Pass=($paused.Count -ge 3 -and $detached -and $signatures.Count -eq 1 -and $resumed)
               Detail="paused samples=$($paused.Count), detached=$detached, distinct damage states=$($signatures.Count), resumed=$resumed" }
        } }
        @{ Kind='LogCount'; Name='no exceptions'; Pattern='\[EXCEPTION\]'; Max=0 }
        @{ Kind='LogCount'; Name='no asserts'; Pattern='\[ASSERT(?:\s|\])'; Max=0 }
    )
}
